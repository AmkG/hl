#ifndef OBJECTS_H
#define OBJECTS_H

/*
Defines an Object::ref type, which is a tagged pointer type.
Usage:
	Object::ref x = Object::to_ref(1);//smallint
	Object::ref y = Object::to_ref(new(hp) Generic()); //object
	if(is_a<int>(x)) {
		std::cout << "x = " << as_a<int>(x) << std::endl;
	}
	if(is_a<Generic*>(y)) {
		std::cout << "y.field = " << as_a<Generic*>(y)->field
			<< std::endl;
	}
*/

#include<stdint.h>
#include<climits>

#include"unichars.hpp"

#include"workarounds.hpp"

class Generic;
class Symbol;
class Cons;
class Closure;
class KClosure;

namespace Object {

/*-----------------------------------------------------------------------------
Declare
-----------------------------------------------------------------------------*/
	class ref;
	template<typename T> struct tag_traits;
	/*assume we won't ever need more than 7 bits of tag*/
	typedef unsigned char tag_type;

	template<typename T> static inline ref to_ref(T);
	template<typename T> static inline bool _is_a(ref);
	template<typename T> static inline T _as_a(ref);

	static inline ref t(void);
	static inline bool _is_t(ref);

	static inline ref nil(void);
	static inline bool _is_nil(ref);

	static inline ref from_a_scaled_int(int);
	static inline int to_a_scaled_int(ref);
}

void throw_TypeError(Object::ref, char const*);
void throw_RangeError(char const*);

template<typename T> static inline bool is_a(Object::ref);
template<typename T> static inline T as_a(Object::ref);

static inline bool is_t(Object::ref);

size_t hash_is(Object::ref);

namespace Object {

/*-----------------------------------------------------------------------------
Configuration
-----------------------------------------------------------------------------*/

	static const unsigned char tag_bits = 2;// can bump up to 3 maybe...

	template<> struct tag_traits<int> {
		static const tag_type tag = 0x1;
	};
	template<> struct tag_traits<Generic*> {
		static const tag_type tag = 0x0;
	};
	template<> struct tag_traits<Symbol*> {
		static const tag_type tag = 0x2;
	};
	template<> struct tag_traits<UnicodeChar> {
		static const tag_type tag = 0x3;
	};


/*-----------------------------------------------------------------------------
Provided information
-----------------------------------------------------------------------------*/

	static const tag_type alignment = 1 << tag_bits;
	static const tag_type tag_mask = alignment - 1;

	/*the real range is the smaller of the range of intptr_t
	shifted down by tag bits, or the range of the `int' type
	*/
	static const intptr_t smallint_min =
		(sizeof(int) >= sizeof(intptr_t)) ?
			INTPTR_MIN >> tag_bits :
		/*otherwise*/
			INT_MIN;
	static const intptr_t smallint_max =
		(sizeof(int) >= sizeof(intptr_t)) ?
			INTPTR_MAX >> tag_bits :
		/*otherwise*/
			INT_MAX;

	/*value for "t"*/
	static const intptr_t t_value = ~((intptr_t) tag_mask);

/*-----------------------------------------------------------------------------
The tagged pointer type
-----------------------------------------------------------------------------*/

	/*stefano prefers to use:
		typedef void* ref;
	or maybe:
		typedef intptr_t ref;
	I may change this later on, but for now
	I want to see whether the conceptual
	separation will help and if compilers,
	in general, will be smart enough to
	optimize away the structure.
	*/
	class ref {
	private:
		intptr_t dat;
		ref(intptr_t x) : dat(x) {}
	public:
		ref(void) : dat(0) {}
		inline bool operator==(ref b) {
			return dat == b.dat;
		}
		inline bool operator!=(ref b) {
			return dat != b.dat;
		}
		inline bool operator!(void) {
			return dat == 0;
		}

		/*safe bool idiom*/
		typedef intptr_t (ref::*unspecified_bool_type);
		inline operator unspecified_bool_type(void) const {
			return dat != 0 ? &ref::dat : 0;
		}

		template<typename T> friend ref to_ref(T);
		template<typename T> friend bool _is_a(ref);
		template<typename T> friend T _as_a(ref);

		friend int to_a_scaled_int(ref);
		friend ref from_a_scaled_int(int);

		friend ref t(void);
		friend bool _is_t(ref);
		friend size_t (::hash_is)(Object::ref);
	};

/*-----------------------------------------------------------------------------
Tagged pointer factories
-----------------------------------------------------------------------------*/

	template<typename T>
	static inline ref to_ref(T x) {
		/* default to Generic* */
		return to_ref<Generic*>(x);
	}
	template<>
	STATIC_INLINE_SPECIALIZATION ref to_ref<Generic*>(Generic* x) {
		intptr_t tmp = reinterpret_cast<intptr_t>(x);
		#ifdef DEBUG
			if(tmp & tag_mask != 0) {
				throw_RangeError("Misaligned pointer");
			}
		#endif
		return ref(tmp + tag_traits<Generic*>::tag);
	}
	template<>
	STATIC_INLINE_SPECIALIZATION ref to_ref<Symbol*>(Symbol* x) {
		intptr_t tmp = reinterpret_cast<intptr_t>(x);
		#ifdef DEBUG
			if(tmp & tag_mask != 0) {
				throw_RangeError("Misaligned pointer");
			}
		#endif
		return ref(tmp + tag_traits<Symbol*>::tag);
	}
	template<>
	STATIC_INLINE_SPECIALIZATION ref to_ref<int>(int x) {
		#ifdef DEBUG
			#if (INT_MAX >= INTPTR_MAX) || (INT_MIN <= INTPTR_MIN)
				if(x < smallint_min || x > smallint_max) {
					throw_RangeError(
						"int out of range of smallint"
					);
				}
			#endif
		#endif
		intptr_t tmp = (((intptr_t) x) << tag_bits);
		return ref(tmp + tag_traits<int>::tag);
	}
	template<>
	STATIC_INLINE_SPECIALIZATION ref to_ref<UnicodeChar>(UnicodeChar x) {
		intptr_t tmp = x.dat << tag_bits;
		return ref(tmp + tag_traits<UnicodeChar>::tag);
	}

	/*no checking, even in debug mode... achtung!*/
	/*This function is used to convert an int computed using
	Object::to_a_scaled_int back to an Object::ref.  It is not
	intended to be used for any other int's.
	This function is intended for optimized smallint
	mathematics.
	*/
	static inline ref from_a_scaled_int(int x) {
                return ref((((intptr_t) x)<<tag_bits) + tag_traits<int>::tag);
	}

	static inline ref nil(void) {
		return ref();
	}
	static inline ref t(void) {
		return ref(t_value);
	}

/*-----------------------------------------------------------------------------
Tagged pointer checking
-----------------------------------------------------------------------------*/

	static inline bool _is_nil(ref obj) {
		return !obj;
	}
	static inline bool _is_t(ref obj) {
		return obj.dat == t_value;
	}

	template<typename T>
	static inline bool _is_a(ref obj) {
		if(tag_traits<T>::tag != 0x0) {
			return (obj.dat & tag_mask) == tag_traits<T>::tag;
		} else {
			return (obj.dat & tag_mask) == tag_traits<T>::tag
				&& !_is_nil(obj) && !_is_t(obj);
		}
	}

/*-----------------------------------------------------------------------------
Tagged pointer referencing
-----------------------------------------------------------------------------*/

	template<typename T>
	static inline T _as_a(ref obj) {
		#ifdef DEBUG
			if(!_is_a<T>(obj)) {
				throw_TypeError(obj,
					"incorrect type for pointer"
				);
			}
		#endif
		intptr_t tmp = obj.dat;
		return reinterpret_cast<T>(tmp - tag_traits<T>::tag);
		/*use subtraction instead of masking, in order to
		allow smart compilers to merge a field access with
		the tag removal. i.e. the pointers are pointers to
		structures, so they will be accessed via fields, and
		in all probability those fields will be at some
		offset from the actual structure address, meaning
		that the processor itself will perform an addition
		to access the field.  The smart compiler can then
		merge the addition of the field offset with the
		subtraction of the tag.
		*/
	}

	template<>
	STATIC_INLINE_SPECIALIZATION int _as_a<int>(ref obj) {
		#ifdef DEBUG
			if(!_is_a<int>(obj)) {
				throw_TypeError(obj,
					"incorrect type for small integer"
				);
			}
		#endif
		intptr_t tmp = obj.dat;
		return (int)(tmp >> tag_bits);
	}

	template<>
	STATIC_INLINE_SPECIALIZATION UnicodeChar _as_a<UnicodeChar>(ref obj) {
		uint32_t tmp = obj.dat;
		return UnicodeChar(tmp >> tag_bits);
	}

	/*no checking, even in debug mode... achtung!*/
	/*This function is used to convert a smallint Object::ref
	to a useable int that is equal to the "real" int, shifted
	to the left by the number of tag bits (i.e. scaled).  It
	should be used only for optimizing smallint math operations.
	*/
	static inline int to_a_scaled_int(ref obj) {
		intptr_t tmp = obj.dat;
		return (int)((tmp - tag_traits<int>::tag)>>tag_bits);
		/*use subtraction instead of masking, again to
		allow smart compilers to merge tag adding and
		subtracting.  For example the typical case would
		be something like:

		Object::ref result =
			Object::from_a_scaled_int(
				Object::to_a_scaled_int(a) +
				Object::to_a_scaled_int(b)
			);

		The above case can be reduced by the compiler to:

		intptr_t result =
			(tag_traits<int>::tag +
				a - tag_traits<int>::tag +
				b - tag_traits<int>::tag
			);

		It can then do some maths and cancel out a tag:

		intptr_t result = a + b - tag_traits<int>::tag;
		*/
	}

/*-----------------------------------------------------------------------------
Utility
-----------------------------------------------------------------------------*/

	static inline size_t round_up_to_alignment(size_t x) {
		return
		(x & tag_mask) ?      (x + alignment - (x & tag_mask)) :
		/*otherwise*/         x ;
	}

}

/*-----------------------------------------------------------------------------
Reflectors outside of the namespace
-----------------------------------------------------------------------------*/

template<typename T>
static inline bool is_a(Object::ref obj) {
	return Object::_is_a<T>(obj);
}
template<typename T>
static inline T as_a(Object::ref obj) {
	return Object::_as_a<T>(obj);
}
static inline bool is_nil(Object::ref obj) {
	return Object::_is_nil(obj);
}
static inline bool is_t(Object::ref obj) {
	return Object::_is_t(obj);
}


#endif //OBJECTS_H

