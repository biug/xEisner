#include "emptyeisnergc3rd_state.h"

namespace emptyeisnergc3rd {
	StateItem::StateItem() = default;
	StateItem::~StateItem() = default;
	StateItem::StateItem(const StateItem & item) = default;

	void StateItem::init(const int & l, const int & r, const int & len) {
		type = 0;
		left = l;
		right = r;
		int num = len + l - r;
		jux.reserve(num);
		l2r_solid_both.reserve(num);
		r2l_solid_both.reserve(num);
		l2r_empty_outside.reserve(num);
		r2l_empty_outside.reserve(num);
		l2r_solid_outside.reserve(num);
		r2l_solid_outside.reserve(num);
		l2r_empty_inside.reserve(num);
		r2l_empty_inside.reserve(num);
		l2r.reserve(num);
		r2l.reserve(num);
		while (num--) {
			jux.push_back(ScoreWithSplit());
			l2r_solid_both.push_back(ScoreAgenda());
			r2l_solid_both.push_back(ScoreAgenda());
			l2r_empty_outside.push_back(ScoreAgenda());
			r2l_empty_outside.push_back(ScoreAgenda());
			l2r_solid_outside.push_back(ScoreAgenda());
			r2l_solid_outside.push_back(ScoreAgenda());
			l2r_empty_inside.push_back(ScoreAgenda());
			r2l_empty_inside.push_back(ScoreAgenda());
			l2r.push_back(ScoreWithSplit());
			r2l.push_back(ScoreWithSplit());
		}
	}

	void StateItem::print(const int & grand) {
		std::cout << "[" << left << "," << right << "]" << std::endl;
		std::cout << "type is: ";
		switch (type) {
		case JUX:
			std::cout << "JUX" << std::endl;
			std::cout << "split: " << jux[grand].getSplit() << " score: " << jux[grand].getScore() << std::endl;
			break;
		case L2R:
			std::cout << "L2R_COMP" << std::endl;
			std::cout << "split: " << l2r[grand].getSplit() << " score: " << l2r[grand].getScore() << std::endl;
			break;
		case R2L:
			std::cout << "R2L_COMP" << std::endl;
			std::cout << "split: " << r2l[grand].getSplit() << " score: " << r2l[grand].getScore() << std::endl;
			break;
		case L2R_SOLID_BOTH:
			std::cout << "L2R_IM_COMP" << std::endl;
			for (const auto agenda : l2r_solid_both[grand]) {
				std::cout << "inner split: " << agenda->getInnerSplit() << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			break;
		case R2L_SOLID_BOTH:
			std::cout << "R2L_IM_COMP" << std::endl;
			for (const auto agenda : r2l_solid_both[grand]) {
				std::cout << "inner split: " << agenda->getInnerSplit() << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			break;
		case L2R_SOLID_OUTSIDE:
			std::cout << "L2R_SOLID_OUTSIDE" << std::endl;
			for (const auto & agenda : l2r_solid_outside[grand]) {
				std::cout << "inner split: " << agenda->getInnerSplit() << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			break;
		case R2L_SOLID_OUTSIDE:
			std::cout << "R2L_SOLID_OUTSIDE" << std::endl;
			for (const auto & agenda : r2l_solid_outside[grand]) {
				std::cout << "inner split: " << agenda->getInnerSplit() << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			break;
		case L2R_EMPTY_INSIDE:
			std::cout << "L2R_EMPTY_INSIDE" << std::endl;
			for (const auto & agenda : l2r_empty_inside[grand]) {
				std::cout << "inner split: " << agenda->getInnerSplit() << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			break;
		case R2L_EMPTY_INSIDE:
			std::cout << "R2L_EMPTY_INSIDE" << std::endl;
			for (const auto & agenda : r2l_empty_inside[grand]) {
				std::cout << "inner split: " << agenda->getInnerSplit() << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			break;
		case L2R_EMPTY_OUTSIDE:
			std::cout << "L2R_EMPTY_OUTSIDE" << std::endl;
			for (const auto & agenda : l2r_empty_outside[grand]) {
				std::cout << "inner split: " << agenda->getInnerSplit() << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			break;
		case R2L_EMPTY_OUTSIDE:
			std::cout << "R2L_EMPTY_OUTSIDE" << std::endl;
			for (const auto & agenda : r2l_empty_outside[grand]) {
				std::cout << "inner split: " << agenda->getInnerSplit() << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			break;
		default:
			std::cout << "ZERO" << std::endl;
			std::cout << "JUX" << std::endl;
			std::cout << "split: " << jux[grand].getSplit() << " score: " << jux[grand].getScore() << std::endl;
			std::cout << "L2R_COMP" << std::endl;
			std::cout << "split: " << l2r[grand].getSplit() << " score: " << l2r[grand].getScore() << std::endl;
			std::cout << "R2L_COMP" << std::endl;
			std::cout << "split: " << r2l[grand].getSplit() << " score: " << r2l[grand].getScore() << std::endl;
			std::cout << "L2R_IM_COMP" << std::endl;
			for (const auto agenda : l2r_solid_both[grand]) {
				std::cout << "inner split: " << agenda->getInnerSplit() << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			std::cout << "R2L_IM_COMP" << std::endl;
			for (const auto agenda : r2l_solid_both[grand]) {
				std::cout << "inner split: " << agenda->getInnerSplit() << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			std::cout << "L2R_SOLID_OUTSIDE" << std::endl;
			for (const auto & agenda : l2r_solid_outside[grand]) {
				std::cout << "inner split: " << agenda->getInnerSplit() << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			std::cout << "R2L_SOLID_OUTSIDE" << std::endl;
			for (const auto & agenda : r2l_solid_outside[grand]) {
				std::cout << "inner split: " << agenda->getInnerSplit() << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			std::cout << "L2R_EMPTY_INSIDE" << std::endl;
			for (const auto & agenda : l2r_empty_inside[grand]) {
				std::cout << "inner split: " << agenda->getInnerSplit() << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			std::cout << "R2L_EMPTY_INSIDE" << std::endl;
			for (const auto & agenda : r2l_empty_inside[grand]) {
				std::cout << "inner split: " << agenda->getInnerSplit() << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			std::cout << "L2R_EMPTY_OUTSIDE" << std::endl;
			for (const auto & agenda : l2r_empty_outside[grand]) {
				std::cout << "inner split: " << agenda->getInnerSplit() << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			std::cout << "R2L_EMPTY_OUTSIDE" << std::endl;
			for (const auto & agenda : r2l_empty_outside[grand]) {
				std::cout << "inner split: " << agenda->getInnerSplit() << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			break;
		}
	}
}
