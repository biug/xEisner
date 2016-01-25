#include "emptyeisner2nd_state.h"

namespace emptyeisner2nd {

	std::string TYPE_NAME[] = {
		"JUX", "L2R", "R2L", "L2R_SOLID_BOTH", "R2L_SOLID_BOTH",
		"L2R_EMPTY_INSIDE", "R2L_EMPTY_INSIDE", "L2R_SOLID_OUTSIDE", "R2L_SOLID_OUTSIDE",
		"L2R_EMPTY_OUTSIDE *PRO*", "L2R_EMPTY_OUTSIDE *OP*", "L2R_EMPTY_OUTSIDE *T*", "L2R_EMPTY_OUTSIDE *pro*",
		"L2R_EMPTY_OUTSIDE *RNR*", "L2R_EMPTY_OUTSIDE *OP*|*T*", "L2R_EMPTY_OUTSIDE *OP*|*pro*", "L2R_EMPTY_OUTSIDE *pro*|*T*",
		"L2R_EMPTY_OUTSIDE *OP*|*pro*|*T*", "L2R_EMPTY_OUTSIDE *RNR*|*RNR*", "L2R_EMPTY_OUTSIDE *", "L2R_EMPTY_OUTSIDE *PRO*|*T*",
		"L2R_EMPTY_OUTSIDE *OP*|*PRO*|*T*", "L2R_EMPTY_OUTSIDE *T*|*pro*", "L2R_EMPTY_OUTSIDE *T*|*", "L2R_EMPTY_OUTSIDE *pro*|*PRO*", "L2R_EMPTY_OUTSIDE *|*T*",
		"R2L_EMPTY_OUTSIDE *PRO*", "R2L_EMPTY_OUTSIDE *OP*", "R2L_EMPTY_OUTSIDE *T*", "R2L_EMPTY_OUTSIDE *pro*",
		"R2L_EMPTY_OUTSIDE *RNR*", "R2L_EMPTY_OUTSIDE *OP*|*T*", "R2L_EMPTY_OUTSIDE *OP*|*pro*", "R2L_EMPTY_OUTSIDE *pro*|*T*",
		"R2L_EMPTY_OUTSIDE *OP*|*pro*|*T*", "R2L_EMPTY_OUTSIDE *RNR*|*RNR*", "R2L_EMPTY_OUTSIDE *", "R2L_EMPTY_OUTSIDE *PRO*|*T*",
		"R2L_EMPTY_OUTSIDE *OP*|*PRO*|*T*", "R2L_EMPTY_OUTSIDE *T*|*pro*", "R2L_EMPTY_OUTSIDE *T*|*", "R2L_EMPTY_OUTSIDE *pro*|*PRO*", "R2L_EMPTY_OUTSIDE *|*T*",
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
			for (type = 0; type < 10; ++type) print();
			type = 26; print();
			type = -1;
		}
	}
}
