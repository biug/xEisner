#include <cmath>
#include <stack>
#include <thread>
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
		m_nSentenceCount = 0;

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
		m_nSentenceLength = 0;
		m_nRealEmpty = 0;
		m_nSentenceCount = round;

		readEmptySentAndArcs(correct);

		Arcs2BiArcs(m_vecCorrectArcs, m_vecCorrectBiArcs);

		m_nMaxEmpty = 25;
		if (m_nSentenceLength <= 10) m_nMaxEmpty = 4;
		else if (m_nSentenceLength <= 20) m_nMaxEmpty = 8;
		else if (m_nSentenceLength <= 50) m_nMaxEmpty = 12;
		else if (m_nSentenceLength <= 100) m_nMaxEmpty = 20;

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
		if (m_nSentenceCount % OUTPUT_STEP == 0) {
			std::cout << m_nTotalErrors << " / " << m_nTrainingRound << " with " << m_nSentenceCount << " sent" << std::endl;
			//printTime();
		}
	}

	void DepParser::parse(const Sentence & sentence, DependencyTree * retval) {
		int idx = 0;
		m_nRealEmpty = 0;
		m_nTrainingRound = 0;
		DependencyTree correct;
		m_nSentenceLength = sentence.size();

		m_nMaxEmpty = 25;
		if (m_nSentenceLength <= 10) m_nMaxEmpty = 4;
		else if (m_nSentenceLength <= 20) m_nMaxEmpty = 8;
		else if (m_nSentenceLength <= 50) m_nMaxEmpty = 12;
		else if (m_nSentenceLength <= 100) m_nMaxEmpty = 20;

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

		// find best average tree
		int maxEC = m_nRealEmpty;
		double maxScore = (double)m_lItems[0][m_nSentenceLength][maxEC].states[R2L].score / (double)(m_nSentenceLength + maxEC);
		for (int ec = 0; ec <= m_nMaxEmpty; ++ec) {
			double averageScore = (double)m_lItems[0][m_nSentenceLength][ec].states[R2L].score / (double)(m_nSentenceLength + ec);
			if (averageScore > maxScore) {
				maxEC = ec;
				maxScore = averageScore;
			}
		}

		switch (m_nState) {
		case ParserState::TRAIN:
			// update twice
			decodeArcs(maxEC);
			update();
			if (maxEC != m_nRealEmpty) {
				decodeArcs(m_nRealEmpty);
				update();
			}
			std::cout << "real empty node is " << m_nRealEmpty << " train empty node is " << maxEC << std::endl;
			break;
		case ParserState::PARSE:
			decodeArcs(maxEC);
			generate(retval, correct);
			break;
		case ParserState::GOLDTEST:
			decodeArcs(m_nRealEmpty);
			goldCheck(m_nRealEmpty);
			break;
		default:
			break;
		}
	}

	void DepParser::decode() {

		initArcScore();
		initBiSiblingArcScore();

		for (int d = 1; d <= m_nSentenceLength + 1; ++d) {

			for (int l = 0, max_l = m_nSentenceLength - d + 1; l < max_l; ++l) {


				int r = l + d - 1;
				StateItem (&items)[MAX_EMPTY_COUNT] = m_lItems[l][r];
				const tscore & l2r_arc_score = m_vecArcScore[l][r];
				const tscore & r2l_arc_score = m_vecArcScore[r][l];

				// initialize
				for (int nec = 0; nec <= m_nMaxEmpty; ++nec) items[nec].init(l, r);

				for (int s = l; s < r; ++s) {

					StateItem (&litems)[MAX_EMPTY_COUNT] = m_lItems[l][s];
					StateItem (&ritems)[MAX_EMPTY_COUNT] = m_lItems[s + 1][r];
					int lnec = 0;
					while (lnec <= m_nMaxEmpty) {
						int rnec = 0;
						StateItem & litem = litems[lnec];
						while (lnec + rnec <= m_nMaxEmpty) {

							StateItem & ritem = ritems[rnec];
							StateItem & item = items[lnec + rnec];

							// jux
							if (litem.states[L2R].split != -1 && ritem.states[R2L].split != -1) {
								item.updateStates(
										litem.states[L2R].score + ritem.states[R2L].score,
										s, lnec, JUX);
							}

							// l2r_empty_inside
							// split point would be encode as an empty point
							if (litem.states[L2R_EMPTY_OUTSIDE].split != -1 && ritem.states[R2L].split != -1) {
								tscore l_empty_inside_base_score = l2r_arc_score + ritem.states[R2L].score;
								for (int ec = 1; ec <= MAX_EMPTY_SIZE; ++ec) {
									item.updateStates(
											// bi-sibling arc score
											biSiblingArcScore(l, ENCODE_EMPTY(s + 1, ec), r) +
											// left part score
											litem.states[L2R_EMPTY_OUTSIDE + ec - 1].score +
											// arc score + right part score
											l_empty_inside_base_score,
											ENCODE_EMPTY(s, ec), lnec, L2R_EMPTY_INSIDE);
								}
							}

							// r2l_empty_inside
							// split point would be encode as an empty point
							if (litem.states[L2R].split != -1 && ritem.states[R2L_EMPTY_OUTSIDE].split != -1) {
								tscore r_empty_inside_base_score = r2l_arc_score + litem.states[L2R].score;
								for (int ec = 1; ec <= MAX_EMPTY_SIZE; ++ec) {
									item.updateStates(
											// bi-sibling arc score
											biSiblingArcScore(r, ENCODE_EMPTY(s + 1, ec), l) +
											// right part score
											ritem.states[R2L_EMPTY_OUTSIDE + ec - 1].score +
											// arc score + left part score
											r_empty_inside_base_score,
											ENCODE_EMPTY(s, ec), lnec, R2L_EMPTY_INSIDE);
								}
							}

							++rnec;
						}
						++lnec;
					}
				}

				for (int s = l + 1; s < r; ++s) {

					StateItem (&litems)[MAX_EMPTY_COUNT] = m_lItems[l][s];
					StateItem (&ritems)[MAX_EMPTY_COUNT] = m_lItems[s][r];
					int lnec = 0;

					tscore l2r_solid_arc_score = m_vecBiSiblingScore[l][r][s] + l2r_arc_score;
					tscore r2l_solid_arc_score = m_vecBiSiblingScore[r][l][s] + r2l_arc_score;

					while (lnec <= m_nMaxEmpty) {
						int rnec = 0;
						StateItem & litem = litems[lnec];
						while (lnec + rnec <= m_nMaxEmpty) {

							StateItem & ritem = ritems[rnec];
							StateItem & solidItem = items[lnec + rnec];

							if (litem.states[L2R_SOLID_OUTSIDE].split != -1) {
								if (ritem.states[JUX].split != -1) {
									// l2r_solid_both
									solidItem.updateStates(
											// left part + right part
											litem.states[L2R_SOLID_OUTSIDE].score + ritem.states[JUX].score +
											// bi-sibling arc score + arc score
											l2r_solid_arc_score,
											s, lnec, L2R_SOLID_BOTH);
								}
								if (ritem.states[L2R].split != -1) {
									// l2r
									solidItem.updateStates(
											// left part + right part
											litem.states[L2R_SOLID_OUTSIDE].score + ritem.states[L2R].score,
											s, lnec, L2R);
								}
							}

							if (ritem.states[R2L_SOLID_OUTSIDE].split != -1) {
								if (litem.states[JUX].split != -1) {
									// r2l_solid_both
									solidItem.updateStates(
											// left part + right part
											litem.states[JUX].score + ritem.states[R2L_SOLID_OUTSIDE].score +
											// bi-sibling arc score + arc score
											r2l_solid_arc_score,
											s, lnec, R2L_SOLID_BOTH);
								}
								if (litem.states[R2L].split != -1) {
									// r2l
									solidItem.updateStates(
											// left part + right part
											litem.states[R2L].score + ritem.states[R2L_SOLID_OUTSIDE].score,
											s, lnec, R2L);
								}
							}

							if (lnec + rnec < m_nMaxEmpty) {
								StateItem & emptyItem = items[lnec + rnec + 1];
								if (litem.states[L2R_SOLID_OUTSIDE].split != -1 && ritem.states[L2R].split != -1) {
									tscore base_score = litem.states[L2R_SOLID_OUTSIDE].score + ritem.states[L2R].score;
									for (int ec = 1; ec <= MAX_EMPTY_SIZE; ++ec) {
										// l2r_empty_outside
										emptyItem.updateStates(
												// bi-sibling arc score
												biSiblingArcScore(l, s, ENCODE_EMPTY(r + 1, ec)) +
												// arc score
												arcScore(l, ENCODE_EMPTY(r + 1, ec)) +
												// left part + right part
												base_score,
												s, lnec, L2R_EMPTY_OUTSIDE + ec - 1);
									}
								}

								if (litem.states[R2L].split != -1 && ritem.states[R2L_SOLID_OUTSIDE].split != -1) {
									tscore base_score = litem.states[R2L].score + ritem.states[R2L_SOLID_OUTSIDE].score;
									for (int ec = 1; ec <= MAX_EMPTY_SIZE; ++ec) {
										// r2l_empty_outside
										emptyItem.updateStates(
												// bi-sibling arc score
												biSiblingArcScore(r, s, ENCODE_EMPTY(l, ec)) +
												// arc score
												arcScore(r, ENCODE_EMPTY(l, ec)) +
												// left part + right part
												base_score,
												s, lnec, R2L_EMPTY_OUTSIDE + ec - 1);
									}
								}
							}
							++rnec;
						}
						++lnec;
					}
				}

				if (d > 1) {
					StateItem (&litems)[MAX_EMPTY_COUNT] = m_lItems[l][r - 1];
					StateItem (&ritems)[MAX_EMPTY_COUNT] = m_lItems[l + 1][r];

					int rnec = 0;
					tscore l2r_solid_both_base_score = m_vecBiSiblingScore[l][r][l] + l2r_arc_score;
					while (rnec <= m_nMaxEmpty && ritems[rnec].states[R2L].split != -1) {
						// l2r_solid_both
						items[rnec].updateStates(
								// right part score
								ritems[rnec].states[R2L].score +
								// bi-sibling arc score + arc score
								l2r_solid_both_base_score,
								// left part is a point, 0 ec
								l, 0, L2R_SOLID_BOTH);
						++rnec;
					}

					int lnec = 0;
					tscore r2l_solid_both_base_score = m_vecBiSiblingScore[r][l][r] + r2l_arc_score;
					while (lnec <= m_nMaxEmpty && litems[lnec].states[L2R].split != -1) {
						// r2l_solid_both
						items[lnec].updateStates(
								// left part score
								litems[lnec].states[L2R].score +
								// bi-sibling arc score + arc score
								r2l_solid_both_base_score,
								// left part is L2R, lnec ec
								r, lnec, R2L_SOLID_BOTH);
						++lnec;
					}

					lnec = 0;
					while (lnec <= m_nMaxEmpty && items[lnec].states[L2R_SOLID_BOTH].split != -1) {
						StateItem & item = items[lnec];
						// l2r_solid_outside
						item.updateStates(
								item.states[L2R_SOLID_BOTH].score,
								item.states[L2R_SOLID_BOTH].split,
								item.states[L2R_SOLID_BOTH].lecnum,
								L2R_SOLID_OUTSIDE);

						++lnec;
					}

					// inside start from 1
					lnec = 1;
					while (lnec <= m_nMaxEmpty && items[lnec].states[L2R_EMPTY_INSIDE].split != -1) {
						StateItem & item = items[lnec];
						// l2r_solid_outside
						item.updateStates(
								item.states[L2R_EMPTY_INSIDE].score,
								item.states[L2R_EMPTY_INSIDE].split,
								item.states[L2R_EMPTY_INSIDE].lecnum,
								L2R_SOLID_OUTSIDE);
						++lnec;
					}

					rnec = 0;
					while (rnec <= m_nMaxEmpty && items[rnec].states[R2L_SOLID_BOTH].split != -1) {
						StateItem & item = items[rnec];
						// r2l_solid_outside
						item.updateStates(
								item.states[R2L_SOLID_BOTH].score,
								item.states[R2L_SOLID_BOTH].split,
								item.states[R2L_SOLID_BOTH].lecnum,
								R2L_SOLID_OUTSIDE);
						++rnec;
					}
					// inside start from 1
					rnec = 1;
					while (rnec <= m_nMaxEmpty && items[rnec].states[R2L_EMPTY_INSIDE].split != -1) {
						StateItem & item = items[rnec];
						// r2l_solid_outside
						item.updateStates(
								item.states[R2L_EMPTY_INSIDE].score,
								item.states[R2L_EMPTY_INSIDE].split,
								item.states[R2L_EMPTY_INSIDE].lecnum,
								R2L_SOLID_OUTSIDE);
						++rnec;
					}

					rnec = 0;
					while (rnec <= 1) {
						lnec = 0;
						StateItem & ritem = m_lItems[r][r][rnec];
						while (lnec + rnec <= m_nMaxEmpty) {
							StateItem & litem = items[lnec];
							if (litem.states[L2R_SOLID_OUTSIDE].split != -1) {
								if (lnec + rnec < m_nMaxEmpty) {
									StateItem & emptyItem = items[lnec + rnec + 1];
									// l2r_empty_ouside
									for (int ec = 1; ec <= MAX_EMPTY_SIZE; ++ec) {
										emptyItem.updateStates(
												// left part + right part
												litem.states[L2R_SOLID_OUTSIDE].score + ritem.states[L2R].score +
												// bi-sibling arc score
												biSiblingArcScore(l, r, ENCODE_EMPTY(r + 1, ec)) +
												// arc score
												arcScore(l, ENCODE_EMPTY(r + 1, ec)),
												r, lnec, L2R_EMPTY_OUTSIDE + ec - 1);
									}
								}
								StateItem & item = items[lnec + rnec];
								// l2r
								item.updateStates(
										litem.states[L2R_SOLID_OUTSIDE].score + ritem.states[L2R].score,
										r, lnec, L2R);
							}
							++lnec;
						}
						++rnec;
					}
					lnec = 1;
					while (lnec <= m_nMaxEmpty) {
						StateItem & litem = items[lnec];
						StateItem & item = items[lnec];
						if (litem.states[L2R_EMPTY_OUTSIDE].split != -1) {
							// l2r
							for (int ec = 1; ec <= MAX_EMPTY_SIZE; ++ec) {
								item.updateStates(
										// empty outside
										litem.states[L2R_EMPTY_OUTSIDE + ec - 1].score,
										ENCODE_EMPTY(r, ec), lnec, L2R);
							}
						}

						++lnec;
					}

					lnec = 0;
					while (lnec <= 1) {
						rnec = 0;
						StateItem & litem = m_lItems[l][l][lnec];
						while (rnec + lnec < m_nMaxEmpty) {
							StateItem & ritem = items[rnec];
							if (ritem.states[R2L_SOLID_OUTSIDE].split != -1) {
								StateItem & emptyItem = items[rnec + lnec + 1];
								// r2l_empty_ouside
								for (int ec = 1; ec <= MAX_EMPTY_SIZE; ++ec) {
									emptyItem.updateStates(
											// left part + right part
											ritem.states[R2L_SOLID_OUTSIDE].score + litem.states[R2L].score +
											// bi-sibling arc score
											biSiblingArcScore(r, l, ENCODE_EMPTY(l, ec)) +
											// arc score
											arcScore(r, ENCODE_EMPTY(l, ec)),
											l, lnec, R2L_EMPTY_OUTSIDE + ec - 1);
								}
								StateItem & item = items[rnec + lnec];
								// r2l
								item.updateStates(
										ritem.states[R2L_SOLID_OUTSIDE].score + litem.states[R2L].score,
										l, lnec, R2L);
							}
							++rnec;
						}
						++lnec;
					}
					rnec = 1;
					while (rnec <= m_nMaxEmpty) {
						StateItem & ritem = items[rnec];
						StateItem & item = items[rnec];
						if (ritem.states[R2L_EMPTY_OUTSIDE].split != -1) {
							// r2l
							for (int ec = 1; ec <= MAX_EMPTY_SIZE; ++ec) {
								item.updateStates(
										// empty outside
										ritem.states[R2L_EMPTY_OUTSIDE + ec - 1].score,
										ENCODE_EMPTY(l, ec), 0, R2L);
							}
						}
						++rnec;
					}
				}
				else {
					for (int ec = 1; ec <= MAX_EMPTY_SIZE; ++ec) {
						// l2r_empty_ouside
						items[1].updateStates(
								biSiblingArcScore(l, -1, ENCODE_EMPTY(r + 1, ec)) +
								arcScore(l, ENCODE_EMPTY(r + 1, ec)),
								r, 1, L2R_EMPTY_OUTSIDE + ec - 1);
						// l2r with 1 empty node
						items[1].updateStates(
								items[1].states[L2R_EMPTY_OUTSIDE + ec - 1].score,
								ENCODE_EMPTY(l, ec), 1, L2R);
					}
					// l2r
					items[0].updateStates(0, r, 0, L2R);

					for (int ec = 1; ec <= MAX_EMPTY_SIZE; ++ec) {
						// r2l_empty_ouside
						items[1].updateStates(
								biSiblingArcScore(r, -1, ENCODE_EMPTY(l, ec)) +
								arcScore(r, ENCODE_EMPTY(l, ec)),
								l, 0, R2L_EMPTY_OUTSIDE + ec - 1);
						// r2l with 1 empty node
						items[1].updateStates(
								items[1].states[R2L_EMPTY_OUTSIDE + ec - 1].score,
								ENCODE_EMPTY(l, ec), 0, R2L);
					}
					// r2l
					items[0].updateStates(0, l, 0, R2L);
				}
			}

			if (d > 1) {
				int l = m_nSentenceLength - d + 1, r = m_nSentenceLength;
				// root
				StateItem (&l2ritems)[MAX_EMPTY_COUNT] = m_lItems[l][r - 1];
				StateItem (&items)[MAX_EMPTY_COUNT] = m_lItems[l][r];
				// initialize
				for (int nec = 0; nec <= m_nMaxEmpty; ++nec) items[nec].init(l, r);
				// r2l_solid_outside
				int lnec = 0, rnec = 0;
				while (lnec <= m_nMaxEmpty && l2ritems[lnec].states[L2R].split != -1) {
					items[lnec].updateStates(
							l2ritems[lnec].states[L2R].score +
							m_vecBiSiblingScore[r][l][r] +
							m_vecArcScore[r][l],
							r, lnec, R2L_SOLID_OUTSIDE);
					++lnec;
				}

				// r2l
				for (int s = l; s < r; ++s) {
					lnec = 0;
					StateItem (&litems)[MAX_EMPTY_COUNT] = m_lItems[l][s];
					StateItem (&ritems)[MAX_EMPTY_COUNT] = m_lItems[s][r];
					while (lnec <= m_nMaxEmpty && litems[lnec].states[R2L].split != -1) {
						rnec = 0;
						while (lnec + rnec <= m_nMaxEmpty && ritems[rnec].states[R2L_SOLID_OUTSIDE].split != -1) {
							items[lnec + rnec].updateStates(
									litems[lnec].states[R2L].score + ritems[rnec].states[R2L_SOLID_OUTSIDE].score,
									s, lnec, R2L);
							++rnec;
						}
						++lnec;
					}
				}
			}
		}
	}

	void DepParser::decodeArcs(int nec) {

		m_vecTrainArcs.clear();
		if (m_lItems[0][m_nSentenceLength][nec].states[R2L].split == -1) return;
		typedef std::tuple<int, int, int> sItem;
		std::stack<sItem> stack;
		stack.push(sItem(0, m_nSentenceLength, nec));
		m_lItems[0][m_nSentenceLength][nec].type = R2L;

		while (!stack.empty()) {
			sItem span = stack.top();
			stack.pop();
			StateItem & item = m_lItems[std::get<0>(span)][std::get<1>(span)][std::get<2>(span)];
			int split = item.states[item.type].split;
			int tnec = std::get<2>(span), lnec = item.states[item.type].lecnum;

//			std::cout << "total tecnum is " << tnec << std::endl;
//			item.print(); //debug

			switch (item.type) {
			case JUX:
				m_lItems[item.left][split][lnec].type = L2R;
				stack.push(sItem(item.left, split, lnec));
				m_lItems[split + 1][item.right][tnec - lnec].type = R2L;
				stack.push(sItem(split + 1, item.right, tnec - lnec));
				break;
			case L2R:
				if (IS_EMPTY(split)) {
					item.type = L2R_EMPTY_OUTSIDE + DECODE_EMPTY_TAG(split) - 1;
					stack.push(span);
					break;
				}
				if (item.left == item.right) {
					break;
				}

				m_lItems[item.left][split][lnec].type = L2R_SOLID_OUTSIDE;
				stack.push(sItem(item.left, split, lnec));
				m_lItems[split][item.right][tnec - lnec].type = L2R;
				stack.push(sItem(split, item.right, tnec - lnec));
				break;
			case R2L:
				if (IS_EMPTY(split)) {
					item.type = R2L_EMPTY_OUTSIDE + DECODE_EMPTY_TAG(split) - 1;
					stack.push(span);
					break;
				}
				if (item.left == item.right) {
					break;
				}

				m_lItems[split][item.right][tnec - lnec].type = R2L_SOLID_OUTSIDE;
				stack.push(sItem(split, item.right, tnec - lnec));
				m_lItems[item.left][split][lnec].type = R2L;
				stack.push(sItem(item.left, split, lnec));
				break;
			case L2R_SOLID_BOTH:
				if (item.left == item.right) {
					break;
				}
				m_vecTrainArcs.push_back(BiGram<int>(item.left, item.right));

				if (split == item.left) {
					m_lItems[item.left + 1][item.right][tnec - lnec].type = R2L;
					stack.push(sItem(item.left + 1, item.right, tnec - lnec));
				}
				else {
					m_lItems[item.left][split][lnec].type = L2R_SOLID_OUTSIDE;
					stack.push(sItem(item.left, split, lnec));
					m_lItems[split][item.right][tnec - lnec].type = JUX;
					stack.push(sItem(split, item.right, tnec - lnec));
				}
				break;
			case R2L_SOLID_BOTH:
				if (item.left == item.right) {
					break;
				}
				m_vecTrainArcs.push_back(BiGram<int>(item.right == m_nSentenceLength ? -1 : item.right, item.left));

				if (split == item.right) {
					m_lItems[item.left][item.right - 1][lnec].type = L2R;
					stack.push(sItem(item.left, item.right - 1, lnec));
				}
				else {
					m_lItems[split][item.right][tnec - lnec].type = R2L_SOLID_OUTSIDE;
					stack.push(sItem(split, item.right, tnec - lnec));
					m_lItems[item.left][split][lnec].type = JUX;
					stack.push(sItem(item.left, split, lnec));
				}
				break;
			case L2R_EMPTY_INSIDE:
				if (item.left == item.right) {
					break;
				}
				m_vecTrainArcs.push_back(BiGram<int>(item.left, item.right));

				m_lItems[item.left][DECODE_EMPTY_POS(split)][lnec].type = L2R_EMPTY_OUTSIDE + DECODE_EMPTY_TAG(split) - 1;
				stack.push(sItem(item.left, DECODE_EMPTY_POS(split), lnec));
				m_lItems[DECODE_EMPTY_POS(split) + 1][item.right][tnec - lnec].type = R2L;
				stack.push(sItem(DECODE_EMPTY_POS(split) + 1, item.right, tnec - lnec));
				break;
			case R2L_EMPTY_INSIDE:
				if (item.left == item.right) {
					break;
				}
				m_vecTrainArcs.push_back(BiGram<int>(item.right, item.left));

				m_lItems[DECODE_EMPTY_POS(split) + 1][item.right][tnec - lnec].type = R2L_EMPTY_OUTSIDE + DECODE_EMPTY_TAG(split) - 1;
				stack.push(sItem(DECODE_EMPTY_POS(split) + 1, item.right, tnec - lnec));
				m_lItems[item.left][DECODE_EMPTY_POS(split)][lnec].type = L2R;
				stack.push(sItem(item.left, DECODE_EMPTY_POS(split), lnec));
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
					m_lItems[item.left][item.right - 1][lnec].type = L2R;
					stack.push(sItem(item.left, item.right - 1, lnec));
					break;
				}

				item.type = IS_EMPTY(split) ? R2L_EMPTY_INSIDE : R2L_SOLID_BOTH;
				stack.push(span);
				break;
			case L2R_EMPTY_OUTSIDE + 0:
				m_vecTrainArcs.push_back(BiGram<int>(item.left, ENCODE_EMPTY(item.right + 1, item.type - L2R_EMPTY_OUTSIDE + 1)));

				if (item.left == item.right) {
					break;
				}

				m_lItems[item.left][split][lnec].type = L2R_SOLID_OUTSIDE;
				stack.push(sItem(item.left, split, lnec));
				m_lItems[split][item.right][tnec - lnec - 1].type = L2R;
				stack.push(sItem(split, item.right, tnec - lnec - 1));
				break;
			case R2L_EMPTY_OUTSIDE + 0:
				m_vecTrainArcs.push_back(BiGram<int>(item.right, ENCODE_EMPTY(item.left, item.type - R2L_EMPTY_OUTSIDE + 1)));

				if (item.left == item.right) {
					break;
				}

				m_lItems[split][item.right][tnec - lnec - 1].type = R2L_SOLID_OUTSIDE;
				stack.push(sItem(split, item.right, tnec - lnec - 1));
				m_lItems[item.left][split][lnec].type = R2L;
				stack.push(sItem(item.left, split, lnec));
				break;
			default:
				break;
			}
		}
	}

	void DepParser::update() {
		++m_nTrainingRound;
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
					std::cout << "generate an empty edge at " << m_nSentenceCount << std::endl;
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

	void DepParser::goldCheck(int nec) {
		if (m_nRealEmpty != nec) return;
		Arcs2BiArcs(m_vecTrainArcs, m_vecTrainBiArcs);
//		std::cout << "total empty node is " << nec << " total score is " <<  m_lItems[0][m_nSentenceLength][nec].states[R2L].score << std::endl; //debug
		if (m_vecCorrectArcs.size() != m_vecTrainArcs.size() || m_lItems[0][m_nSentenceLength][nec].states[R2L].score / GOLD_POS_SCORE != m_vecCorrectArcs.size() + m_vecCorrectBiArcs.size()) {
			std::cout << "gold parse len error at " << m_nTrainingRound << std::endl;
			std::cout << "score is " << m_lItems[0][m_nSentenceLength][nec].states[R2L].score << std::endl;
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

	void DepParser::initArcScore() {
		m_vecArcScore =
			decltype(m_vecArcScore)(
				m_nSentenceLength + 1,
				std::vector<tscore>(m_nSentenceLength));
		for (int d = 1; d <= m_nSentenceLength; ++d) {
			for (int i = 0, max_i = m_nSentenceLength - d + 1; i < max_i; ++i) {
				if (d > 1) {
					m_vecArcScore[i][i + d - 1] = arcScore(i, i + d - 1);
					m_vecArcScore[i + d - 1][i] = arcScore(i + d - 1, i);
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
		int idx = 0;
		for (const auto & node : tcorrect) {
			if (TREENODE_POSTAG(node) == EMPTYTAG) {
				++m_nRealEmpty;
				m_vecCorrectArcs.push_back(Arc(TREENODE_HEAD(node), ENCODE_EMPTY(idx, TEmptyTag::code(TREENODE_WORD(node)))));
			}
			else {
				m_vecCorrectArcs.push_back(Arc(TREENODE_HEAD(node), idx++));
			}
		}
	}

	void DepParser::getOrUpdateSiblingScore(const int & p, const int & c, const int & amount) {
		m_pWeight->getOrUpdateArcScore(m_nRetval, p, c, amount, m_nSentenceLength, m_lSentence);
	}

	void DepParser::getOrUpdateSiblingScore(const int & p, const int & c, const int & c2, const int & amount) {
		m_pWeight->getOrUpdateBiArcScore(m_nRetval, p, c, c2, amount, m_nSentenceLength, m_lSentence);
	}
}
