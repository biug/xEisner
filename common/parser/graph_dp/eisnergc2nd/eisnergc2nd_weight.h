#ifndef _EISNERGC2ND_WEIGHT_H
#define _EISNERGC2ND_WEIGHT_H

#include "eisnergc2nd_macros.h"
#include "common/parser/weight_base.h"
#include "include/learning/perceptron/packed_score.h"

namespace eisnergc2nd {
	class Weight : public WeightBase {
	public:
		// arc feature
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

		// sibling feature
		POSTagSet2IntMap m_mapC1pC2p;
		POSTagSet3IntMap m_mapPpC1pC2p;
		TwoWordsIntMap m_mapC1wC2w;
		WordPOSTagIntMap m_mapC1wC2p;
		WordPOSTagIntMap m_mapC2wC1p;

		// grand feature
		POSTagSet3IntMap m_mapGpPpCp;
		POSTagSet2IntMap m_mapGpCp;
		TwoWordsIntMap m_mapGwCw;
		WordPOSTagIntMap m_mapGwCp;
		WordPOSTagIntMap m_mapCwGp;

		// grand sibling feature
		WordPOSTagPOSTagPOSTagIntMap m_mapGwPpCpC2p;
		WordPOSTagPOSTagPOSTagIntMap m_mapPwGpCpC2p;
		WordPOSTagPOSTagPOSTagIntMap m_mapCwGpPpC2p;
		WordPOSTagPOSTagPOSTagIntMap m_mapC2wGpPpCp;
		WordWordPOSTagPOSTagIntMap m_mapGwPwCpC2p;
		WordWordPOSTagPOSTagIntMap m_mapGwCwPpC2p;
		WordWordPOSTagPOSTagIntMap m_mapGwC2wPpCp;
		WordWordPOSTagPOSTagIntMap m_mapPwCwGpC2p;
		WordWordPOSTagPOSTagIntMap m_mapPwC2wGpCp;
		WordWordPOSTagPOSTagIntMap m_mapCwC2wGpPp;
		POSTagSet4IntMap m_mapGpPpCpC2p;
		WordPOSTagPOSTagIntMap m_mapGwCpC2p;
		WordPOSTagPOSTagIntMap m_mapCwGpC2p;
		WordPOSTagPOSTagIntMap m_mapC2wGpCp;
		WordWordPOSTagIntMap m_mapGwCwC2p;
		WordWordPOSTagIntMap m_mapGwC2wCp;
		WordWordPOSTagIntMap m_mapCwC2wGp;
		POSTagSet3IntMap m_mapGpCpC2p;

	public:
		Weight(const std::string & sRead, const std::string & sRecord);
		~Weight();

		void loadScores();
		void saveScores() const;
		void computeAverageFeatureWeights(const int & round);
	};
}

#endif
