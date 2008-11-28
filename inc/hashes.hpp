#ifndef HASHES_H
#define HASHES_H

#include<stdint.h>

/*
From:
http://www.concentric.net/~Ttwang/tech/inthash.htm
http://www.burtleburtle.net/bob/hash/doobs.html
*/
static inline void mix(uint32_t& a, uint32_t& b, uint32_t& c) {
	a -= b; a -= c; a ^= (c>>13);
	b -= c; b -= a; b ^= (a<<8);
	c -= a; c -= b; c ^= (b>>13);
	a -= b; a -= c; a ^= (c>>12);
	b -= c; b -= a; b ^= (a<<16);
	c -= a; c -= b; c ^= (b>>5);
	a -= b; a -= c; a ^= (c>>3);
	b -= c; b -= a; b ^= (a<<10);
	c -= a; c -= b; c ^= (b>>15);
}

static inline uint32_t int_hash(uint32_t c) {
	uint32_t a, b;
	a = b = 0x9e3779b9;
	mix(a, b, c);
	return c;
}

#endif // HASHES_H

