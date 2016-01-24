#include <cmath>
#include <stack>
#include <queue>
#include <algorithm>
#include <unordered_set>

#include "eceisner2nd_depparser.h"
#include "common/token/word.h"
#include "common/token/pos.h"

typedef std::pair<double, std::vector<WordPOSTag>> tSent;
bool operator<(const tSent& s1, const tSent& s2) {
	if (s1.first != s2.first) return s1.first < s2.first;
	return s1.second.size() < s2.second.size();
}
bool operator>(const tSent& s1, const tSent& s2) {
	if (s1.first != s2.first) return s1.first > s2.first;
	return s1.second.size() > s2.second.size();
}
bool operator<=(const tSent& s1, const tSent& s2) {
	return !(s1 > s2);
}
bool operator>=(const tSent& s1, const tSent& s2) {
	return !(s1 < s2);
}

namespace eisner2nd {

	WordPOSTag ECDepParser::empty_taggedword = WordPOSTag();
	WordPOSTag ECDepParser::start_taggedword = WordPOSTag();
	WordPOSTag ECDepParser::end_taggedword = WordPOSTag();

	ECDepParser::ECDepParser(const std::string & sFeatureInput, const std::string & sFeatureOut, int nState) :
		DepParserBase(nState) {

		m_nSentenceLength = 0;

		m_pWeight = new Weight2nd(sFeatureInput, sFeatureOut, m_nScoreIndex);

		for (int i = 0; i < MAX_SENTENCE_SIZE; ++i) {
			m_lItems[1][i].init(i, i);
		}

		ECDepParser::empty_taggedword.refer(TWord::code(EMPTY_WORD), TPOSTag::code(EMPTY_POSTAG));
		ECDepParser::start_taggedword.refer(TWord::code(START_WORD), TPOSTag::code(START_POSTAG));
		ECDepParser::end_taggedword.refer(TWord::code(END_WORD), TPOSTag::code(END_POSTAG));

		m_pWeight->init(ECDepParser::empty_taggedword, ECDepParser::start_taggedword, ECDepParser::end_taggedword);
	}

	ECDepParser::~ECDepParser() {
		delete m_pWeight;
	}

	void ECDepParser::train(const DependencyTree & correct, const int & round) {
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
		Arcs2BiArcs(m_vecCorrectArcs, m_vecCorrectBiArcs);

		if (m_nState == ParserState::GOLDTEST) {
			m_setFirstGoldScore.clear();
			m_setSecondGoldScore.clear();
			for (const auto & biarc : m_vecCorrectBiArcs) {
				m_setFirstGoldScore.insert(BiGram<int>(biarc.first(), biarc.third()));
				m_setSecondGoldScore.insert(TriGram<int>(biarc.first(), biarc.second(), biarc.third()));
			}
		}

		m_pWeight->referRound(round);
		work(nullptr, correct);
		if (m_nTrainingRound % OUTPUT_STEP == 0) {
			std::cout << m_nTotalErrors << " / " << m_nTrainingRound << std::endl;
		}
	}

	void ECDepParser::parse(const Sentence & sentence, DependencyTree * retval) {
		int idx = 0;
		DependencyTree correct;
		m_nSentenceLength = sentence.size();
		for (const auto & token : sentence) {
			m_lSentence[idx++].refer(TWord::code(SENT_WORD(token)), TPOSTag::code(SENT_POSTAG(token)));
			correct.push_back(DependencyTreeNode(token, -1, NULL_LABEL));
		}
		m_lSentence[idx].refer(TWord::code(ROOT_WORD), TPOSTag::code(ROOT_POSTAG));
		// traverse all empty nodes

		std::string emptyTags[] = {
				"*PRO*", "*OP*", "*T*", "*pro*",
				"*RNR*", "*OP*|*T*", "*OP*|*pro*", "*pro*|*T*",
				"*OP*|*pro*|*T*", "*RNR*|*RNR*", "*", "*PRO*|*T*",
				"*OP*|*PRO*|*T*", "*T*|*pro*", "*T*|*", "*pro*|*PRO*", "*|*T*"};
		AgendaBeam<tSent, 16> bestSents[2], *pGenerated = &bestSents[0], *pGenerator = &bestSents[1];
		AgendaBeam<tSent, 128> finishSent;
		tSent initialSent;

		decode();
		initialSent.first = (double)m_lItems[m_nSentenceLength + 1][0].r2l.getScore() / (double)m_nSentenceLength;
		for (int i = 0; i <= idx; ++i) initialSent.second.push_back(m_lSentence[i]);

		pGenerator->insertItem(initialSent);
		finishSent.insertItem(initialSent);

		while (pGenerator->size() > 0 && pGenerator->bestUnsortItem().second.size() - idx <= 10) {
			// add a ec node
			pGenerated->clear();
			std::cout << "length is " << pGenerator->bestUnsortItem().second.size() - 1 << std::endl;
			for (const auto & pItem : *pGenerator) {
				// l = SentenceLength + 1
				for (int s = 0, l = pItem->second.size(); s <= l; ++s) {
					tSent sent;
					m_nSentenceLength = l;
					for (int i = 0; i < s; ++i) sent.second.push_back(m_lSentence[i] = pItem->second[i]);
					sent.second.push_back(WordPOSTag());
					for (int i = s; i < l; ++i) sent.second.push_back(m_lSentence[i + 1] = pItem->second[i]);
					// in s insert empty node
					AgendaBeam<ScoreWithSplit, 2> bestEmpty;
					for (int e = 0; e < 17; ++e) {
						m_lSentence[s] = sent.second[s] = WordPOSTag(TWord::code(emptyTags[e]), TPOSTag::code("EMCAT"));
						for (int i = 0; i < l; ++i) {
							tscore sc = arcScore(i, s);
							if (i != s && m_lSentence[i].second() != m_lSentence[s].second() && sc > 0) bestEmpty.insertItem(ScoreWithSplit(e, sc));
						}
					}
					for (const auto & pE : bestEmpty) {
						m_lSentence[s] = sent.second[s] = WordPOSTag(TWord::code(emptyTags[pE->getSplit()]), TPOSTag::code("EMCAT"));

						decode();
						sent.first = (double)m_lItems[m_nSentenceLength + 1][0].r2l.getScore() / (double)m_nSentenceLength;
						pGenerated->insertItem(sent);
						finishSent.insertItem(sent);
					}
				}
			}
			std::swap(pGenerated, pGenerator);
		}

		correct.clear();
		tSent sent = finishSent.bestUnsortItem();
		for (int i = 0, l = sent.second.size(); i < l; ++i) {
			m_lSentence[i] = sent.second[i];
			correct.push_back(DependencyTreeNode(POSTaggedWord(TWord::key(m_lSentence[i].first()), TPOSTag::key(m_lSentence[i].second())), -1, NULL_LABEL));
		}
		m_nSentenceLength = sent.second.size() - 1;
		std::cout << "best score is " << sent.first << " empty node is " << m_nSentenceLength - idx << std::endl;
		work(retval, correct);
	}

	void ECDepParser::work(DependencyTree * retval, const DependencyTree & correct) {

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

	void ECDepParser::decode() {

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

					// solid both
					item.updateL2RSolidBoth(k, ritem.jux.getScore() + litem.l2r_solid_both.getScore() + l2r_arc_score + m_lSecondOrderScore[ENCODE_2ND_L2R(i, k)]);
					item.updateR2LSolidBoth(k, litem.jux.getScore() + ritem.r2l_solid_both.getScore() + r2l_arc_score + m_lSecondOrderScore[ENCODE_2ND_R2L(i, k)]);

					// complete
					item.updateL2R(k, litem.l2r_solid_both.getScore() + ritem.l2r.getScore());
					item.updateR2L(k, ritem.r2l_solid_both.getScore() + litem.r2l.getScore());
				}
				// solid both
				item.updateL2RSolidBoth(i, m_lItems[d - 1][i + 1].r2l.getScore() + l2r_arc_score + m_lSecondOrderScore[ENCODE_2ND_L2R(i, i)]);
				item.updateR2LSolidBoth(l, m_lItems[d - 1][i].l2r.getScore() + r2l_arc_score + m_lSecondOrderScore[ENCODE_2ND_R2L(i, i)]);
				// complete
				item.updateL2R(l, item.l2r_solid_both.getScore());
				item.updateR2L(i, item.r2l_solid_both.getScore());
			}
			// root
			StateItem & item = m_lItems[d][m_nSentenceLength - d + 1];
			item.init(m_nSentenceLength - d + 1, m_nSentenceLength);
			// solid both
			item.updateR2LSolidBoth(m_nSentenceLength, m_lItems[d - 1][item.left].l2r.getScore() +
				m_lFirstOrderScore[ENCODE_R2L(item.left)] +
				m_lSecondOrderScore[ENCODE_2ND_R2L(item.left, item.left)]);
			// complete
			item.updateR2L(item.left, item.r2l_solid_both.getScore());
			for (int i = item.left, s = item.left + 1, j = m_nSentenceLength + 1; s < j - 1; ++s) {
				item.updateR2L(s, m_lItems[j - s][s].r2l_solid_both.getScore() + m_lItems[s - i + 1][i].r2l.getScore());
			}
		}
	}

	void ECDepParser::decodeArcs() {

		m_vecTrainArcs.clear();
		std::stack<std::tuple<int, int>> stack;
		m_lItems[m_nSentenceLength + 1][0].type = R2L;
		stack.push(std::tuple<int, int>(m_nSentenceLength + 1, 0));

		while (!stack.empty()) {

			auto span = stack.top();
			stack.pop();

			int s = -1;
			StateItem & item = m_lItems[std::get<0>(span)][std::get<1>(span)];

			if (item.left == item.right) {
				continue;
			}
			switch (item.type) {
			case JUX:
				if (item.left < item.right - 1) {
					s = item.jux.getSplit();
					m_lItems[s - item.left + 1][item.left].type = L2R;
					stack.push(std::tuple<int, int>(s - item.left + 1, item.left));
					m_lItems[item.right - s][s + 1].type = R2L;
					stack.push(std::tuple<int, int>(item.right - s, s + 1));
				}
				break;
			case L2R:
				s = item.l2r.getSplit();

				if (item.left < item.right - 1) {
					m_lItems[s - item.left + 1][item.left].type = L2R_SOLID_BOTH;
					stack.push(std::tuple<int, int>(s - item.left + 1, item.left));
					m_lItems[item.right - s + 1][s].type = L2R;
					stack.push(std::tuple<int, int>(item.right - s + 1, s));
				}
				else {
					m_vecTrainArcs.push_back(BiGram<int>(item.left, item.right));
				}
				break;
			case R2L:
				s = item.r2l.getSplit();

				if (item.left < item.right - 1) {
					m_lItems[item.right - s + 1][s].type = R2L_SOLID_BOTH;
					stack.push(std::tuple<int, int>(item.right - s + 1, s));
					m_lItems[s - item.left + 1][item.left].type = R2L;
					stack.push(std::tuple<int, int>(s - item.left + 1, item.left));
				}
				else {
					m_vecTrainArcs.push_back(BiGram<int>(item.right == m_nSentenceLength ? -1 : item.right, item.left));
				}
				break;
			case L2R_SOLID_BOTH:
				m_vecTrainArcs.push_back(BiGram<int>(item.left, item.right));

				s = item.l2r_solid_both.getSplit();

				if (s == item.left) {
					m_lItems[item.right - s][s + 1].type = R2L;
					stack.push(std::tuple<int, int>(item.right - s, s + 1));
				}
				else {
					m_lItems[s - item.left + 1][item.left].type = L2R_SOLID_BOTH;
					stack.push(std::tuple<int, int>(s - item.left + 1, item.left));
					m_lItems[item.right - s + 1][s].type = JUX;
					stack.push(std::tuple<int, int>(item.right - s + 1, s));
				}
				break;
			case R2L_SOLID_BOTH:
				m_vecTrainArcs.push_back(BiGram<int>(item.right == m_nSentenceLength ? -1 : item.right, item.left));

				s = item.r2l_solid_both.getSplit();

				if (s == item.right) {
					m_lItems[s - item.left][item.left].type = L2R;
					stack.push(std::tuple<int, int>(s - item.left, item.left));
				}
				else {
					m_lItems[item.right - s + 1][s].type = R2L_SOLID_BOTH;
					stack.push(std::tuple<int, int>(item.right - s + 1, s));
					m_lItems[s - item.left + 1][item.left].type = JUX;
					stack.push(std::tuple<int, int>(s - item.left + 1, item.left));
				}
				break;
			default:
				break;
			}
		}
	}

	void ECDepParser::update() {
		Arcs2BiArcs(m_vecTrainArcs, m_vecTrainBiArcs);

		std::unordered_set<BiArc> positiveArcs;
		positiveArcs.insert(m_vecCorrectBiArcs.begin(), m_vecCorrectBiArcs.end());
		for (const auto & arc : m_vecTrainBiArcs) {
			positiveArcs.erase(arc);
		}
		std::unordered_set<BiArc> negativeArcs;
		negativeArcs.insert(m_vecTrainBiArcs.begin(), m_vecTrainBiArcs.end());
		for (const auto & arc : m_vecCorrectBiArcs) {
			negativeArcs.erase(arc);
		}
		if (!positiveArcs.empty() || !negativeArcs.empty()) {
			++m_nTotalErrors;
		}
		for (const auto & arc : positiveArcs) {
			getOrUpdateStackScore(arc.first(), arc.second(), arc.third(), 1);
			getOrUpdateStackScore(arc.first(), arc.third(), 1);
		}
		for (const auto & arc : negativeArcs) {
			getOrUpdateStackScore(arc.first(), arc.second(), arc.third(), -1);
			getOrUpdateStackScore(arc.first(), arc.third(), -1);
		}
	}

	void ECDepParser::generate(DependencyTree * retval, const DependencyTree & correct) {
		for (int i = 0; i < m_nSentenceLength; ++i) {
			retval->push_back(DependencyTreeNode(TREENODE_POSTAGGEDWORD(correct[i]), -1, NULL_LABEL));
		}
		for (const auto & arc : m_vecTrainArcs) {
			TREENODE_HEAD(retval->at(arc.second())) = arc.first();
		}
	}

	void ECDepParser::goldCheck() {
		Arcs2BiArcs(m_vecTrainArcs, m_vecTrainBiArcs);
		if (m_vecCorrectArcs.size() != m_vecTrainArcs.size() || m_lItems[m_nSentenceLength + 1][0].r2l.getScore() / GOLD_POS_SCORE != 2 * m_vecTrainArcs.size()) {
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

	const tscore & ECDepParser::arcScore(const int & p, const int & c) {
		if (m_nState == ParserState::GOLDTEST) {
			m_nRetval = m_setFirstGoldScore.find(BiGram<int>(p, c)) == m_setFirstGoldScore.end() ? GOLD_NEG_SCORE : GOLD_POS_SCORE;
			return m_nRetval;
		}
		m_nRetval = 0;
		if (TPOSTag::key(m_lSentence[p].second()) == "EMCAT") m_nRetval = -100000000000000;
		else getOrUpdateStackScore(p, c, 0);
		return m_nRetval;
	}

	const tscore & ECDepParser::twoArcScore(const int & p, const int & c, const int & c2) {
		if (m_nState == ParserState::GOLDTEST) {
			m_nRetval = m_setSecondGoldScore.find(TriGram<int>(p, c, c2)) == m_setSecondGoldScore.end() ? GOLD_NEG_SCORE : GOLD_POS_SCORE;
			return m_nRetval;
		}
		m_nRetval = 0;
		if (c != -1 && TPOSTag::key(m_lSentence[c].second()) == "EMCAT" && TPOSTag::key(m_lSentence[c2].second()) == "EMCAT") m_nRetval = -100000000000000;
		else getOrUpdateStackScore(p, c, c2, 0);
		return m_nRetval;
	}

	void ECDepParser::initFirstOrderScore(const int & d) {
		for (int i = 0, max_i = m_nSentenceLength - d + 1; i < max_i; ++i) {
			m_lFirstOrderScore[ENCODE_L2R(i)] = arcScore(i, i + d - 1);
			m_lFirstOrderScore[ENCODE_R2L(i)] = arcScore(i + d - 1, i);
//			if (TPOSTag::key(m_lSentence[i + d - 1].second()) == "EMCAT" && m_lFirstOrderScore[ENCODE_L2R(i)] > 0) {
//				if (m_setFirstGoldScore.find(BiGram<int>(i, i + d - 1)) == m_setFirstGoldScore.end()) {
//					std::cout << "in round " << ++m_nTrainingRound << std::endl;
//				}
////				std::cout << "in round " << ++m_nTrainingRound << std::endl;
////				std::cout << "from " << i << " to " << (i + d - 1) << " tag is" << TWord::key(m_lSentence[i + d - 1].first()) << " score is " << m_lFirstOrderScore[ENCODE_L2R(i)] << std::endl;
//			}
//			if (TPOSTag::key(m_lSentence[i].second()) == "EMCAT" && m_lFirstOrderScore[ENCODE_R2L(i)] > 0) {
//				if (m_setFirstGoldScore.find(BiGram<int>(i + d - 1, i)) == m_setFirstGoldScore.end()) {
//					std::cout << "in round " << ++m_nTrainingRound << std::endl;
//				}
////				std::cout << "in round " << ++m_nTrainingRound << std::endl;
////				std::cout << "from " << (i + d - 1) << " to " << i << " tag is " << TWord::key(m_lSentence[i].first()) << " score is " << m_lFirstOrderScore[ENCODE_R2L(i)] << std::endl;
//			}
		}
		int i = m_nSentenceLength - d + 1;
		m_lFirstOrderScore[ENCODE_R2L(i)] = arcScore(m_nSentenceLength, i);
	}

	void ECDepParser::initSecondOrderScore(const int & d) {
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

	void ECDepParser::getOrUpdateStackScore(const int & p, const int & c, const int & amount) {
		m_pWeight->getOrUpdateArcScore(m_nRetval, p, c, amount, m_nSentenceLength, m_lSentence);
	}

	void ECDepParser::getOrUpdateStackScore(const int & p, const int & c, const int & c2, const int & amount) {
		m_pWeight->getOrUpdateBiArcScore(m_nRetval, p, c, c2, amount, m_nSentenceLength, m_lSentence);

	}

}
