#include <cmath>
#include <stack>
#include <algorithm>
#include <unordered_set>

#include "common/token/word.h"
#include "common/token/pos.h"
#include "common/token/emp.h"
#include "emptyeisner3rd_depparser.h"

namespace emptyeisner3rd {

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

	void DepParser::preTrainEmpty(const DependencyTree & correct, const int & round, const int & step) {
		// initialize
		m_vecCorrectArcs.clear();
		m_nTrainingRound = round;
		m_nSentenceLength = 0;

		readEmptySentAndArcs(correct);

		if (!testEmptyTree(m_vecCorrectArcs, m_nSentenceLength)) {
			std::cout << m_nTrainingRound << std::endl;
			std::cout << "tree error" << std::endl;
			std::cout << "arcs" << std::endl;
			for (const auto & arc : m_vecCorrectArcs) {
				std::cout << arc.first() << " " << arc.second() << std::endl;
			}
		}

		Arcs2TriArcs(m_vecCorrectArcs, m_vecCorrectTriArcs);

		for (const auto & arc : m_vecCorrectTriArcs) {
			//if (IS_EMPTY(arc.forth())) {
				getOrUpdateStackScore(arc.first(), arc.second(), arc.third(), arc.forth(), step);
				getOrUpdateStackScore(arc.first(), arc.third(), arc.forth(), step);
				getOrUpdateStackScore(arc.first(), arc.forth(), step);
			//}
		}
	}

	void DepParser::train(const DependencyTree & correct, const int & round) {
		// initialize
		m_vecCorrectArcs.clear();
		m_nTrainingRound = round;
		m_nSentenceLength = 0;

		readEmptySentAndArcs(correct);

		if (!testEmptyTree(m_vecCorrectArcs, m_nSentenceLength)) {
			std::cout << m_nTrainingRound << std::endl;
			std::cout << "tree error" << std::endl;
			std::cout << "arcs" << std::endl;
			for (const auto & arc : m_vecCorrectArcs) {
				std::cout << arc.first() << " " << arc.second() << std::endl;
			}
		}

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
	}

	void DepParser::decode() {
		for (int d = 1; d <= m_nSentenceLength + 1; ++d) {
			initFirstOrderScore(d);
			if (d > 1) {
				initSecondOrderScore(d);
			}
			for (int i = 0, max_i = m_nSentenceLength - d + 1; i < max_i; ++i) {
				int l = i + d - 1;
				tscore l2r_arc_score = m_lFirstOrderScore[ENCODE_L2R(i)];
				tscore r2l_arc_score = m_lFirstOrderScore[ENCODE_R2L(i)];
				StateItem & item = m_lItems[d][i];

				// initialize
				item.init(i, l);

				for (int s = i; s < l; ++s) {
					const StateItem & litem = m_lItems[s - i + 1][i];
					const StateItem & ritem = m_lItems[l - s][s + 1];
					// jux
					item.updateJUX(s, litem.l2r.getScore() + ritem.r2l.getScore());
					// l2r_empty_inside
					tscore l_empty_inside_base_score = l2r_arc_score + ritem.r2l.getScore();
					for (const auto & l_beam : litem.l2r_empty_outside) {
						int p = ENCODE_EMPTY(s, DECODE_EMPTY_TAG(l_beam->getSplit()));
						int k = ENCODE_EMPTY(s + 1, DECODE_EMPTY_TAG(l_beam->getSplit()));
						int j = DECODE_EMPTY_POS(l_beam->getSplit());
						item.updateL2REmptyInside(p, l_beam->getSplit(), l_empty_inside_base_score +
							l_beam->getScore() +
							twoArcScore(i, k, l) +
							triArcScore(i, j == i ? -1 : j, k, l));
					}
					// r2l_empty_inside
					tscore r_empty_inside_base_score = r2l_arc_score + litem.l2r.getScore();
					for (const auto & r_beam : ritem.r2l_empty_outside) {
						int p = ENCODE_EMPTY(s, DECODE_EMPTY_TAG(r_beam->getSplit()));
						int k = ENCODE_EMPTY(s + 1, DECODE_EMPTY_TAG(r_beam->getSplit()));
						int j = DECODE_EMPTY_POS(r_beam->getSplit());
						item.updateR2LEmptyInside(p, r_beam->getSplit(), r_empty_inside_base_score +
							r_beam->getScore() +
							twoArcScore(l, k, i) +
							triArcScore(l, j == l ? -1 : j, k, i));
					}
				}
				for (int k = i + 1; k < l; ++k) {
					StateItem & litem = m_lItems[k - i + 1][i];
					StateItem & ritem = m_lItems[l - k + 1][k];

					const auto & l_beam = litem.l2r_solid_outside;
					const auto & r_beam = ritem.r2l_solid_outside;

					// l2r_solid_both
					tscore l_solid_both_base_score = ritem.jux.getScore() + l2r_arc_score + m_lSecondOrderScore[ENCODE_2ND_L2R(i, k)];
					for (const auto & swbs : l_beam) {
						// j is inner split
						const int & j = swbs->getSplit();
						item.updateL2RSolidBoth(k, j, l_solid_both_base_score +
							swbs->getScore() +
							triArcScore(i, j == i ? -1 : IS_EMPTY(j) ? j + 1 : j, k, l));
					}
					// l2r_empty_outside
					for (const auto & swt : m_abFirstOrderEmptyScore[ENCODE_L2R(i)]) {
						const int & t = swt->getType();
						// s is split with type
						int s = ENCODE_EMPTY(k, t);
						// o is outside empty
						int o = ENCODE_EMPTY(l + 1, t);
						tscore l_empty_outside_base_score = ritem.l2r.getScore() + swt->getScore() + twoArcScore(i, k, o);
						for (const auto & swbs : l_beam) {
							// j is inner split
							const int & j = swbs->getSplit();
							item.updateL2REmptyOutside(s, j, l_empty_outside_base_score +
								swbs->getScore() +
								triArcScore(i, j == i ? -1 : IS_EMPTY(j) ? j + 1 : j, k, o));
						}
					}
					// l2r
					item.updateL2R(k, litem.l2r_solid_outside.bestItem().getScore() + ritem.l2r.getScore());
					// r2l_solid_both
					tscore r_solid_both_base_score = litem.jux.getScore() + r2l_arc_score + m_lSecondOrderScore[ENCODE_2ND_R2L(i, k)];
					for (const auto & swbs : r_beam) {
						const int & j = swbs->getSplit();
						item.updateR2LSolidBoth(k, j, r_solid_both_base_score +
							swbs->getScore() +
							triArcScore(l, j == l ? -1 : IS_EMPTY(j) ? j + 1 : j, k, i));
					}
					// r2l_empty_outside
					for (const auto & swt : m_abFirstOrderEmptyScore[ENCODE_R2L(i)]) {
						const int & t = swt->getType();
						int s = ENCODE_EMPTY(k, t);
						int o = ENCODE_EMPTY(i, t);
						tscore r_empty_outside_base_score = litem.r2l.getScore() + swt->getScore() + twoArcScore(l, k, o);
						for (const auto & swbs : r_beam) {
							const int & j = swbs->getSplit();
							item.updateR2LEmptyOutside(s, j, r_empty_outside_base_score +
								swbs->getScore() +
								triArcScore(l, j == l ? -1 : IS_EMPTY(j) ? j + 1 : j, k, o));
						}
					}
					// r2l
					item.updateR2L(k, ritem.r2l_solid_outside.bestItem().getScore() + litem.r2l.getScore());
				}
				if (d > 1) {
					// l2r_solid_both
					item.updateL2RSolidBoth(i, i, m_lItems[d - 1][i + 1].r2l.getScore() +
						l2r_arc_score +
						m_lSecondOrderScore[ENCODE_2ND_L2R(i, i)] +
						triArcScore(i, -1, -1, l));
					// r2l_solid_both
					item.updateR2LSolidBoth(l, l, m_lItems[d - 1][i].l2r.getScore() +
						r2l_arc_score +
						m_lSecondOrderScore[ENCODE_2ND_R2L(i, i)] +
						triArcScore(l, -1, -1, i));
					// l2r_solid_outside
					for (const auto & swbs : item.l2r_empty_inside) {
						item.updateL2RSolidOutside(swbs->getSplit(), swbs->getInnerSplit(), swbs->getScore());
					}
					for (const auto & swbs : item.l2r_solid_both) {
						item.updateL2RSolidOutside(swbs->getSplit(), swbs->getInnerSplit(), swbs->getScore());
					}
					// r2l_solid_outside
					for (const auto & swbs : item.r2l_empty_inside) {
						item.updateR2LSolidOutside(swbs->getSplit(), swbs->getInnerSplit(), swbs->getScore());
					}
					for (const auto & swbs : item.r2l_solid_both) {
						item.updateR2LSolidOutside(swbs->getSplit(), swbs->getInnerSplit(), swbs->getScore());
					}
				}
				// l2r_empty_ouside
				for (const auto & swt : m_abFirstOrderEmptyScore[ENCODE_L2R(i)]) {
					const int & t = swt->getType();
					int s = ENCODE_EMPTY(l, t);
					int o = ENCODE_EMPTY(l + 1, t);
					if (d > 1) {
						tscore base_l2r_empty_outside_score = swt->getScore() + twoArcScore(i, l, o) + m_lItems[1][l].l2r.getScore();
						for (const auto & swbs : item.l2r_solid_outside) {
							const int & j = swbs->getSplit();
							item.updateL2REmptyOutside(s, swbs->getSplit(), base_l2r_empty_outside_score +
								swbs->getScore() +
								triArcScore(i, j == i ? -1 : IS_EMPTY(j) ? j + 1 : j, l, o));
						}
					}
					else {
						item.updateL2REmptyOutside(s, i, swt->getScore() +
							twoArcScore(i, -1, o) +
							triArcScore(i, -1, -1, o));
					}
				}
				// r2l_empty_outside
				for (const auto & swt : m_abFirstOrderEmptyScore[ENCODE_R2L(i)]) {
					const int & t = swt->getType();
					int s = ENCODE_EMPTY(i, t);
					int o = ENCODE_EMPTY(i, t);
					if (d > 1) {
						tscore base_r2l_empty_outside_score = swt->getScore() + twoArcScore(l, i, o) + m_lItems[1][i].r2l.getScore();
						for (const auto & swbs : item.r2l_solid_outside) {
							const int & j = swbs->getSplit();
							item.updateR2LEmptyOutside(s, swbs->getSplit(), base_r2l_empty_outside_score +
								swbs->getScore() +
								triArcScore(l, j == l ? -1 : IS_EMPTY(j) ? j + 1 : j, i, o));
						}
					}
					else {
						item.updateR2LEmptyOutside(s, l, swt->getScore() +
							twoArcScore(l, -1, o) +
							triArcScore(l, -1, -1, o));
					}

				}
				// l2r
				item.updateL2R(l, item.l2r_solid_outside.bestItem().getScore() + m_lItems[1][l].l2r.getScore());
				if (item.l2r_empty_outside.size() > 0) {
					item.updateL2R(item.l2r_empty_outside.bestItem().getSplit(), item.l2r_empty_outside.bestItem().getScore());
				}
				// r2l
				item.updateR2L(i, item.r2l_solid_outside.bestItem().getScore() + m_lItems[1][i].r2l.getScore());
				if (item.r2l_empty_outside.size() > 0) {
					item.updateR2L(item.r2l_empty_outside.bestItem().getSplit(), item.r2l_empty_outside.bestItem().getScore());
				}

			}
			if (d > 1) {
				// root
				StateItem & item = m_lItems[d][m_nSentenceLength - d + 1];
				item.init(m_nSentenceLength - d + 1, m_nSentenceLength);
				// r2l_solid_outside
				item.updateR2LSolidOutside(m_nSentenceLength, m_nSentenceLength, m_lItems[d - 1][item.left].l2r.getScore() +
					m_lFirstOrderScore[ENCODE_R2L(item.left)] +
					m_lSecondOrderScore[ENCODE_2ND_R2L(item.left, item.left)] +
					triArcScore(item.right, -1, -1, item.left));
				// r2l
				item.updateR2L(item.left, item.r2l_solid_outside.bestItem().getScore() + m_lItems[1][item.left].r2l.getScore());
				for (int i = item.left, s = item.left + 1, j = m_nSentenceLength + 1; s < j - 1; ++s) {
					item.updateR2L(s, m_lItems[j - s][s].r2l_solid_outside.bestItem().getScore() + m_lItems[s - i + 1][i].r2l.getScore());
				}

			}
		}
	}

	void DepParser::decodeArcs() {
		m_vecTrainArcs.clear();
		std::stack<std::tuple<int, int, int>> stack;
		stack.push(std::tuple<int, int, int>(m_nSentenceLength + 1, -1, 0));
		m_lItems[m_nSentenceLength + 1][0].type = R2L;

		while (!stack.empty()) {
			std::tuple<int, int, int> span = stack.top();
			stack.pop();
			StateItem & item = m_lItems[std::get<0>(span)][std::get<2>(span)];
			int split = std::get<1>(span);
			int innersplit = -1;

			switch (item.type) {
			case JUX:
				split = item.jux.getSplit();

				m_lItems[split - item.left + 1][item.left].type = L2R;
				stack.push(std::tuple<int, int, int>(split - item.left + 1, -1, item.left));
				m_lItems[item.right - split][split + 1].type = R2L;
				stack.push(std::tuple<int, int, int>(item.right - split, -1, split + 1));
				break;
			case L2R:
				split = item.l2r.getSplit();

				if (IS_EMPTY(split)) {
					item.type = L2R_EMPTY_OUTSIDE;
					std::get<1>(span) = item.l2r_empty_outside.bestItem().getSplit();
					stack.push(span);
					break;
				}
				if (item.left == item.right) {
					break;
				}

				m_lItems[split - item.left + 1][item.left].type = L2R_SOLID_OUTSIDE;
				stack.push(std::tuple<int, int, int>(split - item.left + 1, m_lItems[split - item.left + 1][item.left].l2r_solid_outside.bestItem().getSplit(), item.left));
				m_lItems[item.right - split + 1][split].type = L2R;
				stack.push(std::tuple<int, int, int>(item.right - split + 1, -1, split));
				break;
			case R2L:
				split = item.r2l.getSplit();

				if (IS_EMPTY(split)) {
					item.type = R2L_EMPTY_OUTSIDE;
					std::get<1>(span) = item.r2l_empty_outside.bestItem().getSplit();
					stack.push(span);
					break;
				}
				if (item.left == item.right) {
					break;
				}

				m_lItems[item.right - split + 1][split].type = R2L_SOLID_OUTSIDE;
				stack.push(std::tuple<int, int, int>(item.right - split + 1, m_lItems[item.right - split + 1][split].r2l_solid_outside.bestItem().getSplit(), split));
				m_lItems[split - item.left + 1][item.left].type = R2L;
				stack.push(std::tuple<int, int, int>(split - item.left + 1, -1, item.left));
				break;
			case L2R_SOLID_BOTH:
				if (item.left == item.right) {
					break;
				}
				m_vecTrainArcs.push_back(BiGram<int>(item.left, item.right));

				if (split == item.left) {
					m_lItems[item.right - split][split + 1].type = R2L;
					stack.push(std::tuple<int, int, int>(item.right - split, -1, split + 1));
				}
				else {
					item.l2r_solid_both.sortItems();
					innersplit = findInnerSplit(item.l2r_solid_both, split);

					m_lItems[split - item.left + 1][item.left].type = L2R_SOLID_OUTSIDE;
					stack.push(std::tuple<int, int, int>(split - item.left + 1, innersplit, item.left));
					m_lItems[item.right - split + 1][split].type = JUX;
					stack.push(std::tuple<int, int, int>(item.right - split + 1, -1, split));
				}
				break;
			case R2L_SOLID_BOTH:
				if (item.left == item.right) {
					break;
				}
				m_vecTrainArcs.push_back(BiGram<int>(item.right == m_nSentenceLength ? -1 : item.right, item.left));

				if (split == item.right) {
					m_lItems[split - item.left][item.left].type = L2R;
					stack.push(std::tuple<int, int, int>(split - item.left, -1, item.left));
				}
				else {
					item.r2l_solid_both.sortItems();
					innersplit = findInnerSplit(item.r2l_solid_both, split);

					m_lItems[item.right - split + 1][split].type = R2L_SOLID_OUTSIDE;
					stack.push(std::tuple<int, int, int>(item.right - split + 1, innersplit, split));
					m_lItems[split - item.left + 1][item.left].type = JUX;
					stack.push(std::tuple<int, int, int>(split - item.left + 1, -1, item.left));
				}
				break;
			case L2R_EMPTY_INSIDE:
				if (item.left == item.right) {
					break;
				}
				m_vecTrainArcs.push_back(BiGram<int>(item.left, item.right));

				item.l2r_empty_inside.sortItems();
				innersplit = findInnerSplit(item.l2r_empty_inside, split);
				split = DECODE_EMPTY_POS(split);

				m_lItems[split - item.left + 1][item.left].type = L2R_EMPTY_OUTSIDE;
				stack.push(std::tuple<int, int, int>(split - item.left + 1, innersplit, item.left));
				m_lItems[item.right - split][split + 1].type = R2L;
				stack.push(std::tuple<int, int, int>(item.right - split, -1, split + 1));
				break;
			case R2L_EMPTY_INSIDE:
				if (item.left == item.right) {
					break;
				}
				m_vecTrainArcs.push_back(BiGram<int>(item.right, item.left));

				item.r2l_empty_inside.sortItems();
				innersplit = findInnerSplit(item.r2l_empty_inside, split);
				split = DECODE_EMPTY_POS(split);

				m_lItems[item.right - split][split + 1].type = R2L_EMPTY_OUTSIDE;
				stack.push(std::tuple<int, int, int>(item.right - split, innersplit, split + 1));
				m_lItems[split - item.left + 1][item.left].type = L2R;
				stack.push(std::tuple<int, int, int>(split - item.left + 1, -1, item.left));
				break;
			case L2R_EMPTY_OUTSIDE:
				m_vecTrainArcs.push_back(BiGram<int>(item.left, ENCODE_EMPTY(item.right + 1, DECODE_EMPTY_TAG(split))));

				if (item.left == item.right) {
					break;
				}

				innersplit = findInnerSplit(item.l2r_empty_outside, split);
				split = DECODE_EMPTY_POS(split);

				m_lItems[split - item.left + 1][item.left].type = L2R_SOLID_OUTSIDE;
				stack.push(std::tuple<int, int, int>(split - item.left + 1, innersplit, item.left));
				m_lItems[item.right - split + 1][split].type = L2R;
				stack.push(std::tuple<int, int, int>(item.right - split + 1, -1, split));
				break;
			case R2L_EMPTY_OUTSIDE:
				m_vecTrainArcs.push_back(BiGram<int>(item.right, ENCODE_EMPTY(item.left, DECODE_EMPTY_TAG(split))));

				if (item.left == item.right) {
					break;
				}

				innersplit = findInnerSplit(item.r2l_empty_outside, split);
				split = DECODE_EMPTY_POS(split);

				m_lItems[item.right - split + 1][split].type = R2L_SOLID_OUTSIDE;
				stack.push(std::tuple<int, int, int>(item.right - split + 1, innersplit, split));
				m_lItems[split - item.left + 1][item.left].type = R2L;
				stack.push(std::tuple<int, int, int>(split - item.left + 1, -1, item.left));
				break;
			case L2R_SOLID_OUTSIDE:
				if (item.left == item.right) {
					break;
				}

				item.type = IS_EMPTY(split) ? L2R_EMPTY_INSIDE : L2R_SOLID_BOTH;
				stack.push(span);
				break;
			case R2L_SOLID_OUTSIDE:
				if (item.left == item.right) {
					break;
				}
				if (item.right == m_nSentenceLength) {
					m_vecTrainArcs.push_back(BiGram<int>(-1, item.left));
					m_lItems[item.right - item.left][item.left].type = L2R;
					stack.push(std::tuple<int, int, int>(item.right - item.left, -1, item.left));
					break;
				}

				item.type = IS_EMPTY(split) ? R2L_EMPTY_INSIDE : R2L_SOLID_BOTH;
				stack.push(span);
				break;
			default:
				break;
			}
		}

		if (!testEmptyTree(m_vecTrainArcs, m_nSentenceLength)) {
			if (m_vecTrainArcs.size() != m_nSentenceLength) {
				std::cout << "len error" << std::endl;
				std::cout << "correct len is " << m_vecCorrectArcs.size() << std::endl;
				std::cout << "train len is " << m_vecTrainArcs.size() << std::endl;
			}
			else {
				std::cout << "tree error" << std::endl;
			}
			std::cout << "train arcs" << std::endl;
			for (const auto & arc : m_vecTrainArcs) {
				std::cout << "from " << arc.first() << " to " << arc.second() << std::endl;
			}
			std::cout << "correct arcs" << std::endl;
			for (const auto & arc : m_vecCorrectArcs) {
				std::cout << "from " << arc.first() << " to " << arc.second() << std::endl;
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
		//std::cout << "positive" << std::endl;	//debug
		for (const auto & arc : positiveArcs) {
			//std::cout << arc << std::endl;	//debug
			getOrUpdateStackScore(arc.first(), arc.second(), arc.third(), arc.forth(), 1);
			getOrUpdateStackScore(arc.first(), arc.third(), arc.forth(), 1);
			getOrUpdateStackScore(arc.first(), arc.forth(), 1);
		}
		//std::cout << "negative" << std::endl;	//debug
		for (const auto & arc : negativeArcs) {
			//std::cout << arc << std::endl;	//debug
			getOrUpdateStackScore(arc.first(), arc.second(), arc.third(), arc.forth(), -1);
			getOrUpdateStackScore(arc.first(), arc.third(), arc.forth(), -1);
			getOrUpdateStackScore(arc.first(), arc.forth(), -1);
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
		if (m_vecCorrectArcs.size() != m_vecTrainArcs.size() || m_lItems[m_nSentenceLength + 1][0].r2l.getScore() / GOLD_POS_SCORE != 3 * m_vecTrainArcs.size()) {
			++m_nTotalErrors;
			std::cout << "gold parse len error at " << m_nTrainingRound << std::endl;
			for (const auto & triarc : m_vecTrainTriArcs) {
				std::cout << triarc.first() << " " << triarc.second() << " " << triarc.third() << " " << triarc.forth() << std::endl;
			}
			std::cout << std::endl;
		}
		else {
			int i = 0;
			std::sort(m_vecCorrectArcs.begin(), m_vecCorrectArcs.end(), [](const Arc & arc1, const Arc & arc2){ return arc1 < arc2; });
			std::sort(m_vecTrainArcs.begin(), m_vecTrainArcs.end(), [](const Arc & arc1, const Arc & arc2){ return arc1 < arc2; });
			for (int n = m_vecCorrectArcs.size(); i < n; ++i) {
				if (m_vecCorrectArcs[i].first() != m_vecTrainArcs[i].first() || m_vecCorrectArcs[i].second() != m_vecTrainArcs[i].second()) {
					++m_nTotalErrors;
					break;
				}
			}
			if (i != m_vecCorrectArcs.size()) {
				std::cout << "gold parse tree error at " << m_nTrainingRound << std::endl;
				for (const auto & triarc : m_vecTrainTriArcs) {
					std::cout << triarc.first() << " " << triarc.second() << " " << triarc.third() << " " << triarc.forth() << std::endl;
				}
				std::cout << std::endl;
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
			if (d > 1) {
				m_lFirstOrderScore[ENCODE_L2R(i)] = arcScore(i, i + d - 1);
				m_lFirstOrderScore[ENCODE_R2L(i)] = arcScore(i + d - 1, i);
			}
			m_abFirstOrderEmptyScore[ENCODE_L2R(i)].clear();
			m_abFirstOrderEmptyScore[ENCODE_R2L(i)].clear();

			for (int t = TEmptyTag::start(), max_t = TEmptyTag::end(); t < max_t; ++t) {
				if (m_nState == ParserState::GOLDTEST || testEmptyNode(i, ENCODE_EMPTY(i + d, t))) {
					const tscore & score = arcScore(i, ENCODE_EMPTY(i + d, t));
					m_abFirstOrderEmptyScore[ENCODE_L2R(i)].insertItem(ScoreWithType(t, score));
				}
				if (m_nState == ParserState::GOLDTEST || testEmptyNode(i + d - 1, ENCODE_EMPTY(i, t))) {
					const tscore & score = arcScore(i + d - 1, ENCODE_EMPTY(i, t));
					m_abFirstOrderEmptyScore[ENCODE_R2L(i)].insertItem(ScoreWithType(t, score));
				}
			}
			m_abFirstOrderEmptyScore[ENCODE_L2R(i)].sortItems();
			m_abFirstOrderEmptyScore[ENCODE_R2L(i)].sortItems();
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

	void DepParser::getOrUpdateStackScore(const int & p, const int & c, const int & amount) {

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

		word_word_tag_tag_int.refer(p_word, c_word, p_tag, c_tag, m_nDis);
		cweight->m_mapPwpCwp.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPwpCwp.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_tag_tag_int.refer(c_word, p_tag, c_tag, m_nDis);
		cweight->m_mapPpCwp.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpCwp.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_tag_int.refer(p_word, c_word, p_tag, m_nDis);
		cweight->m_mapPwpCw.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_int.referLast(m_nDir);
		cweight->m_mapPwpCw.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_tag_int.refer(p_word, c_word, c_tag, m_nDis);
		cweight->m_mapPwCwp.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_tag_int.referLast(m_nDir);
		cweight->m_mapPwCwp.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_int.refer(p_word, c_word, m_nDis);
		cweight->m_mapPwCw.getOrUpdateScore(m_nRetval, word_word_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_int.referLast(m_nDir);
		cweight->m_mapPwCw.getOrUpdateScore(m_nRetval, word_word_int, m_nScoreIndex, amount, m_nTrainingRound);

		if (tag_c == 0 || amount != 0) {
			tag_tag_int.refer(p_tag, c_tag, m_nDis);
			cweight->m_mapPpCp.getOrUpdateScore(m_nRetval, tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
			tag_tag_int.referLast(m_nDir);
			cweight->m_mapPpCp.getOrUpdateScore(m_nRetval, tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		}

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

			word_word_tag_tag_int.refer(p_word, c_word, p_tag, c_tag, 0);
			cweight->m_mapPwpCwp.getOrUpdateScore(m_nRetval, word_word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

			word_tag_tag_int.refer(p_word, p_tag, c_tag, 0);
			cweight->m_mapPwpCp.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
			word_tag_tag_int.referLast(m_nDis);
			cweight->m_mapPwpCp.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
			word_tag_tag_int.referLast(m_nDir);
			cweight->m_mapPwpCp.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

			word_tag_tag_int.refer(c_word, p_tag, c_tag, 0);
			cweight->m_mapPpCwp.getOrUpdateScore(m_nRetval, word_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

			word_word_tag_int.refer(p_word, c_word, p_tag, 0);
			cweight->m_mapPwpCw.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

			word_word_tag_int.refer(p_word, c_word, c_tag, 0);
			cweight->m_mapPwCwp.getOrUpdateScore(m_nRetval, word_word_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

			word_word_int.refer(p_word, c_word, 0);
			cweight->m_mapPwCw.getOrUpdateScore(m_nRetval, word_word_int, m_nScoreIndex, amount, m_nTrainingRound);

			tag_tag_int.refer(p_tag, c_tag, 0);
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
	}

	void DepParser::getOrUpdateStackScore(const int & p, const int & c, const int & c2, const int & amount) {
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

		tag_tag_tag_int.refer(p_tag, c_tag, c2_tag, 0);
		cweight->m_mapPpC1pC2p.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_tag_int.referLast(m_nDir);
		cweight->m_mapPpC1pC2p.getOrUpdateScore(m_nRetval, tag_tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

		word_word_int.refer(c_word, c2_word, 0);
		cweight->m_mapC1wC2w.getOrUpdateScore(m_nRetval, word_word_int, m_nScoreIndex, amount, m_nTrainingRound);
		word_word_int.referLast(m_nDir);
		cweight->m_mapC1wC2w.getOrUpdateScore(m_nRetval, word_word_int, m_nScoreIndex, amount, m_nTrainingRound);

		tag_tag_int.refer(c_tag, c2_tag, 0);
		cweight->m_mapC1pC2p.getOrUpdateScore(m_nRetval, tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);
		tag_tag_int.referLast(m_nDir);
		cweight->m_mapC1pC2p.getOrUpdateScore(m_nRetval, tag_tag_int, m_nScoreIndex, amount, m_nTrainingRound);

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

		int pos_c = DECODE_EMPTY_POS(c);
		int tag_c = DECODE_EMPTY_TAG(c);

		int pos_c2 = DECODE_EMPTY_POS(c2);
		int tag_c2 = DECODE_EMPTY_TAG(c2);

		int pos_c3 = DECODE_EMPTY_POS(c3);
		int tag_c3 = DECODE_EMPTY_TAG(c3);

		p_word = m_lSentence[p][0].first();
		p_tag = m_lSentence[p][0].second();

		c_word = IS_NULL(c) ? empty_taggedword.first() : m_lSentence[pos_c][tag_c].first();
		c_tag = IS_NULL(c) ? empty_taggedword.second() : m_lSentence[pos_c][tag_c].second();

		c2_word = IS_NULL(c2) ? empty_taggedword.first() : m_lSentence[pos_c2][tag_c2].first();
		c2_tag = IS_NULL(c2) ? empty_taggedword.second() : m_lSentence[pos_c2][tag_c2].second();

		c3_word = m_lSentence[pos_c3][tag_c3].first();
		c3_tag = m_lSentence[pos_c3][tag_c3].second();

		int pc = IS_EMPTY(c) ? (pos_c > p ? pos_c : pos_c - 1) : c;
		m_nDir = encodeLinkDistanceOrDirection(p, pc, true);

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
