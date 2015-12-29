#include <cmath>
#include <stack>
#include <algorithm>
#include <unordered_set>

#include "eisner_depparser.h"
#include "common/token/word.h"
#include "common/token/pos.h"

namespace eisner {
	WordPOSTag DepParser::empty_taggedword = WordPOSTag();
	WordPOSTag DepParser::start_taggedword = WordPOSTag();
	WordPOSTag DepParser::end_taggedword = WordPOSTag();

	DepParser::DepParser(const std::string & sFeatureInput, const std::string & sFeatureOut, int nState) :
		DepParserBase(nState) {

		m_nSentenceLength = 0;

		m_Weight = new Weight(sFeatureInput, sFeatureOut);

		for (int i = 0; i < MAX_SENTENCE_SIZE; ++i) {
			m_lItems[1][i].init(i, i);
		}

		DepParser::empty_taggedword.refer(TWord::code(EMPTY_WORD), TPOSTag::code(EMPTY_POSTAG));
		DepParser::start_taggedword.refer(TWord::code(START_WORD), TPOSTag::code(START_POSTAG));
		DepParser::end_taggedword.refer(TWord::code(END_WORD), TPOSTag::code(END_POSTAG));
	}

	DepParser::~DepParser() {
		delete m_Weight;
	}

	void DepParser::train(const DependencyTree & correct, const int & round) {
		// initialize
		int idx = 0;
		m_vecCorrectArcs.clear();
		m_nTrainingRound = round;
		m_nSentenceLength = correct.size();
		for (const auto & node : correct) {
			m_lSentence[idx].refer(TWord::code(TREENODE_WORD(node)), TPOSTag::code(TREENODE_POSTAG(node)));
			m_vecCorrectArcs.push_back(Arc(TREENODE_HEAD(node), idx++));
		}
		m_lSentence[idx].refer(TWord::code(ROOT_WORD), TPOSTag::code(ROOT_POSTAG));

		if (m_nState == ParserState::GOLDTEST) {
			m_setFirstGoldScore.clear();
			for (const auto & arc : m_vecCorrectArcs) {
				m_setFirstGoldScore.insert(BiGram<int>(arc.first() == -1 ? m_nSentenceLength : arc.first(), arc.second()));
			}
		}

		// train
		work(nullptr, correct);
		if (m_nTrainingRound % OUTPUT_STEP == 0) {
			std::cout << m_nTotalErrors << " / " << m_nTrainingRound << std::endl;
		}
	}

	void DepParser::parse(const Sentence & sentence, DependencyTree * retval) {
		int idx = 0;
		m_nTrainingRound = 0;
		DependencyTree correct;
		m_nSentenceLength = sentence.size();
		for (const auto & token : sentence) {
			m_lSentence[idx++].refer(TWord::code(SENT_WORD(token)), TPOSTag::code(SENT_POSTAG(token)));
			correct.push_back(DependencyTreeNode(token, -1, NULL_LABEL));
		}
		m_lSentence[idx].refer(TWord::code(ROOT_WORD), TPOSTag::code(ROOT_POSTAG));
		work(retval, correct);
	}

	void DepParser::work(DependencyTree * retval, const DependencyTree & correct) {

		decode();

		decodeArcs();

		switch (m_nState) {
		case ParserState::TRAIN:
			update();
			break;
		case ParserState::PARSE:
			generate(retval, correct);
			break;
		case ParserState::GOLDTEST:
			goldCheck();
			break;
		default:
			break;
		}
	}

	void DepParser::decode() {
		for (int d = 2; d <= m_nSentenceLength + 1; ++d) {

			initFirstOrderScore(d);

			for (int i = 0, n = m_nSentenceLength - d + 1; i < n; ++i) {

				StateItem & item = m_lItems[d][i];

				item.init(i, i + d - 1);

				const tscore & l2r_arc_score = m_lFirstOrderScore[ENCODE_L2R(i)];
				const tscore & r2l_arc_score = m_lFirstOrderScore[ENCODE_R2L(i)];

				for (int s = i, j = i + d; s < j - 1; ++s) {

					StateItem & litem = m_lItems[s - i + 1][i];
					StateItem & ritem = m_lItems[j - s - 1][s + 1];
					tscore partial_im_complete_score = litem.l2r.getScore() + ritem.r2l.getScore();

					item.updateL2RIm(s, partial_im_complete_score + l2r_arc_score);
					item.updateR2LIm(s, partial_im_complete_score + r2l_arc_score);
				}

				item.updateL2R(item.right, item.l2r_im.getScore());
				item.updateR2L(item.left, item.r2l_im.getScore());

				for (int s = i + 1, j = i + d; s < j - 1; ++s) {

					StateItem & litem = m_lItems[s - i + 1][i];
					StateItem & ritem = m_lItems[j - s][s];

					item.updateL2R(s, litem.l2r_im.getScore() + ritem.l2r.getScore());
					item.updateR2L(s, ritem.r2l_im.getScore() + litem.r2l.getScore());
				}
			}

			StateItem & item = m_lItems[d][m_nSentenceLength - d + 1];

			item.init(m_nSentenceLength - d + 1, m_nSentenceLength);

			item.updateR2LIm(m_nSentenceLength - 1, m_lItems[d - 1][item.left].l2r.getScore() + m_lFirstOrderScore[ENCODE_R2L(item.left)]);

			item.updateR2L(item.left, item.r2l_im.getScore());

			for (int i = item.left, s = item.left + 1, j = m_nSentenceLength + 1; s < j - 1; ++s) {
				item.updateR2L(s, m_lItems[j - s][s].r2l_im.getScore() + m_lItems[s - i + 1][i].r2l.getScore());
			}
		}
	}

	void DepParser::decodeArcs() {
		m_vecTrainArcs.clear();
		std::stack<std::tuple<int, int>> stack;
		stack.push(std::tuple<int, int>(m_nSentenceLength + 1, 0));
		m_lItems[m_nSentenceLength + 1][0].type = R2L_COMP;

		while (!stack.empty()) {
			int s = -1;
			std::tuple<int, int> span = stack.top();
			stack.pop();
			StateItem & item = m_lItems[std::get<0>(span)][std::get<1>(span)];

			if (item.left == item.right) {
				continue;
			}
			switch (item.type) {
			case L2R_COMP:
				s = item.l2r.getSplit();
				m_vecTrainArcs.push_back(BiGram<int>(item.left, s));

				if (item.left < item.right - 1) {
					m_lItems[s - item.left + 1][item.left].type = L2R_IM_COMP;
					stack.push(std::tuple<int, int>(s - item.left + 1, item.left));
					m_lItems[item.right - s + 1][s].type = L2R_COMP;
					stack.push(std::tuple<int, int>(item.right - s + 1, s));
				}
				break;
			case R2L_COMP:
				s = item.r2l.getSplit();
				m_vecTrainArcs.push_back(BiGram<int>(item.right == m_nSentenceLength ? -1 : item.right, s));

				if (item.left < item.right - 1) {
					m_lItems[item.right - s + 1][s].type = R2L_IM_COMP;
					stack.push(std::tuple<int, int>(item.right - s + 1, s));
					m_lItems[s - item.left + 1][item.left].type = R2L_COMP;
					stack.push(std::tuple<int, int>(s - item.left + 1, item.left));
				}
				break;
			case L2R_IM_COMP:
				s = item.l2r_im.getSplit();

				m_lItems[s - item.left + 1][item.left].type = L2R_COMP;
				stack.push(std::tuple<int, int>(s - item.left + 1, item.left));
				m_lItems[item.right - s][s + 1].type = R2L_COMP;
				stack.push(std::tuple<int, int>(item.right - s, s + 1));
				break;
			case R2L_IM_COMP:
				s = item.r2l_im.getSplit();

				m_lItems[s - item.left + 1][item.left].type = L2R_COMP;
				stack.push(std::tuple<int, int>(s - item.left + 1, item.left));
				m_lItems[item.right - s][s + 1].type = R2L_COMP;
				stack.push(std::tuple<int, int>(item.right - s, s + 1));
				break;
			default:
				break;
			}
		}

	}

	void DepParser::update() {
		std::unordered_set<Arc> positiveArcs;
		positiveArcs.insert(m_vecCorrectArcs.begin(), m_vecCorrectArcs.end());
		for (const auto & arc : m_vecTrainArcs) {
			positiveArcs.erase(arc);
		}
		std::unordered_set<Arc> negativeArcs;
		negativeArcs.insert(m_vecTrainArcs.begin(), m_vecTrainArcs.end());
		for (const auto & arc : m_vecCorrectArcs) {
			negativeArcs.erase(arc);
		}
		if (!positiveArcs.empty() || !negativeArcs.empty()) {
			++m_nTotalErrors;
		}
		for (const auto & arc : positiveArcs) {
			getOrUpdateStackScore(arc.first() == -1 ? m_nSentenceLength : arc.first(), arc.second(), 1);
		}

		for (const auto & arc : negativeArcs) {
			getOrUpdateStackScore(arc.first() == -1 ? m_nSentenceLength : arc.first(), arc.second(), -1);
		}
	}

	void DepParser::generate(DependencyTree * retval, const DependencyTree & correct) {
		for (int i = 0; i < m_nSentenceLength; ++i) {
			retval->push_back(DependencyTreeNode(TREENODE_POSTAGGEDWORD(correct[i]), -1, NULL_LABEL));
		}
		for (const auto & arc : m_vecTrainArcs) {
			TREENODE_HEAD(retval->at(arc.second())) = arc.first();
		}
	}

	void DepParser::goldCheck() {
		if (m_vecCorrectArcs.size() != m_vecTrainArcs.size() || m_lItems[m_nSentenceLength + 1][0].r2l.getScore() / GOLD_POS_SCORE != m_vecTrainArcs.size()) {
			std::cout << "gold parse len error at " << m_nTrainingRound << std::endl;
			std::cout << "score is " << m_lItems[m_nSentenceLength + 1][0].r2l.getScore() << std::endl;
			std::cout << "len is " << m_vecTrainArcs.size() << std::endl;
			++m_nTotalErrors;
		}
		else {
			int i = 0;
			std::sort(m_vecCorrectArcs.begin(), m_vecCorrectArcs.end(), [](const Arc & arc1, const Arc & arc2){ return arc1 < arc2; });
			std::sort(m_vecTrainArcs.begin(), m_vecTrainArcs.end(), [](const Arc & arc1, const Arc & arc2){ return arc1 < arc2; });
			for (int n = m_vecCorrectArcs.size(); i < n; ++i) {
				if (m_vecCorrectArcs[i].first() != m_vecTrainArcs[i].first() || m_vecCorrectArcs[i].second() != m_vecTrainArcs[i].second()) {
					break;
				}
			}
			if (i != m_vecCorrectArcs.size()) {
				std::cout << "gold parse tree error at " << m_nTrainingRound << std::endl;
				++m_nTotalErrors;
			}
		}
	}

	tscore DepParser::arcScore(const int & p, const int & c) {
		if (m_nState == ParserState::GOLDTEST) {
			m_nRetval = m_setFirstGoldScore.find(BiGram<int>(p, c)) == m_setFirstGoldScore.end() ? GOLD_NEG_SCORE : GOLD_POS_SCORE;
			return m_nRetval;
		}
		m_nRetval = 0;
		getOrUpdateStackScore(p, c);
		return m_nRetval;
	}

	void DepParser::initFirstOrderScore(const int & d) {
		for (int i = 0, max_i = m_nSentenceLength - d + 1; i < max_i; ++i) {
			m_lFirstOrderScore[ENCODE_L2R(i)] = arcScore(i, i + d - 1);
			m_lFirstOrderScore[ENCODE_R2L(i)] = arcScore(i + d - 1, i);
		}
		int i = m_nSentenceLength - d + 1;
		m_lFirstOrderScore[ENCODE_R2L(i)] = arcScore(m_nSentenceLength, i);
	}

	void DepParser::getOrUpdateStackScore(const int & p, const int & c, const int & amount) {

		Weight * cweight = (Weight*)m_Weight;

		p_1_tag = p > 0 ? m_lSentence[p - 1].second() : start_taggedword.second();
		p1_tag = p < m_nSentenceLength - 1 ? m_lSentence[p + 1].second() : end_taggedword.second();
		c_1_tag = c > 0 ? m_lSentence[c - 1].second() : start_taggedword.second();
		c1_tag = c < m_nSentenceLength - 1 ? m_lSentence[c + 1].second() : end_taggedword.second();

		p_word = m_lSentence[p].first();
		p_tag = m_lSentence[p].second();

		c_word = m_lSentence[c].first();
		c_tag = m_lSentence[c].second();

		m_nDis = encodeLinkDistanceOrDirection(p, c, false);
		m_nDir = encodeLinkDistanceOrDirection(p, c, true);

		word_int.refer(p_word, 0);
		cweight->m_mapPw.getOrUpdateScore(m_nRetval, word_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_int.referLast(m_nDis);
		cweight->m_mapPw.getOrUpdateScore(m_nRetval, word_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_int.referLast(m_nDir);
		cweight->m_mapPw.getOrUpdateScore(m_nRetval, word_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_int.refer(p_tag, 0);
		cweight->m_mapPp.getOrUpdateScore(m_nRetval, tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_int.referLast(m_nDis);
		cweight->m_mapPp.getOrUpdateScore(m_nRetval, tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_int.referLast(m_nDir);
		cweight->m_mapPp.getOrUpdateScore(m_nRetval, tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_int.refer(p_word, p_tag, 0);
		cweight->m_mapPwp.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_int.referLast(m_nDis);
		cweight->m_mapPwp.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_int.referLast(m_nDir);
		cweight->m_mapPwp.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_int.refer(c_word, 0);
		cweight->m_mapCw.getOrUpdateScore(m_nRetval, word_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_int.referLast(m_nDis);
		cweight->m_mapCw.getOrUpdateScore(m_nRetval, word_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_int.referLast(m_nDir);
		cweight->m_mapCw.getOrUpdateScore(m_nRetval, word_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_int.refer(c_tag, 0);
		cweight->m_mapCp.getOrUpdateScore(m_nRetval, tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_int.referLast(m_nDis);
		cweight->m_mapCp.getOrUpdateScore(m_nRetval, tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_int.referLast(m_nDir);
		cweight->m_mapCp.getOrUpdateScore(m_nRetval, tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_int.refer(c_word, c_tag, 0);
		cweight->m_mapCwp.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_int.referLast(m_nDis);
		cweight->m_mapCwp.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_int.referLast(m_nDir);
		cweight->m_mapCwp.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_tag_tag_int.refer(p_word, c_word, p_tag, c_tag, 0);
		cweight->m_mapPwpCwp.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPwpCwp.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPwpCwp.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_tag_int.refer(p_word, p_tag, c_tag, 0);
		cweight->m_mapPwpCp.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPwpCp.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPwpCp.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_tag_int.refer(c_word, p_tag, c_tag, 0);
		cweight->m_mapPpCwp.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPpCwp.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpCwp.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_tag_int.refer(p_word, c_word, p_tag, 0);
		cweight->m_mapPwpCw.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_int.referLast(m_nDis);
		cweight->m_mapPwpCw.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_int.referLast(m_nDir);
		cweight->m_mapPwpCw.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_tag_int.refer(p_word, c_word, c_tag, 0);
		cweight->m_mapPwCwp.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_int.referLast(m_nDis);
		cweight->m_mapPwCwp.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_int.referLast(m_nDir);
		cweight->m_mapPwCwp.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_int.refer(p_word, c_word, 0);
		cweight->m_mapPwCw.getOrUpdateScore(m_nRetval, word_word_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_int.referLast(m_nDis);
		cweight->m_mapPwCw.getOrUpdateScore(m_nRetval, word_word_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_int.referLast(m_nDir);
		cweight->m_mapPwCw.getOrUpdateScore(m_nRetval, word_word_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_int.refer(p_tag, c_tag, 0);
		cweight->m_mapPpCp.getOrUpdateScore(m_nRetval, tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_int.referLast(m_nDis);
		cweight->m_mapPpCp.getOrUpdateScore(m_nRetval, tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpCp.getOrUpdateScore(m_nRetval, tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(p_tag, p1_tag, c_1_tag, c_tag, 0);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(p_1_tag, p_tag, c_1_tag, c_tag, 0);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(p_tag, p1_tag, c_tag, c1_tag, 0);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(p_1_tag, p_tag, c_tag, c1_tag, 0);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(empty_taggedword.second(), p1_tag, c_1_tag, c_tag, 0);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.refer(empty_taggedword.second(), p1_tag, c_1_tag, c_tag, m_nDir);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(p_1_tag, empty_taggedword.second(), c_1_tag, c_tag, 0);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(empty_taggedword.second(), p1_tag, c_tag, c1_tag, 0);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(p_1_tag, empty_taggedword.second(), c_tag, c1_tag, 0);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(p_tag, p1_tag, c_1_tag, empty_taggedword.second(), 0);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(p_1_tag, p_tag, c_1_tag, empty_taggedword.second(), 0);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(p_tag, p1_tag, empty_taggedword.second(), c1_tag, 0);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(p_1_tag, p_tag, empty_taggedword.second(), c1_tag, 0);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(p_tag, empty_taggedword.second(), c_1_tag, c_tag, 0);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(empty_taggedword.second(), p_tag, c_1_tag, c_tag, 0);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(p_tag, p1_tag, c_tag, empty_taggedword.second(), 0);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(p_1_tag, p_tag, c_tag, empty_taggedword.second(), 0);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);


		for (int b = (int)std::fmin(p, c) + 1, e = (int)std::fmax(p, c); b < e; ++b) {
			b_tag = m_lSentence[b].second();
			tag_tag_tag_int.refer(p_tag, b_tag, c_tag, 0);
			cweight->m_mapPpBpCp.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
			tag_tag_tag_int.refer(p_tag, b_tag, c_tag, m_nDis);
			cweight->m_mapPpBpCp.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
			tag_tag_tag_int.refer(p_tag, b_tag, c_tag, m_nDir);
			cweight->m_mapPpBpCp.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		}
	}
}
