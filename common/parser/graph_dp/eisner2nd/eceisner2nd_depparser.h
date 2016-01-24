#ifndef _ECEISNER2ND_DEPPARSER_H
#define _ECEISNER2ND_DEPPARSER_H

#include <vector>
#include <unordered_set>

#include "eisner2nd_state.h"
#include "common/parser/weight2nd.h"
#include "common/parser/depparser_base.h"

namespace eisner2nd {

	class ECDepParser : public DepParserBase {
	private:
		static WordPOSTag empty_taggedword;
		static WordPOSTag start_taggedword;
		static WordPOSTag end_taggedword;

		Weight2nd *m_pWeight;

		StateItem m_lItems[MAX_SENTENCE_SIZE][MAX_SENTENCE_SIZE];
		WordPOSTag m_lSentence[MAX_SENTENCE_SIZE];
		std::vector<Arc> m_vecCorrectArcs;
		std::vector<BiArc> m_vecCorrectBiArcs;
		std::vector<Arc> m_vecTrainArcs;
		std::vector<BiArc> m_vecTrainBiArcs;
		int m_nSentenceLength;

		tscore m_nRetval;
		tscore m_lFirstOrderScore[MAX_SENTENCE_SIZE << 1];
		tscore m_lSecondOrderScore[(MAX_SENTENCE_SIZE << MAX_SENTENCE_BITS) << 1];

		std::unordered_set<BiGram<int>> m_setFirstGoldScore;
		std::unordered_set<TriGram<int>> m_setSecondGoldScore;

		void update();
		void generate(DependencyTree * retval, const DependencyTree & correct);
		void goldCheck();

		const tscore & arcScore(const int & p, const int & c);
		const tscore & twoArcScore(const int & p, const int & c, const int & c2);
		void initFirstOrderScore(const int & d);
		void initSecondOrderScore(const int & d);

		void getOrUpdateStackScore(const int & p, const int & c, const int & amount);
		void getOrUpdateStackScore(const int & p, const int & c, const int & c2, const int & amount);

	public:
		ECDepParser(const std::string & sFeatureInput, const std::string & sFeatureOut, int nState);
		~ECDepParser();

		void decode();
		void decodeArcs();

		void train(const DependencyTree & correct, const int & round);
		void parse(const Sentence & sentence, DependencyTree * retval);
		void work(DependencyTree * retval, const DependencyTree & correct);

		void finishtraining() {
			m_pWeight->computeAverageFeatureWeights(m_nTrainingRound);
			m_pWeight->saveScores();
			std::cout << "Total number of training errors are: " << m_nTotalErrors << std::endl;
		}
	};
}

#endif
