#include "eisner_state.h"

namespace eisner {
	StateItem::StateItem() = default;
	StateItem::~StateItem() = default;

	void StateItem::init(const int & l, const int & r) {
		type = 0;
		left = l;
		right = r;
		l2r.reset();
		r2l.reset();
		l2r_im.reset();
		r2l_im.reset();
	}

	void StateItem::print() {
		std::cout << "[" << left << "," << right << "]" << std::endl;
		std::cout << "type is: ";
		switch (type) {
		case L2R_COMP:
			std::cout << "L2R_COMP" << std::endl;
			std::cout << "split: " << l2r.getSplit() << " score: " << l2r.getScore() << std::endl;
			break;
		case R2L_COMP:
			std::cout << "R2L_COMP" << std::endl;
			std::cout << "split: " << r2l.getSplit() << " score: " << r2l.getScore() << std::endl;
			break;
		case L2R_IM_COMP:
			std::cout << "L2R_IM_COMP" << std::endl;
			std::cout << "split: " << l2r.getSplit() << " score: " << l2r.getScore() << std::endl;
			break;
		case R2L_IM_COMP:
			std::cout << "R2L_IM_COMP" << std::endl;
			std::cout << "split: " << r2l.getSplit() << " score: " << r2l.getScore() << std::endl;
			break;
		default:
			std::cout << "ZERO" << std::endl;
			std::cout << "L2R_COMP" << std::endl;
			std::cout << "split: " << l2r.getSplit() << " score: " << l2r.getScore() << std::endl;
			std::cout << "R2L_COMP" << std::endl;
			std::cout << "split: " << r2l.getSplit() << " score: " << r2l.getScore() << std::endl;
			std::cout << "L2R_IM_COMP" << std::endl;
			std::cout << "split: " << l2r.getSplit() << " score: " << l2r.getScore() << std::endl;
			std::cout << "R2L_IM_COMP" << std::endl;
			std::cout << "split: " << r2l.getSplit() << " score: " << r2l.getScore() << std::endl;
			break;
		}
	}
}