#include <stack>
#include <algorithm>
#include <unordered_set>

#include "eisnergc3rd_macros.h"

namespace eisnergc3rd {

	bool operator<(const Arc & arc1, const Arc & arc2) {
		if (arc1.first() != arc2.first()) {
			return arc1.first() < arc2.first();
		}
		return arc1.second() < arc2.second();
	}

	int findInnerSplit(const ScoreAgenda & agendaBeam, const int & split) {
		int is = -1;
		for (const auto & agenda : agendaBeam) {
			if (agenda->getSplit() == split) {
				is = agenda->getInnerSplit();
				break;
			}
		}
		if (is == -1) {
			std::cout << "bad eisner at " << __LINE__ << std::endl;
		}
		return is;
	}

	void Arcs2QuarArcs(std::vector<Arc> & arcs, std::vector<QuarArc> & quararcs) {
		for (auto & arc : arcs) {
			if (arc.first() == -1) {
				arc.refer(arcs.size(), arc.second());
				break;
			}
		}
		quararcs.clear();
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
							quararcs.push_back(QuarArc(grand, head, -1, -1, itr->second()));
						}
						else if (itr == itr_e - 2 || head < (itr + 2)->second()) {
							quararcs.push_back(QuarArc(grand, head, -1, (itr + 1)->second(), itr->second()));
						}
						else {
							quararcs.push_back(QuarArc(grand, head, (itr + 2)->second(), (itr + 1)->second(), itr->second()));
						}
					}
					else {
						if (itr == itr_s || head >(itr - 1)->second()) {
							quararcs.push_back(QuarArc(grand, head, -1, -1, itr->second()));
						}
						else if (itr == itr_s + 1 || head > (itr - 2)->second()) {
							quararcs.push_back(QuarArc(grand, head, -1, (itr - 1)->second(), itr->second()));
						}
						else {
							quararcs.push_back(QuarArc(grand, head, (itr - 2)->second(), (itr - 1)->second(), itr->second()));
						}
					}
				}
			}
			else {
				quararcs.push_back(QuarArc(grand, head, -1, -1, itr_s->second()));
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