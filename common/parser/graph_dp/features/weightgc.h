#ifndef _WEIGHTGC_H
#define _WEIGHTGC_H

#include "weight1st.h"
#include "common/parser/macros_base.h"
#include "include/learning/perceptron/packed_score.h"

class Weightgc : public Weight1st {
public:

	POSTagSet3IntMap m_mapGpHpMp;
	POSTagSet2IntMap m_mapGpMp;
	TwoWordsIntMap m_mapGwMw;
	WordPOSTagIntMap m_mapGwMp;
	WordPOSTagIntMap m_mapMwGp;

public:
	Weightgc(const std::string & sRead, const std::string & sRecord, int nParserState);
	~Weightgc();

	void loadScores();
	void saveScores() const;
	void computeAverageFeatureWeights(const int & round);

	void getOrUpdateGrandArcScore(tscore & retval, const int & g, const int & h, const int & m, const int & amount, int sentLen, WordPOSTag (&sent)[MAX_SENTENCE_SIZE]);
};

#endif
