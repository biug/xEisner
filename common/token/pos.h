#ifndef _POS_H
#define _POS_H
#include "token.h"

class TPOSTag {
protected:
	static Token tokenizer;

public:
	TPOSTag();
	~TPOSTag();

	static int code(const ttoken & s);
	static const ttoken & key(const int & token);
	static int count();
	static const int START = 1;

	static Token & getTokenizer();
};

inline TPOSTag::TPOSTag() = default;

inline TPOSTag::~TPOSTag() = default;

inline int TPOSTag::count() {
	return TPOSTag::tokenizer.count();
}

inline int TPOSTag::code(const ttoken & s) {
	return TPOSTag::tokenizer.lookup(s);
}

inline const ttoken & TPOSTag::key(const int & index) {
	return TPOSTag::tokenizer.key(index);
}

inline Token & TPOSTag::getTokenizer() {
	return tokenizer;
}

#endif
