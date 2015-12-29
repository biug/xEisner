#ifndef _EISNER_DEPPARSER_H
#define _EISNER_DEPPARSER_H

#include <vector>
#include <unordered_set>

#include "eisnergc_state.h"
#include "eisnergc_weight.h"
#include "common/parser/depparser_base.h"

namespace eisnergc {
	class DepParser : public DepParserBase {
	private:
		static WordPOSTag empty_taggedword;
		static WordPOSTag start_taggedword;
		static WordPOSTag end_taggedword;

		StateItem m_lItems[MAX_SENTENCE_SIZE][MAX_SENTENCE_SIZE];
		WordPOSTag m_lSentence[MAX_SENTENCE_SIZE];
		std::vector<Arc> m_vecCorrectArcs;
		std::vector<BiArc> m_vecCorrectBiArcs;
		std::vector<Arc> m_vecTrainArcs;
		std::vector<BiArc> m_vecTrainBiArcs;
		int m_nSentenceLength;

		tscore m_nRetval;
		std::vector<std::vector<tscore>> m_vecArcScore;
		std::vector<std::vector<std::vector<tscore>>> m_vecGrandChildScore;
		std::vector<std::vector<int>> m_vecGrandsAsLeft;
		std::vector<std::vector<int>> m_vecGrandsAsRight;

		int m_nDis, m_nDir;

		Word g_word, h_word, m_word;
		POSTag g_tag, h_tag, m_tag;

		WordInt word_int;
		POSTagInt tag_int;

		POSTag h_1_tag, h1_tag, m_1_tag, m1_tag, b_tag;

		TwoWordsInt word_word_int;
		POSTagSet2Int tag_tag_int;
		WordPOSTagInt word_tag_int;

		POSTagSet3Int tag_tag_tag_int;
		WordPOSTagPOSTagInt word_tag_tag_int;
		WordWordPOSTagInt word_word_tag_int;

		POSTagSet4Int tag_tag_tag_tag_int;
		WordWordPOSTagPOSTagInt word_word_tag_tag_int;

		std::unordered_set<BiGram<int>> m_setFirstGoldScore;
		std::unordered_set<TriGram<int>> m_setSecondGoldScore;

		void update();
		void generate(DependencyTree * retval, const DependencyTree & correct);
		void goldCheck();

		const tscore & arcScore(const int & h, const int & m);
		const tscore & biArcScore(const int & g, const int & h, const int & m);
		void initArcScore();
		void initGrandChildScore();
		bool initGrands(int level);

		void getOrUpdateStackScore(const int & h, const int & m, const int & amount);
		void getOrUpdateStackScore(const int & g, const int & h, const int & m, const int & amount);

	public:
		DepParser(const std::string & sFeatureInput, const std::string & sFeatureOut, int nState);
		~DepParser();

		void decode();
		void decodeArcs();

		void train(const DependencyTree & correct, const int & round);
		void parse(const Sentence & sentence, DependencyTree * retval);
		void work(DependencyTree * retval, const DependencyTree & correct);
	};
}

#endif
