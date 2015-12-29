#ifndef _EISNERGC2ND_STATE_H
#define _EISNERGC2ND_STATE_H

#include "eisnergc2nd_macros.h"
#include "common/parser/agenda.h"
#include "include/learning/perceptron/score.h"

namespace eisnergc2nd {

	enum STATE{
		JUX = 1,
		L2R,
		R2L,
		L2R_IM,
		R2L_IM
	};

	class StateItem {
	public:
		int type;
		int left, right;
		std::unordered_map<int, ScoreWithSplit> jux, l2r, r2l, l2r_im, r2l_im;

	public:

		StateItem();
		~StateItem();

		void init(const int & l, const int & r);

		void updateJUX(const int & grand, const int & split, const tscore & score);
		void updateL2R(const int & grand, const int & split, const tscore & score);
		void updateR2L(const int & grand, const int & split, const tscore & score);
		void updateL2RIm(const int & grand, const int & split, const tscore & score);
		void updateR2LIm(const int & grand, const int & split, const tscore & score);

		void print(const int & grand);
	};

	inline void StateItem::updateJUX(const int & grand, const int & split, const tscore & score) {
		if (jux[grand] < score) {
			jux[grand].refer(split, score);
		}
	}

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
