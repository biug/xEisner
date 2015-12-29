#ifndef _WORD_H
#define _WORD_H
#include "token.h"

class TWord {
protected:
	static Token tokenizer;

public:
	TWord();
	~TWord();

	static int code(const ttoken & s);
	static const ttoken & key(const int & token);
	static int count();
	static const int START = 1;

	static Token & getTokenizer();
};

inline TWord::TWord() = default;

inline TWord::~TWord() = default;

inline int TWord::code(const ttoken & s) {
	return tokenizer.lookup(s);
}

inline int TWord::count() {
	return TWord::tokenizer.count();
}

inline const ttoken & TWord::key(const int & index) {
	return TWord::tokenizer.key(index);
}

inline Token & TWord::getTokenizer() {
	return tokenizer;
}

#endif
