#include "emptyeisner2nd_state.h"

namespace emptyeisner2nd {
	StateItem::StateItem() : type(-1), left(-1), right(-1), ecnum(-1) {};
	StateItem::~StateItem() = default;
	StateItem::StateItem(const StateItem & item) = default;

	void StateItem::print() {
		std::cout << "[" << left << "," << right << "]" << std::endl;
		std::cout << "type is: ";
		if (type >= 0) {
			std::cout << TYPE_NAME[type] << std::endl;
			std::cout << "split: " << states[type].split << " score: " << states[type].score << std::endl;
		}
		else {
			for (type = 0; type < 43; ++type) print();
			type = -1;
		}
	}
}
