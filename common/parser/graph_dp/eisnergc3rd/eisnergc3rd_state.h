#ifndef _EISNERGC3RD_STATE_H
#define _EISNERGC3RD_STATE_H

#include "eisnergc3rd_macros.h"
#include "common/parser/agenda.h"
#include "include/learning/perceptron/score.h"

namespace eisnergc3rd {

	enum STATE{
		JUX = 1,
		L2R_SOLID_BOTH,
		R2L_SOLID_BOTH,
		L2R,
		R2L
	};

	class StateItem {
	public:
		int type;
		int left, right;
		std::unordered_map<int, ScoreWithSplit> jux, l2r, r2l;
		std::unordered_map<int, ScoreAgenda> l2r_solid_both, r2l_solid_both;

	public:

		StateItem();
		~StateItem();

		void init(const int & l, const int & r);

		void updateJUX(const int & grand, const int & split, const tscore & score);
		void updateL2RSolidBoth(const int & grand, const int & split, const int & innersplit, const tscore & score);
		void updateR2LSolidBoth(const int & grand, const int & split, const int & innersplit, const tscore & score);
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
