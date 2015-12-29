#include <stack>
#include <unordered_set>

#include "eisner_macros.h"

namespace eisner {

	bool operator<(const Arc & arc1, const Arc & arc2) {
		if (arc1.first() != arc2.first()) {
			return arc1.first() < arc2.first();
		}
		return arc1.second() < arc2.second();
	}
}