#ifndef WORKAROUNDS_H
#define WORKAROUNDS_H

/*
 * This file contains work-arounds for some compiler
 * bugs.
 * (We might also use it for detecting and using
 * GCC extensions, such as __attribute__((__pure__)))
 */

/*-----------------------------------------------------------------------------
GNU C version
-----------------------------------------------------------------------------*/

#ifdef __GNUC__
#define __GNUC_VERSION__ (__GNUC__ * 10000 \
                          + __GNUC_MINOR__ * 100 \
                          + __GNUC_PATCHLEVEL__)
#endif

/*-----------------------------------------------------------------------------
Issue: template specializations should "inherit" storage from primary
	(unspecialized) template.
Known to occur on G++ < 4.3
-----------------------------------------------------------------------------*/

#ifdef __GNUC__
	#if __GNUC_VERSION__ < 40300
		#define	STATIC_INLINE_SPECIALIZATION static inline
	#endif
#endif

#ifndef STATIC_INLINE_SPECIALIZATION
	#define STATIC_INLINE_SPECIALIZATION
#endif

/*-----------------------------------------------------------------------------
Non-issue: GCC supports rearranging code for likelihood/unlikelihood
Known to be supported on GCC >= 2.96
-----------------------------------------------------------------------------*/

#ifdef __GNUC__
	#if __GNUC_VERSION__ >= 29600
		#define likely(x)	__builtin_expect(!!(x), 1)
		#define unlikely(x)	__builtin_expect(!!(x), 0)
	#endif
#endif

#ifndef likely
	#define likely(x)	(x)
	#define unlikely(x)	(x)
#endif

#endif // WORKAROUNDS_H

