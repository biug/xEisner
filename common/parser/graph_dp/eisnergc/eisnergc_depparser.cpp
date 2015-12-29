#include <cmath>
#include <stack>
#include <algorithm>
#include <set>
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
				// rightGrands should remove g in [l, r - 1]
				while (true) {
					bool finish = true;
					for (auto && itr = m_vecGrandsAsLeft[l].begin(); itr != m_vecGrandsAsLeft[l].end(); ++itr) {
						if (*itr > l && *itr <= r) {
							m_vecGrandsAsLeft[l].erase(itr);
							finish = false;
							break;
						}
					}
					if (finish) {
						break;
					}
				}
				while (true) {
					bool finish = true;
					for (auto && itr = m_vecGrandsAsRight[r].begin(); itr != m_vecGrandsAsRight[r].end(); ++itr) {
						if (*itr >= l && *itr < r) {
							m_vecGrandsAsRight[r].erase(itr);
							finish = false;
							break;
						}
					}
					if (finish) {
						break;
					}
				}

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

//				item.print();
			}
		}
		// best root
		StateItem & item = m_lItems[m_nSentenceLength + 1][0];
		item.r2l.clear();
		for (int m = 0; m < m_nSentenceLength; ++m) {
			StateItem & litem = m_lItems[m + 1][0];
			StateItem & ritem = m_lItems[m_nSentenceLength - m][m];
			if (litem.r2l.find(m_nSentenceLength) != litem.r2l.end() && ritem.l2r.find(m_nSentenceLength) != ritem.l2r.end()) {
//				std::cout << "m = " << m << std::endl;
				item.updateR2L(0, m,
						litem.r2l[m_nSentenceLength].getScore() +
						ritem.l2r[m_nSentenceLength].getScore() +
						m_vecArcScore[m_nSentenceLength][m]);
//				std::cout << "score left is " << litem.r2l[litem.rightGrandsMap[m_nSentenceLength]].getScore() << std::endl;
//				std::cout << "score right is " << ritem.l2r[ritem.leftGrandsMap[m_nSentenceLength]].getScore() << std::endl;
//				std::cout << "score arc is " << m_vecArcScore[m_nSentenceLength][m] << std::endl;
			}
		}
//		std::cout << "total score is " << item.r2l[0].getScore() << " split is " << item.r2l[0].getSplit() << std::endl;
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
		std::unordered_map<int, std::set<int>> children;
		for (int p = 0; p < m_nSentenceLength; ++p) {
			GrandAgendaU gaArc;
			GrandAgendaB gaGC;
			for (int h = 0; h <= m_nSentenceLength; ++h) {
				if (h != p) {
					gaArc.insertItem(ScoreWithSplit(h, m_vecArcScore[h][p]));
					for (int m = 0; m < m_nSentenceLength; ++m) {
						if (m != p && m != h) {
							gaGC.insertItem(ScoreWithBiSplit(h, m, m_vecGrandChildScore[p][m][h]));
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
				if (agenda->getScore() >= 0) {
					grands[p].insert(agenda->getSplit());
					children[agenda->getSplit()].insert(p);
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
				if (agenda->getScore() >= 0) {
					grands[p].insert(agenda->getSplit());
					grands[agenda->getInnerSplit()].insert(p);
					children[agenda->getSplit()].insert(p);
					children[p].insert(agenda->getInnerSplit());
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

//		for (int i = 0; i < m_nSentenceLength; ++i) {
//			std::cout << i << "'s head could be" << std::endl;
//			for (const auto & g : vecGrands[i]) {
//				std::cout << g << " ";
//			}
//			std::cout << std::endl;
//		}
		return true;
	}

	void DepParser::getOrUpdateStackScore(const int & h, const int & m, const int & amount) {

		Weight * cweight = (Weight*)m_Weight;

		h_1_tag = h > 0 ? m_lSentence[h - 1].second() : start_taggedword.second();
		h1_tag = h < m_nSentenceLength - 1 ? m_lSentence[h + 1].second() : end_taggedword.second();
		m_1_tag = m > 0 ? m_lSentence[m - 1].second() : start_taggedword.second();
		m1_tag = m < m_nSentenceLength - 1 ? m_lSentence[m + 1].second() : end_taggedword.second();

		h_word = m_lSentence[h].first();
		h_tag = m_lSentence[h].second();

		m_word = m_lSentence[m].first();
		m_tag = m_lSentence[m].second();

		m_nDis = encodeLinkDistanceOrDirection(h, m, false);
		m_nDir = encodeLinkDistanceOrDirection(h, m, true);

		word_int.refer(h_word, 0);
		cweight->m_mapPw.getOrUpdateScore(m_nRetval, word_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_int.referLast(m_nDis);
		cweight->m_mapPw.getOrUpdateScore(m_nRetval, word_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_int.referLast(m_nDir);
		cweight->m_mapPw.getOrUpdateScore(m_nRetval, word_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_int.refer(h_tag, 0);
		cweight->m_mapPp.getOrUpdateScore(m_nRetval, tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_int.referLast(m_nDis);
		cweight->m_mapPp.getOrUpdateScore(m_nRetval, tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_int.referLast(m_nDir);
		cweight->m_mapPp.getOrUpdateScore(m_nRetval, tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_int.refer(h_word, h_tag, 0);
		cweight->m_mapPwp.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_int.referLast(m_nDis);
		cweight->m_mapPwp.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_int.referLast(m_nDir);
		cweight->m_mapPwp.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_int.refer(m_word, 0);
		cweight->m_mapCw.getOrUpdateScore(m_nRetval, word_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_int.referLast(m_nDis);
		cweight->m_mapCw.getOrUpdateScore(m_nRetval, word_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_int.referLast(m_nDir);
		cweight->m_mapCw.getOrUpdateScore(m_nRetval, word_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_int.refer(m_tag, 0);
		cweight->m_mapCp.getOrUpdateScore(m_nRetval, tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_int.referLast(m_nDis);
		cweight->m_mapCp.getOrUpdateScore(m_nRetval, tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_int.referLast(m_nDir);
		cweight->m_mapCp.getOrUpdateScore(m_nRetval, tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_int.refer(m_word, m_tag, 0);
		cweight->m_mapCwp.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_int.referLast(m_nDis);
		cweight->m_mapCwp.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_int.referLast(m_nDir);
		cweight->m_mapCwp.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_tag_tag_int.refer(h_word, m_word, h_tag, m_tag, 0);
		cweight->m_mapPwpCwp.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPwpCwp.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPwpCwp.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_tag_int.refer(h_word, h_tag, m_tag, 0);
		cweight->m_mapPwpCp.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPwpCp.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPwpCp.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_tag_int.refer(m_word, h_tag, m_tag, 0);
		cweight->m_mapPpCwp.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPpCwp.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpCwp.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_tag_int.refer(h_word, m_word, h_tag, 0);
		cweight->m_mapPwpCw.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_int.referLast(m_nDis);
		cweight->m_mapPwpCw.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_int.referLast(m_nDir);
		cweight->m_mapPwpCw.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_tag_int.refer(h_word, m_word, m_tag, 0);
		cweight->m_mapPwCwp.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_int.referLast(m_nDis);
		cweight->m_mapPwCwp.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_int.referLast(m_nDir);
		cweight->m_mapPwCwp.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_int.refer(h_word, m_word, 0);
		cweight->m_mapPwCw.getOrUpdateScore(m_nRetval, word_word_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_int.referLast(m_nDis);
		cweight->m_mapPwCw.getOrUpdateScore(m_nRetval, word_word_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_int.referLast(m_nDir);
		cweight->m_mapPwCw.getOrUpdateScore(m_nRetval, word_word_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_int.refer(h_tag, m_tag, 0);
		cweight->m_mapPpCp.getOrUpdateScore(m_nRetval, tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_int.referLast(m_nDis);
		cweight->m_mapPpCp.getOrUpdateScore(m_nRetval, tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpCp.getOrUpdateScore(m_nRetval, tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(h_tag, h1_tag, m_1_tag, m_tag, 0);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(h_1_tag, h_tag, m_1_tag, m_tag, 0);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(h_tag, h1_tag, m_tag, m1_tag, 0);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(h_1_tag, h_tag, m_tag, m1_tag, 0);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(empty_taggedword.second(), h1_tag, m_1_tag, m_tag, 0);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(h_1_tag, empty_taggedword.second(), m_1_tag, m_tag, 0);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(empty_taggedword.second(), h1_tag, m_tag, m1_tag, 0);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(h_1_tag, empty_taggedword.second(), m_tag, m1_tag, 0);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(h_tag, h1_tag, m_1_tag, empty_taggedword.second(), 0);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(h_1_tag, h_tag, m_1_tag, empty_taggedword.second(), 0);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(h_tag, h1_tag, empty_taggedword.second(), m1_tag, 0);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(h_1_tag, h_tag, empty_taggedword.second(), m1_tag, 0);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(h_tag, empty_taggedword.second(), m_1_tag, m_tag, 0);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpPp1Cp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(empty_taggedword.second(), h_tag, m_1_tag, m_tag, 0);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPp_1PpCp_1Cp.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(h_tag, h1_tag, m_tag, empty_taggedword.second(), 0);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpPp1CpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_tag_tag_int.refer(h_1_tag, h_tag, m_tag, empty_taggedword.second(), 0);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDis);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPp_1PpCpCp1.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		for (int b = (int)std::fmin(h, m) + 1, e = (int)std::fmax(h, m); b < e; ++b) {
			b_tag = m_lSentence[b].second();
			tag_tag_tag_int.refer(h_tag, b_tag, m_tag, 0);
			cweight->m_mapPpBpCp.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
			tag_tag_tag_int.referLast(m_nDis);
			cweight->m_mapPpBpCp.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
			tag_tag_tag_int.referLast(m_nDir);
			cweight->m_mapPpBpCp.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		}
	}

	void DepParser::getOrUpdateStackScore(const int & g, const int & h, const int & m, const int & amount) {
		Weight * cweight = (Weight*)m_Weight;

		g_word = m_lSentence[g].first();
		g_tag = m_lSentence[g].second();

		h_tag = m_lSentence[h].second();

		m_word = m_lSentence[m].first();
		m_tag = m_lSentence[m].second();

		m_nDir = encodeLinkDistanceOrDirection(g, h, true);

		tag_tag_tag_int.refer(g_tag, h_tag, m_tag, 0);
		cweight->m_mapGpHpMp.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapGpHpMp.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_int.refer(g_tag, m_tag, 0);
		cweight->m_mapGpMp.getOrUpdateScore(m_nRetval, tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_int.referLast(m_nDir);
		cweight->m_mapGpMp.getOrUpdateScore(m_nRetval, tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_int.refer(g_word, m_word, 0);
		cweight->m_mapGwMw.getOrUpdateScore(m_nRetval, word_word_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_int.referLast(m_nDir);
		cweight->m_mapGwMw.getOrUpdateScore(m_nRetval, word_word_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_int.refer(g_word, m_tag, 0);
		cweight->m_mapGwMp.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_int.referLast(m_nDir);
		cweight->m_mapGwMp.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_int.refer(m_word, g_tag, 0);
		cweight->m_mapMwGp.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_int.referLast(m_nDir);
		cweight->m_mapMwGp.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
	}
}
