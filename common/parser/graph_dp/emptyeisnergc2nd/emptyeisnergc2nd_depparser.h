#ifndef _EMPTY_EISNERGC2ND_DEPPARSER_H
#define _EMPTY_EISNERGC2ND_DEPPARSER_H

#include <vector>
#include <unordered_set>

#include "emptyeisnergc2nd_state.h"
#include "emptyeisnergc2nd_weight.h"
#include "common/parser/depparser_base.h"

namespace emptyeisnergc2nd {

	class DepParser : public DepParserBase {
	private:
		static WordPOSTag empty_taggedword;
		static WordPOSTag start_taggedword;
		static WordPOSTag end_taggedword;

		Weight *m_pWeight;

		double m_tStartTime;
		double m_tInitSpaceTime;
		double m_tGetScoreTime;
		//double m_tUpdateScoreTime;
		//double m_tDecodeTime;

		std::vector<StateItem> m_lItems[MAX_SENTENCE_SIZE];
		WordPOSTag m_lSentence[MAX_SENTENCE_SIZE][MAX_EMPTYTAG_SIZE];
		WordPOSTag m_lSentenceWithEmpty[MAX_SENTENCE_SIZE];
		std::vector<Arc> m_vecCorrectArcs;
		std::vector<TriArc> m_vecCorrectTriArcs;
		std::vector<Arc> m_vecTrainArcs;
		std::vector<TriArc> m_vecTrainTriArcs;
		int m_nSentenceLength;

		tscore m_nRetval;
		std::vector<std::vector<tscore>> m_vecArcScore;
		std::vector<std::vector<std::vector<tscore>>> m_vecBiSiblingScore;
		std::vector<std::vector<std::vector<tscore>>> m_vecGrandChildScore;
		std::vector<std::vector<int>> m_vecGrandsAsLeft;
		std::vector<std::vector<int>> m_vecGrandsAsRight;
		std::vector<std::vector<AgendaBeam<ScoreWithType, MAX_EMPTY_SIZE>>> m_vecFirstOrderEmptyScore;

		int m_nDis, m_nDir;

		Word g_word, p_word, c_word, c2_word, c3_word;
		POSTag g_tag, p_tag, c_tag, c2_tag, c3_tag;

		POSTag p_1_tag, p1_tag, c_1_tag, c1_tag, b_tag;

		WordInt word_int;
		POSTagInt tag_int;

		TwoWordsInt word_word_int;
		POSTagSet2Int tag_tag_int;
		WordPOSTagInt word_tag_int;

		POSTagSet3Int tag_tag_tag_int;
		WordPOSTagPOSTagInt word_tag_tag_int;
		WordWordPOSTagInt word_word_tag_int;

		POSTagSet4Int tag_tag_tag_tag_int;
		WordWordPOSTagPOSTagInt word_word_tag_tag_int;
		WordPOSTagPOSTagPOSTagInt word_tag_tag_tag_int;

		POSTagSet5Int tag_tag_tag_tag_tag_int;
		WordPOSTagPOSTagPOSTagPOSTagInt word_tag_tag_tag_tag_int;

		std::unordered_set<BiGram<int>> m_setArcGoldScore;
		std::unordered_set<TriGram<int>> m_setBiSiblingArcGoldScore;
		std::unordered_set<TriGram<int>> m_setGrandSiblingArcGoldScore;
		std::unordered_set<QuarGram<int>> m_setGrandBiSiblingArcGoldScore;

		void update();
		void generate(DependencyTree * retval, const DependencyTree & correct);
		void goldCheck();

		void readEmptySentAndArcs(const DependencyTree & correct);

		const tscore & arcScore(const int & p, const int & c);
		const tscore & biSiblingArcScore(const int & p, const int & c, const int & c2);
		const tscore & grandSiblingArcScore(const int & g, const int & p, const int & c);
		const tscore & grandBiSiblingArcScore(const int & g, const int & p, const int & c, const int & c2);
		void initArcScore();
		void initBiSiblingArcScore();
		void initGrandSiblingArcScore();
		bool initGrands(int level);

		bool testEmptyNode(const int & p, const int & c);
		void getOrUpdateGrandScore(const int & g, const int & p, const int & c, const int & amount);
		void getOrUpdateGrandScore(const int & g, const int & p, const int & c, const int & c2, const int & amount);
		void getOrUpdateSiblingScore(const int & p, const int & c, const int & amount);
		void getOrUpdateSiblingScore(const int & p, const int & c, const int & c2, const int & amount);

	public:
		DepParser(const std::string & sFeatureInput, const std::string & sFeatureOut, int nState);
		~DepParser();

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

		void printTime() {
//			std::cout << "total time tick is " << GetTickCount() - m_tStartTime << std::endl;
			std::cout << "total malloc time is " << m_tInitSpaceTime << std::endl;
			std::cout << "total get score time is " << m_tGetScoreTime << std::endl;
			//std::cout << "total update score time is " << m_tUpdateScoreTime << std::endl;
			//std::cout << "total decode time is " << m_tDecodeTime << std::endl;
		}
	};
}

#endif
