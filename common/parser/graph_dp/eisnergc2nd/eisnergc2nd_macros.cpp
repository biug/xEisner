#include <stack>
#include <algorithm>
#include <unordered_set>

#include "eisnergc2nd_macros.h"

namespace eisnergc2nd {

	bool operator<(const Arc & arc1, const Arc & arc2) {
		if (arc1.first() != arc2.first()) {
			return arc1.first() < arc2.first();
		}
		return arc1.second() < arc2.second();
	}

	void Arcs2TriArcs(std::vector<Arc> & arcs, std::vector<TriArc> & triarcs) {
		for (auto & arc : arcs) {
			if (arc.first() == -1) {
				arc.refer(arcs.size(), arc.second());
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
					if (head > itr->second()) {
						if (itr == itr_e - 1 || head < (itr + 1)->second()) {
							triarcs.push_back(TriArc(grand, head, -1, itr->second()));
						}
						else {
							triarcs.push_back(TriArc(grand, head, (itr + 1)->second(), itr->second()));
						}
					}
					else {
						if (itr == itr_s || head > (itr - 1)->second()) {
							triarcs.push_back(TriArc(grand, head, -1, itr->second()));
						}
						else {
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