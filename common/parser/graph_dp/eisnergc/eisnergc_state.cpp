#include "eisnergc_state.h"

namespace eisnergc {
	StateItem::StateItem() : type(0), left(0), right(0) {};
	StateItem::~StateItem() = default;

	void StateItem::init(const int & l, const int & r) {
		type = 0;
		left = l;
		right = r;
		l2r.clear();
		r2l.clear();
		l2r_im.clear();
		r2l_im.clear();
	}

	void StateItem::print() {
		std::cout << "[" << left << "," << right << "]" << std::endl;
		std::cout << "type is: ";
		switch (type) {
		case L2R_COMP:
			std::cout << "L2R_COMP" << std::endl;
			for (const auto & agenda : l2r) {
				std::cout << "grand: " << agenda.first << " split: " << agenda.second.getSplit() << " score: " << agenda.second.getScore() << std::endl;
			}
			break;
		case R2L_COMP:
			std::cout << "R2L_COMP" << std::endl;
			for (const auto & agenda : r2l) {
				std::cout << "grand: " << agenda.first << " split: " << agenda.second.getSplit() << " score: " << agenda.second.getScore() << std::endl;
			}
			break;
		case L2R_IM_COMP:
			std::cout << "L2R_IM_COMP" << std::endl;
			for (const auto & agenda : l2r_im) {
				std::cout << "grand: " << agenda.first << " split: " << agenda.second.getSplit() << " score: " << agenda.second.getScore() << std::endl;
			}
			break;
		case R2L_IM_COMP:
			std::cout << "R2L_IM_COMP" << std::endl;
			for (const auto & agenda : r2l_im) {
				std::cout << "grand: " << agenda.first << " split: " << agenda.second.getSplit() << " score: " << agenda.second.getScore() << std::endl;
			}
			break;
		default:
			std::cout << "ZERO" << std::endl;
			std::cout << "L2R_COMP" << std::endl;
			for (const auto & agenda : l2r) {
				std::cout << "grand: " << agenda.first << " split: " << agenda.second.getSplit() << " score: " << agenda.second.getScore() << std::endl;
			}
			std::cout << "R2L_COMP" << std::endl;
			for (const auto & agenda : r2l) {
				std::cout << "grand: " << agenda.first << " split: " << agenda.second.getSplit() << " score: " << agenda.second.getScore() << std::endl;
			}
			std::cout << "L2R_IM_COMP" << std::endl;
			for (const auto & agenda : l2r_im) {
				std::cout << "grand: " << agenda.first << " split: " << agenda.second.getSplit() << " score: " << agenda.second.getScore() << std::endl;
			}
			std::cout << "R2L_IM_COMP" << std::endl;
			for (const auto & agenda : r2l_im) {
				std::cout << "grand: " << agenda.first << " split: " << agenda.second.getSplit() << " score: " << agenda.second.getScore() << std::endl;
			}
			break;
		}
	}

	void StateItem::printGrands() {
		std::cout << "span is [" << left << " , " << right << "]" << std::endl;
		std::cout << "left grands" << std::endl;
		for (const auto & g : l2r) {
			std::cout << g.first << " ";
		}
		std::cout << std::endl;
		std::cout << "right grands" << std::endl;
		for (const auto & g : r2l) {
			std::cout << g.first << " ";
		}
		std::cout << std::endl;
	}
}
