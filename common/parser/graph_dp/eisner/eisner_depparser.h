#ifndef _EISNER_DEPPARSER_H
#define _EISNER_DEPPARSER_H

#include <vector>
#include <unordered_set>

#include "eisner_state.h"
#include "eisner_weight.h"
#include "common/parser/depparser_base.h"

namespace eisner {
	class DepParser : public DepParserBase {
	private:
		static WordPOSTag empty_taggedword;
		static WordPOSTag start_taggedword;
		static WordPOSTag end_taggedword;

		StateItem m_lItems[MAX_SENTENCE_SIZE][MAX_SENTENCE_SIZE];
		WordPOSTag m_lSentence[MAX_SENTENCE_SIZE];
		std::vector<Arc> m_vecCorrectArcs;
		std::vector<Arc> m_vecTrainArcs;
		int m_nSentenceLength;

		tscore m_nRetval;
		tscore m_lFirstOrderScore[MAX_SENTENCE_SIZE << 1];

		int m_nDis, m_nDir;

		Word p_word, c_word;
		POSTag p_tag, c_tag;

		WordInt word_int;
		POSTagInt tag_int;

		POSTag p_1_tag, p1_tag, c_1_tag, c1_tag, b_tag;

		TwoWordsInt word_word_int;
		POSTagSet2Int tag_tag_int;
		WordPOSTagInt word_tag_int;

		POSTagSet3Int tag_tag_tag_int;
		WordPOSTagPOSTagInt word_tag_tag_int;
		WordWordPOSTagInt word_word_tag_int;

		POSTagSet4Int tag_tag_tag_tag_int;
		WordWordPOSTagPOSTagInt word_word_tag_tag_int;

		std::unordered_set<BiGram<int>> m_setFirstGoldScore;

		void update();
		void generate(DependencyTree * retval, const DependencyTree & correct);
		void goldCheck();

		tscore arcScore(const int & p, const int & c);
		void initFirstOrderScore(const int & d);

		void getOrUpdateStackScore(const int & p, const int & c, const int & amount = 0);

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
