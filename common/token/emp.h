#ifndef _EMP_H
#define _EMP_H
#include "token.h"

class TEmptyTag {
protected:
	static Token tokenizer;

public:
	TEmptyTag();
	~TEmptyTag();

	static int code(const ttoken & s);
	static const ttoken & key(const int & token);
	static int count();
	static const int start();
	static const int end();

	static Token & getTokenizer();
};

inline TEmptyTag::TEmptyTag() = default;

inline TEmptyTag::~TEmptyTag() = default;

inline int TEmptyTag::count() {
	return TEmptyTag::tokenizer.count();
}

inline const int TEmptyTag::start() {
	return TEmptyTag::tokenizer.start();
}

inline const int TEmptyTag::end() {
	return TEmptyTag::tokenizer.end();
}

inline int TEmptyTag::code(const ttoken & s) {
	return TEmptyTag::tokenizer.lookup(s);
}

inline const ttoken & TEmptyTag::key(const int & index) {
	return TEmptyTag::tokenizer.key(index);
}

inline Token & TEmptyTag::getTokenizer() {
	return tokenizer;
}

#endif
