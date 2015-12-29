#ifndef _EMPTY_EISNERGC2ND_MACROS_H
#define _EMPTY_EISNERGC2ND_MACROS_H

#include "common/parser/agenda.h"
#include "common/parser/macros_base.h"
#include "include/learning/perceptron/packed_score.h"

namespace emptyeisnergc2nd {
#define OUTPUT_STEP 1

#define AGENDA_SIZE		4
#define MAX_EMPTY_SIZE	17

#define EMPTYTAG			"EMCAT"
#define MAX_EMPTYTAG_SIZE	32

#define GOLD_POS_SCORE 10
#define GOLD_NEG_SCORE -50

#define GRAND_AGENDA_SIZE 16
#define GRAND_MAX_LEVEL 1

#define ENCODE_L2R(X)			((X) << 1)
#define ENCODE_R2L(X)			(((X) << 1) + 1)
#define ENCODE_2ND_L2R(X,Y)		ENCODE_L2R(((X) << MAX_SENTENCE_BITS) | (Y))
#define ENCODE_2ND_R2L(X,Y)		ENCODE_R2L(((X) << MAX_SENTENCE_BITS) | (Y))

#define ENCODE_EMPTY(X,T)		(((T) << MAX_SENTENCE_BITS) | (X))
#define DECODE_EMPTY_POS(X)		((X) & ((1 << MAX_SENTENCE_BITS) - 1))
#define DECODE_EMPTY_TAG(X)		((X) >> MAX_SENTENCE_BITS)

#define LESS_EMPTY_EMPTY(X,Y)	(DECODE_EMPTY_POS(X) < DECODE_EMPTY_POS(Y))
#define LESS_EMPTY_SOLID(X,Y)	(DECODE_EMPTY_POS(X) <= (Y))
#define LESS_SOLID_EMPTY(X,Y)	((X) < DECODE_EMPTY_POS(Y))
#define LESS_SOLID_SOLID(X,Y)	((X) < (Y))

#define ARC_LEFT(X,Y)			(IS_EMPTY(Y) ? LESS_EMPTY_SOLID(Y,X) : LESS_SOLID_SOLID(Y,X))
#define ARC_RIGHT(X,Y)			(IS_EMPTY(Y) ? LESS_SOLID_EMPTY(X,Y) : LESS_SOLID_SOLID(X,Y))

	typedef PackedScoreMap<WordInt> WordIntMap;
	typedef PackedScoreMap<POSTagInt> POSTagIntMap;

	typedef PackedScoreMap<TwoWordsInt> TwoWordsIntMap;
	typedef PackedScoreMap<POSTagSet2Int> POSTagSet2IntMap;
	typedef PackedScoreMap<WordPOSTagInt> WordPOSTagIntMap;

	typedef PackedScoreMap<POSTagSet3Int> POSTagSet3IntMap;
	typedef PackedScoreMap<WordWordPOSTagInt> WordWordPOSTagIntMap;
	typedef PackedScoreMap<WordPOSTagPOSTagInt> WordPOSTagPOSTagIntMap;

	typedef PackedScoreMap<POSTagSet4Int> POSTagSet4IntMap;
	typedef PackedScoreMap<WordWordPOSTagPOSTagInt> WordWordPOSTagPOSTagIntMap;
	typedef PackedScoreMap<WordPOSTagPOSTagPOSTagInt> WordPOSTagPOSTagPOSTagIntMap;

	typedef PackedScoreMap<POSTagSet5Int> POSTagSet5IntMap;
	typedef PackedScoreMap<WordPOSTagPOSTagPOSTagPOSTagInt> WordPOSTagPOSTagPOSTagPOSTagIntMap;

	typedef AgendaBeam<ScoreWithSplit, AGENDA_SIZE> ScoreAgenda;

	typedef BiGram<int> Arc;
	typedef QuarGram<int> TriArc;

	typedef AgendaBeam<ScoreWithSplit, GRAND_AGENDA_SIZE << GRAND_MAX_LEVEL> GrandAgendaU;
	typedef AgendaBeam<ScoreWithBiSplit, GRAND_AGENDA_SIZE << GRAND_MAX_LEVEL> GrandAgendaB;

	bool operator<(const Arc & arc1, const Arc & arc2);
	bool compareArc(const Arc & arc1, const Arc & arc2);
	void Arcs2TriArcs(std::vector<Arc> & arcs, std::vector<TriArc> & triarcs);
}

#endif
