#include "emptyeisner2nd_state.h"

namespace emptyeisner2nd {
	StateItem::StateItem() = default;
	StateItem::~StateItem() = default;
	StateItem::StateItem(const StateItem & item) = default;

	void StateItem::init(const int & l, const int & r) {
		type = 0;
		left = l;
		right = r;
		jux.reset();
		l2r.reset();
		r2l.reset();
		l2r_solid_both.reset();
		r2l_solid_both.reset();
		l2r_solid_outside.reset();
		r2l_solid_outside.reset();
		l2r_empty_inside.reset();
		r2l_empty_inside.reset();
		l2r_empty_outside.clear();
		r2l_empty_outside.clear();
	}

	void StateItem::print(const int & grand) {
		std::cout << "[" << left << "," << right << "]" << std::endl;
		std::cout << "type is: ";
		switch (type) {
		case JUX:
			std::cout << "JUX" << std::endl;
			std::cout << "split: " << jux.getSplit() << " score: " << jux.getScore() << std::endl;
			break;
		case L2R:
			std::cout << "L2R_COMP" << std::endl;
			std::cout << "split: " << l2r.getSplit() << " score: " << l2r.getScore() << std::endl;
			break;
		case R2L:
			std::cout << "R2L_COMP" << std::endl;
			std::cout << "split: " << r2l.getSplit() << " score: " << r2l.getScore() << std::endl;
			break;
		case L2R_SOLID_BOTH:
			std::cout << "L2R_IM_COMP" << std::endl;
			std::cout << " split: " << l2r_solid_both.getSplit() << " score: " << l2r_solid_both.getScore() << std::endl;
			break;
		case R2L_SOLID_BOTH:
			std::cout << "R2L_IM_COMP" << std::endl;
			std::cout << " split: " << r2l_solid_both.getSplit() << " score: " << r2l_solid_both.getScore() << std::endl;
			break;
		case L2R_SOLID_OUTSIDE:
			std::cout << "L2R_SOLID_OUTSIDE" << std::endl;
			std::cout << " split: " << l2r_solid_outside.getSplit() << " score: " << l2r_solid_outside.getScore() << std::endl;
			break;
		case R2L_SOLID_OUTSIDE:
			std::cout << "R2L_SOLID_OUTSIDE" << std::endl;
			std::cout << " split: " << r2l_solid_outside.getSplit() << " score: " << r2l_solid_outside.getScore() << std::endl;
			break;
		case L2R_EMPTY_INSIDE:
			std::cout << "L2R_EMPTY_INSIDE" << std::endl;
			std::cout << "inner split: " << l2r_empty_inside.getInnerSplit() << " split: " << l2r_empty_inside.getSplit() << " score: " << l2r_empty_inside.getScore() << std::endl;
			break;
		case R2L_EMPTY_INSIDE:
			std::cout << "R2L_EMPTY_INSIDE" << std::endl;
			std::cout << "inner split: " << r2l_empty_inside.getInnerSplit() << " split: " << r2l_empty_inside.getSplit() << " score: " << r2l_empty_inside.getScore() << std::endl;
			break;
		case L2R_EMPTY_OUTSIDE:
			std::cout << "L2R_EMPTY_OUTSIDE" << std::endl;
			for (const auto & agenda : l2r_empty_outside) {
				std::cout << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			break;
		case R2L_EMPTY_OUTSIDE:
			std::cout << "R2L_EMPTY_OUTSIDE" << std::endl;
			for (const auto & agenda : r2l_empty_outside) {
				std::cout << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			break;
		default:
			std::cout << "ZERO" << std::endl;
			std::cout << "JUX" << std::endl;
			std::cout << "split: " << jux.getSplit() << " score: " << jux.getScore() << std::endl;
			std::cout << "L2R_COMP" << std::endl;
			std::cout << "split: " << l2r.getSplit() << " score: " << l2r.getScore() << std::endl;
			std::cout << "R2L_COMP" << std::endl;
			std::cout << "split: " << r2l.getSplit() << " score: " << r2l.getScore() << std::endl;
			std::cout << "L2R_IM_COMP" << std::endl;
			std::cout << " split: " << l2r_solid_both.getSplit() << " score: " << l2r_solid_both.getScore() << std::endl;
			std::cout << "R2L_IM_COMP" << std::endl;
			std::cout << " split: " << r2l_solid_both.getSplit() << " score: " << r2l_solid_both.getScore() << std::endl;
			std::cout << "L2R_SOLID_OUTSIDE" << std::endl;
			std::cout << " split: " << l2r_solid_outside.getSplit() << " score: " << l2r_solid_outside.getScore() << std::endl;
			std::cout << "R2L_SOLID_OUTSIDE" << std::endl;
			std::cout << " split: " << r2l_solid_outside.getSplit() << " score: " << r2l_solid_outside.getScore() << std::endl;
			std::cout << "L2R_EMPTY_INSIDE" << std::endl;
			std::cout << "inner split: " << l2r_empty_inside.getInnerSplit() << " split: " << l2r_empty_inside.getSplit() << " score: " << l2r_empty_inside.getScore() << std::endl;
			std::cout << "R2L_EMPTY_INSIDE" << std::endl;
			std::cout << "inner split: " << r2l_empty_inside.getInnerSplit() << " split: " << r2l_empty_inside.getSplit() << " score: " << r2l_empty_inside.getScore() << std::endl;
			std::cout << "L2R_EMPTY_OUTSIDE" << std::endl;
			for (const auto & agenda : l2r_empty_outside) {
				std::cout << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			std::cout << "R2L_EMPTY_OUTSIDE" << std::endl;
			for (const auto & agenda : r2l_empty_outside) {
				std::cout << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			break;
		}
	}
}
