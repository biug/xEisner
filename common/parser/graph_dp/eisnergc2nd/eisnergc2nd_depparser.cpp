#include <set>
#include <cmath>
#include <stack>
#include <algorithm>
#include <unordered_set>

#include "eisnergc2nd_depparser.h"
#include "common/token/word.h"
#include "common/token/pos.h"

namespace eisnergc2nd {

	WordPOSTag DepParser::empty_taggedword = WordPOSTag();
	WordPOSTag DepParser::start_taggedword = WordPOSTag();
	WordPOSTag DepParser::end_taggedword = WordPOSTag();

	DepParser::DepParser(const std::string & sFeatureInput, const std::string & sFeatureOut, int nState) :
		DepParserBase(nState) {

		m_nSentenceLength = 0;
		m_nTrainingRound = 0;

		m_Weight = new Weight(sFeatureInput, sFeatureOut);

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
			m_setArcGoldScore.clear();
			m_setBiSiblingArcGoldScore.clear();
			m_setGrandSiblingArcGoldScore.clear();
			m_setGrandBiSiblingArcGoldScore.clear();
			for (const auto & quararc : m_vecCorrectTriArcs) {
				m_setArcGoldScore.insert(BiGram<int>(quararc.second(), quararc.forth()));
				m_setBiSiblingArcGoldScore.insert(TriGram<int>(quararc.second(), quararc.third(), quararc.forth()));
				if (quararc.first() != -1) {
					m_setGrandSiblingArcGoldScore.insert(TriGram<int>(quararc.first(), quararc.second(), quararc.forth()));
					m_setGrandBiSiblingArcGoldScore.insert(QuarGram<int>(quararc.first(), quararc.second(), quararc.third(), quararc.forth()));
				}
			}
		}

		int lastTotalErrors = m_nTotalErrors;
		work(nullptr, correct);
		if (m_nTrainingRound > 1) {
			nBackSpace(std::to_string(lastTotalErrors) + " / " + std::to_string(m_nTrainingRound - 1));
		}
		std::cout << m_nTotalErrors << " / " << m_nTrainingRound;
	}

	void DepParser::parse(const Sentence & sentence, DependencyTree * retval) {
		int idx = 0;
		++m_nTrainingRound;
		DependencyTree correct;
		m_nSentenceLength = sentence.size();
		for (const auto & token : sentence) {
			m_lSentence[idx++].refer(TWord::code(SENT_WORD(token)), TPOSTag::code(SENT_POSTAG(token)));
			correct.push_back(DependencyTreeNode(token, -1, NULL_LABEL));
		}
		m_lSentence[idx].refer(TWord::code(ROOT_WORD), TPOSTag::code(ROOT_POSTAG));
		work(retval, correct);
		if (m_nTrainingRound > 1) {
			nBackSpace("parsing " + std::to_string(m_nTrainingRound - 1));
		}
		std::cout << "parsing " << m_nTrainingRound;
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

		for (int d = 1; d <= m_nSentenceLength + 1; ++d) {
			m_lItems[d].clear();
		}
	}

	void DepParser::decode() {

		initArcScore();
		initBiSiblingArcScore();
		initGrandSiblingArcScore();
		for (int level = 0; !initGrands(level); ++level);

		for (int i = 0; i < m_nSentenceLength; ++i) {
			m_lItems[1].push_back(StateItem());
			StateItem & item = m_lItems[1][i];
			item.init(i, i);
			for (const auto & g : m_vecGrandsAsLeft[i]) {
				item.jux[g] = ScoreWithSplit();
				item.l2r[g] = ScoreWithSplit();
				item.r2l[g] = ScoreWithSplit();
				item.l2r_im[g] = ScoreWithSplit();
				item.r2l_im[g] = ScoreWithSplit();
			}
			for (const auto & g : m_vecGrandsAsRight[i]) {
				item.jux[g] = ScoreWithSplit();
				item.l2r[g] = ScoreWithSplit();
				item.r2l[g] = ScoreWithSplit();
				item.l2r_im[g] = ScoreWithSplit();
				item.r2l_im[g] = ScoreWithSplit();
			}
		}

		for (int d = 2; d <= m_nSentenceLength; ++d) {
			for (int i = 0, max_i = m_nSentenceLength - d + 1; i < max_i; ++i) {

				m_lItems[d].push_back(StateItem());

				int l = i + d - 1;
				StateItem & item = m_lItems[d][i];
				const tscore & l2r_arc_score = m_vecArcScore[i][l];
				const tscore & r2l_arc_score = m_vecArcScore[l][i];

				// initialize
				item.init(i, l);
				// init left grands
				std::vector<int> tempLeftGrands;
				for (const auto & g : m_vecGrandsAsLeft[i]) {
					if (g < i || g > l) {
						tempLeftGrands.push_back(g);
					}
				}
				m_vecGrandsAsLeft[i] = tempLeftGrands;
				// init right grands
				std::vector<int> tempRightGrands;
				for (const auto & g : m_vecGrandsAsRight[l]) {
					if (g < i || g > l) {
						tempRightGrands.push_back(g);
					}
				}
				m_vecGrandsAsRight[l] = tempRightGrands;

				// jux
				// O(n^3)
				for (int s = i; s < l; ++s) {
					for (const auto & g : m_vecGrandsAsLeft[i]) {
						StateItem & litem = m_lItems[s - i + 1][i];
						StateItem & ritem = m_lItems[l - s][s + 1];
						if (litem.l2r.find(g) != litem.l2r.end() && ritem.r2l.find(g) != ritem.r2l.end()) {
							item.updateJUX(g, s, litem.l2r[g].getScore() + ritem.r2l[g].getScore());
						}
					}
				}

				// l2r_solid_both & r2l_solid_both
				for (int k = i + 1; k < l; ++k) {

					StateItem & litem = m_lItems[k - i + 1][i];
					StateItem & ritem = m_lItems[l - k + 1][k];

					if (ritem.jux.find(i) != ritem.jux.end()) {
						tscore l_base_jux = ritem.jux[i].getScore() + l2r_arc_score + m_vecBiSiblingScore[i][l][k];
						if (litem.l2r_im.size() < m_vecGrandsAsLeft[i].size()) {
							for (const auto & agenda : litem.l2r_im) {
								int g = agenda.first;
								if (g < i || g > l) {
									item.updateL2RIm(g, k, l_base_jux + agenda.second.getScore() + m_vecGrandChildScore[i][l][g] + grandBiSiblingArcScore(g, i, k, l));
								}
							}
						}
						else {
							for (const auto & g : m_vecGrandsAsLeft[i]) {
								if (litem.l2r_im.find(g) != litem.l2r_im.end()) {
									item.updateL2RIm(g, k, l_base_jux + litem.l2r_im[g].getScore() + m_vecGrandChildScore[i][l][g] + grandBiSiblingArcScore(g, i, k, l));
								}
							}
						}
					}
					if (ritem.l2r.find(i) != ritem.l2r.end()) {
						if (litem.l2r_im.size() < m_vecGrandsAsLeft[i].size()) {
							for (const auto & agenda : litem.l2r_im) {
								int g = agenda.first;
								if (g < i || g > l) {
									item.updateL2R(g, k, agenda.second.getScore() + ritem.l2r[i].getScore());
								}
							}
						}
						else {
							for (const auto & g : m_vecGrandsAsLeft[i]) {
								if (litem.l2r_im.find(g) != litem.l2r_im.end()) {
									item.updateL2R(g, k, litem.l2r_im[g].getScore() + ritem.l2r[i].getScore());
								}
							}
						}
					}

					if (litem.jux.find(l) != litem.jux.end()) {
						tscore r_base_jux = litem.jux[l].getScore() + r2l_arc_score + m_vecBiSiblingScore[l][i][k];
						if (ritem.r2l_im.size() < m_vecGrandsAsRight[l].size()) {
							for (const auto agenda : ritem.r2l_im) {
								int g = agenda.first;
								if (g < i || g > l) {
									item.updateR2LIm(g, k, r_base_jux + agenda.second.getScore() + m_vecGrandChildScore[l][i][g] + grandBiSiblingArcScore(g, l, k, i));
								}
							}
						}
						else {
							for (const auto & g : m_vecGrandsAsRight[l]) {
								if (ritem.r2l_im.find(g) != ritem.r2l_im.end()) {
									item.updateR2LIm(g, k, r_base_jux + ritem.r2l_im[g].getScore() + m_vecGrandChildScore[l][i][g] + grandBiSiblingArcScore(g, l, k, i));
								}
							}
						}
					}
					if (litem.r2l.find(l) != litem.r2l.end()) {
						if (ritem.r2l_im.size() < m_vecGrandsAsRight[l].size()) {
							for (auto && agenda : ritem.r2l_im) {
								int g = agenda.first;
								if (g < i || g > l) {
									item.updateR2L(g, k, agenda.second.getScore() + litem.r2l[l].getScore());
								}
							}
						}
						else {
							for (const auto & g : m_vecGrandsAsRight[l]) {
								if (ritem.r2l_im.find(g) != ritem.r2l_im.end()) {
									item.updateR2L(g, k, ritem.r2l_im[g].getScore() + litem.r2l[l].getScore());
								}
							}
						}
					}
				}

				// solid both
				for (const auto & g : m_vecGrandsAsLeft[i]) {
					if (m_lItems[d - 1][i + 1].r2l.find(i) != m_lItems[d - 1][i + 1].r2l.end()) {
						item.updateL2RIm(g, i, m_lItems[d - 1][i + 1].r2l[i].getScore() +
												l2r_arc_score +
												m_vecBiSiblingScore[i][l][i] +
												m_vecGrandChildScore[i][l][g] +
												grandBiSiblingArcScore(g, i, -1, l));
					}
					if (item.l2r_im.find(g) != item.l2r_im.end()) {
						item.updateL2R(g, l, item.l2r_im[g].getScore());
					}
				}
				for (const auto & g : m_vecGrandsAsRight[l]) {
					if (m_lItems[d - 1][i].l2r.find(l) != m_lItems[d - 1][i].l2r.end()) {
						item.updateR2LIm(g, l, m_lItems[d - 1][i].l2r[l].getScore() +
												r2l_arc_score +
												m_vecBiSiblingScore[l][i][l] +
												m_vecGrandChildScore[l][i][g] +
												grandBiSiblingArcScore(g, l, -1, i));
					}
					if (item.r2l_im.find(g) != item.r2l_im.end()) {
						item.updateR2L(g, i, item.r2l_im[g].getScore());
					}
				}
			}
		}
		// best root
		m_lItems[m_nSentenceLength + 1].push_back(StateItem());
		StateItem & item = m_lItems[m_nSentenceLength + 1][0];
		item.init(0, m_nSentenceLength);
		for (int m = 0; m < m_nSentenceLength; ++m) {
			StateItem & litem = m_lItems[m + 1][0];
			StateItem & ritem = m_lItems[m_nSentenceLength - m][m];
			if (litem.r2l.find(m_nSentenceLength) != litem.r2l.end() && ritem.l2r.find(m_nSentenceLength) != ritem.l2r.end()) {
				item.updateR2L(m_nSentenceLength, m, litem.r2l[m_nSentenceLength].getScore() + ritem.l2r[m_nSentenceLength].getScore() + arcScore(m_nSentenceLength, m));
			}
		}
	}

	void DepParser::decodeArcs() {

		m_vecTrainArcs.clear();
		std::stack<std::tuple<int, int, int>> stack;

		int s = m_lItems[m_nSentenceLength + 1][0].r2l[m_nSentenceLength].getSplit();
		m_vecTrainArcs.push_back(Arc(-1, s));

		m_lItems[s + 1][0].type = R2L;
		stack.push(std::tuple<int, int, int>(m_nSentenceLength, s + 1, 0));
		m_lItems[m_nSentenceLength - s][s].type = L2R;
		stack.push(std::tuple<int, int, int>(m_nSentenceLength, m_nSentenceLength - s, s));

		while (!stack.empty()) {

			auto span = stack.top();
			stack.pop();

			int g = std::get<0>(span);
			StateItem & item = m_lItems[std::get<1>(span)][std::get<2>(span)];

			if (item.left == item.right) {
				continue;
			}

			switch (item.type) {
			case JUX:
				if (item.left < item.right - 1) {
					s = item.jux[g].getSplit();
					m_lItems[s - item.left + 1][item.left].type = L2R;
					stack.push(std::tuple<int, int, int>(g, s - item.left + 1, item.left));
					m_lItems[item.right - s][s + 1].type = R2L;
					stack.push(std::tuple<int, int, int>(g, item.right - s, s + 1));
				}
				break;
			case L2R:
				s = item.l2r[g].getSplit();

				if (item.left < item.right - 1) {
					m_lItems[s - item.left + 1][item.left].type = L2R_IM;
					stack.push(std::tuple<int, int, int>(g, s - item.left + 1, item.left));
					m_lItems[item.right - s + 1][s].type = L2R;
					stack.push(std::tuple<int, int, int>(item.left, item.right - s + 1, s));
				}
				else {
					m_vecTrainArcs.push_back(Arc(item.left, item.right));
				}
				break;
			case R2L:
				s = item.r2l[g].getSplit();

				if (item.left < item.right - 1) {
					m_lItems[item.right - s + 1][s].type = R2L_IM;
					stack.push(std::tuple<int, int, int>(g, item.right - s + 1, s));
					m_lItems[s - item.left + 1][item.left].type = R2L;
					stack.push(std::tuple<int, int, int>(item.right, s - item.left + 1, item.left));
				}
				else {
					m_vecTrainArcs.push_back(Arc(item.right == m_nSentenceLength ? -1 : item.right, item.left));
				}
				break;
			case L2R_IM:
				s = item.l2r_im[g].getSplit();
				m_vecTrainArcs.push_back(Arc(item.left, item.right));

				if (s == item.left) {
					m_lItems[item.right - s][s + 1].type = R2L;
					stack.push(std::tuple<int, int, int>(item.left, item.right - s, s + 1));
				}
				else {
					m_lItems[s - item.left + 1][item.left].type = L2R_IM;
					stack.push(std::tuple<int, int, int>(g, s - item.left + 1, item.left));
					m_lItems[item.right - s + 1][s].type = JUX;
					stack.push(std::tuple<int, int, int>(item.left, item.right - s + 1, s));
				}
				break;
			case R2L_IM:
				s = item.r2l_im[g].getSplit();
				m_vecTrainArcs.push_back(Arc(item.right == m_nSentenceLength ? -1 : item.right, item.left));

				if (s == item.right) {
					m_lItems[s - item.left][item.left].type = L2R;
					stack.push(std::tuple<int, int, int>(item.right, s - item.left, item.left));
				}
				else {
					m_lItems[item.right - s + 1][s].type = R2L_IM;
					stack.push(std::tuple<int, int, int>(g, item.right - s + 1, s));
					m_lItems[s - item.left + 1][item.left].type = JUX;
					stack.push(std::tuple<int, int, int>(item.right, s - item.left + 1, item.left));
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
			if (arc.first() != -1) {
				getOrUpdateGrandScore(arc.first(), arc.second(), arc.forth(), 1);
				getOrUpdateGrandScore(arc.first(), arc.second(), arc.third(), arc.forth(), 1);
			}
			getOrUpdateSiblingScore(arc.second(), arc.third(), arc.forth(), 1);
			getOrUpdateSiblingScore(arc.second(), arc.forth(), 1);
		}
		for (const auto & arc : negativeArcs) {
			if (arc.first() != -1) {
				getOrUpdateGrandScore(arc.first(), arc.second(), arc.forth(), -1);
				getOrUpdateGrandScore(arc.first(), arc.second(), arc.third(), arc.forth(), -1);
			}
			getOrUpdateSiblingScore(arc.second(), arc.third(), arc.forth(), -1);
			getOrUpdateSiblingScore(arc.second(), arc.forth(), -1);
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
		if (m_vecCorrectArcs.size() != m_vecTrainArcs.size() || m_lItems[m_nSentenceLength + 1][0].r2l[m_nSentenceLength].getScore() / GOLD_POS_SCORE != 4 * m_vecTrainArcs.size() - 3) {
			std::cout << "gold parse len error at " << m_nTrainingRound << std::endl;
			std::cout << "score is " << m_lItems[m_nSentenceLength + 1][m_nSentenceLength].r2l[0].getScore() << std::endl;
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
			m_nRetval = m_setArcGoldScore.find(BiGram<int>(p, c)) == m_setArcGoldScore.end() ? GOLD_NEG_SCORE : GOLD_POS_SCORE;
			return m_nRetval;
		}
		m_nRetval = 0;
		getOrUpdateSiblingScore(p, c, 0);
		return m_nRetval;
	}

	const tscore & DepParser::biSiblingArcScore(const int & p, const int & c, const int & c2) {
		if (m_nState == ParserState::GOLDTEST) {
			m_nRetval = m_setBiSiblingArcGoldScore.find(TriGram<int>(p, c, c2)) == m_setBiSiblingArcGoldScore.end() ? GOLD_NEG_SCORE : GOLD_POS_SCORE;
			return m_nRetval;
		}
		m_nRetval = 0;
		getOrUpdateSiblingScore(p, c, c2, 0);
		return m_nRetval;
	}

	const tscore & DepParser::grandSiblingArcScore(const int & g, const int & p, const int & c) {
		if (m_nState == ParserState::GOLDTEST) {
			m_nRetval = m_setGrandSiblingArcGoldScore.find(TriGram<int>(g, p, c)) == m_setGrandSiblingArcGoldScore.end() ? GOLD_NEG_SCORE : GOLD_POS_SCORE;
			return m_nRetval;
		}
		m_nRetval = 0;
		getOrUpdateGrandScore(g, p, c, 0);
		return m_nRetval;
	}

	const tscore & DepParser::grandBiSiblingArcScore(const int & g, const int & p, const int & c, const int & c2) {
		if (m_nState == ParserState::GOLDTEST) {
			m_nRetval = m_setGrandBiSiblingArcGoldScore.find(QuarGram<int>(g, p, c, c2)) == m_setGrandBiSiblingArcGoldScore.end() ? GOLD_NEG_SCORE : GOLD_POS_SCORE;
			return m_nRetval;
		}
		m_nRetval = 0;
		getOrUpdateGrandScore(g, p, c, c2, 0);
		return m_nRetval;
	}

	void DepParser::initArcScore() {
		m_vecArcScore =
			std::vector<std::vector<tscore>>(
			m_nSentenceLength + 1,
			std::vector<tscore>(m_nSentenceLength));
		for (int i = 0; i < m_nSentenceLength; ++i) {
			for (int j = 0; j < m_nSentenceLength; ++j) {
				if (j != i) {
					m_vecArcScore[i][j] = arcScore(i, j);
				}
			}
		}
		for (int i = 0; i < m_nSentenceLength; ++i) {
			m_vecArcScore[m_nSentenceLength][i] = arcScore(m_nSentenceLength, i);
		}
	}

	void DepParser::initBiSiblingArcScore() {
		m_vecBiSiblingScore =
			std::vector<std::vector<std::vector<tscore>>>(
			m_nSentenceLength + 1,
			std::vector<std::vector<tscore>>(
			m_nSentenceLength,
			std::vector<tscore>(m_nSentenceLength + 1)));
		for (int i = 0; i < m_nSentenceLength; ++i) {
			for (int j = 0; j < i; ++j) {
				m_vecBiSiblingScore[i][j][i] = biSiblingArcScore(i, -1, j);
				for (int k = j + 1; k < i; ++k) {
					m_vecBiSiblingScore[i][j][k] = biSiblingArcScore(i, k, j);
				}
			}
			for (int j = i + 1; j < m_nSentenceLength; ++j) {
				m_vecBiSiblingScore[i][j][i] = biSiblingArcScore(i, -1, j);
				for (int k = i + 1; k < j; ++k) {
					m_vecBiSiblingScore[i][j][k] = biSiblingArcScore(i, k, j);
				}
			}
		}
		for (int j = 0; j < m_nSentenceLength; ++j) {
			m_vecBiSiblingScore[m_nSentenceLength][j][m_nSentenceLength] = biSiblingArcScore(m_nSentenceLength, -1, j);
		}
	}

	void DepParser::initGrandSiblingArcScore() {
		m_vecGrandChildScore =
			std::vector<std::vector<std::vector<tscore>>>(
			m_nSentenceLength,
			std::vector<std::vector<tscore>>(
			m_nSentenceLength,
			std::vector<tscore>(m_nSentenceLength + 1)));
		for (int i = 0; i < m_nSentenceLength; ++i) {
			for (int j = 0; j < m_nSentenceLength; ++j) {
				if (j != i) {
					for (int g = 0; g <= m_nSentenceLength; ++g) {
						if (g != i && g != j) {
							m_vecGrandChildScore[i][j][g] = grandSiblingArcScore(g, i, j);
						}
					}
				}
			}
		}
	}

	bool DepParser::initGrands(int level) {

		std::vector<std::vector<int>> vecGrands;
		m_vecGrandsAsLeft.clear();
		m_vecGrandsAsRight.clear();

		if (level < 0) {
			return false;
		}
		if (level > GRAND_MAX_LEVEL) {
			for (int p = 0; p < m_nSentenceLength; ++p) {
				vecGrands.push_back(std::vector<int>());
				for (int h = 0; h <= m_nSentenceLength; ++h) {
					if (h != p) {
						vecGrands[p].push_back(h);
					}
				}
			}
			m_vecGrandsAsLeft = vecGrands;
			m_vecGrandsAsRight = vecGrands;
			return true;
		}

		std::unordered_map<int, std::set<int>> grands;
		for (int p = 0; p < m_nSentenceLength; ++p) {
			GrandAgendaU gaArc;
			GrandAgendaB gaGC;
			GrandAgendaB gaBiArc;
			for (int h = 0; h <= m_nSentenceLength; ++h) {
				if (h != p) {
					gaArc.insertItem(ScoreWithSplit(h, m_vecArcScore[h][p]));
					gaBiArc.insertItem(ScoreWithBiSplit(h, h, m_vecBiSiblingScore[h][p][h]));
					for (int m = 0; m < m_nSentenceLength; ++m) {
						if (m != p && m != h) {
							gaGC.insertItem(ScoreWithBiSplit(h, m, m_vecGrandChildScore[p][m][h]));
							if (h != m_nSentenceLength) {
								gaBiArc.insertItem(ScoreWithBiSplit(h, m, m_vecBiSiblingScore[h][p][m]));
							}
						}
					}
				}
			}
			// sort items
			gaArc.sortItems();
			gaGC.sortItems();
			int size = (GRAND_AGENDA_SIZE << level);
			int i = 0;
			// only use top 'size' item
			for (const auto & agenda : gaArc) {
				if (agenda->getScore() > 0) {
					grands[p].insert(agenda->getSplit());
				}
				else {
					break;
				}
				if (++i >= size) {
					break;
				}
			}
			i = 0;
			for (const auto & agenda : gaGC) {
				if (agenda->getScore() > 0) {
					grands[p].insert(agenda->getSplit());
					grands[agenda->getInnerSplit()].insert(p);
				}
				else {
					break;
				}
				if (++i >= size) {
					break;
				}
			}
			i = 0;
			for (const auto & agenda : gaBiArc) {
				if (agenda->getScore() > 0) {
					grands[p].insert(agenda->getSplit());
					if (agenda->getInnerSplit() != agenda->getSplit()) {
						grands[agenda->getInnerSplit()].insert(agenda->getSplit());
					}
				}
				else {
					break;
				}
				if (++i >= size) {
					break;
				}
			}
			vecGrands.push_back(std::vector<int>());
		}
		std::set<std::pair<int, int>> goldArcs;
		// generate grand candidate
		for (const auto & grand : grands) {
			for (const auto & g : grand.second) {
				vecGrands[grand.first].push_back(g);
				goldArcs.insert(std::pair<int, int>(g, grand.first));
			}
		}
		// check if grand candidate can generate a 'non-projective' tree
		if (!hasNonProjectiveTree(goldArcs, m_nSentenceLength)) {
			return false;
		}

		m_vecGrandsAsLeft = vecGrands;
		m_vecGrandsAsRight = vecGrands;

		return true;
	}

	void DepParser::getOrUpdateSiblingScore(const int & p, const int & c, const int & amount) {

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

	void DepParser::getOrUpdateSiblingScore(const int & p, const int & c, const int & c2, const int & amount) {
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

	void DepParser::getOrUpdateGrandScore(const int & g, const int & p, const int & c, const int & amount) {
		Weight * cweight = (Weight*)m_Weight;

		g_word = m_lSentence[g].first();
		g_tag = m_lSentence[g].second();

		p_tag = m_lSentence[p].second();

		c_word = m_lSentence[c].first();
		c_tag = m_lSentence[c].second();

		m_nDir = encodeLinkDistanceOrDirection(g, p, true);

		tag_tag_tag_int.refer(g_tag, p_tag, c_tag, 0);
		cweight->m_mapGpPpCp.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapGpPpCp.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_int.refer(g_tag, c_tag, 0);
		cweight->m_mapGpCp.getOrUpdateScore(m_nRetval, tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_int.referLast(m_nDir);
		cweight->m_mapGpCp.getOrUpdateScore(m_nRetval, tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_int.refer(g_word, c_word, 0);
		cweight->m_mapGwCw.getOrUpdateScore(m_nRetval, word_word_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_int.referLast(m_nDir);
		cweight->m_mapGwCw.getOrUpdateScore(m_nRetval, word_word_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_int.refer(g_word, c_tag, 0);
		cweight->m_mapGwCp.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_int.referLast(m_nDir);
		cweight->m_mapGwCp.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_int.refer(c_word, g_tag, 0);
		cweight->m_mapCwGp.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_int.referLast(m_nDir);
		cweight->m_mapCwGp.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
	}

	void DepParser::getOrUpdateGrandScore(const int & g, const int & p, const int & c, const int & c2, const int & amount) {
		Weight * cweight = (Weight*)m_Weight;

		g_word = m_lSentence[g].first();
		g_tag = m_lSentence[g].second();

		p_word = m_lSentence[p].first();
		p_tag = m_lSentence[p].second();

		c_word = IS_NULL(c) ? empty_taggedword.first() : m_lSentence[c].first();
		c_tag = IS_NULL(c) ? empty_taggedword.second() : m_lSentence[c].second();

		c2_word = m_lSentence[c2].first();
		c2_tag = m_lSentence[c2].second();

		m_nDir = encodeLinkDistanceOrDirection(g, p, true);

		// word tag tag tag
		word_tag_tag_tag_int.refer(g_word, p_tag, c_tag, c2_tag, 0);
		cweight->m_mapGwPpCpC2p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapGwPpCpC2p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_tag_tag_int.refer(p_word, g_tag, c_tag, c2_tag, 0);
		cweight->m_mapPwGpCpC2p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPwGpCpC2p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_tag_tag_int.refer(c_word, g_tag, p_tag, c2_tag, 0);
		cweight->m_mapCwGpPpC2p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapCwGpPpC2p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_tag_tag_int.refer(c2_word, g_tag, p_tag, c_tag, 0);
		cweight->m_mapC2wGpPpCp.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapC2wGpPpCp.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		// word word tag tag
		word_word_tag_tag_int.refer(g_word, p_word, c_tag, c2_tag, 0);
		cweight->m_mapGwPwCpC2p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapGwPwCpC2p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_tag_tag_int.refer(g_word, c_word, p_tag, c2_tag, 0);
		cweight->m_mapGwCwPpC2p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapGwCwPpC2p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_tag_tag_int.refer(g_word, c2_word, p_tag, c_tag, 0);
		cweight->m_mapGwC2wPpCp.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapGwC2wPpCp.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_tag_tag_int.refer(p_word, c_word, g_tag, c2_tag, 0);
		cweight->m_mapPwCwGpC2p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPwCwGpC2p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_tag_tag_int.refer(p_word, c2_word, g_tag, c_tag, 0);
		cweight->m_mapCwC2wGpPp.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapCwC2wGpPp.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_tag_tag_int.refer(c_word, c2_word, g_tag, p_tag, 0);
		cweight->m_mapCwC2wGpPp.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapCwC2wGpPp.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		// tag tag tag tag
		tag_tag_tag_tag_int.refer(g_tag, p_tag, c_tag, c2_tag, 0);
		cweight->m_mapGpPpCpC2p.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapGpPpCpC2p.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		// word tag tag
		word_tag_tag_int.refer(g_word, c_tag, c2_tag, 0);
		cweight->m_mapGwCpC2p.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapGwCpC2p.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_tag_int.refer(c_word, g_tag, c2_tag, 0);
		cweight->m_mapCwGpC2p.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapCwGpC2p.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_tag_int.refer(c2_word, g_tag, c_tag, 0);
		cweight->m_mapC2wGpCp.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapC2wGpCp.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		// word word tag
		word_word_tag_int.refer(g_word, c_word, c2_tag, 0);
		cweight->m_mapGwCwC2p.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_int.referLast(m_nDir);
		cweight->m_mapGwCwC2p.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_tag_int.refer(g_word, c2_word, c_tag, 0);
		cweight->m_mapGwC2wCp.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_int.referLast(m_nDir);
		cweight->m_mapGwC2wCp.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_tag_int.refer(c_word, c2_word, g_tag, 0);
		cweight->m_mapCwC2wGp.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_int.referLast(m_nDir);
		cweight->m_mapCwC2wGp.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		// tag tag tag
		tag_tag_tag_int.refer(g_tag, c_tag, c2_tag, 0);
		cweight->m_mapGpCpC2p.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapGpCpC2p.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

	}
}
