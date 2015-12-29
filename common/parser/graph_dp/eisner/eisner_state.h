#ifndef _EISNER_STATE_H
#define _EISNER_STATE_H

#include "eisner_macros.h"
#include "include/learning/perceptron/score.h"

namespace eisner {
	enum STATE{
		L2R_COMP = 1,
		R2L_COMP,
		L2R_IM_COMP,
		R2L_IM_COMP,
	};

	class StateItem {
	public:
		int type;
		int left, right;
		ScoreWithSplit l2r, r2l, l2r_im, r2l_im;

	public:

		StateItem();
		~StateItem();

		void init(const int & l, const int & r);

		void updateL2R(const int & split, const tscore & score);
		void updateR2L(const int & split, const tscore & score);
		void updateL2RIm(const int & split, const tscore & score);
		void updateR2LIm(const int & split, const tscore & score);

		void print();
	};

	inline void StateItem::updateL2R(const int & split, const tscore & score) {
		if (l2r < score) {
			l2r.refer(split, score);
		}
	}

	inline void StateItem::updateR2L(const int & split, const tscore & score) {
		if (r2l < score) {
			r2l.refer(split, score);
		}
	}

	inline void StateItem::updateL2RIm(const int & split, const tscore & score) {
		if (l2r_im < score) {
			l2r_im.refer(split, score);
		}
	}

	inline void StateItem::updateR2LIm(const int & split, const tscore & score) {
		if (r2l_im < score) {
			r2l_im.refer(split, score);
		}
	}
}

#endif
