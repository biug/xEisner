#include <stack>
#include <algorithm>
#include <unordered_set>

#include "emptyeisnergc2nd_macros.h"

namespace emptyeisnergc2nd {

	bool operator<(const Arc & arc1, const Arc & arc2) {
		if (arc1.first() != arc2.first()) {
			return arc1.first() < arc2.first();
		}
		if (IS_EMPTY(arc1.second())) {
			if (IS_EMPTY(arc2.second())) {
				return LESS_EMPTY_EMPTY(arc1.second(), arc2.second());
			}
			else {
				return LESS_EMPTY_SOLID(arc1.second(), arc2.second());
			}
		}
		else {
			if (IS_EMPTY(arc2.second())) {
				return LESS_SOLID_EMPTY(arc1.second(), arc2.second());
			}
			else {
				return LESS_SOLID_SOLID(arc1.second(), arc2.second());
			}
		}
	}

	bool compareArc(const Arc & arc1, const Arc & arc2) {
		if (DECODE_EMPTY_POS(arc1.second()) != DECODE_EMPTY_POS(arc2.second())) {
			return DECODE_EMPTY_POS(arc1.second()) < DECODE_EMPTY_POS(arc2.second());
		}
		if (ARC_LEFT(arc1.first(), arc1.second())) {
			if (ARC_RIGHT(arc2.first(), arc2.second())) {
				return false;
			}
			else {
				return arc1.first() > arc2.first();
			}
		}
		else {
			if (ARC_LEFT(arc2.first(), arc2.second())) {
				return true;
			}
			else {
				return arc1.first() > arc2.first();
			}
		}
	}

	bool operator<(const ScoreWithType & swt1, const ScoreWithType & swt2) {
		return swt1.getScore() < swt2.getScore();
	}

	void Arcs2TriArcs(std::vector<Arc> & arcs, std::vector<TriArc> & triarcs) {
		int maxid = -1;
		for (auto & arc : arcs) {
			if (!IS_EMPTY(arc.second()) && maxid < arc.second()) {
				maxid = arc.second();
			}
		}
		for (auto & arc : arcs) {
			if (arc.first() == -1) {
				arc.refer(maxid + 1, arc.second());
				break;
			}
		}
		triarcs.clear();
		std::sort(arcs.begin(), arcs.end(), [](const Arc & arc1, const Arc & arc2) { return arc1 < arc2; });
		auto itr_s = arcs.begin(), itr_e = arcs.begin();
		while (itr_s != arcs.end()) {
			while (itr_e != arcs.end() && itr_e->first() == itr_s->first()) {
				++itr_e;
			}
			int head = itr_s->first();
			int grand = -1;
			for (const auto & arc : arcs) {
				if (arc.second() == head) {
					grand = arc.first();
					break;
				}
			}
			if (itr_e - itr_s > 1) {
				for (auto itr = itr_s; itr != itr_e; ++itr) {
					if (ARC_LEFT(head, itr->second())) {
						if (itr == itr_e - 1 || ARC_RIGHT(head, (itr + 1)->second())) {
							triarcs.push_back(TriArc(grand, head, -1, itr->second()));
						}
						else {
							if (IS_EMPTY((itr + 1)->second())) {
								triarcs.push_back(TriArc(grand, head, itr == itr_e - 2 || ARC_RIGHT(head, (itr + 2)->second()) ? -1 : (itr + 2)->second(), itr->second()));
							}
							triarcs.push_back(TriArc(grand, head, (itr + 1)->second(), itr->second()));
						}
					}
					else {
						if (itr == itr_s || ARC_LEFT(head, (itr - 1)->second())) {
							triarcs.push_back(TriArc(grand, head, -1, itr->second()));
						}
						else {
							if (IS_EMPTY((itr - 1)->second())) {
								triarcs.push_back(TriArc(grand, head, itr == itr_s + 1 || ARC_LEFT(head, (itr - 2)->second()) ? -1 : (itr - 2)->second(), itr->second()));
							}
							triarcs.push_back(TriArc(grand, head, (itr - 1)->second(), itr->second()));
						}
					}
				}
			}
			else {
				triarcs.push_back(TriArc(grand, head, -1, itr_s->second()));
			}
			itr_s = itr_e;
		}
		for (auto & arc : arcs) {
			if (arc.first() == arcs.size()) {
				arc.refer(-1, arc.second());
				break;
			}
		}
	}
}
