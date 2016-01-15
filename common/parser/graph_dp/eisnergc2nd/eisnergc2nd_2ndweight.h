#ifndef _EISNERGC2ND_2NDWEIGHT_H
#define _EISNERGC2ND_2NDWEIGHT_H

#include "eisnergc2nd_macros.h"
#include "common/parser/weight_base.h"
#include "include/learning/perceptron/packed_score.h"

namespace eisnergc2nd {
	class Weight2nd : public WeightBase {
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

		POSTagSet2IntMap m_mapC1pC2p;
		POSTagSet3IntMap m_mapPpC1pC2p;
		TwoWordsIntMap m_mapC1wC2w;
		WordPOSTagIntMap m_mapC1wC2p;
		WordPOSTagIntMap m_mapC2wC1p;

	public:
		Weight2nd(const std::string & sRead, const std::string & sRecord);
		~Weight2nd();

		void loadScores();
		void saveScores() const;
		void computeAverageFeatureWeights(const int & round);
	};
}

#endif
