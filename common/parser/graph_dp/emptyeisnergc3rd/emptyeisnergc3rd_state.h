#ifndef _EMPTY_EISNERGC3RD_STATE_H
#define _EMPTY_EISNERGC3RD_STATE_H

#include "emptyeisnergc3rd_macros.h"
#include "common/parser/agenda.h"
#include "include/learning/perceptron/score.h"

namespace emptyeisnergc3rd {

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
		std::vector<ScoreWithSplit> jux;
		std::vector<ScoreAgenda> l2r_solid_both, r2l_solid_both;
		std::vector<ScoreAgenda> l2r_empty_outside, r2l_empty_outside;
		std::vector<ScoreAgenda> l2r_empty_inside, r2l_empty_inside;
		std::vector<ScoreAgenda> l2r_solid_outside, r2l_solid_outside;
		std::vector<ScoreWithSplit> l2r, r2l;

	public:

		StateItem();
		StateItem(const StateItem & item);
		~StateItem();

		void init(const int & l, const int & r, const int & len);

		void updateJUX(const int & grand, const int & split, const tscore & score);
		void updateL2RSolidBoth(const int & grand, const int & split, const int & innersplit, const tscore & score);
		void updateR2LSolidBoth(const int & grand, const int & split, const int & innersplit, const tscore & score);
		void updateL2REmptyOutside(const int & grand, const int & split, const int & innersplit, const tscore & score);
		void updateR2LEmptyOutside(const int & grand, const int & split, const int & innersplit, const tscore & score);
		void updateL2RSolidOutside(const int & grand, const int & split, const int & innersplit, const tscore & score);
		void updateR2LSolidOutside(const int & grand, const int & split, const int & innersplit, const tscore & score);
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

	inline void StateItem::updateL2RSolidBoth(const int & grand, const int & split, const int & innersplit, const tscore & score) {
		l2r_solid_both[grand].insertItem(ScoreWithBiSplit(split, innersplit, score));
	}

	inline void StateItem::updateR2LSolidBoth(const int & grand, const int & split, const int & innersplit, const tscore & score) {
		r2l_solid_both[grand].insertItem(ScoreWithBiSplit(split, innersplit, score));
	}

	inline void StateItem::updateL2REmptyInside(const int & grand, const int & split, const int & innersplit, const tscore & score) {
		l2r_empty_inside[grand].insertItem(ScoreWithBiSplit(split, innersplit, score));
	}

	inline void StateItem::updateR2LEmptyInside(const int & grand, const int & split, const int & innersplit, const tscore & score) {
		r2l_empty_inside[grand].insertItem(ScoreWithBiSplit(split, innersplit, score));
	}

	inline void StateItem::updateL2RSolidOutside(const int & grand, const int & split, const int & innersplit, const tscore & score) {
		l2r_solid_outside[grand].insertItem(ScoreWithBiSplit(split, innersplit, score));
	}

	inline void StateItem::updateR2LSolidOutside(const int & grand, const int & split, const int & innersplit, const tscore & score) {
		r2l_solid_outside[grand].insertItem(ScoreWithBiSplit(split, innersplit, score));
	}

	inline void StateItem::updateL2REmptyOutside(const int & grand, const int & split, const int & innersplit, const tscore & score) {
		l2r_empty_outside[grand].insertItem(ScoreWithBiSplit(split, innersplit, score));
	}

	inline void StateItem::updateR2LEmptyOutside(const int & grand, const int & split, const int & innersplit, const tscore & score) {
		r2l_empty_outside[grand].insertItem(ScoreWithBiSplit(split, innersplit, score));
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
