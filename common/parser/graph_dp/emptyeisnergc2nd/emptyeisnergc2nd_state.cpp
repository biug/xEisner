#include "emptyeisnergc2nd_state.h"

namespace emptyeisnergc2nd {
	StateItem::StateItem() = default;
	StateItem::~StateItem() = default;
	StateItem::StateItem(const StateItem & item) = default;

	void StateItem::init(const int & l, const int & r, const int & len) {
		type = 0;
		left = l;
		right = r;
		int num = len + l - r;
		jux = std::vector<ScoreWithSplit>(num, ScoreWithSplit());
		l2r_solid_both = std::vector<ScoreWithSplit>(num, ScoreWithSplit());
		r2l_solid_both = std::vector<ScoreWithSplit>(num, ScoreWithSplit());
		l2r_empty_outside = std::vector<ScoreAgenda>(num, ScoreAgenda());
		r2l_empty_outside = std::vector<ScoreAgenda>(num, ScoreAgenda());
		l2r_solid_outside = std::vector<ScoreWithSplit>(num, ScoreWithSplit());
		r2l_solid_outside = std::vector<ScoreWithSplit>(num, ScoreWithSplit());
		l2r_empty_inside = std::vector<ScoreWithBiSplit>(num, ScoreWithBiSplit());
		r2l_empty_inside = std::vector<ScoreWithBiSplit>(num, ScoreWithBiSplit());
		l2r = std::vector<ScoreWithSplit>(num, ScoreWithSplit());
		r2l = std::vector<ScoreWithSplit>(num, ScoreWithSplit());
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
			std::cout << " split: " << l2r_solid_both[grand].getSplit() << " score: " << l2r_solid_both[grand].getScore() << std::endl;

			break;
		case R2L_SOLID_BOTH:
			std::cout << "R2L_IM_COMP" << std::endl;
			std::cout << " split: " << r2l_solid_both[grand].getSplit() << " score: " << r2l_solid_both[grand].getScore() << std::endl;
			break;
		case L2R_SOLID_OUTSIDE:
			std::cout << "L2R_SOLID_OUTSIDE" << std::endl;
			std::cout << " split: " << l2r_solid_outside[grand].getSplit() << " score: " << l2r_solid_outside[grand].getScore() << std::endl;
			break;
		case R2L_SOLID_OUTSIDE:
			std::cout << "R2L_SOLID_OUTSIDE" << std::endl;
			std::cout << " split: " << r2l_solid_outside[grand].getSplit() << " score: " << r2l_solid_outside[grand].getScore() << std::endl;
			break;
		case L2R_EMPTY_INSIDE:
			std::cout << "L2R_EMPTY_INSIDE" << std::endl;
			std::cout << "inner split: " << l2r_empty_inside[grand].getInnerSplit() << " split: " << l2r_empty_inside[grand].getSplit() << " score: " << l2r_empty_inside[grand].getScore() << std::endl;
			break;
		case R2L_EMPTY_INSIDE:
			std::cout << "R2L_EMPTY_INSIDE" << std::endl;
			std::cout << "inner split: " << r2l_empty_inside[grand].getInnerSplit() << " split: " << r2l_empty_inside[grand].getSplit() << " score: " << r2l_empty_inside[grand].getScore() << std::endl;
			break;
		case L2R_EMPTY_OUTSIDE:
			std::cout << "L2R_EMPTY_OUTSIDE" << std::endl;
			for (const auto & agenda : l2r_empty_outside[grand]) {
				std::cout << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			break;
		case R2L_EMPTY_OUTSIDE:
			std::cout << "R2L_EMPTY_OUTSIDE" << std::endl;
			for (const auto & agenda : r2l_empty_outside[grand]) {
				std::cout << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
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
			std::cout << " split: " << l2r_solid_both[grand].getSplit() << " score: " << l2r_solid_both[grand].getScore() << std::endl;
			std::cout << "R2L_IM_COMP" << std::endl;
			std::cout << " split: " << r2l_solid_both[grand].getSplit() << " score: " << r2l_solid_both[grand].getScore() << std::endl;
			std::cout << "L2R_SOLID_OUTSIDE" << std::endl;
			std::cout << " split: " << l2r_solid_outside[grand].getSplit() << " score: " << l2r_solid_outside[grand].getScore() << std::endl;
			std::cout << "R2L_SOLID_OUTSIDE" << std::endl;
			std::cout << " split: " << r2l_solid_outside[grand].getSplit() << " score: " << r2l_solid_outside[grand].getScore() << std::endl;
			std::cout << "L2R_EMPTY_INSIDE" << std::endl;
			std::cout << "inner split: " << l2r_empty_inside[grand].getInnerSplit() << " split: " << l2r_empty_inside[grand].getSplit() << " score: " << l2r_empty_inside[grand].getScore() << std::endl;
			std::cout << "R2L_EMPTY_INSIDE" << std::endl;
			std::cout << "inner split: " << r2l_empty_inside[grand].getInnerSplit() << " split: " << r2l_empty_inside[grand].getSplit() << " score: " << r2l_empty_inside[grand].getScore() << std::endl;
			std::cout << "L2R_EMPTY_OUTSIDE" << std::endl;
			for (const auto & agenda : l2r_empty_outside[grand]) {
				std::cout << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			std::cout << "R2L_EMPTY_OUTSIDE" << std::endl;
			for (const auto & agenda : r2l_empty_outside[grand]) {
				std::cout << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
			}
			break;
		}
	}
}
