#include <cmath>
#include <stack>
#include <algorithm>
#include <unordered_set>

#include "common/token/word.h"
#include "common/token/pos.h"
#include "common/token/emp.h"
#include "emptyeisner2nd_depparser.h"

namespace emptyeisner2nd {

	WordPOSTag DepParser::empty_taggedword = WordPOSTag();
	WordPOSTag DepParser::start_taggedword = WordPOSTag();
	WordPOSTag DepParser::end_taggedword = WordPOSTag();

	DepParser::DepParser(const std::string & sFeatureInput, const std::string & sFeatureOut, int nState) :
		DepParserBase(nState) {

		m_nSentenceLength = 0;

		m_pWeight = new Weightec2nd(sFeatureInput, sFeatureOut, m_nScoreIndex);

		DepParser::empty_taggedword.refer(TWord::code(EMPTY_WORD), TPOSTag::code(EMPTY_POSTAG));
		DepParser::start_taggedword.refer(TWord::code(START_WORD), TPOSTag::code(START_POSTAG));
		DepParser::end_taggedword.refer(TWord::code(END_WORD), TPOSTag::code(END_POSTAG));

		m_pWeight->init(DepParser::empty_taggedword, DepParser::start_taggedword, DepParser::end_taggedword);
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

		Arcs2BiArcs(m_vecCorrectArcs, m_vecCorrectBiArcs);

		if (m_nState == ParserState::GOLDTEST) {
			m_setArcGoldScore.clear();
			m_setBiSiblingArcGoldScore.clear();
			for (const auto & biarc : m_vecCorrectBiArcs) {
				m_setArcGoldScore.insert(BiGram<int>(biarc.first(), biarc.third()));
				m_setBiSiblingArcGoldScore.insert(TriGram<int>(biarc.first(), biarc.second(), biarc.third()));
			}
		}

		m_pWeight->referRound(round);
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
					(idx == 0 ? START_POSTAG : TPOSTag::key(m_lSentence[idx - 1][0].second())) +
					TEmptyTag::key(i) + SENT_POSTAG(token)
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
				TPOSTag::key(m_lSentence[idx - 1][0].second()) + TEmptyTag::key(i) + END_POSTAG
				),
				TPOSTag::code(TEmptyTag::key(i))
				);
		}
		work(retval, correct);
	}

	void DepParser::work(DependencyTree * retval, const DependencyTree & correct) {

		for (int d = 1; d <= m_nSentenceLength + 1; ++d) {
			m_lItems[d].clear();
		}

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
		initBiSiblingArcScore();

		for (int d = 1; d <= m_nSentenceLength + 1; ++d) {

			m_lItems[d] = std::vector<StateItem>(m_nSentenceLength - d + 1);

			for (int i = 0, max_i = m_nSentenceLength - d + 1; i < max_i; ++i) {

				int l = i + d - 1;
				StateItem & item = m_lItems[d][i];
				const tscore & l2r_arc_score = m_vecArcScore[i][l];
				const tscore & r2l_arc_score = m_vecArcScore[l][i];

				// initialize
				item.init(i, l);

				for (int s = i; s < l; ++s) {
					StateItem & litem = m_lItems[s - i + 1][i];
					StateItem & ritem = m_lItems[l - s][s + 1];

					item.updateJUX(s, litem.l2r.getScore() + ritem.r2l.getScore());

					// l2r_empty_inside
					tscore l_empty_inside_base_score = l2r_arc_score + ritem.r2l.getScore();
					for (const auto & l_beam : litem.l2r_empty_outside) {
						int p = ENCODE_EMPTY(s, DECODE_EMPTY_TAG(l_beam->getSplit()));
						int k = ENCODE_EMPTY(s + 1, DECODE_EMPTY_TAG(l_beam->getSplit()));
						int j = DECODE_EMPTY_POS(l_beam->getSplit());
						item.updateL2REmptyInside(p, l_beam->getSplit(), l_empty_inside_base_score +
							l_beam->getScore() +
							biSiblingArcScore(i, k, l) + // empty node bisibling
							biSiblingArcScore(i, j == i ? -1 : j, l)); // solid node bisibling
					}

					// r2l_empty_inside
					tscore r_empty_inside_base_score = r2l_arc_score + litem.l2r.getScore();
					for (const auto & r_beam : ritem.r2l_empty_outside) {
						int p = ENCODE_EMPTY(s, DECODE_EMPTY_TAG(r_beam->getSplit()));
						int k = ENCODE_EMPTY(s + 1, DECODE_EMPTY_TAG(r_beam->getSplit()));
						int j = DECODE_EMPTY_POS(r_beam->getSplit());
						item.updateR2LEmptyInside(p, r_beam->getSplit(), r_empty_inside_base_score +
							r_beam->getScore() +
							biSiblingArcScore(l, k, i) +
							biSiblingArcScore(l, j == l ? -1 : j, i));
					}
				}

				for (int k = i + 1; k < l; ++k) {

					StateItem & litem = m_lItems[k - i + 1][i];
					StateItem & ritem = m_lItems[l - k + 1][k];

					// l2r solid both
					tscore l_base_jux = ritem.jux.getScore() + l2r_arc_score + m_vecBiSiblingScore[i][l][k];
					item.updateL2RSolidBoth(k, l_base_jux + litem.l2r_solid_outside.getScore());
					// r2l solid both
					tscore r_base_jux = litem.jux.getScore() + r2l_arc_score + m_vecBiSiblingScore[l][i][k];
					item.updateR2LSolidBoth(k, r_base_jux + ritem.r2l_solid_outside.getScore());

					// l2r_empty_outside
					for (const auto & swt : m_vecFirstOrderEmptyScore[i][l]) {
						const int & t = swt->getType();
						// s is split with type
						int s = ENCODE_EMPTY(k, t);
						// o is outside empty
						int o = ENCODE_EMPTY(l + 1, t);
						tscore l_empty_outside_base_score = ritem.l2r.getScore() + swt->getScore() + biSiblingArcScore(i, k, o);
						item.updateL2REmptyOutside(s, l_empty_outside_base_score +
							litem.l2r_solid_outside.getScore());
					}
					// complete
					item.updateL2R(k, litem.l2r_solid_outside.getScore() + ritem.l2r.getScore());

					// r2l_empty_outside
					for (const auto & swt : m_vecFirstOrderEmptyScore[l][i]) {
						const int & t = swt->getType();
						int s = ENCODE_EMPTY(k, t);
						int o = ENCODE_EMPTY(i, t);
						tscore r_empty_outside_base_score = litem.r2l.getScore() + swt->getScore() + biSiblingArcScore(l, k, o);
						item.updateR2LEmptyOutside(s, r_empty_outside_base_score +
							ritem.r2l_solid_outside.getScore());
					}
					// complete
					item.updateR2L(k, ritem.r2l_solid_outside.getScore() + litem.r2l.getScore());
				}

				if (d > 1) {
					StateItem & litem = m_lItems[d - 1][i];
					StateItem & ritem = m_lItems[d - 1][i + 1];

					// l2r_solid_both
					item.updateL2RSolidBoth(i, ritem.r2l.getScore() +
						l2r_arc_score +
						m_vecBiSiblingScore[i][l][i]);
					// r2l_solid_both
					item.updateR2LSolidBoth(l, litem.l2r.getScore() +
						r2l_arc_score +
						m_vecBiSiblingScore[l][i][l]);

					item.updateL2R(l, item.l2r_solid_both.getScore());
					item.updateL2RSolidOutside(item.l2r_solid_both.getSplit(), item.l2r_solid_both.getScore());
					item.updateL2RSolidOutside(item.l2r_empty_inside.getSplit(), item.l2r_empty_inside.getScore());

					item.updateR2L(i, item.r2l_solid_both.getScore());
					item.updateR2LSolidOutside(item.r2l_solid_both.getSplit(), item.r2l_solid_both.getScore());
					item.updateR2LSolidOutside(item.r2l_empty_inside.getSplit(), item.r2l_empty_inside.getScore());
				}
				if (d > 1) {
					// l2r_empty_ouside
					for (const auto & swt : m_vecFirstOrderEmptyScore[i][l]) {
						const int & t = swt->getType();
						int s = ENCODE_EMPTY(l, t);
						int o = ENCODE_EMPTY(l + 1, t);
						tscore base_l2r_empty_outside_score = swt->getScore() + biSiblingArcScore(i, l, o) + m_lItems[1][l].l2r.getScore();
						item.updateL2REmptyOutside(s, base_l2r_empty_outside_score +
							item.l2r_solid_outside.getScore());
					}
					// l2r
					item.updateL2R(l, item.l2r_solid_outside.getScore() + m_lItems[1][l].l2r.getScore());

					// r2l_empty_outside
					for (const auto & swt : m_vecFirstOrderEmptyScore[l][i]) {
							const int & t = swt->getType();
							int s = ENCODE_EMPTY(i, t);
							int o = ENCODE_EMPTY(i, t);
							tscore base_r2l_empty_outside_score = swt->getScore() + biSiblingArcScore(l, i, o) + m_lItems[1][i].r2l.getScore();
							item.updateR2LEmptyOutside(s, base_r2l_empty_outside_score +
								item.r2l_solid_outside.getScore());
					}
					// r2l
					item.updateR2L(i, item.r2l_solid_outside.getScore() + m_lItems[1][i].r2l.getScore());
				}
				else {
					// l2r_empty_ouside
					for (const auto & swt : m_vecFirstOrderEmptyScore[i][l]) {
						const int & t = swt->getType();
						int s = ENCODE_EMPTY(l, t);
						int o = ENCODE_EMPTY(l + 1, t);
						item.updateL2REmptyOutside(s, swt->getScore() +
							biSiblingArcScore(i, -1, o));
					}
					// l2r
					item.updateL2R(l, 0);

					// r2l_empty_outside
					for (const auto & swt : m_vecFirstOrderEmptyScore[l][i]) {
						const int & t = swt->getType();
						int s = ENCODE_EMPTY(i, t);
						int o = ENCODE_EMPTY(i, t);
						item.updateR2LEmptyOutside(s, swt->getScore() +
							biSiblingArcScore(l, -1, o));
					}
					// r2l
					item.updateR2L(i, 0);
				}

				if (item.l2r_empty_outside.size() > 0) {
					item.updateL2R(item.l2r_empty_outside.bestItem().getSplit(), item.l2r_empty_outside.bestItem().getScore());
				}

				if (item.r2l_empty_outside.size() > 0) {
					item.updateR2L(item.r2l_empty_outside.bestItem().getSplit(), item.r2l_empty_outside.bestItem().getScore());
				}

//				if (m_nTrainingRound == 12) item.print(0);
			}

			if (d > 1) {
				// root
				m_lItems[d].push_back(StateItem());
				StateItem & item = m_lItems[d][m_nSentenceLength - d + 1];
				item.init(m_nSentenceLength - d + 1, m_nSentenceLength);
				// r2l_solid_outside
				item.updateR2LSolidOutside(m_nSentenceLength, m_lItems[d - 1][item.left].l2r.getScore() +
						m_vecArcScore[item.right][item.left] +
						m_vecBiSiblingScore[item.right][item.left][item.right]);
				// r2l
				item.updateR2L(item.left, item.r2l_solid_outside.getScore() + m_lItems[1][item.left].r2l.getScore());
				for (int i = item.left, s = item.left + 1, j = m_nSentenceLength + 1; s < j - 1; ++s) {
					item.updateR2L(s, m_lItems[j - s][s].r2l_solid_outside.getScore() + m_lItems[s - i + 1][i].r2l.getScore());
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
				stack.push(std::tuple<int, int, int>(split - item.left + 1, m_lItems[split - item.left + 1][item.left].l2r_solid_outside.getSplit(), item.left));
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
				stack.push(std::tuple<int, int, int>(item.right - split + 1, m_lItems[item.right - split + 1][split].r2l_solid_outside.getSplit(), split));
				m_lItems[split - item.left + 1][item.left].type = R2L;
				stack.push(std::tuple<int, int, int>(split - item.left + 1, -1, item.left));
				break;
			case L2R_SOLID_BOTH:
				if (item.left == item.right) {
					break;
				}
				split = item.l2r_solid_both.getSplit();
				m_vecTrainArcs.push_back(BiGram<int>(item.left, item.right));

				if (split == item.left) {
					m_lItems[item.right - split][split + 1].type = R2L;
					stack.push(std::tuple<int, int, int>(item.right - split, -1, split + 1));
				}
				else {
					m_lItems[split - item.left + 1][item.left].type = L2R_SOLID_OUTSIDE;
					stack.push(std::tuple<int, int, int>(split - item.left + 1, -1, item.left));
					m_lItems[item.right - split + 1][split].type = JUX;
					stack.push(std::tuple<int, int, int>(item.right - split + 1, -1, split));
				}
				break;
			case R2L_SOLID_BOTH:
				if (item.left == item.right) {
					break;
				}
				split = item.r2l_solid_both.getSplit();
				m_vecTrainArcs.push_back(BiGram<int>(item.right == m_nSentenceLength ? -1 : item.right, item.left));

				if (split == item.right) {
					m_lItems[split - item.left][item.left].type = L2R;
					stack.push(std::tuple<int, int, int>(split - item.left, -1, item.left));
				}
				else {
					m_lItems[item.right - split + 1][split].type = R2L_SOLID_OUTSIDE;
					stack.push(std::tuple<int, int, int>(item.right - split + 1, -1, split));
					m_lItems[split - item.left + 1][item.left].type = JUX;
					stack.push(std::tuple<int, int, int>(split - item.left + 1, -1, item.left));
				}
				break;
			case L2R_EMPTY_INSIDE:
				if (item.left == item.right) {
					break;
				}
				split = item.l2r_empty_inside.getSplit();
				m_vecTrainArcs.push_back(BiGram<int>(item.left, item.right));

				split = DECODE_EMPTY_POS(split);

				m_lItems[split - item.left + 1][item.left].type = L2R_EMPTY_OUTSIDE;
				stack.push(std::tuple<int, int, int>(split - item.left + 1, item.l2r_empty_inside.getInnerSplit(), item.left));
				m_lItems[item.right - split][split + 1].type = R2L;
				stack.push(std::tuple<int, int, int>(item.right - split, -1, split + 1));
				break;
			case R2L_EMPTY_INSIDE:
				if (item.left == item.right) {
					break;
				}
				split = item.r2l_empty_inside.getSplit();
				m_vecTrainArcs.push_back(BiGram<int>(item.right, item.left));

				split = DECODE_EMPTY_POS(split);

				m_lItems[item.right - split][split + 1].type = R2L_EMPTY_OUTSIDE;
				stack.push(std::tuple<int, int, int>(item.right - split, item.r2l_empty_inside.getInnerSplit(), split + 1));
				m_lItems[split - item.left + 1][item.left].type = L2R;
				stack.push(std::tuple<int, int, int>(split - item.left + 1, -1, item.left));
				break;
			case L2R_EMPTY_OUTSIDE:
				m_vecTrainArcs.push_back(BiGram<int>(item.left, ENCODE_EMPTY(item.right + 1, DECODE_EMPTY_TAG(split))));

				if (item.left == item.right) {
					break;
				}

				split = DECODE_EMPTY_POS(split);

				m_lItems[split - item.left + 1][item.left].type = L2R_SOLID_OUTSIDE;
				stack.push(std::tuple<int, int, int>(split - item.left + 1, -1, item.left));
				m_lItems[item.right - split + 1][split].type = L2R;
				stack.push(std::tuple<int, int, int>(item.right - split + 1, -1, split));
				break;
			case R2L_EMPTY_OUTSIDE:
				m_vecTrainArcs.push_back(BiGram<int>(item.right, ENCODE_EMPTY(item.left, DECODE_EMPTY_TAG(split))));

				if (item.left == item.right) {
					break;
				}

				split = DECODE_EMPTY_POS(split);

				m_lItems[item.right - split + 1][split].type = R2L_SOLID_OUTSIDE;
				stack.push(std::tuple<int, int, int>(item.right - split + 1, -1, split));
				m_lItems[split - item.left + 1][item.left].type = R2L;
				stack.push(std::tuple<int, int, int>(split - item.left + 1, -1, item.left));
				break;
			case L2R_SOLID_OUTSIDE:
				if (item.left == item.right) {
					break;
				}

				split = item.l2r_solid_outside.getSplit();
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

				split = item.r2l_solid_outside.getSplit();
				item.type = IS_EMPTY(split) ? R2L_EMPTY_INSIDE : R2L_SOLID_BOTH;
				stack.push(span);
				break;
			default:
				break;
			}
		}
	}

	void DepParser::update() {
		Arcs2BiArcs(m_vecTrainArcs, m_vecTrainBiArcs);

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
		for (const auto & arc : positiveArcs) {
			getOrUpdateSiblingScore(arc.first(), arc.second(), 1);
		}
		for (const auto & arc : negativeArcs) {
			getOrUpdateSiblingScore(arc.first(), arc.second(), -1);
		}


		std::unordered_set<BiArc> positiveBiArcs;
		positiveBiArcs.insert(m_vecCorrectBiArcs.begin(), m_vecCorrectBiArcs.end());
		for (const auto & arc : m_vecTrainBiArcs) {
			positiveBiArcs.erase(arc);
		}
		std::unordered_set<BiArc> negativeBiArcs;
		negativeBiArcs.insert(m_vecTrainBiArcs.begin(), m_vecTrainBiArcs.end());
		for (const auto & arc : m_vecCorrectBiArcs) {
			negativeBiArcs.erase(arc);
		}
		if (!positiveBiArcs.empty() || !negativeBiArcs.empty()) {
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
		for (const auto & arc : positiveBiArcs) {
			getOrUpdateSiblingScore(arc.first(), arc.second(), arc.third(), 1);
		}
		for (const auto & arc : negativeBiArcs) {
			getOrUpdateSiblingScore(arc.first(), arc.second(), arc.third(), -1);
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
		Arcs2BiArcs(m_vecTrainArcs, m_vecTrainBiArcs);
		std::cout << "total score is " <<  m_lItems[m_nSentenceLength + 1][0].r2l.getScore() << std::endl; //debug
		if (m_vecCorrectArcs.size() != m_vecTrainArcs.size() || m_lItems[m_nSentenceLength + 1][0].r2l.getScore() / GOLD_POS_SCORE != m_vecCorrectArcs.size() + m_vecCorrectBiArcs.size()) {
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
				for (const auto & biarc : m_vecTrainBiArcs) {
					std::cout << biarc << std::endl;
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
//		std::cout << p << " -> " << c << " " << m_nRetval << std::endl;
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
			decltype(m_vecArcScore)(
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
						(m_nSentenceLength == 0 ? START_POSTAG : TPOSTag::key(m_lSentence[m_nSentenceLength - 1][0].second())) +
						TEmptyTag::key(i) + TREENODE_POSTAG(node)
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
				TPOSTag::key(m_lSentence[m_nSentenceLength - 1][0].second()) + TEmptyTag::key(i) + END_POSTAG
				),
				TPOSTag::code(TEmptyTag::key(i))
				);
		}
		int idx = 0, idxwe = 0;
		ttoken empty_tag = "";
		for (const auto & node : tcorrect) {
			if (TREENODE_POSTAG(node) == EMPTYTAG) {
				m_lSentenceWithEmpty[idxwe++].refer(
					TWord::code((idx == 0 ? START_POSTAG : TPOSTag::key(m_lSentence[idx - 1][0].second())) +
					TREENODE_WORD(node) +
					(idx == m_nSentenceLength ? END_POSTAG : TPOSTag::key(m_lSentence[idx][0].second()))),
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
		return m_pWeight->testEmptyNode(p, c, m_lSentence);
	}

	void DepParser::getOrUpdateSiblingScore(const int & p, const int & c, const int & amount) {
		m_pWeight->getOrUpdateArcScore(m_nRetval, p, c, amount, m_nSentenceLength, m_lSentence);
	}

	void DepParser::getOrUpdateSiblingScore(const int & p, const int & c, const int & c2, const int & amount) {
		m_pWeight->getOrUpdateBiArcScore(m_nRetval, p, c, c2, amount, m_nSentenceLength, m_lSentence);
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

}
