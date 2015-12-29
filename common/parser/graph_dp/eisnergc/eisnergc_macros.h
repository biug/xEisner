#ifndef _EISNERGC_MACROS_H
#define _EISNERGC_MACROS_H

#include "common/parser/agenda.h"
#include "common/parser/macros_base.h"
#include "include/learning/perceptron/packed_score.h"

namespace eisnergc {
#define OUTPUT_STEP 100

#define GOLD_POS_SCORE 10
#define GOLD_NEG_SCORE -50

#define GRAND_AGENDA_SIZE 16
#define GRAND_MAX_LEVEL 1

#define ENCODE_L2R(X)			((X) << 1)
#define ENCODE_R2L(X)			(((X) << 1) + 1)
#define ENCODE_2ND_L2R(X,Y)		ENCODE_L2R(((X) << MAX_SENTENCE_BITS) | (Y))
#define ENCODE_2ND_R2L(X,Y)		ENCODE_R2L(((X) << MAX_SENTENCE_BITS) | (Y))

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

	typedef BiGram<int> Arc;
	typedef TriGram<int> BiArc;

	typedef AgendaBeam<ScoreWithSplit, GRAND_AGENDA_SIZE << GRAND_MAX_LEVEL> GrandAgendaU;
	typedef AgendaBeam<ScoreWithBiSplit, GRAND_AGENDA_SIZE << GRAND_MAX_LEVEL> GrandAgendaB;

	bool operator<(const Arc & arc1, const Arc & arc2);
	void Arcs2BiArcs(std::vector<Arc> & arcs, std::vector<BiArc> & biarcs);
}

#endif
