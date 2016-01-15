#include <cmath>
#include <stack>
#include <algorithm>
#include <unordered_set>

#include "emptyeisnergc2nd_depparser.h"
#include "common/token/word.h"
#include "common/token/pos.h"
#include "common/token/emp.h"

namespace emptyeisnergc2nd {

	WordPOSTag DepParser::empty_taggedword = WordPOSTag();
	WordPOSTag DepParser::start_taggedword = WordPOSTag();
	WordPOSTag DepParser::end_taggedword = WordPOSTag();

	DepParser::DepParser(const std::string & sFeatureInput, const std::string & sFeatureOut, int nState) :
		DepParserBase(nState) {

		m_nSentenceLength = 0;

		m_pWeight = new Weight(sFeatureInput, sFeatureOut);

		DepParser::empty_taggedword.refer(TWord::code(EMPTY_WORD), TPOSTag::code(EMPTY_POSTAG));
		DepParser::start_taggedword.refer(TWord::code(START_WORD), TPOSTag::code(START_POSTAG));
		DepParser::end_taggedword.refer(TWord::code(END_WORD), TPOSTag::code(END_POSTAG));
	}

	DepParser::~DepParser() {
		delete m_pWeight;
	}

	void DepParser::train(const DependencyTree & correct, const int & round) {
		// initialize
		m_vecCorrectArcs.clear();
		m_nTrainingRound = round;
		m_nSentenceLength = 0;

		readEmptySentAndArcs(correct);

		Arcs2TriArcs(m_vecCorrectArcs, m_vecCorrectTriArcs);

		if (m_nState == ParserState::GOLDTEST) {
			m_setArcGoldScore.clear();
			m_setBiSiblingArcGoldScore.clear();
			m_setGrandSiblingArcGoldScore.clear();
			m_setGrandBiSiblingArcGoldScore.clear();
			for (const auto & triarc : m_vecCorrectTriArcs) {

				m_setArcGoldScore.insert(BiGram<int>(triarc.second(), triarc.forth()));
				m_setBiSiblingArcGoldScore.insert(TriGram<int>(triarc.second(), triarc.third(), triarc.forth()));
				if (triarc.first() != -1) {
					m_setGrandSiblingArcGoldScore.insert(TriGram<int>(triarc.first(), triarc.second(), triarc.forth()));
					m_setGrandBiSiblingArcGoldScore.insert(QuarGram<int>(triarc.first(), triarc.second(), triarc.third(), triarc.forth()));
				}
			}
		}

		work(nullptr, correct);
		if (m_nTrainingRound % OUTPUT_STEP == 0) {
			std::cout << m_nTotalErrors << " / " << m_nTrainingRound << std::endl;
			//printTime();
		}
	}

	void DepParser::parse(const Sentence & sentence, DependencyTree * retval) {
		int idx = 0;
		m_nTrainingRound = 0;
		DependencyTree correct;
		m_nSentenceLength = sentence.size();
		for (const auto & token : sentence) {
			m_lSentence[idx][0].refer(TWord::code(SENT_WORD(token)), TPOSTag::code(SENT_POSTAG(token)));
			for (int i = TEmptyTag::start(), max_i = TEmptyTag::end(); i < max_i; ++i) {
				m_lSentence[idx][i].refer(
					TWord::code(
					(idx == 0 ? START_WORD : TWord::key(m_lSentence[idx - 1][0].first())) +
					TEmptyTag::key(i) + SENT_WORD(token)
					),
					TPOSTag::code(TEmptyTag::key(i))
					);
			}
			correct.push_back(DependencyTreeNode(token, -1, NULL_LABEL));
			++idx;
		}
		m_lSentence[idx][0].refer(TWord::code(ROOT_WORD), TPOSTag::code(ROOT_POSTAG));
		for (int i = TEmptyTag::start(), max_i = TEmptyTag::end(); i < max_i; ++i) {
			m_lSentence[idx][i].refer(
				TWord::code(
				TWord::key(m_lSentence[idx - 1][0].first()) + TEmptyTag::key(i) + END_WORD
				),
				TPOSTag::code(TEmptyTag::key(i))
				);
		}
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
				item.l2r_solid_both[g] = ScoreWithSplit();
				item.r2l_solid_both[g] = ScoreWithSplit();
				item.l2r_solid_outside[g] = ScoreWithSplit();
				item.r2l_solid_outside[g] = ScoreWithSplit();
				item.l2r_empty_inside[g] = ScoreWithBiSplit();
				item.r2l_empty_inside[g] = ScoreWithBiSplit();
				item.l2r_empty_outside[g] = ScoreAgenda();
				item.r2l_empty_outside[g] = ScoreAgenda();
			}
			for (const auto & g : m_vecGrandsAsRight[i]) {
				item.jux[g] = ScoreWithSplit();
				item.l2r[g] = ScoreWithSplit();
				item.r2l[g] = ScoreWithSplit();
				item.l2r_solid_outside[g] = ScoreWithSplit();
				item.r2l_solid_outside[g] = ScoreWithSplit();
				item.l2r_empty_inside[g] = ScoreWithBiSplit();
				item.r2l_empty_inside[g] = ScoreWithBiSplit();
				item.l2r_empty_outside[g] = ScoreAgenda();
				item.r2l_empty_outside[g] = ScoreAgenda();
			}
		}

		for (int d = 1; d <= m_nSentenceLength; ++d) {

			m_lItems[d].reserve(m_nSentenceLength - d + 1);

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

				for (int s = i; s < l; ++s) {
					StateItem & litem = m_lItems[s - i + 1][i];
					StateItem & ritem = m_lItems[l - s][s + 1];

					for (const auto & g : m_vecGrandsAsLeft[i]) {
						// jux
						if (litem.l2r.find(g) != litem.l2r.end() && ritem.r2l.find(g) != ritem.r2l.end()) {
							item.updateJUX(g, s, litem.l2r[g].getScore() + ritem.r2l[g].getScore());
						}
					}

					// l2r_empty_inside
					if (ritem.r2l.find(i) != ritem.r2l.end()) {
						tscore l_empty_inside_base_score = l2r_arc_score + ritem.r2l[i].getScore();
						for (const auto & g : m_vecGrandsAsLeft[i]) {
							if (litem.l2r_empty_outside.find(g) != litem.l2r_empty_outside.end()) {
								for (const auto & l_beam : litem.l2r_empty_outside[g]) {
									int p = ENCODE_EMPTY(s, DECODE_EMPTY_TAG(l_beam->getSplit()));
									int k = ENCODE_EMPTY(s + 1, DECODE_EMPTY_TAG(l_beam->getSplit()));
									int j = DECODE_EMPTY_POS(l_beam->getSplit());
									item.updateL2REmptyInside(g, p, l_beam->getSplit(), l_empty_inside_base_score +
										l_beam->getScore() +
										biSiblingArcScore(i, k, l) + // empty node bisibling
										biSiblingArcScore(i, j == i ? -1 : j, l) + // solid node bisibling
										m_vecGrandChildScore[i][l][g] +
										grandBiSiblingArcScore(g, i, k, l) + // empty node grand bisibling
										grandBiSiblingArcScore(g, i, j == i ? -1 : j, l)); // solid node grand bisibling
								}
							}
						}
					}

					// r2l_empty_inside
					if (litem.l2r.find(l) != litem.l2r.end()) {
						tscore r_empty_inside_base_score = r2l_arc_score + litem.l2r[l].getScore();
						for (const auto & g : m_vecGrandsAsRight[l]) {
							if (ritem.r2l_empty_outside.find(g) != ritem.r2l_empty_outside.end()) {
								for (const auto & r_beam : ritem.r2l_empty_outside[g]) {
									int p = ENCODE_EMPTY(s, DECODE_EMPTY_TAG(r_beam->getSplit()));
									int k = ENCODE_EMPTY(s + 1, DECODE_EMPTY_TAG(r_beam->getSplit()));
									int j = DECODE_EMPTY_POS(r_beam->getSplit());
									item.updateR2LEmptyInside(g, p, r_beam->getSplit(), r_empty_inside_base_score +
										r_beam->getScore() +
										biSiblingArcScore(l, k, i) +
										biSiblingArcScore(l, j == l ? -1 : j, i) +
										m_vecGrandChildScore[l][i][g] +
										grandBiSiblingArcScore(g, l, k, i) +
										grandBiSiblingArcScore(g, l, j == l ? -1 : j, i));
								}
							}
						}
					}
				}

				for (int k = i + 1; k < l; ++k) {

					StateItem & litem = m_lItems[k - i + 1][i];
					StateItem & ritem = m_lItems[l - k + 1][k];

					// l2r solid both
					if (ritem.jux.find(i) != ritem.jux.end()) {
						tscore l_base_jux = ritem.jux[i].getScore() + l2r_arc_score + m_vecBiSiblingScore[i][l][k];
						for (const auto & g : m_vecGrandsAsLeft[i]) {
							if (litem.l2r_solid_outside.find(g) != litem.l2r_solid_outside.end()) {
								item.updateL2RSolidBoth(g, k, l_base_jux + m_vecGrandChildScore[i][l][g] + grandBiSiblingArcScore(g, i, k, l) + litem.l2r_solid_outside[g].getScore());
							}
						}
					}
					// r2l solid both
					if (litem.jux.find(l) != litem.jux.end()) {
						tscore r_base_jux = litem.jux[l].getScore() + r2l_arc_score + m_vecBiSiblingScore[l][i][k];
						for (const auto & g : m_vecGrandsAsRight[l]) {
							if (ritem.r2l_solid_outside.find(g) != ritem.r2l_solid_outside.end()) {
								item.updateR2LSolidBoth(g, k, r_base_jux + m_vecGrandChildScore[l][i][g] + grandBiSiblingArcScore(g, l, k, i) + ritem.r2l_solid_outside[g].getScore());
							}
						}
					}

					if (ritem.l2r.find(i) != ritem.l2r.end()) {
						for (const auto & g : m_vecGrandsAsLeft[i]) {
							if (litem.l2r_solid_outside.find(g) != litem.l2r_solid_outside.end()) {
								// l2r_empty_outside
								for (const auto & swt : m_vecFirstOrderEmptyScore[i][l]) {
									const int & t = swt->getType();
									// s is split with type
									int s = ENCODE_EMPTY(k, t);
									// o is outside empty
									int o = ENCODE_EMPTY(l + 1, t);
									tscore l_empty_outside_base_score = ritem.l2r[i].getScore() + swt->getScore() + biSiblingArcScore(i, k, o);
									item.updateL2REmptyOutside(g, s, l_empty_outside_base_score +
										litem.l2r_solid_outside[g].getScore() +
										grandSiblingArcScore(g, i, o) +
										grandBiSiblingArcScore(g, i, k, o));
								}
								// complete
								item.updateL2R(g, k, litem.l2r_solid_outside[g].getScore() + ritem.l2r[i].getScore());
							}
						}
					}

					if (litem.r2l.find(l) != litem.r2l.end()) {
						for (const auto & g : m_vecGrandsAsRight[l]) {
							if (ritem.r2l_solid_outside.find(g) != ritem.r2l_solid_outside.end()) {
								// r2l_empty_outside
								for (const auto & swt : m_vecFirstOrderEmptyScore[l][i]) {
									const int & t = swt->getType();
									int s = ENCODE_EMPTY(k, t);
									int o = ENCODE_EMPTY(i, t);
									tscore r_empty_outside_base_score = litem.r2l[l].getScore() + swt->getScore() + biSiblingArcScore(l, k, o);
									item.updateR2LEmptyOutside(g, s, r_empty_outside_base_score +
										ritem.r2l_solid_outside[g].getScore() +
										grandSiblingArcScore(g, l, o) +
										grandBiSiblingArcScore(g, l, k, o));
								}
								// complete
								item.updateR2L(g, k, ritem.r2l_solid_outside[g].getScore() + litem.r2l[l].getScore());
							}
						}
					}
				}

				if (d > 1) {
					StateItem & litem = m_lItems[d - 1][i];
					StateItem & ritem = m_lItems[d - 1][i + 1];

					if (ritem.r2l.find(i) != ritem.r2l.end()) {
						for (const auto & g : m_vecGrandsAsLeft[i]) {
							// l2r_solid_both
							item.updateL2RSolidBoth(g, i, ritem.r2l[i].getScore() +
								l2r_arc_score +
								m_vecBiSiblingScore[i][l][i] +
								m_vecGrandChildScore[i][l][g] +
								grandBiSiblingArcScore(g, i, -1, l));
						}
					}
					if (litem.l2r.find(l) != litem.l2r.end()) {
						for (const auto & g : m_vecGrandsAsRight[l]) {
							// r2l_solid_both
							item.updateR2LSolidBoth(g, l, litem.l2r[l].getScore() +
								r2l_arc_score +
								m_vecBiSiblingScore[l][i][l] +
								m_vecGrandChildScore[l][i][g] +
								grandBiSiblingArcScore(g, l, -1, i));
						}
					}
					for (const auto & g : m_vecGrandsAsLeft[i]) {
						if (item.l2r_solid_both.find(g) != item.l2r_solid_both.end()) {
							item.updateL2R(g, l, item.l2r_solid_both[g].getScore());
							item.updateL2RSolidOutside(g, item.l2r_solid_both[g].getSplit(), item.l2r_solid_both[g].getScore());
						}
						if (item.l2r_empty_inside.find(g) != item.l2r_empty_inside.end()) {
							item.updateL2RSolidOutside(g, item.l2r_empty_inside[g].getSplit(), item.l2r_empty_inside[g].getScore());
						}

					}
					for (const auto & g : m_vecGrandsAsRight[l]) {
						if (item.r2l_solid_both.find(g) != item.r2l_solid_both.end()) {
							item.updateR2L(g, i, item.r2l_solid_both[g].getScore());
							item.updateR2LSolidOutside(g, item.r2l_solid_both[g].getSplit(), item.r2l_solid_both[g].getScore());
						}
						if (item.r2l_empty_inside.find(g) != item.r2l_empty_inside.end()) {
							item.updateR2LSolidOutside(g, item.r2l_empty_inside[g].getSplit(), item.r2l_empty_inside[g].getScore());
						}
					}
				}
				if (d > 1) {
					if (m_lItems[1][l].l2r.find(i) != m_lItems[1][l].l2r.end()) {
						for (const auto & g : m_vecGrandsAsLeft[i]) {
							if (item.l2r_solid_outside.find(g) != item.l2r_solid_outside.end()) {
								// l2r_empty_ouside
								for (const auto & swt : m_vecFirstOrderEmptyScore[i][l]) {
									const int & t = swt->getType();
									int s = ENCODE_EMPTY(l, t);
									int o = ENCODE_EMPTY(l + 1, t);
									tscore base_l2r_empty_outside_score = swt->getScore() + biSiblingArcScore(i, l, o) + m_lItems[1][l].l2r[i].getScore();
									item.updateL2REmptyOutside(g, s, base_l2r_empty_outside_score +
										item.l2r_solid_outside[g].getScore() +
										grandSiblingArcScore(g, i, o) +
										grandBiSiblingArcScore(g, i, l, o));
								}
								// l2r
								item.updateL2R(g, l, item.l2r_solid_outside[g].getScore() + m_lItems[1][l].l2r[i].getScore());
							}
						}
					}

					if (m_lItems[1][i].r2l.find(l) != m_lItems[1][i].r2l.end()) {
						for (const auto & g : m_vecGrandsAsRight[l]) {
							if (item.r2l_solid_outside.find(g)!= item.r2l_solid_outside.end()) {
								// r2l_empty_outside
								for (const auto & swt : m_vecFirstOrderEmptyScore[l][i]) {
										const int & t = swt->getType();
										int s = ENCODE_EMPTY(i, t);
										int o = ENCODE_EMPTY(i, t);
										tscore base_r2l_empty_outside_score = swt->getScore() + biSiblingArcScore(l, i, o) + m_lItems[1][i].r2l[l].getScore();
										item.updateR2LEmptyOutside(g, s, base_r2l_empty_outside_score +
											item.r2l_solid_outside[g].getScore() +
											grandSiblingArcScore(g, l, o) +
											grandBiSiblingArcScore(g, l, i, o));
								}
								// r2l
								item.updateR2L(g, i, item.r2l_solid_outside[g].getScore() + m_lItems[1][i].r2l[l].getScore());
							}
						}
					}
				}
				else {
					for (const auto & g : m_vecGrandsAsLeft[i]) {
						// l2r_empty_ouside
						for (const auto & swt : m_vecFirstOrderEmptyScore[i][l]) {
							const int & t = swt->getType();
							int s = ENCODE_EMPTY(l, t);
							int o = ENCODE_EMPTY(l + 1, t);
							item.updateL2REmptyOutside(g, s, swt->getScore() +
								biSiblingArcScore(i, -1, o) +
								grandSiblingArcScore(g, i, o) +
								grandBiSiblingArcScore(g, i, -1, o));
						}
						// l2r
						item.updateL2R(g, l, 0);
					}
					for (const auto & g : m_vecGrandsAsRight[l]) {
						// r2l_empty_outside
						for (const auto & swt : m_vecFirstOrderEmptyScore[l][i]) {
							const int & t = swt->getType();
							int s = ENCODE_EMPTY(i, t);
							int o = ENCODE_EMPTY(i, t);
							item.updateR2LEmptyOutside(g, s, swt->getScore() +
								biSiblingArcScore(l, -1, o) +
								grandSiblingArcScore(g, l, o) +
								grandBiSiblingArcScore(g, l, -1, o));
						}
						// r2l
						item.updateR2L(g, i, 0);
					}
				}

				for (const auto & g : m_vecGrandsAsLeft[i]) {
					if (item.l2r_empty_outside.find(g) != item.l2r_empty_outside.end()) {
						// l2r
						item.updateL2R(g, item.l2r_empty_outside[g].bestItem().getSplit(), item.l2r_empty_outside[g].bestItem().getScore());
					}
				}
				for (const auto & g : m_vecGrandsAsRight[l]) {
					// r2l
					if (item.r2l_empty_outside.find(g) != item.r2l_empty_outside.end()) {
						item.updateR2L(g, item.r2l_empty_outside[g].bestItem().getSplit(), item.r2l_empty_outside[g].bestItem().getScore());
					}
				}
			}
		}
		// best root
		m_lItems[m_nSentenceLength + 1].push_back(StateItem());
		StateItem & item = m_lItems[m_nSentenceLength + 1][0];
		item.r2l[0] = ScoreWithSplit();
		for (int m = 0; m < m_nSentenceLength; ++m) {
			if (m_lItems[m + 1][0].r2l.find(m_nSentenceLength) != m_lItems[m + 1][0].r2l.end() &&
				m_lItems[m_nSentenceLength - m][m].l2r.find(m_nSentenceLength) != m_lItems[m_nSentenceLength - m][m].l2r.end()) {
				item.updateR2L(0, m, m_lItems[m + 1][0].r2l[m_nSentenceLength].getScore() +
					m_lItems[m_nSentenceLength - m][m].l2r[m_nSentenceLength].getScore() +
					arcScore(m_nSentenceLength, m));
			}
		}
	}

	void DepParser::decodeArcs() {

		m_vecTrainArcs.clear();
		std::stack<std::tuple<int, int, int, int>> stack;

		int s = m_lItems[m_nSentenceLength + 1][0].r2l[0].getSplit();
		m_vecTrainArcs.push_back(Arc(-1, s));

		m_lItems[s + 1][0].type = R2L;
		stack.push(std::tuple<int, int, int, int>(m_nSentenceLength, s + 1, -1, 0));
		m_lItems[m_nSentenceLength - s][s].type = L2R;
		stack.push(std::tuple<int, int, int, int>(m_nSentenceLength, m_nSentenceLength - s, -1, s));

		while (!stack.empty()) {
			std::tuple<int, int, int, int> span = stack.top();
			stack.pop();
			StateItem & item = m_lItems[std::get<1>(span)][std::get<3>(span)];
			int grand = std::get<0>(span);
			int split = std::get<2>(span);

			switch (item.type) {
			case JUX:
				split = item.jux[grand].getSplit();

				m_lItems[split - item.left + 1][item.left].type = L2R;
				stack.push(std::tuple<int, int, int, int>(grand, split - item.left + 1, -1, item.left));
				m_lItems[item.right - split][split + 1].type = R2L;
				stack.push(std::tuple<int, int, int, int>(grand, item.right - split, -1, split + 1));
				break;
			case L2R:
				split = item.l2r[grand].getSplit();

				if (IS_EMPTY(split)) {
					item.type = L2R_EMPTY_OUTSIDE;
					std::get<2>(span) = item.l2r_empty_outside[grand].bestItem().getSplit();
					stack.push(span);
					break;
				}
				if (item.left == item.right) {
					break;
				}

				m_lItems[split - item.left + 1][item.left].type = L2R_SOLID_OUTSIDE;
				stack.push(std::tuple<int, int, int, int>(grand, split - item.left + 1, m_lItems[split - item.left + 1][item.left].l2r_solid_outside[grand].getSplit(), item.left));
				m_lItems[item.right - split + 1][split].type = L2R;
				stack.push(std::tuple<int, int, int, int>(item.left, item.right - split + 1, -1, split));
				break;
			case R2L:
				split = item.r2l[grand].getSplit();

				if (IS_EMPTY(split)) {
					item.type = R2L_EMPTY_OUTSIDE;
					std::get<2>(span) = item.r2l_empty_outside[grand].bestItem().getSplit();
					stack.push(span);
					break;
				}
				if (item.left == item.right) {
					break;
				}

				m_lItems[item.right - split + 1][split].type = R2L_SOLID_OUTSIDE;
				stack.push(std::tuple<int, int, int, int>(grand, item.right - split + 1, m_lItems[item.right - split + 1][split].r2l_solid_outside[grand].getSplit(), split));
				m_lItems[split - item.left + 1][item.left].type = R2L;
				stack.push(std::tuple<int, int, int, int>(item.right, split - item.left + 1, -1, item.left));
				break;
			case L2R_SOLID_BOTH:
				if (item.left == item.right) {
					break;
				}
				split = item.l2r_solid_both[grand].getSplit();
				m_vecTrainArcs.push_back(BiGram<int>(item.left, item.right));

				if (split == item.left) {
					m_lItems[item.right - split][split + 1].type = R2L;
					stack.push(std::tuple<int, int, int, int>(item.left, item.right - split, -1, split + 1));
				}
				else {
					m_lItems[split - item.left + 1][item.left].type = L2R_SOLID_OUTSIDE;
					stack.push(std::tuple<int, int, int, int>(grand, split - item.left + 1, -1, item.left));
					m_lItems[item.right - split + 1][split].type = JUX;
					stack.push(std::tuple<int, int, int, int>(item.left, item.right - split + 1, -1, split));
				}
				break;
			case R2L_SOLID_BOTH:
				if (item.left == item.right) {
					break;
				}
				split = item.r2l_solid_both[grand].getSplit();
				m_vecTrainArcs.push_back(BiGram<int>(item.right == m_nSentenceLength ? -1 : item.right, item.left));

				if (split == item.right) {
					m_lItems[split - item.left][item.left].type = L2R;
					stack.push(std::tuple<int, int, int, int>(item.right, split - item.left, -1, item.left));
				}
				else {
					m_lItems[item.right - split + 1][split].type = R2L_SOLID_OUTSIDE;
					stack.push(std::tuple<int, int, int, int>(grand, item.right - split + 1, -1, split));
					m_lItems[split - item.left + 1][item.left].type = JUX;
					stack.push(std::tuple<int, int, int, int>(item.right, split - item.left + 1, -1, item.left));
				}
				break;
			case L2R_EMPTY_INSIDE:
				if (item.left == item.right) {
					break;
				}
				split = item.l2r_empty_inside[grand].getSplit();
				m_vecTrainArcs.push_back(BiGram<int>(item.left, item.right));

				split = DECODE_EMPTY_POS(split);

				m_lItems[split - item.left + 1][item.left].type = L2R_EMPTY_OUTSIDE;
				stack.push(std::tuple<int, int, int, int>(grand, split - item.left + 1, item.l2r_empty_inside[grand].getInnerSplit(), item.left));
				m_lItems[item.right - split][split + 1].type = R2L;
				stack.push(std::tuple<int, int, int, int>(item.left, item.right - split, -1, split + 1));
				break;
			case R2L_EMPTY_INSIDE:
				if (item.left == item.right) {
					break;
				}
				split = item.r2l_empty_inside[grand].getSplit();
				m_vecTrainArcs.push_back(BiGram<int>(item.right, item.left));

				split = DECODE_EMPTY_POS(split);

				m_lItems[item.right - split][split + 1].type = R2L_EMPTY_OUTSIDE;
				stack.push(std::tuple<int, int, int, int>(grand, item.right - split, item.r2l_empty_inside[grand].getInnerSplit(), split + 1));
				m_lItems[split - item.left + 1][item.left].type = L2R;
				stack.push(std::tuple<int, int, int, int>(item.right, split - item.left + 1, -1, item.left));
				break;
			case L2R_EMPTY_OUTSIDE:
				m_vecTrainArcs.push_back(BiGram<int>(item.left, ENCODE_EMPTY(item.right + 1, DECODE_EMPTY_TAG(split))));

				if (item.left == item.right) {
					break;
				}

				split = DECODE_EMPTY_POS(split);

				m_lItems[split - item.left + 1][item.left].type = L2R_SOLID_OUTSIDE;
				stack.push(std::tuple<int, int, int, int>(grand, split - item.left + 1, -1, item.left));
				m_lItems[item.right - split + 1][split].type = L2R;
				stack.push(std::tuple<int, int, int, int>(item.left, item.right - split + 1, -1, split));
				break;
			case R2L_EMPTY_OUTSIDE:
				m_vecTrainArcs.push_back(BiGram<int>(item.right, ENCODE_EMPTY(item.left, DECODE_EMPTY_TAG(split))));

				if (item.left == item.right) {
					break;
				}

				split = DECODE_EMPTY_POS(split);

				m_lItems[item.right - split + 1][split].type = R2L_SOLID_OUTSIDE;
				stack.push(std::tuple<int, int, int, int>(grand, item.right - split + 1, -1, split));
				m_lItems[split - item.left + 1][item.left].type = R2L;
				stack.push(std::tuple<int, int, int, int>(item.right, split - item.left + 1, -1, item.left));
				break;
			case L2R_SOLID_OUTSIDE:
				if (item.left == item.right) {
					break;
				}

				split = item.l2r_solid_outside[grand].getSplit();
				item.type = IS_EMPTY(split) ? L2R_EMPTY_INSIDE : L2R_SOLID_BOTH;
				stack.push(span);
				break;
			case R2L_SOLID_OUTSIDE:
				if (item.left == item.right) {
					break;
				}

				split = item.r2l_solid_outside[grand].getSplit();
				item.type = IS_EMPTY(split) ? R2L_EMPTY_INSIDE : R2L_SOLID_BOTH;
				stack.push(span);
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
		else {
			for (const auto & arc : m_vecTrainArcs) {
				if (IS_EMPTY(arc.second())) {
					std::cout << "generate an empty edge at " << m_nTrainingRound << std::endl;
					break;
				}
			}
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
		std::vector<Arc> emptyArcs;
		for (const auto & arc : m_vecTrainArcs) {
			if (IS_EMPTY(arc.second())) {
				emptyArcs.push_back(arc);
			}
		}
		std::sort(emptyArcs.begin(), emptyArcs.end(), [](const Arc & arc1, const Arc & arc2){ return compareArc(arc1, arc2); });

		auto itr_e = emptyArcs.begin();
		int idx = 0, nidx = 0;
		std::unordered_map<int, int> nidmap;
		nidmap[-1] = -1;
		while (itr_e != emptyArcs.end() && idx != m_nSentenceLength) {
			if (idx < DECODE_EMPTY_POS(itr_e->second())) {
				nidmap[idx] = nidx++;
				retval->push_back(DependencyTreeNode(TREENODE_POSTAGGEDWORD(correct[idx++]), -1, NULL_LABEL));
			}
			else {
				nidmap[itr_e->second() * 256 + itr_e->first()] = nidx++;
				retval->push_back(DependencyTreeNode(POSTaggedWord(TEmptyTag::key(DECODE_EMPTY_TAG((itr_e++)->second())), "EMCAT"), -1, NULL_LABEL));
			}
		}
		while (idx != m_nSentenceLength) {
			nidmap[idx] = nidx++;
			retval->push_back(DependencyTreeNode(TREENODE_POSTAGGEDWORD(correct[idx++]), -1, NULL_LABEL));
		}
		while (itr_e != emptyArcs.end()) {
			nidmap[itr_e->second() * 256 + itr_e->first()] = nidx++;
			retval->push_back(DependencyTreeNode(POSTaggedWord(TEmptyTag::key(DECODE_EMPTY_TAG((itr_e++)->second())), "EMCAT"), -1, NULL_LABEL));
		}

		for (const auto & arc : m_vecTrainArcs) {
			TREENODE_HEAD(retval->at(nidmap[IS_EMPTY(arc.second()) ? arc.second() * 256 + arc.first() : arc.second()])) = nidmap[arc.first()];
		}
	}

	void DepParser::goldCheck() {
		Arcs2TriArcs(m_vecTrainArcs, m_vecTrainTriArcs);
		std::cout << "total score is " <<  m_lItems[m_nSentenceLength + 1][0].r2l[0].getScore() << std::endl; //debug
		if (m_vecCorrectArcs.size() != m_vecTrainArcs.size() || m_lItems[m_nSentenceLength + 1][0].r2l[0].getScore() / GOLD_POS_SCORE != 2 * m_vecCorrectArcs.size() - 1 + 2 * m_vecCorrectTriArcs.size() - 2) {
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
				for (const auto & triarc : m_vecTrainTriArcs) {
					std::cout << triarc << std::endl;
				}
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

//	const tscore & DepParser::triSiblingArcScore(const int & p, const int & c, const int & c2, const int & c3) {
//		if (m_nState == ParserState::GOLDTEST) {
//			m_nRetval = m_setTriSiblingArcGoldScore.find(QuarGram<int>(p, c, c2, c3)) == m_setTriSiblingArcGoldScore.end() ? GOLD_NEG_SCORE : GOLD_POS_SCORE;
//			return m_nRetval;
//		}
//		m_nRetval = 0;
//		getOrUpdateSiblingScore(p, c, c2, c3, 0);
//		return m_nRetval;
//	}

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

//	const tscore & DepParser::grandTriSiblingArcScore(const int & g, const int & p, const int & c, const int & c2, const int & c3) {
//		if (m_nState == ParserState::GOLDTEST) {
//			m_nRetval = m_setGrandTriSiblingArcGoldScore.find(QuinGram<int>(g, p, c, c2, c3)) == m_setGrandTriSiblingArcGoldScore.end() ? GOLD_NEG_SCORE : GOLD_POS_SCORE;
//			return m_nRetval;
//		}
//		m_nRetval = 0;
//		getOrUpdateGrandScore(g, p, c, c2, c3, 0);
//		return m_nRetval;
//	}

	void DepParser::initArcScore() {
		m_vecArcScore =
			std::vector<std::vector<tscore>>(
			m_nSentenceLength + 1,
			std::vector<tscore>(m_nSentenceLength));
		m_vecFirstOrderEmptyScore =
				decltype(m_vecFirstOrderEmptyScore)(
						m_nSentenceLength,
						std::vector<AgendaBeam<ScoreWithType, MAX_EMPTY_SIZE>>(m_nSentenceLength));
		for (int d = 1; d <= m_nSentenceLength; ++d) {
			for (int i = 0, max_i = m_nSentenceLength - d + 1; i < max_i; ++i) {
				if (d > 1) {
					m_vecArcScore[i][i + d - 1] = arcScore(i, i + d - 1);
					m_vecArcScore[i + d - 1][i] = arcScore(i + d - 1, i);
				}

				for (int t = TEmptyTag::start(), max_t = TEmptyTag::end(); t < max_t; ++t) {
					if (m_nState == ParserState::GOLDTEST || testEmptyNode(i, ENCODE_EMPTY(i + d, t))) {
						tscore score = arcScore(i, ENCODE_EMPTY(i + d, t));
						if (score != 0) {
							m_vecFirstOrderEmptyScore[i][i + d - 1].insertItem(ScoreWithType(t, score));
						}
					}
					if (m_nState == ParserState::GOLDTEST || testEmptyNode(i + d - 1, ENCODE_EMPTY(i, t))) {
						tscore score = arcScore(i + d - 1, ENCODE_EMPTY(i, t));
						if (score != 0) {
							m_vecFirstOrderEmptyScore[i + d - 1][i].insertItem(ScoreWithType(t, score));
						}
					}
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
		for (int d = 2; d <= m_nSentenceLength; ++d) {
			for (int i = 0, max_i = m_nSentenceLength - d + 1; i < max_i; ++i) {
				int l = i + d - 1;
				m_vecBiSiblingScore[i][l][i] = biSiblingArcScore(i, -1, l);
				m_vecBiSiblingScore[l][i][l] = biSiblingArcScore(l, -1, i);
				for (int k = i + 1, l = i + d - 1; k < l; ++k) {
					m_vecBiSiblingScore[i][l][k] = biSiblingArcScore(i, k, l);
					m_vecBiSiblingScore[l][i][k] = biSiblingArcScore(l, k, i);
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

	void DepParser::readEmptySentAndArcs(const DependencyTree & correct) {
		DependencyTree tcorrect(correct.begin(), correct.end());
		// for normal sentence
		for (const auto & node : tcorrect) {
			if (TREENODE_POSTAG(node) == EMPTYTAG) {
				for (auto & tnode : tcorrect) {
					if (TREENODE_HEAD(tnode) > m_nSentenceLength) {
						--TREENODE_HEAD(tnode);
					}
				}
			}
			else {
				m_lSentence[m_nSentenceLength][0].refer(TWord::code(TREENODE_WORD(node)), TPOSTag::code(TREENODE_POSTAG(node)));
				for (int i = TEmptyTag::start(), max_i = TEmptyTag::end(); i < max_i; ++i) {
					m_lSentence[m_nSentenceLength][i].refer(
						TWord::code(
						(m_nSentenceLength == 0 ? START_WORD : TWord::key(m_lSentence[m_nSentenceLength - 1][0].first())) +
						TEmptyTag::key(i) + TREENODE_WORD(node)
						),
						TPOSTag::code(TEmptyTag::key(i))
						);
				}
				++m_nSentenceLength;
			}
		}
		m_lSentence[m_nSentenceLength][0].refer(TWord::code(ROOT_WORD), TPOSTag::code(ROOT_POSTAG));
		for (int i = TEmptyTag::start(), max_i = TEmptyTag::end(); i < max_i; ++i) {
			m_lSentence[m_nSentenceLength][i].refer(
				TWord::code(
				TWord::key(m_lSentence[m_nSentenceLength - 1][0].first()) + TEmptyTag::key(i) + END_WORD
				),
				TPOSTag::code(TEmptyTag::key(i))
				);
		}
		int idx = 0, idxwe = 0;
		ttoken empty_tag = "";
		for (const auto & node : tcorrect) {
			if (TREENODE_POSTAG(node) == EMPTYTAG) {
				m_lSentenceWithEmpty[idxwe++].refer(
					TWord::code((idx == 0 ? START_WORD : TWord::key(m_lSentence[idx - 1][0].first())) +
					TREENODE_WORD(node) +
					(idx == m_nSentenceLength ? END_WORD : TWord::key(m_lSentence[idx][0].first()))),
					TPOSTag::code(TREENODE_WORD(node))
					);
				m_vecCorrectArcs.push_back(Arc(TREENODE_HEAD(node), ENCODE_EMPTY(idx, TEmptyTag::code(TREENODE_WORD(node)))));
			}
			else {
				m_lSentenceWithEmpty[idxwe++].refer(TWord::code(TREENODE_WORD(node)), TPOSTag::code(TREENODE_POSTAG(node)));
				m_vecCorrectArcs.push_back(Arc(TREENODE_HEAD(node), idx++));
			}
		}
		m_lSentenceWithEmpty[idxwe++].refer(TWord::code(ROOT_WORD), TPOSTag::code(ROOT_POSTAG));

	}

	bool DepParser::testEmptyNode(const int & p, const int & c) {

		Weight * cweight = (Weight*)m_pWeight;

		int pos_c = DECODE_EMPTY_POS(c);
		int tag_c = DECODE_EMPTY_TAG(c);

		p_tag = m_lSentence[p][0].second();

		c_tag = m_lSentence[pos_c][tag_c].second();

		int pc = pos_c > p ? pos_c : pos_c - 1;
		m_nDis = encodeLinkDistanceOrDirection(p, pc, false);
		m_nDir = encodeLinkDistanceOrDirection(p, pc, true);

		tag_tag_int.refer(p_tag, c_tag, m_nDis);
		if (!cweight->m_mapPpCp.hasKey(tag_tag_int)) {
			return false;
		}
		tag_tag_int.referLast(m_nDir);
		if (!cweight->m_mapPpCp.hasKey(tag_tag_int)) {
			return false;
		}

		return true;
	}

	void DepParser::getOrUpdateSiblingScore(const int & p, const int & c, const int & amount) {

		Weight * cweight = (Weight*)m_pWeight;

		int pos_c = DECODE_EMPTY_POS(c);
		int tag_c = DECODE_EMPTY_TAG(c);

		p_1_tag = p > 0 ? m_lSentence[p - 1][0].second() : start_taggedword.second();
		p1_tag = p < m_nSentenceLength - 1 ? m_lSentence[p + 1][0].second() : end_taggedword.second();
		c_1_tag = pos_c > 0 ? m_lSentence[pos_c][0].second() : start_taggedword.second();
		if (tag_c == 0) {
			c1_tag = pos_c < m_nSentenceLength - 1 ? m_lSentence[pos_c + 1][0].second() : end_taggedword.second();
		}
		else {
			c1_tag = pos_c < m_nSentenceLength ? m_lSentence[pos_c][0].second() : end_taggedword.second();
		}

		p_word = m_lSentence[p][0].first();
		p_tag = m_lSentence[p][0].second();

		c_word = m_lSentence[pos_c][tag_c].first();
		c_tag = m_lSentence[pos_c][tag_c].second();

		int pc = IS_EMPTY(c) ? (pos_c > p ? pos_c : pos_c - 1) : c;
		m_nDis = encodeLinkDistanceOrDirection(p, pc, false);
		m_nDir = encodeLinkDistanceOrDirection(p, pc, true);

		if (tag_c == 0) {

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

		}

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

		// change here
		for (int b = pos_c < p ? pos_c + 2 : p + 1, e = (int)std::fmax(p, pos_c); b < e; ++b) {
			b_tag = m_lSentence[b][0].second();
			tag_tag_tag_int.refer(p_tag, b_tag, c_tag, 0);
			cweight->m_mapPpBpCp.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
			tag_tag_tag_int.refer(p_tag, b_tag, c_tag, m_nDis);
			cweight->m_mapPpBpCp.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
			tag_tag_tag_int.refer(p_tag, b_tag, c_tag, m_nDir);
			cweight->m_mapPpBpCp.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		}
	}

	void DepParser::getOrUpdateSiblingScore(const int & p, const int & c, const int & c2, const int & amount) {
		Weight * cweight = (Weight*)m_pWeight;

		int pos_c = DECODE_EMPTY_POS(c);
		int tag_c = DECODE_EMPTY_TAG(c);

		int pos_c2 = DECODE_EMPTY_POS(c2);
		int tag_c2 = DECODE_EMPTY_TAG(c2);

		p_tag = m_lSentence[p][0].second();

		c_word = IS_NULL(c) ? empty_taggedword.first() : m_lSentence[pos_c][tag_c].first();
		c_tag = IS_NULL(c) ? empty_taggedword.second() : m_lSentence[pos_c][tag_c].second();

		c2_word = m_lSentence[pos_c2][tag_c2].first();
		c2_tag = m_lSentence[pos_c2][tag_c2].second();

		int pc = IS_EMPTY(c) ? (pos_c > p ? pos_c : pos_c - 1) : c;
		m_nDir = encodeLinkDistanceOrDirection(p, pc, true);

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

//	void DepParser::getOrUpdateSiblingScore(const int & p, const int & c, const int & c2, const int & c3, const int & amount) {
//		Weight * cweight = (Weight*)m_Weight;
//
//		int pos_c = DECODE_EMPTY_POS(c);
//		int tag_c = DECODE_EMPTY_TAG(c);
//
//		int pos_c2 = DECODE_EMPTY_POS(c2);
//		int tag_c2 = DECODE_EMPTY_TAG(c2);
//
//		int pos_c3 = DECODE_EMPTY_POS(c3);
//		int tag_c3 = DECODE_EMPTY_TAG(c3);
//
//		p_word = m_lSentence[p][0].first();
//		p_tag = m_lSentence[p][0].second();
//
//		c_word = IS_NULL(c) ? empty_taggedword.first() : m_lSentence[pos_c][tag_c].first();
//		c_tag = IS_NULL(c) ? empty_taggedword.second() : m_lSentence[pos_c][tag_c].second();
//
//		c2_word = IS_NULL(c2) ? empty_taggedword.first() : m_lSentence[pos_c2][tag_c2].first();
//		c2_tag = IS_NULL(c2) ? empty_taggedword.second() : m_lSentence[pos_c2][tag_c2].second();
//
//		c3_word = m_lSentence[pos_c3][tag_c3].first();
//		c3_tag = m_lSentence[pos_c3][tag_c3].second();
//
//		int pc = IS_EMPTY(c) ? (pos_c > p ? pos_c : pos_c - 1) : c;
//		m_nDir = encodeLinkDistanceOrDirection(p, pc, true);
//
//		// word tag tag tag
//		word_tag_tag_tag_int.refer(p_word, c_tag, c2_tag, c3_tag, 0);
//		cweight->m_mapPwC1pC2pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_tag_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapPwC1pC2pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		word_tag_tag_tag_int.refer(c_word, p_tag, c2_tag, c3_tag, 0);
//		cweight->m_mapC1wPpC2pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_tag_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapC1wPpC2pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		word_tag_tag_tag_int.refer(c2_word, p_tag, c_tag, c3_tag, 0);
//		cweight->m_mapC2wPpC1pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_tag_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapC2wPpC1pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		word_tag_tag_tag_int.refer(c3_word, p_tag, c_tag, c2_tag, 0);
//		cweight->m_mapC3wPpC1pC2p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_tag_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapC3wPpC1pC2p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		// word word tag tag
//		word_word_tag_tag_int.refer(p_word, c_word, c2_tag, c3_tag, 0);
//		cweight->m_mapPwC1wC2pC3p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_word_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapPwC1wC2pC3p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		word_word_tag_tag_int.refer(p_word, c2_word, c_tag, c3_tag, 0);
//		cweight->m_mapPwC2wC1pC3p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_word_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapPwC2wC1pC3p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		word_word_tag_tag_int.refer(p_word, c3_word, c_tag, c2_tag, 0);
//		cweight->m_mapPwC3wC1pC2p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_word_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapPwC3wC1pC2p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		word_word_tag_tag_int.refer(c_word, c2_word, p_tag, c3_tag, 0);
//		cweight->m_mapC1wC2wPpC3p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_word_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapC1wC2wPpC3p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		word_word_tag_tag_int.refer(c_word, c3_word, p_tag, c2_tag, 0);
//		cweight->m_mapC1wC3wPpC2p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_word_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapC1wC3wPpC2p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		word_word_tag_tag_int.refer(c2_word, c3_word, p_tag, c_tag, 0);
//		cweight->m_mapC2wC3wPpC1p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_word_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapC2wC3wPpC1p.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		// tag tag tag tag
//		tag_tag_tag_tag_int.refer(p_tag, c_tag, c2_tag, c3_tag, 0);
//		cweight->m_mapPpC1pC2pC3p.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		tag_tag_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapPpC1pC2pC3p.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		// word tag tag
//		word_tag_tag_int.refer(c_word, c2_tag, c3_tag, 0);
//		cweight->m_mapC1wC2pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapC1wC2pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		word_tag_tag_int.refer(c2_word, c_tag, c3_tag, 0);
//		cweight->m_mapC2wC1pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapC2wC1pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		word_tag_tag_int.refer(c3_word, c_tag, c2_tag, 0);
//		cweight->m_mapC3wC1pC2p.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapC3wC1pC2p.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		// word word tag
//		word_word_tag_int.refer(c_word, c2_word, c3_tag, 0);
//		cweight->m_mapC1wC2wC3p.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_word_tag_int.referLast(m_nDir);
//		cweight->m_mapC1wC2wC3p.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		word_word_tag_int.refer(c_word, c3_word, c2_tag, 0);
//		cweight->m_mapC1wC3wC2p.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_word_tag_int.referLast(m_nDir);
//		cweight->m_mapC1wC3wC2p.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		word_word_tag_int.refer(c2_word, c3_word, c_tag, 0);
//		cweight->m_mapC2wC3wC1p.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_word_tag_int.referLast(m_nDir);
//		cweight->m_mapC2wC3wC1p.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		// tag tag tag
//		tag_tag_tag_int.refer(c_tag, c2_tag, c3_tag, 0);
//		cweight->m_mapC1pC2pC3p.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		tag_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapC1pC2pC3p.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		// other
//		word_word_int.refer(c_word, c3_word, 0);
//		cweight->m_mapC1wC3w.getOrUpdateScore(m_nRetval, word_word_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_word_int.referLast(m_nDir);
//		cweight->m_mapC1wC3w.getOrUpdateScore(m_nRetval, word_word_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		word_tag_int.refer(c_word, c3_tag, 0);
//		cweight->m_mapC1wC3p.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_tag_int.referLast(m_nDir);
//		cweight->m_mapC1wC3p.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		word_tag_int.refer(c3_word, c_tag, 0);
//		cweight->m_mapC3wC1p.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_tag_int.referLast(m_nDir);
//		cweight->m_mapC3wC1p.getOrUpdateScore(m_nRetval, word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		tag_tag_int.refer(c_tag, c3_tag, 0);
//		cweight->m_mapC1pC3p.getOrUpdateScore(m_nRetval, tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		tag_tag_int.referLast(m_nDir);
//		cweight->m_mapC1pC3p.getOrUpdateScore(m_nRetval, tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//	}

	void DepParser::getOrUpdateGrandScore(const int & g, const int & p, const int & c, const int & amount) {
		Weight * cweight = (Weight*)m_pWeight;

		int pos_c = DECODE_EMPTY_POS(c);
		int tag_c = DECODE_EMPTY_TAG(c);

		g_word = m_lSentence[g][0].first();
		g_tag = m_lSentence[g][0].second();

		p_tag = m_lSentence[p][0].second();

		c_word = m_lSentence[pos_c][tag_c].first();
		c_tag = m_lSentence[pos_c][tag_c].second();

		int pc = IS_EMPTY(c) ? (pos_c > p ? pos_c : pos_c - 1) : c;
		m_nDir = encodeLinkDistanceOrDirection(p, pc, true);

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
		Weight * cweight = (Weight*)m_pWeight;

		int pos_c = DECODE_EMPTY_POS(c);
		int tag_c = DECODE_EMPTY_TAG(c);

		int pos_c2 = DECODE_EMPTY_POS(c2);
		int tag_c2 = DECODE_EMPTY_TAG(c2);

		g_word = m_lSentence[g][0].first();
		g_tag = m_lSentence[g][0].second();

		p_word = m_lSentence[p][0].first();
		p_tag = m_lSentence[p][0].second();

		c_word = IS_NULL(c) ? empty_taggedword.first() : m_lSentence[pos_c][tag_c].first();
		c_tag = IS_NULL(c) ? empty_taggedword.second() : m_lSentence[pos_c][tag_c].second();

		c2_word = m_lSentence[pos_c2][tag_c2].first();
		c2_tag = m_lSentence[pos_c2][tag_c2].second();

		int pc = IS_EMPTY(c) ? (pos_c > p ? pos_c : pos_c - 1) : c;
		m_nDir = encodeLinkDistanceOrDirection(p, pc, true);

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

//	void DepParser::getOrUpdateGrandScore(const int & g, const int & p, const int & c, const int & c2, const int & c3, const int & amount) {
//		Weight * cweight = (Weight*)m_Weight;
//
//		int pos_c = DECODE_EMPTY_POS(c);
//		int tag_c = DECODE_EMPTY_TAG(c);
//
//		int pos_c2 = DECODE_EMPTY_POS(c2);
//		int tag_c2 = DECODE_EMPTY_TAG(c2);
//
//		int pos_c3 = DECODE_EMPTY_POS(c3);
//		int tag_c3 = DECODE_EMPTY_TAG(c3);
//
//		g_word = m_lSentence[g][0].first();
//		g_tag = m_lSentence[g][0].second();
//
//		p_word = m_lSentence[p][0].first();
//		p_tag = m_lSentence[p][0].second();
//
//		c_word = IS_NULL(c) ? empty_taggedword.first() : m_lSentence[pos_c][tag_c].first();
//		c_tag = IS_NULL(c) ? empty_taggedword.second() : m_lSentence[pos_c][tag_c].second();
//
//		c2_word = IS_NULL(c2) ? empty_taggedword.first() : m_lSentence[pos_c2][tag_c2].first();
//		c2_tag = IS_NULL(c2) ? empty_taggedword.second() : m_lSentence[pos_c2][tag_c2].second();
//
//		c3_word = m_lSentence[pos_c3][tag_c3].first();
//		c3_tag = m_lSentence[pos_c3][tag_c3].second();
//
//		int pc = IS_EMPTY(c) ? (pos_c > p ? pos_c : pos_c - 1) : c;
//		m_nDir = encodeLinkDistanceOrDirection(p, pc, true);
//
//		word_tag_tag_tag_tag_int.refer(g_word, p_tag, c_tag, c2_tag, c3_tag, 0);
//		cweight->m_mapGwPpCpC2pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_tag_tag_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapGwPpCpC2pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		word_tag_tag_tag_tag_int.refer(p_word, g_tag, c_tag, c2_tag, c3_tag, 0);
//		cweight->m_mapPwGpCpC2pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_tag_tag_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapPwGpCpC2pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		word_tag_tag_tag_tag_int.refer(c_word, g_tag, p_tag, c2_tag, c3_tag, 0);
//		cweight->m_mapCwGpPpC2pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_tag_tag_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapCwGpPpC2pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		word_tag_tag_tag_tag_int.refer(c2_word, g_tag, p_tag, c_tag, c3_tag, 0);
//		cweight->m_mapC2wGpPpCpC2p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_tag_tag_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapC2wGpPpCpC2p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		word_tag_tag_tag_tag_int.refer(c3_word, g_tag, p_tag, c_tag, c2_tag, 0);
//		cweight->m_mapC3wGpPpCpC2p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_tag_tag_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapC3wGpPpCpC2p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		tag_tag_tag_tag_tag_int.refer(g_tag, p_tag, c_tag, c2_tag, c3_tag, 0);
//		cweight->m_mapGpPpCpC2pC3p.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		tag_tag_tag_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapGpPpCpC2pC3p.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		// word tag tag tag
//		word_tag_tag_tag_int.refer(g_word, c_tag, c2_tag, c3_tag, 0);
//		cweight->m_mapGwCpC2pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_tag_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapGwCpC2pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		word_tag_tag_tag_int.refer(c_word, g_tag, c2_tag, c3_tag, 0);
//		cweight->m_mapCwGpC2pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_tag_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapCwGpC2pC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		word_tag_tag_tag_int.refer(c2_word, g_tag, c_tag, c3_tag, 0);
//		cweight->m_mapC2wGpCpC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_tag_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapC2wGpCpC3p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		word_tag_tag_tag_int.refer(c3_word, g_tag, c_tag, c2_tag, 0);
//		cweight->m_mapC3wGpCpC2p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		word_tag_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapC3wGpCpC2p.getOrUpdateScore(m_nRetval, word_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//
//		// tag tag tag tag
//		tag_tag_tag_tag_int.refer(g_tag, c_tag, c2_tag, c3_tag, 0);
//		cweight->m_mapGpCpC2pC3p.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//		tag_tag_tag_tag_int.referLast(m_nDir);
//		cweight->m_mapGpCpC2pC3p.getOrUpdateScore(m_nRetval, tag_tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
//	}

}
