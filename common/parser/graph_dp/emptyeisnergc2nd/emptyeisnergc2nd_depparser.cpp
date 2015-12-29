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

		m_Weight = new Weight(sFeatureInput, sFeatureOut);

		for (int i = 0; i < MAX_SENTENCE_SIZE; ++i) {
			m_lItems[1].push_back(StateItem());
			m_lItems[1][i].init(i, i, m_nSentenceLength);
		}

		DepParser::empty_taggedword.refer(TWord::code(EMPTY_WORD), TPOSTag::code(EMPTY_POSTAG));
		DepParser::start_taggedword.refer(TWord::code(START_WORD), TPOSTag::code(START_POSTAG));
		DepParser::end_taggedword.refer(TWord::code(END_WORD), TPOSTag::code(END_POSTAG));

		m_tStartTime = 0;
		m_tInitSpaceTime = 0;
		m_tGetScoreTime = 0;
		//m_tUpdateScoreTime = 0;
		//m_tStartTime = 0;
	}

	DepParser::~DepParser() {
		delete m_Weight;
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

		for (int d = 1; d <= m_nSentenceLength; ++d) {

			initArcScore(d);
			if (d > 1) {
				initBiSiblingArcScore(d);
			}

			m_lItems[d].reserve(m_nSentenceLength - d + 1);

			for (int i = 0, max_i = m_nSentenceLength - d + 1; i < max_i; ++i) {

				//auto s_time = GetTickCount();
				m_lItems[d].push_back(StateItem());
				//m_tInitSpaceTime += GetTickCount() - s_time;

				int l = i + d - 1;
				StateItem & item = m_lItems[d][i];
				const tscore & l2r_arc_score = m_lArcScore[ENCODE_L2R(i)];
				const tscore & r2l_arc_score = m_lArcScore[ENCODE_R2L(i)];

				// initialize
				//auto start_time = GetTickCount(); //debug
				item.init(i, l, m_nSentenceLength);
				//m_tInitSpaceTime += GetTickCount() - start_time; //debug

				initGrandSiblingArcScore(d, i);

				// jux
				for (int s = i; s < l; ++s) {
					int g = 0;
					const StateItem & litem = m_lItems[s - i + 1][i];
					const StateItem & ritem = m_lItems[l - s][s + 1];
					do {
						if (g == i) {
							g = l + 1;
						}
						int grand = GRAND_INDEX(g, i, d);
						int lgrand = GRAND_INDEX(g, i, s - i + 1);
						int rgrand = GRAND_INDEX(g, s + 1, l - s);

						item.updateJUX(grand, s, litem.l2r[lgrand].getScore() + ritem.r2l[rgrand].getScore());
						// l2r_empty_inside
						tscore l_empty_inside_base_score = l2r_arc_score + ritem.r2l[GRAND_INDEX(i, s + 1, l - s)].getScore();
						for (const auto & l_beam : litem.l2r_empty_outside[lgrand]) {
							int p = ENCODE_EMPTY(s, DECODE_EMPTY_TAG(l_beam->getSplit()));
							int k = ENCODE_EMPTY(s + 1, DECODE_EMPTY_TAG(l_beam->getSplit()));
							int j = DECODE_EMPTY_POS(l_beam->getSplit());
							item.updateL2REmptyInside(grand, p, l_beam->getSplit(), l_empty_inside_base_score +
								l_beam->getScore() +
								biSiblingArcScore(i, k, l) + // empty node bisibling
								biSiblingArcScore(i, j == i ? -1 : j, l) + // solid node bisibling
								m_lGrandSiblingArcScore[ENCODE_L2R(g)] +
								grandBiSiblingArcScore(g, i, k, l) + // empty node grand bisibling
								grandBiSiblingArcScore(g, i, j == i ? -1 : j, l)); // solid node grand bisibling
						}

						// r2l_empty_inside
						tscore r_empty_inside_base_score = r2l_arc_score + litem.l2r[GRAND_INDEX(l, i, s - i + 1)].getScore();
						for (const auto & r_beam : ritem.r2l_empty_outside[rgrand]) {
							int p = ENCODE_EMPTY(s, DECODE_EMPTY_TAG(r_beam->getSplit()));
							int k = ENCODE_EMPTY(s + 1, DECODE_EMPTY_TAG(r_beam->getSplit()));
							int j = DECODE_EMPTY_POS(r_beam->getSplit());
							item.updateR2LEmptyInside(grand, p, r_beam->getSplit(), r_empty_inside_base_score +
								r_beam->getScore() +
								biSiblingArcScore(l, k, i) +
								biSiblingArcScore(l, j == l ? -1 : j, i) +
								m_lGrandSiblingArcScore[ENCODE_R2L(g)] +
								grandBiSiblingArcScore(g, l, k, i) +
								grandBiSiblingArcScore(g, l, j == l ? -1 : j, i));
//							if (i == 24 && l == 25 && grand == 23) {
//								std::cout << "l = " << l << " j = " << j << " i = " << i << std::endl;
//								std::cout << r_empty_inside_base_score << std::endl;
//								std::cout << r_beam->getScore() << std::endl;
//								std::cout << biSiblingArcScore(l, k, i) << std::endl;
//								std::cout << biSiblingArcScore(l, j == l ? -1 : j, i) << std::endl;
//								std::cout << m_lGrandSiblingArcScore[ENCODE_R2L(g)] << std::endl;
//								std::cout << grandBiSiblingArcScore(g, l, k, i) << std::endl;
//								std::cout << grandBiSiblingArcScore(g, l, j == l ? -1 : j, i) << std::endl;
//							}
						}
					} while (++g <= m_nSentenceLength);
				}

				for (int k = i + 1; k < l; ++k) {

					StateItem & litem = m_lItems[k - i + 1][i];
					StateItem & ritem = m_lItems[l - k + 1][k];

					int lgrand = GRAND_INDEX(l, i, k - i + 1);
					int rgrand = GRAND_INDEX(i, k, l - k + 1);

					tscore l_base_jux = ritem.jux[rgrand].getScore() + l2r_arc_score + m_lBiSiblingArcScore[ENCODE_2ND_L2R(i, k)];
					tscore r_base_jux = litem.jux[lgrand].getScore() + r2l_arc_score + m_lBiSiblingArcScore[ENCODE_2ND_R2L(i, k)];


					initGrandBiSiblingArcScore(d, i, k);

					int g = 0;
					do {
						if (g == i) {
							g = l + 1;
						}
						int grand = GRAND_INDEX(g, i, d);

						const auto & l_beam = litem.l2r_solid_outside[GRAND_INDEX(g, i, k - i + 1)];
						const auto & r_beam = ritem.r2l_solid_outside[GRAND_INDEX(g, k, l - k + 1)];

						// l2r solid both
						item.updateL2RSolidBoth(grand, k, l_base_jux + m_lGrandSiblingArcScore[ENCODE_L2R(g)] + m_lGrandBiSiblingArcScore[ENCODE_L2R(g)] + l_beam.getScore());

						// r2l solid both
						item.updateR2LSolidBoth(grand, k, r_base_jux + m_lGrandSiblingArcScore[ENCODE_R2L(g)] + m_lGrandBiSiblingArcScore[ENCODE_R2L(g)] + r_beam.getScore());

						// l2r_empty_outside
						for (const auto & swt : m_abFirstOrderEmptyScore[ENCODE_L2R(i)]) {
							const int & t = swt->getType();
							// s is split with type
							int s = ENCODE_EMPTY(k, t);
							// o is outside empty
							int o = ENCODE_EMPTY(l + 1, t);
							tscore l_empty_outside_base_score = ritem.l2r[rgrand].getScore() + swt->getScore() + biSiblingArcScore(i, k, o);
							item.updateL2REmptyOutside(grand, s, l_empty_outside_base_score +
								l_beam.getScore() +
								grandSiblingArcScore(g, i, o) +
								grandBiSiblingArcScore(g, i, k, o));
						}

						// r2l_empty_outside
						for (const auto & swt : m_abFirstOrderEmptyScore[ENCODE_R2L(i)]) {
							const int & t = swt->getType();
							int s = ENCODE_EMPTY(k, t);
							int o = ENCODE_EMPTY(i, t);
							tscore r_empty_outside_base_score = litem.r2l[lgrand].getScore() + swt->getScore() + biSiblingArcScore(l, k, o);
							item.updateR2LEmptyOutside(grand, s, r_empty_outside_base_score +
								r_beam.getScore() +
								grandSiblingArcScore(g, l, o) +
								grandBiSiblingArcScore(g, l, k, o));
						}

						// complete
						item.updateL2R(grand, k, litem.l2r_solid_outside[GRAND_INDEX(g, i, k - i + 1)].getScore() + ritem.l2r[rgrand].getScore());
						item.updateR2L(grand, k, ritem.r2l_solid_outside[GRAND_INDEX(g, k, l - k + 1)].getScore() + litem.r2l[lgrand].getScore());

					} while (++g <= m_nSentenceLength);
				}

				if (d > 1) {
					int g = 0;
					do {
						if (g == i) {
							g = l + 1;
						}
						int grand = GRAND_INDEX(g, i, d);

						// l2r_solid_both
						item.updateL2RSolidBoth(grand, i, m_lItems[d - 1][i + 1].r2l[GRAND_INDEX(i, i + 1, d - 1)].getScore() +
							l2r_arc_score +
							m_lBiSiblingArcScore[ENCODE_2ND_L2R(i, i)] +
							m_lGrandSiblingArcScore[ENCODE_L2R(g)] +
							grandBiSiblingArcScore(g, i, -1, l));
						// r2l_solid_both
						item.updateR2LSolidBoth(grand, l, m_lItems[d - 1][i].l2r[GRAND_INDEX(l, i, d - 1)].getScore() +
							r2l_arc_score +
							m_lBiSiblingArcScore[ENCODE_2ND_R2L(i, i)] +
							m_lGrandSiblingArcScore[ENCODE_R2L(g)] +
							grandBiSiblingArcScore(g, l, -1, i));

						item.updateL2R(grand, l, item.l2r_solid_both[grand].getScore());
						item.updateR2L(grand, i, item.r2l_solid_both[grand].getScore());
						// l2r_solid_outside
						item.updateL2RSolidOutside(grand, item.l2r_empty_inside[grand].getSplit(), item.l2r_empty_inside[grand].getScore());
						item.updateL2RSolidOutside(grand, item.l2r_solid_both[grand].getSplit(), item.l2r_solid_both[grand].getScore());
						item.updateR2LSolidOutside(grand, item.r2l_empty_inside[grand].getSplit(), item.r2l_empty_inside[grand].getScore());
						item.updateR2LSolidOutside(grand, item.r2l_solid_both[grand].getSplit(), item.r2l_solid_both[grand].getScore());
					} while (++g <= m_nSentenceLength);
				}
				int g = 0;
				do {
					if (g == i) {
						g = l + 1;
					}
					int grand = GRAND_INDEX(g, i, d);
					// l2r_empty_ouside
					for (const auto & swt : m_abFirstOrderEmptyScore[ENCODE_L2R(i)]) {
						const int & t = swt->getType();
						int s = ENCODE_EMPTY(l, t);
						int o = ENCODE_EMPTY(l + 1, t);
						if (d > 1) {
							tscore base_l2r_empty_outside_score = swt->getScore() + biSiblingArcScore(i, l, o) + m_lItems[1][l].l2r[GRAND_INDEX(i, l, 1)].getScore();

							item.updateL2REmptyOutside(grand, s, base_l2r_empty_outside_score +
								item.l2r_solid_outside[grand].getScore() +
								grandSiblingArcScore(g, i, o) +
								grandBiSiblingArcScore(g, i, l, o));

							item.updateL2REmptyOutside(grand, s, base_l2r_empty_outside_score +
								item.l2r_solid_outside[grand].getScore() +
								grandSiblingArcScore(g, i, o) +
								grandBiSiblingArcScore(g, i, l, o));
						}
						else {
							item.updateL2REmptyOutside(grand, s, swt->getScore() +
								biSiblingArcScore(i, -1, o) +
								grandSiblingArcScore(g, i, o) +
								grandBiSiblingArcScore(g, i, -1, o));
						}
					}
					// r2l_empty_outside
					for (const auto & swt : m_abFirstOrderEmptyScore[ENCODE_R2L(i)]) {
						const int & t = swt->getType();
						int s = ENCODE_EMPTY(i, t);
						int o = ENCODE_EMPTY(i, t);
						if (d > 1) {
							tscore base_r2l_empty_outside_score = swt->getScore() + biSiblingArcScore(l, i, o) + m_lItems[1][i].r2l[GRAND_INDEX(l, i, 1)].getScore();

							item.updateR2LEmptyOutside(grand, s, base_r2l_empty_outside_score +
								item.r2l_solid_outside[grand].getScore() +
								grandSiblingArcScore(g, l, o) +
								grandBiSiblingArcScore(g, l, i, o));
						}
						else {
							item.updateR2LEmptyOutside(grand, s, swt->getScore() +
								biSiblingArcScore(l, -1, o) +
								grandSiblingArcScore(g, l, o) +
								grandBiSiblingArcScore(g, l, -1, o));
						}

					}
					// l2r
					if (d > 1) {
						item.updateL2R(grand, l, item.l2r_solid_outside[grand].getScore() + m_lItems[1][l].l2r[GRAND_INDEX(i, l, 1)].getScore());
					}
					else {
						item.updateL2R(grand, l, 0);
					}
					if (item.l2r_empty_outside[grand].size() > 0) {
						item.updateL2R(grand, item.l2r_empty_outside[grand].bestItem().getSplit(), item.l2r_empty_outside[grand].bestItem().getScore());
					}
					// r2l
					if (d > 1) {
						item.updateR2L(grand, i, item.r2l_solid_outside[grand].getScore() + m_lItems[1][i].r2l[GRAND_INDEX(l, i, 1)].getScore());
					}
					else {
						item.updateR2L(grand, i, 0);
					}
					if (item.r2l_empty_outside[grand].size() > 0) {
						item.updateR2L(grand, item.r2l_empty_outside[grand].bestItem().getSplit(), item.r2l_empty_outside[grand].bestItem().getScore());
					}

				} while (++g <= m_nSentenceLength);

//				if (i == 24 && l == 25) {
//					item.print(23);
//				}
//				if (i == 25 && l == 25) {
//					item.print(23);
//				}
//				if (i == 24 && l == 24) {
//					item.print(24);
//				}
			}
		}
		// best root
		m_lItems[m_nSentenceLength + 1].push_back(StateItem());
		StateItem & item = m_lItems[m_nSentenceLength + 1][0];
		item.r2l.push_back(ScoreWithSplit());
		for (int m = 0; m < m_nSentenceLength; ++m) {
			item.updateR2L(0, m, m_lItems[m + 1][0].r2l[GRAND_INDEX(m_nSentenceLength, 0, m + 1)].getScore() +
				m_lItems[m_nSentenceLength - m][m].l2r[GRAND_INDEX(m_nSentenceLength, m, m_nSentenceLength - m)].getScore() +
				arcScore(m_nSentenceLength, m));
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
			int gid = GRAND_INDEX(grand, item.left, item.right - item.left + 1);

//			item.print(gid);

			switch (item.type) {
			case JUX:
				split = item.jux[gid].getSplit();

				m_lItems[split - item.left + 1][item.left].type = L2R;
				stack.push(std::tuple<int, int, int, int>(grand, split - item.left + 1, -1, item.left));
				m_lItems[item.right - split][split + 1].type = R2L;
				stack.push(std::tuple<int, int, int, int>(grand, item.right - split, -1, split + 1));
				break;
			case L2R:
				split = item.l2r[gid].getSplit();

				if (IS_EMPTY(split)) {
					item.type = L2R_EMPTY_OUTSIDE;
					std::get<2>(span) = item.l2r_empty_outside[gid].bestItem().getSplit();
					stack.push(span);
					break;
				}
				if (item.left == item.right) {
					break;
				}

				m_lItems[split - item.left + 1][item.left].type = L2R_SOLID_OUTSIDE;
				stack.push(std::tuple<int, int, int, int>(grand, split - item.left + 1, m_lItems[split - item.left + 1][item.left].l2r_solid_outside[GRAND_INDEX(grand, item.left, split - item.left + 1)].getSplit(), item.left));
				m_lItems[item.right - split + 1][split].type = L2R;
				stack.push(std::tuple<int, int, int, int>(item.left, item.right - split + 1, -1, split));
				break;
			case R2L:
				split = item.r2l[gid].getSplit();

				if (IS_EMPTY(split)) {
					item.type = R2L_EMPTY_OUTSIDE;
					std::get<2>(span) = item.r2l_empty_outside[gid].bestItem().getSplit();
					stack.push(span);
					break;
				}
				if (item.left == item.right) {
					break;
				}

				m_lItems[item.right - split + 1][split].type = R2L_SOLID_OUTSIDE;
				stack.push(std::tuple<int, int, int, int>(grand, item.right - split + 1, m_lItems[item.right - split + 1][split].r2l_solid_outside[GRAND_INDEX(grand, split, item.right - split + 1)].getSplit(), split));
				m_lItems[split - item.left + 1][item.left].type = R2L;
				stack.push(std::tuple<int, int, int, int>(item.right, split - item.left + 1, -1, item.left));
				break;
			case L2R_SOLID_BOTH:
				if (item.left == item.right) {
					break;
				}
				split = item.l2r_solid_both[gid].getSplit();
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
				split = item.r2l_solid_both[gid].getSplit();
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
				split = item.l2r_empty_inside[gid].getSplit();
				m_vecTrainArcs.push_back(BiGram<int>(item.left, item.right));

				split = DECODE_EMPTY_POS(split);

				m_lItems[split - item.left + 1][item.left].type = L2R_EMPTY_OUTSIDE;
				stack.push(std::tuple<int, int, int, int>(grand, split - item.left + 1, item.l2r_empty_inside[gid].getInnerSplit(), item.left));
				m_lItems[item.right - split][split + 1].type = R2L;
				stack.push(std::tuple<int, int, int, int>(item.left, item.right - split, -1, split + 1));
				break;
			case R2L_EMPTY_INSIDE:
				if (item.left == item.right) {
					break;
				}
				split = item.r2l_empty_inside[gid].getSplit();
				m_vecTrainArcs.push_back(BiGram<int>(item.right, item.left));

				split = DECODE_EMPTY_POS(split);

				m_lItems[item.right - split][split + 1].type = R2L_EMPTY_OUTSIDE;
				stack.push(std::tuple<int, int, int, int>(grand, item.right - split, item.r2l_empty_inside[gid].getInnerSplit(), split + 1));
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

				split = item.l2r_solid_outside[gid].getSplit();
				item.type = IS_EMPTY(split) ? L2R_EMPTY_INSIDE : L2R_SOLID_BOTH;
				stack.push(span);
				break;
			case R2L_SOLID_OUTSIDE:
				if (item.left == item.right) {
					break;
				}

				split = item.r2l_solid_outside[gid].getSplit();
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
		//std::cout << "positive" << std::endl;
		for (const auto & arc : positiveArcs) {
			//std::cout << arc << std::endl;
			if (arc.first() != -1) {
				getOrUpdateGrandScore(arc.first(), arc.second(), arc.forth(), 1);
				getOrUpdateGrandScore(arc.first(), arc.second(), arc.third(), arc.forth(), 1);
			}
			getOrUpdateSiblingScore(arc.second(), arc.third(), arc.forth(), 1);
			getOrUpdateSiblingScore(arc.second(), arc.forth(), 1);
		}
		//std::cout << "negative" << std::endl;
		for (const auto & arc : negativeArcs) {
			//std::cout << arc << std::endl;
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
//		for (const auto & arc : m_vecCorrectArcs) {
//			std::cout << arc.first() << " -> " << arc.second() << std::endl;
//		}
//		std::cout << std::endl;
//		for (const auto & arc : m_vecTrainArcs) {
//			std::cout << arc.first() << " -> " << arc.second() << std::endl;
//		}
//		std::cout << "train arcs size is " << m_vecCorrectArcs.size() << std::endl;
//		std::cout << "train tri arcs size is " << m_vecCorrectTriArcs.size() << std::endl;
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
//			auto start_time = GetTickCount();
			m_nRetval = m_setArcGoldScore.find(BiGram<int>(p, c)) == m_setArcGoldScore.end() ? GOLD_NEG_SCORE : GOLD_POS_SCORE;
//			m_tGetScoreTime += GetTickCount() - start_time;
			return m_nRetval;
		}
		m_nRetval = 0;
		getOrUpdateSiblingScore(p, c, 0);
		return m_nRetval;
	}

	const tscore & DepParser::biSiblingArcScore(const int & p, const int & c, const int & c2) {
		if (m_nState == ParserState::GOLDTEST) {
//			auto start_time = GetTickCount();
			m_nRetval = m_setBiSiblingArcGoldScore.find(TriGram<int>(p, c, c2)) == m_setBiSiblingArcGoldScore.end() ? GOLD_NEG_SCORE : GOLD_POS_SCORE;
//			m_tGetScoreTime += GetTickCount() - start_time;
			return m_nRetval;
		}
		m_nRetval = 0;
		getOrUpdateSiblingScore(p, c, c2, 0);
		return m_nRetval;
	}

//	const tscore & DepParser::triSiblingArcScore(const int & p, const int & c, const int & c2, const int & c3) {
//		if (m_nState == ParserState::GOLDTEST) {
////			auto start_time = GetTickCount();
//			m_nRetval = m_setTriSiblingArcGoldScore.find(QuarGram<int>(p, c, c2, c3)) == m_setTriSiblingArcGoldScore.end() ? GOLD_NEG_SCORE : GOLD_POS_SCORE;
////			m_tGetScoreTime += GetTickCount() - start_time;
//			return m_nRetval;
//		}
//		m_nRetval = 0;
//		getOrUpdateSiblingScore(p, c, c2, c3, 0);
//		return m_nRetval;
//	}

	const tscore & DepParser::grandSiblingArcScore(const int & g, const int & p, const int & c) {
		if (m_nState == ParserState::GOLDTEST) {
//			auto start_time = GetTickCount();
			m_nRetval = m_setGrandSiblingArcGoldScore.find(TriGram<int>(g, p, c)) == m_setGrandSiblingArcGoldScore.end() ? GOLD_NEG_SCORE : GOLD_POS_SCORE;
//			m_tGetScoreTime += GetTickCount() - start_time;
			return m_nRetval;
		}
		m_nRetval = 0;
		getOrUpdateGrandScore(g, p, c, 0);
		return m_nRetval;
	}

	const tscore & DepParser::grandBiSiblingArcScore(const int & g, const int & p, const int & c, const int & c2) {
		if (m_nState == ParserState::GOLDTEST) {
//			auto start_time = GetTickCount();
			m_nRetval = m_setGrandBiSiblingArcGoldScore.find(QuarGram<int>(g, p, c, c2)) == m_setGrandBiSiblingArcGoldScore.end() ? GOLD_NEG_SCORE : GOLD_POS_SCORE;
//			m_tGetScoreTime += GetTickCount() - start_time;
			return m_nRetval;
		}
		m_nRetval = 0;
		getOrUpdateGrandScore(g, p, c, c2, 0);
		return m_nRetval;
	}

//	const tscore & DepParser::grandTriSiblingArcScore(const int & g, const int & p, const int & c, const int & c2, const int & c3) {
//		if (m_nState == ParserState::GOLDTEST) {
////			auto start_time = GetTickCount();
//			m_nRetval = m_setGrandTriSiblingArcGoldScore.find(QuinGram<int>(g, p, c, c2, c3)) == m_setGrandTriSiblingArcGoldScore.end() ? GOLD_NEG_SCORE : GOLD_POS_SCORE;
////			m_tGetScoreTime += GetTickCount() - start_time;
//			return m_nRetval;
//		}
//		m_nRetval = 0;
//		getOrUpdateGrandScore(g, p, c, c2, c3, 0);
//		return m_nRetval;
//	}

	void DepParser::initArcScore(const int & d) {
		for (int i = 0, max_i = m_nSentenceLength - d + 1; i < max_i; ++i) {
			if (d > 1) {
				m_lArcScore[ENCODE_L2R(i)] = arcScore(i, i + d - 1);
				m_lArcScore[ENCODE_R2L(i)] = arcScore(i + d - 1, i);
			}
			m_abFirstOrderEmptyScore[ENCODE_L2R(i)].clear();
			m_abFirstOrderEmptyScore[ENCODE_R2L(i)].clear();

			for (int t = TEmptyTag::start(), max_t = TEmptyTag::end(); t < max_t; ++t) {
				if (m_nState == ParserState::GOLDTEST || testEmptyNode(i, ENCODE_EMPTY(i + d, t))) {
					tscore score = arcScore(i, ENCODE_EMPTY(i + d, t));
					if (score != 0) {
						m_abFirstOrderEmptyScore[ENCODE_L2R(i)].insertItem(ScoreWithType(t, score));
					}
				}
				if (m_nState == ParserState::GOLDTEST || testEmptyNode(i + d - 1, ENCODE_EMPTY(i, t))) {
					tscore score = arcScore(i + d - 1, ENCODE_EMPTY(i, t));
					if (score != 0) {
						m_abFirstOrderEmptyScore[ENCODE_R2L(i)].insertItem(ScoreWithType(t, score));
					}
				}
			}
		}
	}

	void DepParser::initBiSiblingArcScore(const int & d) {
		for (int i = 0, max_i = m_nSentenceLength - d + 1; i < max_i; ++i) {
			int l = i + d - 1;
			m_lBiSiblingArcScore[ENCODE_2ND_L2R(i, i)] = biSiblingArcScore(i, -1, l);
			m_lBiSiblingArcScore[ENCODE_2ND_R2L(i, i)] = biSiblingArcScore(l, -1, i);
			for (int k = i + 1, l = i + d - 1; k < l; ++k) {
				m_lBiSiblingArcScore[ENCODE_2ND_L2R(i, k)] = biSiblingArcScore(i, k, l);
				m_lBiSiblingArcScore[ENCODE_2ND_R2L(i, k)] = biSiblingArcScore(l, k, i);
			}
		}
	}

	void DepParser::initGrandSiblingArcScore(const int & d, const int & i) {
		int l = i + d - 1;
		int g = 0;
		do {
			if (g == i) {
				g = l + 1;
			}
			m_lGrandSiblingArcScore[ENCODE_L2R(g)] = grandSiblingArcScore(g, i, l);
			m_lGrandSiblingArcScore[ENCODE_R2L(g)] = grandSiblingArcScore(g, l, i);
		} while (++g <= m_nSentenceLength);
	}

	void DepParser::initGrandBiSiblingArcScore(const int & d, const int & i, const int & k) {
		int l = i + d - 1;
		int g = 0;
		do {
			if (g == i) {
				g = l + 1;
			}
			m_lGrandBiSiblingArcScore[ENCODE_L2R(g)] = grandBiSiblingArcScore(g, i, k, l);
			m_lGrandBiSiblingArcScore[ENCODE_R2L(g)] = grandBiSiblingArcScore(g, l, k, i);
		} while (++g <= m_nSentenceLength);
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

		Weight * cweight = (Weight*)m_Weight;

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

		Weight * cweight = (Weight*)m_Weight;

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
		Weight * cweight = (Weight*)m_Weight;

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
		Weight * cweight = (Weight*)m_Weight;

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
		Weight * cweight = (Weight*)m_Weight;

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
