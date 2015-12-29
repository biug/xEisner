#include "eisnergc3rd_state.h"

namespace eisnergc3rd {
	StateItem::StateItem() = default;
	StateItem::~StateItem() = default;

	void StateItem::init(const int & l, const int & r) {
		type = 0;
		left = l;
		right = r;
		jux.clear();
		l2r.clear();
		r2l.clear();
		l2r_solid_both.clear();
		r2l_solid_both.clear();
	}

	void StateItem::print(const int & grand) {
		std::cout << "[" << left << "," << right << "]" << " grand is " << grand << std::endl;
		std::cout << "type is: ";
		switch (type) {
		case JUX:
			std::cout << "JUX" << std::endl;
			if (jux.find(grand) != jux.end())
				std::cout << "split: " << jux[grand].getSplit() << " score: " << jux[grand].getScore() << std::endl;
			else
				std::cout << "bad grand" << std::endl;
			break;
		case L2R:
			std::cout << "L2R_COMP" << std::endl;
			if (l2r.find(grand) != l2r.end())
				std::cout << "split: " << l2r[grand].getSplit() << " score: " << l2r[grand].getScore() << std::endl;
			else
				std::cout << "bad grand" << std::endl;
			break;
		case R2L:
			std::cout << "R2L_COMP" << std::endl;
			if (r2l.find(grand) != r2l.end())
				std::cout << "split: " << r2l[grand].getSplit() << " score: " << r2l[grand].getScore() << std::endl;
			else
				std::cout << "bad grand" << std::endl;
			break;
		case L2R_SOLID_BOTH:
			std::cout << "L2R_IM_COMP" << std::endl;
			if (l2r_solid_both.find(grand) != l2r_solid_both.end())
				for (const auto & agenda : l2r_solid_both[grand]) {
					std::cout << "inner split: " << agenda->getInnerSplit() << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
				}
			else
				std::cout << "bad grand" << std::endl;
			break;
		case R2L_SOLID_BOTH:
			std::cout << "R2L_IM_COMP" << std::endl;
			if (r2l_solid_both.find(grand) != r2l_solid_both.end())
				for (const auto & agenda : r2l_solid_both[grand]) {
					std::cout << "inner split: " << agenda->getInnerSplit() << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
				}
			else
				std::cout << "bad grand" << std::endl;
			break;
		default:
			std::cout << "ZERO" << std::endl;
			if (jux.find(grand) != jux.end())
				std::cout << "JUX split: " << jux[grand].getSplit() << " score: " << jux[grand].getScore() << std::endl;
			if (l2r.find(grand) != l2r.end())
				std::cout << "L2R_COMP split: " << l2r[grand].getSplit() << " score: " << l2r[grand].getScore() << std::endl;
			if (r2l.find(grand) != r2l.end())
				std::cout << "R2L_COMP split: " << r2l[grand].getSplit() << " score: " << r2l[grand].getScore() << std::endl;
			if (l2r_solid_both.find(grand) != l2r_solid_both.end())
				for (const auto & agenda : l2r_solid_both[grand]) {
					std::cout << "L2R_IM_COMP inner split: " << agenda->getInnerSplit() << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
				}
			if (r2l_solid_both.find(grand) != r2l_solid_both.end())
				for (const auto & agenda : r2l_solid_both[grand]) {
					std::cout << "R2L_IM_COMP inner split: " << agenda->getInnerSplit() << " split: " << agenda->getSplit() << " score: " << agenda->getScore() << std::endl;
				}
			break;
		}
	}
}
