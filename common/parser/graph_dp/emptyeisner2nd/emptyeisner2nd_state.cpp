#include "emptyeisner2nd_state.h"

namespace emptyeisner2nd {

	std::string TYPE_NAME[] = {
		"JUX", "L2R", "R2L", "L2R_SOLID_BOTH", "R2L_SOLID_BOTH",
		"L2R_EMPTY_INSIDE", "R2L_EMPTY_INSIDE", "L2R_SOLID_OUTSIDE", "R2L_SOLID_OUTSIDE",
		"L2R_EMPTY_OUTSIDE *PRO*", "R2L_EMPTY_OUTSIDE *PRO*",
	};

	StateItem::StateItem() : type(-1), left(-1), right(-1) {};
	StateItem::~StateItem() = default;
	StateItem::StateItem(const StateItem & item) = default;

	void StateItem::print() {
		std::cout << "[" << left << "," << right << "]" << std::endl;
		std::cout << "type is: ";
		if (type >= 0) {
			std::cout << TYPE_NAME[type] << std::endl;
			std::cout << "left lecnum is " << states[type].lecnum << std::endl;
			std::cout << "split: " << states[type].split << " score: " << states[type].score << std::endl;
		}
		else {
			for (type = 0; type < 11; ++type) print();
			type = -1;
		}
	}
}
