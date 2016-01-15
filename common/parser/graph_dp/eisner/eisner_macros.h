#ifndef _EISNER_MACROS_H
#define _EISNER_MACROS_H

#include "common/parser/macros_base.h"
#include "include/learning/perceptron/packed_score.h"

namespace eisner {
#define OUTPUT_STEP 1000

#define GOLD_POS_SCORE 10
#define GOLD_NEG_SCORE -50

#define ENCODE_L2R(X)			((X) << 1)
#define ENCODE_R2L(X)			(((X) << 1) + 1)

	typedef BiGram<int> Arc;
	bool operator<(const Arc & arc1, const Arc & arc2);
}

#endif
