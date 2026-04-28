/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdio.h>
#include <stdlib.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"

/* -------------------------------- */
/* My share of the global variables */
/* -------------------------------- */
/* Serial Number and link date information */
const char  RumpRevisionLevel[]="Version 0.950" ;
const char  RumpLinkDate[]= __DATE__ "  "  __TIME__  ;
const REAL  RumpVersionNumber=4.0f;

#include "revision.h"

/* ---------------------------------------------------------------------------
-- Default search path for RUMP data files
--
-- The best case is for the make file to properly define this parameter
-- in the RUMPCONFIGPATH variable.  If this is not defined, then try to
-- use GENPLOT's, and if that's not available, default to some operating
-- system reasonable default.
--
-- This choice can be over-riden by the user by either setting a PROFILE
-- variable in the case of OS/2, or an environment variable on any OS.
--------------------------------------------------------------------------- */
#ifdef RUMPCONFIGPATH									/* Hope this is defined		*/
	const char	*RumpDefaultConfigPath=RUMPCONFIGPATH;
#elif defined CONFIGPATH								/* If not, use GENPLOT's	*/
	const	char	*RumpDefaultConfigPath=CONFIGPATH;
#elif (defined OS2 || defined NT)					/* Otherwise, use default	*/
	const char	*RumpDefaultConfigPath="c:\\usr\\rump\\data;c:\\usr\\rump";
#else															/* Appropriate to OS			*/
	const char	*RumpDefaultConfigPath="/usr/local/lib/rump/data:/usr/local/lib/rump";
#endif
