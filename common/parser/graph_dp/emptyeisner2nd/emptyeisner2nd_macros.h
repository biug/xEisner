#ifndef _EMPTY_EISNER2ND_MACROS_H
#define _EMPTY_EISNER2ND_MACROS_H

#include "common/parser/agenda.h"
#include "common/parser/macros_base.h"
#include "include/learning/perceptron/packed_score.h"

namespace emptyeisner2nd {
#define OUTPUT_STEP 1000

#define AGENDA_SIZE		8
#define MAX_EMPTY_SIZE	17

#define EMPTYTAG			"EMCAT"

#define GOLD_POS_SCORE 10
#define GOLD_NEG_SCORE -50

#define LESS_EMPTY_EMPTY(X,Y)	(DECODE_EMPTY_POS(X) < DECODE_EMPTY_POS(Y))
#define LESS_EMPTY_SOLID(X,Y)	(DECODE_EMPTY_POS(X) <= (Y))
#define LESS_SOLID_EMPTY(X,Y)	((X) < DECODE_EMPTY_POS(Y))
#define LESS_SOLID_SOLID(X,Y)	((X) < (Y))

#define ARC_LEFT(X,Y)			(IS_EMPTY(Y) ? LESS_EMPTY_SOLID(Y,X) : LESS_SOLID_SOLID(Y,X))
#define ARC_RIGHT(X,Y)			(IS_EMPTY(Y) ? LESS_SOLID_EMPTY(X,Y) : LESS_SOLID_SOLID(X,Y))

	typedef AgendaBeam<ScoreWithSplit, AGENDA_SIZE> ScoreAgenda;

	typedef BiGram<int> Arc;
	typedef TriGram<int> BiArc;

	bool operator<(const Arc & arc1, const Arc & arc2);
	bool compareArc(const Arc & arc1, const Arc & arc2);
	void Arcs2BiArcs(std::vector<Arc> & arcs, std::vector<BiArc> & triarcs);
}

#endif