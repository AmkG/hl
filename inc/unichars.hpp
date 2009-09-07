#ifndef UNICHARS_H
#define UNICHARS_H

#include<stdint.h>

class Process;

class UnicodeChar {
private:
	UnicodeChar(); //disallowed!
	typedef char intptr_tIsTooSmallIfThisCausesAnError[
		1 - 2 * (sizeof(intptr_t) < sizeof(uint32_t))];
public:
	uint32_t dat;
	explicit UnicodeChar(uint32_t x) : dat(x) { }
	explicit UnicodeChar(char x) : dat(x) { }
	bool operator==(UnicodeChar const& o) const { return dat == o.dat; }

	// pushes in the process stack a number (a fixnum or a big int) representing
	// this char code in UTF-8
	// may allocate
	void push_as_int(Process & proc);

	/*insert routines for conversion to/from a set of chars here*/
};

#endif //UNICHARS_H

