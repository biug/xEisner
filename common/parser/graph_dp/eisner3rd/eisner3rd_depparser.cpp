#include <cmath>
#include <stack>
#include <algorithm>
#include <unordered_set>

#include "eisner3rd_depparser.h"
#include "common/token/word.h"
#include "common/token/pos.h"

namespace eisner3rd {

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
		// for normal sentence
		for (const auto & node : correct) {
			m_lSentence[idx].refer(TWord::code(TREENODE_WORD(node)), TPOSTag::code(TREENODE_POSTAG(node)));
			m_vecCorrectArcs.push_back(Arc(TREENODE_HEAD(node), idx++));
		}
		m_lSentence[idx].refer(TWord::code(ROOT_WORD), TPOSTag::code(ROOT_POSTAG));
		Arcs2TriArcs(m_vecCorrectArcs, m_vecCorrectTriArcs);

		if (m_nState == ParserState::GOLDTEST) {
			m_setFirstGoldScore.clear();
			m_setSecondGoldScore.clear();
			m_setThirdGoldScore.clear();
			for (const auto & triarc : m_vecCorrectTriArcs) {
				m_setFirstGoldScore.insert(BiGram<int>(triarc.first(), triarc.forth()));
				m_setSecondGoldScore.insert(TriGram<int>(triarc.first(), triarc.third(), triarc.forth()));
				m_setThirdGoldScore.insert(QuarGram<int>(triarc.first(), triarc.second(), triarc.third(), triarc.forth()));
			}
		}

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
			initSecondOrderScore(d);

			for (int i = 0, max_i = m_nSentenceLength - d + 1; i < max_i; ++i) {

				int l = i + d - 1;
				StateItem & item = m_lItems[d][i];
				const tscore & l2r_arc_score = m_lFirstOrderScore[ENCODE_L2R(i)];
				const tscore & r2l_arc_score = m_lFirstOrderScore[ENCODE_R2L(i)];

				// initialize
				item.init(i, l);

				// jux
				for (int s = i; s < l; ++s) {
					item.updateJUX(s, m_lItems[s - i + 1][i].l2r.getScore() + m_lItems[l - s][s + 1].r2l.getScore());
				}

				for (int k = i + 1; k < l; ++k) {

					StateItem & litem = m_lItems[k - i + 1][i];
					StateItem & ritem = m_lItems[l - k + 1][k];

					const auto & l_solid_both_beam = litem.l2r_solid_both;
					const auto & r_solid_both_beam = ritem.r2l_solid_both;

					// solid both
					tscore l_base_score = ritem.jux.getScore() + l2r_arc_score + m_lSecondOrderScore[ENCODE_2ND_L2R(i, k)];

					for (const auto & sws : l_solid_both_beam) {
						const int & j = sws->getSplit();
						item.updateL2RSolidBoth(k, j, l_base_score +
							sws->getScore() +
							triArcScore(i, j == i ? -1 : j, k, l));
					}

					tscore r_base_score = litem.jux.getScore() + r2l_arc_score + m_lSecondOrderScore[ENCODE_2ND_R2L(i, k)];

					for (const auto & sws : r_solid_both_beam) {
						const int & j = sws->getSplit();
						item.updateR2LSolidBoth(k, j, r_base_score +
							sws->getScore() +
							triArcScore(l, j == l ? -1 : j, k, i));
					}

					// complete
					item.updateL2R(k, litem.l2r_solid_both.bestItem().getScore() + ritem.l2r.getScore());
					item.updateR2L(k, ritem.r2l_solid_both.bestItem().getScore() + litem.r2l.getScore());
				}
				// solid both
				item.updateL2RSolidBoth(i, i, m_lItems[d - 1][i + 1].r2l.getScore() +
					l2r_arc_score +
					m_lSecondOrderScore[ENCODE_2ND_L2R(i, i)] +
					triArcScore(i, -1, -1, l));
				item.updateR2LSolidBoth(l, l, m_lItems[d - 1][i].l2r.getScore() +
					r2l_arc_score +
					m_lSecondOrderScore[ENCODE_2ND_R2L(i, i)] +
					triArcScore(l, -1, -1, i));
				// complete
				item.updateL2R(l, item.l2r_solid_both.bestItem().getScore());
				item.updateR2L(i, item.r2l_solid_both.bestItem().getScore());
			}
			// root
			StateItem & item = m_lItems[d][m_nSentenceLength - d + 1];
			item.init(m_nSentenceLength - d + 1, m_nSentenceLength);
			// solid both
			item.updateR2LSolidBoth(m_nSentenceLength, m_nSentenceLength, m_lItems[d - 1][item.left].l2r.getScore() +
				m_lFirstOrderScore[ENCODE_R2L(item.left)] +
				m_lSecondOrderScore[ENCODE_2ND_R2L(item.left, item.left)] +
				triArcScore(item.right, -1, -1, item.left));
			// complete
			item.updateR2L(item.left, item.r2l_solid_both.bestItem().getScore());
			for (int i = item.left, s = item.left + 1, j = m_nSentenceLength + 1; s < j - 1; ++s) {
				item.updateR2L(s, m_lItems[j - s][s].r2l_solid_both.bestItem().getScore() + m_lItems[s - i + 1][i].r2l.getScore());
			}
		}
	}

	void DepParser::decodeArcs() {

		m_vecTrainArcs.clear();
		std::stack<std::tuple<int, int, int>> stack;
		m_lItems[m_nSentenceLength + 1][0].type = R2L;
		stack.push(std::tuple<int, int, int>(m_nSentenceLength + 1, -1, 0));

		while (!stack.empty()) {

			auto span = stack.top();
			stack.pop();

			int is = -1;
			int s = std::get<1>(span);
			StateItem & item = m_lItems[std::get<0>(span)][std::get<2>(span)];

			if (item.left == item.right) {
				continue;
			}
			switch (item.type) {
			case JUX:
				if (item.left < item.right - 1) {
					s = item.jux.getSplit();
					m_lItems[s - item.left + 1][item.left].type = L2R;
					stack.push(std::tuple<int, int, int>(s - item.left + 1, -1, item.left));
					m_lItems[item.right - s][s + 1].type = R2L;
					stack.push(std::tuple<int, int, int>(item.right - s, -1, s + 1));
				}
				break;
			case L2R:
				s = item.l2r.getSplit();

				if (item.left < item.right - 1) {
					m_lItems[s - item.left + 1][item.left].type = L2R_SOLID_BOTH;
					stack.push(std::tuple<int, int, int>(s - item.left + 1, m_lItems[s - item.left + 1][item.left].l2r_solid_both.bestItem().getSplit(), item.left));
					m_lItems[item.right - s + 1][s].type = L2R;
					stack.push(std::tuple<int, int, int>(item.right - s + 1, -1, s));
				}
				else {
					m_vecTrainArcs.push_back(BiGram<int>(item.left, item.right));
				}
				break;
			case R2L:
				s = item.r2l.getSplit();

				if (item.left < item.right - 1) {
					m_lItems[item.right - s + 1][s].type = R2L_SOLID_BOTH;
					stack.push(std::tuple<int, int, int>(item.right - s + 1, m_lItems[item.right - s + 1][s].r2l_solid_both.bestItem().getSplit(), s));
					m_lItems[s - item.left + 1][item.left].type = R2L;
					stack.push(std::tuple<int, int, int>(s - item.left + 1, -1, item.left));
				}
				else {
					m_vecTrainArcs.push_back(BiGram<int>(item.right == m_nSentenceLength ? -1 : item.right, item.left));
				}
				break;
			case L2R_SOLID_BOTH:
				m_vecTrainArcs.push_back(BiGram<int>(item.left, item.right));

				if (s == item.left) {
					m_lItems[item.right - s][s + 1].type = R2L;
					stack.push(std::tuple<int, int, int>(item.right - s, -1, s + 1));
				}
				else {
					is = findInnerSplit(item.l2r_solid_both, s);

					m_lItems[s - item.left + 1][item.left].type = L2R_SOLID_BOTH;
					stack.push(std::tuple<int, int, int>(s - item.left + 1, is, item.left));
					m_lItems[item.right - s + 1][s].type = JUX;
					stack.push(std::tuple<int, int, int>(item.right - s + 1, -1, s));
				}
				break;
			case R2L_SOLID_BOTH:
				m_vecTrainArcs.push_back(BiGram<int>(item.right == m_nSentenceLength ? -1 : item.right, item.left));

				if (s == item.right) {
					m_lItems[s - item.left][item.left].type = L2R;
					stack.push(std::tuple<int, int, int>(s - item.left, -1, item.left));
				}
				else {
					is = findInnerSplit(item.r2l_solid_both, s);

					m_lItems[item.right - s + 1][s].type = R2L_SOLID_BOTH;
					stack.push(std::tuple<int, int, int>(item.right - s + 1, is, s));
					m_lItems[s - item.left + 1][item.left].type = JUX;
					stack.push(std::tuple<int, int, int>(s - item.left + 1, -1, item.left));
				}
				break;
			default:
				break;
			}
		}
	}

	void DepParser::update() {
		Arcs2TriArcs(m_vecTrainArcs, m_vecTrainTriArcs);

		std::unordered_set<TriArc> positiveArcs;
		positiveArcs.insert(m_vecCorrectTriArcs.begin(), m_vecCorrectTriArcs.end());
		for (const auto & arc : m_vecTrainTriArcs) {
			positiveArcs.erase(arc);
		}
		std::unordered_set<TriArc> negativeArcs;
		negativeArcs.insert(m_vecTrainTriArcs.begin(), m_vecTrainTriArcs.end());
		for (const auto & arc : m_vecCorrectTriArcs) {
			negativeArcs.erase(arc);
		}
		if (!positiveArcs.empty() || !negativeArcs.empty()) {
			++m_nTotalErrors;
		}
		for (const auto & arc : positiveArcs) {
			getOrUpdateStackScore(arc.first(), arc.second(), arc.third(), arc.forth(), 1);
			getOrUpdateStackScore(arc.first(), arc.third(), arc.forth(), 1);
			getOrUpdateStackScore(arc.first(), arc.forth(), 1);
		}
		for (const auto & arc : negativeArcs) {
			getOrUpdateStackScore(arc.first(), arc.second(), arc.third(), arc.forth(), -1);
			getOrUpdateStackScore(arc.first(), arc.third(), arc.forth(), -1);
			getOrUpdateStackScore(arc.first(), arc.forth(), -1);
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
		Arcs2TriArcs(m_vecTrainArcs, m_vecTrainTriArcs);
		if (m_vecCorrectArcs.size() != m_vecTrainArcs.size() || m_lItems[m_nSentenceLength + 1][0].r2l.getScore() / GOLD_POS_SCORE != 3 * m_vecTrainArcs.size()) {
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

	const tscore & DepParser::arcScore(const int & p, const int & c) {
		if (m_nState == ParserState::GOLDTEST) {
			m_nRetval = m_setFirstGoldScore.find(BiGram<int>(p, c)) == m_setFirstGoldScore.end() ? GOLD_NEG_SCORE : GOLD_POS_SCORE;
			return m_nRetval;
		}
		m_nRetval = 0;
		getOrUpdateStackScore(p, c, 0);
		return m_nRetval;
	}

	const tscore & DepParser::twoArcScore(const int & p, const int & c, const int & c2) {
		if (m_nState == ParserState::GOLDTEST) {
			m_nRetval = m_setSecondGoldScore.find(TriGram<int>(p, c, c2)) == m_setSecondGoldScore.end() ? GOLD_NEG_SCORE : GOLD_POS_SCORE;
			return m_nRetval;
		}
		m_nRetval = 0;
		getOrUpdateStackScore(p, c, c2, 0);
		return m_nRetval;
	}

	const tscore & DepParser::triArcScore(const int & p, const int & c, const int & c2, const int & c3) {
		if (m_nState == ParserState::GOLDTEST) {
			m_nRetval = m_setThirdGoldScore.find(QuarGram<int>(p, c, c2, c3)) == m_setThirdGoldScore.end() ? GOLD_NEG_SCORE : GOLD_POS_SCORE;
			return m_nRetval;
		}
		m_nRetval = 0;
		getOrUpdateStackScore(p, c, c2, c3, 0);
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

	void DepParser::initSecondOrderScore(const int & d) {
		for (int i = 0, max_i = m_nSentenceLength - d + 1; i < max_i; ++i) {
			int l = i + d - 1;
			m_lSecondOrderScore[ENCODE_2ND_L2R(i, i)] = twoArcScore(i, -1, l);
			m_lSecondOrderScore[ENCODE_2ND_R2L(i, i)] = twoArcScore(l, -1, i);
			for (int k = i + 1, l = i + d - 1; k < l; ++k) {
				m_lSecondOrderScore[ENCODE_2ND_L2R(i, k)] = twoArcScore(i, k, l);
				m_lSecondOrderScore[ENCODE_2ND_R2L(i, k)] = twoArcScore(l, k, i);
			}
		}
		int i = m_nSentenceLength - d + 1;
		m_lSecondOrderScore[ENCODE_2ND_R2L(i, i)] = twoArcScore(m_nSentenceLength, -1, i);
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

	void DepParser::getOrUpdateStackScore(const int & p, const int & c, const int & c2, const int & amount) {
		Weight * cweight = (Weight*)m_Weight;

		p_tag = m_lSentence[p].second();

		c_word = IS_NULL(c) ? empty_taggedword.first() : m_lSentence[c].first();
		c_tag = IS_NULL(c) ? empty_taggedword.second() : m_lSentence[c].second();

		c2_word = m_lSentence[c2].first();
		c2_tag = m_lSentence[c2].second();

		m_nDir = encodeLinkDistanceOrDirection(p, c2, true);

		tag_tag_int.refer(c_tag, c2_tag, 0);
		cweight->m_mapC1pC2p.getOrUpdateScore(m_nRetval, tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_int.referLast(m_nDir);
		cweight->m_mapC1pC2p.getOrUpdateScore(m_nRetval, tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_int.refer(p_tag, c_tag, c2_tag, 0);
		cweight->m_mapPpC1pC2p.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpC1pC2p.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_int.refer(c_word, c2_word, 0);
		cweight->m_mapC1wC2w.getOrUpdateScore(m_nRetval, word_word_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_int.referLast(m_nDir);
		cweight->m_mapC1wC2w.getOrUpdateScore(m_nRetval, word_word_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_int.refer(c_word, c2_tag, 0);
		cweight->m_mapC1wC2p.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_int.referLast(m_nDir);
		cweight->m_mapC1wC2p.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_int.refer(c2_word, c_tag, 0);
		cweight->m_mapC1wC2p.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_int.referLast(m_nDir);
		cweight->m_mapC1wC2p.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
	}

	void DepParser::getOrUpdateStackScore(const int & p, const int & c, const int & c2, const int & c3, const int & amount) {
		Weight * cweight = (Weight*)m_Weight;

		p_word = m_lSentence[p].first();
		p_tag = m_lSentence[p].second();

		c_word = IS_NULL(c) ? empty_taggedword.first() : m_lSentence[c].first();
		c_tag = IS_NULL(c) ? empty_taggedword.second() : m_lSentence[c].second();

		c2_word = IS_NULL(c2) ? empty_taggedword.first() : m_lSentence[c2].first();
		c2_tag = IS_NULL(c2) ? empty_taggedword.second() : m_lSentence[c2].second();

		c3_word = m_lSentence[c3].first();
		c3_tag = m_lSentence[c3].second();

		m_nDir = encodeLinkDistanceOrDirection(p, c3, true);

		// word tag tag tag
		word_tag_tag_tag_int.refer(p_word, c_tag, c2_tag, c3_tag, 0);
		cweight->m_mapPwC1pC2pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPwC1pC2pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_tag_tag_int.refer(c_word, p_tag, c2_tag, c3_tag, 0);
		cweight->m_mapC1wPpC2pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapC1wPpC2pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_tag_tag_int.refer(c2_word, p_tag, c_tag, c3_tag, 0);
		cweight->m_mapC2wPpC1pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapC2wPpC1pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_tag_tag_int.refer(c3_word, p_tag, c_tag, c2_tag, 0);
		cweight->m_mapC3wPpC1pC2p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapC3wPpC1pC2p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		// word word tag tag
		word_word_tag_tag_int.refer(p_word, c_word, c2_tag, c3_tag, 0);
		cweight->m_mapPwC1wC2pC3p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPwC1wC2pC3p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_tag_tag_int.refer(p_word, c2_word, c_tag, c3_tag, 0);
		cweight->m_mapPwC2wC1pC3p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPwC2wC1pC3p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_tag_tag_int.refer(p_word, c3_word, c_tag, c2_tag, 0);
		cweight->m_mapPwC3wC1pC2p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPwC3wC1pC2p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_tag_tag_int.refer(c_word, c2_word, p_tag, c3_tag, 0);
		cweight->m_mapC1wC2wPpC3p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapC1wC2wPpC3p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_tag_tag_int.refer(c_word, c3_word, p_tag, c2_tag, 0);
		cweight->m_mapC1wC3wPpC2p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapC1wC3wPpC2p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_tag_tag_int.refer(c2_word, c3_word, p_tag, c_tag, 0);
		cweight->m_mapC2wC3wPpC1p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapC2wC3wPpC1p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		// tag tag tag tag
		tag_tag_tag_tag_int.refer(p_tag, c_tag, c2_tag, c3_tag, 0);
		cweight->m_mapPpC1pC2pC3p.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpC1pC2pC3p.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		// word tag tag
		word_tag_tag_int.refer(c_word, c2_tag, c3_tag, 0);
		cweight->m_mapC1wC2pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapC1wC2pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_tag_int.refer(c2_word, c_tag, c3_tag, 0);
		cweight->m_mapC2wC1pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapC2wC1pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_tag_int.refer(c3_word, c_tag, c2_tag, 0);
		cweight->m_mapC3wC1pC2p.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapC3wC1pC2p.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		// word word tag
		word_word_tag_int.refer(c_word, c2_word, c3_tag, 0);
		cweight->m_mapC1wC2wC3p.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_int.referLast(m_nDir);
		cweight->m_mapC1wC2wC3p.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_tag_int.refer(c_word, c3_word, c2_tag, 0);
		cweight->m_mapC1wC3wC2p.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_int.referLast(m_nDir);
		cweight->m_mapC1wC3wC2p.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_tag_int.refer(c2_word, c3_word, c_tag, 0);
		cweight->m_mapC2wC3wC1p.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_int.referLast(m_nDir);
		cweight->m_mapC2wC3wC1p.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		// tag tag tag
		tag_tag_tag_int.refer(c_tag, c2_tag, c3_tag, 0);
		cweight->m_mapC1pC2pC3p.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapC1pC2pC3p.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		// other
		word_word_int.refer(c_word, c3_word, 0);
		cweight->m_mapC1wC3w.getOrUpdateScore(m_nRetval, word_word_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_int.referLast(m_nDir);
		cweight->m_mapC1wC3w.getOrUpdateScore(m_nRetval, word_word_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_int.refer(c_word, c3_tag, 0);
		cweight->m_mapC1wC3p.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_int.referLast(m_nDir);
		cweight->m_mapC1wC3p.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_int.refer(c3_word, c_tag, 0);
		cweight->m_mapC3wC1p.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_int.referLast(m_nDir);
		cweight->m_mapC3wC1p.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_int.refer(c_tag, c3_tag, 0);
		cweight->m_mapC1pC3p.getOrUpdateScore(m_nRetval, tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_int.referLast(m_nDir);
		cweight->m_mapC1pC3p.getOrUpdateScore(m_nRetval, tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
	}

}
