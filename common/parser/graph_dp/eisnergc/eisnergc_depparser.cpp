#include <set>
#include <cmath>
#include <stack>
#include <cstring>
#include <algorithm>
#include <unordered_set>

#include "eisnergc_depparser.h"
#include "common/token/word.h"
#include "common/token/pos.h"

namespace eisnergc {

	WordPOSTag DepParser::empty_taggedword = WordPOSTag();
	WordPOSTag DepParser::start_taggedword = WordPOSTag();
	WordPOSTag DepParser::end_taggedword = WordPOSTag();

	DepParser::DepParser(const std::string & sFeatureInput, const std::string & sFeatureOut, int nState) :
		DepParserBase(nState) {

		m_nSentenceLength = 0;

		std::string sFeature = sFeatureInput.substr(0, sFeatureInput.find('#'));
		std::string sFeature1st = sFeatureInput.substr(sFeatureInput.find('#') + 1);
		std::string sFeatureO = sFeatureOut.substr(0, sFeatureOut.find('#'));
		m_pWeight1st = m_nState == ParserState::GOLDTEST ? nullptr : new Weight1st(sFeature1st, sFeature1st, ParserState::PARSE);
		m_pWeight = m_nState == ParserState::GOLDTEST ? nullptr : new Weightgc(sFeature, sFeatureO, nState);
		std::string sItr = sFeatureO.substr(sFeatureO.find("eisnergc") + strlen("eisnergc")).substr(0, 2);
		if (!isdigit(sItr[1])) sItr = sItr.substr(0, 1);
		m_nIteration = atoi(sItr.c_str());
		std::cout << "Iteration " << m_nIteration << std::endl;

		DepParser::empty_taggedword.refer(TWord::code(EMPTY_WORD), TPOSTag::code(EMPTY_POSTAG));
		DepParser::start_taggedword.refer(TWord::code(START_WORD), TPOSTag::code(START_POSTAG));
		DepParser::end_taggedword.refer(TWord::code(END_WORD), TPOSTag::code(END_POSTAG));
	}

	DepParser::~DepParser() {
		if (m_pWeight1st) delete m_pWeight1st;
		if (m_pWeight) delete m_pWeight;
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
		Arcs2BiArcs(m_vecCorrectArcs, m_vecCorrectBiArcs);

		if (m_nState == ParserState::GOLDTEST) {
			m_setFirstGoldScore.clear();
			m_setSecondGoldScore.clear();
			for (const auto & arc : m_vecCorrectArcs) {
				m_setFirstGoldScore.insert(BiGram<int>(arc.first() == -1 ? m_nSentenceLength : arc.first(), arc.second()));
			}
			for (const auto & biarc : m_vecCorrectBiArcs) {
				m_setSecondGoldScore.insert(TriGram<int>(biarc.first(), biarc.second(), biarc.third()));
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

		initArcScore();
		initGrandChildScore();
		for (int level = 0; !initGrands(level); ++level);

		for (int i = 0; i < m_nSentenceLength; ++i) {
			StateItem & item = m_lItems[1][i];
			item.init(i, i);
			for (const auto & g : m_vecGrandsAsLeft[i]) {
				item.l2r[g] = ScoreWithSplit();
				item.r2l[g] = ScoreWithSplit();
				item.l2r_im[g] = ScoreWithSplit();
				item.r2l_im[g] = ScoreWithSplit();
			}
			for (const auto & g : m_vecGrandsAsRight[i]) {
				item.l2r[g] = ScoreWithSplit();
				item.r2l[g] = ScoreWithSplit();
				item.l2r_im[g] = ScoreWithSplit();
				item.r2l_im[g] = ScoreWithSplit();
			}
		}

		for (int d = 2; d <= m_nSentenceLength; ++d) {
			for (int l = 0, n = m_nSentenceLength - d + 1; l < n; ++l) {
				int r = l + d - 1;
				StateItem & item = m_lItems[d][l];
				item.init(l, r);
				// remove grands in vecGrands
				// leftGrands should remove g in [l + 1, r]
				std::vector<int> newGrandsAsLeft;
				for (const auto & grand : m_vecGrandsAsLeft[l]) {
					if (grand < l || grand > r) {
						newGrandsAsLeft.push_back(grand);
					}
				}
				m_vecGrandsAsLeft[l] = newGrandsAsLeft;
				// rightGrands should remove g in [l, r - 1]
				std::vector<int> newGrandsAsRight;
				for (const auto & grand : m_vecGrandsAsRight[r]) {
					if (grand < l || grand > r) {
						newGrandsAsRight.push_back(grand);
					}
				}
				m_vecGrandsAsRight[r] = newGrandsAsRight;

				for (int m = l + 1; m < r; ++m) {
					StateItem & ritem = m_lItems[r - m + 1][m];
					StateItem & litem = m_lItems[m - l + 1][l];
					bool useRight = ritem.l2r.find(l) != ritem.l2r.end();
					bool useLeft = litem.r2l.find(r) != litem.r2l.end();
					if (useRight) {
						tscore rPartialScore = ritem.l2r[l].getScore();
						for (const auto & g : m_vecGrandsAsLeft[l]) {
							if (litem.l2r_im.find(g) != litem.l2r_im.end()) {
								item.updateL2R(g, m, litem.l2r_im[g].getScore() + rPartialScore);
							}
						}
					}
					if (useLeft) {
						tscore lPartialScore = litem.r2l[r].getScore();
						for (const auto & g : m_vecGrandsAsRight[r]) {
							if (ritem.r2l_im.find(g) != ritem.r2l_im.end()) {
								item.updateR2L(g, m, ritem.r2l_im[g].getScore() + lPartialScore);
							}
						}
					}
				}
				tscore & l2rArcScore = m_vecArcScore[l][r];
				tscore & r2lArcScore = m_vecArcScore[r][l];

				for (int m = l; m < r; ++m) {
					StateItem & ritem = m_lItems[r - m][m + 1];
					StateItem & litem = m_lItems[m - l + 1][l];
					bool useRight = ritem.r2l.find(l) != ritem.r2l.end();
					bool useLeft = litem.l2r.find(r) != litem.l2r.end();
					if (useRight) {
						tscore l2rPartialScore = ritem.r2l[l].getScore() + l2rArcScore;
						for (const auto & g : m_vecGrandsAsLeft[l]) {
							if (litem.l2r.find(g) != litem.l2r.end()) {
								item.updateL2RIm(g, m, litem.l2r[g].getScore() + l2rPartialScore + m_vecGrandChildScore[l][r][g]);
							}
						}
					}
					if (useLeft) {
						tscore r2lPartialScore = litem.l2r[r].getScore() + r2lArcScore;
						for (const auto & g : m_vecGrandsAsRight[r]) {
							if (ritem.r2l.find(g) != ritem.r2l.end()) {
								item.updateR2LIm(g, m, ritem.r2l[g].getScore() + r2lPartialScore + m_vecGrandChildScore[r][l][g]);
							}
						}
					}
				}

				for (const auto & g : item.l2r_im) {
					item.updateL2R(g.first, r, g.second.getScore());
				}
				for (const auto & g : item.r2l_im) {
					item.updateR2L(g.first, l, g.second.getScore());
				}
			}
		}
		// best root
		StateItem & item = m_lItems[m_nSentenceLength + 1][0];
		item.r2l.clear();
		for (int m = 0; m < m_nSentenceLength; ++m) {
			StateItem & litem = m_lItems[m + 1][0];
			StateItem & ritem = m_lItems[m_nSentenceLength - m][m];
			if (litem.r2l.find(m_nSentenceLength) != litem.r2l.end() && ritem.l2r.find(m_nSentenceLength) != ritem.l2r.end()) {
				item.updateR2L(0, m,
						litem.r2l[m_nSentenceLength].getScore() +
						ritem.l2r[m_nSentenceLength].getScore() +
						m_vecArcScore[m_nSentenceLength][m]);
			}
		}
	}

	void DepParser::decodeArcs() {

		m_vecTrainArcs.clear();
		std::stack<std::tuple<int, int, int>> stack;

		int s = m_lItems[m_nSentenceLength + 1][0].r2l[0].getSplit();
		m_vecTrainArcs.push_back(Arc(-1, s));

		m_lItems[s + 1][0].type = R2L_COMP;
		stack.push(std::tuple<int, int, int>(s + 1, m_nSentenceLength, 0));
		m_lItems[m_nSentenceLength - s][s].type = L2R_COMP;
		stack.push(std::tuple<int, int, int>(m_nSentenceLength - s, m_nSentenceLength, s));

		while (!stack.empty()) {

			auto span = stack.top();
			stack.pop();

			int split, grand = std::get<1>(span);
			StateItem & item = m_lItems[std::get<0>(span)][std::get<2>(span)];

			if (item.left == item.right) {
				continue;
			}

			switch (item.type) {
			case L2R_COMP:
				split = item.l2r[grand].getSplit();

				m_lItems[split - item.left + 1][item.left].type = L2R_IM_COMP;
				stack.push(std::tuple<int, int, int>(split - item.left + 1, grand, item.left));
				m_lItems[item.right - split + 1][split].type = L2R_COMP;
				stack.push(std::tuple<int, int, int>(item.right - split + 1, item.left, split));
				break;
			case R2L_COMP:
				split = item.r2l[grand].getSplit();

				m_lItems[item.right - split + 1][split].type = R2L_IM_COMP;
				stack.push(std::tuple<int, int, int>(item.right - split + 1, grand, split));
				m_lItems[split - item.left + 1][item.left].type = R2L_COMP;
				stack.push(std::tuple<int, int, int>(split - item.left + 1, item.right, item.left));
				break;
			case L2R_IM_COMP:
				m_vecTrainArcs.push_back(Arc(item.left, item.right));

				split = item.l2r_im[grand].getSplit();

				m_lItems[split - item.left + 1][item.left].type = L2R_COMP;
				stack.push(std::tuple<int, int, int>(split - item.left + 1, grand, item.left));
				m_lItems[item.right - split][split + 1].type = R2L_COMP;
				stack.push(std::tuple<int, int, int>(item.right - split, item.left, split + 1));
				break;
			case R2L_IM_COMP:
				m_vecTrainArcs.push_back(Arc(item.right, item.left));

				split = item.r2l_im[grand].getSplit();

				m_lItems[item.right - split][split + 1].type = R2L_COMP;
				stack.push(std::tuple<int, int, int>(item.right - split, grand, split + 1));
				m_lItems[split - item.left + 1][item.left].type = L2R_COMP;
				stack.push(std::tuple<int, int, int>(split - item.left + 1, item.right, item.left));
				break;
			default:
				break;
			}
		}
	}

	void DepParser::update() {
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
			if (arc.first() != -1) {
				getOrUpdateStackScore(arc.first(), arc.second(), arc.third(), 1);
			}
			getOrUpdateStackScore(arc.second(), arc.third(), 1);
		}
		for (const auto & arc : negativeArcs) {
			if (arc.first() != -1) {
				getOrUpdateStackScore(arc.first(), arc.second(), arc.third(), -1);
			}
			getOrUpdateStackScore(arc.second(), arc.third(), -1);
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
		Arcs2BiArcs(m_vecTrainArcs, m_vecTrainBiArcs);
		if (m_vecCorrectArcs.size() != m_vecTrainArcs.size() || m_lItems[m_nSentenceLength + 1][0].r2l[0].getScore() / GOLD_POS_SCORE != 2 * m_vecTrainArcs.size() - 1) {
			std::cout << "gold parse len error at " << m_nTrainingRound << std::endl;
			std::cout << "score is " << m_lItems[m_nSentenceLength + 1][0].r2l[0].getScore() << std::endl;
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

	const tscore & DepParser::arc1stScore(const int & h, const int & m) {
		if (m_nState == ParserState::GOLDTEST) return m_nRetval = 0;
		m_nRetval = 0;
		get1stStackScore(h, m);
		return m_nRetval;
	}

	const tscore & DepParser::arcScore(const int & h, const int & m) {
		if (m_nState == ParserState::GOLDTEST) {
			m_nRetval = m_setFirstGoldScore.find(BiGram<int>(h, m)) == m_setFirstGoldScore.end() ? GOLD_NEG_SCORE : GOLD_POS_SCORE;
			return m_nRetval;
		}
		m_nRetval = 0;
		getOrUpdateStackScore(h, m, 0);
		return m_nRetval;
	}

	const tscore & DepParser::biArcScore(const int & g, const int & h, const int & m) {
		if (m_nState == ParserState::GOLDTEST) {
			m_nRetval = m_setSecondGoldScore.find(TriGram<int>(g, h, m)) == m_setSecondGoldScore.end() ? GOLD_NEG_SCORE : GOLD_POS_SCORE;
			return m_nRetval;
		}
		m_nRetval = 0;
		getOrUpdateStackScore(g, h, m, 0);
		return m_nRetval;
	}

	void DepParser::initArcScore() {
		m_vecArcScore.clear();
		for (int i = 0; i < m_nSentenceLength; ++i) {
			std::vector<tscore> vecScores;
			for (int j = 0; j < m_nSentenceLength; ++j) {
				vecScores.push_back(j == i ? 0 : arcScore(i, j));
			}
			m_vecArcScore.push_back(vecScores);
		}
		std::vector<tscore> rootScores;
		for (int i = 0; i < m_nSentenceLength; ++i) {
			rootScores.push_back(arcScore(m_nSentenceLength, i));
		}
		m_vecArcScore.push_back(rootScores);
	}

	void DepParser::initGrandChildScore() {
		m_vecGrandChildScore.clear();
		for (int i = 0; i < m_nSentenceLength; ++i) {
			std::vector<std::vector<tscore>> vecVecScores;
			for (int j = 0; j < m_nSentenceLength; ++j) {
				std::vector<tscore> vecScores;
				if (j != i) {
					for (int g = 0; g <= m_nSentenceLength; ++g) {
						vecScores.push_back(g == i || g == j ? 0 : biArcScore(g, i, j));
					}
				}
				vecVecScores.push_back(vecScores);
			}
			m_vecGrandChildScore.push_back(vecVecScores);
		}
	}

	bool DepParser::initGrands(int level) {
//		std::cout << "level is " << level << std::endl;

		std::vector<std::vector<int>> vecGrands;
		m_vecGrandsAsLeft.clear();
		m_vecGrandsAsRight.clear();

		if (level < 0) {
			return false;
		}
//		if (level > GRAND_MAX_LEVEL) {
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
//		}

		tscore score;
		std::unordered_map<int, std::set<int>> grands;
		for (int p = 0; p < m_nSentenceLength; ++p) {
			GrandAgendaA gaA;
			GrandAgendaB gaGC;
			for (int h = 0; h <= m_nSentenceLength; ++h) {
				if (h != p) {
					if (m_nState == ParserState::TRAIN) {
						score = arc1stScore(h, p) + m_vecArcScore[h][p] * 8237 * 20;
					}
					else {
						score = arc1stScore(h, p) * m_nIteration + m_vecArcScore[h][p] * 20;
					}
					if (score >= 0) {
						gaA.insertItem(ScoreWithSplit(h, score));
					}
					for (int m = 0; m < m_nSentenceLength; ++m) {
						if (m != p && m != h) {
							score = m_vecArcScore[p][m] + m_vecGrandChildScore[p][m][h];
							if (score >= 0) {
								gaGC.insertItem(ScoreWithBiSplit(h, m, score));
							}
						}
					}
				}
			}
			// sort items
			gaA.sortItems();
			gaGC.sortItems();
			int size = (GRAND_AGENDA_SIZE << level);
			int i = 0;
			for (const auto & agenda :gaA) {
				grands[p].insert(agenda->getSplit());
				if (++i >= size) {
					break;
				}
			}
			i = 0;
			for (const auto & agenda : gaGC) {
				grands[p].insert(agenda->getSplit());
				grands[agenda->getInnerSplit()].insert(p);
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


	void DepParser::get1stStackScore(const int & h, const int & m) {
		m_pWeight1st->getOrUpdateArcScore(m_nRetval, h, m, 0, m_nSentenceLength, m_lSentence);
	}

	void DepParser::getOrUpdateStackScore(const int & h, const int & m, const int & amount) {
		m_pWeight->getOrUpdateArcScore(m_nRetval, h, m, amount, m_nSentenceLength, m_lSentence);
	}

	void DepParser::getOrUpdateStackScore(const int & g, const int & h, const int & m, const int & amount) {
		m_pWeight->getOrUpdateGrandArcScore(m_nRetval, g, h, m, amount, m_nSentenceLength, m_lSentence);
	}
}
