/* <routine name> */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define	_POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

#define	TRUE	1
#define	FALSE	0

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <float.h>
#include <setjmp.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static int mathcheck(int type, char *name);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ---------------------------------------------------------------------------
-- Math exception error handler --
--------------------------------------------------------------------------- */
EXPORT volatile sig_atomic_t _Sys_Math_Exception = -1;		/* Math flag		*/
EXPORT char						  _Sys_Math_Message[256] = "";	/* Error message	*/

/* ---------------------------------------------------------------------------
-- Routine to accept math exceptions and do something with them.  Under OS/2
-- we have a little better chance of actually giving useful messages. 
--
-- Flag _Sys_Math_Exception: 
--    <  0 ==> print error message locally (leave flag unchanged)
--    >= 0 ==> return error message only, increment _Sys_Math_Exception
--
-- Idea: We always trap the system library math errs via matherr and _matherrl.
--       Look at the flag, _Sys_Math_Exception.  If negative, we are letting
--       the system handle the error messages and return asking for default
--       processing.  If someone has changed the flag to 0 or a positive
--       number, we look up the error message, decide if it is important
--       enough to bother with, and then increment _Sys_Math_Exception and
--       set a pointer _Sys_Math_Message to a string indicating the type of
--       error and its library call.  Set value to a default result and return.
--
-- To use in then in a program:
--
--      _Sys_Math_Exception = 0;				Enable my processing
--      while (_Sys_Math_Exception != 0) {
--         ... single library call processing code
--      }
--      if (_Sys_Math_Exception != 0) fputs(_Sys_Math_Message, stderr);
--      _Sys_Math_Exception = -1;			Disable my processing
--
-- Messages DOMAIN, OVERFLOW, SING are trapped and generates errors.
-- Messages UNDERFLOW, TLOSS and PLOSS are ignored.
--------------------------------------------------------------------------- */
typedef struct _table {
	int type;
	char *err;
} ERR_TABLE;

static ERR_TABLE i8087_err_table[] = {
	{-UNDERFLOW,		"Underflow exception (set to zero)"},
	{-TLOSS,				"Total loss of precision (WARNING)"},
	{-PLOSS,				"Partial loss of precision (WARNING)"},
	{DOMAIN,				"Argument out of bounds (domain error)"},
	{OVERFLOW,			"Overflow (what gave THAT BIG a number?)"},
	{SING,				"Argument singularity (you figure it out)"},
	{0x7FFF,				"Unknown math error in library (ask MICROSOFT)"} };

/* ----- Math errors from internal routines ---------- */
#ifdef CSET2
int matherr(struct exception *except) {
	if (_Sys_Math_Exception < 0) return(0);
	if (mathcheck(except->type, except->name)) except->retval = 0;
	return(1);										/* Skip system warning	*/
}
#elif defined MSC70
int _matherr(struct _exception *except) {
	if (_Sys_Math_Exception < 0) return(0);
	if (mathcheck(except->type, except->name)) except->retval = 0;
	return(1);										/* Skip system warning	*/
}
#elif defined MSC60
/* ----- Math errors from internal long routines ---------- */
int _matherrl(struct _exceptionl *except) {
	if (_Sys_Math_Exception < 0) return(0);
	if (mathcheck(except->type, except->name)) except->retval = 0;
	return(1);										/* Skip system warning	*/
}
#endif
/* ----- Combined math handling routines -------- */
static int mathcheck(int type, char *name) {
	ERR_TABLE *aptr=i8087_err_table;

	while (aptr->type != 0x7FFF && abs(aptr->type) != type) aptr++;
	if (aptr->type < 0) return(FALSE);		/* Return default return */

	if (aptr->type == 0x7FFF) {
		sprintf(_Sys_Math_Message, "Math exception in %s(): Unknown code: %0x (ask C folks)\n", name, type);
	} else {
		sprintf(_Sys_Math_Message, "Math exception in %s(): %s\n", name, aptr->err);
	}
	_Sys_Math_Exception++;						/* Add to number of error messages */
	return(TRUE);
}
