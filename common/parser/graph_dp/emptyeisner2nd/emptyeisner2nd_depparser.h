#ifndef _EMPTY_EISNER2ND_DEPPARSER_H
#define _EMPTY_EISNER2ND_DEPPARSER_H

#include <vector>
#include <unordered_set>

#include "emptyeisner2nd_state.h"
#include "common/parser/depparser_base.h"
#include "common/parser/weightec2nd.h"

namespace emptyeisner2nd {

	class DepParser : public DepParserBase {
	private:
		static WordPOSTag empty_taggedword;
		static WordPOSTag start_taggedword;
		static WordPOSTag end_taggedword;

		Weightec2nd *m_pWeight;

		StateItem m_lItems[MAX_SENTENCE_SIZE][MAX_SENTENCE_SIZE][MAX_EMPTY_COUNT];
		WordPOSTag m_lSentence[MAX_SENTENCE_SIZE][MAX_EMPTYTAG_SIZE];
		WordPOSTag m_lSentenceWithEmpty[MAX_SENTENCE_SIZE];
		std::vector<Arc> m_vecCorrectArcs;
		std::vector<BiArc> m_vecCorrectBiArcs;
		std::vector<Arc> m_vecTrainArcs;
		std::vector<BiArc> m_vecTrainBiArcs;
		int m_nSentenceLength;

		tscore m_nRetval;
		std::vector<std::vector<tscore>> m_vecArcScore;
		std::vector<std::vector<std::vector<tscore>>> m_vecBiSiblingScore;
		std::vector<std::vector<AgendaBeam<ScoreWithType, MAX_EMPTY_SIZE>>> m_vecFirstOrderEmptyScore;

		std::unordered_set<BiGram<int>> m_setArcGoldScore;
		std::unordered_set<TriGram<int>> m_setBiSiblingArcGoldScore;

		void update();
		void generate(DependencyTree * retval, const DependencyTree & correct);
		void goldCheck();

		void readEmptySentAndArcs(const DependencyTree & correct);

		const tscore & arcScore(const int & p, const int & c);
		const tscore & biSiblingArcScore(const int & p, const int & c, const int & c2);
		void initArcScore();
		void initBiSiblingArcScore();

		bool testEmptyNode(const int & p, const int & c);
		void getOrUpdateSiblingScore(const int & p, const int & c, const int & amount);
		void getOrUpdateSiblingScore(const int & p, const int & c, const int & c2, const int & amount);

	public:
		DepParser(const std::string & sFeatureInput, const std::string & sFeatureOut, int nState);
		~DepParser();

		void decode();
		void decodeArcs(int nec);

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
