#ifndef _EISNER3RD_DEPPARSER_H
#define _EISNER3RD_DEPPARSER_H

#include <vector>
#include <unordered_set>

#include "eisner3rd_state.h"
#include "eisner3rd_weight.h"
#include "common/parser/depparser_base.h"

namespace eisner3rd {

	class DepParser : public DepParserBase {
	private:
		static WordPOSTag empty_taggedword;
		static WordPOSTag start_taggedword;
		static WordPOSTag end_taggedword;

		StateItem m_lItems[MAX_SENTENCE_SIZE][MAX_SENTENCE_SIZE];
		WordPOSTag m_lSentence[MAX_SENTENCE_SIZE];
		std::vector<Arc> m_vecCorrectArcs;
		std::vector<TriArc> m_vecCorrectTriArcs;
		std::vector<Arc> m_vecTrainArcs;
		std::vector<TriArc> m_vecTrainTriArcs;
		int m_nSentenceLength;

		tscore m_nRetval;
		tscore m_lFirstOrderScore[MAX_SENTENCE_SIZE << 1];
		tscore m_lSecondOrderScore[(MAX_SENTENCE_SIZE << MAX_SENTENCE_BITS) << 1];

		int m_nDis, m_nDir;

		Word p_word, c_word, c2_word, c3_word;
		POSTag p_tag, c_tag, c2_tag, c3_tag;

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

		std::unordered_set<BiGram<int>> m_setFirstGoldScore;
		std::unordered_set<TriGram<int>> m_setSecondGoldScore;
		std::unordered_set<QuarGram<int>> m_setThirdGoldScore;

		void update();
		void generate(DependencyTree * retval, const DependencyTree & correct);
		void goldCheck();

		const tscore & arcScore(const int & p, const int & c);
		const tscore & twoArcScore(const int & p, const int & c, const int & c2);
		const tscore & triArcScore(const int & p, const int & c, const int & c2, const int & c3);
		void initFirstOrderScore(const int & d);
		void initSecondOrderScore(const int & d);

		void getOrUpdateStackScore(const int & p, const int & c, const int & amount);
		void getOrUpdateStackScore(const int & p, const int & c, const int & c2, const int & amount);
		void getOrUpdateStackScore(const int & p, const int & c, const int & c2, const int & c3, const int & amount);

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
