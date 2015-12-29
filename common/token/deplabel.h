#ifndef _DEPLABEL_H
#define _DEPLABEL_H
#include "token.h"

class TDepLabel {
protected:
	static Token tokenizer;

public:
	TDepLabel();
	~TDepLabel();

	static int code(const ttoken & s);
	static const ttoken & key(const int & token);
	static int count();
	static const int START = 1;
	static void add(const ttoken & s);

	static Token & getTokenizer();
};

inline TDepLabel::TDepLabel() = default;

inline TDepLabel::~TDepLabel() = default;

inline int TDepLabel::count() {
	return TDepLabel::tokenizer.count();
}

inline int TDepLabel::code(const ttoken & s) {
	return TDepLabel::tokenizer.lookup(s);
}

inline const ttoken & TDepLabel::key(const int & index) {
	return TDepLabel::tokenizer.key(index);
}

inline Token & TDepLabel::getTokenizer() {
	return tokenizer;
}

inline void TDepLabel::add(const ttoken & s) {
	tokenizer.add(s);
}

#endif
