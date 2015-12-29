#ifndef _EMPTY_EISNERGC3RD_WEIGHT_H
#define _EMPTY_EISNERGC3RD_WEIGHT_H

#include "emptyeisnergc3rd_macros.h"
#include "common/parser/weight_base.h"
#include "include/learning/perceptron/packed_score.h"

namespace emptyeisnergc3rd {
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

		// tri-sibling feature
		WordPOSTagPOSTagPOSTagIntMap m_mapPwC1pC2pC3p;
		WordPOSTagPOSTagPOSTagIntMap m_mapC1wPpC2pC3p;
		WordPOSTagPOSTagPOSTagIntMap m_mapC2wPpC1pC3p;
		WordPOSTagPOSTagPOSTagIntMap m_mapC3wPpC1pC2p;
		WordWordPOSTagPOSTagIntMap m_mapPwC1wC2pC3p;
		WordWordPOSTagPOSTagIntMap m_mapPwC2wC1pC3p;
		WordWordPOSTagPOSTagIntMap m_mapPwC3wC1pC2p;
		WordWordPOSTagPOSTagIntMap m_mapC1wC2wPpC3p;
		WordWordPOSTagPOSTagIntMap m_mapC1wC3wPpC2p;
		WordWordPOSTagPOSTagIntMap m_mapC2wC3wPpC1p;
		POSTagSet4IntMap m_mapPpC1pC2pC3p;
		WordPOSTagPOSTagIntMap m_mapC1wC2pC3p;
		WordPOSTagPOSTagIntMap m_mapC2wC1pC3p;
		WordPOSTagPOSTagIntMap m_mapC3wC1pC2p;
		WordWordPOSTagIntMap m_mapC1wC2wC3p;
		WordWordPOSTagIntMap m_mapC1wC3wC2p;
		WordWordPOSTagIntMap m_mapC2wC3wC1p;
		POSTagSet3IntMap m_mapC1pC2pC3p;
		TwoWordsIntMap m_mapC1wC3w;
		WordPOSTagIntMap m_mapC1wC3p;
		WordPOSTagIntMap m_mapC3wC1p;
		POSTagSet2IntMap m_mapC1pC3p;

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

		// grand tri-sibling feature
		WordPOSTagPOSTagPOSTagPOSTagIntMap m_mapGwPpCpC2pC3p;
		WordPOSTagPOSTagPOSTagPOSTagIntMap m_mapPwGpCpC2pC3p;
		WordPOSTagPOSTagPOSTagPOSTagIntMap m_mapCwGpPpC2pC3p;
		WordPOSTagPOSTagPOSTagPOSTagIntMap m_mapC2wGpPpCpC2p;
		WordPOSTagPOSTagPOSTagPOSTagIntMap m_mapC3wGpPpCpC2p;
		POSTagSet5IntMap m_mapGpPpCpC2pC3p;
		WordPOSTagPOSTagPOSTagIntMap m_mapGwCpC2pC3p;
		WordPOSTagPOSTagPOSTagIntMap m_mapCwGpC2pC3p;
		WordPOSTagPOSTagPOSTagIntMap m_mapC2wGpCpC3p;
		WordPOSTagPOSTagPOSTagIntMap m_mapC3wGpCpC2p;
		POSTagSet4IntMap m_mapGpCpC2pC3p;

	public:
		Weight(const std::string & sRead, const std::string & sRecord);
		~Weight();

		void loadScores() override;
		void saveScores() const override;
		void computeAverageFeatureWeights(const int & round) override;
	};
}

#endif