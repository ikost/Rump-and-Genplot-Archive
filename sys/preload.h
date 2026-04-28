#ifndef __preload
   #define __preload

/* ===========================================================================
-- <preload.h> header file
--
-- Compilers often provide features beyond the POSIX/ANSI standard, which are
-- enabled by setting particular #define's.   This file makes these
-- capabilities known to the code, and sets the necessary parameters to enable
-- their definition in the standard include files.  All of the GENPLOT/RUMP
-- code loads this include file prior to any of the standard include files.
--
-- Specific capabilities are defined in the individual sections of this
-- file.
=========================================================================== */
#if (defined CSET2) 
	#define	_POSIX_EXTENSIONS
	#define	_FP_INLINE
#elif (defined CONVEX_C)
	#define _POSIX_EXTENSIONS
#endif

/* ---------------------------------------------------------------------------
-- Many compilers provide additional math functions.  If defined, intrinsic's
-- for each of these functions are assumed.  Otherwise, we default to safe and
-- either avoid use, or use internally defined functions.
--
-- HAS_BESSEL     j0(x) j1(x) jn(x)  y0(x) y1(x) yn(x)
-- HAS_ERFC       erf(x) erfc(x)
-- HAS_GAMMA      gamma(x)             [ actually returns ln(gamma(x)) ]
--------------------------------------------------------------------------- */
#if ! (defined WATCOM || (defined OS2 && defined GNU_C) )
	#define	HAS_BESSEL					/* Almost everyone seems to have these beasts */
#endif

#if (defined CSET2)						/* Capabilites with the CSET compiler */
	#define	HAS_ERFC
	#define	HAS_GAMMA
	#ifdef __DEBUG_ALLOC__				/* if defined, using debug malloc etc. */
		#include <stdlib.h>
	#endif
#endif

#if (defined LINUX && defined GNU_C)		/* In this case at least */
	#define	HAS_ERFC
#endif

#endif /*  __preload */
