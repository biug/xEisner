#ifndef _EISNERGC2ND_GCWEIGHT_H
#define _EISNERGC2ND_GCWEIGHT_H

#include "eisnergc2nd_macros.h"
#include "common/parser/weight_base.h"
#include "include/learning/perceptron/packed_score.h"

namespace eisnergc2nd {
	class Weightgc : public WeightBase {
	public:

		WordIntMap m_mapPw;
		POSTagIntMap m_mapPp;
		WordPOSTagIntMap m_mapPwp;
		WordIntMap m_mapCw;
		POSTagIntMap m_mapCp;
		WordPOSTagIntMap m_mapCwp;
		WordWordPOSTagPOSTagIntMap m_mapPwpCwp;
		WordPOSTagPOSTagIntMap m_mapPpCwp;
		WordPOSTagPOSTagIntMap m_mapPwpCp;
		WordWordPOSTagIntMap m_mapPwCwp;
		WordWordPOSTagIntMap m_mapPwpCw;
		TwoWordsIntMap m_mapPwCw;
		POSTagSet2IntMap m_mapPpCp;
		POSTagSet3IntMap m_mapPpBpCp;
		POSTagSet4IntMap m_mapPpPp1Cp_1Cp;
		POSTagSet4IntMap m_mapPp_1PpCp_1Cp;
		POSTagSet4IntMap m_mapPpPp1CpCp1;
		POSTagSet4IntMap m_mapPp_1PpCpCp1;

		POSTagSet3IntMap m_mapGpPpCp;
		POSTagSet2IntMap m_mapGpCp;
		TwoWordsIntMap m_mapGwCw;
		WordPOSTagIntMap m_mapGwCp;
		WordPOSTagIntMap m_mapCwGp;

	public:
		Weightgc(const std::string & sRead, const std::string & sRecord);
		~Weightgc();

		void loadScores();
		void saveScores() const;
		void computeAverageFeatureWeights(const int & round);
	};
}

#endif
