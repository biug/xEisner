#ifndef _EISNERGC_STATE_H
#define _EISNERGC_STATE_H

#include "eisnergc_macros.h"
#include "include/learning/perceptron/score.h"

namespace eisnergc {
	enum State {
		L2R_COMP = 1,
		R2L_COMP,
		L2R_IM_COMP,
		R2L_IM_COMP
	};

	class StateItem {
	public:
		int type;
		int left, right;
		// records grand score
		std::unordered_map<int, ScoreWithSplit> l2r, r2l, l2r_im, r2l_im;

	public:

		StateItem();
		~StateItem();

		void init(const int & l, const int & r);

		void updateL2R(const int & grand, const int & split, const tscore & score);
		void updateR2L(const int & grand, const int & split, const tscore & score);
		void updateL2RIm(const int & grand, const int & split, const tscore & score);
		void updateR2LIm(const int & grand, const int & split, const tscore & score);

		void print();
		void printGrands();
	};

	inline void StateItem::updateL2R(const int & grand, const int & split, const tscore & score) {
		if (l2r[grand] < score) {
			l2r[grand].refer(split, score);
		}
	}

	inline void StateItem::updateR2L(const int & grand, const int & split, const tscore & score) {
		if (r2l[grand] < score) {
			r2l[grand].refer(split, score);
		}
	}

	inline void StateItem::updateL2RIm(const int & grand, const int & split, const tscore & score) {
		if (l2r_im[grand] < score) {
			l2r_im[grand].refer(split, score);
		}
	}

	inline void StateItem::updateR2LIm(const int & grand, const int & split, const tscore & score) {
		if (r2l_im[grand] < score) {
			r2l_im[grand].refer(split, score);
		}
	}
}

#endif
