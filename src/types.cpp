#include "all_defines.hpp"
#include "types.hpp"
#include "processes.hpp"
#include "executors.hpp"

#include <iostream>

/*-----------------------------------------------------------------------------
Cons
-----------------------------------------------------------------------------*/

Object::ref Cons::len() {
  int l = 1;
  Object::ref next = cdr();
  Object::ref p2 = next;
  while (maybe_type<Cons>(next)) {
    next = ::cdr(next);
    p2 = ::cdr(::cdr(p2));
    if(next == p2 && next != Object::nil()) return Object::to_ref(0); // fail on infinite lists
    l++;
  }
  // !! there could be an overflow, but we don't expect to have lists so long
  return Object::to_ref(l);
}

/*-----------------------------------------------------------------------------
Closure
-----------------------------------------------------------------------------*/

Closure* Closure::NewClosure(Heap & h, size_t n) {
  Closure *c = h.create_variadic<Closure>(n);
  c->body = Object::nil();
  c->nonreusable = true;
  c->kontinuation = false;
  return c;
}

Closure* Closure::NewKClosure(Heap & h, ProcessStack & stack, size_t n) {
  Closure *c = h.lifo_create_variadic<Closure>(n);
  c->body = Object::nil();
	if (stack.size() > 0 && maybe_type<Closure>(stack[0])) {
		c->owner = stack[0];
	}
	if (stack.size() > 1 && maybe_type<Closure>(stack[1])) {
		c->next_k = stack[1];
	}
  c->nonreusable = false;
  c->kontinuation = true;
  return c;
}

Closure* Closure::NewKClosure(Heap & h, size_t n) {
  Closure *c = h.lifo_create_variadic<Closure>(n);
  c->body = Object::nil();
  c->nonreusable = false;
  c->kontinuation = true;
  return c;
}

void Closure::print_trace(std::ostream & o) {
	if (kontinuation) {
		if (owner != Object::nil()) {
			Closure *c = expect_type<Closure>(owner);
			if (c->kontinuation) {
				// walk up continuation chain
				c->print_trace(o);
			} else {
				o << "\tCalled by ";
				expect_type<Bytecode>(c->body)->print_info(o);
				o << "\n";
				if (next_k != Object::nil()) {
					expect_type<Closure>(next_k)->print_trace(o);
				}
			}
		} else {
			o << "\tno info";
		}
	} else {
		o << "\tCalled by ";
		expect_type<Bytecode>(body)->print_info(o);
		o << "\n";
	}
}

/*-----------------------------------------------------------------------------
HlString
-----------------------------------------------------------------------------*/

void HlString::sref(Heap& hp, ProcessStack& stack) {
	/*first check the inputs*/
	HlString* Tp = expect_type<HlString>(stack.top(3),
				"'string-sref expects a string as first argument");
	if(!is_a<UnicodeChar>(stack.top(2))) {
		throw_HlError("'string-sref expects a character as second argument");
	}
	/*TODO: When BigInts are implemented, consider changing
	this to support BigInts
	*/
	if(!is_a<int>(stack.top(1))) {
		throw_HlError("'string-sref expects an integer as third argument");
	}
	HlStringImpl* Sp = known_type<HlStringImpl>(Tp->impl);
	if(Sp->shared) {
		/*create a new copy!*/
		HlStringImpl* nSp = hp.create_variadic<HlStringImpl>(Sp->size());
		/*revive Tp and Sp*/
		Tp = known_type<HlString>(stack.top(3));
		Sp = known_type<HlStringImpl>(Tp->impl);
		for(size_t i = 0; i < Sp->size(); ++i) {
			(*nSp)[i] = (*Sp)[i];
		}
		Tp->impl = Object::to_ref(nSp);
		Sp = nSp;
	}
	size_t ind = as_a<int>(stack.top(1));
	if(ind >= Sp->size()) {
		throw_HlError("'string-sref index out of bounds");
	}
	(*Sp)[ind] = stack.top(2);
	stack.top(3) = stack.top(2);
	stack.pop(2);
}

void HlString::stack_create(Heap& hp, ProcessStack& stack, size_t N) {
	/*first, create the implementation*/
	HlStringImpl* Sp = hp.create_variadic<HlStringImpl>(N);
	/*fill in from the stack*/
	for(size_t i = 0; i < N; ++i) {
		if(is_a<UnicodeChar>(stack.top(N-i))) {
			(*Sp)[i] = stack.top(N-i);
		} else {
			throw_HlError("'string bytecode expects characters for all parameters");
		}
	}
	stack.pop(N);
	/*push Sp onto the stack*/
	stack.push(Object::to_ref(Sp));
	/*create the PImpl*/
	HlString* Tp = hp.create<HlString>();
	Tp->impl = stack.top();
	stack.top() = Object::to_ref(Tp);
}

bool HlString::is(Object::ref o) const {
	HlString* hs = maybe_type<HlString>(o);
	if(hs) {
		if(hs->impl == impl) return true;
		if(hs->size() != size()) return false;
		HlStringImpl& S1 = *known_type<HlStringImpl>(impl);
		HlStringImpl& S2 = *known_type<HlStringImpl>(hs->impl);
		for(size_t i = 0; i < size(); ++i) {
			if(S1[i] != S2[i]) {
				return false;
			}
		}
		return true;
	} else return false;
}

void HlString::enhash(HashingClass* hc) const {
	if(impl) {
		HlStringImpl& S = *known_type<HlStringImpl>(impl);
		for(size_t i = 0; i < size(); ++i) {
			hc->enhash(as_a<UnicodeChar>(S[i]).dat);
		}
	}
}

std::string HlString::to_cpp_string(void) const {
	if(impl) {
		HlStringImpl& S = *known_type<HlStringImpl>(impl);
		std::string rv = "";
		char utf8c[4];
		/*convert to UTF-8 encoding*/
		for(size_t i = 0; i < size(); ++i) {
			UnicodeChar uc = as_a<UnicodeChar>(S[i]);
			uint32_t c = uc.dat;
			if(c < 128) {
				utf8c[0] = c;
				rv.append(utf8c, 1);
			} else if(c < 2048) {
				utf8c[0] = 192 + c / 64;
				utf8c[1] = 128 + c % 64;
				rv.append(utf8c, 2);
			} else if(c < 65536) {
				utf8c[0] = 224 + c / 4096;
				utf8c[1] = 128 + ((c / 64) % 64);
				utf8c[2] = 128 + c % 64;
				rv.append(utf8c, 3);
			} else if(c < 2097152) {
				utf8c[0] = 240 + c / 262144;
				utf8c[1] = 128 + ((c / 4096) % 64);
				utf8c[2] = 128 + ((c / 64) % 64);
				utf8c[3] = 128 + c % 64;
				rv.append(utf8c, 4);
			} /*bigger characters are not valid UTF8*/
		}
		return rv;
	} else return std::string("");
}

static inline bool partial_utf8_char(unsigned char c) { /*Pure*/
	return c >= 128 && c < 192;
}
static inline uint32_t extract_utf8(unsigned char c) { /*Pure*/
	return c & 63;
}

void HlString::from_cpp_string(
		Heap& hp,
		ProcessStack& stack,
		std::string s) {
	/*Generate a vector of UnicodeChar's first*/
	std::vector<UnicodeChar> rv;
	size_t sz = s.size();
	for(size_t i = 0; i < sz; ++i) { // not foreach-able
		unsigned char c = s[i];
		/*TODO*/
		if(c > 128) {
			if(c >= 240 && i + 3 < sz
					/*check succeeding 3 bytes are
					ensure correct range
					*/
					&& partial_utf8_char(s[i+1])
					&& partial_utf8_char(s[i+2])
					&& partial_utf8_char(s[i+3])
					) {
				/*push back a single code point and skip
				three bytes.
				*/
				rv.push_back(
					UnicodeChar(
						((uint32_t)c) * 262144
						+ extract_utf8(s[i+1]) * 4096
						+ extract_utf8(s[i+2]) * 64
						+ extract_utf8(s[i+3])
					)
				);
				i += 3;
			} else if(c >= 224 && i + 2 < sz
					&& partial_utf8_char(s[i+1])
					&& partial_utf8_char(s[i+2])
					) {
				/*push back and skip*/
				rv.push_back(
					UnicodeChar(
						((uint32_t)c) * 4096
						+ extract_utf8(s[i+1]) * 64
						+ extract_utf8(s[i+2])
					)
				);
				i += 2;
			} else if(c >= 192 && i + 1 < sz
					&& partial_utf8_char(s[i+1])
					) {
				rv.push_back(
					UnicodeChar(
						((uint32_t)c) * 64
						+ extract_utf8(s[i+1])
					)
				);
				i += 1;
			} /*else ignore character*/
		} else {
			rv.push_back(UnicodeChar((char)c));
		}
	}
	/*convert vector*/
	HlStringImpl* si = hp.create_variadic<HlStringImpl>(rv.size());
	for(size_t i = 0; i < rv.size(); ++i) {
		(*si)[i] = Object::to_ref(rv[i]);
	}
	stack.push(Object::to_ref<Generic*>(si));
	HlString* rvs = hp.create<HlString>();
	rvs->impl = stack.top();
	stack.top() = Object::to_ref<Generic*>(rvs);
}

/*-----------------------------------------------------------------------------
HlTable
-----------------------------------------------------------------------------*/

#include"hashes.hpp"

HashingClass::HashingClass(void)
	: a(0x9e3779b9), b(0x9e3779b9), c(0) { }
void HashingClass::enhash(size_t x) {
	c = c ^ (uint32_t)x;
	mix(a, b, c);
}

/*is-compatible hashing*/
size_t hash_is(Object::ref o) {
	/*if it's not a heap object, hash directly*/
	if(!is_a<Generic*>(o)) {
		/*32-bit, even on 64-bit systems; shouldn't be
		a problem in practice, since we don't expect hash
		tables to have more than 4,294,967,296 entries.
		At least not *yet*.
		*/
		return (size_t) int_hash((uint32_t) o.dat);
	} else {
		HashingClass hc;
		as_a<Generic*>(o)->enhash(&hc);
		return (size_t) hc.c;
	}
}

inline Object::ref* HlTable::linear_lookup(Object::ref k) const {
	HlArray& A = *known_type<HlArray>(impl);
	for(size_t i = 0; i < pairs; ++i) {
		if(::is(k, A[i * 2])) {
			return &A[i * 2 + 1];
		}
	}
	return NULL;
}

inline Object::ref* HlTable::arrayed_lookup(Object::ref k) const {
	if(!is_a<int>(k)) return NULL;
	HlArray& A = *known_type<HlArray>(impl);
	int x = as_a<int>(k);
	if(x < 0 || x >= A.size()) return NULL;
	return &A[x];
}

inline Object::ref* HlTable::hashed_lookup(Object::ref k) const {
	HlArray& A = *known_type<HlArray>(impl);
	size_t I = hash_is(k) % A.size();
	size_t J = I;
	Object::ref key;
loop:
	if(!A[I]) return NULL;
	if(::is(k, car(A[I]))) {
		return &known_type<Cons>(A[I])->cdr();
	}
	++I; if(I > A.size()) I = 0;
	if(J == I) return NULL; //wrapped already
	goto loop;
}

Object::ref* HlTable::location_lookup(Object::ref k) const {
	if(tbtype == hl_table_empty) return NULL;
	switch(tbtype) {
	case hl_table_linear:
		return linear_lookup(k);
		break;
	case hl_table_arrayed:
		return arrayed_lookup(k);
		break;
	case hl_table_hashed:
		return hashed_lookup(k);
		break;
	}
}

/*Inserts a key-value cons pair to the correct hashed
location in the specified array.
*/
static inline void add_kv_to_array(HlArray& A, Cons& kv, size_t sz) {
	size_t I = hash_is(kv.car()) % sz;
find_loop:
	if(!A[I]) {
		A[I] = Object::to_ref(&kv);
		return;
	}
	++I; if(I >= sz) I = 0;
	goto find_loop;
}


void HlTable::insert(Heap& hp, ProcessStack& stack) {
	/*creates a copy of the key in stack.top(1) if it's
	a string key
	*/
	HlString* sp = maybe_type<HlString>(stack.top(1));
	if(sp) {
		/*create a new string*/
		HlString& NS = *hp.create<HlString>();
		/*re-read old string*/
		HlString& S = *known_type<HlString>(stack.top(1));
		NS.impl = S.impl;
		if(S.impl) {
			HlStringImpl& I = *known_type<HlStringImpl>(S.impl);
			I.shared = true;
		}
		stack.top(1) = Object::to_ref(&NS);
	}

	HlTable& T = *known_type<HlTable>(stack.top(3));
	if(T.tbtype == hl_table_empty) {
		/*either we become an hl_table_arrayed,
		or a hl_table_linear
		*/
		Object::ref k = stack.top(1);
		/*maybe we become arrayed*/
		if(is_a<int>(k)) {
			int I = as_a<int>(k);
			if(I >= 0 && I < ARRAYED_LEVEL) {
				HlArray& A = *hp.create_variadic<HlArray>(ARRAYED_LEVEL);
				A[I] = stack.top(2);
				/*need to re-read, we might have gotten GC'ed*/
				HlTable& T = *known_type<HlTable>(stack.top(3));
				T.impl = Object::to_ref(&A);
				T.tbtype = hl_table_arrayed;
				goto clean_up;
			}
		}
		/*nah, we have to become linear*/
		HlArray& A = *hp.create_variadic<HlArray>(LINEAR_LEVEL * 2);
		A[0] = stack.top(1); /*key*/
		A[1] = stack.top(2); /*value*/
		/*need to re-read, we might have gotten GC'ed*/
		HlTable& T = *known_type<HlTable>(stack.top(3));
		T.impl = Object::to_ref(&A);
		T.tbtype = hl_table_linear;
		T.pairs = 1;
		goto clean_up;
	} else {
		/*if the key already exists, we just have
		to replace its value
		*/
		Object::ref* op = T.location_lookup(stack.top(1));
		if(op) {
			*op = stack.top(2);
			goto clean_up;
		}

		/*not a value replacement.  Well, if the given value is
		nil, we don't actually insert anything
		Note that this checking needs to be done after we
		determine we're not replacing a value
		*/
		if(!stack.top(2)) goto clean_up;

		/*No, we have to insert it.  First figure out what to do*/
		switch(T.tbtype) {
		case hl_table_arrayed:
			/*Can we still insert into the array?*/
			if(is_a<int>(stack.top(1))) {
				int k = as_a<int>(stack.top(1));
				if(k >= 0) {
					HlArray& A = *known_type<HlArray>(
						T.impl
					);
					size_t sz = A.size();
					if(k < sz) {
						A[k] = stack.top(2);
						goto clean_up;
					} else if(k < sz + ARRAYED_LEVEL) {
						/*copy the implementation into
						a new array.
						*/
						HlArray& NA =
							*hp.create_variadic
							<HlArray>(
							sz + ARRAYED_LEVEL
						);
						/*re-read*/
						HlTable& T =
							*known_type<HlTable>(
							stack.top(3)
						);
						HlArray& OA =
							*known_type<HlArray>(
							T.impl
						);
						for(size_t i = 0; i < sz; ++i)
						{
							NA[i] = OA[i];
						}
						NA[k] = stack.top(2);
						T.impl = Object::to_ref(&NA);
						goto clean_up;
					}
				}
			}
			/*failed to insert... transform into hashed*/
			goto transform_arrayed_to_hashed;
			break;
		case hl_table_linear:
			/*check if there's still space*/
			if(T.pairs < LINEAR_LEVEL) {
				int i = T.pairs;
				HlArray& A = *known_type<HlArray>(T.impl);
				A[i * 2] = stack.top(1); /*key*/
				A[i * 2 + 1] = stack.top(2); /*value*/
				T.pairs++;
				goto clean_up;
			}
			/*failed to insert... transform to hashed*/
			goto transform_linear_to_hashed;
		case hl_table_hashed:
			/*first, determine if we have to re-hash*/
			HlArray& A = *known_type<HlArray>(T.impl);
			if(T.pairs < A.size() / 2) {
				/*no re-hash, just insert*/
				/*first create Cons cell for k-v pair*/
				Cons* cp = hp.create<Cons>();
				cp->car() = stack.top(1);
				cp->cdr() = stack.top(2);
				/*re-read*/
				HlTable& T = *known_type<HlTable>(stack.top(3));
				HlArray& A = *known_type<HlArray>(T.impl);

				add_kv_to_array(A, *cp, A.size());
				++T.pairs;
				goto clean_up;
			}
			goto rehash;
		}
	}
transform_arrayed_to_hashed: {
	/*decide what the new size must be*/
	HlTable& T = *known_type<HlTable>(stack.top(3));
	HlArray& A = *known_type<HlArray>(T.impl);
	size_t old_size = A.size();
	size_t new_size = 4 * old_size;

	/*create a cons pair for the current k-v*/
	Cons* cp = hp.create<Cons>();
	cp->car() = stack.top(1);
	cp->cdr() = stack.top(2);
	stack.top(1) = Object::to_ref(cp);
	/*create a new array*/
	HlArray* ap = hp.create_variadic<HlArray>(new_size);
	stack.top(2) = Object::to_ref(ap);

	/*enhash*/
	T.pairs = 0;
	for(size_t i = 0; i < old_size; ++i) {
		HlTable& T = *known_type<HlTable>(stack.top(3));
		HlArray& A = *known_type<HlArray>(T.impl);
		if(A[i]) {
			Cons& newkv = *hp.create<Cons>();
			HlTable& T = *known_type<HlTable>(stack.top(3));
			HlArray& A = *known_type<HlArray>(T.impl);
			newkv.car() = Object::to_ref((int) i);
			newkv.cdr() = A[i];
			add_kv_to_array(*known_type<HlArray>(stack.top(2)),
				newkv,
				new_size
			);
			++T.pairs;
		}
	}
	/*insert new key*/
	add_kv_to_array(*known_type<HlArray>(stack.top(2)),
		*known_type<Cons>(stack.top(1)),
		new_size
	);
	{HlTable& T = *known_type<HlTable>(stack.top(3));
		++T.pairs;
		/*replace implementation*/
		T.impl = stack.top(2);
		T.tbtype = hl_table_hashed;
	}
	/*clean up stack*/
	stack.top(3) = cdr(stack.top(1));
	stack.pop(2);
	return;
}
transform_linear_to_hashed: {
	/*create a cons pair for the current k-v*/
	Cons* cp = hp.create<Cons>();
	cp->car() = stack.top(1);
	cp->cdr() = stack.top(2);
	stack.top(1) = Object::to_ref(cp);
	/*create a new array*/
	HlArray* ap = hp.create_variadic<HlArray>(
		4 * LINEAR_LEVEL
	);
	stack.top(2) = Object::to_ref(ap);

	/*enhash*/
	known_type<HlTable>(stack.top(3))->pairs = 0;
	for(size_t i = 0; i < LINEAR_LEVEL; ++i) {
		/*have to read it in each time, because
		of GC clobbering pointers
		NOTE: can probably be rearranged somewhat
		*/
		HlTable& T = *known_type<HlTable>(stack.top(3));
		HlArray& A = *known_type<HlArray>(T.impl);
		/*if value is non-nil, create Cons pair and insert*/
		if(A[i * 2 + 1]) {
			Cons& newkv = *hp.create<Cons>();
			HlTable& T = *known_type<HlTable>(stack.top(3));
			HlArray& A = *known_type<HlArray>(T.impl);
			newkv.car() = A[i * 2];
			newkv.cdr() = A[i * 2 + 1];
			add_kv_to_array(*known_type<HlArray>(stack.top(2)),
				newkv,
				4 * LINEAR_LEVEL
			);
			++T.pairs;
		}
	}
	/*insert new key*/
	add_kv_to_array(*known_type<HlArray>(stack.top(2)),
		*known_type<Cons>(stack.top(1)),
		4 * LINEAR_LEVEL
	);
	HlTable& T = *known_type<HlTable>(stack.top(3));
	++T.pairs;
	/*replace implementation*/
	T.impl = stack.top(2);
	T.tbtype = hl_table_hashed;
	/*clean up stack*/
	stack.top(3) = cdr(stack.top(1));
	stack.pop(2);
	return;
}
rehash: {
	/*create a cons pair for the current k-v*/
	Cons* cp = hp.create<Cons>();
	cp->car() = stack.top(1);
	cp->cdr() = stack.top(2);
	stack.top(1) = Object::to_ref(cp);
	/*create the new array*/
	HlArray* NAp;
	{
		HlTable& T = *known_type<HlTable>(stack.top(3));
		HlArray& A = *known_type<HlArray>(T.impl);
		NAp = hp.create_variadic<HlArray>(A.size() * 2);
	}
	HlArray& NA = *NAp;
	/*re-read*/
	HlTable& T = *known_type<HlTable>(stack.top(3));
	HlArray& A = *known_type<HlArray>(T.impl);
	Cons& new_kv = *known_type<Cons>(stack.top(1));

	/*rehash*/
	size_t sz = NA.size();
	T.pairs = 0;
	for(size_t i = 0; i < A.size(); ++i) {
		if(A[i] && cdr(A[i])) {
			add_kv_to_array(NA, *known_type<Cons>(A[i]), sz);
			++T.pairs;
		}
	}
	/*insert new key*/
	add_kv_to_array(NA, new_kv, sz);
	++T.pairs;
	/*replace implementation*/
	T.impl = Object::to_ref(&NA);
	goto clean_up;
}
clean_up:
	stack.top(3) = stack.top(2);
	stack.pop(2);
	return;
}

/*copies a string key*/
/*sub-function of HlTable::keys*/
static void maybe_copy_string_key(Heap& hp, ProcessStack& stack) {
	/*stack top is:
		stack.top( 1 ) =
			(tb . ks)
	where ks is the list of keys
	what we want to do is to check if
	the first key (which is the most
	recently acquired key) is a string,
	and clone its pimpl (sharing the
	implementation) if so.
	*/
	Cons& ks = *known_type<Cons>(
		cdr(stack.top())
	);
	/*is it a string?*/
	if(maybe_type<HlString>(ks.car())) {
		/*create a copy of that string*/
		HlString& NS = *hp.create<HlString>();
		/*re-read old string*/
		Cons& ks = *known_type<Cons>(
			cdr(stack.top())
		);
		HlString& S = *known_type<HlString>(
			ks.car()
		);
		NS.impl = S.impl;
		/*the implementation is already
		shared at this point
		*/
		ks.car() = Object::to_ref<Generic*>(
			&NS
		);
	}
}

/*returns the keys of a table in a list*/
void HlTable::keys(Heap& hp, ProcessStack& stack) {
	/*determine the table type*/
	HlTableType type;
	{HlTable& t = *expect_type<HlTable>(stack.top());
		type = t.tbtype;
		if(type == hl_table_empty) {
			stack.top() = Object::nil();
			return;
		}
	}
	/*determine the size of the HlArray implementation*/
	size_t sz;
	{HlTable& t = *known_type<HlTable>(stack.top());
	HlArray& a = *known_type<HlArray>(t.impl);
		sz = a.size();
	}
	/*create a temporary Cons pair
		car = table
		cdr = list of keys being built
	why? because we want to avoid having to
	push a new item on the stack.  in the
	future, JITted code might prefer to have
	a statically-located stack; obviously a
	statically-located stack would also be
	statically-sized.
	*/
	{Cons& c = *hp.create<Cons>();
		c.car() = stack.top();
		stack.top() = Object::to_ref<Generic*>(&c);
	}
	switch(type){
	case hl_table_arrayed:
		for(size_t i = 0; i < sz; ++i) {
			HlTable& t = *known_type<HlTable>(
				car(stack.top())
			);
			HlArray& a = *known_type<HlArray>(t.impl);
			/*value valid?*/
			if(a[i]) {
				Cons& c = *hp.create<Cons>();
				c.car() = Object::to_ref<int>(i);
				c.cdr() = cdr(stack.top());
				scdr(stack.top(),
					Object::to_ref<Generic*>(&c)	
				);
			}
		}
		stack.top() = cdr(stack.top());
		return;
	case hl_table_linear:
		{/*have to look up pairs*/
			HlTable& t = *known_type<HlTable>(
				car(stack.top())
			);
			size_t pairs = t.pairs;
			for(size_t i = 0; i < pairs; ++i) {
				HlTable& t = *known_type<HlTable>(
					car(stack.top())
				);
				HlArray& a = *known_type<HlArray>(t.impl);
				/*value valid?*/
				if(a[i * 2 + 1]) {
					/*add a list element to the
					list of keys
					*/
					Cons& c = *hp.create<Cons>();
					c.cdr() = cdr(stack.top());
					scdr(stack.top(),
						Object::to_ref<Generic*>(&c)
					);
					/*re-read*/
					HlTable& t = *known_type<HlTable>(
						car(stack.top())
					);
					HlArray& a = *known_type<HlArray>(
						t.impl
					);
					c.car() = a[i * 2];
					maybe_copy_string_key(hp, stack);
				}
			}
		}
		stack.top() = cdr(stack.top());
		return;
	case hl_table_hashed:
		for(size_t i = 0; i < sz; ++i) {
			HlTable& t = *known_type<HlTable>(
				car(stack.top())
			);
			HlArray& a = *known_type<HlArray>(t.impl);
			/*value valid?*/
			if(a[i] && cdr(a[i])) {
				/*add a list element to the list of keys*/
				Cons& c = *hp.create<Cons>();
				c.cdr() = cdr(stack.top());
				scdr(stack.top(),
					Object::to_ref<Generic*>(&c)
				);
				/*re-read*/
				HlTable& t = *known_type<HlTable>(
					car(stack.top())
				);
				HlArray& a = *known_type<HlArray>(t.impl);
				/*add the key to list of keys*/
				c.car() = car(a[i]);
				maybe_copy_string_key(hp, stack);
			}
		}
		stack.top() = cdr(stack.top());
		return;
	}
}


