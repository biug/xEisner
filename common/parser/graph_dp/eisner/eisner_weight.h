#ifndef _EISNER_WEIGHT_H
#define _EISNER_WEIGHT_H

#include "eisner_macros.h"
#include "common/parser/weight_base.h"
#include "include/learning/perceptron/packed_score.h"

namespace eisner {
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

	public:
		Weight(const std::string & sRead, const std::string & sRecord);
		~Weight();

		void loadScores() override;
		void saveScores() const override;
		void computeAverageFeatureWeights(const int & round) override;
	};
}

#endif