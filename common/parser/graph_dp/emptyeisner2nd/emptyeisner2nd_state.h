#ifndef _EMPTY_EISNER2ND_STATE_H
#define _EMPTY_EISNER2ND_STATE_H

#include <unordered_map>

#include "emptyeisner2nd_macros.h"
#include "common/parser/agenda.h"
#include "include/learning/perceptron/score.h"

namespace emptyeisner2nd {

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
		ScoreWithSplit jux, l2r, r2l, l2r_solid_both, r2l_solid_both, l2r_solid_outside, r2l_solid_outside;
		ScoreWithBiSplit l2r_empty_inside, r2l_empty_inside;
		ScoreAgenda l2r_empty_outside, r2l_empty_outside;

	public:

		StateItem();
		StateItem(const StateItem & item);
		~StateItem();

		void init(const int & l, const int & r);

		void updateJUX(const int & split, const tscore & score);
		void updateL2RSolidBoth(const int & split, const tscore & score);
		void updateR2LSolidBoth(const int & split, const tscore & score);
		void updateL2REmptyOutside(const int & split, const tscore & score);
		void updateR2LEmptyOutside(const int & split, const tscore & score);
		void updateL2RSolidOutside(const int & split, const tscore & score);
		void updateR2LSolidOutside(const int & split, const tscore & score);
		void updateL2REmptyInside(const int & split, const int & innersplit, const tscore & score);
		void updateR2LEmptyInside(const int & split, const int & innersplit, const tscore & score);
		void updateL2R(const int & split, const tscore & score);
		void updateR2L(const int & split, const tscore & score);

		void print(const int & grand);
	};

	inline void StateItem::updateJUX(const int & split, const tscore & score) {
		if (jux < score) {
			jux.refer(split, score);
		}
	}

	inline void StateItem::updateL2RSolidBoth(const int & split, const tscore & score) {
		if (l2r_solid_both < score) {
			l2r_solid_both.refer(split, score);
		}
	}

	inline void StateItem::updateR2LSolidBoth(const int & split, const tscore & score) {
		if (r2l_solid_both < score) {
			r2l_solid_both.refer(split, score);
		}
	}

	inline void StateItem::updateL2REmptyOutside(const int & split, const tscore & score) {
		l2r_empty_outside.insertItem(ScoreWithSplit(split, score));
	}

	inline void StateItem::updateR2LEmptyOutside(const int & split, const tscore & score) {
		r2l_empty_outside.insertItem(ScoreWithSplit(split, score));
	}

	inline void StateItem::updateL2RSolidOutside(const int & split, const tscore & score) {
		if (l2r_solid_outside < score) {
			l2r_solid_outside.refer(split, score);
		}
	}

	inline void StateItem::updateR2LSolidOutside(const int & split, const tscore & score) {
		if (r2l_solid_outside < score) {
			r2l_solid_outside.refer(split, score);
		}
	}

	inline void StateItem::updateL2REmptyInside(const int & split, const int & innersplit, const tscore & score) {
		if (l2r_empty_inside < score) {
			l2r_empty_inside.refer(split, innersplit, score);
		}
	}

	inline void StateItem::updateR2LEmptyInside(const int & split, const int & innersplit, const tscore & score) {
		if (r2l_empty_inside < score) {
			r2l_empty_inside.refer(split, innersplit, score);
		}
	}

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
}

#endif
