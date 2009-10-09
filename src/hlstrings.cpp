#include"all_defines.hpp"

#include"types.hpp"

#include<boost/shared_ptr.hpp>
#include<boost/shared_array.hpp>

#include<algorithm>

typedef unsigned char BYTE;
typedef boost::shared_ptr<HlStringImpl> string_ptr;

/*advances the specified BYTE* */
static inline void utf8_adv(BYTE const*& p) {
	BYTE b = *p;
	if(likely(b < 128)) { ++p; }
	else if(b >= 240) { p += 4; }
	else if(b >= 224) { p += 3; }
	else if(b >= 192) { p += 2; }
	else throw_HlError("utf8 string traversal error");
}
static inline void utf8_adv(BYTE*& p) {
	BYTE b = *p;
	if(likely(b < 128)) { ++p; }
	else if(b >= 240) { p += 4; }
	else if(b >= 224) { p += 3; }
	else if(b >= 192) { p += 2; }
	else throw_HlError("utf8 string traversal error");
}
/*advances the specified BYTE* and also sets the flag for non-ascii*/
static inline void utf8_adv_check(BYTE const*& p, bool& nonascii) {
	BYTE b = *p;
	if(likely(b < 128)) { ++p; }
	else if(b >= 240) { p += 4; nonascii = true; }
	else if(b >= 224) { p += 3; nonascii = true; }
	else if(b >= 192) { p += 2; nonascii = true; }
	else throw_HlError("utf8 string traversal error");
}
static inline void utf8_adv_check(BYTE*& p, bool& nonascii) {
	BYTE b = *p;
	if(likely(b < 128)) { ++p; }
	else if(b >= 240) { p += 4; nonascii = true; }
	else if(b >= 224) { p += 3; nonascii = true; }
	else if(b >= 192) { p += 2; nonascii = true; }
	else throw_HlError("utf8 string traversal error");
}
static inline uint32_t extract_utf8(unsigned char c) { /*Pure*/
	return c & 63;
}
/*reads the specified utf8 character*/
UnicodeChar utf8_read(BYTE const* start) {
	BYTE rv = *start;
	if(rv < 128) {
		 return UnicodeChar((char)rv);
	} else if(rv >= 240) {
		return UnicodeChar(
			((uint32_t)rv - 240) * 262144
			+ extract_utf8(*(start + 1)) * 4096
			+ extract_utf8(*(start + 2)) * 64
			+ extract_utf8(*(start + 3))
		);
	} else if(rv >= 224) {
		return UnicodeChar(
			((uint32_t)rv - 224) * 4096
			+ extract_utf8(*(start + 1)) * 64
			+ extract_utf8(*(start + 2))
		);
	} else if(rv >= 192) {
		return UnicodeChar(
			((uint32_t)rv - 192) * 64
			+ extract_utf8(*(start + 1))
		);
	}
	throw_HlError("utf8 string read error");
}

class AsciiImpl : public HlStringImpl {
private:
	boost::shared_array<BYTE> const buffer;
	BYTE const* const start;
	size_t const len;

public:
	~AsciiImpl() { }
	void point_at(
			HlStringBufferPointer& b,
			boost::shared_ptr<HlStringPath>& p,
			size_t i) const {
		b.buffer = buffer;
		b.start = start + i;
		b.end = start + len;
	}
	size_t length(void) const { return len; }
	UnicodeChar ref(size_t i) const {
		return UnicodeChar(
			(char) start[i]
		);
	}
	void cut(string_ptr& into, size_t i, size_t l) const {
		into.reset(
			new AsciiImpl(buffer, start + i, l)
		);
	}
	/*direct construction*/
	AsciiImpl(
			boost::shared_array<BYTE> const& nb,
			BYTE const* ns,
			size_t nl)
		: buffer(nb), start(ns), len(nl) { }
	/*empty construction*/
	AsciiImpl(void) : buffer(), start(0), len(0) { }
	/*typical construction*/
	static string_ptr create_from_buffer(BYTE const* b, BYTE const* e) {
		size_t nsz = e - b;
		boost::shared_array<BYTE> buffer(new BYTE[nsz]);
		std::copy(b, e, &buffer[0]);
		return string_ptr(
			new AsciiImpl(buffer, &buffer[0], nsz)
		);
	}
};

#define repeat(i) for(size_t repeat_macro_variable = (i); repeat_macro_variable != 0; --repeat_macro_variable)

class Utf8Impl : public HlStringImpl {
private:
	boost::shared_array<BYTE> const buffer;
	BYTE const* const start;
	BYTE const* const end;
	size_t const len;

	Utf8Impl(void); // empty construction is disallowed!

public:
	~Utf8Impl() { }
	void point_at(
			HlStringBufferPointer& b,
			boost::shared_ptr<HlStringPath>& p,
			size_t i) const {
		/*have to iteratively skip the first part*/
		BYTE const* nstart = start;
		if(unlikely(i != 0)) {
			do {
				utf8_adv(nstart);
				--i;
			} while(i != 0);
		}
		b.buffer = buffer;
		b.start = nstart;
		b.end = end;
	}
	size_t length(void) const { return len; }
	UnicodeChar ref(size_t i) const {
		BYTE const* nstart = start;
		repeat(i) utf8_adv(nstart);
		return utf8_read(nstart);
	}
	void cut(string_ptr& into, size_t i, size_t l) const {
		/*get the start*/
		BYTE const* nstart = start;
		repeat(i) utf8_adv(nstart);
		/*for the trivial case where we would reach
		the end of this string, don't bother
		iterating over the rest of the string
		*/
		if(i + l == len) {
			into.reset(
				new Utf8Impl(buffer, nstart, end, l)
			);
			return;
		}
		/*iterate over the substring.  since we are
		iterating over the substring anyway, we might
		as well check if the substring is pure ascii.
		*/
		bool nonascii = false;
		BYTE const* nend = nstart;
		repeat(l) utf8_adv_check(nend, nonascii);
		if(nonascii) {
			into.reset(
				new Utf8Impl(buffer, nstart, nend, l)
			);
			return;
		} else {
			into.reset(
				new AsciiImpl(buffer, nstart, l)
			);
			return;
		}
	}
	/*direct construction*/
	Utf8Impl(
			boost::shared_array<BYTE> const& nb,
			BYTE const* ns,
			BYTE const* ne,
			size_t nl)
		: buffer(nb), start(ns), end(ne), len(nl) { }
	/*typical construction*/
	static string_ptr create_from_buffer(
			BYTE const* b,
			BYTE const* e,
			size_t l) {
		size_t num_bytes = e - b;
		boost::shared_array<BYTE> buffer(new BYTE[num_bytes]);
		std::copy(b, e, &buffer[0]);
		return string_ptr(
			new Utf8Impl(buffer, &buffer[0], &buffer[num_bytes], l)
		);
	}
};

void append_string_impl(string_ptr& dest, string_ptr const& one, string_ptr& two);

class RopeImpl : public HlStringImpl {
private:
	string_ptr const one;
	string_ptr const two;
	/*length of one*/
	size_t const l1;
	/*total length*/
	size_t const len;

	RopeImpl(void); // disallowed!

public:
	~RopeImpl() { }
	void point_at(
			HlStringBufferPointer& b,
			boost::shared_ptr<HlStringPath>& p,
			size_t i) const {
		if(likely(i < l1)) {
			/*push the second string onto the string path, to
			be traversed after the first one.
			*/
			HlStringPath::push(two, p);
			one->point_at(b, p, i);
		} else {
			/*the pointed value is after the first string,
			so point only within the second string,
			ignoring the first.
			*/
			two->point_at(b, p, i - l1);
		}
	}
	size_t length(void) const { return len; }
	UnicodeChar ref(size_t i) const {
		if(i < l1) {
			return one->ref(i);
		} else {
			return two->ref(i - l1);
		}
	}
	void cut(string_ptr& into, size_t i, size_t l) const {
		size_t il_extent = i + l;
		/*check for trivial cases*/
		if(il_extent <= l1) {
			/*completely within one*/
			one->cut(into, i, l);
			return;
		} else if(i >= l1) {
			/*completely within two*/
			two->cut(into, i - l1, l);
			return;
		} else if(i == 0) {
			/*contains one uncut, but with two possibly cut*/
			string_ptr cut_two;
			two->cut(cut_two, 0, il_extent - l1);
			append_string_impl(into, one, cut_two);
			return;
		} else if(il_extent == len) {
			/*one is cut, but followed by the entire two*/
			string_ptr cut_one;
			one->cut(cut_one, i, l1 - i);
			append_string_impl(into, cut_one, two);
			return
		} else {
			/*both are cut*/
			string_ptr cut_one;
			one->cut(cut_one, i, l1 - i);
			string_ptr cut_two;
			two->cut(cut_two, 0, il_extent - l1)
			append_string_impl(into, cut_one, cut_two);
		}
	}
};

