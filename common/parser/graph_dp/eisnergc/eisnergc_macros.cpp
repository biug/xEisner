#include <stack>
#include <algorithm>
#include <unordered_set>

#include "eisnergc_macros.h"

namespace eisnergc {

	bool operator<(const Arc & arc1, const Arc & arc2) {
		if (arc1.first() != arc2.first()) {
			return arc1.first() < arc2.first();
		}
		return arc1.second() < arc2.second();
	}

	void Arcs2BiArcs(std::vector<Arc> & arcs, std::vector<BiArc> & biarcs) {
		for (auto & arc : arcs) {
			if (arc.first() == -1) {
				arc.refer(arcs.size(), arc.second());
				break;
			}
		}
		biarcs.clear();
		std::sort(arcs.begin(), arcs.end(), [](const Arc & arc1, const Arc & arc2) { return arc1 < arc2; });
		for (const auto & arc : arcs) {
			int grand = -1;
			for (const auto & garc : arcs) {
				if (garc.second() == arc.first()) {
					grand = garc.first();
					break;
				}
			}
			biarcs.push_back(BiArc(grand, arc.first(), arc.second()));
		}
		for (auto & arc : arcs) {
			if (arc.first() == arcs.size()) {
				arc.refer(-1, arc.second());
				break;
			}
		}
	}
}