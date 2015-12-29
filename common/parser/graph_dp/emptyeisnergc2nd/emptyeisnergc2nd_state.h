#ifndef _EMPTY_EISNERGC2ND_STATE_H
#define _EMPTY_EISNERGC2ND_STATE_H

#include <unordered_map>

#include "emptyeisnergc2nd_macros.h"
#include "common/parser/agenda.h"
#include "include/learning/perceptron/score.h"

namespace emptyeisnergc2nd {

	enum STATE{
		JUX = 1,
		L2R_SOLID_BOTH,
		R2L_SOLID_BOTH,
		L2R_EMPTY_OUTSIDE,
		R2L_EMPTY_OUTSIDE,
		L2R_SOLID_OUTSIDE,
		R2L_SOLID_OUTSIDE,
		L2R_EMPTY_INSIDE,
		R2L_EMPTY_INSIDE,
		L2R,
		R2L
	};

	class StateItem {
	public:
		int type;
		int left, right;
		std::unordered_map<int, ScoreWithSplit> jux, l2r_solid_both, r2l_solid_both, l2r_solid_outside, r2l_solid_outside, l2r, r2l;
		std::unordered_map<int, ScoreWithBiSplit> l2r_empty_inside, r2l_empty_inside;
		std::unordered_map<int, ScoreAgenda> l2r_empty_outside, r2l_empty_outside;

	public:

		StateItem();
		StateItem(const StateItem & item);
		~StateItem();

		void init(const int & l, const int & r);

		void updateJUX(const int & grand, const int & split, const tscore & score);
		void updateL2RSolidBoth(const int & grand, const int & split, const tscore & score);
		void updateR2LSolidBoth(const int & grand, const int & split, const tscore & score);
		void updateL2REmptyOutside(const int & grand, const int & split, const tscore & score);
		void updateR2LEmptyOutside(const int & grand, const int & split, const tscore & score);
		void updateL2RSolidOutside(const int & grand, const int & split, const tscore & score);
		void updateR2LSolidOutside(const int & grand, const int & split, const tscore & score);
		void updateL2REmptyInside(const int & grand, const int & split, const int & innersplit, const tscore & score);
		void updateR2LEmptyInside(const int & grand, const int & split, const int & innersplit, const tscore & score);
		void updateL2R(const int & grand, const int & split, const tscore & score);
		void updateR2L(const int & grand, const int & split, const tscore & score);

		void print(const int & grand);
	};

	inline void StateItem::updateJUX(const int & grand, const int & split, const tscore & score) {
		if (jux[grand] < score) {
			jux[grand].refer(split, score);
		}
	}

	inline void StateItem::updateL2RSolidBoth(const int & grand, const int & split, const tscore & score) {
		if (l2r_solid_both[grand] < score) {
			l2r_solid_both[grand].refer(split, score);
		}
	}

	inline void StateItem::updateR2LSolidBoth(const int & grand, const int & split, const tscore & score) {
		if (r2l_solid_both[grand] < score) {
			r2l_solid_both[grand].refer(split, score);
		}
	}

	inline void StateItem::updateL2REmptyOutside(const int & grand, const int & split, const tscore & score) {
		l2r_empty_outside[grand].insertItem(ScoreWithSplit(split, score));
	}

	inline void StateItem::updateR2LEmptyOutside(const int & grand, const int & split, const tscore & score) {
		r2l_empty_outside[grand].insertItem(ScoreWithSplit(split, score));
	}

	inline void StateItem::updateL2RSolidOutside(const int & grand, const int & split, const tscore & score) {
		if (l2r_solid_outside[grand] < score) {
			l2r_solid_outside[grand].refer(split, score);
		}
	}

	inline void StateItem::updateR2LSolidOutside(const int & grand, const int & split, const tscore & score) {
		if (r2l_solid_outside[grand] < score) {
			r2l_solid_outside[grand].refer(split, score);
		}
	}

	inline void StateItem::updateL2REmptyInside(const int & grand, const int & split, const int & innersplit, const tscore & score) {
		if (l2r_empty_inside[grand] < score) {
			l2r_empty_inside[grand].refer(split, innersplit, score);
		}
	}

	inline void StateItem::updateR2LEmptyInside(const int & grand, const int & split, const int & innersplit, const tscore & score) {
		if (r2l_empty_inside[grand] < score) {
			r2l_empty_inside[grand].refer(split, innersplit, score);
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
}

#endif
