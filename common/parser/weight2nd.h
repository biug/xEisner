#ifndef _WEIGHT2ND_H
#define _WEIGHT2ND_H

#include "macros_base.h"
#include "common/parser/weight1st.h"
#include "include/learning/perceptron/packed_score.h"

class Weight2nd : public Weight1st {
protected:

	POSTagSet2IntMap m_mapC1pC2p;
	POSTagSet3IntMap m_mapPpC1pC2p;
	TwoWordsIntMap m_mapC1wC2w;
	WordPOSTagIntMap m_mapC1wC2p;
	WordPOSTagIntMap m_mapC2wC1p;

public:
	Weight2nd(const std::string & sRead, const std::string & sRecord, int nParserState);
	~Weight2nd();

	void loadScores();
	void saveScores() const;
	void computeAverageFeatureWeights(const int & round);

	void getOrUpdateBiArcScore(tscore & retval, const int & p, const int & c, const int & c2, const int & amount, int sentLen, WordPOSTag (&sent)[MAX_SENTENCE_SIZE]);
};

#endif
