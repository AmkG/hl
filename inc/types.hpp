#ifndef TYPES_H
#define TYPES_H
/*
Defines a set of types for use on the hl-side.
*/

#include<cstring>
#include<string>
#include<boost/shared_ptr.hpp>
#include<boost/shared_array.hpp>
#include<boost/scoped_ptr.hpp>

#include"objects.hpp"
#include"specializeds.hpp"
#include"heaps.hpp"
#include"processes.hpp"
#include"history.hpp"

/*-----------------------------------------------------------------------------
Utility
-----------------------------------------------------------------------------*/

/*Usage:
Cons* cp = expect_type<Cons>(proc.stack().top(),
		"Your mom expects a Cons cell on top"
);
*/
template<class T>
static inline T* expect_type(Object::ref x, char const* error = "type error") {
	if(!is_a<Generic*>(x)) throw_HlError(error);
	T* tmp = dynamic_cast<T*>(as_a<Generic*>(x));
	if(!tmp) throw_HlError(error);
	return tmp;
}

// check that object x is of type T
template<class T>
static inline void check_type(Object::ref x, const char *error = "type error"){
	if (!is_a<Generic*>(x) || !dynamic_cast<T*>(as_a<Generic*>(x)))
		throw_HlError(error);	
}

/*Usage:
// your mom knows it's a Cons on top
Cons* cp = known_type<Cons>(proc.stack().top());
*/
template<class T>
static inline T* known_type(Object::ref x) {
	return static_cast<T*>(as_a<Generic*>(x));
}

/*
Cons* cp = maybe_type<Cons>*(proc.stack().top());
if(cp) {
	your_mom_does_a_cons(cp);
} else {
	your_mom_does_something_else();
}
*/
template<class T>
static inline T* maybe_type(Object::ref x) {
	if(!is_a<Generic*>(x)) return NULL;
	return dynamic_cast<T*>(as_a<Generic*>(x));
}

/*-----------------------------------------------------------------------------
Cons cell
-----------------------------------------------------------------------------*/

class Cons : public GenericDerived<Cons> {
private:
  
  Object::ref car_ref;
  Object::ref cdr_ref;

public:

  inline Object::ref& car() { return car_ref; }
  inline Object::ref& cdr() { return cdr_ref; }
  inline Object::ref scar(Object::ref x) { return car_ref = x; }
  inline Object::ref scdr(Object::ref x) { return cdr_ref = x; }

  Cons() : car_ref(Object::nil()), cdr_ref(Object::nil()) {}
  /*
   * Note that we *can't* safely construct any heap-based objects
   * by, say, passing them any of their data.  This is because the
   * act of allocating space for these objects may start a garbage
   * collection, which can move *other* objects.  So any references
   * passed into the constructor will be invalid; instead, the
   * process must save the data to be put in the new object in a
   * root location (such as the process stack) and after it is
   * given the newly-constructed object, it must store the data
   * straight from the root location.
   */

  Object::ref type(void) const {
    return Object::to_ref(symbol_cons);
  }

  void traverse_references(GenericTraverser *gt) {
    gt->traverse(car_ref);
    gt->traverse(cdr_ref);
  }

  Object::ref len();

};

/*
 * WARNING: It's possible to write something like:
 * car(foo) = 42;
 * THIS IS WRONG.  Avoid.
 * */
extern inline Object::ref const& car(Object::ref const& x) {
	if(!x) return x;
        return expect_type<Cons>(x,"'car expects a Cons cell")->car();
}
extern inline Object::ref const& cdr(Object::ref const& x) {
	if(!x) return x;
	return expect_type<Cons>(x,"'cdr expects a Cons cell")->cdr();
}
extern inline Object::ref scar(Object::ref c, Object::ref v) {
	return expect_type<Cons>(c,"'scar expects a true Cons cell")->scar(v);
}
extern inline Object::ref scdr(Object::ref c, Object::ref v) {
	return expect_type<Cons>(c,"'scdr expects a true Cons cell")->scdr(v);
}


/*-----------------------------------------------------------------------------
HlPid
-----------------------------------------------------------------------------*/
/*Represents a process that can
be sent messages to by this process
*/

class Process;

class HlPid : public GenericDerived<HlPid> {
public:
	Process* process;

	bool is(Object::ref o) const {
		HlPid* pp = maybe_type<HlPid>(o);
		if(pp) {
			return process == pp->process;
		} else return false;
	}
	void enhash(HashingClass* hc) const {
		hc->enhash((size_t) process);
	}
	Object::ref type(void) const {
		return Object::to_ref(symbol_pid);
	}
};

/*-----------------------------------------------------------------------------
Closures
-----------------------------------------------------------------------------*/

/*Closure
*/
class Closure : public GenericDerivedVariadic<Closure> {
private:
  Object::ref body;
  bool nonreusable;
  boost::scoped_ptr<History::InnerRing> kontinuation;

public:
  Closure(size_t sz) : GenericDerivedVariadic<Closure>(sz), 
                       nonreusable(true),
                       kontinuation() {}

  /*copy constructor*/
  Closure(Closure const& o)
    : body(o.body),
      nonreusable(true),
      GenericDerivedVariadic<Closure>(o),
      kontinuation(
        o.kontinuation ?
         new History::InnerRing(*o.kontinuation) :
        /*else*/
         0) { }
  Object::ref& operator[](size_t i) { 
    if (i < size())
      return index(i);
    else
      throw_HlError("internal: trying to access closed vars with an index too large!");
  }
  Object::ref code() { return body; }
  void codereset(Object::ref b) { body = b; }
  void banreuse() {
    /*ban reuse for this and for every other continuation referred to*/
    Closure* kp = this;
  loop:
    if(kp->nonreusable) return; // reuse already banned
    kp->nonreusable = true;
    for(size_t i = 0; i < kp->sz; ++i) {
      Object::ref tmp = kp->index(i);
      if(is_a<Generic*>(tmp)) {
        Closure* cp = dynamic_cast<Closure*>(as_a<Generic*>(tmp));
        if(cp && cp->kontinuation) {
          /*assume only one continuation is actually referred to
          directly (this is what the compiler emits anyway)
          */
          kp = cp;
          goto loop;
        }
      }
    }
  }
  bool reusable() { return !nonreusable; }

  static Closure* NewKClosure(Heap & h, size_t n);
  static Closure* NewClosure(Heap & h, size_t n);

  Object::ref type(void) const {
    return Object::to_ref(symbol_fn);
  }

  void traverse_references(GenericTraverser* gt) {
    gt->traverse(body);    
    for(size_t i = 0; i < sz; ++i) {
      gt->traverse(index(i));
    }
    if(kontinuation) kontinuation->traverse_references(gt);
  }

  friend class History;
};

/*-----------------------------------------------------------------------------
Strings
-----------------------------------------------------------------------------*/

/*abstract base for HlString implementations*/
class HlStringBufferPointer;
class HlStringPath;
class HlStringIter;
class HlStringImpl {
private:
	HlStringImpl(void); // disallowed!
protected:
	explicit HlStringImpl(size_t nl) : len(nl) { }
	size_t const len;
public:
	virtual ~HlStringImpl() { }
	virtual size_t rope_depth(void) const { return 0; }
	virtual void point_at(
		HlStringBufferPointer&,
		boost::shared_ptr<HlStringPath>&,
		size_t) const =0;
	size_t length(void) const { return len; }
	virtual UnicodeChar ref(size_t i) const =0;
	virtual void cut(
		boost::shared_ptr<HlStringImpl>& into,
		size_t start,
		size_t len) const =0;
	virtual void to_cpp_string(std::string&) const =0;
};
class HlString : public GenericDerived<HlString> {
public:
	boost::shared_ptr<HlStringImpl> pimpl;
	mutable uint32_t hash_cache;

	HlString(void) : pimpl(), hash_cache(0) { }

	bool is(Object::ref) const;
	void enhash(HashingClass*) const;

	UnicodeChar ref(size_t i) const {
		return pimpl->ref(i);
	}
	size_t size(void) const {
		return pimpl->length();
	}

	/*defined in this header*/
	HlStringIter at(size_t i) const;

	/*
	precondition:
		stack.top(2) = left string
		stack.top(1) = right string
	postcondition:
		stack.top(1) = concatenated strings
	*/
	static void append(Heap& hp, ProcessStack& stack);

	/*creates a string from the characters on stack.top(N) to stack.top(1)*/
	static void stack_create(Heap& hp, ProcessStack& stack, size_t N);
	Object::ref type(void) const {
		return Object::to_ref(symbol_string);
	}

	static Object::ref length(Object::ref o) {
		HlString* sp = expect_type<HlString>(o,
			"'string-length expects a string"
		);
		return Object::to_ref<int>(
			sp->pimpl->length()
		);
	}
	static Object::ref string_ref(Object::ref o, Object::ref i) {
		HlString* sp = expect_type<HlString>(o,
			"'string-ref expects a string as first argument"
		);
		if(!is_a<int>(i)) {
			throw_HlError(
				"'string-ref expects an integer as "
				"second argument"
			);
		}
		int ii = as_a<int>(i);
		return Object::to_ref(sp->pimpl->ref(ii));
	}

	/*conversion to and from C++ std::strings
	std::string's are assumed to be UTF-8
	*/
	std::string to_cpp_string(void) const {
		std::string rv;
		pimpl->to_cpp_string(rv);
		return rv;
	}
	/*return on stack top*/
	static void from_cpp_string(
		Heap& hp,
		ProcessStack& stack,
		std::string const&
	);
};

/*HlStringBufferPointer, which is the core of HlStringIter*/
class HlStringBufferPointer {
private:
	boost::shared_array<unsigned char> buffer;
	unsigned char const* start;
	unsigned char const* end;
	static inline uint32_t extract_utf8(unsigned char c) { /*Pure*/
		return c & 63;
	}
public:
	void swap(HlStringBufferPointer& o) {
		buffer.swap(o.buffer);
		unsigned char const* tmp = start;
		start = o.start; o.start = tmp;
		tmp = end;
		end = o.end; o.end = tmp;
	}
	UnicodeChar operator*(void) const {
		unsigned char rv = *start;
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
		throw_HlError("Invalid character in string buffer");
	}
	HlStringBufferPointer& operator++(void) {
		unsigned char c = *start;
		if(c < 128) { ++start; }
		else if(c >= 240) { start += 4; }
		else if(c >= 224) { start += 3; }
		else if(c >= 192) { start += 2; }
		else throw_HlError("Invalid character in string buffer");
		return *this;
	}
	bool at_end(void) const {
		return start == end;
	}
	friend class AsciiImpl;
	friend class Utf8Impl;
	friend class HlStringIter;
};

/*linked-list of HlString's*/
class HlStringPath {
private:
	boost::shared_ptr<HlStringImpl> const sp;
	boost::shared_ptr<HlStringPath> next;
	HlStringPath(void); // disallowed
	explicit HlStringPath(boost::shared_ptr<HlStringImpl> const& nsp)
		: sp(nsp), next() { }
public:
	static void push(
			boost::shared_ptr<HlStringImpl> const& sp,
			boost::shared_ptr<HlStringPath>& cur) {
		boost::shared_ptr<HlStringPath> rv(
			new HlStringPath(sp)
		);
		rv->next.swap(cur);
		cur.swap(rv);
	}
	static void pop(
			boost::shared_ptr<HlStringImpl>& sp,
			boost::shared_ptr<HlStringPath>& cur) {
		sp = cur->sp;
		cur = cur->next;
	}
};

/*HlStringIter, which eventually forms HlStringPointer*/
/*
how it works: we use a rope implementation, so if an
iterator for a rope node is taken, the rope node pushes
its right string onto the HlStringPath list, then takes
the iterator for the left string.  The left string, if
it is also a rope node, also pushes its own right string
to the list and takes the iterator for its own left
string.  This goes on until the left string is a plain
buffer.
*/
class HlStringIter {
private:
	HlStringBufferPointer b;
	boost::shared_ptr<HlStringPath> p;
public:
	void swap(HlStringIter& o) {
		o.b.swap(b);
		o.p.swap(p);
	}
	explicit HlStringIter(HlStringImpl const& s, size_t at = 0) {
		s.point_at(b, p, at);
	}
	HlStringIter(void) : b(), p() { }
	UnicodeChar operator*(void) const { return *b; }
	HlStringIter& operator++(void) {
		++b;
		while(b.at_end() && p) {
			boost::shared_ptr<HlStringImpl> tmp;
			HlStringPath::pop(tmp, p);
			tmp->point_at(b, p, 0);
		}
		return *this;
	}
	bool at_end(void) const {
		return b.at_end() && !p;
	}
	void destruct(boost::shared_ptr<HlStringImpl>&, size_t&) const;
};

inline HlStringIter HlString::at(size_t i) const {
	return HlStringIter(*pimpl, i);
}

/*core for HlStringBuilder*/
class HlStringBuilderCore {
private:
	boost::shared_ptr<HlStringImpl> prefix;
	std::vector<unsigned char> building;
	size_t building_unichars; // number of unicode characters, not bytes!
	bool utf8_mode;

	/*creates a HlStringImpl out of the current
	building and building_unichars, then appends
	it to the end of the prefix
	*/
	void build_prefix(void);

public:
	void swap(HlStringBuilderCore& o) {
		prefix.swap(o.prefix);
		building.swap(o.building);
		size_t ts = building_unichars;
		building_unichars = o.building_unichars;
		building_unichars = ts;
		bool tmp = utf8_mode;
		utf8_mode = o.utf8_mode;
		o.utf8_mode = tmp;
	}

	void add(UnicodeChar);
	void add_s(boost::shared_ptr<HlStringImpl> const&);
	void inner(boost::shared_ptr<HlStringImpl>&);
	HlStringBuilderCore(void)
		: prefix(), building(),
		  building_unichars(0), utf8_mode(false) { }
	explicit HlStringBuilderCore(boost::shared_ptr<HlStringImpl> const& o)
		: prefix(o), building(),
		  building_unichars(0), utf8_mode(false) { }
};

/*-----------------------------------------------------------------------------
HlStringBuilder
-----------------------------------------------------------------------------*/

class HlStringBuilder : public GenericDerived<HlStringBuilder> {
public:
	HlStringBuilderCore core;

	Object::ref type(void) const {
		return Object::to_ref(symbol_string_builder);
	}

	static Object::ref create(Process& proc) {
		return Object::to_ref<Generic*>(proc.create<HlStringBuilder>());
	}
	static Object::ref add(Object::ref sb, Object::ref c) {
		HlStringBuilder* sbp = expect_type<HlStringBuilder>(sb,
			"'sb-add expects a string-builder as first argument"
		);
		if(!is_a<UnicodeChar>(c)) {
			throw_HlError("'sb-add expects a character as second argument");
		}
		sbp->core.add(as_a<UnicodeChar>(c));
		return c;
	}
	static Object::ref add_s(Object::ref sb, Object::ref s) {
		HlStringBuilder* sbp = expect_type<HlStringBuilder>(sb,
			"'sb-add-s expects a string-builder as first argument"
		);
		HlString* sp = expect_type<HlString>(s,
			"'sb-add-s expects a string as second argument"
		);
		sbp->core.add_s(sp->pimpl);
		return s;
	}
	static Object::ref inner(Process& proc, Object::ref const& sb) {
		/*extract the inner of the sb first, then construct*/
		HlStringBuilder* sbp = expect_type<HlStringBuilder>(sb,
			"'sb-inner expects a string-builder"
		);
		boost::shared_ptr<HlStringImpl> sip;
		sbp->core.inner(sip);
		/*construct a new HlString on the process. after
		construction, treat sbp as invalid!
		*/
		HlString* sp = proc.create<HlString>();
		sp->pimpl.swap(sip);
		return Object::to_ref<Generic*>(sp);
	}
};

/*-----------------------------------------------------------------------------
HlStringPointer
-----------------------------------------------------------------------------*/

class HlStringPointer : public GenericDerived<HlStringPointer> {
public:
	HlStringIter core;

	Object::ref type(void) const {
		return Object::to_ref(symbol_string_pointer);
	}

	static Object::ref create(
			Process&proc, Object::ref s, Object::ref i) {
		/*extract and check types*/
		HlString* sp = expect_type<HlString>(s,
			"'string-pointer expects a string as first argument"
		);
		if(!is_a<int>(i)) {
			throw_HlError(
				"'string-pointer expects an integer as second argument."
			);
		}
		int ii = as_a<int>(i);
		/*first, create a temporary HlStringIter*/
		HlStringIter it = sp->at(ii);
		/*now allocate - treat sp as invalid afterwards*/
		HlStringPointer* spp = proc.create<HlStringPointer>();
		spp->core.swap(it);
		return Object::to_ref<Generic*>(spp);
	}
	static Object::ref ref(Object::ref sp) {
		HlStringPointer* spp = expect_type<HlStringPointer>(sp,
			"'sp-ref expects a string-pointer"
		);
		return Object::to_ref(*spp->core);
	}
	static Object::ref at_end(Object::ref sp) {
		HlStringPointer* spp = expect_type<HlStringPointer>(sp,
			"'sp-at-end expects a string-pointer"
		);
		return
			spp->core.at_end() ?		Object::t() :
			/*otherwise*/			Object::nil()
		;
	}
	static Object::ref adv(Object::ref sp) {
		HlStringPointer* spp = expect_type<HlStringPointer>(sp,
			"'sp-adv expects a string-pointer"
		);
		++spp->core;
		return sp;
	}
	/*
	precondition:
		stack.top() = string-pointer to destruct
	postcondition:
		stack.top() = cons-cell
			car = string
			cdr = number
	*/
	static Object::ref destruct(Heap& hp, ProcessStack& stack) {
		HlStringPointer* spp = expect_type<HlStringPointer>(stack.top(),
			"'sp-destruct expects a string-pointer"
		);
		boost::shared_ptr<HlStringImpl> sip;
		size_t i;
		spp->core.destruct(sip, i);
		/*now alloc a cons cell and put it on stack*/
		{ Cons* cp = hp.create<Cons>();
			stack.top() = Object::to_ref<Generic*>(cp);
		}
		/*alloc a string*/
		{ HlString* sp = hp.create<HlString>();
			sp->pimpl.swap(sip);
			/*assign them*/
			Cons* cp = known_type<Cons>(stack.top());
			cp->car() = Object::to_ref<Generic*>(sp);
			cp->cdr() = Object::to_ref<int>(i);
		}
	}
};

/*-----------------------------------------------------------------------------
Floating point numbers
-----------------------------------------------------------------------------*/

class Float : public GenericDerived<Float> {
private:
  double val;
public:
  // construction
  static inline Float* mk(Heap & h, double val) {
    Float *f = h.create<Float>();
    f->val = val;
    return f;
  }
  // make a float that will live forever
  static inline Float* mkEternal(double val) {
    Float *f = new Float();
    f->val = val;
    return f;
  }
  inline double get() { return val; }
  Object::ref type(void) const {
    return Object::to_ref(symbol_float);
  }
  // Numbers are immutable
};

/*conversion*/
inline Object::ref i_to_f( Object::ref o, Process& proc ) {
	#ifdef DEBUG
		if(!is_a<int>(o)) {
			throw_HlError("'i-to-f expected int");
		}
	#endif
	return Object::to_ref<Generic*>(
		Float::mk(proc.heap(), (double) as_a<int>(o))
	);
}

inline Object::ref f_to_i( Object::ref o ) {
	#ifdef DEBUG
		Float* tmp = expect_type<Float>(o, "f-to-i expected float");
	#endif
	return Object::to_ref<int>(
		(int) known_type<Float>(o)->get()
	);
}

/*-----------------------------------------------------------------------------
Array
-----------------------------------------------------------------------------*/

class HlArray : public GenericDerivedVariadic<HlArray> {
public:
	void traverse_references(GenericTraverser* gt) {
		for(size_t i = 0; i < sz; ++i) {
			gt->traverse(index(i));
		}
	}
	size_t size(void) const {
		return sz;
	}
	Object::ref& operator[](size_t i) {
		return index(i);
	}
	Object::ref const& operator[](size_t i) const {
		return index(i);
	}
	Object::ref type(void) const {
		return Object::to_ref(symbol_array);
	}
	HlArray(size_t sz) : GenericDerivedVariadic<HlArray>(sz) { }
};

/*-----------------------------------------------------------------------------
Tables
-----------------------------------------------------------------------------*/

enum HlTableType {
	/*table is empty*/
	hl_table_empty,

	/*pairs are stored in a straight array:
		[0] = first key
		[1] = first value
		[2] = second key
		[3] = second value
		...
	*/
	hl_table_linear,

	/*values are stored in straight array
		[0] = value for key '0
		[1] = value for key '1
		[2] = value for key '2
		...
	*/
	hl_table_arrayed,

	/*pairs are stored in hash buckets.  When hashes collide,
	simply put in next hash bucket.
		[0] = (k . v) pair with (hash k) == 0
		[1] = (k . v) pair with (hash k) == 0 or (hash k) == 1
		...
		[N] = (k . v) pair with (hash k) == N
		[N+1] = (k . v) pair with (hash k) == N or (hash k) == N + 1
	*/
	hl_table_hashed
};

class HlTable : public GenericDerived<HlTable> {
private:
	/*number of entries in a linear table before we switch
	over to a hashed table
	*/
	static const size_t LINEAR_LEVEL = 8;

	/*Maximum numeric index when inserting a numeric
	key into an empty table, and we decide to use
	an arrayed table
	*/
	static const size_t ARRAYED_LEVEL = 4;

	/*used internally*/
	Object::ref* linear_lookup(Object::ref) const;
	Object::ref* arrayed_lookup(Object::ref) const;
	Object::ref* hashed_lookup(Object::ref) const;

	Object::ref* location_lookup(Object::ref) const;

public:
	Object::ref impl;
	HlTableType tbtype;
	size_t pairs;		/*number of key-value pairs*/

	void traverse_references(GenericTraverser* gt) {
		gt->traverse(impl);
	}

	inline Object::ref lookup(Object::ref k) const {
		Object::ref* op = location_lookup(k);
		if(op) return *op; else return Object::nil();
	}

	/*On entry:
		stack.top(1) = key
		stack.top(2) = value
		stack.top(3) = table (type not checked!)
	On return:
		stack.top(1) = value
	*/
	static void insert(Heap&, ProcessStack&);

	static void keys(Heap&, ProcessStack&);

	Object::ref type(void) const {
		return Object::to_ref(symbol_table);
	}

	HlTable(void) : impl(Object::nil()), tbtype(hl_table_empty) { }
};

/*-----------------------------------------------------------------------------
SharedVar's / Containers
-----------------------------------------------------------------------------*/

class SharedVar : public GenericDerived<SharedVar> {
public:
	Object::ref val;
	void traverse_references(GenericTraverser* gt) {
		gt->traverse(val);
	}

	Object::ref type(void) const {
		return Object::to_ref(symbol_container);
	}

	SharedVar(void) : val(Object::nil()) { }
};

inline Object::ref make_sv(Object::ref nval, Process& proc) {
	SharedVar* rv = proc.heap().create<SharedVar>();
	rv->val = nval;
	return Object::to_ref(rv);
}

inline Object::ref sv_ref(Object::ref sv) {
	return expect_type<SharedVar>(sv, "sv-ref expects a container")
		->val;
}

inline Object::ref sv_set(Object::ref sv, Object::ref nval) {
	expect_type<SharedVar>(sv, "sv-set expects a container")
		->val = nval;
	return nval;
}

/*-----------------------------------------------------------------------------
I/O Ports (async)
-----------------------------------------------------------------------------*/

class IOPort;

class HlIOPort : public GenericDerived<HlIOPort> {
public:
  boost::shared_ptr<IOPort> p;

  bool is(Object::ref o) const {
    HlIOPort* op = maybe_type<HlIOPort>(o);
    return op && op->p == p;
  }

  void enhash(HashingClass* hc) const {
    hc->enhash((size_t) p.get());
  }

  Object::ref type(void) const {
    return Object::to_ref(symbol_ioport);
  }
};

class Event;

class HlEvent : public GenericDerived<HlEvent> {
public:
  Object::ref hl_pid;
  /*the above slot is needed to keep alive the
  process to be sent a message.

  The problem is that after the event is created,
  the process to be sent a message to might wait
  for the message.  This leaves it open to the
  process-level GC.

  The Event Set is considered a root, but the
  individual event objects are not.  Thus, we
  need the hl_pid slot to keep alive a copy of
  the HlPid, so that we can keep alive the
  actual process.

  This is an admittedly hackish solution
  */

  boost::shared_ptr<Event> p;

  bool is(Object::ref o) const {
    HlEvent* op = maybe_type<HlEvent>(o);
    return op && op->p == p;
  }

  Object::ref type(void) const {
    return Object::to_ref(symbol_event);
  }

  void enhash(HashingClass* hc) const {
    hc->enhash((size_t) p.get());
  }

  void traverse_references(GenericTraverser *gt) {
    gt->traverse(hl_pid);
  }

};


/*-----------------------------------------------------------------------------
Tagged types
-----------------------------------------------------------------------------*/

class HlTagged : public GenericDerived<HlTagged> {
public:
	/*NOTE! these two fields should be immutable after construction.
	This is because it would then allow is-comparison to be performed
	recursively wtihout worrying about cycles.
	*/
	Object::ref o_type;
	Object::ref o_rep;

	bool is(Object::ref o) const {
		HlTagged* htp = maybe_type<HlTagged>(o);
		if(htp) {
			/*!!WARNING: deep recursion might overflow C-stack!
			TODO: figure out a non-recursive algo
			*/
			return ::is(o_type, htp->o_type) &&
				::is(o_rep, htp->o_rep);
		} else return false;
	}
	void enhash(HashingClass* hc) const {
		hc->enhash(hash_is(o_type));
		hc->enhash(hash_is(o_rep));
	}

	void traverse_references(GenericTraverser* gt) {
		gt->traverse(o_type);
		gt->traverse(o_rep);
	}
	Object::ref type(void) const {
		return o_type;
	}
};

/*-----------------------------------------------------------------------------
Types
-----------------------------------------------------------------------------*/

extern inline Object::ref type(Object::ref ob) {
	if(ob == Object::t() || ob == Object::nil()) {
		return Object::to_ref(symbol_bool);
	}
	if(is_a<Generic*>(ob)) {
		return as_a<Generic*>(ob)->type();
	}
	/*maybe some way of putting this in tag_traits?*/
	if(is_a<Symbol*>(ob)) {
		return Object::to_ref(symbol_sym);
	}
	if(is_a<int>(ob)) {
		return Object::to_ref(symbol_int);
	}
	if(is_a<UnicodeChar>(ob)) {
		return Object::to_ref(symbol_char);
	}
}

extern inline Object::ref rep(Object::ref ob) {
	HlTagged* htp = maybe_type<HlTagged>(ob);
	if(htp) {
		return htp->o_rep;
	} else return ob;
}

/*-----------------------------------------------------------------------------
Binary Objects
-----------------------------------------------------------------------------*/

/*Binary objects are just shared immutable arrays
of bytes.
*/
class BinObj : public GenericDerived<BinObj> {
public:
	boost::shared_ptr<std::vector<unsigned char> > pdat;
	static inline BinObj* create(Heap& hp, boost::shared_ptr<std::vector<unsigned char> > const& np) {
		BinObj* rv = hp.create<BinObj>();
		rv->pdat = np;
		return rv;
	}
	unsigned char const& operator[](size_t i) const {
		return (*pdat)[i];
	}
	size_t size(void) const {
		return pdat->size();
	}

	Object::ref type(void) const {
		return Object::to_ref(symbol_binobj);
	}

	static Object::ref from_Cons(Process& proc, Object::ref dat) {
		boost::shared_ptr<std::vector<unsigned char> > bp(
			new std::vector<unsigned char>()
		);
		while(dat) {
			#ifdef DEBUG
				expect_type<Cons>(
					dat,
					"l-to-b: expected a Cons cell"
				);
				if(!is_a<int>(car(dat))) {
					throw_HlError(
						"l-to-b: expected an integer."
					);
				}
			#endif
			bp->push_back(as_a<int>(car(dat)));
			dat = cdr(dat);
		}
		return Object::to_ref<Generic*>(create(proc.heap(), bp));
	}

	static Object::ref bin_ref(Object::ref dat, Object::ref off) {
		#ifdef DEBUG
			expect_type<BinObj>(
				dat,
				"b-ref: expected a binary object"
			);
			if(!is_a<int>(off)) {
				throw_HlError(
					"b-ref: expected an integer "
					"for index."
				);
			}
		#endif
		return Object::to_ref<int>(
			(*known_type<BinObj>(dat))[as_a<int>(off)]
		);
	}

	static Object::ref bin_len(Object::ref dat) {
		#ifdef DEBUG
			expect_type<BinObj>(
				dat,
				"b-len: expected a binary object"
			);
		#endif
		return Object::to_ref<int>(
			known_type<BinObj>(dat)->size()
		);
	}

};

/*-----------------------------------------------------------------------------
Accessor Objects
-----------------------------------------------------------------------------*/
/* Objects returned by '<bc>accessor are simply
plain Closure's, but whose last entry is an object
of type AccessorSlot below.
'<bc>disclose skips the last entry if it is an
AccessorSlot entry.
'<bc>accessor-ref checks the last entry if it
is an AccessorSlot entry and returns its value
if so.
*/
class AccessorSlot : public GenericDerived<AccessorSlot> {
public:
	Object::ref slot;
	void traverse_references(GenericTraverser* gt) {
		gt->traverse(slot);
	}
	Object::ref type(void) const {
		return Object::to_ref(symbol_unspecified);
	}
};

#endif //TYPES_H

