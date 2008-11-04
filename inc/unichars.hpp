#ifndef UNICHARS_H
#define UNICHARS_H

#include<stdint.h>

class UnicodeChar {
private:
	UnicodeChar(); //disallowed!
	typedef char intptr_tIsTooSmallIfThisCausesAnError[
		1 - 2 * (sizeof(intptr_t) < sizeof(uint32_t))];
public:
	uint32_t dat;
	explicit UnicodeChar(uint32_t x) : dat(x) { }
	explicit UnicodeChar(char x) : dat(x) { }
	/*insert routines for conversion to/from a set of chars here*/
};

#endif //UNICHARS_H

