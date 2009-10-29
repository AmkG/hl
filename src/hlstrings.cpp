#include"all_defines.hpp"

#include"types.hpp"

#include<boost/shared_ptr.hpp>
#include<boost/shared_array.hpp>

#include<algorithm>

typedef unsigned char BYTE;
typedef boost::shared_ptr<HlStringImpl> string_ptr;

#define ASCII_BUFFER_LEVEL 256
#define UTF8_BUFFER_LEVEL 32
#define ASCII_SWITCH_LEVEL UTF8_BUFFER_LEVEL

/*-----------------------------------------------------------------------------
HlString algorithms
-----------------------------------------------------------------------------*/

void HlString::enhash(HashingClass* hcp) const {
	if(hash_cache == 0) {
		HashingClass my_hasher;
		HlStringIter it(*pimpl);
		while(!it.at_end()) {
			my_hasher.enhash((*it).dat);
			++it;
		}
		hash_cache = my_hasher.c;
	}
	hcp->enhash(hash_cache);
}

bool HlString::is(Object::ref o) const {
	HlString* op = maybe_type<HlString>(o);
	if(!op) return false;
	HlString const& os = *op;
	if(pimpl->length() != os.pimpl->length()) return false;
	/*compare each character*/
	HlStringIter it1(*pimpl);
	HlStringIter it2(*pimpl);
	while(!it1.at_end()) {
		if(!(*it1 == *it2)) return false;
		++it1; ++it2;
	}
	return true;
}

/*-----------------------------------------------------------------------------
Implementations
-----------------------------------------------------------------------------*/

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
	if(likely(rv < 128)) {
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
	void to_cpp_string(std::string& into) const {
		into.insert(into.end(), start, &start[len]);
	}
	/*direct construction*/
	AsciiImpl(
			boost::shared_array<BYTE> const& nb,
			BYTE const* ns,
			size_t nl)
		: buffer(nb), start(ns), HlStringImpl(nl) { }
	/*empty construction*/
	AsciiImpl(void) : buffer(), start(0), HlStringImpl(0) { }
	/*typical construction*/
	static string_ptr create_from_buffer(BYTE const* b, BYTE const* e) {
		size_t nsz = e - b;
		boost::shared_array<BYTE> buffer(new BYTE[nsz]);
		std::copy(b, e, &buffer[0]);
		return string_ptr(
			new AsciiImpl(buffer, &buffer[0], nsz)
		);
	}
	friend class RopeImpl;
};

#define repeat(i) for(size_t repeat_macro_variable = (i); repeat_macro_variable != 0; --repeat_macro_variable)

class Utf8Impl : public HlStringImpl {
private:
	boost::shared_array<BYTE> const buffer;
	BYTE const* const start;
	BYTE const* const end;

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
	void to_cpp_string(std::string& into) const {
		into.insert(into.end(), start, end);
	}
	/*direct construction*/
	Utf8Impl(
			boost::shared_array<BYTE> const& nb,
			BYTE const* ns,
			BYTE const* ne,
			size_t nl)
		: buffer(nb), start(ns), end(ne), HlStringImpl(nl) { }
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

class RopeImpl : public HlStringImpl {
private:
	string_ptr const one;
	string_ptr const two;
	/*length of the string in one*/
	size_t const l1;
	/*depth of this rope*/
	size_t const depth;

	RopeImpl(void); // disallowed!

	/*direct construction, only allowed for friends*/
	RopeImpl(
			string_ptr const& n1,
			string_ptr const& n2,
			size_t nd,
			size_t nl1)
		: one(n1), two(n2), depth(nd), l1(nl1),
		  HlStringImpl(nl1 + n2->length()) { }

public:
	~RopeImpl() { }
	size_t rope_depth(void) const { return depth; }
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
			return;
		} else {
			/*both are cut*/
			string_ptr cut_one;
			one->cut(cut_one, i, l1 - i);
			string_ptr cut_two;
			two->cut(cut_two, 0, il_extent - l1);
			append_string_impl(into, cut_one, cut_two);
			return;
		}
	}
	void to_cpp_string(std::string& into) const {
		one->to_cpp_string(into);
		two->to_cpp_string(into);
	}
	static void append_string_impl(string_ptr& dest, string_ptr const& one, string_ptr const& two);
	static void base_append_string_impl( string_ptr& dest, string_ptr const& one, string_ptr const& two, size_t d1, size_t d2);
};

void RopeImpl::base_append_string_impl(
		string_ptr& dest,
		string_ptr const& one,
		string_ptr const& two,
		size_t d1,
		size_t d2) {
	size_t nd =
		d1 > d2 ?		d1 + 1 :
		/*otherwise*/		d2 + 1 ;
	dest.reset(
		new RopeImpl(one, two, nd, one->length())
	);
}

void RopeImpl::append_string_impl(
		string_ptr& dest,
		string_ptr const& one,
		string_ptr const& two) {
	/*trivial cases where one string is empty*/
	if(one->length() == 0) {
		dest = two;
		return;
	} else if(two->length() == 0){
		dest = one;
		return;
	}
	/*check for catting two short Ascii strings.*/
	AsciiImpl* as1 = dynamic_cast<AsciiImpl*>(one.get());
	if(as1 && as1->length() < (ASCII_BUFFER_LEVEL / 2)) {
		AsciiImpl* as2 = dynamic_cast<AsciiImpl*>(two.get());
		if(as2 && as2->length() < (ASCII_BUFFER_LEVEL / 2)) {
			size_t l1 = as1->length();
			size_t l2 = as2->length();
			/*create a single buffer and concat them*/
			boost::shared_array<BYTE> nbp(new BYTE(l1 + l2));
			std::copy(as2->start, &as2->start[l2],
				std::copy(as1->start, &as1->start[l1],
					&nbp[0]
				)
			);
			dest.reset(new AsciiImpl(nbp, &nbp[0], l1 + l2));
			return;
		}
	}
	size_t d1 = one->rope_depth();
	size_t d2 = two->rope_depth();
	if(d1 > d2 + 1) {
		/*first subtree is pretty heavy, check if:
		  one    two
		   /\     |
		  /\ c    d
		 a  b
		If so, do this:
		    /\
		   /  \
		  /\  /\
		 a b  c d
		*/
		RopeImpl& r1 = static_cast<RopeImpl&>(*one);
		size_t d1_2 = r1.two->rope_depth();
		if(d1_2 <= d2 + 1) {
			string_ptr tmp;
			base_append_string_impl(tmp, r1.two, two, d1_2, d2);
			append_string_impl(dest, r1.one, tmp);
			return;
		}
	}
	base_append_string_impl(dest, one, two, d1, d2);
}

/*alias for the RopeImpl static*/
static inline void append_string_impl(
		string_ptr& dest,
		string_ptr const& one,
		string_ptr const& two) {
	RopeImpl::append_string_impl(dest, one, two);
}

/*-----------------------------------------------------------------------------
Construction
-----------------------------------------------------------------------------*/

/*
On-stack creation
*/
void HlString::stack_create(Heap& hp, ProcessStack& stack, size_t N) {
	HlStringBuilderCore sb;
	/*iterate over the stack top, frop top(N) to top(1)*/
	for(size_t i = 0; i < N; ++i) {
		Object::ref cc = stack.top(N - i);
		if(!is_a<UnicodeChar>(cc)) {
			throw_HlError(
				"attempt to create string from non-character"
			);
		}
		sb.add(as_a<UnicodeChar>(cc));
	}
	/*pop*/
	stack.pop(N);
	HlString* sp = hp.create<HlString>();
	sb.inner(sp->pimpl);
	stack.push(Object::to_ref<Generic*>(sp));
}

/*
by appending two strings together
precondition:
	stack.top(2) = left string
	stack.top(1) = right string
postcondition:
	stack.top(1) = concatenated strings
*/
void HlString::append(Heap& hp, ProcessStack& stack) {
	HlString* psl = expect_type<HlString>(stack.top(2),
		"'string-join expects two strings, first argument is not a string"
	);
	HlString* psr = expect_type<HlString>(stack.top(1),
		"'string-join expects two strings, first argument is not a string"
	);
	HlString* pa = hp.create<HlString>();
	append_string_impl(pa->pimpl, psl->pimpl, psr->pimpl);
	stack.pop(1);
	stack.top(1) = Object::to_ref<Generic*>(pa);
}

/*
Construction of a HlString from a std::string
*/
void HlString::from_cpp_string(
		Heap& hp,
		ProcessStack& stack,
		std::string const& s) {
	/*create a new buffer to contain the string*/
	boost::shared_array<BYTE> buffer( new BYTE[s.size()] );
	BYTE* start = &buffer[0];
	BYTE* dest = start;
	/*get the string contents*/
	BYTE const* src = (BYTE const*) &*s.begin();
	/*keep track of the length of the string in unichars*/
	size_t len = 0;
	bool nonascii = false;
	/*copy into buffer*/
	while(src != (BYTE const*) &*s.end()) {
		BYTE const* p = src;
		utf8_adv_check(p, nonascii);
		std::copy(src, p, dest);
		size_t diff = p - src;
		dest += diff;
		src = p;
		++len;
	}
	string_ptr rv;
	/*were we ASCII or UTF-8?*/
	if(nonascii) {
		rv.reset(
			new Utf8Impl(buffer, start, dest, len)
		);
	} else {
		rv.reset(
			new AsciiImpl(buffer, start, len)
		);
	}
	/*now allocate on the given Heap*/
	HlString* sp = hp.create<HlString>();
	sp->pimpl = rv;
	stack.push(Object::to_ref<Generic*>(sp));
}

/*----------------------------------------------------------------------------
HlStringBuilder
----------------------------------------------------------------------------*/

void HlStringBuilderCore::build_prefix(void) {
	/*if there's nothing to build, there's nothing to build*/
	if(building_unichars == 0) return;
	/*construct temporary, then swap*/
	HlStringBuilderCore t;
	/*create a new string for the buffer*/
	if(utf8_mode) {
		t.prefix = Utf8Impl::create_from_buffer(
			&*building.begin(), &*building.end(), building_unichars
		);
	} else {
		t.prefix = AsciiImpl::create_from_buffer(
			&*building.begin(), &*building.end()
		);
	}
	/*prepend our prefix to the new one*/
	if(prefix) {
		append_string_impl(t.prefix, prefix, t.prefix);
	}
	/*now swap*/
	swap(t);
}

void HlStringBuilderCore::add(UnicodeChar uc) {
	uint32_t cval = uc.dat;
	if(likely(!utf8_mode)) {
		if(likely(cval < 128)) {
			/*pure ASCII so far...*/
			building.push_back((BYTE) cval);
			++building_unichars;
			if(unlikely(building_unichars > ASCII_BUFFER_LEVEL)) {
				build_prefix();
			}
			return;
		} else {
			/*oh noes, we've found a non-ASCII character!
			fine fine fine, switch to utf8-mode
			*/
			/*if we already have a bunch of ASCII
			characters pre-built, push them to the
			current prefix.
			*/
			if(building_unichars + 1 > ASCII_SWITCH_LEVEL) {
				build_prefix();
			}
			utf8_mode = 1;
		}
	}
	/*builder should be UTF-8 mode by now*/
	if(likely(cval < 128)) {
		building.push_back((BYTE) cval);
		++building_unichars;
	} else {
		/*first create into a temporary array before
		inserting into the building vector.  This is
		to atomize the insertion into the vector, at
		least in terms of exceptions - if an
		exception gets thrown while allocating
		storage, building_unichars doesn't get
		updated and we don't have partial utf8
		characters in the building vector.
		*/
		BYTE bs[4];
		if(cval < 0x800) {
			bs[0] = 0xC0 + (cval >> 6);
			bs[1] = 0x80 + (cval & 0x3F);
			building.insert(building.end(), &bs[0], &bs[2]);
		} else if(cval < 0x10000) {
			bs[0] = 0xE0 + (cval >> 12);
			bs[1] = 0x80 + ((cval >> 6) & 0x3F);
			bs[2] = 0x80 + (cval & 0x3F);
			building.insert(building.end(), &bs[0], &bs[3]);
		} else if(cval < 0x110000) {
			bs[0] = 0xF0 + (cval >> 18);
			bs[1] = 0x80 + ((cval >> 12) & 0x3F);
			bs[2] = 0x80 + ((cval >> 6) & 0x3F);
			bs[3] = 0x80 + (cval & 0x3F);
			building.insert(building.end(), &bs[0], &bs[4]);
		} else {
			throw_HlError("attempt to add invalid unicode character!");
		}
		++building_unichars;
	}
	/*if we hit the level, push it into the prefix*/
	if(unlikely(building_unichars > UTF8_BUFFER_LEVEL)) {
		build_prefix();
	}
}

void HlStringBuilderCore::add_s(string_ptr const& s) {
	build_prefix();
	/*concatenate and swap*/
	string_ptr tmp;
	append_string_impl(tmp, prefix, s);
	tmp.swap(prefix);
}
void HlStringBuilderCore::inner(string_ptr& s) {
	build_prefix();
	if(unlikely(!prefix)) {
		/*empty string!*/
		s.reset(new AsciiImpl);
	} else {
		s = prefix;
	}
}

/*-----------------------------------------------------------------------------
HlStringIter
-----------------------------------------------------------------------------*/

void HlStringIter::destruct(string_ptr& s, size_t& i) const {
	HlStringBufferPointer tmp_b = b;
	boost::shared_ptr<HlStringPath> tmp_p = p;
	HlStringBuilderCore sb;
	/*construct the prefix string*/
	while(!tmp_b.at_end()) {
		sb.add(*tmp_b);
		++tmp_b;
	}
	/*construct the concatenation of the strings*/
	while(tmp_p) {
		string_ptr tmp_s;
		HlStringPath::pop(tmp_s, tmp_p);
		sb.add_s(tmp_s);
	}
	sb.inner(s);
	i = 0;
}

