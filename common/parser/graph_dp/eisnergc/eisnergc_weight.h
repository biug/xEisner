#ifndef _EISNERGC_WEIGHT_H
#define _EISNERGC_WEIGHT_H

#include "eisnergc_macros.h"
#include "common/parser/weight_base.h"
#include "include/learning/perceptron/packed_score.h"

namespace eisnergc {
	class Weight : public WeightBase {
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

		POSTagSet3IntMap m_mapGpHpMp;
		POSTagSet2IntMap m_mapGpMp;
		TwoWordsIntMap m_mapGwMw;
		WordPOSTagIntMap m_mapGwMp;
		WordPOSTagIntMap m_mapMwGp;

	public:
		Weight(const std::string & sRead, const std::string & sRecord);
		~Weight();

		void loadScores() override;
		void saveScores() const override;
		void computeAverageFeatureWeights(const int & round) override;
	};
}

#endif