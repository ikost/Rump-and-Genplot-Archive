/* gvcalc.c */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define	_POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdio.h>
#if (defined LINUX && ! defined __USE_XOPEN)	/* Need prototype for j0, erfc, etc. */
	#define	__USE_XOPEN								/* Must be placed just before stdlib.h */
#endif
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <math.h>
#include <float.h>
#include <setjmp.h>
#include <time.h>
#include <fcntl.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#ifdef NT
	#undef	_POSIX_
/*	#include <io.h> */
	#include <windows.h>
	#define	_POSIX_
#endif

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#define	GVCALC_C_SOURCE
#define  GV_MATH_EXTENSIONS
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"
#include "gvdefs.h"
#include	"FermiIntegral.h"								/* Functions FDM05, FDP05, FDP15, FDP25 */

#ifdef __GNUC__									/* Disable header inline for Mersenne twist in Linux */
	#define	MT_GENERATE_CODE_IN_HEADER	0	
#endif
#include "../mtwist-1.1/randistrs.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define	REAL_STACK_DIM			200
#define	COMPLEX_STACK_DIM		200
#define	STRING_STACK_DIM		50

#define	DUMMY_VALUE	(-67096211)			/* A value used to test if defaults wanted */

/* Macro used here for efficiency -- every now and then check to verify
 * no unexpected side effects in code -- ie. nint(*(rstackptr++)) */
#define nint(x) ( ((x) >= 0) ? (long) ( (x)+0.5) : - ((long) ((-x)+0.5) ) )

#define EPS_TRIG	1E-12						/* Smallest ATAN2 allowed for Z */

/* Character string stacks */
typedef struct _STRSTACK {
	char *str;												/* Pointer to the string				*/
	enum {IS_STATIC, IS_FUNIT, IS_PFUNIT, IS_VARENTRY, IS_MALLOC} status;
} STRSTACK;
#define	POP_SSTACK	if (sstackptr->status == IS_MALLOC) free(sstackptr->str); sstackptr++


/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */
TMPREAL ERF_S(int key, TMPREAL x);		/* May be linked by others */
TMPREAL NDTRI(TMPREAL p);
TMPREAL BETAI(TMPREAL x, TMPREAL a, TMPREAL b);
TMPREAL BETAI_Ix(TMPREAL x, TMPREAL a, TMPREAL b);
TMPREAL PQ_CHI(TMPREAL x2, TMPREAL v, char type);
#define	ERFCI(p)	 (-NDTRI((p)/2.0)/SQRT(2.0))				/* Make life easy */
#define	ERFI(p)   (NDTRI((1.0+(p))/2.0)/SQRT(2.0))

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
#ifndef HAS_GAMMA
	PRIVATE TMPREAL r_gamma(TMPREAL x);
#endif
PRIVATE TMPREAL norm_gamma(TMPREAL x);
PRIVATE TMPREAL poly_e(TMPREAL x, double *numer, int iorder);
PRIVATE TMPREAL rpoly_e(TMPREAL x, double *numer, int i1, double *denom, int i2);
PRIVATE int     gv_find_in_array(ARRAY *arrayptr, TMPREAL value);
PRIVATE TMPREAL chebyshev(TMPREAL x, REAL *coef, int iorder);
PRIVATE TMPREAL spl_eval(TMPREAL x, ARRAY *arrayptr, int key);
PRIVATE TMPREAL ispl_eval(TMPREAL xlow, TMPREAL xhigh, ARRAY *arrayptr);
PRIVATE TMPREAL p_integrate(CURVE *cv, TMPREAL lower, TMPREAL upper);
PRIVATE TMPREAL find_near_curve(int mode, CURVE *cv, TMPREAL x0, TMPREAL y0, TMPREAL z0);
PRIVATE TMPREAL surface_interpolate(SURFACE *sf, TMPREAL x0, TMPREAL y0);
PRIVATE TMPREAL find_median(REAL *x, int len);
PRIVATE TMPREAL do_sprintf(BOOL is_complex);

PRIVATE TMPREAL HandleOSFnc(unsigned int mycmd);
PRIVATE TMPREAL HandleNumStringFnc(unsigned int mycmd, INT64 p1, INT64 p2, INT64 p3, INT64 p4, INT64 p5);
PRIVATE int     HandleStringFnc(unsigned int mycmd, INT64 p1, INT64 p2, INT64 p3, TMPREAL r1);
PRIVATE TMPREAL HandleLexFnc(unsigned int mycmd, INT64 p1, INT64 p2);
static int HandleCtypeFnc(unsigned int cmd, int iarg);
#ifdef	NT
	PRIVATE TMPREAL HandleLowIOFnc(unsigned int mycmd, int fd, int p1, int p2, int p3, int p4, int p5);
#endif

PRIVATE int real_eval(GVCMDS *cmds);
PRIVATE int complex_eval(GVCMDS *cmds);

static char *REXX_xrange(int start, int end);
static char *REXX_space(char *str, int n, int pad);
static char *REXX_justify(char *str, int length, int pad);
static char *REXX_insert(char *new, char *target, int start, int length, int pad);
static char *REXX_overlay(char *new, char *target, int start, int length, int pad);
static char *REXX_delstr(char *str, int n, int length);
static char *REXX_delword(char *str, int n, int length);
static int   REXX_words(char *str);
static int   REXX_wordpos(char *needle, char *str, int start);
static int   REXX_abbrev(char *tok1, char *tok2, int minlen);
static int   REXX_pos(char *needle, char *haystack, int offset);
static int   REXX_lastpos(char *needle, char *haystack, int start);
static int   REXX_verify(char *str, char *chars, char *mode, int start);
static int   REXX_wordindex(char *str, int start);
static int   REXX_wordlength(char *str, int start);
static int	 REXX_compare(char *s1, char*s2, char pad);

static char *REXX_upcase(char *str);
static char *REXX_lowercase(char *str);
static char *REXX_subword(char *str, int posn, int count);
#define	REXX_word(str, posn)	(REXX_subword(str, posn, 1))
static char *REXX_translate(char *str, char *new, char *old, char pad);
static char *REXX_substr(char *str, int start, int n_chars, char pad);
static char *REXX_strip(char *str, int mode, int achr);
static char *REXX_reverse(char *str);
static char *REXX_left(char *str, int length, int pad);
static char *REXX_right(char *str, int length, int pad);
static char *REXX_center(char *str, int length, int pad);
static char *REXX_concat(char *str1, char *str2);
static char *REXX_char(int achr);
static char *REXX_copies(char *str, int copies);

/* Formerly exists - now renamed as int2hex and hex2int since have many */
/* static int   REXX_x2d(char *str); */
/* static char *REXX_d2x(int ival);  */
static char *my_int2base(UINT64 ival, unsigned int base);
static unsigned int my_base2int(char *str, unsigned int base);

static char *my_hex2bin(char *str);
static char *my_bin2hex(char *str);

static TMPREAL my_hex2float(char *str);
static TMPREAL my_hex2double(char *str);
static TMPREAL my_time2double(char *str);
static char *my_float2hex(TMPREAL rval);
static char *my_double2hex(TMPREAL rval);

static char *my_hex_work(char *str1, char *str2, int op);
static char *my_bit_work(char *str1, char *str2, int op);

/* 48 bit arithmetic random number routines */
#if (defined NT || defined MAC_OSX)
	static double drand48(void);
	static long int lrand48(void);
	static long int mrand48(void);
	static void srand48(long int SeedValue);
#endif

/* Local parameter passing and special routine for @solve() function */
static GVCMDS *solve_cmds;
static TMPREAL solve_fnc_test(double x, int i, int *err);
static TMPREAL subfnc_eval(double x, int *err);
static void z_subfnc_eval(double x, int *err, TMPCOMPLEX *rc);
static int GVIntegrate(double *result, TMPREAL (*fnc)(double x, int *ierr), double a, double b, double eps, int miniter, int maxiter);
static int GVIntegrate_C(TMPCOMPLEX *result, void (*fnc)(double x, int *ierr, TMPCOMPLEX *rc), double a, double b, double eps, int miniter, int maxiter);

/* Local functions */
static TMPREAL JN_ME(TMPREAL nu, TMPREAL x);
static TMPREAL JN_ME_LOW(TMPREAL nu, TMPREAL x);

static TMPREAL digamma(TMPREAL z);
static TMPREAL my_timer(BOOL reset);

static void ShowCmdInfo(int cmd, GVCMDS *cmds);
static void ShowRealCmdStack(TMPREAL *top, TMPREAL *now);
static void ShowComplexCmdStack(TMPCOMPLEX *top, TMPCOMPLEX *now);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
	
/* ---------------------------------------------------------------------------
-- Math exception error handler --
--------------------------------------------------------------------------- */
#if (defined MSC60 || defined CSET2 || defined MSC70)
	void SIGHANDLERTYPE gv_fpe_handler(int sig, int errcode);
#else
	void SIGHANDLERTYPE gv_fpe_handler(int sig);
#endif
PRIVATE jmp_buf math_err_env[10];			/* To allow recursion to 10 levels */
PRIVATE int recurse_level = -1;				/* Current recursion level */
PRIVATE char *fpe_msg=NULL;					/* Float error message    */

/* ---------------------------------------------------------------------------
-- Routine to accept math exceptions and do something with them.  Under OS/2
-- we have a little better chance of actually giving useful messages. 
--------------------------------------------------------------------------- */
#if (defined MSC60 || defined CSET2 || defined MSC70)
	typedef struct _table {
		int type;
		char *err;
	} ERR_TABLE;

	PRIVATE ERR_TABLE FPE_err_table[] = {
		{FPE_INVALID,		"Invalid number (bad argument?)"},
		{FPE_DENORMAL,		"Denormalized result"},
		{FPE_ZERODIVIDE,	"Divide by zero"},
		{FPE_OVERFLOW,		"Overflow exception"},
		{FPE_UNDERFLOW,	"Underflow exception"},
		{FPE_INEXACT,		"Inexact result from calculation"},
		{0x7FFF,				"Other unknown floating point error"} };

	void SIGHANDLERTYPE gv_fpe_handler(int sig, int errcode) {
		ERR_TABLE *aptr=FPE_err_table;

		while (aptr->type != 0x7FFF && abs(aptr->type) != errcode) aptr++;
		fpe_msg = aptr->err;
		_fpreset();											/* Reset coprocessor */
		longjmp(math_err_env[recurse_level], -1);
	}
#else
	void SIGHANDLERTYPE gv_fpe_handler(int sig) {
		fpe_msg = "Exception reported by raise(SIGFPE)";
		longjmp(math_err_env[recurse_level], -1);
	}
#endif

/* ---------------------------------------------------------------------------
-- Routine to skip parameter errors in CRT functions (like close(-1))
-- and do nothing.  Replaces the standard that crashes the program.
-- All arguments are NULL unless the debug version of CRT is compiled.
--------------------------------------------------------------------------- */
#ifdef NT
void ignore_invalid_parms(const wchar_t *expr, const wchar_t *fnc, const wchar_t *file, unsigned int line, uintptr_t pReserved) {
	return;
}
#endif

/* ---------------------------------------------------------------------------
-- Main routine to get the value of a command structure --
--------------------------------------------------------------------------- */
PRIVATE TMPREAL    *rstackptr;							/* Real    stack pointers */
PRIVATE TMPCOMPLEX *cstackptr;							/* Complex stack pointers */
PRIVATE STRSTACK   *sstackptr;							/* String  stack pointers */
PRIVATE int		     subptr, ssubptr;					/* Substack pointer		  */
PRIVATE BOOL		  WarnArray;							/* Possible warn on array bound */

void gv_eval_cmds_ex(GVCMDS *cmds, int *err, TMPCOMPLEX *rvalue, char **strval);

void *gv_eval_ptr_cmds(GVCMDS *cmds, int *err, GVP_PARSEMODE mode) {
	char *rcode=NULL;
	gv_eval_cmds_ex(cmds, err, NULL, &rcode);
	return(rcode);
}

char *gv_eval_str_cmds(GVCMDS *cmds, int *err) {
	char *rcode=NULL;
	gv_eval_cmds_ex(cmds, err, NULL, &rcode);
	return(rcode);
}

TMPCOMPLEX gv_eval_cmds(GVCMDS *cmds, int *err) {
	TMPCOMPLEX rvalue;
	gv_eval_cmds_ex(cmds, err, &rvalue, NULL);
	return(rvalue);
}

PRIVATE TMPREAL	 *rstack_public;
PRIVATE STRSTACK	 *sstack_public;
PRIVATE TMPCOMPLEX *cstack_public;

void gv_eval_cmds_ex(GVCMDS *cmds, int *err, TMPCOMPLEX *rvalue, char **strval) {
	
	TMPREAL	rstack[REAL_STACK_DIM];						/* Eats up space, but is reentrant */
	TMPCOMPLEX	cstack[COMPLEX_STACK_DIM];
	STRSTACK sstack[STRING_STACK_DIM];

	static BOOL FirstWarn = TRUE;							/* First time signal() fails */
	int errcode, itype;
	void (SIGHANDLERTYPE *old_fpe)(int);

#ifdef NT
	_invalid_parameter_handler oldHandler;
#endif

	struct _REENTRANT_HOLD {								/* Structure so truly re-entrant code */
		TMPREAL		*rstack_public;
		STRSTACK		*sstack_public;
		TMPCOMPLEX	*cstack_public;
		TMPREAL	   *rstackptr;
		TMPCOMPLEX	*cstackptr;
		STRSTACK		*sstackptr;
		int			subptr, ssubptr;
		BOOL			WarnArray;
		char			*fpe_msg;
	} hold;

/* Make truely re-entrant by saving all public variables and restoring at end */
	recurse_level++;
	hold.rstack_public = rstack_public;
	hold.sstack_public = sstack_public;
	hold.cstack_public = cstack_public;
	hold.rstackptr	    = rstackptr;
	hold.cstackptr		 = cstackptr;
	hold.sstackptr		 = sstackptr;
	hold.subptr        = subptr;
	hold.ssubptr       = ssubptr;
	hold.WarnArray     = WarnArray;
	hold.fpe_msg       = fpe_msg;

/* Set the default return values */
	if (rvalue != NULL) rvalue->x = rvalue->y = 0;	/* Set result to zero	*/
	if (strval != NULL) *strval = NULL;					/* And this result also	*/

/* Make the stack public knowledge (bad programming, but so what's new) */
	rstack_public = rstack;
	cstack_public = cstack;
	sstack_public = sstack;

/* Scan the cmd structure for information */
	cmds += 2*sizeof(int);									/* Magic + Block length	*/
	GVGETITEM(&itype, cmds, int);							/* Type of expression	*/
	cmds += sizeof(int);										/* And array lengths		*/
	
	if (GVMathMode & MATHFORCEREAL && itype & GV_INFO_COMPLEX_REFERENCE) {
		ERRputs("ERROR: Expression requires complex math, but MATHMODE forced to real\n");
		if (err != NULL) *err = -2;						/* Return error code		*/
		goto FULL_RETURN;
	}

	if ( (old_fpe = signal(SIGFPE, (void(*)(int)) gv_fpe_handler)) == SIG_ERR ) {
		if (FirstWarn) ERRputs("Can't set SIGFPE exception handler (could be bad news!)\n");
		FirstWarn = FALSE;
	}

/* In general, ignore underflow, denorm, and inexact problems */
#ifdef OS2
	_Sys_Math_Exception = 0;
	_control87(0xFFFF, EM_UNDERFLOW | EM_DENORMAL);	
#elif defined NT
	_Sys_Math_Exception = 0;
	_controlfp(EM_UNDERFLOW | EM_DENORMAL | EM_INEXACT, _MCW_EM);
	oldHandler = _set_invalid_parameter_handler(&ignore_invalid_parms);
#endif

	if (setjmp(math_err_env[recurse_level]) != 0) goto FP_ERROR;	/* Error handling	*/

/* Setup up parameters to begin */
	subptr    = 0;												/* Set subpointer to 0	*/
	ssubptr   = 0;												/* Set string subpointer to 0	*/
	WarnArray = TRUE;											/* Reset warnings			*/

	rstackptr = NULL;											/* Not used at this point */
	cstackptr = NULL;
	sstackptr = NULL;
					
	if (itype & (GV_INFO_STRING_REFERENCE | GV_INFO_IS_STRING_EXPR)) {
		sstackptr = sstack + STRING_STACK_DIM - 1;
		sstackptr->status = IS_STATIC;								/* So first not accidentally deleted	*/
	}

	if (GVMathMode & MATHCOMPLEX || itype & GV_INFO_COMPLEX_REFERENCE) {
		cstackptr = cstack + COMPLEX_STACK_DIM - 1;				/* Set point to last		*/
		errcode   = complex_eval(cmds);								/* Start past cnt/type	*/
	} else {
		rstackptr = rstack + REAL_STACK_DIM - 1;					/* Set point to last		*/
		errcode   = real_eval(cmds);									/* Start past cnt/type	*/
	}

/* Assign the return values */
	if (errcode == 0) {
		if (rvalue != NULL && (itype & GV_INFO_IS_NUMERIC_EXPR)) {
			if (cstackptr != NULL) {
				*rvalue = *cstackptr;
			} else {
				rvalue->x = *rstackptr;
			}
		}
		if (strval != NULL) {
			if (itype & GV_INFO_IS_STRING_EXPR) {
				if (sstackptr->status == IS_MALLOC) {
					*strval = sstackptr->str;
					sstackptr->status = IS_STATIC;
				} else if (sstackptr->status == IS_STATIC) {
					*strval = strdup(sstackptr->str);
				}
			} else if (itype & GV_INFO_IS_FUNIT_EXPR) {
				if (sstackptr->status == IS_FUNIT) *strval = sstackptr->str;
			}
		}

/* Pop the appropriate stack expected to have result and make sure stack is empty */
		if (itype & GV_INFO_IS_NUMERIC_EXPR) {
			if (cstackptr != NULL) cstackptr++;					/* Either or */
			if (rstackptr != NULL) rstackptr++;
		}
		if (itype & (GV_INFO_IS_STRING_EXPR | GV_INFO_IS_FUNIT_EXPR)) sstackptr++;
		
		if (cstackptr != NULL && cstackptr != (cstack + COMPLEX_STACK_DIM - 1))
			ERRputs("PROGRAM ERROR: Something left on complex calculation stack (huh?)\n");
		if (rstackptr != NULL && rstackptr != (rstack + REAL_STACK_DIM - 1))
			ERRputs("PROGRAM ERROR: Something left on real calculation stack (huh?)\n");
		if (sstackptr != NULL && sstackptr != (sstack + STRING_STACK_DIM - 1))
			ERRputs("PROGRAM ERROR: Something left on string calculation stack (huh?)\n");
	}

	if (errcode == -2) goto FP_ERROR;
	goto ALL_EXIT;

/* --- ERRORS HANDLING --- */
FP_ERROR:
#if (defined CSET2 || defined MSC60 || defined MSC70)
	{
		char msg[256];
		if (_Sys_Math_Exception > 0) {
			sprintf(msg, "MATH: %s", _Sys_Math_Message);
		} else {
			sprintf(msg, "MATH exception: %s\n", fpe_msg);
		}
		gv_math_error_msg(msg);
	}
#endif
	errcode = 1;
	goto ALL_EXIT;

/* ---- All exit through this code ---- */
ALL_EXIT:
	if (old_fpe != SIG_ERR) signal(SIGFPE, old_fpe);
	if (err != NULL) *err = errcode;
#if (defined CSET2 || defined MSC60 || defined MSC70)
	_Sys_Math_Exception = -1;						/* Internal messages now */
#endif

/* Make truely re-entrant by restoring all public variables and restoring at end */
FULL_RETURN:
	rstack_public = hold.rstack_public;
	sstack_public = hold.sstack_public;
	cstack_public = hold.cstack_public;
	rstackptr     = hold.rstackptr;
	cstackptr	  = hold.cstackptr;
	sstackptr	  = hold.sstackptr;
	subptr        = hold.subptr;
	ssubptr       = hold.ssubptr;
	WarnArray     = hold.WarnArray;
	fpe_msg       = hold.fpe_msg;
	recurse_level--;

#ifdef NT
	_set_invalid_parameter_handler(oldHandler);
#endif

	return;
}

/* ===========================================================================
-- Routine for printing math warning messages, and shutting up after
-- too many.
--
-- Notes: Because an eval on $GVMathErrorsCnt will clear the error counter,
--        this variable is *only* set when there is an error.  Holds the
--        last non-zero count of error messages, but is never set to zero.
=========================================================================== */
#define	MAX_ERROR_REPORTS		(25)
void gv_array_bounds_msg(int idx, int size) {
	char msg[256];
	sprintf(msg, "ARRAY index [%d] out of bounds.  Must satisfy 0 <= [idx] < %d\n", idx, size);
	gv_math_error_msg(msg);
	return;
}

void gv_math_error_msg(char *msg) {
	static int err_counter = 0;							/* GVMathErrorCounter set to 0 by GVParse		*/
	static int peak_errors = 0;							/* Number when greater than <MAX> per eval	*/
	static BOOL init = FALSE;

	/* First time through, define the internal variable to count errors */
	if (! init) {
		GVLinkInt("$GVMathErrorCnt", GVF_INTERNAL | GVF_NODELETE, &peak_errors);
		init = TRUE;
	}

	/* NULL message is an internal request to zero the counter */
	if (msg == NULL) { err_counter = 0; return; }

/* Otherwise, print message but stop after a reasonable number */
	peak_errors = ++err_counter;							/* Keep updated after the fact only */
	if (GVMathMode & MATH_NO_CALC_MSG) return;		/* No printed message */

	if (err_counter <= MAX_ERROR_REPORTS) ERRputs(msg);
	if (err_counter == MAX_ERROR_REPORTS) 
		ERRprintf("ERROR: %d math errors have been reported - no further messages will print.\n"
					 "       Total number of errors stored in variable $GVMathErrorCnt.\n", err_counter);
	return;
}


/* ===========================================================================
-- Routine to evaluate the command stack in real space
--
-- Usage:  errcode = real_eval(GVCMDS *cmds);
--
-- Inputs: cmds - parsed stack of commands
--
-- Returns: errcode - 0 ==> success
--                  -1 ==> stack underflow (unknown fatal condition)
--                  -2 ==> math exception (OS/2 only)
--
-- Stack: value of expression (real part only) on *rstackptr
=========================================================================== */
static int TMPREAL_Compare(const void *a, const void *b) {
	if (*((TMPREAL *) a) < *((TMPREAL *) b)) return -1;
	if (*((TMPREAL *) a) > *((TMPREAL *) b)) return +1;
	return 0;
}
static int TMPCOMPLEX_Compare(const void *a, const void *b) {
	TMPCOMPLEX *c,*d;
	c = ((TMPCOMPLEX *) a);	d = ((TMPCOMPLEX *) b);
	if (c->x < d->x) return -1;
	if (c->x > d->x) return +1;
	if (c->y < d->y) return -1;
	if (c->y > d->y) return +1;
	return 0;
}

PRIVATE int real_eval(GVCMDS *cmds) {

	int     rcode;										/* Return code						*/

	int     subptr_ini     = subptr;				/* Initial values of pointers */
	int     ssubptr_ini    = ssubptr;			/* Initial values of pointers */
	TMPREAL  *rstackptr_ini = rstackptr;		/* for checking range errors	*/
	STRSTACK *sstackptr_ini = sstackptr;		/* for checking range errors	*/

	unsigned int mycmd;
	ptrdiff_t jump_length;
	int icnt, imin, imax;
	REAL xmin, xmax;
	long i;
	TMPREAL a,b;
	TMPREAL xp,yp,zp;
	TMPREAL r,r2,x,x0,sigma,width,skew,kurt,n,p,beta,eta,gamma;
	TMPREAL v,t,v1,v2,F,chi2;

	COMPLEX       *complexptr;
	REAL          *realptr;						/* Pointers stored in cmds	*/
	DOUBLE        *doubleptr;
	CURVE			  *curveptr;
	SURFACE		  *surfaceptr;
	INT	        *intptr;
	EXT_FNC_LINK  *fnc;
	EXT_FNCA_LINK *fnca;
	EXT_STR_FNC_LINK *sfnc;
	ARRAY         *arrayptr, *arrayptr2;
	COMPLEX_ARRAY *c_arrayptr;
	INT_ARRAY	  *i_arrayptr;
	DOUBLE_ARRAY  *d_arrayptr;
	STRING_ARRAY  *s_arrayptr;
	char			  *stringptr;
	void			  *avoidptr;
	FILE			  **pfileptr;				/* Pointer to file pointer */

	if (SysDebugFlag & 0x06) TTYprintf("----------------------------\n");

	rcode = 0;									/* Default return value */
 	while (*cmds != 0xFF) {
		if ( (mycmd = *(cmds++)) == 254) mycmd = *(cmds++)+250;
		if (SysDebugFlag & 0x06) ShowCmdInfo(mycmd, cmds);

		switch (mycmd) {

		case load_immed_real:
			GVGETITEM(--rstackptr, cmds, TMPREAL);
			break;
		case load_immed_imag:
			GVGETITEM(--rstackptr, cmds, TMPREAL);
			break;

		case add_me:
			rstackptr[1] += rstackptr[0]; rstackptr++; break;
		case sub_me:
			rstackptr[1] -= rstackptr[0]; rstackptr++; break;
		case mul_me:
			rstackptr[1] *= rstackptr[0]; rstackptr++; break;
		case div_me:
			rstackptr[1] /= rstackptr[0]; rstackptr++; break;
		case pow_me:
			rstackptr[1] = POW(rstackptr[1],rstackptr[0]); rstackptr++;
			break;

		case lt_me:
			rstackptr[1] = (rstackptr[1] <  rstackptr[0]) ? 1 : 0 ; 
			rstackptr++;	break;
		case le_me:
			rstackptr[1] = (rstackptr[1] <= rstackptr[0]) ? 1 : 0 ;
			rstackptr++;	break;
		case ne_me:
			rstackptr[1] = (rstackptr[1] != rstackptr[0]) ? 1 : 0 ;
			rstackptr++; break;
		case gt_me:
			rstackptr[1] = (rstackptr[1] >  rstackptr[0]) ? 1 : 0 ;
			rstackptr++; break;
		case ge_me:
			rstackptr[1] = (rstackptr[1] >= rstackptr[0]) ? 1 : 0 ; 
			rstackptr++; break;
		case eq_me:
			rstackptr[1] = (rstackptr[1] == rstackptr[0]) ? 1 : 0 ; 
			rstackptr++; break;

		case not_me:
			*rstackptr = (*rstackptr == 0) ? 1 : 0 ;
			break;
		case and_me:
			rstackptr[1] = ( (rstackptr[1]!=0) && (rstackptr[0]!=0) ) ? 1 : 0 ;
			rstackptr++; break;
		case or_me:
			rstackptr[1] = ( (rstackptr[1]!=0) || (rstackptr[0]!=0) ) ? 1 : 0 ; 
			rstackptr++; break;
		case eqv_me:
			rstackptr[1] = ( (rstackptr[1]!=0) == (rstackptr[0]!=0) ) ? 1 : 0 ; 
			rstackptr++; break;
		case neqv_me:
			rstackptr[1] = ( (rstackptr[1]!=0) == (rstackptr[0]!=0) ) ? 0 : 1 ; 
			rstackptr++; break;

		case bit_and_me:
			rstackptr[1] = nint(rstackptr[0]) & nint(rstackptr[1]);
			rstackptr++; break;
		case bit_or_me:
			rstackptr[1] = nint(rstackptr[0]) | nint(rstackptr[1]);
			rstackptr++; break;
		case bit_eor_me:
			rstackptr[1] = nint(rstackptr[0]) ^ nint(rstackptr[1]);
			rstackptr++; break;
		case bit_not_me:
			*rstackptr = ~nint(*rstackptr);
			break;

		case conditional_me:
			GVGETITEM(&jump_length, cmds, ptrdiff_t);
			if (*rstackptr <= 0) cmds += jump_length;
			rstackptr++;	break;
		case jump_me:
			GVGETITEM(&jump_length, cmds, ptrdiff_t);
			cmds += jump_length;
			break;

/* Function calls - Must match order of FNCS declarations in code. */
		case chs_me:
			*rstackptr = -(*rstackptr);						break;
		case abs_me:
			*rstackptr = FABS(*rstackptr);					break;
		case sign_me:
			if (*rstackptr != 0) *rstackptr = (*rstackptr >= 0) ? 1 : -1;
			break;
		case limit_me:
			if (rstackptr[0] < rstackptr[1]) { x = rstackptr[0]; rstackptr[0] = rstackptr[1]; rstackptr[1] = x; }
			if (rstackptr[2] > rstackptr[0]) rstackptr[2] = rstackptr[0];
			if (rstackptr[2] < rstackptr[1]) rstackptr[2] = rstackptr[1];
			rstackptr += 2; break;
		case min_me:
			rstackptr[1] = min(rstackptr[0], rstackptr[1]); 
			rstackptr++; break;
		case max_me:
			rstackptr[1] = max(rstackptr[0], rstackptr[1]); 
			rstackptr++; break;

		case ave_me:
			GVGETITEM(&icnt, cmds, int);
			for (a=0,i=0; i<icnt; i++) a += *(rstackptr++);
			*(--rstackptr) = a/icnt;
			break;
		case std_me:
			GVGETITEM(&icnt, cmds, int);
			for (a=b=0,i=0; i<icnt; i++) { a += *rstackptr; b += pow(*rstackptr,2); rstackptr++; }
			*(--rstackptr) = (icnt <= 1) ? 0 : sqrt((b-a*a/icnt)/(icnt-1.0));
			break;
		case sdom_me:
			GVGETITEM(&icnt, cmds, int);
			for (a=b=0,i=0; i<icnt; i++) { a += *rstackptr; b += pow(*rstackptr,2); rstackptr++; }
			*(--rstackptr) = (icnt <= 1) ? 0 : sqrt((b-a*a/icnt)/(icnt-1.0)/icnt);
			break;
		case median_me:
		case mad_me:
			GVGETITEM(&icnt, cmds, int);
			qsort(rstackptr, icnt, sizeof(*rstackptr), &TMPREAL_Compare);
			if ( (icnt & 0x01) == 1) {						/* Odd # of points, just take middle */
				a = rstackptr[icnt/2];						/* midpoint */
			} else {
				a = (rstackptr[icnt/2-1]+rstackptr[icnt/2])/2;
			}
			if (mycmd == mad_me) {							/* More to do */
				for (i=0; i<icnt; i++) rstackptr[i] = fabs(rstackptr[i]-a);		/* Get absolute deviation */
				qsort(rstackptr, icnt, sizeof(*rstackptr), &TMPREAL_Compare);	/* Sort again */
				if ( (icnt & 0x01) == 1) {					/* Odd # of points, just take middle */
					a = rstackptr[icnt/2];					/* midpoint */
				} else {
					a = (rstackptr[icnt/2-1]+rstackptr[icnt/2])/2;
				}
			}
			rstackptr += icnt;
			*(--rstackptr) = a;
			break;

		case count_me:
			GVGETITEM(&icnt, cmds, int);
			rstackptr += icnt;
			*(--rstackptr) = (TMPREAL) icnt;
			break;

		case int_me:
			*rstackptr = ((long) *rstackptr);
			break;
		case nint_me:
			*rstackptr = (TMPREAL) nint(*rstackptr);
			break;
		case frac_me:
			*rstackptr = FMOD(*rstackptr, 1.0);
			break;
		case mod_me:
			rstackptr[1] = FMOD(rstackptr[1], rstackptr[0]);
			rstackptr++; break;
		case mantissa_me:
			*rstackptr = *rstackptr / pow(10, FLOOR(LOG(FABS(*rstackptr))/LOG(10.0))); break;
		case exponent_me:
			*rstackptr = FLOOR(LOG(FABS(*rstackptr))/LOG(10.0)); break;
		case m1n_me:						/* Minus 1 to the nth power */
			i = nint(*rstackptr);
			*rstackptr = (i%2 == 0) ? 1 : -1 ;
			break;
		case real_me:
		case conj_me:
			break;
		case imag_me:						/* Both imaginary and arg return 0 */
		case arg_me:
			*rstackptr = 0;
			break;

		case sin_me:
			*rstackptr = SIN(*rstackptr); break;
		case cos_me:
			*rstackptr = COS(*rstackptr); break;
		case tan_me:
			*rstackptr = TAN(*rstackptr); break;
		case cot_me:
			*rstackptr = 1.0/TAN(*rstackptr); break;
		case sind_me:
			*rstackptr = SIN(*rstackptr*DEGREES_TO_RADIANS); break;
		case cosd_me:
			*rstackptr = COS(*rstackptr*DEGREES_TO_RADIANS); break;
		case tand_me:
			*rstackptr = TAN(*rstackptr*DEGREES_TO_RADIANS); break;
		case cotd_me:
			*rstackptr = 1.0/TAN(*rstackptr*DEGREES_TO_RADIANS); break;
		case asin_me:
			*rstackptr = ASIN(*rstackptr); break;
		case acos_me:
			*rstackptr = ACOS(*rstackptr); break;
		case atan_me:
			*rstackptr = ATAN(*rstackptr); break;
		case acot_me:
			*rstackptr = (*rstackptr == 0) ? PI/2.0 : ATAN(1.0/(*rstackptr)); break;
		case atan2_me:
			rstackptr[1] = ATAN2(rstackptr[1],rstackptr[0]);
			rstackptr++; break;

		case asind_me:
			*rstackptr = RADIANS_TO_DEGREES*ASIN(*rstackptr); break;
		case acosd_me:
			*rstackptr = RADIANS_TO_DEGREES*ACOS(*rstackptr); break;
		case atand_me:
			*rstackptr = RADIANS_TO_DEGREES*ATAN(*rstackptr); break;
		case acotd_me:
			*rstackptr = RADIANS_TO_DEGREES*( (*rstackptr == 0) ? PI/2.0 : ATAN(1.0/(*rstackptr))); break;
		case atan2d_me:
			rstackptr[1] = RADIANS_TO_DEGREES*ATAN2(rstackptr[1],rstackptr[0]);
			rstackptr++; break;

		case sinh_me:
		case csch_me:
			*rstackptr = SINH(*rstackptr); 
			if (mycmd == csch_me) *rstackptr = 1.0/(*rstackptr);
			break;
		case cosh_me:
		case sech_me:
			*rstackptr = COSH(*rstackptr); 
			if (mycmd == sech_me) *rstackptr = 1.0/(*rstackptr);
			break;
		case tanh_me:
		case coth_me:
			*rstackptr = TANH(*rstackptr);
			if (mycmd == coth_me) *rstackptr = 1.0/(*rstackptr);
			break;

		case acsch_me:
			*rstackptr = 1.0/(*rstackptr);
		case asinh_me:
			*rstackptr = LOG(*rstackptr + SQRT((*rstackptr)*(*rstackptr)+1) );
			break;

		case asech_me:
			*rstackptr = 1.0/(*rstackptr);
		case acosh_me:
			*rstackptr = LOG(*rstackptr + SQRT((*rstackptr)*(*rstackptr)-1) );
			break;

		case acoth_me:
			*rstackptr = 1.0/(*rstackptr);
		case atanh_me:
			*rstackptr = 0.5*LOG( (1+(*rstackptr)) / (1-(*rstackptr)) );
			break;

		case ln_me:
			*rstackptr = LOGZ(*rstackptr); break;
		case log_me:
			*rstackptr = LOG10Z(*rstackptr); break;
		case exp_me:
			*rstackptr = EXP(*rstackptr); break;
		case sqrt_me:
			*rstackptr = SQRT(*rstackptr); break;

		case fact_me:						/* Factorial */
			i = nint(*rstackptr);
			if (i >= 0 && i < 171) {
				for (*rstackptr=1.0; i>1; i--) *rstackptr *= i;
			} else {
				*rstackptr = TMPREAL_MAX;
			}
			break;
		case gamma_me:						/* Gamma function */
			*rstackptr = norm_gamma(*rstackptr);
			break;
		case lngamma_me:
			if (*rstackptr < 0) {							/* Handle negative alone */
				x = *rstackptr;								/* See r_gamma() for		*/
				r = x-FLOOR(x);								/* mod(|x|,1) basically	*/
				*rstackptr = (r == 0.0) ? 0 :				/* Really +/- INF			*/
					LN_GAMMA(1-r) + LN_GAMMA(r) - LN_GAMMA(1-x);
			} else {
				*rstackptr = LN_GAMMA(*rstackptr);
			}
			break;
		case digamma_me:					/* Digamma function */
			*rstackptr = digamma(*rstackptr);
			break;

		case rnd_me:						/* Random numbers */
			rstackptr--;					/* Now no arguments */
			*rstackptr = ((TMPREAL) rand()) / (RAND_MAX+1.0); break;

		case srand_me:
			srand((unsigned int) nint(*rstackptr));
			break;

		case drand48_me:					/* Random numbers */
			rstackptr--;					/* Now no arguments */
			*rstackptr = (TMPREAL) drand48(); break;

		case lrand48_me:					/* Random numbers */
			rstackptr--;					/* Now no arguments */
			*rstackptr = (TMPREAL) lrand48(); break;

		case mrand48_me:					/* Random numbers */
			rstackptr--;					/* Now no arguments */
			*rstackptr = (TMPREAL) mrand48(); break;

		case srand48_me:
			srand48((long int) nint(*rstackptr));
			break;

		case rnd_seed_me:
			if (*rstackptr != -1) {
				mt_seed32new(nint(*rstackptr));
			} else {
				mt_goodseed();
			}
			*rstackptr = 0;
			break;
		case rnd_lrand_me:
			rstackptr--;
			*rstackptr = (TMPREAL) mt_lrand()/2;
			break;
		case rnd_drand_me:
			rstackptr--;
			*rstackptr = (TMPREAL) mt_ldrand();
			break;
		case rnd_iuniform_me:
			rstackptr[1] = (TMPREAL) rd_iuniform(nint(rstackptr[1]), nint(rstackptr[0]));
			rstackptr++;
			break;
		case rnd_uniform_me:
			rstackptr[1] = (TMPREAL) rd_luniform(rstackptr[1], rstackptr[0]);
			rstackptr++;
			break;
		case rnd_exponential_me:
			*rstackptr = (TMPREAL) rd_lexponential(*rstackptr);
			break;
		case rnd_erlang_me:
			rstackptr[1] = (TMPREAL) rd_lerlang(nint(rstackptr[1]), rstackptr[0]);
			rstackptr++;
			break;
		case rnd_weibull_me:
			rstackptr[1] = (TMPREAL) rd_lweibull(rstackptr[1], rstackptr[0]);
			rstackptr++;
			break;
		case rnd_norm_me:
			rstackptr--;
			*rstackptr = (TMPREAL) rd_lnormal(0.0, 1.0);
			break;
		case rnd_normal_me:
			rstackptr[1] = (TMPREAL) rd_lnormal(rstackptr[1], rstackptr[0]);
			rstackptr++;
			break;
		case rnd_lognormal_me:
			rstackptr[1] = (TMPREAL) rd_llognormal(rstackptr[0], rstackptr[1]);		/* Reversed intentionally */
			rstackptr++;
			break;
		case rnd_triangle_me:
			rstackptr[2] = (TMPREAL) rd_ltriangular(rstackptr[2], rstackptr[1], rstackptr[0]);
			rstackptr += 2;
			break;

		case ceil_me:
			*rstackptr = CEIL(*rstackptr); break;

		case floor_me:
			*rstackptr = FLOOR(*rstackptr); break;

		case ldexp_me:
			rstackptr[1] = LDEXP(rstackptr[1], GVTrimToInt32(rstackptr[0])); 
			rstackptr++; break;

		case round_me:
			r = *rstackptr;	rstackptr++;			/* Requested number of digits */
			if (r > 300 || r < -300) break;
			r = POW(10.0, nint(r));						/* Number of digits */
			x = *rstackptr*r;
			if (FABS(FMOD(x,1.0)) == 0.5) x += .1*(rand()-RAND_MAX/2.0)/RAND_MAX;
			if (x < 0) 
			  *rstackptr = FLOOR(x+0.5) / r;
			else
				*rstackptr =  CEIL(x-0.5) / r;
			break;

		case rainbow_me:
			rstackptr[2] = GVSelectContinuumColor((double) rstackptr[2], (double) rstackptr[1], (double) rstackptr[0], GV_PAL_DEFAULT);
			rstackptr += 2;
			break;

		case rgb_me:
			rstackptr[2] = MY_RGB(GVTrimToInt16(rstackptr[2]), GVTrimToInt16(rstackptr[1]), GVTrimToInt16(rstackptr[0]) );
			rstackptr += 2;
			break;

#ifdef HAS_BESSEL
		case j0_me:
			*rstackptr = J0(*rstackptr); break;

		case j1_me:
			*rstackptr = J1(*rstackptr); break;

		case jn_me:
			if (rstackptr[1] != (TMPREAL) GVTrimToInt32(rstackptr[1])) {
				if (rstackptr[0] < 0) {
					gv_math_error_msg("ERROR: Bessel functions of fractional order with x<0 are imaginary\n");
					rstackptr[1] = 0;
				} else {
					rstackptr[1] = JN_ME(rstackptr[1], rstackptr[0]);
				}
			} else {
				rstackptr[1] = JN((int) nint(rstackptr[1]), rstackptr[0]);
			}
			rstackptr++; break;

		case y0_me:
			*rstackptr = Y0(*rstackptr); break;

		case y1_me:
			*rstackptr = Y1(*rstackptr); break;

		case yn_me:
			rstackptr[1] = YN((int) nint(rstackptr[1]), rstackptr[0]); 
			rstackptr++; break;
#endif	/* HAS_BESSEL */

		case tn_me:									/* Chebyshev polynomials	*/
			i = nint(rstackptr[1]);				/* Get order of chebyshev	*/
			r  = *rstackptr;						/* X value						*/
			
			if (i <= 0) {							/* t0(x) = 0					*/
				r = 1;
			} else if (i == 1) {
				r = r;
			} else if (i <= 10) {				/* For i=2,10, recursion	*/
#ifndef OS2
				TMPREAL tn_1=1, tn=r, th;		/* t0(x) & t1(x)				*/
				while (--i != 0) {th = tn;  tn = 2*r*tn - tn_1;  tn_1 = th;}
				r = tn;
#else
				ERRprintf("ERROR: OS/2 Compiler error prevents calculation of Chebyshev polynomials\n");
				r = 1;
#endif
			} else if (FABS(r) < 1) {			/* High order range okay	*/
				r = cos(i*acos(r));				
			} else {									/* High order, range extended */
				r = COSH(i*LOG(FABS(r)+SQRT(r*r-1)));
				if (i%2==1 && *rstackptr<0) r = -r;
			}
			*(++rstackptr) = r;
			break;
			
		case erf_me:
			*rstackptr = ERF(*rstackptr);
			break;
		case erfc_me:
			*rstackptr = ERFC(*rstackptr);
			break;
		case erfi_me:
			*rstackptr = ERFI(*rstackptr);
			break;
		case erfci_me:
			*rstackptr = ERFCI(*rstackptr);
			break;
		case lnerfc_me:
			*rstackptr = ERF_S(4,*rstackptr); break;
		case ndtr_me:
			*rstackptr = NDTR(*rstackptr); break;
		case ndtri_me:
			*rstackptr = NDTRI(*rstackptr); break;
		case fdm0p5_me:
			*rstackptr = FDM0P5(*rstackptr); break;
		case fdp0p5_me:
			*rstackptr = FDP0P5(*rstackptr); break;
		case fdp1p5_me:
			*rstackptr = FDP1P5(*rstackptr); break;
		case fdp2p5_me:
			*rstackptr = FDP2P5(*rstackptr); break;
		case beta_me:										/* Beta function */
		case lnbeta_me:									/* Beta function */
			b = *rstackptr++;
			a = *rstackptr;
			*rstackptr = LN_GAMMA(a)+LN_GAMMA(b)-LN_GAMMA(a+b);
			if (mycmd == beta_me) *rstackptr = EXP(*rstackptr);
			break;
			
		case betai_me:										/* B(a,b,x) function */
		case betai_Ix_me:
			b = *rstackptr++;
			a = *rstackptr++;
			x = *rstackptr;
			*rstackptr = (mycmd == betai_me) ? BETAI(x, a, b) : BETAI_Ix(x, a, b);
			break;

		case z_test_me:							/* Z test when mean/sigma known for population */
			/* rstackptr[0] = sigma; rstackptr[1] = mean; */
			GVGETITEM(&icnt, cmds, int);		/* Number of elements in array */
			GVGETITEM(&x, cmds, TMPREAL);		/* Mean of array */
			if (icnt <= 1) {
				t = 0.0;
			} else {
				t = (x-rstackptr[1]) / (rstackptr[0]/sqrt(icnt));
				t = 2*NDTR(FABS(t)) - 1.0;
			}
			rstackptr++;							/* Pop sigma */
			*rstackptr = t;
			break;

		case t1_test_me:							/* t-test with one array and mean */
			/* rstackptr[0] = mean; */
			GVGETITEM(&icnt, cmds, int);		/* Number of elements in array */
			GVGETITEM(&x, cmds, TMPREAL);		/* Mean of array */
			GVGETITEM(&r, cmds, TMPREAL);		/* Sigma of the array */
			if (icnt <= 1) {
				t = 1.0;
			} else {
				v = icnt-1;											/* Degrees of freedom	*/
				t = (x-rstackptr[0]) / (r/sqrt(icnt));		/* t value in t-test		*/
				t = 1.0 - BETAI_Ix(v/(v+t*t), v/2, 0.5);
			}
			*rstackptr = t;
			break;

		case t_test_me:							/* Student's t-Distribution	*/
			v = *rstackptr++;						/* Degrees of freedom (total) */
			t = *rstackptr;						/* Difference of the mean		*/
			*rstackptr = 1.0 - BETAI_Ix(v/(v+t*t), v/2, 0.5);
			break;

		case f_test_me:							/* F-Distribution function			*/
			v2 = *rstackptr++;					/* Degrees of freedom on 1st set */
			v1 = *rstackptr++;					/* Degrees of freedom on 2nd set */
			F  = *rstackptr;						/* Statistic F							*/
			*rstackptr = BETAI_Ix(v2/(v2+v1*F), v2/2, v1/2);
			break;

		case chi2_me:
			v = *rstackptr++;
			t = *rstackptr;
			if (t <= 0) {
				*rstackptr = 0;
			} else {
				*rstackptr = POW(t/2,v/2)/t*EXP(-t/2)/norm_gamma(v/2);
			}
			break;

		case p_chi_me:
		case q_chi_me:
			v    = *rstackptr++;					/* Number of degrees of freedom */
			chi2 = *rstackptr;					/* chi^2 measured */
			*rstackptr = PQ_CHI(chi2, v, (char) ((mycmd == p_chi_me) ? 'P' : 'Q') );
			break;

		case gnoise_me:
			rstackptr--;							/* Now no arguments */
			*rstackptr = NDTRI(drand48());
			while (fabs(*rstackptr) > 8) *rstackptr = NDTRI(drand48());
			break;
		case gauss_me:
		case gaussn_me:
			sigma = *rstackptr++;
			x0    = *rstackptr++;
			x     = *rstackptr;
			if (sigma > 0) {
				*rstackptr = EXP(-0.5*POW((x-x0)/sigma,2));
				if (mycmd == gauss_me) *rstackptr /= (SQRT(2*PI)*sigma);
			} else {
				*rstackptr = (x == x0) ? 1 : 0 ;
			}
			break;
		case poisson_me:
			x0 = *rstackptr++;
			x  = *rstackptr;
			*rstackptr = EXP(x*LOG(x0)-LN_GAMMA(x+1)-x0);
			break;
		case lorentz_me:
			width = FABS(*rstackptr++);			/* Width */
			x0    = *rstackptr++;					/* Average */
			x     = *rstackptr;
			*rstackptr = width/2/PI / (POW(x-x0,2)+POW(width/2,2));
			break;
		case binomial_me:
			p = *rstackptr++;
			n = *rstackptr++;
			x = *rstackptr;
			if (x >= 0 && x <= n && p >= 0 && p <= 1.0) {
				*rstackptr = EXP(LN_GAMMA(n+1)-LN_GAMMA(x+1)-LN_GAMMA(n-x+1) + x*LOG(p)+(n-x)*LOG(1-p));
			} else {
				*rstackptr = 0;
			}
			break;

		case edgeworth_me:
			kurt  = *rstackptr++;
			skew  = *rstackptr++;
			sigma = *rstackptr++;
			x0    = *rstackptr++;
			x     = *rstackptr;
			if (sigma > 0) {
				r  = (x - x0) / sigma;
				r2 = r*r;
				*rstackptr = EXP(-r2/2) / sigma / SQRT(2*PI) * (1 +
					skew/6 * (r*(r2-3)) + kurt/24 * ((r2-6)*r2+3) +
					skew*skew/72 * (((r2-15)*r2+45)*r2-15) );
			} else {
				*rstackptr = (x == x0) ? 1 : 0 ;
			}
			break;

		case weibull_me:
			beta  = *rstackptr++;
			eta   = *rstackptr++;
			gamma = *rstackptr++;
			x     = *rstackptr;
			if (beta <= 0 || eta <= 0 || x <= gamma) {
				*rstackptr = 0;
			} else {
				*rstackptr = beta/eta * POW((x-gamma)/eta,beta-1) * EXP(-POW((x-gamma)/eta,beta));
			}
			break;

		case spline_me:
			GVGETITEM(&arrayptr, cmds, ARRAY *);
			*rstackptr = spl_eval(*rstackptr, arrayptr, 0);
			break;
		case ispln_me:
			GVGETITEM(&arrayptr, cmds, ARRAY *);
			rstackptr[1] = ispl_eval(rstackptr[1], rstackptr[0], arrayptr);	/* Integral */
			rstackptr++; break;
		case dspln_me:
			GVGETITEM(&arrayptr, cmds, ARRAY *);
			*rstackptr = spl_eval(*rstackptr, arrayptr, 1);
			break;
		case ddspln_me:
			GVGETITEM(&arrayptr, cmds, ARRAY *);
			*rstackptr = spl_eval(*rstackptr, arrayptr, 2);
			break;

		case hv_me:
			if (*rstackptr == 0.0)
				*rstackptr = 0.5;
			else if (*rstackptr < 0.0) 
				*rstackptr = 0;
			else
				*rstackptr = 1;
			break;

		case cheby_me:									/* cheby(x,array) */
			GVGETITEM(&arrayptr, cmds, ARRAY *);
			*rstackptr = chebyshev(*rstackptr, arrayptr->x, *(arrayptr->size));
			break;
			
/* String functions */
		case interp_string_stack_val:
			{
				char *str, *endstr;
				str = sstack_public[*(cmds++)].str;
				--rstackptr;
				*rstackptr = strtod(str, &endstr);
				while (isspace(*endstr)) endstr++;
				if (*endstr != '\0') {
					ERRprintf("WARNING: String \"%s\" interpreted as float - leftover \"%s\" ignored\n", 
								 str, endstr);
				}
			}
			break;
			
		case rgb_color_me:							/* These are string only */
		case strcmp_me:	
		case stricmp_me:
		case strlen_me:
		case strnlen_me:
		case strcspn_me:
		case strspn_me:
		case hex2int_me:
		case oct2int_me:
		case bin2int_me:
		case hex2float_me:
		case hex2double_me:
		case time2double_me:
		case rexx_words:
		case rexx_ichar:
		case atof_me:
		case atoi_me:
		case isatoi_me:
		case isatof_me:
		case file_sizeof_me:
		case file_dateof_me:
		case file_isfile_me:
		case file_isdir_me:
		case fclose_me:
		case pclose_me:
		case feof_me:
		case ferror_me:
		case fflush_me:
		case ftell_me:
		case fgetc_me:
		case fputs_me:
			*(--rstackptr) = HandleNumStringFnc(mycmd, 0, 0, 0, 0, 0);
			break;
		case strncmp_me:								/* These use a single int value */	
		case strnicmp_me:
		case lexequal_me:
		case strtol_me:
		case rexx_abbrev:
		case rexx_wordpos:
		case rexx_pos:
		case rexx_lastpos:
		case rexx_wordindex:
		case rexx_wordlength:
		case fputc_me:
		case base2int_me:
		case rexx_verify:								/* These use two int values */
		case rexx_compare:
			*rstackptr = HandleNumStringFnc(mycmd, GVTrimToInt64(*rstackptr), 0, 0, 0, 0);
			break;
		case fseek_me:
			rstackptr[1] = HandleNumStringFnc(mycmd, GVTrimToInt64(rstackptr[1]), GVTrimToInt64(rstackptr[0]), 0, 0, 0);
			++rstackptr;
			break;
		case sprintf_me:
			do_sprintf(FALSE);						/* do_sprintf will modify rstackptr */
			break;
		case fprintf_me:
			do_sprintf(FALSE);						/* do_sprintf will modify rstackptr */
			*(--rstackptr) = HandleNumStringFnc(fprintf_me, 0, 0, 0, 0, 0);
			break;
		case printf_me:
			do_sprintf(FALSE);						/* do_sprintf will modify rstackptr */
			printf("%s", sstackptr[0].str);
			if (sstackptr->status == IS_MALLOC) free(sstackptr->str); 
			sstackptr++;
			*(--rstackptr) = 0;
			break;
#ifdef NT
		/* No fd, 1 int, return int */
		case creat_me:									/* Create file descriptor			*/
			*rstackptr = HandleLowIOFnc(mycmd, 0, GVTrimToInt32(*rstackptr), 0, 0, 0, 0);
			break;
		/* No fd, 2 ints, return int */
		case open_me:									/* Open file descriptor				*/
		case open_comx_me:
		case beep_me:
			rstackptr[1] = HandleLowIOFnc(mycmd, 0, GVTrimToInt32(rstackptr[1]), GVTrimToInt32(rstackptr[0]), 0, 0, 0);
			rstackptr++;
			break;
		/* fd, 0 ints, return int */
		case set_baud_me:								/* These need no integer parameters */
		case write_me:									/* Write to file descriptor		*/
			*rstackptr = HandleLowIOFnc(mycmd, GVTrimToInt32(*rstackptr), 0, 0, 0, 0, 0);
			break;
		/* fd, 0 ints, return string */
		case get_baud_me:								/* Returns string value */
			HandleLowIOFnc(mycmd, GVTrimToInt32(*rstackptr), 0, 0, 0, 0, 0);
			rstackptr++;
			break;
		/* fd, 1 int, return int */
		case get_timeout_me:							/* Get a timeout parameter			*/
			rstackptr[1] = HandleLowIOFnc(mycmd, GVTrimToInt32(rstackptr[1]), GVTrimToInt32(rstackptr[0]), 0, 0, 0, 0);
			rstackptr++;
			break;
		/* fd, 5 ints, return int */
		case set_timeout_me:
			rstackptr[5] = HandleLowIOFnc(mycmd, GVTrimToInt32(rstackptr[5]), GVTrimToInt32(rstackptr[4]), GVTrimToInt32(rstackptr[3]), GVTrimToInt32(rstackptr[2]), GVTrimToInt32(rstackptr[1]), GVTrimToInt32(rstackptr[0]) );
			rstackptr += 5;
			break;
		/* fd, 1 int, return string */
		case read_me:									/* Read from file descriptor		*/
		case query_me:									/* Read from file descriptor		*/
			HandleLowIOFnc(mycmd, GVTrimToInt32(rstackptr[1]), GVTrimToInt32(rstackptr[0]), 0, 0, 0, 0);
			rstackptr+= 2;
			break;
		/* Handled here */
		case lseek_me:									/* Seek within file descriptor	*/
			rstackptr[2] = (TMPREAL) _lseeki64(GVTrimToInt32(rstackptr[2]), GVTrimToInt64(rstackptr[1]), GVTrimToInt32(rstackptr[0]) );
			rstackptr += 2;
			break;
		case close_me:									/* Close file descriptor			*/
			*rstackptr = _close(GVTrimToInt32(*rstackptr));
			break;
		case eof_me:									/* End of file test					*/
			*rstackptr = _eof(GVTrimToInt32(*rstackptr));
			break;
		case tell_me:									/* End of file test					*/
			*rstackptr = (TMPREAL) _telli64(GVTrimToInt32(*rstackptr));
			break;
#endif

		case ctype_isalnum:
		case ctype_isalpha:
		case ctype_iscntrl:
		case ctype_isdigit:
		case ctype_isgraph:
		case ctype_islower:
		case ctype_isprint:
		case ctype_ispunct:
		case ctype_isspace:
		case ctype_isupper:
		case ctype_isxdigit:
		case ctype_tolower:
		case ctype_toupper:
			*rstackptr = (TMPREAL) HandleCtypeFnc(mycmd, nint(*rstackptr));
			break;

		case save_string_stack_val:				/* These are commands only */
		case rexx_concat:
		case rexx_upcase:
		case rexx_lowercase:
		case rexx_reverse:
		case pwd_me:
		case getenv_me:
		case fullpath_me:
		case fopen_me:
		case popen_me:
		case fgets_me:
		case hex2bin_me:
		case bin2hex_me:
		case bin_or_me:
		case bin_and_me:
		case bin_xor_me:
		case hex_or_me:
		case hex_and_me:
		case hex_xor_me:
			HandleStringFnc(mycmd, 0, 0, 0, 0.0);
			break;
		case load_string_stack_val:				/* These use a single int from cmds */
		case pop_string_stack:
			HandleStringFnc(mycmd, *(cmds++), 0,0, 0.0);
			break;
		case ctime_me:									/* These use a single int from stack */
		case strftime_me:
		case rexx_char:
		case int2hex_me:								/* Was rexx_d2x */
		case int2oct_me:
		case int2bin_me:
		case rexx_word:
		case rexx_strip:
		case rexx_copies:
		case rexx_translate:
			HandleStringFnc(mycmd, GVTrimToInt64(*rstackptr), 0,0, 0.0);
			++rstackptr;
			break;
		case float2hex_me:
		case double2hex_me:
			HandleStringFnc(mycmd, 0,0, 0, *rstackptr);
			++rstackptr;
			break;
		case rexx_subword:							/* These use two ints */
		case rexx_left:
		case rexx_right:
		case rexx_center:			
		case int2base_me:
		case rexx_xrange:
		case rexx_space:
		case rexx_justify:
		case rexx_delstr:
		case rexx_delword:
			HandleStringFnc(mycmd, GVTrimToInt64(rstackptr[1]), GVTrimToInt64(rstackptr[0]), 0, 0.0);
			rstackptr += 2;
			break;
		case rexx_substr:
		case rexx_insert:
		case rexx_overlay:
			HandleStringFnc(mycmd, GVTrimToInt64(rstackptr[2]), GVTrimToInt64(rstackptr[1]), GVTrimToInt64(rstackptr[0]), 0.0);
			rstackptr += 3;
			break;

		case solve_me:									/* Very important */
		{
			SOLVE_PARMS parms;
			double root;									/* Root to be found */
			static double last_root = 0.0;			/* Keep as next starting point */
			int iargs, rc, iskip;
			GVCMDS *solve_cmds_hold;

			iargs = *(cmds++);							/* How many args were given */

			/* Default values */
			parms.LowerBound = -1.0;					/* Specified bounds */
			parms.UpperBound = +1.0;
			parms.Guess      = last_root;				/* Use last result if not specified */
			parms.epsilon    = 4*REAL_EPSILON;
			parms.MaxIterate = 100;
			parms.fnc        = solve_fnc_test;
			parms.cmds       = NULL;

			/* Process the arguments, reverse order */
			while (iargs > 5) { rstackptr++; iargs--; }
			if (iargs >= 5) { parms.MaxIterate = nint(*rstackptr); rstackptr++; }	/* Burned by macro definitions */
			if (iargs >= 4) parms.epsilon    = (double) *(rstackptr++);
			if (iargs >= 3) parms.Guess      = (double) *(rstackptr++);
			if (iargs >= 2) parms.UpperBound = (double) *(rstackptr++);					/* Specified bounds */
			if (iargs >= 1) parms.LowerBound = (double) *(rstackptr++);

			if (parms.epsilon    == DUMMY_VALUE || parms.epsilon    <= 0) parms.epsilon    = 4*REAL_EPSILON;				/* Defaults wanted? */
			if (parms.MaxIterate == DUMMY_VALUE || parms.MaxIterate <= 0) parms.MaxIterate = 100;
			if (parms.Guess      == DUMMY_VALUE                         ) parms.Guess      = last_root;
/*			TTYprintf("Solve parms: %g %g %g %g %d\n", parms.LowerBound, parms.UpperBound, parms.Guess, parms.epsilon, parms.MaxIterate); */

			GVGETITEM(&iskip, cmds, int);															/* How long is "subroutine" cmds */
			solve_cmds_hold = solve_cmds;															/* Need to restore for re-entrancy */
			solve_cmds = cmds;																		/* Publish cmds for solve_fnc_test */

			rc = GVSolve(&root, 0, &parms, NULL);												/* Go and let it solve */
			switch (rc) {
				case  0: break;
				case -1:
					{ 	char token[LONG_STR_SIZE];
					sprintf(token, "ERROR: Root not bracketed by specified range (SOLVE)\n"
							  "       f(%g) = %g\tf(%g) = %g\n", 
							  parms.LowerBound, subfnc_eval(parms.LowerBound,NULL), 
							  parms.UpperBound, subfnc_eval(parms.UpperBound,NULL));
					gv_math_error_msg(token);
					}
					break;
				case -2:	gv_math_error_msg("ERROR: Function could not be evaluated (SOLVE)\n"); break;
				case -3:	gv_math_error_msg("ERROR: No solution found in maximum iterations (SOLVE)\n"); break;
				default:	gv_math_error_msg("ERROR: Unknown error (SOLVE)\n"); break;
			}
			*(--rstackptr) = (TMPREAL) (last_root = root);									/* Save if calling again soon */
			cmds += iskip;																				/* And skip over subroutine	*/
			solve_cmds = solve_cmds_hold;															/* And restore */
		}
		break;

		case dydx_me:
		{
			double x0,dx;
			TMPREAL yh,yl;
			int iargs, iskip, ierr[2];
			GVCMDS *solve_cmds_hold;

			/* Get and dump arguments */
			iargs = *(cmds++);							/* How many args were given */
			x0 = dx = 0;									/* Initial values */
			while (iargs > 2) { rstackptr++; iargs--; }
			if (iargs >= 2) dx  = *(rstackptr++);	/* Epsilon to use */
			if (iargs >= 1) x0  = *(rstackptr++);	/* point to evaluate */

			GVGETITEM(&iskip, cmds, int);				/* How long is "subroutine" cmds */
			solve_cmds_hold = solve_cmds;				/* Need to restore for re-entrancy */
			solve_cmds = cmds;							/* Publish cmds for subfnc_eval */

			/* Set the parameters, possibly via defaults */
			if (x0 == 0 && dx == 0) dx = sqrt(REAL_EPSILON);
			if (dx == 0)				dx = x0*1E-4;	/* Reasonable distance */

			yh = subfnc_eval(x0+dx, ierr);
			yl = subfnc_eval(x0-dx, ierr+1);
			*(--rstackptr) = (yh-yl)/(2*dx);
			if (ierr[0] != 0 || ierr[1] != 0) gv_math_error_msg("ERROR: dydx function could not be evaluated\n"); 
			cmds += iskip;																				/* And skip over subroutine	*/
			solve_cmds = solve_cmds_hold;															/* And restore */
		}
		break;

		case integrate_me:
		{
#define	NDIVS	(100)
			double xl, xh, eps, sum;
			int iargs, iskip, miniter, maxiter, rc;
			GVCMDS *solve_cmds_hold;

			/* Get and dump arguments */
			eps = 1E-5;										/* Nominal precision */
			miniter = 256;									/* Minimum # of iterations */
			maxiter = 65536;								/* Allow 65,000 iterations */
			iargs = *(cmds++);							/* How many args were given */
			while (iargs > 5) { rstackptr++; iargs--; }
			if (iargs >= 5) { maxiter = nint(*rstackptr); rstackptr++; }	/* Burned by macro definitions */
			if (iargs >= 4) { miniter = nint(*rstackptr); rstackptr++; }	/* Burned by macro definitions */
			if (iargs >= 3) eps = *(rstackptr++);	/* Precision requested */
			if (eps == DUMMY_VALUE) eps = 1E-5;
			xh  = *(rstackptr++);						/* upper limit */
			xl  = *(rstackptr++);						/* lower limit */

			GVGETITEM(&iskip, cmds, int);				/* How long is "subroutine" cmds */
			solve_cmds_hold = solve_cmds;				/* Need to restore for re-entrancy */
			solve_cmds = cmds;							/* Publish cmds for subfnc_eval */

			/* Set the parameters, possibly via defaults */
			rc = GVIntegrate(&sum, subfnc_eval, xl, xh, eps, miniter, maxiter);
			*(--rstackptr) = sum;
			switch (rc) {
				case  0: break;
				case -1: gv_math_error_msg("ERROR: Function could not be evaluated (INTEGRATE)\n"); break;
				case -2: gv_math_error_msg("ERROR: Did not converge within maximum number of evaluations (INTEGRATE)\n"); break;
				default:	gv_math_error_msg("ERROR: Unknown error (INTEGRATE)\n"); break;
			}
			cmds += iskip;																				/* And skip over subroutine	*/
			solve_cmds = solve_cmds_hold;															/* And restore */
		}
		break;

		case sum_me:
		case prod_me:
		{
			double sum;
			int iargs, iskip;
			int istart, iend, istep, errcnt, ierr;
			char token[LONG_STR_SIZE]; 
			GVCMDS *solve_cmds_hold;

			/* Get and dump arguments */
			istart = iend = istep = 1;					/* Default values */
			iargs = *(cmds++);							/* Dump excess arguments */
			while (iargs > 3) { rstackptr++; iargs--; }
			if (iargs >= 3) { istep = nint(*rstackptr); rstackptr++; }
			iend    = nint(*rstackptr); rstackptr++;	/* Upper limit */
			istart  = nint(*rstackptr); rstackptr++;	/* Lower limit */

			GVGETITEM(&iskip, cmds, int);				/* How long is "subroutine" cmds */
			solve_cmds_hold = solve_cmds;				/* Need to restore for re-entrancy */
			solve_cmds = cmds;							/* Publish cmds for subfnc_eval */

			/* Set the parameters, possibly via defaults */
			sum = (mycmd == sum_me) ? 0 : 1;			/* Initial value is either 0 or 1 for sum or product */
			errcnt = 0;
			if (istep < 0) istep = -istep;
			if (istep == 0) {
				gv_math_error_msg("ERROR: Continued sum/product with step of 0 can't be completed\n");
			} else {
				if (istart>iend) { i = istart; istart = iend; iend = i; }
				for (i=istart; i<=iend; i+=istep) {
					double rc;
					rc = subfnc_eval(i, &ierr);
					if (mycmd == sum_me)  sum += rc;
					if (mycmd == prod_me) sum *= rc;
					if (ierr != 0) { if (errcnt++ > 20) break; }
				}
			}
			if (errcnt != 0) { sprintf(token, "Errors (%d) during continued sum/product\n", errcnt); gv_math_error_msg(token); }
			*(--rstackptr) = sum;
			cmds += iskip;																				/* And skip over subroutine	*/
			solve_cmds = solve_cmds_hold;															/* And restore */
		}
		break;

		case chdir_me:
		case mkdir_me:
		case rmdir_me:
		case rm_me:
		case mv_me:
		case unlink_me:
			*(--rstackptr) = HandleOSFnc(mycmd);
			break;

		case lex_get_token:
		case lex_get_token_p:
		case lex_chk_token:
			*(--rstackptr) = HandleLexFnc(mycmd, 0, 0);
			break;
			
/* Array functions */
		case poly_me:
		case dpoly_me:
			{
			int i,j, ideg;
			TMPREAL cf,tmp=0;

			GVGETITEM(&arrayptr, cmds, ARRAY *);

			if (mycmd == poly_me) {						/* Handle poly here			*/
				for (i=*(arrayptr->size)-1; i>=0; i--) 
					tmp = (*rstackptr)*tmp + arrayptr->x[i];
				*rstackptr = tmp;
			} else if (mycmd == dpoly_me) {
				ideg = nint(*rstackptr);				/* Dump the degree		*/
				rstackptr++;
				if (ideg >= *(arrayptr->size) || ideg < -10) {
					tmp = 0;
				} else {
					for (i=*(arrayptr->size)-1; i>=ideg && i>=0; i--) {	/* Terms */
						cf = arrayptr->x[i];
						if (ideg > 0) for (j=0; j< ideg; j++) cf *= i-j;
						if (ideg < 0) for (j=0; j<-ideg; j++) cf /= i+j+1;
						tmp = (*rstackptr)*tmp + cf;
					}
					for (i=0; i>ideg; i--) tmp *= (*rstackptr);
				}
				*rstackptr = tmp;
			} else {
				ERRprintf("GVCALC Screwup:  Tell developers problem at dpoly_me in gvcalc.c\n");
				*(--rstackptr) = 0;
			}
			break;
		}

		case array_min:
		case array_max:
		case array_sum:
		case array_avg:
		case array_count:
		case array_median:
		case array_mad:
		case array_var:
		case array_std:
		case array_sdom:
		case array_rms: 
		case array_skew:
		case array_kurt:
		case array_span:
		case array_absmin:
		case array_absmax:
		case array_abssum:
		case array_absavg:
			if (*(cmds++) == 0) {						/* 0 ==> Arrays */
				imax = nint(*rstackptr); rstackptr++;
				imin = nint(*rstackptr);
				GVGETITEM(&arrayptr, cmds, ARRAY *);
				*rstackptr = gv_eval_array_fnc(mycmd, arrayptr, imin, imax);
			} else {
				xmax = (REAL) *rstackptr; rstackptr++;
				xmin = (REAL) *rstackptr;
				GVGETITEM(&curveptr, cmds, CURVE *);
				*rstackptr = gv_eval_curve_fnc(mycmd, curveptr, xmin, xmax);
			}
			break;

		case array_covar:
		case array_weight_avg:
		case array_weight_sigma:
		case array_weight_var:
		case array_weight_std:
		case array_weight_sdom:
		case array_weight_absavg:
			imax = nint(*rstackptr); rstackptr++;
			imin = nint(*rstackptr);
			GVGETITEM(&arrayptr, cmds, ARRAY *);
			GVGETITEM(&arrayptr2, cmds, ARRAY *);
			*rstackptr = gv_eval_array_fnc2(mycmd, arrayptr, arrayptr2, imin, imax);
			break;
			
		case array_index:
			GVGETITEM(&arrayptr, cmds, ARRAY *);
			*rstackptr = gv_find_in_array(arrayptr, *rstackptr);
			break;

		case curve_integral:
		case curve_correlate:
		case curve_avg:
		case curve_median:
		case curve_std:
		case curve_var:
		case curve_skew:
		case curve_kurt:
			xmax = (REAL) *rstackptr; rstackptr++;
			xmin = (REAL) *rstackptr;
			GVGETITEM(&curveptr, cmds, CURVE *);
			*rstackptr = gv_eval_curve(mycmd, curveptr, xmin, xmax, NULL);
			break;

		case time_me:
			*(--rstackptr) = (TMPREAL) time(NULL);
			break;

		case clock_me:
			*(--rstackptr) = clock()/(1.0*CLOCKS_PER_SEC);
			break;

		case timer_me:
			*rstackptr = my_timer((BOOL) (*rstackptr != 0.0) );
			break;

		case curve_pintegral:
			b = *rstackptr++;						/* Upper limit */
			a = *rstackptr;						/* Lower limit	*/
			GVGETITEM(&curveptr, cmds, CURVE *);
			*rstackptr = p_integrate(curveptr, a, b);
			break;

		case curve_near:
			yp = *rstackptr++;											/* Y value */
			xp = *rstackptr;												/* X value */
			GVGETITEM(&curveptr, cmds, CURVE *);
			*rstackptr = find_near_curve(2, curveptr, xp, yp, 0.0);
			break;

		case curve_3d_near:
			zp = *rstackptr++;											/* Z value */
			yp = *rstackptr++;											/* Y value */
			xp = *rstackptr;												/* X value */
			GVGETITEM(&curveptr, cmds, CURVE *);
			*rstackptr = find_near_curve(3, curveptr, xp, yp, zp);
			break;

		case load_dummy_value:
			*(--rstackptr) = DUMMY_VALUE;
			break;
		case load_e:
			*(--rstackptr) = e;
			break;
		case load_pi:
			*(--rstackptr) = PI;
			break;
		case load_realmin:
			*(--rstackptr) = REAL_MIN;
			break;
		case load_realmax:
			*(--rstackptr) = REAL_MAX;
			break;
		case load_i:
			*(--rstackptr) = GVLocalIndex;
			break;

		case load_stack_val:						/*  Load SS:SP-4-4*[DS:DI] */
			*(--rstackptr) = rstack_public[*(cmds++)];
			break;
		case save_stack_val:						/*  Pop ST(0) onto secondary stack */
/*			TTYprintf("Putting %g on the sub-stack\n", *rstackptr); */
			rstack_public[subptr++] = *(rstackptr++);
			break;
		case pop_stack:							/*  Decrement stack by [DS:DI] */
/*			TTYprintf("Popping %d elements from sub-stack\n", *cmds); */
			subptr -= *(cmds++);					/* Decrement by next argument */
			break;
		case xeq_function:						/*  Execute function pointed by next adr */
			GVGETITEM(&fnc, cmds, EXT_FNC_LINK *);
			i = *(cmds++);							/* Position of the stack */
			--rstackptr;							/* Pop the stack one		 */
			if ((*fnc)(0, rstackptr, rstack_public+i) != 0) rcode = 2;	/* REAL, result, args	 */
			break;

		case xeq_function_a:						/* Execute real function of read/str arguments */
		case xeq_str_function:					/* Execute string function pointed by next adr */
		{
			int i,j,k;
			char *tmpstr;							/* Response */
			char **strargs=NULL;					/* Location of string arguments */
			int s_args;								/* Number of string arguments */
			if (mycmd == xeq_str_function) {
				GVGETITEM(&sfnc, cmds, EXT_STR_FNC_LINK *);
			} else {
				GVGETITEM(&fnca, cmds, EXT_FNCA_LINK *);
			}
			s_args = *(cmds++);					/* Number of string argumentts */
			i = *(cmds++);							/* Position of the stack */
			j = *(cmds++);							/* Position of the string stack */
			if (s_args > 0) {
				strargs = calloc(s_args, sizeof(*strargs));
				for (k=0; k<s_args; k++) strargs[k] = sstack_public[j+k].str;
			}

			if (mycmd == xeq_str_function) {
				if ((*sfnc)(0, &tmpstr, rstack_public+i, strargs) != 0) {
					if (s_args > 0) free(strargs);
					rcode = 2;	/* REAL, result, args	 */
				}
				--sstackptr;							/* And put the value on the stack */
				sstackptr->status = IS_MALLOC;
				sstackptr->str = tmpstr;
			} else {
				--rstackptr;							/* Pop the stack one		 */
				if ((*fnca)(0, rstackptr, rstack_public+i, strargs) != 0) {
					if (s_args > 0) free(strargs);
					rcode = 2;	/* REAL, result, args	 */
				}
			}
			if (s_args > 0) free(strargs);
		}
			break;

		case load_real:							/*  Load simple real */
			GVGETITEM(&realptr, cmds, REAL *);
			*(--rstackptr) = *realptr;
			break;
		case load_double:							/*  Load simple double */
			GVGETITEM(&doubleptr, cmds, DOUBLE *);
			*(--rstackptr) = *doubleptr;
			break;
		case load_complex:						/*  Load simple complex */
			GVGETITEM(&complexptr, cmds, COMPLEX *);
			*(--rstackptr) = complexptr->x;
			break;
		case load_int:								/*  Load simple int */
			GVGETITEM(&intptr, cmds, INT *);
			*(--rstackptr) = *intptr;
			break;			
		case load_string_idx:					/* Load specific element of string */
			GVGETITEM(&stringptr, cmds, CHAR *);
			i = nint(*rstackptr);
			*rstackptr = stringptr[i];
			break;
		case load_string_tmp:					/* Load embedded string (in cmds) */
			--sstackptr;
			sstackptr->str  = (CHAR *) cmds;	/* String starts here	*/
			sstackptr->status = IS_STATIC;
			cmds += strlen((CHAR *) cmds)+1;	/* And skip over it		*/
			break;
		case load_pfile_ptr:
			GVGETITEM(&pfileptr, cmds, FILE **);
			--sstackptr;
			sstackptr->str  = (CHAR *) pfileptr;
			sstackptr->status = IS_PFUNIT;
			break;
		case load_varentry_ptr:
			GVGETITEM(&avoidptr, cmds, void *);
			--sstackptr;
			sstackptr->str  = (CHAR *) avoidptr;
			sstackptr->status = IS_VARENTRY;
			break;
			
		case load_string_ptr:
			GVGETITEM(&stringptr, cmds, CHAR *);
			--sstackptr;
			sstackptr->str  = (CHAR *) stringptr;
			sstackptr->status = IS_STATIC;
			break;
		case load_strarray_idx:
			GVGETITEM(&s_arrayptr, cmds, STRING_ARRAY *);
			i = nint(*rstackptr); rstackptr++;
			if (WarnArray && (i < 0 || i >= *s_arrayptr->size)) {			/* Warn on array bounds */
				if (GVMathMode & MATHWARN) gv_array_bounds_msg(i, *s_arrayptr->size);
				WarnArray = FALSE;								/* Only once per run */
			}
			--sstackptr;
			sstackptr->str  = s_arrayptr->sval[i];
			sstackptr->status = IS_STATIC;
			break;
			
		case load_surface_pt:
		case surf_interp:													/* Interpolate on a surface */
			yp = *rstackptr++;
			xp = *rstackptr;
			GVGETITEM(&surfaceptr, cmds, CURVE *);
			*rstackptr = surface_interpolate(surfaceptr, xp, yp);
			break;

		case load_real_idx:						/* Load real indexed value (array+stack) */
		case load_real_array:					/* Load array indexed value (array+index) */
			if      (mycmd == load_real_array) {i = GVLocalIndex; --rstackptr;}
			else if (mycmd == load_real_idx)    i = nint(*rstackptr);
			GVGETITEM(&arrayptr, cmds, ARRAY *);
			if (WarnArray && (i < 0 || i >= *arrayptr->size)) {			/* Warn on array bounds */
				if (GVMathMode & MATHWARN) gv_array_bounds_msg(i, *arrayptr->size);
				WarnArray = FALSE;								/* Only once per run */
			}
			*rstackptr = arrayptr->x[i];						/* (--rstackptr) above */
			break;

		case load_int_idx:						/*  Load from indexed integer array */
		case load_int_array:
			if      (mycmd == load_int_array) {i = GVLocalIndex; --rstackptr;}
			else if (mycmd == load_int_idx)    i = nint(*rstackptr);
			GVGETITEM(&i_arrayptr, cmds, INT_ARRAY *);
			if (WarnArray && (i < 0 || i >= *i_arrayptr->size)) {			/* Warn on array bounds */
				if (GVMathMode & MATHWARN) gv_array_bounds_msg(i, *i_arrayptr->size);
				WarnArray = FALSE;								/* Only once per run */
			}
			*rstackptr = i_arrayptr->ival[i];
			break;

		case load_double_idx:					/*  Load from indexed double array */
		case load_double_array:
			if      (mycmd == load_double_array) {i = GVLocalIndex; --rstackptr;}
			else if (mycmd == load_double_idx)    i = nint(*rstackptr);
			GVGETITEM(&d_arrayptr, cmds, DOUBLE_ARRAY *);
			if (WarnArray && (i < 0 || i >= *d_arrayptr->size)) {			/* Warn on array bounds */
				if (GVMathMode & MATHWARN) gv_array_bounds_msg(i, *d_arrayptr->size);
				WarnArray = FALSE;								/* Only once per run */
			}
			*rstackptr = d_arrayptr->x[i];
			break;

		case load_complex_idx:						/* Load complex indexed value (array+stack) */
		case load_complex_array:					/* Load array indexed value (array+index) */
			if      (mycmd == load_complex_array) {i = GVLocalIndex; --rstackptr;}
			else if (mycmd == load_complex_idx)    i = nint(*rstackptr);
			GVGETITEM(&c_arrayptr, cmds, COMPLEX_ARRAY *);
			if (WarnArray && (i < 0 || i >= *c_arrayptr->size)) {			/* Warn on array bounds */
				if (GVMathMode & MATHWARN) gv_array_bounds_msg(i, *c_arrayptr->size);
				WarnArray = FALSE;								/* Only once per run */
			}
			*rstackptr = c_arrayptr->z[i].x;					/* (--rstackptr) above */
			break;

		default:
			ERRprintf("PROGRAMMER SCREWUP: GVCALC case %d was not handled\n", mycmd);
			return(-1);
		}

		if (rstackptr > rstackptr_ini) {						/* Oops! */
			ERRputs("Real stack underflow in evaluation (huh?)\n");
			return(-1);
		}
		if (sstackptr > sstackptr_ini) {						/* Oops! */
			ERRputs("String stack underflow in evaluation (huh?)\n");
			return(-1);
		}

#if (defined CSET2 || defined MSC60 || defined MSC70)
		if (_Sys_Math_Exception != 0) return(-2);
#endif

		/* If requested, show stack at end of the operation */
		if (SysDebugFlag & 0x04) ShowRealCmdStack(rstackptr_ini, rstackptr);
	}

	if (subptr != subptr_ini) {
		ERRputs("WARNING: Something left on secondary stack (huh?)\n");
		return(-1);
	} else if (ssubptr != ssubptr_ini) {
		ERRputs("WARNING: Something left on secondary string stack (huh?)\n");
		return(-1);
	}

	if (SysDebugFlag & 0x06) TTYprintf("----------------------------\n");
	return(rcode);
}
	
/* ===========================================================================
=========================================================================== */
PRIVATE void to_rtheta(TMPCOMPLEX z, TMPREAL *r, TMPREAL *theta) {

/*	z.y = 0; */
	if (FABS(z.x) > FABS(z.y)) {
		*r = FABS(z.x)*SQRT(1+(z.y/z.x)*(z.y/z.x));
	} else if (z.y == 0) {
		*r = 0;
		*theta = 0;
		return;
	} else {
		*r = FABS(z.y)*SQRT(1+(z.x/z.y)*(z.x/z.y));
	}
	*theta = ATAN2(z.y,z.x);
	if (GVMathMode & MATH_BRANCH_PLUS && *theta < 0) *theta += 2*PI;
	return;
}

PRIVATE TMPCOMPLEX to_xy(TMPREAL r, TMPREAL theta) {
	TMPCOMPLEX z;
	TMPREAL cy,sy;
	cy = COS(theta);
	sy = SIN(theta);
	if (FABS(cy) < EPS_TRIG) cy = 0;
	if (FABS(sy) < EPS_TRIG) sy = 0;
	z.x = r*cy;
	z.y = r*sy;
	return(z);
}

PRIVATE TMPCOMPLEX C_INV(TMPCOMPLEX z1) {
	TMPCOMPLEX result;
	TMPREAL m;
	m = z1.x*z1.x + z1.y*z1.y;
	result.x =  z1.x / m;
	result.y = -z1.y / m;
	return(result);
}


PRIVATE TMPCOMPLEX C_ADD(TMPCOMPLEX z1, TMPCOMPLEX z2) {
	TMPCOMPLEX result;
	result.x = z1.x+z2.x;
	result.y = z1.y+z2.y;
	return(result);
}

PRIVATE TMPCOMPLEX C_SUB(TMPCOMPLEX z1, TMPCOMPLEX z2) {
	TMPCOMPLEX result;
	result.x = z1.x-z2.x;
	result.y = z1.y-z2.y;
	return(result);
}

PRIVATE TMPCOMPLEX C_MUL(TMPCOMPLEX z1, TMPCOMPLEX z2) {
	TMPCOMPLEX result;
	result.x = z1.x*z2.x - z1.y*z2.y;
	result.y = z1.x*z2.y + z1.y*z2.x;
	return(result);
}

PRIVATE TMPCOMPLEX C_DIV(TMPCOMPLEX z1, TMPCOMPLEX z2) {
	TMPCOMPLEX result;
	TMPREAL m;
	m = z2.x*z2.x + z2.y*z2.y;
	result.x = ( z1.x*z2.x + z1.y*z2.y) / m ;
	result.y = (-z1.x*z2.y + z1.y*z2.x) / m ;
	return(result);
}

PRIVATE TMPCOMPLEX C_EXP(TMPCOMPLEX z) {
	TMPCOMPLEX result;
	result = to_xy(EXP(z.x), z.y);
	return(result);
}

PRIVATE TMPCOMPLEX C_LOG(TMPCOMPLEX z) {
	TMPREAL r, theta;
	TMPCOMPLEX result;
	to_rtheta(z, &r, &theta);		/* Go to R/theta	*/
	result.x = LOGZ(r);
	result.y = theta;
	return(result);
}

PRIVATE TMPCOMPLEX C_SQRT(TMPCOMPLEX z) {
	TMPREAL r, theta;
	TMPCOMPLEX result;
	if (z.y == 0) z.y = 0.0;		/* Problem with +/- 0 in windows!! */
	to_rtheta(z, &r, &theta);		/* Go to R/theta	*/
	result = to_xy(SQRT(r), theta/2);
	return(result);
}

PRIVATE TMPCOMPLEX C_SINH(TMPCOMPLEX z) {
	TMPCOMPLEX result;
	TMPREAL cy, sy, r;
	cy = COS(z.y);
	sy = SIN(z.y);
	r = EXP(z.x);
	if (FABS(cy) < EPS_TRIG) cy = 0;
	if (FABS(sy) < EPS_TRIG) sy = 0;
	result.x = (r-1/r)*cy/2;
	result.y = (r+1/r)*sy/2;
	return(result);
}

PRIVATE TMPCOMPLEX C_COSH(TMPCOMPLEX z) {
	TMPCOMPLEX result;
	TMPREAL cy, sy, r;
	cy = COS(z.y);
	sy = SIN(z.y);
	r = EXP(z.x);
	if (FABS(cy) < EPS_TRIG) cy = 0;
	if (FABS(sy) < EPS_TRIG) sy = 0;
	result.x = (r+1/r)*cy/2;
	result.y = (r-1/r)*sy/2;
	return(result);
}

PRIVATE TMPCOMPLEX C_TANH(TMPCOMPLEX z) {
	TMPCOMPLEX r1,r2,result;
	TMPREAL cy, sy, r;
	cy = COS(z.y);
	sy = SIN(z.y);
	r  = EXP(z.x);
	if (FABS(cy) < EPS_TRIG) cy = 0;
	if (FABS(sy) < EPS_TRIG) sy = 0;
	r1.x = (r-1/r)*cy;						/* sinh term */
	r1.y = (r+1/r)*sy;
	r2.x = (r+1/r)*cy;
	r2.y = (r-1/r)*sy;
	r = r2.x*r2.x+r2.y*r2.y;
	result.x = (r1.x*r2.x+r1.y*r2.y)/r;
	result.y = (r1.y*r2.x-r1.x*r2.y)/r;
	return(result);
}

PRIVATE TMPCOMPLEX C_ASINH(TMPCOMPLEX z) {
	TMPCOMPLEX result;

	result.x = z.x*z.x-z.y*z.y + 1;				/* z**2 + 1 */
	result.y = 2*z.x*z.y;
	result = C_SQRT(result);						/* SQRT(z**2+1) */
	result.x += z.x;									/* z + SQRT(z**2+1) */
	result.y += z.y;
	result = C_LOG(result);							/* And log gives me asinh */
	return(result);
}

PRIVATE TMPCOMPLEX C_ACOSH(TMPCOMPLEX z) {
	TMPCOMPLEX result;
	
	result.x = z.x*z.x-z.y*z.y - 1;				/* z**2 - 1 */
	result.y = 2*z.x*z.y;
	result = C_SQRT(result);						/* SQRT(z**2-1) */
	result.x += z.x;									/* z + SQRT(z**2-1) */
	result.y += z.y;
	result = C_LOG(result);							/* And log gives me asinh */
	return(result);
}

PRIVATE TMPCOMPLEX C_ATANH(TMPCOMPLEX z) {
	TMPCOMPLEX r1, r2, result;
	TMPREAL r;
	r1.x = 1+z.x;										/* (1+z) */
	r1.y = z.y;
	r2.x = 1-z.x;										/* (1-z) */
	r2.y = -z.y;
	r = r2.x*r2.x+r2.y*r2.y;
	result.x = (r1.x*r2.x+r1.y*r2.y)/r;			/* (1+z) / (1-z) */
	result.y = (r1.y*r2.x-r1.x*r2.y)/r;
	result   = C_LOG(result);						/* LOG((1+z) / (1-z)) */
	result.x /= 2;
	result.y /= 2;
	return(result);
}

/* =============================================================================
-- Routine to return ln(gamma(x)) over some phenonemal range!
--
-- Usage:  call	c_gamma
--
-- Inputs: ST(0) - X in ln(gamma(x))
--
-- Output: ST(0) <-- ln(gamma(ST(0)))
--
-- Implementation: See Abramowitz and Stegun 6.1.40 and 6.1.41
--
--
-- To deal with negative real parts to the arguments, use
--
-- For any integer n,
--
--                    n  gamma(z+n) gamma[1-(z-n)]
--     gamma(z) = (-1)   -------------------------
--                               g(1-z)
--
-- Choose for x<0, n = -floor(x)
--      x+n   = u = mod(|x|,1)
--      1-x-n = 1-u
--
-- Now, all z's in gamma(z) above have positive real arguments
============================================================================= */
PRIVATE TMPCOMPLEX C_LNGAMMA(TMPCOMPLEX z) {

	TMPCOMPLEX t,result;
	TMPREAL m, frac;
	int i,imax=5;
	static double coef[] = {+12, -360, +1260, -1680, +1188};

	if (z.y == 0 && z.x > 0) {				/* Deal with positive simply */
		result.y = 0;
		result.x = LN_GAMMA(z.x);
		return(result);
	}
	
/* Make sure that |z| is greater than 10 for asymptotic expansion */
	if (z.x < 0) {
		i = (int) (-FLOOR(z.x));							/* z+i will be positive */
		frac = z.x+i;											/* Better be positive	*/
		if (frac == 0.0 && z.y == 0) {					/* Really +-INF */
			result.x = result.y = 0;
		} else {
			z.x = 1-z.x;	z.y = -z.y;						/* Now, 1-z */
			t = C_LNGAMMA(z);
			z.x = 1-frac;										/* Now (1-(z+n)) */
			result = C_LNGAMMA(z);
			result.x -= t.x; result.y -= t.y;
			z.x = frac; z.y = -z.y;							/* Now (z+n) */
			t = C_LNGAMMA(z);
			result.x += t.x; result.y += t.y;
			if (i%2 == 1) {									/* Have a -1 power */
				result.y += PI;
				if (result.y > PI) result.y -= 2*PI;
			}
		}
		return(result);
	} else if (z.x < 10) {
		result.x = 1; result.y = 0;
		while (z.x < 10) {				/* Get to range we get good answer */		
			m = z.x*z.x+z.y*z.y;
			t.x = ( result.x*z.x + result.y*z.y) / m;
			t.y = (-result.x*z.y + result.y*z.x) / m;
			result = t;
			z.x += 1;
		}
		result = C_LOG(result);
	} else {
		result.x = result.y = 0;
	}

	t = C_LOG(z);								/* (z+1/2)*ln(z)-z+1/2*ln(2*PI) */
	result.x += (z.x-.5)*t.x - z.y*t.y - z.x  + LOG(2*PI)/2 ;
	result.y += (z.x-.5)*t.y + z.y*t.x - z.y;

	m = z.x*z.x+z.y*z.y;
	t.x = (z.x*z.x-z.y*z.y) / m / m;		/* t == 1/z**2 */
	t.y = -2*z.x*z.y        / m / m;

	if (z.x > 100) imax--;					/* Drop one term  if |z| > 100  */
	if (z.x > 1000) imax--;					/* Drop two terms if |z| > 1000 */

	for (i=0; i<imax; i++) {
		m   = z.x*t.x - z.y*t.y;		/* Divide by z**2 to get next coeff */
		z.y = z.x*t.y + z.y*t.x;
		z.x = m;								/* Need temporary register */
		result.x += z.x / coef[i];
		result.y += z.y / coef[i];
	}

	while (result.y >  PI) result.y -= 2*PI;	/* Choose branch 0 < theta < 2*PI */
	while (result.y < -PI) result.y += 2*PI;
	return(result);
}

/* ============================================================================
--   Function to evaluate the error function
--
--   Usage: COMPLEX = C_ERF(key,z)
--
--   Inputs: z - Input argument of the error function.
--				 KEY - which to return  1 => ERF, 2 => ERFC, 3 => NDTR 
--
--   Output: Output value of the error or complementary function.
--
--   ndtr(z) = {1+erf[z/sqrt(2)]} / 2.0  = erfc[-z/sqrt(2)]/2.0
--
--   Implementation: See Abramowitz and Stegun 7.1.29 - Infinite series
--                   Rather inefficient, but at least implemented.
-- ========================================================================= */
PRIVATE TMPCOMPLEX C_ERF(int mode, TMPCOMPLEX z) {

   TMPCOMPLEX result;
	TMPREAL	 x,y, tmp, tmp2, errx, erry;
	int n;

	x = z.x;												/* Real case handled earlier */
	y = z.y;

/* Correct argument if doing ndtr function */
	if (mode == 3) {									/* Handle as erfc(-z/sqrt(2))/2 */
		x = -x/SQRT(2.0);
		y = -y/SQRT(2.0);
	}

/* Initial real and imaginary parts */
	tmp = EXP(-x*x) / PI;
   result.x = (x == 0) ? 0     : tmp/2/x*(1-COS(2*x*y));
   result.y = (x == 0) ? tmp*y : tmp/2/x*SIN(2*x*y);

   for (n=1; n<50; n++) {
	   tmp2 = tmp*2*EXP(-n*n/4.0)/(n*n+4*x*x);
		errx = tmp2 * (2*x*(1-COSH(n*y)*COS(2*x*y))+n*SINH(n*y)*SIN(2*x*y));
		erry = tmp2 * (2*x*COSH(n*y)*SIN(2*x*y)+n*SINH(n*y)*COS(2*x*y));
		result.x += errx;
		result.y += erry;
		if ( (FABS(errx/result.x)+FABS(erry/result.y)) < 1E-8) break;
	}
/*   printf("C_ERF converged in %i terms\n", n); */
	
	if (mode == 1) {									/* Wanted erf(z)  */
	   result.x = ERF(x) + result.x;
	} else if (mode == 2) {							/* Wanted erfc(z) */
	   result.x = ERFC(x) - result.x;
		result.y = -result.y;
	} else if (mode == 3) {							/* Wanted ndtr(z) */
	   result.x = (ERFC(x) - result.x) / 2;
		result.y = -result.y / 2;
	}
   return(result);
}

PRIVATE void NotImplemented(TMPREAL tmp, char *fnc) {
	if (tmp != 0 && GVMathMode & MATHERROR) 
		ERRprintf("WARNING: %s of a complex argument has not been implemented\n", fnc);
	return;
}

PRIVATE int complex_eval(GVCMDS *cmds) {

	int     rcode;										/* Return code						*/
	int     rc;											/* Temp value						*/

	int        subptr_ini     = subptr;			/* Initial values of pointers */
	int        ssubptr_ini    = ssubptr;		/* Initial values of pointers */
	TMPCOMPLEX *cstackptr_ini = cstackptr;		/* for checking range errors	*/
	STRSTACK   *sstackptr_ini = sstackptr;		/* for checking range errors	*/

	unsigned int mycmd;
	ptrdiff_t jump_length;
	int icnt, imin, imax;
	REAL xmin, xmax;
	long i;
	TMPCOMPLEX z;										/* Temporary variable */
	TMPCOMPLEX a,b;
	TMPREAL xp,yp,zp;
	TMPREAL r,c,m,n,p;
	TMPREAL r2,x,skew,kurt,beta,eta,gamma;
	TMPREAL v,t,v1,v2,F,chi2;
	TMPCOMPLEX c_x0, c_sigma, c_width;

	COMPLEX       *complexptr;
	REAL          *realptr;						/* Pointers stored in cmds	*/
	DOUBLE        *doubleptr;
	CURVE			  *curveptr;
	SURFACE		  *surfaceptr;
	INT	        *intptr;
	EXT_FNC_LINK  *fnc;
	EXT_FNCA_LINK *fnca;
	EXT_STR_FNC_LINK *sfnc;
	ARRAY         *arrayptr, *arrayptr2;
	COMPLEX_ARRAY *c_arrayptr;
	INT_ARRAY	  *i_arrayptr;
	DOUBLE_ARRAY  *d_arrayptr;
	STRING_ARRAY  *s_arrayptr;
	char			  *stringptr;
	FILE			  **pfileptr;				/* Pointer to file pointer */

	if (SysDebugFlag & 0x06) TTYprintf("----------------------------\n");

	rcode = 0;									/* Default return value */
	while (*cmds != 0xFF) {
		if ( (mycmd = *(cmds++)) == 254) mycmd = *(cmds++)+250;
		if (SysDebugFlag & 0x06) ShowCmdInfo(mycmd, cmds);

		switch (mycmd) {

		case load_immed_real:
			--cstackptr;						/* Open up space */
			GVGETITEM(&(cstackptr->x), cmds, TMPREAL);
			cstackptr->y = 0;
			break;
		case load_immed_imag:
			--cstackptr;						/* Open up space */
			GVGETITEM(&(cstackptr->y), cmds, TMPREAL);
			cstackptr->x = 0;
			break;

		case add_me:
			cstackptr[1].x += cstackptr[0].x;
			cstackptr[1].y += cstackptr[0].y;
			cstackptr++;
			break;
		case sub_me:
			cstackptr[1].x -= cstackptr[0].x;
			cstackptr[1].y -= cstackptr[0].y;
			cstackptr++;
			break;
		case mul_me:
			r = cstackptr[1].x*cstackptr[0].x - cstackptr[1].y*cstackptr[0].y;
			c = cstackptr[1].x*cstackptr[0].y + cstackptr[1].y*cstackptr[0].x;
			++cstackptr;
			cstackptr->x = r;
			cstackptr->y = c;
			break;
		case div_me:
			m = cstackptr[0].x*cstackptr[0].x + cstackptr[0].y*cstackptr[0].y;
			r = ( cstackptr[1].x*cstackptr[0].x + cstackptr[1].y*cstackptr[0].y) / m ;
			c = (-cstackptr[1].x*cstackptr[0].y + cstackptr[1].y*cstackptr[0].x) / m ;
			++cstackptr;
			cstackptr->x = r;
			cstackptr->y = c;
			break;
		case pow_me:
			to_rtheta(cstackptr[1], &r, &t);						/* Go to R/theta	*/
			if (r == 0) {												/* As long as not zero */
				++cstackptr;											/* Leave as zero	*/
			} else {
				m = POW(r,cstackptr[0].x) * EXP(-t*cstackptr[0].y);	/* New magnitude	*/
				c = LOG(r)*cstackptr[0].y + t*cstackptr[0].x;			/* New phase		*/
				*(++cstackptr) = to_xy(m,c);						/* And back to xy	*/
			}
			break;

		case lt_me:
			m = (cstackptr[1].x*cstackptr[1].x + cstackptr[1].y*cstackptr[1].y);
			t = (cstackptr[0].x*cstackptr[0].x + cstackptr[0].y*cstackptr[0].y);
			++cstackptr;
			cstackptr->x = (m < t) ? 1 : 0;
			cstackptr->y = 0;
			break;
		case le_me:
			m = (cstackptr[1].x*cstackptr[1].x + cstackptr[1].y*cstackptr[1].y);
			t = (cstackptr[0].x*cstackptr[0].x + cstackptr[0].y*cstackptr[0].y);
			++cstackptr;
			cstackptr->x = (m <= t) ? 1 : 0;
			cstackptr->y = 0;
			break;
		case ne_me:
			t = ((cstackptr[1].x != cstackptr[0].x) || (cstackptr[1].y != cstackptr[0].y)) ? 1 : 0 ;
			++cstackptr;
			cstackptr->x = t;
			cstackptr->y = 0;
			break;
		case gt_me:
			m = (cstackptr[1].x*cstackptr[1].x + cstackptr[1].y*cstackptr[1].y);
			t = (cstackptr[0].x*cstackptr[0].x + cstackptr[0].y*cstackptr[0].y);
			++cstackptr;
			cstackptr->x = (m > t) ? 1 : 0;
			cstackptr->y = 0;
			break;
		case ge_me:
			m = (cstackptr[1].x*cstackptr[1].x + cstackptr[1].y*cstackptr[1].y);
			t = (cstackptr[0].x*cstackptr[0].x + cstackptr[0].y*cstackptr[0].y);
			++cstackptr;
			cstackptr->x = (m >= t) ? 1 : 0;
			cstackptr->y = 0;
			break;
		case eq_me:
			t = ((cstackptr[1].x == cstackptr[0].x) && (cstackptr[1].y == cstackptr[0].y)) ? 1 : 0 ;
			++cstackptr;
			cstackptr->x = t;
			cstackptr->y = 0;
			break;

		case not_me:
			cstackptr->x = (cstackptr->x == 0) ? 1 : 0 ;
			cstackptr->y = 0;
			break;
		case and_me:
			cstackptr[1].x = ( (cstackptr[1].x!=0) && (cstackptr[0].x!=0) ) ? 1 : 0 ;
			cstackptr++;
			cstackptr->y = 0;
			break;
		case or_me:
			cstackptr[1].x = ( (cstackptr[1].x!=0) || (cstackptr[0].x!=0) ) ? 1 : 0 ; 
			cstackptr++; cstackptr->y = 0; break;
		case eqv_me:
			cstackptr[1].x = ( (cstackptr[1].x!=0) == (cstackptr[0].x!=0) ) ? 1 : 0 ; 
			cstackptr++; cstackptr->y = 0; break;
		case neqv_me:
			cstackptr[1].x = ( (cstackptr[1].x!=0) == (cstackptr[0].x!=0) ) ? 0 : 1 ; 
			cstackptr++; cstackptr->y = 0; break;

		case bit_and_me:
			cstackptr[1].x = nint(cstackptr[0].x) & nint(cstackptr[1].x);
			cstackptr++; cstackptr->y = 0; break;
		case bit_or_me:
			cstackptr[1].x = nint(cstackptr[0].x) | nint(cstackptr[1].x);
			cstackptr++; cstackptr->y = 0; break;
		case bit_eor_me:
			cstackptr[1].x = nint(cstackptr[0].x) ^ nint(cstackptr[1].x);
			cstackptr++; cstackptr->y = 0; break;
		case bit_not_me:
			cstackptr->x = ~nint(cstackptr->x);
			cstackptr->y = 0; break;
			break;

		case conditional_me:
			GVGETITEM(&jump_length, cmds, ptrdiff_t);
			if (cstackptr->x <= 0) cmds += jump_length;
			cstackptr++;	break;
		case jump_me:
			GVGETITEM(&jump_length, cmds, ptrdiff_t);
			cmds += jump_length;
			break;

/* Function calls - Must match order of FNCS declarations in code. */
		case chs_me:
			cstackptr->x = -cstackptr->x;
			cstackptr->y = -cstackptr->y;
			break;
		case abs_me:
			cstackptr->x = SQRT((cstackptr->x)*(cstackptr->x)+(cstackptr->y)*(cstackptr->y));
			cstackptr->y = 0;
			break;
		case sign_me:
			if (cstackptr->x != 0) cstackptr->x = (cstackptr->x >= 0) ? 1 : -1;
			if (cstackptr->y != 0) cstackptr->y = (cstackptr->y >= 0) ? 1 : -1;
			break;
		case limit_me:
			if (cstackptr[0].x < cstackptr[1].x) { z = cstackptr[0]; cstackptr[0] = cstackptr[1]; cstackptr[1] = z; }
			if (cstackptr[2].x > cstackptr[0].x) cstackptr[2] = cstackptr[0];
			if (cstackptr[2].x < cstackptr[1].x) cstackptr[2] = cstackptr[1];
			cstackptr += 2; break;
		case min_me:
			if (cstackptr[0].x < cstackptr[1].x) {
				cstackptr[1] = cstackptr[0];
			} else if (cstackptr[0].x == cstackptr[1].x) {
				if (cstackptr[0].y < cstackptr[1].y) cstackptr[1] = cstackptr[0];
			}
			cstackptr++;
			break;
		case max_me:
			if (cstackptr[0].x > cstackptr[1].x) {
				cstackptr[1] = cstackptr[0];
			} else if (cstackptr[0].x == cstackptr[1].x) {
				if (cstackptr[0].y > cstackptr[1].y) cstackptr[1] = cstackptr[0];
			}
			cstackptr++;
			break;

		case ave_me:
			GVGETITEM(&icnt, cmds, int);
			for (a.x=a.y=0,i=0; i<icnt; i++) { a.x += cstackptr->x; a.y += cstackptr->y; cstackptr++; }
			--cstackptr;
			cstackptr->x = a.x/icnt; cstackptr->y = a.y/icnt; 
			break;
		case std_me:
			GVGETITEM(&icnt, cmds, int);
			for (a.x=a.y=b.x=b.y=0,i=0; i<icnt; i++) { 
				a.x += cstackptr->x;			 a.y += cstackptr->y; 
				b.x += pow(cstackptr->x,2); b.y += pow(cstackptr->y,2);
				cstackptr++;
			}
			--cstackptr;
			if (icnt <= 1) {
				cstackptr->x = cstackptr->y = 0.0;
			} else {
				cstackptr->x = sqrt((b.x-a.x*a.x/icnt)/(icnt-1.0));
				cstackptr->y = sqrt((b.y-a.y*a.y/icnt)/(icnt-1.0));
			}
			break;
		case sdom_me:
			GVGETITEM(&icnt, cmds, int);
			for (a.x=a.y=b.x=b.y=0,i=0; i<icnt; i++) { 
				a.x += cstackptr->x;			 a.y += cstackptr->y; 
				b.x += pow(cstackptr->x,2); b.y += pow(cstackptr->y,2);
				cstackptr++;
			}
			--cstackptr;
			if (icnt <= 1) {
				cstackptr->x = cstackptr->y = 0.0;
			} else {
				cstackptr->x = sqrt((b.x-a.x*a.x/icnt)/(icnt-1.0)/icnt);
				cstackptr->y = sqrt((b.y-a.y*a.y/icnt)/(icnt-1.0)/icnt);
			}
			break;
		case median_me:
		case mad_me:
			GVGETITEM(&icnt, cmds, int);
			qsort(cstackptr, icnt, sizeof(*cstackptr), &TMPCOMPLEX_Compare);
			if ( (icnt & 0x01) == 1) {					/* Odd # of points, just take middle */
				a = cstackptr[icnt/2];					/* midpoint */
			} else {
				a.x = (cstackptr[icnt/2-1].x+cstackptr[icnt/2].x)/2;
				a.y = (cstackptr[icnt/2-1].y+cstackptr[icnt/2].y)/2;
			}
			if (mycmd == mad_me) {
				for (i=0; i<icnt; i++) {					/* Get absolute deviation */
					cstackptr[i].x -= sqrt(pow(cstackptr[i].x-a.x,2) + pow(cstackptr[i].y-a.y,2));
					cstackptr[i].y  = 0;
				}
				qsort(cstackptr, icnt, sizeof(*cstackptr), &TMPCOMPLEX_Compare);
				if ( (icnt & 0x01) == 1) {					/* Odd # of points, just take middle */
					a = cstackptr[icnt/2];					/* midpoint */
				} else {
					a.x = (cstackptr[icnt/2-1].x+cstackptr[icnt/2].x)/2;
					a.y = 0;
				}
			}
			cstackptr += icnt;
			*(--cstackptr) = a;
			break;

		case count_me:
			GVGETITEM(&icnt, cmds, int);
			cstackptr += icnt;
			--cstackptr;
			cstackptr->x = (TMPREAL) icnt; cstackptr->y = 0;
			break;

		case int_me:
			cstackptr->x = ((long) cstackptr->x);
			cstackptr->y = 0;
			break;
		case nint_me:
			cstackptr->x = (TMPREAL) nint(cstackptr->x);
			cstackptr->y = 0;
			break;
		case frac_me:
			cstackptr->x = FMOD(cstackptr->x, 1.0);
			cstackptr->y = 0;
			break;
		case mod_me:
			cstackptr[1].x = FMOD(cstackptr[1].x, cstackptr[0].x);
			cstackptr++;
			cstackptr->y = 0;
			break;
		case mantissa_me:
			cstackptr->x = cstackptr->x / pow(10, FLOOR(LOG(FABS(cstackptr->x))/LOG(10.0))); 
			cstackptr->y = 0; break;
		case exponent_me:
			cstackptr->x = FLOOR(LOG(FABS(cstackptr->x))/LOG(10.0)); 
			cstackptr->y = 0; break;
		case m1n_me:						/* Minus 1 to the nth power */
			i = nint(cstackptr->x);
			cstackptr->x = (i%2 == 0) ? 1 : -1 ;
			cstackptr->y = 0;
			break;
		case real_me:
			cstackptr->y = 0;
			break;
		case imag_me:
			cstackptr->x = cstackptr->y;
			cstackptr->y = 0;
			break;
		case conj_me:
			cstackptr->y = -cstackptr->y;
			break;
		case arg_me:
			if (cstackptr->x != 0 || cstackptr->y != 0) {
				cstackptr->x = ATAN2(cstackptr->y,cstackptr->x);
				if (GVMathMode & MATH_BRANCH_PLUS && cstackptr->x < 0) 
					cstackptr->x += 2*PI;
				cstackptr->y = 0;
			}
			break;

		case sinh_me:
		case csch_me:
			*cstackptr = C_SINH(*cstackptr);
			if (mycmd == csch_me) *cstackptr = C_INV(*cstackptr);
			break;
		case cosh_me:
		case sech_me:
			*cstackptr = C_COSH(*cstackptr);
			if (mycmd == sech_me) *cstackptr = C_INV(*cstackptr);
			break;
		case tanh_me:
		case coth_me:
			*cstackptr = C_TANH(*cstackptr);
			if (mycmd == coth_me) *cstackptr = C_INV(*cstackptr);
			break;

		case acsch_me:
			 *cstackptr = C_INV(*cstackptr);
		case asinh_me:
			*cstackptr = C_ASINH(*cstackptr);
			break;

		case asech_me:
			 *cstackptr = C_INV(*cstackptr);
		case acosh_me:
			*cstackptr = C_ACOSH(*cstackptr);
			break;

		case acoth_me:
			 *cstackptr = C_INV(*cstackptr);
		case atanh_me:
			*cstackptr = C_ATANH(*cstackptr);
			break;
			
		case sind_me:
			cstackptr->x *= DEGREES_TO_RADIANS;	cstackptr->y *= DEGREES_TO_RADIANS;
		case sin_me:								/* sin(z) = sinh(iz)/i	*/
			z.x = -cstackptr->y;	z.y = cstackptr->x;
			z = C_SINH(z);
			cstackptr->x = z.y;	cstackptr->y = -z.x;
			break;

		case cosd_me:
			cstackptr->x *= DEGREES_TO_RADIANS;	cstackptr->y *= DEGREES_TO_RADIANS;
		case cos_me:								/* cos(z) = cosh(iz)		*/
			z.x = -cstackptr->y;	z.y = cstackptr->x;
			z = C_COSH(z);
			cstackptr->x = z.x;		cstackptr->y = z.y;
			break;

		case tand_me:
			cstackptr->x *= DEGREES_TO_RADIANS;	cstackptr->y *= DEGREES_TO_RADIANS;
		case tan_me:								/* tan(z) = tanh(iz)/i	*/
			z.x = -cstackptr->y;	z.y = cstackptr->x;
			z = C_TANH(z);
			cstackptr->x = z.y;	cstackptr->y = -z.x;
			break;

		case cotd_me:
			cstackptr->x *= DEGREES_TO_RADIANS;	cstackptr->y *= DEGREES_TO_RADIANS;
		case cot_me:
			z.x = -cstackptr->y;	z.y = cstackptr->x;
			z = C_TANH(z);
			m = z.x*z.x + z.y*z.y;
			cstackptr->x = z.y/m; cstackptr->y = z.x/m;
			break;

		case asind_me:
		case asin_me:								/* asin(y) = asinh(iy)/i */
			z.x = -cstackptr->y; z.y = cstackptr->x;
			z = C_ASINH(z);
			cstackptr->x = z.y;	cstackptr->y = -z.x;
			if (mycmd == asind_me) {cstackptr->x *= RADIANS_TO_DEGREES; cstackptr->y *= RADIANS_TO_DEGREES;}
			break;

		case acosd_me:
		case acos_me:								/* acos(y) = acosh(y)/i	*/
			z = C_ACOSH(*cstackptr);
			cstackptr->x = z.y;	cstackptr->y = -z.x;
			if (mycmd == acosd_me) {cstackptr->x *= RADIANS_TO_DEGREES; cstackptr->y *= RADIANS_TO_DEGREES;}
			break;

		case atand_me:
		case atan_me:								/* atan(y) = atanh(iy)/i	*/
			z.x = -(cstackptr->y); z.y = cstackptr->x;
			z = C_ATANH(z);
			cstackptr->x = z.y;	cstackptr->y = -z.x;
			if (mycmd == atand_me) {cstackptr->x *= RADIANS_TO_DEGREES; cstackptr->y *= RADIANS_TO_DEGREES;}
			break;

		case acotd_me:
		case acot_me:
			m = cstackptr->x*cstackptr->x + cstackptr->y*cstackptr->y;
			z.x = -cstackptr->y/m; z.y = -cstackptr->x/m;
			z = C_ATANH(z);
			cstackptr->x = z.y;	cstackptr->y = -z.x;
			if (mycmd == acotd_me) {cstackptr->x *= RADIANS_TO_DEGREES; cstackptr->y *= RADIANS_TO_DEGREES;}
			break;

		case atan2_me:
			cstackptr[1].x = ATAN2(cstackptr[1].x,cstackptr[0].x);
			cstackptr++;
			break;
		case atan2d_me:
			cstackptr[1].x = RADIANS_TO_DEGREES*ATAN2(cstackptr[1].x,cstackptr[0].x);
			cstackptr++;
			break;

		case ln_me:
			*cstackptr = C_LOG(*cstackptr);
			break;
		case log_me:
			*cstackptr = C_LOG(*cstackptr);
			cstackptr->x /= LOG(10.0);
			cstackptr->y /= LOG(10.0);
			break;
		case exp_me:
			*cstackptr = C_EXP(*cstackptr);
			break;
		case sqrt_me:
			*cstackptr = C_SQRT(*cstackptr);
			break;

		case erf_me:
			if (cstackptr->y == 0) {
				cstackptr->x = ERF(cstackptr->x);
			} else {
				*cstackptr = C_ERF(1, *cstackptr);
			}
			break;
		case erfc_me:
			if (cstackptr->y == 0) {
				cstackptr->x = ERFC(cstackptr->x);
			} else {
				*cstackptr = C_ERF(2, *cstackptr);
			}
			break;
		case erfi_me:
			NotImplemented(cstackptr->y, "erfi");
			cstackptr->x = ERFI(cstackptr->x);
			cstackptr->y = 0; break;
		case erfci_me:
			NotImplemented(cstackptr->y, "erfi");
			cstackptr->x = ERFCI(cstackptr->x);
			cstackptr->y = 0; break;
		case lnerfc_me:
			if (cstackptr->y == 0) {
				cstackptr->x = ERF_S(4,cstackptr->x);
			} else {
				*cstackptr = C_ERF(2, *cstackptr);
				*cstackptr = C_LOG(*cstackptr);
			}
			break;
		case ndtr_me:
			if (cstackptr->y == 0) {
				cstackptr->x = NDTR(cstackptr->x);
			} else {
				*cstackptr = C_ERF(3, *cstackptr);
			}
			break;

		case fact_me:						/* Factorial (LEFT LIMITED TO REALS!!) */
			NotImplemented(cstackptr->y, "factorial");
			i = nint(cstackptr->x);
			cstackptr->x = 1.0;
			if (i > 0 && i < 171)  for (; i>1; i--) {cstackptr->x *= i;}
			break;

		case gamma_me:						/* Gamma function */
			*cstackptr = C_EXP(C_LNGAMMA(*cstackptr));
			break;
		case lngamma_me:
			if (cstackptr->y == 0 && cstackptr->x > 0) {		/* Only positive so */
				cstackptr->x = LN_GAMMA(cstackptr->x);			/* ln(-value) okay */
			} else {
				*cstackptr = C_LNGAMMA(*cstackptr);
			}
			break;
		case digamma_me:					/* Digamma function */
			if (cstackptr->y != 0) NotImplemented(cstackptr->y, "digamma");
			cstackptr->x = digamma(cstackptr->x);
			break;

		case rnd_me:						/* Random numbers */
			cstackptr--;					/* Now no arguments */
			cstackptr->x = ((TMPREAL) rand()) / (RAND_MAX+1.0);
			cstackptr->y = ((TMPREAL) rand()) / (RAND_MAX+1.0);
			break;

		case srand_me:
			srand((unsigned int) nint(cstackptr->x));
			break;

		case drand48_me:					/* Random numbers */
			cstackptr--;					/* Now no arguments */
			cstackptr->x = (TMPREAL) drand48();
			cstackptr->y = (TMPREAL) drand48();
			break;

		case lrand48_me:					/* Random numbers */
			cstackptr--;					/* Now no arguments */
			cstackptr->x = (TMPREAL) lrand48();
			cstackptr->y = (TMPREAL) lrand48();
			break;

		case mrand48_me:					/* Random numbers */
			cstackptr--;					/* Now no arguments */
			cstackptr->x = (TMPREAL) mrand48();
			cstackptr->y = (TMPREAL) mrand48();
			break;

		case srand48_me:
			srand48((long int) nint(cstackptr->x));
			break;

		case rnd_seed_me:
			if (cstackptr[0].x != -1) {
				mt_seed32new(nint(cstackptr[0].x));
			} else {
				mt_goodseed();
			}
			cstackptr[0].x = cstackptr[0].y = 0;
			break;
		case rnd_lrand_me:
			cstackptr--;
			cstackptr[0].x = (TMPREAL) mt_lrand()/2;
			cstackptr[0].y = (TMPREAL) mt_lrand()/2;
			break;
		case rnd_drand_me:
			cstackptr--;
			cstackptr[0].x = (TMPREAL) mt_ldrand();
			cstackptr[0].y = (TMPREAL) mt_ldrand();
			break;
		case rnd_iuniform_me:
			cstackptr[1].x = (TMPREAL) rd_iuniform(nint(cstackptr[1].x), nint(cstackptr[0].x));
			if (cstackptr[0].y != cstackptr[1].y) cstackptr[1].y = (TMPREAL) rd_iuniform(nint(cstackptr[1].y), nint(cstackptr[0].y));
			cstackptr++;
			break;
		case rnd_uniform_me:
			cstackptr[1].x = (TMPREAL) rd_luniform(cstackptr[1].x, cstackptr[0].x);
			if (cstackptr[0].y != cstackptr[1].y) cstackptr[1].y = (TMPREAL) rd_luniform(cstackptr[1].y, cstackptr[0].y);
			cstackptr++;
			break;
		case rnd_exponential_me:
			cstackptr[0].x = (TMPREAL) rd_lexponential(cstackptr[0].x);
			if (cstackptr[0].y != 0) cstackptr[0].y = (TMPREAL) rd_lexponential(cstackptr[0].y);
			break;
		case rnd_erlang_me:
			cstackptr[1].x = (TMPREAL) rd_lerlang(nint(cstackptr[1].x), cstackptr[0].x);
			cstackptr[1].y = 0;
			cstackptr++;
			break;
		case rnd_weibull_me:
			cstackptr[1].x = (TMPREAL) rd_lweibull(cstackptr[1].x, cstackptr[0].x);
			cstackptr[1].y = 0;
			cstackptr++;
			break;
		case rnd_norm_me:
			cstackptr--;
			cstackptr[0].x = (TMPREAL) rd_lnormal(0.0, 1.0);
			cstackptr[0].y = 0;
			break;
		case rnd_normal_me:
			cstackptr[1].x = (TMPREAL) rd_lnormal(cstackptr[1].x, cstackptr[0].x);
			if (cstackptr[0].y > 0) {
				cstackptr[1].y = (TMPREAL) rd_lnormal(cstackptr[1].y, cstackptr[0].y);
			} else {
				cstackptr[1].y = 0;
			}
			cstackptr++;
			break;
		case rnd_lognormal_me:
			cstackptr[1].x = (TMPREAL) rd_llognormal(cstackptr[0].x, cstackptr[1].x);		/* Reversed intentionally */
			if (cstackptr[0].y > 0 && cstackptr[1].y > 0) {
				cstackptr[1].y = (TMPREAL) rd_llognormal(cstackptr[0].y, cstackptr[1].y);	/* Reversed intentionally */
			} else {
				cstackptr[1].y = 0;
			}
			cstackptr++;
			break;
		case rnd_triangle_me:
			cstackptr[2].x = (TMPREAL) rd_ltriangular(cstackptr[2].x, cstackptr[1].x, cstackptr[0].x);
			if (cstackptr[0].y < cstackptr[1].y && cstackptr[0].y > cstackptr[2].y) {
				cstackptr[2].y = (TMPREAL) rd_ltriangular(cstackptr[2].y, cstackptr[1].y, cstackptr[0].y);
			} else {
				cstackptr[2].y = 0;
			}
			cstackptr += 2;
			break;

		case ceil_me:
			NotImplemented(cstackptr->y, "ceil");
			cstackptr->x = CEIL(cstackptr->x);
			cstackptr->y = 0; break;

		case floor_me:
			NotImplemented(cstackptr->y, "floor");
			cstackptr->x = FLOOR(cstackptr->x);
			cstackptr->y = 0; break;

		case ldexp_me:
			cstackptr[1].x = LDEXP(cstackptr[1].x, GVTrimToInt32(cstackptr[0].x)); 
			cstackptr[1].y = LDEXP(cstackptr[1].y, GVTrimToInt32(cstackptr[0].x)); 
			cstackptr++; break;

		case rainbow_me:
			cstackptr[2].x = GVSelectContinuumColor((double) cstackptr[2].x, (double) cstackptr[1].x, (double) cstackptr[0].x, GV_PAL_DEFAULT);
			cstackptr += 2;
			break;
			
		case rgb_me:
			cstackptr[2].x = MY_RGB(GVTrimToInt16(cstackptr[2].x), GVTrimToInt16(cstackptr[1].x), GVTrimToInt16(cstackptr[0].x) );
			cstackptr += 2;
			break;

		case round_me:
			NotImplemented(cstackptr->y, "round");
			r = cstackptr->x;		cstackptr++;			/* Requested number of digits */
			if (r > 300 || r < -300) break;
			r = POW(10.0, nint(r));							/* Number of digits */
			x = cstackptr->x*r;
			if (FABS(FMOD(x,1.0)) == 0.5) x += .1*(rand()-RAND_MAX/2.0)/RAND_MAX;
			if (x < 0) 
			  cstackptr->x = FLOOR(x+0.5) / r;
			else
			  cstackptr->x =  CEIL(x-0.5) / r;
			break;

#ifdef HAS_BESSEL
		case j0_me:
			NotImplemented(cstackptr->y, "j0");
			cstackptr->x = J0(cstackptr->x);
			cstackptr->y = 0; break;

		case j1_me:
			NotImplemented(cstackptr->y, "j1");
			cstackptr->x = J1(cstackptr->x);
			cstackptr->y = 0; break;

		case jn_me:
			NotImplemented(cstackptr->y, "jn");
			cstackptr[1].x = JN(GVTrimToInt32(cstackptr[1].x), cstackptr[0].x); 
			cstackptr++; break;

		case y0_me:
			NotImplemented(cstackptr->y, "y0");
			cstackptr->x = Y0(cstackptr->x);
			cstackptr->y = 0; break;

		case y1_me:
			NotImplemented(cstackptr->y, "y1");
			cstackptr->x = Y1(cstackptr->x);
			cstackptr->y = 0; break;

		case yn_me:
			NotImplemented(cstackptr->y, "yn");
			cstackptr[1].x = YN(GVTrimToInt32(cstackptr[1].x), cstackptr[0].x); 
			cstackptr++; break;
#endif	/* HAS_BESSEL */

		case tn_me:
			i = nint(cstackptr[1].x);							/* Order				*/
			if (i == 0) {
				++cstackptr;
				cstackptr->x = 1;
				cstackptr->y = 0;
			} else if (i == 1) {
				cstackptr[1] = cstackptr[0];
				cstackptr++;
			} else {
				z = C_ACOSH(*cstackptr);						/* acosh(z)			*/
				z.x *= i; z.y *= i;								/* i*acos(z)		*/
				*(++cstackptr) = C_COSH(z);					/* cos(i*acos(z))	*/
			}
			break;
			
		case ndtri_me:
			NotImplemented(cstackptr->y, "ndtri");
			cstackptr->x = NDTRI(cstackptr->x);
			cstackptr->y = 0; break;

		case fdm0p5_me:
			NotImplemented(cstackptr->y, "fdm0p5");
			cstackptr->x = FDM0P5(cstackptr->x); 
			cstackptr->y = 0; break;
		case fdp0p5_me:
			NotImplemented(cstackptr->y, "fdp0p5");
			cstackptr->x = FDP0P5(cstackptr->x); 
			cstackptr->y = 0; break;
		case fdp1p5_me:
			NotImplemented(cstackptr->y, "fdp1p5");
			cstackptr->x = FDP1P5(cstackptr->x); 
			cstackptr->y = 0; break;
		case fdp2p5_me:
			NotImplemented(cstackptr->y, "fdp2p5");
			cstackptr->x = FDP2P5(cstackptr->x); 
			cstackptr->y = 0; break;

		case beta_me:										/* Beta function */
		case lnbeta_me:									/* Beta function */
			b = *cstackptr++;
			a = *cstackptr;
			if (a.y == 0 && b.y == 0) {
				cstackptr->x = LN_GAMMA(a.x)+LN_GAMMA(b.x)-LN_GAMMA(a.x+b.x);
				cstackptr->y = 0;
				if (mycmd == beta_me) cstackptr->x = EXP(cstackptr->x);
			} else {
				*cstackptr = C_SUB(C_ADD(C_LNGAMMA(a), C_LNGAMMA(b)), C_LNGAMMA(C_ADD(a,b)));
				if (mycmd == beta_me) *cstackptr = C_EXP(*cstackptr);
			}
			break;
			
		case betai_me:										/* Beta function */
		case betai_Ix_me:
			b = *cstackptr++;
			a = *cstackptr++;
			z = *cstackptr;
			r = FABS(a.y)+FABS(b.y)+FABS(z.y);
			NotImplemented(r, "betai");
			cstackptr->x = (mycmd == betai_me) ? BETAI(z.x, a.x, b.x) : BETAI_Ix(z.x, a.x, b.x);
			cstackptr->y = 0;
			break;

		case z_test_me:							/* Z test when mean/sigma known for population */
			/* cstackptr->x = sigma; (cstackptr+1)->x = mean; */
			GVGETITEM(&icnt, cmds, int);		/* Number of elements in array */
			GVGETITEM(&x, cmds, TMPREAL);		/* Mean of array */
			if (icnt <= 0) {
				t = 1.0;
			} else {
				t = (x-(cstackptr+1)->x) / (cstackptr->x/sqrt(icnt));
				t = 2*NDTR(FABS(t)) - 1.0;
			}
			cstackptr++;							/* Pop sigma */
			cstackptr->x = t; cstackptr->y = 0;
			break;

		case t1_test_me:							/* t-test with one array and mean */
			/* cstackptr->x = mean; */
			GVGETITEM(&icnt, cmds, int);		/* Number of elements in array */
			GVGETITEM(&x, cmds, TMPREAL);		/* Mean of array */
			GVGETITEM(&r, cmds, TMPREAL);		/* Sigma of the array */
			if (icnt <= 1) {
				t = 1.0;
			} else {
				v = icnt-1;												/* Degrees of freedom	*/
				t = (x-cstackptr->x) / (r/sqrt(icnt));		/* t value in t-test		*/
				t = 1.0 - BETAI_Ix(v/(v+t*t), v/2, 0.5);
			}
			cstackptr->x = t; cstackptr->y = 0;
			break;

		case t_test_me:							/* Student's t-Distribution	*/
			v = (cstackptr++)->x;				/* Degrees of freedom (total) */
			t = cstackptr->x;						/* Difference of the mean		*/
			cstackptr->x = 1.0 - BETAI_Ix(v/(v+t*t), v/2, 0.5);
			cstackptr->y = 0;
			break;

		case f_test_me:							/* F-Distribution function			*/
			v2 = (cstackptr++)->x;				/* Degrees of freedom on 1st set */
			v1 = (cstackptr++)->x;				/* Degrees of freedom on 2nd set */
			F  = cstackptr->x;					/* Statistic F							*/
			cstackptr->x = BETAI_Ix(v2/(v2+v1*F), v2/2, v1/2);
			cstackptr->y = 0;
			break;

		case chi2_me:
			v = (cstackptr++)->x;
			t = cstackptr->x;
			if (t <= 0) {
				cstackptr->x = 0;
			} else {
				cstackptr->x = POW(t/2,v/2)/t*EXP(-t/2)/norm_gamma(v/2);
			}
			cstackptr->y = 0.0;
			break;

		case p_chi_me:
		case q_chi_me:
			v    = (cstackptr++)->x;			/* Number of degrees of freedom */
			chi2 = cstackptr->x;					/* chi^2 measured */
			cstackptr->x = PQ_CHI(chi2, v, (char) ((mycmd == p_chi_me) ? 'P' : 'Q') );
			cstackptr->y = 0;
			break;

		case gnoise_me:
			cstackptr--;							/* Now no arguments */
			cstackptr->x = NDTRI(drand48());
			cstackptr->y = NDTRI(drand48());
			while (fabs(cstackptr->x) > 8) cstackptr->x = NDTRI(drand48());
			while (fabs(cstackptr->y) > 8) cstackptr->y = NDTRI(drand48());
			break;

		case gauss_me:
		case gaussn_me:
			c_sigma = *cstackptr++;
			c_x0    = *cstackptr++;
			z       = *cstackptr;
			if (c_sigma.y == 0 && c_x0.y == 0 && z.y == 0) {
				if (c_sigma.y > 0) {
					cstackptr->x = EXP(-0.5*POW((z.x-c_x0.x)/c_sigma.x,2));
					if (mycmd == gauss_me) cstackptr->x /= (SQRT(2*PI)*c_sigma.x);
				} else {
					cstackptr->x = (z.x == c_x0.x) ? 1 : 0 ;
				}
				cstackptr->y = 0;
			} else {
				if (c_sigma.x == 0 && c_sigma.y == 0) {
					cstackptr->x = (z.x == c_x0.x && z.y == c_x0.y) ? 1 : 0 ;
					cstackptr->y = 0;
				} else {
					z = C_DIV(C_SUB(z,c_x0),c_sigma);		/* (x-x0)/sigma */
					z = C_MUL(z,z);  z.x /= -2; z.y /= -2;	/* u**2/2       */
					z = C_EXP(z);									/* exp[{(x-x0)/sigma}^2/2] */
					if (mycmd == gauss_me) {
						c_sigma.x *= SQRT(2*PI); c_sigma.y *= SQRT(2*PI);
						*cstackptr = C_DIV(z, c_sigma);
					}
				}
			}
			break;

		case poisson_me:
			c_x0 = *cstackptr++;
			z    = *cstackptr;
			if (c_x0.y == 0 && z.y == 0) {
				cstackptr->x = EXP(z.x*LOG(c_x0.x)-LN_GAMMA(z.x+1)-c_x0.x);
				cstackptr->y = 0;
			} else {
				*cstackptr = C_SUB(C_MUL(z,C_LOG(c_x0)),c_x0);
				z.x = z.x+1; 
				*cstackptr = C_EXP(C_SUB(*cstackptr, C_LNGAMMA(z)));
			}
			break;
		case lorentz_me:
			c_width = *cstackptr++;
			c_x0    = *cstackptr++;
			z       = *cstackptr;
			if (c_x0.y == 0 && c_width.y == 0 && z.y == 0) {
				c_width.x = FABS(c_width.x);
				cstackptr->x = c_width.x/2/PI / (POW(z.x-c_x0.x,2)+POW(c_width.x/2,2));
				cstackptr->y = 0;
			} else {
				c_width.x /= 2; c_width.y /= 2;
				z = C_SUB(z,c_x0);
				*cstackptr = C_DIV(c_width, C_ADD(C_MUL(z,z), C_MUL(c_width,c_width)));
				cstackptr->x /= PI; cstackptr->y /= PI;
			}
			break;
		case binomial_me:
			r = FABS(cstackptr[2].y)+FABS(cstackptr[1].y)+FABS(cstackptr->y);
			NotImplemented(r, "binomial");
			p = (*cstackptr++).x;
			n = (*cstackptr++).x;
			x = (*cstackptr).x;
			if (x >= 0 && x <= n) {
				cstackptr->x = EXP(LN_GAMMA(n+1)-LN_GAMMA(x+1)-LN_GAMMA(n-x+1) + x*LOG(p)+(n-x)*LOG(1-p));
			} else {
				cstackptr->x = 0;
			}
			cstackptr->y = 0;
			break;

		case edgeworth_me:
			r = FABS(cstackptr[4].y)+FABS(cstackptr[3].y)+FABS(cstackptr[2].y)+FABS(cstackptr[1].y)+FABS(cstackptr->y);
			NotImplemented(r, "edgeworth");
			kurt    = cstackptr++->x;
			skew    = cstackptr++->x;
			c_sigma = *cstackptr++;
			c_x0    = *cstackptr++;
			z       = *cstackptr;
			if (c_sigma.x > 0) {
				r  = (z.x - c_x0.x) / c_sigma.x;
				r2 = r*r;
				cstackptr->x = EXP(-r2/2) / c_sigma.x / SQRT(2*PI) * (
					skew/6 * (r*(r2-3)) + kurt/24 * ((r2-6)*r2+3) +
					skew*skew/72 * (((r2-15)*r2+45)*r2-15) );
			} else {
				cstackptr->x = (z.x == c_x0.x) ? 1 : 0 ;
			}
			cstackptr->y = 0;
			break;
			
		case weibull_me:
			r = FABS(cstackptr[3].y)+FABS(cstackptr[2].y)+FABS(cstackptr[1].y)+FABS(cstackptr->y);
			NotImplemented(r, "weibull");
			beta  = cstackptr++->x;
			eta   = cstackptr++->x;
			gamma = cstackptr++->x;
			x     = cstackptr->x;
			if (beta <= 0 || eta <= 0 || x <= gamma) {
				cstackptr->x = 0;
			} else {
				cstackptr->x = beta/eta * POW((x-gamma)/eta,beta-1) * EXP(-POW((x-gamma)/eta,beta));
			}
			break;

		case spline_me:
			NotImplemented(cstackptr->y, "spline");
			GVGETITEM(&arrayptr, cmds, ARRAY *);
			cstackptr->x = spl_eval(cstackptr->x, arrayptr, 0);
			cstackptr->y = 0;
			break;
		case ispln_me:
			NotImplemented(cstackptr->y, "ispln");
			GVGETITEM(&arrayptr, cmds, ARRAY *);
			cstackptr[1].x = ispl_eval(cstackptr[1].x, cstackptr[0].x, arrayptr);	/* Integral */
			cstackptr[1].y = 0;
			cstackptr++; break;
		case dspln_me:
			NotImplemented(cstackptr->y, "dspln");
			GVGETITEM(&arrayptr, cmds, ARRAY *);
			cstackptr->x = spl_eval(cstackptr->x, arrayptr, 1);
			cstackptr->y = 0;
			break;
		case ddspln_me:
			NotImplemented(cstackptr->y, "ddspln");
			GVGETITEM(&arrayptr, cmds, ARRAY *);
			cstackptr->x = spl_eval(cstackptr->x, arrayptr, 2);
			cstackptr->y = 0;
			break;

		case hv_me:
			if (cstackptr->x == 0.0)
				cstackptr->x = 0.5;
			else if (cstackptr->x < 0.0) 
				cstackptr->x = 0;
			else
				cstackptr->x = 1;
			cstackptr->y = 0;
			break;

		case cheby_me:									/* cheby(x,array) */
			NotImplemented(cstackptr->y, "cheby");
			GVGETITEM(&arrayptr, cmds, ARRAY *);
			cstackptr->x = chebyshev(cstackptr->x, arrayptr->x, *(arrayptr->size));
			cstackptr->y = 0;
			break;

/* String functions */
		case interp_string_stack_val:
			{	
				int sign;
				char *str, *endptr, *aptr;
				str = sstack_public[*(cmds++)].str;
				--cstackptr;
				cstackptr->x = strtod(str, &endptr);
				cstackptr->y = 0;
				while (isspace(*endptr)) endptr++;
				if (*endptr == '+' || *endptr == '-') {
					sign = (*endptr == '+') ? +1 : -1 ;
					endptr++;
					while (isspace(*endptr)) endptr++;
					if (tolower(*endptr) == 'j' || tolower(*endptr) == 'i') {
						cstackptr->y = sign;
						endptr++;
					} else {
						cstackptr->y = sign*strtod(endptr, &aptr);
						if (tolower(*aptr) == 'i' || tolower(*aptr) == 'j') {
							endptr = aptr+1;
						} else {
							cstackptr->y = 0;
						}
					}
				}
				while (isspace(*endptr)) endptr++;
				if (*endptr != '\0') {
					ERRprintf("WARNING: String \"%s\" interpreted as float - leftover \"%s\" ignored\n",
								 str, endptr);
				}
			}
			break;
			
		case rgb_color_me:							/* These are string only */
		case strcmp_me:
		case stricmp_me:
		case strlen_me:
		case strnlen_me:
		case strcspn_me:
		case strspn_me:
		case hex2int_me:
		case oct2int_me:
		case bin2int_me:
		case hex2float_me:
		case hex2double_me:
		case time2double_me:
		case rexx_words:
		case rexx_ichar:
		case atof_me:
		case atoi_me:
		case isatoi_me:
		case isatof_me:
		case file_sizeof_me:
		case file_dateof_me:
		case file_isfile_me:
		case file_isdir_me:
		case fclose_me:
		case pclose_me:
		case feof_me:
		case ferror_me:
		case fflush_me:
		case ftell_me:
		case fgetc_me:
		case fputs_me:
			--cstackptr;
			cstackptr->x = HandleNumStringFnc(mycmd, 0, 0, 0, 0, 0);
			cstackptr->y = 0;
			break;
		case strncmp_me:								/* These use a single int value */	
		case strnicmp_me:
		case lexequal_me:
		case strtol_me:
		case rexx_abbrev:
		case rexx_wordpos:
		case rexx_pos:
		case rexx_lastpos:
		case rexx_wordindex:
		case rexx_wordlength:
		case fputc_me:
		case base2int_me:
		case rexx_verify:								/* These use two int values */
		case rexx_compare:
			cstackptr->x = HandleNumStringFnc(mycmd, GVTrimToInt64(cstackptr->x), 0, 0, 0, 0);
			cstackptr->y = 0;
			break;
		case fseek_me:
			cstackptr[1].x = HandleNumStringFnc(mycmd, GVTrimToInt64(cstackptr[1].x), GVTrimToInt64(cstackptr[0].x), 0, 0, 0);
			cstackptr[1].y = 0;
			++cstackptr;
			break;
		case sprintf_me:
			do_sprintf(TRUE);							/* do_sprintf will modify cstackptr */
			break;
		case fprintf_me:
			do_sprintf(TRUE);							/* do_sprintf will modify cstackptr */
			--cstackptr;
			cstackptr->x = HandleNumStringFnc(fprintf_me, 0, 0, 0, 0, 0);
			cstackptr->y = 0;
			break;
		case printf_me:
			do_sprintf(TRUE);							/* do_sprintf will modify rstackptr */
			printf("%s", sstackptr[0].str);
			if (sstackptr->status == IS_MALLOC) free(sstackptr->str); 
			sstackptr++;
			--cstackptr;
			cstackptr->x = cstackptr->y  = 0;
			break;
#ifdef NT
		/* No fd, 1 int, return int */
		case creat_me:									/* Create file descriptor			*/
			cstackptr->x = HandleLowIOFnc(mycmd, 0, GVTrimToInt32(cstackptr->x), 0, 0, 0, 0);
			cstackptr->y = 0;
			break;
		/* No fd, 2 ints, return int */
		case open_me:									/* Open file descriptor				*/
		case open_comx_me:
		case beep_me:
			cstackptr[1].x = HandleLowIOFnc(mycmd, 0, GVTrimToInt32(cstackptr[1].x), GVTrimToInt32(cstackptr[0].x), 0, 0, 0);
			cstackptr[1].y = 0;
			cstackptr++;
			break;
		/* fd, 0 ints, return int */
		case set_baud_me:								/* These need no integer parameters */
		case write_me:									/* Write to file descriptor		*/
			cstackptr->x = HandleLowIOFnc(mycmd, GVTrimToInt32(cstackptr->x), 0, 0, 0, 0, 0);
			cstackptr->y = 0;
			break;
		/* fd, 0 ints, return string */
		case get_baud_me:								/* Returns string value */
			HandleLowIOFnc(mycmd, GVTrimToInt32(cstackptr->x), 0, 0, 0, 0, 0);
			cstackptr++;
			break;
		/* fd, 1 int, return int */
		case get_timeout_me:
			cstackptr[1].x = HandleLowIOFnc(mycmd, GVTrimToInt32(cstackptr[1].x), GVTrimToInt32(cstackptr[0].x), 0, 0, 0, 0);
			cstackptr[1].y = 0;
			cstackptr++;
			break;
		/* fd, 5 ints, return int */
		case set_timeout_me:
			cstackptr[5].x = HandleLowIOFnc(mycmd, GVTrimToInt32(cstackptr[5].x), GVTrimToInt32(cstackptr[4].x), GVTrimToInt32(cstackptr[3].x), GVTrimToInt32(cstackptr[2].x), GVTrimToInt32(cstackptr[1].x), GVTrimToInt32(cstackptr[0].x) );
			cstackptr[5].y = 0;
			cstackptr += 5;
			break;
		/* fd, 1 int, return string */
		case read_me:									/* Read from file descriptor		*/
		case query_me:
			HandleLowIOFnc(mycmd, GVTrimToInt32(cstackptr[1].x), GVTrimToInt32(cstackptr[0].y), 0, 0, 0, 0);
			cstackptr+= 2;
			break;
		/* Handled here */
		case lseek_me:									/* Seek within file descriptor	*/
			cstackptr[2].x = (TMPREAL) _lseeki64(GVTrimToInt32(cstackptr[2].x), GVTrimToInt64(cstackptr[1].x), GVTrimToInt32(cstackptr[0].x));
			cstackptr[2].y = 0;
			cstackptr += 2;
			break;
		case close_me:									/* Close file descriptor			*/
			cstackptr->x = _close(GVTrimToInt32(cstackptr->x));
			cstackptr->y = 0;
			break;
		case eof_me:									/* End of file test					*/
			cstackptr->x = _eof(GVTrimToInt32(cstackptr->x));
			cstackptr->y = 0;
			break;
		case tell_me:									/* End of file test					*/
			cstackptr->x = (TMPREAL) _telli64(GVTrimToInt32(cstackptr->x));
			cstackptr->y = 0;
			break;
#endif

		case ctype_isalnum:
		case ctype_isalpha:
		case ctype_iscntrl:
		case ctype_isdigit:
		case ctype_isgraph:
		case ctype_islower:
		case ctype_isprint:
		case ctype_ispunct:
		case ctype_isspace:
		case ctype_isupper:
		case ctype_isxdigit:
		case ctype_tolower:
		case ctype_toupper:
			cstackptr->x = (TMPREAL) HandleCtypeFnc(mycmd, nint(cstackptr->x));
			cstackptr->y = 0;
			break;

		case save_string_stack_val:				/* These are commands only */
		case rexx_concat:
		case rexx_upcase:
		case rexx_lowercase:
		case rexx_reverse:
		case pwd_me:
		case getenv_me:
		case fullpath_me:
		case fopen_me:
		case popen_me:
		case fgets_me:
		case hex2bin_me:
		case bin2hex_me:
		case bin_or_me:
		case bin_and_me:
		case bin_xor_me:
		case hex_or_me:
		case hex_and_me:
		case hex_xor_me:
			HandleStringFnc(mycmd, 0, 0, 0, 0.0);
			break;
		case load_string_stack_val:				/* These use a single int from cmds */
		case pop_string_stack:
			HandleStringFnc(mycmd, *(cmds++), 0, 0, 0.0);
			break;
		case ctime_me:									/* These use a single int from stack */
		case strftime_me:
		case rexx_char:
		case int2hex_me:								/* Was rexx_d2x */
		case int2oct_me:
		case int2bin_me:
		case rexx_word:
		case rexx_strip:
		case rexx_copies:
		case rexx_translate:
			HandleStringFnc(mycmd, GVTrimToInt64(cstackptr->x), 0, 0, 0.0);
			++cstackptr;
			break;
		case float2hex_me:
		case double2hex_me:
			HandleStringFnc(mycmd, 0, 0, 0, cstackptr->x);
			++cstackptr;
			break;
		case rexx_subword:							/* These use two ints */
		case rexx_left:
		case rexx_right:
		case rexx_center:			
		case int2base_me:
		case rexx_xrange:
		case rexx_space:
		case rexx_justify:
		case rexx_delstr:
		case rexx_delword:
			HandleStringFnc(mycmd, GVTrimToInt64(cstackptr[1].x), GVTrimToInt64(cstackptr[0].x), 0, 0.0);
			cstackptr += 2;
			break;
		case rexx_substr:
		case rexx_insert:
		case rexx_overlay:
			HandleStringFnc(mycmd, GVTrimToInt64(cstackptr[2].x), GVTrimToInt64(cstackptr[1].x), GVTrimToInt64(cstackptr[0].x), 0.0);
			cstackptr += 3;
			break;

		case integrate_me:
		{
#define	NDIVS	(100)
			double xh, xl, eps;
			int iargs, iskip, miniter, maxiter, rc, ierr;
			GVCMDS *solve_cmds_hold;

			/* Get and dump arguments */
			eps = 1E-5;										/* Nominal precision */
			miniter = 256;									/* Minimum # of iterations */
			maxiter = 65536;								/* Allow 65,000 iterations */
			iargs = *(cmds++);							/* How many args were given */
			while (iargs > 5) { cstackptr++; iargs--; }
			if (iargs >= 5) { maxiter = nint(cstackptr->x); cstackptr++; }	/* Burned by macro definitions */
			if (iargs >= 4) { miniter = nint(cstackptr->x); cstackptr++; }	/* Burned by macro definitions */
			if (iargs >= 3) { eps = cstackptr->x; cstackptr++;}				/* Precision requested */
			if (eps == DUMMY_VALUE) eps = 1E-5;
			ierr = 0;
			xh  = cstackptr->x; if (cstackptr->y != 0) ierr = -3; cstackptr++;		/* upper limit */
			xl  = cstackptr->x; if (cstackptr->y != 0) ierr = -3; cstackptr++;		/* lower limit */

			GVGETITEM(&iskip, cmds, int);				/* How long is "subroutine" cmds */
			solve_cmds_hold = solve_cmds;				/* Need to restore for re-entrancy */
			solve_cmds = cmds;							/* Publish cmds for subfnc_eval */

			/* Set the parameters, possibly via defaults */
			rc = GVIntegrate_C(--cstackptr, z_subfnc_eval, xl, xh, eps, miniter, maxiter);
			if (ierr != 0 && rc == 0) rc = ierr;
			switch (rc) {
				case  0: break;
				case -1: gv_math_error_msg("ERROR: Function could not be evaluated (INTEGRATE)\n"); break;
				case -2: gv_math_error_msg("ERROR: Did not converge within maximum number of evaluations (INTEGRATE)\n"); break;
				case -3: gv_math_error_msg("ERROR: Integration limits were truncated to real part only\n"); break;
				default:	gv_math_error_msg("ERROR: Unknown error (INTEGRATE)\n"); break;
			}
			cmds += iskip;																				/* And skip over subroutine	*/
			solve_cmds = solve_cmds_hold;															/* And restore */
		}
		break;

		case solve_me:									/* Very important */
		case dydx_me:
/*			ERRprintf("WARNING: SOLVE for complex arguments has not been (will not be) implemented\n"); */
			if (GVMathMode & MATHERROR) 
				ERRprintf("WARNING: Functions of functions have not been completely implemented for complex arguments\n");
			cstackptr += *(cmds++);					/* Dump all arguments */
			GVGETITEM(&icnt, cmds, int);
			cmds += icnt;
			--cstackptr;								/* And return whatever was last there */
			break;

		case sum_me:
		case prod_me:
		{
			TMPCOMPLEX sum, tmp;
			int iargs, iskip;
			int istart, iend, istep, errcnt, ierr;
			char token[LONG_STR_SIZE]; 
			GVCMDS *solve_cmds_hold;

			/* Get and dump arguments */
			istart = iend = istep = 1;					/* Default values */
			iargs = *(cmds++);							/* Dump excess arguments */
			while (iargs > 3) { cstackptr++; iargs--; }
			if (iargs >= 3) { istep = nint(cstackptr->x); cstackptr++; }
			iend    = nint(cstackptr->x); cstackptr++;	/* Upper limit */
			istart  = nint(cstackptr->x); cstackptr++;	/* Lower limit */

			GVGETITEM(&iskip, cmds, int);				/* How long is "subroutine" cmds */
			solve_cmds_hold = solve_cmds;				/* Need to restore for re-entrancy */
			solve_cmds = cmds;							/* Publish cmds for subfnc_eval */

			/* Set the parameters, possibly via defaults */
			sum.x = (mycmd == sum_me) ? 0 : 1;		/* Initial value is either 0 or 1 for sum or product */
			sum.y = 0;
			errcnt = 0;
			if (istep < 0) istep = -istep;
			if (istep == 0) {
				gv_math_error_msg("ERROR: Continued sum/product with step of 0 can't be completed\n");
			} else {
				if (istart>iend) { i = istart; istart = iend; iend = i; }
				for (i=istart; i<=iend; i+=istep) {
					TMPCOMPLEX rc;
					z_subfnc_eval(i, &ierr, &rc);
					if (mycmd == sum_me)  {sum.x += rc.x; sum.y += rc.y;}
					if (mycmd == prod_me) {tmp.x = sum.x*rc.x-sum.y*rc.y; tmp.y = sum.x*rc.y+sum.y*rc.x; sum = tmp; }
					if (ierr != 0) { if (errcnt++ > 20) break; }
				}
			}
			if (errcnt != 0) { sprintf(token, "Errors (%d) during continued sum/product\n", errcnt); gv_math_error_msg(token); }
			--cstackptr;
			cstackptr->x = sum.x;
			cstackptr->y = sum.y;
			cmds += iskip;																				/* And skip over subroutine	*/
			solve_cmds = solve_cmds_hold;															/* And restore */
		}
		break;

		case chdir_me:
		case mkdir_me:
		case rmdir_me:
		case rm_me:
		case mv_me:
		case unlink_me:
			--cstackptr;
			cstackptr->x = HandleOSFnc(mycmd);
			cstackptr->y = 0;
			break;

		case lex_get_token:
		case lex_get_token_p:
		case lex_chk_token:
			--cstackptr;
			cstackptr->x = HandleLexFnc(mycmd, 0, 0);
			cstackptr->y = 0;
			break;

/* Array functions */
		case poly_me:
		case dpoly_me:
			{
			int i,j, ideg;
			TMPREAL cf;
			TMPCOMPLEX tmp={0,0},tmp2;

			GVGETITEM(&arrayptr, cmds, ARRAY *);

			if (mycmd == poly_me) {						/* Handle poly here			*/
				for (i=*(arrayptr->size)-1; i>=0; i--) {
					tmp2.x = (cstackptr->x*tmp.x-cstackptr->y*tmp.y) + arrayptr->x[i];
					tmp2.y = (cstackptr->x*tmp.y+cstackptr->y*tmp.x);
					tmp = tmp2;
				}
				*cstackptr = tmp;
			} else if (mycmd == dpoly_me) {
				ideg = nint(cstackptr->x);				/* Dump the degree		*/
				cstackptr++;
				if (ideg >= *(arrayptr->size) || ideg < -10) {
					tmp.x = tmp.y = 0;
				} else {
					for (i=*(arrayptr->size)-1; i>=ideg && i>=0; i--) {	/* Terms */
						cf = arrayptr->x[i];
						if (ideg > 0) for (j=0; j< ideg; j++) cf *= i-j;
						if (ideg < 0) for (j=0; j<-ideg; j++) cf /= i+j+1;
						tmp2.x = (cstackptr->x*tmp.x-cstackptr->y*tmp.y) + cf;
						tmp2.y = (cstackptr->x*tmp.y+cstackptr->y*tmp.x);
						tmp = tmp2;
					}
					for (i=0; i>ideg; i--) {
						tmp2.x = (cstackptr->x*tmp.x-cstackptr->y*tmp.y);
						tmp2.y = (cstackptr->x*tmp.y+cstackptr->y*tmp.x);
						tmp = tmp2;
					}
				}
				*cstackptr = tmp;
			} else {
				ERRprintf("GVCALC Screwup:  Tell developers problem at dpoly_me in gvcalc.c\n");
				--cstackptr;
				cstackptr->x = cstackptr->y = 0;
			}
			break;
		}

		case array_min:
		case array_max:
		case array_sum:
		case array_avg:
		case array_count:
		case array_median:
		case array_mad:
		case array_var:
		case array_std:
		case array_sdom:
		case array_rms: 
		case array_skew:
		case array_kurt:
		case array_span:
		case array_absmin:
		case array_absmax:
		case array_abssum:
		case array_absavg:
			if (*(cmds++) == 0) {						/* 0 ==> Arrays */
				imax = nint(cstackptr->x); cstackptr++;
				imin = nint(cstackptr->x);
				GVGETITEM(&arrayptr, cmds, ARRAY *);
				cstackptr->x = gv_eval_array_fnc(mycmd, arrayptr, imin, imax);
				cstackptr->y = 0;
			} else {
				xmax = (REAL) cstackptr->x; cstackptr++;
				xmin = (REAL) cstackptr->x;
				GVGETITEM(&curveptr, cmds, CURVE *);
				cstackptr->x = gv_eval_curve_fnc(mycmd, curveptr, xmin, xmax);
				cstackptr->y = 0;
			}
			break;

		case array_covar:
		case array_weight_avg:
		case array_weight_sigma:
		case array_weight_var:
		case array_weight_std:
		case array_weight_sdom:
		case array_weight_absavg:
			imax = nint(cstackptr->x); cstackptr++;
			imin = nint(cstackptr->x);
			GVGETITEM(&arrayptr, cmds, ARRAY *);
			GVGETITEM(&arrayptr2, cmds, ARRAY *);
			cstackptr->x = gv_eval_array_fnc2(mycmd, arrayptr, arrayptr2, imin, imax);
			cstackptr->y = 0;
			break;

		case array_index:
			GVGETITEM(&arrayptr, cmds, ARRAY *);
			cstackptr->x = gv_find_in_array(arrayptr, cstackptr->x);
			cstackptr->y = 0;
			break;

		case complex_poly_me:
		{
			int len;
			COMPLEX *zptr;
			TMPCOMPLEX tmp={0,0},tmp2;

			GVGETITEM(&c_arrayptr, cmds, COMPLEX_ARRAY *);

			len = *(c_arrayptr->size);				/* Number of terms			*/
			zptr = c_arrayptr->z + (len-1);		/* Start at last element	*/
			while (len--) {
				tmp2.x = (cstackptr->x*tmp.x-cstackptr->y*tmp.y) + zptr->x;
				tmp2.y = (cstackptr->x*tmp.y+cstackptr->y*tmp.x) + zptr->y;
				tmp = tmp2;
				zptr--;
			}
			*cstackptr = tmp;
			break;
		}

		case curve_integral:
		case curve_correlate:
		case curve_avg:
		case curve_median:
		case curve_std:
		case curve_var:
		case curve_skew:
		case curve_kurt:
			xmax = (REAL) cstackptr->x; cstackptr++;
			xmin = (REAL) cstackptr->x;
			GVGETITEM(&curveptr, cmds, CURVE *);
			cstackptr->x = gv_eval_curve(mycmd, curveptr, xmin, xmax, NULL);
			cstackptr->y = 0;
			break;

		case time_me:
			--cstackptr;
			cstackptr->x = (TMPREAL) time(NULL); cstackptr->y = 0;
			break;

		case clock_me:
			--cstackptr;
			cstackptr->x = clock()/(1.0*CLOCKS_PER_SEC); cstackptr->y = 0;
			break;

		case timer_me:
			cstackptr->x = my_timer((BOOL) (cstackptr->x != 0.0) );
			cstackptr->y = 0;
			break;

		case curve_pintegral:
			b = *cstackptr++;						/* Upper limit */
			a = *cstackptr;						/* Lower limit	*/
			GVGETITEM(&curveptr, cmds, CURVE *);
			cstackptr->x = p_integrate(curveptr, a.x, b.x);
			cstackptr->y = 0;
			break;

		case curve_near:
			yp = (cstackptr++)->x;										/* Y value */
			xp = cstackptr->x;											/* X value */
			GVGETITEM(&curveptr, cmds, CURVE *);
			cstackptr->x = find_near_curve(2, curveptr, xp, yp, 0.0);
			cstackptr->y = 0;
			break;

		case curve_3d_near:
			zp = (cstackptr++)->x;										/* Z value */
			yp = (cstackptr++)->x;										/* Y value */
			xp = cstackptr->x;											/* X value */
			GVGETITEM(&curveptr, cmds, CURVE *);
			cstackptr->x = find_near_curve(3, curveptr, xp, yp, zp);
			cstackptr->y = 0;
			break;

		case load_dummy_value:
			--cstackptr;
			cstackptr->x = DUMMY_VALUE; cstackptr->y = 0;
			break;
		case load_e:
			--cstackptr;
			cstackptr->x = e; cstackptr->y = 0;
			break;
		case load_pi:
			--cstackptr;
			cstackptr->x = PI; cstackptr->y = 0;
			break;
		case load_realmin:
			--cstackptr;
			cstackptr->x = REAL_MIN; cstackptr->y = 0;
			break;
		case load_realmax:
			--cstackptr;
			cstackptr->x = REAL_MAX; cstackptr->y = 0;
			break;
		case load_i:
			--cstackptr;
			cstackptr->x = GVLocalIndex; cstackptr->y = 0;
			break;
		case load_j:
			--cstackptr;
			cstackptr->x = 0; cstackptr->y = 1;
			break;
			
		case load_stack_val:						/*  Load SS:SP-4-4*[DS:DI] */
			--cstackptr;
			*cstackptr = cstack_public[*(cmds++)];
			break;
		case save_stack_val:						/*  Pop ST(0) onto secondary stack */
			cstack_public[subptr++] = *(cstackptr++);
			break;
		case pop_stack:							/*  Decrement stack by [DS:DI] */
			subptr -= *(cmds++);					/* Decrement by next argument */
			break;
		case xeq_function:						/*  Execute function pointed by next adr */
			GVGETITEM(&fnc, cmds, EXT_FNC_LINK *);
			i = *(cmds++);							/* Position of the stack */
			--cstackptr;							/* Pop the stack one		 */
			cstackptr->x = cstackptr->y = 0;
			rc = (*fnc)(1, (TMPREAL *) cstackptr, (TMPREAL *) (cstack_public+i));
			if (rc < 0) {
				TMPREAL buf[20];					/* Allow up to 20 variables	*/
				int j;
				for (j=i; j<subptr; j++) buf[j-i] = cstack_public[j].x;
				if ((*fnc)(0, &cstackptr->x, buf) != 0) rcode = 2;	/* REAL, result, args	 */
			} else if (rc > 0) {
				rcode = 2;
			}
			break;
		case xeq_function_a:						/* Execute real function of read/str arguments */
		case xeq_str_function:					/*  Execute function pointed by next adr */
		{
			int i,j,k;
			char *tmpstr;							/* Response */
			char **strargs=NULL;					/* Location of string arguments */
			int s_args;								/* Number of string arguments */
			if (mycmd == xeq_str_function) {
				GVGETITEM(&sfnc, cmds, EXT_STR_FNC_LINK *);
			} else {
				GVGETITEM(&fnca, cmds, EXT_FNCA_LINK *);
			}
			s_args = *(cmds++);					/* Number of string argumentts */
			i = *(cmds++);							/* Position of the stack */
			j = *(cmds++);							/* Position of the string stack */
			if (s_args > 0) {
				strargs = calloc(s_args, sizeof(*strargs));
				for (k=0; k<s_args; k++) strargs[k] = sstack_public[j+k].str;
			}
			if (mycmd == xeq_str_function) {
				rc = (*sfnc)(1, &tmpstr, (TMPREAL *) cstack_public+i, strargs);
				if (rc < 0) {							/* Try again with only real args */
					TMPREAL buf[20];
					int j;
					for (j=i; j<subptr; j++) buf[j-i] = cstack_public[j].x;
					rc = (*sfnc)(0, &tmpstr, (TMPREAL *) buf, strargs);
				}
				if (rc != 0) {
					if (s_args > 0) free(strargs);
					rcode = 2;	/* REAL, result, args	 */
				}
				--sstackptr;							/* And put the value on the stack */
				sstackptr->status = IS_MALLOC;
				sstackptr->str = tmpstr;
			} else {
				--cstackptr;							/* Pop the stack one		 */
				cstackptr->x = cstackptr->y = 0;
				rc = (*fnca)(1, (TMPREAL *) cstackptr, (TMPREAL *) cstack_public+i, strargs);
				if (rc < 0) {							/* Try again with only real args */
					TMPREAL buf[20];
					int j;
					for (j=i; j<subptr; j++) buf[j-i] = cstack_public[j].x;
					cstackptr->y = 0;
					rc = (*fnca)(0, &cstackptr->x, (TMPREAL *) buf, strargs);
				}
				if (rc != 0) {
					if (s_args > 0) free(strargs);
					rcode = 2;	/* REAL, result, args	 */
				}
			}
			if (s_args > 0) free(strargs);
		}
			break;

		case load_real:							/*  Load simple real */
			GVGETITEM(&realptr, cmds, REAL *);
			--cstackptr;
			cstackptr->x = *realptr;
			cstackptr->y = 0;
			break;
		case load_double:							/*  Load simple double */
			GVGETITEM(&doubleptr, cmds, DOUBLE *);
			--cstackptr;
			cstackptr->x = *doubleptr;
			cstackptr->y = 0;
			break;
		case load_complex:						/*  Load simple complex */
			GVGETITEM(&complexptr, cmds, COMPLEX *);
			--cstackptr;
			cstackptr->x = complexptr->x;
			cstackptr->y = complexptr->y;
			break;
		case load_int:								/*  Load simple int */
			GVGETITEM(&intptr, cmds, INT *);
			--cstackptr;
			cstackptr->x = *intptr;
			cstackptr->y = 0;
			break;			
		case load_string_idx:					/* Load specific element of string */
			GVGETITEM(&stringptr, cmds, CHAR *);
			i = nint(cstackptr->x);
			cstackptr->x = stringptr[i]; cstackptr->y = 0;
			break;
		case load_string_tmp:					/* Load embedded string (in cmds) */
			--sstackptr;
			sstackptr->str  = (CHAR *) cmds;	/* String starts here	*/
			sstackptr->status = IS_STATIC;
			cmds += strlen((CHAR *) cmds)+1;	/* And skip over it		*/
			break;
		case load_pfile_ptr:
			GVGETITEM(&pfileptr, cmds, FILE **);
			--sstackptr;
			sstackptr->str  = (CHAR *) pfileptr;
			sstackptr->status = IS_PFUNIT;
			break;
		case load_string_ptr:
			GVGETITEM(&stringptr, cmds, CHAR *);
			--sstackptr;
			sstackptr->str  = (CHAR *) stringptr;
			sstackptr->status = IS_STATIC;
			break;
		case load_strarray_idx:
			GVGETITEM(&s_arrayptr, cmds, STRING_ARRAY *);
			i = nint(cstackptr->x); cstackptr++;
			if (WarnArray && (i < 0 || i >= *s_arrayptr->size)) {			/* Warn on array bounds */
				if (GVMathMode & MATHWARN) gv_array_bounds_msg(i, *s_arrayptr->size);
				WarnArray = FALSE;								/* Only once per run */
			}
			--sstackptr;
			sstackptr->str  = s_arrayptr->sval[i];
			sstackptr->status = IS_STATIC;
			break;
			
		case load_surface_pt:
		case surf_interp:													/* Interpolate on a surface */
			yp = (cstackptr++)->x;
			xp = cstackptr->x;
			GVGETITEM(&surfaceptr, cmds, CURVE *);
			cstackptr->x = surface_interpolate(surfaceptr, xp, yp);
			cstackptr->y = 0;
			break;

		case load_real_idx:						/* Load real indexed value (array+stack) */
		case load_real_array:					/* Load array indexed value (array+index) */
			if      (mycmd == load_real_array) {i = GVLocalIndex; --cstackptr;}
			else if (mycmd == load_real_idx)    i = nint(cstackptr->x);
			GVGETITEM(&arrayptr, cmds, ARRAY *);
			if (WarnArray && (i < 0 || i >= *arrayptr->size)) {			/* Warn on array bounds */
				if (GVMathMode & MATHWARN) gv_array_bounds_msg(i, *arrayptr->size);
				WarnArray = FALSE;								/* Only once per run */
			}
			cstackptr->x = arrayptr->x[i];						/* (--cstackptr) above */
			cstackptr->y = 0;
			break;

		case load_int_idx:						/*  Load from indexed integer array */
		case load_int_array:
			if      (mycmd == load_int_array) {i = GVLocalIndex; --cstackptr;}
			else if (mycmd == load_int_idx)    i = nint(cstackptr->x);
			GVGETITEM(&i_arrayptr, cmds, INT_ARRAY *);
			if (WarnArray && (i < 0 || i >= *i_arrayptr->size)) {			/* Warn on array bounds */
				if (GVMathMode & MATHWARN) gv_array_bounds_msg(i, *i_arrayptr->size);
				WarnArray = FALSE;								/* Only once per run */
			}
			cstackptr->x = i_arrayptr->ival[i];
			cstackptr->y = 0;
			break;

		case load_double_idx:					/*  Load from indexed double array */
		case load_double_array:
			if      (mycmd == load_double_array) {i = GVLocalIndex; --cstackptr;}
			else if (mycmd == load_double_idx)    i = nint(cstackptr->x);
			GVGETITEM(&d_arrayptr, cmds, DOUBLE_ARRAY *);
			if (WarnArray && (i < 0 || i >= *d_arrayptr->size)) {			/* Warn on array bounds */
				if (GVMathMode & MATHWARN) gv_array_bounds_msg(i, *d_arrayptr->size);
				WarnArray = FALSE;								/* Only once per run */
			}
			cstackptr->x = d_arrayptr->x[i];
			cstackptr->y = 0;
			break;

		case load_complex_idx:						/* Load complex indexed value (array+stack) */
		case load_complex_array:					/* Load array indexed value (array+index) */
			if      (mycmd == load_complex_array) {i = GVLocalIndex; --cstackptr;}
			else if (mycmd == load_complex_idx)    i = nint(cstackptr->x);
			GVGETITEM(&c_arrayptr, cmds, COMPLEX_ARRAY *);
			if (WarnArray && (i < 0 || i >= *c_arrayptr->size)) {			/* Warn on array bounds */
				if (GVMathMode & MATHWARN) gv_array_bounds_msg(i, *c_arrayptr->size);
				WarnArray = FALSE;								/* Only once per run */
			}
			cstackptr->x = c_arrayptr->z[i].x;				/* (--cstackptr) above */
			cstackptr->y = c_arrayptr->z[i].y;				/* (--cstackptr) above */
			break;

		default:
			ERRprintf("PROGRAMMER SCREWUP: GVCALC case %d was not handled\n", mycmd);
			return(-1);
		}

		if (cstackptr > cstackptr_ini) {
			ERRputs("Complex stack underflow in evaluation (huh?)\n");
			return(-1);
		}
		if (sstackptr > sstackptr_ini) {						/* Oops! */
			ERRputs("String stack underflow in evaluation (huh?)\n");
			return(-1);
		}

#if (defined CSET2 || defined MSC60 || defined MSC70)
		if (_Sys_Math_Exception != 0) return(-2);
#endif

		/* If requested, show stack at end of the operation */
		if (SysDebugFlag & 0x04) ShowComplexCmdStack(cstackptr_ini, cstackptr);
	}

	if (subptr != subptr_ini) {
		ERRputs("WARNING: Something left on secondary stack (huh?)\n");
		return(-1);
	} else if (ssubptr != ssubptr_ini) {
		ERRputs("WARNING: Something left on secondary string stack (huh?)\n");
		return(-1);
	}

	if (SysDebugFlag & 0x06) TTYprintf("----------------------------\n");
	return(rcode);
}
	

/* ---------------------------------------------------------------------------
-- This set of routines checks the value of a TMPREAL number and makes sure
-- it will fit into the given type variable.  If not, the value is set to the
-- maximum value which will fit.
--------------------------------------------------------------------------- */
INT16 GVTrimToInt16(TMPREAL x) {
	x += (x>0) ? 0.5 : -0.5;						/* Make it nearest int */
	return (x>INT16_MAX)  ? INT16_MAX  : (x<INT16_MIN) ? INT16_MIN : (INT16) x;
}
UINT16 GVTrimToUint16(TMPREAL x) {
	x += 0.5;											/* Make it nearest int */
	return (x>UINT16_MAX) ? UINT16_MAX : (UINT16) x;
}

INT32 GVTrimToInt32(TMPREAL x) {
	x += (x>0) ? 0.5 : -0.5;						/* Make it nearest int */
	return (x>INT32_MAX)  ? INT32_MAX  : (x<INT32_MIN) ? INT32_MIN : (INT32) x;
}
UINT32 GVTrimToUint32(TMPREAL x) {
	x += 0.5;											/* Make it nearest int */
	return (x>UINT32_MAX) ? UINT32_MAX : (UINT32) x;
}

INT64 GVTrimToInt64(TMPREAL x) {
	x += (x>0) ? 0.5 : -0.5;						/* Make it nearest int */
	return (x>INT64_MAX)  ? INT64_MAX : (x<INT64_MIN) ? INT64_MIN : (INT64) x;
}
UINT64 GVTrimToUint64(TMPREAL x) {
	x += 0.5;											/* Make it nearest int */
	return (x>UINT64_MAX) ? UINT64_MAX : (UINT64) x;
}

long GVTrimToNint(TMPREAL x) {
	double rval;
	rval = GVTrimToDouble(x);
	return (rval >= 0) ? ( (long) (rval+0.5) ) : ( - (long) (-rval+0.5) );
}

double GVTrimToDouble(TMPREAL x) {
	if (x <= DBL_MAX && x >= -DBL_MAX) return((double) x);
	if (GVMathMode & MATHWARN) 
		ERRputs("Math Warning: Result out of range -- Value set to +/-MAX\n");
	return( x>0 ? DBL_MAX : -DBL_MAX );
}

COMPLEX GVTrimToComplex(TMPCOMPLEX z) {
	COMPLEX result;
	result.x = GVTrimToReal(z.x);
	result.y = GVTrimToReal(z.y);
	return(result);
}

float GVTrimToFloat(TMPREAL x) {
	if (FABS(x) <= REAL_MIN) return(0.0f);
	if (FABS(x) <= REAL_MAX) return((float) x);
	if (GVMathMode & MATHWARN) 
		ERRputs("Math Warning: Result out of range -- Value set to +/-MAX\n");
	return (float) ( x>0 ? REAL_MAX : -REAL_MAX );
}


/* ===========================================================================
-- Routine to scan token and select one of the continuum color schemes
--
-- Usage: GVP_CONTINUUMPALETTE GVSelectContinuumPalette(char *token, GVP_CONTINUUMPALETTE default);
--
-- Inputs: token - token to be interpreted.  If NULL, will query locally.
--         dflt  - value to return as default (no entry)
--                 Use GV_PAL_DEFAULT to get the current global setting
--
-- Output: none
--
-- Return: A valid GVP_CONTINUUMPALETTE value (or dflt).  GV_PAL_ERROR 
--         indicates a problem in interpreting.
=========================================================================== */
#define	LOOKUP_PALETTE_SIZE	(1024)
static int UserLookupPalette[LOOKUP_PALETTE_SIZE];		/* User palette */

GVP_CONTINUUMPALETTE GVSelectContinuumPalette(char *token, GVP_CONTINUUMPALETTE dflt) {
	
	GVP_CONTINUUMPALETTE rc;
	char mytok[DFLT_STR_SIZE];
	static BOOL UserInitialized = FALSE;

	typedef  struct _PAL_LIST {
		char *name;
		int minlen;
		GVP_CONTINUUMPALETTE palette;
	} PAL_LIST;

	PAL_LIST *citem;
	static PAL_LIST palette_list[] = {
		{"-?",   2, (GVP_CONTINUUMPALETTE) -10},	{"-help", -2, (GVP_CONTINUUMPALETTE) -10},
		{"?",    1, (GVP_CONTINUUMPALETTE) -10},	{"help",   4, (GVP_CONTINUUMPALETTE) -10},
		{"default", 3, GV_PAL_DEFAULT}, 
		{"RANDOM", 3, GV_PAL_RANDOM},
		{"PSYCHO", 6, GV_PAL_RANDOM},

		{"GREY", 4, GV_PAL_GREY},
		{"HOT",  3, GV_PAL_HOT},
		{"COLD", 4, GV_PAL_COLD}, 
		{"HOT2", 4, GV_PAL_HOT2},
		{"HEAT",	4,	GV_PAL_HEAT},
		{"COOL",	4, GV_PAL_COOL},
		{"JET",	3,	GV_PAL_JET},
		{"HSV",	3, GV_PAL_HSV},

		{"AFM",  3, GV_PAL_AFM},
		{"GOLD", 4, GV_PAL_GOLD},
		{"COPPER", 3, GV_PAL_COPPER},
		{"BONE",   4, GV_PAL_BONE},
		{"PINK",   4, GV_PAL_PINK},
		{"PRISM",  5, GV_PAL_PRISM},

		{"SPRING", 3, GV_PAL_SPRING},
		{"SUMMER", 3, GV_PAL_SUMMER},
		{"AUTUMN", 3, GV_PAL_AUTUMN},
		{"WINTER", 3, GV_PAL_WINTER},

		{"MOT",  3, GV_PAL_MOT},
		{"USER", 4, GV_PAL_USER},
		{NULL,	0, GV_PAL_DEFAULT} };

/* If no token is given, query ourselves */
	if (token == NULL) {
		if (! LexGetTokenP(mytok, sizeof(mytok), "Continuum paleete choice (-? gives list): ")) return(dflt);
		token = mytok;
	}
				
	rc = dflt;
	if ( (citem = LexCmdl(token, palette_list, sizeof(PAL_LIST))) == NULL) {
		ERRprintf("ERROR: %s not recognized as a continuum palette choice.  Use -? for list\n", token);
		rc = GV_PAL_ERROR;
	} else if (citem->palette == (GVP_CONTINUUMPALETTE) -10) {
		LexCmdlPrint(palette_list, sizeof(PAL_LIST), "Continuum palette choices:");
	} else {
		rc = citem->palette;
		if (rc == GV_PAL_DEFAULT) rc = dflt;		/* Reset again */
		if (! UserInitialized) {
			int i,j;
			UserInitialized = TRUE;
			for (i=0; i<LOOKUP_PALETTE_SIZE; i++) {
				j = (int) (256.0*i/LOOKUP_PALETTE_SIZE);
				UserLookupPalette[i] = MY_RGB(j,j,j);
			}
			GVLinkIntArray("$RAINBOW", GVF_NODELETE | GVF_INTERNAL, UserLookupPalette, LOOKUP_PALETTE_SIZE, NULL);
		}
	}
	return(rc);
}

/* =============================================================================
-- Routine to return the RGB code corresponding to a continuum rainbow mapping
--
-- Usage:  int GVSelectContinuumColor(double x, double xlow, double xhigh, 
--                                    GVP_CONTINUUMPALETTE palette);
--
-- Inputs: x    - value to code
--         xmin - value at lower end of color scale
--         xmax - value at upper end of color scale
--         palette - palette choice (-1 ==> default)
--
-- Output: none
--
-- Return: integer with encoded RGB value
--
-- Note: (1) Uses color mapping based on value set by PALETTE command through
--           the exported GVP_CONTINUUMPALETTE enumeration, unless other specified.
--       (2) Range is tested so colors are always valid within range
============================================================================= */
EXPORT GVP_CONTINUUMPALETTE GVContinuumPalette=GV_PAL_HEAT;

int GVSelectContinuumColor(double x, double xlow, double xhigh, GVP_CONTINUUMPALETTE palette) {

	double red, green, blue;
	int i;

	if (xhigh == xlow) xhigh = xlow+1.0;
	x = (x-xlow)/(xhigh-xlow);

	if (x < 0.0) x = 0.0;						/* Limit x to valid range */
	if (x > 1.0) x = 1.0;

	if (palette == -1 || palette == GV_PAL_DEFAULT) palette = GVContinuumPalette;
		
	switch (palette) {
		case GV_PAL_RANDOM:
		{
			static double *r=NULL,*g=NULL,*b=NULL;
			if (r == NULL) {
				r = calloc(1024,sizeof(*r));
				g = calloc(1024,sizeof(*g));
				b = calloc(1024,sizeof(*b));
				for (i=0; i<1024; i++) {
					r[i] = 0.1+0.8*rand()/(1.0*RAND_MAX);
					g[i] = 0.1+0.8*rand()/(1.0*RAND_MAX);
					b[i] = 0.1+0.8*rand()/(1.0*RAND_MAX);
				}
				r[0] = g[0] = b[0] = 0.0;
				r[1023] = g[1023] = b[1023] = 1.0;
			}
			i = (int) (x*1024+0.5);
			red   = r[i];
			green = g[i];
			blue  = b[i];
		}
			break;
		case GV_PAL_GREY:
			red = green = blue = x;
			break;
		case GV_PAL_MOT: 
			red    = 0.6 * ( (x<0.33) ? (3*x) : 3*(1-x)/2 );
			green  = 0.8 * ( (x<0.33) ? 0 : (x<0.67) ? (x-0.33)/0.34 : 1-(x-0.67)/0.33 );
			blue   = (x<0.50) ? 0 : 2*x-1;
			break;
		case GV_PAL_AFM:
			red   = sqrt(x);
			green = (x<0.33) ? 0 : (x-0.33)/0.67 ;
			blue  = (x<0.75) ? 0 : (x-0.75)/0.25 ;
			break;
		case GV_PAL_HOT:
			blue  = (x<0.25) ? 1   : (x<0.5)  ? (2-4*x) : 0 ;
			green = (x<0.25) ? 4*x : (x<0.75) ? 1 : (4-4*x) ;
			x = 1.0-x;
			red  = (x<0.25) ? 1   : (x<0.5)  ? (2-4*x) : 0 ;
			break;
		case GV_PAL_HOT2:
			x = x*16;												/* Divide into 16th */
			blue  = (x<2) ? (0.5+x/4) : (x<5) ? 1       : (x<8)  ? 1-(x-5)/3 : 0 ;
			green = (x<2) ? 0         : (x<5) ? (x-2)/3 : (x<11) ? 1 : (x<14) ? (14-x)/3 : 0 ;
			x = 16-x;
			red   = (x<2) ? (0.5+x/4) : (x<5) ? 1       : (x<8)  ? 1-(x-5)/3 : 0 ;
			break;
		case GV_PAL_COLD:
			red   = (x<0.25) ? 1 : (x<0.5) ? (2-4*x) : 0 ;
			green = (x<0.25) ? 4*x : (x<0.75) ? 1 : (4-4*x) ;
			x = 1.0-x;
			blue  = (x<0.25) ? 1 : (x<0.5) ? (2-4*x) : 0 ;
			break;
		case GV_PAL_GOLD:
			red   = x ;
			green = 215.0*x/255.0 ;
			blue  = 0 ;
			break;
		case GV_PAL_JET:
			red   = (x<0.375) ? 0 : (x<0.625) ? 4*(x-0.375) : (x<0.875) ? 1 : 4*(1.125-x) ;
			green = (x<0.125) ? 0 : (x<0.375) ? 4*(x-0.125) : (x<0.625) ? 1.0 : (x<0.875) ? 4*(0.875-x) : 0 ;
			blue  = (x<0.125) ? 0.5+4*x : (x<0.375) ? 1 : (x<0.625) ? 4*(0.625-x) : 0 ;
			break;
		case GV_PAL_HSV:
			red   = (x<0.1875) ? 1 : (x<0.375) ? 2-x/0.1875 : (x<0.65) ? 0 : (x<0.8375) ? (x-0.65)/0.1875 : 1 ;
			green = (x<0.1875) ? x/0.1875 : (x<0.5) ? 1.0 : (x<0.6875) ? (0.6875-x)/0.1875 : 0 ;
			blue  = (x<0.25)   ? 0 : (x<0.4375) ? (x-0.25)/0.1875 : (x<0.875) ? 1.0 : 1-(x-0.875)/0.1875 ;
			break;
		case GV_PAL_COOL:
			red   = x;
			green = 1-x;
			blue  = 1;
			break;
		case GV_PAL_HEAT:
			red   = (x<0.375) ? x/0.375 : 1 ;
			green = (x<0.375) ? 0 : (x<0.75) ? x/0.375-1 : 1 ;
			blue  = (x<0.75)  ? 0 : 4*(x-0.75) ;
			break;
		case GV_PAL_SPRING:
			red   = 1;
			green = x;
			blue  = 1-x;
			break;
		case GV_PAL_SUMMER:
			red   = x;
			green = 0.5+0.5*x;
			blue  = 0.4;
			break;
		case GV_PAL_AUTUMN:
			red   = 1.0;
			green = x;
			blue  = 0;
			break;
		case GV_PAL_WINTER:
			red   = 0;
			green = 1-0.5*x;
			blue  = x;
			break;
		case GV_PAL_COPPER:
			red   = (x<0.8) ? x/0.8 : 1 ;
			green = 0.78*x;
			blue  = 0.5*x;
			break;
		case GV_PAL_BONE:
			red   = 0.86*x + ((x>0.75) ? 0.14*(x-0.75)/0.25 : 0) ;
			green = (x<0.28) ? 0.86*x : (x<0.78) ? 0.86*x+0.14*(x-0.28)/(0.78-0.28) : 1-0.86*(1-x) ;
			blue  = 1.2*x  - ((x>0.375) ? 0.20*(x-0.375)/0.625 : 0) ;
			break;
		case GV_PAL_PINK:
			red   = min(pow(x,0.25), 1-0.385*(1-x));
			green = min(max(0.85*pow(x,1/2.06),1.2*x-0.021), 1-0.385*(1-x));
			blue  = max(0.85*pow(x,1/2.06), 1-1.2*(1-x));
			break;
		case GV_PAL_PRISM:
			i = (int) (x*1024+0.5);									/* Nearest integer */
			red   = ((i+8)%24 > 8) ? 1 : ((i+8)%24 ==  8) ? 0.5 : 0 ;
			green = ((i+4)%24 >12) ? 1 : ((i+4)%24 == 12) ? 0.5 : 0 ;
			blue =  ((i-4)%24 >12) ? 1 : 0 ;
			break;
		case GV_PAL_USER:
			i = (int) (x*(LOOKUP_PALETTE_SIZE-0.01));
			return UserLookupPalette[i];
		default:															/* If I blow it - just do greyscale */
			red = green = blue = x;
			break;
	}
	return MY_RGB(nint(red*255), nint(green*255), nint(blue*255));
}

/* =============================================================================
-- Routine to return ln(gamma(x)) over some phenonemal range!
--
-- Usage:  call	r_gamma
--
-- Inputs: ST(0) - X in ln(gamma(x))
--
-- Output: ST(0) <-- ln(gamma(ST(0)))
--
-- Although this code recursively will handle negative arguments, it is
-- never called except with x > 0.  For negative arguments, always returns
-- ln(|gamma(x)|) via recursive calls.
--
--
-- To deal with negative real parts to the arguments, use
--
-- For any integer n,
--
--                    n  gamma(z+n) gamma[1-(z-n)]
--     gamma(z) = (-1)   -------------------------
--                               g(1-z)
--
-- Choose for x<0, n = -floor(x)
--      x+n   = u = mod(|x|,1)
--      1-x-n = 1-u
--
-- Now, all z's in gamma(z) above have positive real arguments
============================================================================= */
#ifndef HAS_GAMMA

PRIVATE TMPREAL r_gamma(TMPREAL x) {

	TMPREAL t,result, frac;
	int i,imax=5;
	static double coef[] = {+12, -360, +1260, -1680, +1188};

/* ---------------------------------------------------------------------------
-- Handle low value integer cases first, and then for others make sure
-- that asymptotic expression okay via |z| > 10
--------------------------------------------------------------------------- */
	frac = x-FLOOR(x);										/* Fractional part */
	if (x <= 0) {												/* Could be bad! */
		if (frac == 0.0) {									/* Really +-INF */
			result = TMPREAL_MAX;
		} else {
			result = r_gamma(1-frac) + r_gamma(frac) - r_gamma(1-x);
		}
		return(result);
	} else if (x <= 20 && frac == 0.0) {
		result = 1;
		while (x > 2) {x -= 1; result *= x;}
		return(LOG(result));
	} else if (x < 10) {
		result = 1;
		while (x < 10) {result /= x; x += 1;}
		result = LOG(result);
	} else {
		result = 0;
	}

	if (x > 100)  imax--;			/* Drop one term  if |z| > 100  */
	if (x > 1000) imax--;			/* Drop two terms if |z| > 1000 */

	t = 1/(x*x);								/* 1/x**2 */

	result += (x-.5) * LOG(x) - x + LOG(2*PI)/2 ;
	for (i=0; i<imax; i++) {
		x = x*t;
		result += x / coef[i];
	}

	return(result);
}

#endif /* ! HAS_GAMMA */


/* =============================================================================
-- Usage: call	near ptr gamma
--
-- Inputs: ST(0) - argument of gamma function
--
-- Output: ST(0) - value of GAMMA(ST)
--
-- Stack utilization at max is 3 elements.
============================================================================ */
#if 0														/* Not currently used */
PRIVATE TMPREAL norm_factorial(TMPREAL x);
PRIVATE TMPREAL norm_factorial(TMPREAL x) {
	return norm_gamma(x+1.0);
}
#endif

PRIVATE TMPREAL norm_gamma(TMPREAL x) {
	
/*--------------------------------------------------------------
; ... (1) Find integer part of X and test against argument
; ... (2) Check range so limit to -170 to +170
; ... (3) Compress argument to range [1,2]
; ... (4) If exact, done.  Otherwise, evaluate the gamma also.
;-------------------------------------------------------------- */

#ifdef OLD_VALUES_BEFORE_OPTIMIZING_FURTHER

	static double t[] = {-164.9051819,		/* 1		*/
								-0.8161632,			/* x		*/
							  -33.1495056,			/* x**2	*/
								 2.337796,			/* x**3	*/
								-1.8894378};		/* x**4	*/
	static double b[] = {-164.7453461,		/* 1		*/
								-97.1084671,		/* x		*/
								 77.9550629,		/* x**2	*/
								-15.5237560};		/* x**3	*/

     define f(x) = poly(x,p)/(x*poly(x,q))
     alloc p array 5 alloc q array 5
#endif

/* Max deviation < 2E-10 */
	static double t[] = {-164.9051877737037,		/* 1		*/
								-0.8161847731759783,		/* x		*/
							   -33.14949909606396,		/* x**2	*/
								 2.337802896609842,		/* x**3	*/
								-1.889439495817852};		/* x**4	*/
	static double b[] = {-164.7453525531265,		/* 1		*/
								-97.10846129996624,		/* x		*/
								 77.95506274169051,		/* x**2	*/
								-15.52375713074555};		/* x**3	*/

	TMPREAL tmp=1;
	int i;
	
	if (x < -170) {
		return(0);
	} else if (x > 170) {
		return(TMPREAL_MAX);
	}

	if (x >= 1) {
		while (x > 2) tmp *= --x;					/* gamma(x) = (x-1)*gamma(x-1) */
		if (x == 1.0 || x == 2.0) return tmp;	/* Exact value */
		if (x == 1.5) return tmp*SQRT(PI)/2.0;	/* Exact values */
	} else if (x < 1) {
		i = (int) x;
		if (x == (TMPREAL) i) return(TMPREAL_MAX);
		while (x < 1) tmp /= x++;					/* gamma(x) = gamma(x+1)/x	*/
		if (x == 1.5) return tmp*SQRT(PI)/2.0;	/* Exact values */
	}

	tmp  = tmp *  ((((x*t[4]+t[3])*x+t[2])*x+t[1])*x+t[0]) / 
		          (((((x+b[3])*x+b[2])*x+b[1])*x+b[0])*x);
	tmp += 1.535871e-011;							/* Gets exact for gamma(1.5) */
	return(tmp);
}

/* ============================================================================
--   Function to evaluate the error function
--
--   Usage: real = ERF_S(key,X)
--
--   Inputs: X - Input argument of the error function.
--				 KEY - which to return  1 => ERF
--												2 => ERFC
--												3 => NDTR (normal distribution value)
--														    = erfc[-x/sqrt(2)]/2.0
--
--   Output: ERF_S - Output value of the error or complementary function.
--
-- Comments on NDTR:
--
-- Computes Y = P(X) = probability that the random variable U, distributed
-- normally on (0,1), is less than or equal to X.  
--
-- NOTE: NDTR(X) = {1+erf[X/sqrt(2)]} / 2.0
-- ========================================================================= */
#define ERF_XMIN		1.0E-5								/* Low   X range starts */
#define ERF_XLARGE	4.1875								/* Large X range starts */
#define SQRPI			0.56418958354775628690			/* 1/sqrt(pi)	*/
#define SQRT2I			0.70710678118654752445			/* 1/sqrt(2)	*/

#if (defined i386 || defined i486)
	#define ERFC_MAX		27.226
#else
	#define ERFC_MAX		15.065574224
#endif

TMPREAL ERF_S(int key, TMPREAL x) {
	
	TMPREAL xerf, xerfc, xsq, xi;
	int flags=0;
#define	B_NEG			0x01						/* Argument was negative	 */
#define	B_ERFC		0x02						/* xerfc is valid, not xerf */
#define	B_UNNORM		0x04						/* Needs the e^{x^2}/2 mult */

/* Coefficients for 0.0 <= Y < .477 */
	static double p[] = {1.128379167615, 0.0908744014861, 0.01670667572898};
   static double q[] = {1.0,            0.413868661356,  0.05276291052144};
/* Coefficients for .477 <= Y <= 4.0 */
	static double p1[] = {0.99999011363, 0.93099282488, 0.41114638022, 0.076369325737, 5.841349927e-06};
	static double q1[] = {1.0,           2.0592722535, 1.7352416832, 0.7260948247, 0.13560588011};
/* Coefficients for correction above 4.0-8.0 */
	static double r0[] = {-4.147022992e-008, 6.577948625e-006, -0.0004594082129, 0.01821908914, -0.4827995598, -9.578360558, 16.48487473};
	static double r1[] = {-1.701077849e-006, 8.30114368e-005, -0.001881676726, 0.0252633635, -0.2128335088, 1.102018595, -3.16755867, 3.729088545};

#ifdef HAS_ERFC
	if (key == 1) return(ERF(x));
	if (key == 2) return(ERFC(x));
	if (key == 3) return(NDTR(x));
#endif /* HAS_ERFC */

/* ... If NDTR, must cheat a little */
	if (key == 3) x = -x*SQRT2I;			/* If NDTR, argument = -x/sqrt(2) */

	if (x < 0.0) {								/* Work only with positive quantities */
		flags |= B_NEG;
		x = -x;
	}
	xsq = x*x;

/* ... Very small, just use simplest approximation */
	if (x < ERF_XMIN) {
		xerf = x*p[0];							/* Next term is x^3, so ignore */

/* ... ABS(Y) <= .477, evaluate approximation for erf */
	} else if (x < 0.477) {
		xerf = x * rpoly_e(xsq, p,2, q,2) ;

/* ... .477 <= ABS(Y) <= 4.0  --- Accuracy worst - 7E-10 maximum deviation */
	} else if (x < 4.0) {
		flags |= B_ERFC;
		xerfc = EXP(-xsq) * rpoly_e(x,p1,4,q1,4);	/* Big polynomial fit */

/* ... 4.0 < y < infty, asymptotic series expansion for ERFC */
/* ... Accurate to 1E-10 in the unnormalized value */
	} else {
		flags |= B_ERFC | B_UNNORM;
		xi   = 1.0/xsq;									/* X inverse */
		xerfc = SQRPI * (1.0+xi*(-0.5+xi*(0.75+xi*(-1.875+6.625*xi))));
		if (x < 8) {
		   xerfc += poly_e(xi, r0, 6);
		} else {
		   xerfc += poly_e(1/x, r1, 7);
		}
/*		if (xi > 0.0184258) xerfc -= 0.1407615*pow(xi-0.0184258,3);	*/	/* Empirical correction */
	}

/* Handle ln(erfc) separately */
	if (key == 4) {
		if (! (flags & B_ERFC)) {								/* Two possibilities */
			if (flags & B_NEG) xerf = -xerf;					/* Get xerf				*/
			if (x < 1E-3) {										/* Do ln(1-erf) self */
				xerfc = -xerf*(1+xerf*(1/2.0+xerf*(1/3.0+xerf*(1/4.0+xerf/5.0))));
			} else {
				xerfc = LOG(1.0-xerf);
			}
		} else if (! (flags & B_UNNORM)) {					/* Simple erfc */
			if (flags & B_NEG) xerfc = 2.0-xerfc;
			xerfc = LOG(xerfc);
		} else if (flags & B_NEG) {							/* Large negative args */
			xerfc = (x > 7) ? 0 : xerfc*EXP(-xsq)/x;
			xerfc = LOG(2.0-xerfc);
		} else {														/* Use expansion */
			xerfc = LOG(xerfc) - xsq - LOG(x);
		}
		return(xerfc);
	}


/* Correct for normalization , deal with erf/erfc correct */
	if (x > ERFC_MAX) {									/* Out of range */
		xerfc = 0.0;
		xerf  = 1.0;
	} else if (flags & B_UNNORM) {					/* Have erfc w/out e^x^2/x */
		xerfc *= EXP(-xsq)/x;
		xerf  = 1.0-xerfc;
	} else if (flags & B_ERFC) {						/* Have only xerfc */
		xerf  = 1.0-xerfc;
	} else {
		xerfc = 1.0-xerf;
	}
	if (flags & B_NEG) {xerf = -xerf; xerfc = 2.0-xerfc;}

	if (key == 3) return(xerfc/2);				/* NDTR(x)	*/
	if (key == 2) return(xerfc);					/* ERFC(x)	*/
	return(xerf);										/* ERF(x)	*/
}


/* ===========================================================================
-- REAL FUNCTION NDTRI
--
-- Computes X = P^(-1)(Y), the argument X such that Y= P(X) = probability that
-- the random variable U, distributed normally (0,1), is equal or less than X.
--          
-- Usage:  real = NDTRI(p)
--
-- Inputs: p - input probability
--
-- Output: NDTRI - Inverse normal probability
--
-- Note: 1. Maximum error is 1.5E-6 w/ relative error <1E-6 in [-6,6]
--	2. If P<=0, X set to -9.1553         (note error for <0)
--	3. If P>=1, X set to +9.1553         (note error for >0)
--       4. Based on approximations in C. Hastings, Approximations for Digital
--          Computers, Princeton Univ. Press, Princeton, N.J., 1955.  See
--          Equation 26.2.23, Handbook of Mathematical Functions, Abramowitz
--          and Stegun, Dover Publications, Inc., New York.
--      4a. Most now my own terms.  Correctors resolve answers.
--       5. The corrector in inner section corrects answers to be valid to
--          within 2x10^-6 absolute and relative over the interval [-0.05,-5.5]
--	         and [0.05,2.5].  The absolute error is <1E-6 on [-0.05,0.05].
--       6. Added corrections to get within 3E-7 over range -2.5 2.5, and below
--          4E-6 over full range -38 +9 (+ range limited by roundoff to 1.0).
-- ========================================================================= */
TMPREAL NDTRI(TMPREAL p) {
	
#define LIMIT_ARG_VAL	38.47465;

	static double num[]   = {2.515517,		  0.802853,			0.010328};
	static double denom[] = {1.0,				  1.432788,			0.189269,		 0.001308};

	static double r2[] = {0.00029859715701, 0.00046690101221, -0.00041094492583, 1.4836042132e-005, 9.0445398334e-005,
								 -5.9417052852e-005, 2.4083658236e-005, -7.7312843132e-006, 1.9257277017e-006};
	static double r3[] = {0.00041854218865, -0.00014162624904, -0.00017261376736, 8.1839729896e-005, -1.2808004467e-005,
							   -2.3016012831e-006, 2.164855319e-006, -8.930115E-007, 3.281998E-007};
	static double r5[]  = {2.6521271801e-005, -0.00028301695906, 2.7742027695e-005, 1.5858032378e-005, -6.1300664207e-006,
								  1.2089310091e-006, -1.3779458613e-007,  -7.8864e-009, 7.9375e-009};
	static double r10[] = {-0.00033499183423, 0.0003643588597, 0.00016385965131, -0.00020531893775, 9.8624665016e-005, -2.7121319943e-005, 
								  -5.944520413e-007, 3.709582797e-006, -2.2303484285e-006, 4.2741e-006, -2.7178e-006};
	static double r25[] = {0.0004295823574, 9.4136771678e-005, -0.00019633197782, 7.2794900177e-005, -1.2725426308e-005,
								  -2.1193301912e-006, 3.0003340843e-006, -1.6890972433e-006, 9.0911607484e-007, -4.5248e-007, 1.1065e-007};
            
	static double taylor[] = {-4.5935005102e-010, 0.16666665975, 0.058333524176, 0.025197330986, 0.012041674594,
									   0.0060996947887, 0.0032662097955, 0.0014721469231, 0.0017529695707, -0.00033785703859};
	static double ctay[] = {6.4439238424e-009, -2.6928790649e-007, 3.7038273173e-006, -2.339886523e-005, 6.9000506134e-005, -2.0661086273e-005, 2.1380683449e-006};
	TMPREAL t;

	t = (p <= 0.5) ? p : 1.0-p;		/* Work with values on [0,0.5] */

	if (t <= 0) {							/* Lower/Upper bounds */
		t = LIMIT_ARG_VAL;
	} else if (t > 0.15) {										/* Near origin - get it right */
		t = (0.5-t)*2.50662827463100004;						/* Taylor expansion */
		t += t*poly_e(t*t,taylor,10);
		if (t > 0.40) {											/* Final corrections */
			t += poly_e(t*t*t*t, ctay, 6);					/* To rediculous precision */
		} else {
			t -= 1.15E-012*t;
		}
	} else {
		t  =  SQRT(-2.0*LOG(t));		/* LOG(1.0/(t*t)) */
		t  -= rpoly_e(t,num,2,denom,3);
		if (t < 2) {
			t -= poly_e(t-1.5, r2, 8);
		} else if (t < 3) {
			t -= poly_e(t-2.5, r3, 8);
		} else if (t < 5) {
			t -= poly_e(t-4.0, r5, 8);
		} else if (t < 14.75) {
			t -= poly_e((t-10.0)/5.0, r10, 10);
		} else {
			t -= poly_e((t-25.0)/10.0, r25, 10);
		}
	}

	return( p>0.5 ? t : -t);
}

/* ============================================================================
-- Usage: real = spl_eval(x,c,mode)
--
-- Inputs: X    - coordinate to evaluate spline at
--         C    - array from above w/ (5,NPT) values
--         MODE - 0 => evaluate function
--                1 => evaluate 1st derivative
--                2 => evaluate 2nd derivative
--
-- Output: spl_eval - result of spline fit
--         s(x) = ((c(i,3)*d+c(i,2))*d+c(i,1))*d+y(i) 
--                where x(i) .le. t .lt. x(i+1) and d = x-x(i).
-- ========================================================================= */
PRIVATE TMPREAL spl_eval(TMPREAL x, ARRAY *arrayptr, int key) {
	
	typedef struct _spline_element {
		REAL c[3];								/* Coefficients of the spline */
		REAL x,y;								/* x,y values at the starting knot */
	} SPLINE;

	TMPREAL d;
	SPLINE *spl;
	INT npt;
	static INT i=0;

	npt = (*arrayptr->size)/5;				/* Determine number of points */
	spl = (SPLINE *) arrayptr->x;
	
	if (i < 0) i = 0;							/* Make sure i is in range */
	if (i >= npt-1) i = npt-2;				/* Both ways */

	while (i != 0     && x < spl[i].x)   i--;
	while (i != npt-2 && x > spl[i+1].x) i++;

	d = x - spl[i].x;

	if (key == 0) {
		d = ((spl[i].c[2]*d+spl[i].c[1])*d+spl[i].c[0])*d + spl[i].y;
	} else if (key == 1) {
		d = (3*spl[i].c[2]*d+2*spl[i].c[1])*d+spl[i].c[0];
	} else {
		d = 6*spl[i].c[2]*d + 2*spl[i].c[1];
	}

	return(d);
}


/* ============================================================================
-- Usage: real = ispl_eval(xlow, xhigh, c)
--
-- Inputs: XLOW  - Lower limit
--         XHIGH - Upper limit
--
-- Output: spl_eval - integral of cubic spline over specified limits
--         s(x) = ((c(i,3)*d+c(i,2))*d+c(i,1))*d+y(i) 
--                where x(i) .le. t .lt. x(i+1) and d = x-x(i).
-- ========================================================================= */
PRIVATE TMPREAL ispl_eval(TMPREAL xlow, TMPREAL xhigh, ARRAY *arrayptr) {
	
	typedef struct _spline_element {
		REAL cf[3];								/* Coefficients of the spline */
		REAL x,y;								/* x,y values at the starting knot */
	} SPLINE;

	SPLINE *spl;
	BOOL invert;
	TMPREAL x1, x2, integral;
	int npt;

/* Check for reversed arguments */
	if ( (invert = (xhigh < xlow)) ) {
		x1 = xlow;
		xlow  = xhigh;
		xhigh = x1;
	}

/* Locate which segment each point lies within */
	npt = (*arrayptr->size)/5;				/* Determine number of points */
	spl = (SPLINE *) arrayptr->x;			/* Can blowup if SPL not terminated REAL_MAX */
	while (xlow > spl[1].x && npt>1) spl++,npt--;

/* Now, just sum across elements */
	for (integral=0.0; xlow<xhigh && npt>0; spl++,npt--) {
		x1 = xlow - spl->x;
		x2 = (npt == 1 || (xhigh < spl[1].x) ? xhigh : spl[1].x) - spl->x;
		integral += spl->cf[2]*(pow(x2,4)-pow(x1,4))/4.0 + 
						spl->cf[1]*(pow(x2,3)-pow(x1,3))/3.0 +
						spl->cf[0]*(x2*x2-x1*x1)        /2.0 +
						spl->y    *(x2-x1);
		if (npt > 1) xlow = spl[1].x;
	}

	if (invert) integral = -integral;
	return(integral);
}


/* ----------------------------------------------------------------------------
-- Usage: REAL = POLY_E(X,ARRAY,I1)            - Polynomial evaluation
--	 REAL = RPOLY_E(X,NUMER,I1,DENOM,I2)  - Rational polynomial evaluation
--
-- Input: X           - argument
--        ARRAY(0:I1) - coefficients of polynomial
--        NUMER(0:I1) - coefficients for numerator
--        DENOM(0:I2) - coefficients for denominator
--        I1,I2       - maximum coefficient
--
-- Output: POLY_E = A(0) + A(1)*X + A(2)*X*X + A(3)*X*X*X + ...
--
--                  N(0) + N(1)*X + N(2)*X*X + N(3)*X*X*X + ...
--        RPOLY_E = -------------------------------------------
--                  D(0) + D(1)*X + D(2)*X*X + D(3)*X*X*X + ...
---------------------------------------------------------------------------- */
PRIVATE TMPREAL poly_e(TMPREAL x, double *numer, int iorder) {
	
	TMPREAL tmp;
	numer += iorder;								/* Go to end of the array */
	tmp = *numer;
	while (iorder--) tmp = tmp*x + *(--numer);
	return(tmp);
}

PRIVATE TMPREAL rpoly_e(TMPREAL x, double *numer, int i1, double *denom, int i2) {

	TMPREAL top, bot;
	
	numer += i1;
	denom += i2;
	top = *numer;
	bot = *denom;
	while (i1--) top = top*x + *(--numer);
	while (i2--) bot = bot*x + *(--denom);
	return(top/bot);
}

PRIVATE TMPREAL chebyshev(TMPREAL x, REAL *coef, int order) {

	int i;
	TMPREAL d1=0, d2=0, tmp;
	
	for (i=order-1; i>0; i--) {
		tmp = d1;										/* Save so becomes d2 next	*/
		d1 = 2*x*d1 - d2 + coef[i];				/* Clenshaw's recurrence	*/
		d2 = tmp;
	}
	d1 = x*d1 - d2 + coef[0]/2 ;					/* Final step					*/
	return(d1);
}

/* ---------------------------------------------------------------------------
-- Routine to locate position of a value in an array.  Returns the index of the
-- first time in the array that the value is equal to or greater than the given
-- value.  -1 is returned if no values are >= to value specified.
--
-- Usage:  int = gv_find_in_array(ARRAY *arrayptr, TMPREAL value);
--
-- Inputs: arrayptr - pointer to an array structure
--         value    - value to search for
--
-- Output: none
--
-- Returns: index if found, -1 if no array element is >= given value
---------------------------------------------------------------------------- */
PRIVATE int gv_find_in_array(ARRAY *arrayptr, TMPREAL value) {
	
	int i;

	for (i=0; i<(*(arrayptr->size)); i++) {
		if (arrayptr->x[i] >= value) return(i);
	}
	return(-1);
}

/* ---------------------------------------------------------------------------
-- Routine to evaluate the array functions on expressions.  Called from GVPARSE
-- when seeing an @fnc type function.  Handles efficiently the case of a simple
-- array, but has flexibility to handle full expression as well
--
-- Usage:  TMPREAL = gv_eval_array_exp(unsigned todo, TMPREAL *rval, GVCMDS *mycmd, int length);
--
-- Inputs: mycmd - command to be evaluated (one of @SUM, @MIN, @MAX ...);
--         arrayptr - pointer to an array structure
--
-- Output: *rval - set to value of the expression evaluation
--
-- Returns: 0 if successful, +1 if any type of error (FATAL)
---------------------------------------------------------------------------- */
EXTERN int gv_eval_array_exp(unsigned int todo, TMPREAL *rval, 
									  GVCMDS *cmds, int length) {

	int i, errcnt=0, err;
	ARRAY *array;

	TMPREAL x, sumx=0,sumxx=0;							/* Sums of X and of X^2		*/
	TMPREAL xmin=0, xmax=0;								/* Min/max values				*/
	int	  imin=0, imax=0;								/* Index of min/max values	*/


/* First, see if it is just an array load and nothing else */
	if ( *cmds==load_real_array && cmds[1+sizeof(ARRAY *)]==0xFF ) {
		cmds++;														/* Skip the load array		*/
		GVGETITEM(&array, cmds, ARRAY *);					/* Get the array reference	*/
		*rval = gv_eval_array_fnc(todo, array, 0, INT_MAX);	/* Evaluate for the array	*/
		return(0);													/* And return successful	*/
	}

/* If not, go through manually for the number of elements in expression */
/* REMEMBER: The cmds stack normally first has a byte count which I ignore */
	cmds--;													/* Back up one, fake count */
	for (i=0; i<length; i++) {
		GVLocalIndex = i;
		x = (gv_eval_cmds(cmds, &err)).x;
		if (err != 0 && errcnt++ > 10) {				/* * Give up at this point */
			ERRputs("FATAL: Parsing of @fnc expression fails for too many errors\n");
			return(1);
		}
		if (i == 0) xmin = xmax = x;						/* Need to handle */
		sumx  += x;
		sumxx += x*x;
		if (x < xmin) {xmin = x; imin = i;}
		if (x > xmax) {xmax = x; imax = i;}
	}
	
	*rval = 0.0;												/* Default return value */
	switch (todo) {
		case array_min:
			GVMaxIndex = imin;
			*rval = xmin;
			break;
		case array_max:
			GVMaxIndex = imax;
			*rval = xmax;
			break;
		case array_sum:
			*rval = sumx;
			break;
		case array_avg:
			if (length > 0) *rval = sumx/length;
			break;
		case array_var:
		case array_std:
		case array_rms:
		case array_sdom:
			if (length > 1) *rval = (sumxx-sumx*sumx/length) / (length-1) ;
			if (todo != array_var)  *rval = SQRT(*rval);
			if (todo == array_sdom) *rval = *rval/SQRT(length);
			break;
		case array_absmin:
		case array_absmax:
		case array_abssum:
		case array_absavg:
		case array_span:
		case array_median:
		case array_mad:
		case array_skew:
		case array_kurt:
			printf("Not implemented yet\n");
			break;
		default:
			panic;
	}
	return(0);
}


/* ---------------------------------------------------------------------------
-- Routine to evaluate the array functions on simple arrays.  Can be called
-- either from this code or from GVPARSE
--
-- Usage:  TMPREAL = eval_array_fnc(unsigned char todo, ARRAY *arrayptr);
--
-- Inputs: todo - command to be evaluated (one of @SUM, @MIN, @MAX ...);
--         arrayptr - pointer to an array structure
--
-- Output: none
--
-- Returns: value of function evaluation
---------------------------------------------------------------------------- */
static TMPREAL gv_eval_buf(unsigned int todo, REAL *x, int len);

TMPREAL gv_eval_array_fnc(unsigned int todo, ARRAY *arrayptr, int imin, int imax) {

	int i, len;
	REAL *x;

	len = *(arrayptr->size);
	x = arrayptr->x;

/* Limit the range to valid extents */
	if (len != 0) {
		if (imin < 0)     imin = 0;								/* Limit range for checking */
		if (imax < 0)     imax = 0;
		if (imin > len-1) imin = len-1;
		if (imax > len-1) imax = len-1;
		if (imin > imax) { i = imin; imin = imax; imax = i; }
		x = x+imin;														/* Shift offset to starting point */
		len = imax-imin+1;											/* Redefine the length				 */
	}

	return gv_eval_buf(todo, x, len);
}
	

static TMPREAL gv_eval_buf(unsigned int todo, REAL *x, int len) {

	int i;
	REAL *xt, low, high;
	TMPREAL tmp;
	TMPREAL avg, std, var;

/* If there isn't any data in the array, just return zero - not garbage */
	if (len <= 0) {
		if (todo == array_min || todo == array_max) GVMaxIndex = 0;
		return(0.0);									/* Return valid even if no data */
	}

/* Otherwise, everything is setup, just do */
	switch (todo) {
		case array_count:
			tmp = len;
			break;
			
		case array_min:
			GVMaxIndex = 0;
			low = x[0];
			for (i=0; i<len; i++) {
				if (x[i] < low) {low = x[i]; GVMaxIndex = i;}
			}
			tmp = low;
			break;

		case array_max:
			GVMaxIndex = 0;
			high = x[0];
			for (i=0; i<len; i++) {
				if (x[i] > high) {high = x[i]; GVMaxIndex = i;}
			}
			tmp = high;
			break;

		case array_span:
			high = low = x[0];
			for (i=0; i<len; i++) {
				if (x[i] < low)  low = x[i];
				if (x[i] > high) high = x[i];
			}
			tmp = (TMPREAL) (high - low);
			break;
			
		case array_sum:
		case array_avg:
			for (tmp=0,i=0; i<len; i++) tmp += x[i];
			if (todo == array_avg && len > 1) tmp /= len;
			break;

		case array_absmin:
			GVMaxIndex = 0;
			low = (REAL) fabs(x[0]);
			for (i=0; i<len; i++) {
				if (fabs(x[i]) < low) {low = (REAL) fabs(x[i]); GVMaxIndex = i;}
			}
			tmp = low;
			break;

		case array_absmax:
			GVMaxIndex = 0;
			high = (REAL) fabs(x[0]);
			for (i=0; i<len; i++) {
				if (fabs(x[i]) > high) {high = (REAL) fabs(x[i]); GVMaxIndex = i;}
			}
			tmp = high;
			break;

		case array_abssum:
		case array_absavg:
			for (tmp=0,i=0; i<len; i++) tmp += fabs(x[i]);
			if (todo == array_absavg && len > 1) tmp /= len;
			break;

/* ---------------------------------------------------------------------------
-- Algorithm:  Bound median by iterative guess.  At each guess,
--             sum points above/below to update interval boundaries.
--
-- Sort algorithm could possibly be faster in some situations, but generally
-- this in-place locator is about 2x as fast.  Concept from Numerical
-- Recipes section 13.2.
--
-- Returns: n odd  - exact data point.
--          n even - average of upper/lower points at median
--
-- Notes: The Numerical Recipes code has two problems.
--          (1) (minor) AMP at 1.5 is far too small based on
--              my testing - 4.0 used below may still be too
--          (2) The code fails to find the median if too many
--              duplicate points near the median.  This, for
--              the code given, will hang the loop.
--------------------------------------------------------------------------- */
		case array_median:
			tmp = find_median(x, len);
			break;
		case array_mad:
			tmp = find_median(x, len);
			xt = malloc(len*sizeof(*xt));
			for (i=0; i<len; i++) xt[i] = (REAL) (fabs(x[i]-tmp));
			tmp = find_median(xt, len);
			free(xt);
			break;

		case array_var:
		case array_std:
		case array_sdom:
		case array_rms:
		case array_skew:
		case array_kurt:
			tmp = 0.0;
			if (len <= 1) break;									/* Bad conditions */
			if (len <= 2 && todo == array_skew) break;
			if (len <= 3 && todo == array_kurt) break;

			for (avg=var=0,i=0; i<len; i++) {
				avg += x[i];
				var += x[i]*x[i];
			}
			avg   = avg/len;										/* Average	*/
			var   = (var-len*avg*avg) / (len-1);			/* Variance */
			std   = SQRT(var);									/* Standard deviation */

			if (var <= 0) {										/* Shouldn't occur - everything at average */
				tmp = 0.0;
				
			} else if (todo == array_var) {					/* Variance simple */
				tmp = var;

			} else if (todo == array_std  || todo == array_rms || todo == array_sdom) {
				if (std < 0.0001*FABS(avg)) {				/* Redo as direct */
					for (std=0,i=0; i<len; i++) std += (x[i]-avg)*(x[i]-avg);
					std = SQRT(std/(len-1));
				}
				tmp = (todo != array_sdom) ? std : std/SQRT(len) ;		/* Return standard deviation of SDOM */

			} else if (todo == array_skew) {
				for (tmp=0,i=0; i<len; i++) tmp += POW((x[i]-avg)/std,3);
				tmp = tmp*len/(len-1)/(len-2);

			} else if (todo == array_kurt) {
				for (tmp=0,i=0; i<len; i++) tmp += POW((x[i]-avg)/std,4);
				tmp = tmp*(len)*(len+1)/(len-1)/(len-2)/(len-3) - 3.0*(len-1)*(len-1)/(len-2)/(len-3);
			}
			break;

		default:
			panic;
	}
	return(tmp);
}

/* ---------------------------------------------------------------------------
-- Routine to evaluate the array functions on simple arrays.  Can be called
-- either from this code or from GVPARSE
--
-- Usage:  TMPREAL = eval_array_fnc2(unsigned char todo, ARRAY *arrayptr, ARRAY *arrayptr2);
--
-- Inputs: todo - command to be evaluated (one of @SUM, @MIN, @MAX ...);
--         arrayptr - pointer to an array structure
--
-- Output: none
--
-- Returns: value of function evaluation
---------------------------------------------------------------------------- */
TMPREAL gv_eval_array_fnc2(unsigned int todo, ARRAY *arrayptr, ARRAY *arrayptr2, int imin, int imax) {

	int i, len;
	REAL *x, *y;
	TMPREAL tmp, tmp2, tmp3;

	len = *(arrayptr->size);
	x   = arrayptr->x;
	if (*(arrayptr2->size) < len) len = *(arrayptr2->size);			/* Use only overlapping spaces */
	y   = arrayptr2->x;

/* If there isn't any data in the array, just return zero - not garbage */
	if (len <= 0) {
		if (todo == array_min || todo == array_max) GVMaxIndex = 0;
		return(0.0);									/* Return valid even if no data */
	}

/* Limit the range to valid extents */
	if (imin < 0)     imin = 0;								/* Limit range for checking */
	if (imax < 0)     imax = 0;
	if (imin > len-1) imin = len-1;
	if (imax > len-1) imax = len-1;
	if (imin > imax) { i = imin; imin = imax; imax = i; }

	x = x+imin;														/* Shift offset to starting point */
	y = y+imin;
	len = imax-imin+1;											/* Redefine the length				 */

	switch (todo) {
		/* Average of the value, weighted by 1/sigma^2 */
		case array_covar:											/* Covariance of variables */
			for (tmp2=0,tmp3=0,i=0; i<len; i++) { tmp2 += x[i]; tmp3 += y[i]; }
			if (len > 1) { tmp2 /= len; tmp3 /= len; }
			for (tmp=0,i=0; i<len; i++) tmp += (x[i]-tmp2)*(y[i]-tmp3);
			if (len > 1) tmp /= (len-1);
			break;
			
		case array_weight_avg:
			for (tmp=0,tmp2=0,i=0; i<len; i++) { tmp += x[i]/(y[i]*y[i]); tmp2 += 1/(y[i]*y[i]); }
			if (len > 1 && tmp2>0) tmp /= tmp2;
			break;

		/* Average of the absolute value, weighted by 1/sigma^2 */
		case array_weight_absavg:
			for (tmp=0,tmp2=0,i=0; i<len; i++) { tmp += fabs(x[i])/(y[i]*y[i]); tmp2 += 1/(y[i]*y[i]); }
			if (len > 1 && tmp2 > 0) tmp /= tmp2;
			break;

		/* Estimate of the uncertainty in the mean (above) given known uncertainties sigma */
		case array_weight_sigma:
			for (tmp=0,tmp2=0,i=0; i<len; i++) { tmp += 1/(y[i]*y[i]); }
			if (len > 1) tmp = 1/SQRT(tmp);
			break;

		/* Estimate of the variance in the mean (above) given relative uncertainties sigma */
		case array_weight_var:
		case array_weight_std:
		case array_weight_sdom:
			for (tmp=tmp2=tmp3=0,i=0; i<len; i++) { tmp += x[i]/(y[i]*y[i]); tmp2 += 1/(y[i]*y[i]); tmp3 += x[i]*x[i]/(y[i]*y[i]); }
			if (len > 1 && tmp2>0) {
				tmp  /= tmp2;			/* <x> */
				tmp3 /= tmp2;			/* <x^2> */
				tmp = (tmp3-tmp*tmp)*len/(len-1);			/* Estimated sigma^2 variance for weighting values of sigma only */
				if (todo == array_weight_std)  tmp = SQRT(tmp);
				if (todo == array_weight_sdom) tmp = SQRT(tmp/len);
			}
			break;

		default:
			panic;
	}
	return(tmp);
}

/* ---------------------------------------------------------------------------
-- Routine to evaluate the array functions on simple curves.  Can be called
-- either from this code or from GVPARSE
--
-- Usage:  TMPREAL = eval_curve_fnc(unsigned char todo, 2DCURVE *curveptr);
--
-- Inputs: todo - command to be evaluated (one of @SUM, @MIN, @MAX ...);
--         arrayptr - pointer to an array structure
--
-- Output: none
--
-- Returns: value of function evaluation
---------------------------------------------------------------------------- */
TMPREAL gv_eval_curve_fnc(unsigned int todo, CURVE *curveptr, REAL xmin, REAL xmax) {

	int i, ipt, npt;
	REAL *x, *y, *buf, xtmp;
	TMPREAL tmp;

	npt = curveptr->npt;
	x   = curveptr->x;
	y   = curveptr->y;
	if (xmin > xmax) { xtmp = xmin; xmin = xmax; xmax = xtmp; }

	/* If immediate call, just do.  Otherwise create buffer and copy valid points */
	if (xmin == -REAL_MAX && xmax == REAL_MAX) {								/* Default values that return all as valid */
		tmp = gv_eval_buf(todo, y, npt);
	} else if ( (buf = malloc(npt*sizeof(*buf))) != NULL) {				/* Make space for potentially every point */
		for (i=0,ipt=0; i<npt; i++) {
			if (x[i] >= xmin && x[i] <= xmax) buf[ipt++] = y[i];
		}
		tmp = gv_eval_buf(todo, buf, ipt);
		free(buf);
	} else {
		tmp = 0;
	}

	return tmp;
}

/* ---------------------------------------------------------------------------
-- Routine to evaluate the curve functions.  Called from GVPARSE when seeing
-- some @fnc type functions.  
--
-- Usage:  TMPREAL = find_median(REAL *x, int npt);
--
-- Inputs: x - array of data values
--         npt - number of points in the array
--
-- Output: none
--
-- Returns: median value of the array
--
-- Notes: The original algorithm had a setting the new guess for the median
--        to aa.  But this failed in a few cases where it would oscillate
--        across between two points giving +/-2 and never reaching the middle
--        I changed the weighting to be a 75% aa and 25% median average to
--        slow down the approach and avoid this trap
---------------------------------------------------------------------------- */
#define	AFAC	1.5f
#define	AMP	4.0f

PRIVATE TMPREAL find_median(REAL *x, int npt) {

	REAL xfloor, xceil, median, low, high;		/* Guess & bounds	*/
	REAL denom,sum,sumx, aa, avgsp;				/* Random vars		*/
	int i, icnt, ilow, ihigh, iter,itermax,imatch;
	
	if (npt <= 1) {								/* Pathological cases */
		return(x[0]);
	}

	low = +REAL_MAX; high = -REAL_MAX;		/* Bounds on median		*/
	for (i=0; i<npt; i++) {						/* Make bounds more valid */
		if (x[i] < low)  {
			low = x[i]; ilow = 1;
		} else if (x[i] > high) {
			high = x[i]; ihigh = 1;
		} else if (x[i] == low) {
			ilow++;
		} else if (x[i] == high) {
			ihigh++;
		}
	}

/* Now, handle really bad cases where half or more of the samples
-- are equal to either the maximum or minimum.  Deal with immediately */
	if (ihigh > npt/2) return(high);		/* Median is same as top/bottom */
	if (ilow  > npt/2) return(low);
	if (ihigh == npt/2) {
		median = -REAL_MAX;
		for (i=0; i<npt; i++) {
			if (x[i] == high) continue;
			if (x[i] > median) median = x[i];
		}
		return((median+high)/2);
	}
	if (ilow == npt/2) {
		median = +REAL_MAX;
		for (i=0; i<npt; i++) {
			if (x[i] == low) continue;
			if (x[i] < median) median = x[i];
		}
		return((median+low)/2);
	}
		
/* Otherwise, start bifurcating the bounds */
	median = (low+high)/2;								/* initial guess */
	avgsp  = (REAL) fabs((high-low)/(npt-1));		/* Average spacing between values */
				
	itermax = 10 +											/* A lower level needed */
				 (int) fabs((LOG((fabs(high)+1E-37)/(fabs(low)+1E-37))) / LOG(2.0)) +
				 (int) (2*LOG(npt));
	if (itermax > 250) itermax = 250;				/* Don't run forever even with large N */

	for (iter=0; iter<itermax; iter++) {			/* unrecognized problem	*/
		xfloor = -REAL_MAX; xceil = REAL_MAX;		/* Relative to median	*/
		sum = sumx = 0.0f;								/* Estimate updator		*/
		for (icnt=0,imatch=0,i=0; i<npt; i++) {
			if (x[i] == median) {
				imatch++; continue;
			} else if (x[i] > median) {
				if (x[i] < xceil) xceil = x[i];		/* Nearest point above estimated median */
				icnt++;										/* Relative # above to below */
			} else {
				if (x[i] > xfloor) xfloor = x[i];	/* Nearest point below estimated median */
				icnt--;
			}
			denom = (REAL) (1.0f/(avgsp+fabs(x[i]-median)));
			sum  += denom;
			sumx += x[i]*denom;
		}

		if (abs(icnt) < 2) break;				/* Done				*/
		if (abs(icnt) <= imatch) break;		/* Also done		*/

		if (icnt > 0) {							/* Guess too low	*/
			low = median;
			aa = xceil + max(0.0f,sumx/sum-median)*AMP;
			if (aa > high) aa = (median+high)/2;
		} else {								/* Guess too high */
			high = median;
			aa = xfloor + min(0.0f,sumx/sum-median)*AMP;
			if (aa < low) aa = (median+low)/2;
		}
		if (SysDebugFlag & 0x02) TTYprintf("iter: %d  guess: %f %f %d %d %f %f %f %f\n", iter, median, aa, icnt, imatch, low, high, xfloor, xceil);
		avgsp = (REAL) (AFAC*fabs(aa-median));
		median = (REAL) (0.75*aa+0.25*median);		/* Slow down the convergence a bit ... but avoid a real problem */
	}

	if (SysDebugFlag & 0x02) TTYprintf("iter: %d %d  %d %d\n", iter, itermax, icnt, imatch);
				
	if (iter == itermax) {
		ERRprintf("ERROR: Iterative @median finder failed.  Use \"sort y eval y[npt/2]\"\n");
/*		ERRprintf("iter: %d  tiermax: %d  icnt: %d imatch: %d\n", iter, itermax, icnt, imatch); */
/*		SysDebugFlag |= 0x02; */
		return -1.0;
	}

	if (imatch > abs(icnt)) {				/* Is our guess the answer? */
		median = median;
	} else if (npt%2 == 0) {				/* even => must do average */
		if (icnt > 0) xfloor = median;	/* Was median exact some?	*/
		if (icnt < 0) xceil  = median;
		median = (xfloor+xceil)/2;
	} else {										/* odd => expect exact value */
		if (icnt > 0) median = xceil;
		if (icnt < 0) median = xfloor;
	}

	return median;
}


/* ---------------------------------------------------------------------------
-- Routine to evaluate the curve functions.  Called from GVPARSE when seeing
-- some @fnc type functions.  
--
-- Usage:  TMPREAL = gv_eval_curve(unsigned todo, CURVE *curve, REAL xmin, REAL xmax, int *rc);
--
-- Inputs: mycmd - command to be evaluated (one of @INTEGRAL ...);
--         curve - pointer to an curve structure
--         xmin,xmax - range of X data to include in calculations
--                     Curve is assumed sorted if these are not -REAL_MAX, +REAL_MAX
--
-- Output: *rc - if not NULL, receives success.  
--               0 if successful, +1 if any type of error (FATAL)
--
-- Returns: value of the expression evaluation
---------------------------------------------------------------------------- */
EXTERN TMPREAL gv_eval_curve(unsigned int todo, CURVE *curve, REAL xmin, REAL xmax, int *rc) {

	REAL *x, *y, *z;
	TMPREAL result,s0,s1,s2,s3,s4,ds,sx,sxx,sy,syy,sxy;
	int i,npt;

	x = curve->x;
	y = curve->y;
	z = curve->z;
	npt = curve->npt;

	/* Limit range if the optional range has been specified */
	if (xmin != -REAL_MAX || xmax != REAL_MAX) {								/* Default values that return all as valid */
		int istart,iend;
		for (istart=0; istart<npt; istart++) {
			if (x[istart] > xmin) break;											/* First point that is valid */
		}
		for (iend=npt-1; iend>istart; iend--) {
			if (x[istart] < xmax) break;											/* Last point that is valid */
		}
		x += istart; y += istart; z += istart; npt = iend-istart+1;
	}

	switch (todo) {
		case curve_integral:
			for (result=0,i=1; i<npt; i++) {
				result += (y[i]+y[i-1])/2 * (x[i]-x[i-1]);
			}
			break;
		case curve_correlate:
			if (npt < 2) {result = 0; break;}
			s0 = sx = sxx = sy = syy = sxy = 0;
			for (i=0; i<npt; i++) {
				s0 += 1;	sx += x[i]; sxx += x[i]*x[i];
				         sy += y[i]; syy += y[i]*y[i]; sxy += x[i]*y[i];
			}
			result = (s0*sxy-sx*sy) / SQRT((s0*sxx-sx*sx)*(s0*syy-sy*sy));
			break;

		case curve_avg:
		case curve_std:
		case curve_var:
		case curve_skew:
		case curve_kurt:
		case curve_median:
			/* Do all the moments simultaneously ... just not expensive enough to bother */
			s0 = s1 = s2 = s3 = s4 = 0.0;
			for (i=0; i<npt-1; i++) {
				s0 += (       y[i+1]+     y[i])*(x[i+1]-x[i])/2.0;
				s1 += (x[i+1]*y[i+1]+x[i]*y[i])*(x[i+1]-x[i])/2.0;
			}
			if (s0 == 0.0) {											/* Bad problem ... no way to normalize so abort */
				result = 0.0;
				break;
			}
			s1 = s1/s0;													/* Normalize to get the mean */
			switch (todo) {
				case curve_avg:										/* Already have, so just return */
					result = s1;
					break;
				case curve_median:									/* Generate the CDF and find 50% point */
					s1 = 0;
					for (i=0; i<npt-1; i++) {
						ds = (y[i+1]+y[i])*(x[i+1]-x[i])/2.0;
						if (s1+ds > s0/2) break;					/* We cross the 50% point */
						s1 += ds;
					}
					s1 /= s0; ds /= s0;								/* Now normalized in the range */
					result = x[i]+(x[i+1]-x[i])*(0.5-s1)/ds;	/* Linear interpolated range */
					break;
				default:
					for (i=0; i<npt-1; i++) {
						s2 += (POW(x[i+1]-s1,2)*y[i+1]+POW(x[i]-s1,2)*y[i])*(x[i+1]-x[i])/2.0;
						s3 += (POW(x[i+1]-s1,3)*y[i+1]+POW(x[i]-s1,3)*y[i])*(x[i+1]-x[i])/2.0;
						s4 += (POW(x[i+1]-s1,4)*y[i+1]+POW(x[i]-s1,4)*y[i])*(x[i+1]-x[i])/2.0;
					}
					s2 /= s0;  s3 /= s0; s4 /= s0;			/* Normalize */
					s2 = (s2 > 0.0) ? SQRT(s2) : 0.0 ;							/* Standard deviation */
					s3 = (s2 > 0.0) ? s3 / POW(s2,3) : 0.0 ;					/* Skewness */
					s4 = (s2 > 0.0) ? s4 / POW(s2,4) - 3.0 : 0.0 ;			/* Excess kurtosis */
					if (todo == curve_std)  result = s2;						/* Return which needed */
					if (todo == curve_var)  result = s2*s2;
					if (todo == curve_skew) result = s3;
					if (todo == curve_kurt) result = s4;
			}
			break;

		default:
			panic;
	}

	/* Return the value */
	if (rc != NULL) *rc = 0;
	return result;
}


/* ---------------------------------------------------------------------------
-- Routine to evaluate some surface functions.  Called from GVPARSE when seeing
-- some @fnc type functions.
--
-- Usage:  TMPREAL = gv_eval_surface(unsigned int todo, TMPREAL *rval, CURVE *surface);
--
-- Inputs: mycmd - command to be evaluated (one of @ZINTEGRAL ...);
--         suface - pointer to a surface structure
--
-- Output: *rval - set to value of the expression evaluation
--
-- Returns: 0 if successful, +1 if any type of error (FATAL)
---------------------------------------------------------------------------- */
EXTERN int gv_eval_surface(unsigned int todo, TMPREAL *rval, SURFACE *surface) {

	REAL *x, *y, *z;
	int nrow, ncol;
	TMPREAL result,dx,dy;
	int i,j;

	x = surface->x;
	y = surface->y;
	z = surface->z;
	nrow = surface->nrow;
	ncol = surface->ncol;

	switch (todo) {
		case surf_integral:
			result = 0.0;
			for (i=0; i<ncol; i++) {
				dx = (i == 0) ? (x[1]-x[0])/2 : (i==ncol-1) ? (x[ncol-1]-x[ncol-2])/2 : (x[i+1]-x[i-1])/2 ;
				for (j=0; j<nrow; j++) {
					dy = (j == 0) ? (y[1]-y[0])/2 : (j==nrow-1) ? (y[nrow-1]-y[nrow-2])/2 : (y[j+1]-y[j-1])/2 ;
					result += z[j+i*nrow]*dx*dy;
				}
			}
			*rval = result;
			break;
		default:
			panic;
	}
	return(0);
}


/* ===========================================================================
-- Routine to return the regularized incomplete beta function I_x(A,B)
-- Must be multiplied by B(a,b) to get true incomplete beta function
--
-- Usage: TMPREAL BETAI_Ix(TMPREAL x, TMPREAL a, TMPREAL b);
--        TMPREAL BETAI (TMPREAL x, TMPREAL a, TMPREAL b);
--
-- Inputs: a,b,x - parameters in incomplete beta function
--
-- Output: none
--
-- Return: BETAI_Ix ==> I_x(a,b) -- regularized incomplete beta function
--         BETAI  ==> B(x;a,b) -- incomplete beta function
--
-- Error handling: If x is outside range of [0,1], the limiting value for
--                 0 or 1 is returned.
--
-- The regularized incomplete beta function is used in evaluation of the t-test and
-- f-test functions.
=========================================================================== */
#define	ITMAX		1000								/* Iterations to convergence */
#define	EPSILON	5E-8								/* Precision desired */

TMPREAL BETAI(TMPREAL x, TMPREAL a, TMPREAL b) {
	return BETAI_Ix(x,a,b) * EXP(LN_GAMMA(a)+LN_GAMMA(b)-LN_GAMMA(a+b));
}

TMPREAL BETAI_Ix(TMPREAL x, TMPREAL a, TMPREAL b) {

	TMPREAL rval;								/* Result */
	int invert;									/* Is symmetry form used? */

	TMPREAL d;									/* Parameters for continued fraction */
	TMPREAL bz,bm,bp,bpp;
	TMPREAL az,am,ap,app,aold;
	int m;

/* Trivial cases in initial tests */
	if (x <= 0.0) {									/* Just return value for zero */
		return(0.0);
	} else if (x >= 1.0) {
		return(1.0);
	} 

/* Use symmetry if necessary to improve convergence of continued fraction */
	invert = x > (1+a)/(a+b+2);
	if (invert) {x = 1-x; rval = a; a = b; b = rval;}

/* Prefactor and continued fraction evaluation */
	rval = EXP(a*LOG(x)+b*LOG(1-x)-LN_GAMMA(a)-LN_GAMMA(b)+LN_GAMMA(a+b)) / a;

/* Continued fraction component - based on PFTV Numerical Recipes code */
	az = 1;
	am = 1;
	bm = 1;
	bz = 1-(a+b)*x/(a+1);

	for (m=1; m<=ITMAX; m++) {						/* Continue fraction */
		d    = m*(b-m)*x        / ((a+2*m-1)*(a+2*m));
		ap   = az+d*am;
		bp   = bz+d*bm;
		d    = -(a+m)*(a+b+m)*x / ((a+2*m)*(a+2*m+1));
		app  = ap+d*az;
		bpp  = bp+d*bz;
		aold = az;
		am   = ap/bpp;
		bm   = bp/bpp;
		az   = app/bpp;
		bz   = 1.0;
		if (FABS(az-aold) < (EPSILON*FABS(az))) break;
	}
	if (m > ITMAX) {
		ERRprintf("WARNING: B(x|v1,v2) regularized incomplete beta function failed to converge\n");
	}
	
	rval = rval * az;
	if (invert) rval = 1-rval;						/* Rest of inversion symmetry */
	return(rval);
}

#undef	ITMAX
#undef	EPSILON

/* ===========================================================================
-- Routine to handle partial integration of a curve.  Does really stupid
-- trapezoidal rule, but best I'm willing to do at the moment.
--
-- Usage: p_integrate(CURVE *cv, TMPREAL lower, TMPREAL upper);
--
-- Inputs: cv    -> curve
--         lower -> lower limit of integral 
--         upper -> upper limit of integral 
--
-- Output: none
--
-- Returns: Estimate of the integral
--
-- Note: Value of function is assumed to be zero if function is not defined.
=========================================================================== */
PRIVATE TMPREAL p_integrate(CURVE *cv, TMPREAL lower, TMPREAL upper) {

	int i, negate, mode;
	TMPREAL tmp;
	REAL *x, *y;

	negate = (upper < lower);
	if (negate) {tmp = upper; upper = lower; lower = tmp;}
	
	x = cv->x; y = cv->y;
	tmp = 0;

/* mode bit definitions:
--   0 --> this x point is below lower limit
--   1 --> this x point is above upper limit
--   2 --> previous x point is below lower limit
--   3 --> previous x point is above lower limit
--- */
	mode = 0;
	if (x[0] < lower) mode |= 0x01;				/* Initial mode values */
	if (x[0] > upper) mode |= 0x02;

	for (i=1; i<cv->npt; i++) {
		mode = (mode & 0x3) << 2;						/* Last point to bits 2,3 */
		if (x[i] < lower) mode |= 0x01;
		if (x[i] > upper) mode |= 0x02;
		switch (mode) {									/* Only show possible pairs */
			case 0:													/* Both in range */
				tmp += (y[i]+y[i-1])/2 * (x[i]-x[i-1]);	break;
			case 1:													/* i-1 in range, i low */
				tmp += (y[i]+y[i-1])/2 * (lower-x[i-1]);	break;
			case 2:													/* i-1 in range, i high */
				tmp += (y[i]+y[i-1])/2 * (upper-x[i-1]);	break;
			case 4:													/* i-1 low, i in range */
				tmp += (y[i]+y[i-1])/2 * (x[i]-lower);		break;
			case 5:													/* i-1 low, i low */
				break;
			case 6:													/* i-1 low, i high */
				tmp += (y[i]+y[i-1])/2 * (upper-lower);	break;
			case 8:													/* i-1 high, i in range */
				tmp += (y[i]+y[i-1])/2 * (x[i]-upper);		break;
			case 9:													/* i-1 high, i low */
				tmp += (y[i]+y[i-1])/2 * (lower-upper);	break;
			case 10:													/* i-1 high, i high */
				break;
		}
	}
	if (negate) tmp = -tmp;
	return(tmp);
}


/* ===========================================================================
-- Routine to find point nearest to a specified value.  
--
-- Usage: find_near_curve(int mode, CURVE *cv, TMPREAL x0, TMPREAL y0, TMPREAL z0);
--
-- Inputs: mode  -> 1 ==> 1D search (x)
--                  2 ==> 2D search (x,y)
--                  3 ==> 3D search (x,y,z)
--         cv    -> curve
--         x0    -> Sought x point
--         y0    -> Sought y point
--         z0    -> Sought z point (only in mode = 3)
--
-- Output: none
--
-- Returns: Index of nearest point
=========================================================================== */
PRIVATE TMPREAL find_near_curve(int mode, CURVE *cv, TMPREAL x0, TMPREAL y0, TMPREAL z0) {

	int i, ipt;
	TMPREAL r, rmax;
	REAL *x, *y, *z;

/* Which should be used in determining "distance" */
	x = cv->x; 
	y = (mode >= 2) ? cv->y : NULL;
	z = (mode >= 3) ? cv->z : NULL;

	ipt = 0; rmax = 1E38;
	for (i=0; i<cv->npt; i++) {
		r = pow(x[i]-x0,2);
		if (y != NULL) r += pow(y[i]-y0,2);
		if (z != NULL) r += pow(z[i]-z0,2);
		if (r < rmax) { rmax = r; ipt = i; }
	}
	return (TMPREAL) ipt;
}	

/* ===========================================================================
-- Routine to interpolate on a surface structure
--
-- Usage: surface_interpolate(SURFACE *sf, TMPREAL x0, TMPREAL y0);
--
-- Inputs: sf    -> surface
--         x0    -> Sought x point
--         y0    -> Sought y point
--
-- Output: none
--
-- Returns: Linearly interpolated value at given point
=========================================================================== */
PRIVATE TMPREAL surface_interpolate(SURFACE *sf, TMPREAL x0, TMPREAL y0) {

	int ix,iy;
	REAL *x, *y, *z;
	double xf,yf, x1,y1,z1, x2,y2,z2, x3,y3,z3;
	double a,b,denom;
	int nx, ny;

/* Get easy variables */
	x = sf->x; y = sf->y; z = sf->z;
	nx = sf->ncol; ny = sf->nrow;

/* Find the index where x[i] and x[i+1] bound value safely */
	if ( (x0<x[0]) == (x0<x[nx-1]) ) {						/* Outside one or the other */
		if (fabs(x0-x[0]) < fabs(x0-x[nx-1])) {
			xf = 0.0; ix = 0;
		} else {
			xf = 1.0; ix = nx-2;
		}
	} else {
		for (ix=0; ix<nx-2; ix++) {
			if ( (x0<x[ix]) != (x0<x[ix+1]) ) break;
		}
		xf = (x[ix+1]-x[ix] != 0) ? (x0-x[ix])/(x[ix+1]-x[ix]) : 0.0 ;
	}

/* Find the index where y[i] and y[i+1] bound value safely */
	if ( (y0<y[0]) == (y0<y[ny-1]) ) {						/* Outside one or the other */
		if (fabs(y0-y[0]) < fabs(y0-y[ny-1])) {
			yf = 0.0; iy = 0;
		} else {
			yf = 1.0; iy = ny-2;
		}
	} else {
		for (iy=0; iy<ny-2; iy++) {
			if ( (y0<y[iy]) != (y0<y[iy+1]) ) break;
		}
		yf = (y[iy+1]-y[iy] != 0) ? (y0-y[iy])/(y[iy+1]-y[iy]) : 0.0 ;
	}
	if (xf < 0 || xf > 1 || yf < 0 || yf > 1) {
		TTYprintf("TELL PROGRAMMER: xf,yf: %f %f x0,y0: %f %f\n", xf, yf, x0,y0);
		xf = 0; yf = 0;
	}
	
	x1 = 1.0; y1 = 0.0; z1 = z[iy+(ix+1)*ny];			/* Diagonal element */
	x2 = 0.0; y2 = 1.0; z2 = z[iy+1+ix*ny];			/* Diagonal element */
	if (xf+yf < 1.0) {
		x3 = 0.0; y3 = 0.0; z3 = z[iy+ix*ny];			/* Bottom corner */
	} else {
		x3 = 1.0; y3 = 1.0; z3 = z[iy+1+(ix+1)*ny];	/* Upper corner */
	}

	denom = (x1-x3)*(y2-y3) - (x2-x3)*(y1-y3);		/* If zero, coplanar points ... punt */
	if (denom == 0) denom = 1.0 ;							/* Garbage in = garbage out */

	a = ((z1-z3)*(y2-y3)-(z2-z3)*(y1-y3)) / denom;
	b = ((x1-x3)*(z2-z3)-(x2-x3)*(z1-z3)) / denom;

	return (TMPREAL) (a*(xf-x3) + b*(yf-y3) + z3);
}	


/* ===========================================================================
-- Routine to determine the chisqr probability function Q(cs,df)
--
-- Usage:  real = CalcQChiSqr(x2, v)
--
-- Inputs: x2 - X^2 (chisqr) value observed
--         v  - Number of degrees of freedom
--
-- Output: CalcQChiSqr - Probability that X^2 or worse value would be observed 
--                   from a normal distribution
--
-- References: Abramowitz and Stegen
--    Chi-Square Probability Function    Sec. 26.4
--    xxx
--        6.5.29
--
--   P(x^2|v)               probability of X^2 being >= x^2 with v degrees
--   Q(x^2|v) = 1-P(x^2|v)
--
-- Function actually returns Q.
--
-- For x^2 < v, use series expansion basically 26.4.6 slightly rewritten.
-- For x^2 > v, use 
============================================================================ */
#define	ITMAX		500									/* Max # loop iterations */
#define	EPSILON	5E-8									/* Desired accuracy */
#define	SQRT2PI	2.5066282746310005024			/* sqrt(2*pi) */

TMPREAL PQ_CHI(TMPREAL x2, TMPREAL v, char type) {

	TMPREAL cof[] = {190.95517189584865,     -216.8366818468335,
						  60.194417588474245,      -3.08751309780930638,
							0.00302946087537534544, -0.0000134451028721331642} ;
	TMPREAL rval,gln,ap,sum,sumold,a0,a1,an,ana,anf,b0,b1,fac;
	int i,pfnc;
	
	pfnc = (type == 'p' || type == 'P');

	if (x2 < 0.0 || v <= 0.0) return(0.0);	/* Bad arguments */

	ap = (x2-v)/SQRT(2*v);							/* First guess */
	if (pfnc   && ap > 20.0)  return(1.0);		/* Close enough for government */
	if (! pfnc && ap < -20.0) return(1.0);		/* Too close for difference */

/* ... First, evaluate ln(gamma(v/2)) by Sterling formula (full!) */
/* ... Source of this expansion unknown, but very good ... */
	ap  = v/2;											/* AP used later also! */
	gln = (ap-0.5)*LOG(ap+4.5)-(ap+4.5);		/* Sterling approx! */
	sum = SQRT2PI;										/* Additional for negative! */
	for (i=0; i<6; i++) {							/* Corrections */
		sum += cof[i]/ap;
		ap += 1;
	}
	gln += LOG(sum);									/* And final values */

/* -------------------------------------
                        v/2                                      2
           -x^2/2  |x^2|     1
 RVAL =   e        |---|  -_-----
                   | 2 |  | (v/2)

---------------------------------------- */
	rval = EXP(-x2/2.0+v/2.0*LOG(x2/2.0)-gln);	/* Needed by both */

/* ... Now, either use continued fractions or series approximation */
	if (x2 < v+0.5) {										/* Use series approximation */
		ap  = v/2.0;										/* reformulated A&S 26.4.6 */
		sum = b0 = 1.0/ap;								/* First term (start at r=1) */
		for (i=0; i<ITMAX; i++) {
			ap   = ap+1;									/* (v/2+r) */
			b0  *= (x2/2)/ap;								/* (x^2/2)^r/(v/2)/(v/2+1)/. */
			sum += b0;
			if (FABS(b0) < FABS(sum)*EPSILON)
				return(pfnc ? sum*rval : 1.0-sum*rval);
		}
	} else {												/* Use continued fraction */
		sumold = 0.0;									/* Looks like A&S 26.4.10 */
		a0 = 1.0;
		a1 = x2/2;
		b0 = 0.0;
		b1 = 1.0;
		fac = 1.0;
		for (i=0; i<ITMAX; i++) {					/* Term # used in series below */
			an = i+1;									/* Index in the sum */
			ana = an-v/2;
			a0 = (a1+a0*ana)*fac;
			b0 = (b1+b0*ana)*fac;
			anf = an*fac;
			a1 = x2/2*a0+anf*a1;
			b1 = x2/2*b0+anf*b1;
			if (a1 != 0) {
				fac = 1/a1;
				sum = b1*fac;
				if (FABS(sum-sumold) < EPSILON*fabs(sum))
					return(pfnc ? 1-rval*sum : rval*sum);
				sumold = sum;
			}
		}
	}

	ERRprintf("WARNING: Q(x^2|v) chi-square probability failed to converge\n");
	return(0.0);										/* Don't really know why fail */
}

#undef	ITMAX
#undef	EPSILON
#undef	SQRT2PI

/* ===========================================================================
-- Routine to handle calls to lexical routines.  These are unusual in that
-- they set a string value which is passed by reference essentially.
--
-- Usage: HandleLexFnc(unsigned int mycmd, INT64 p1, INT64 p2) {
--
-- Inputs: mycmd - the command code (switch value)
--         p1  - first integer required by some fncs
--         p2  - second integer required by some fncs
--
-- Returns: integer which should get pushed on the stack
--
-- Handles:  strcmp_me, stricmp_me, strlen_me, strnlen_me, strcspn_me,
--           strspn_me, strncmp_me, strnicmp_me, lexequal_me
=========================================================================== */
PRIVATE TMPREAL HandleLexFnc(unsigned int mycmd, INT64 p1, INT64 p2) {

	char *prompt, token[1024];
	int rc, status;
	GV_ENTRY *entry;								/* Variable pointer */

/* All other possibilities handled here */
	switch (mycmd) {
		case lex_get_token:
			rc = LexGetToken(token, sizeof(token));
			break;
		case lex_get_token_p:
			prompt = sstackptr[0].str;			/* Get the prompt */
			rc = LexGetTokenP(token, sizeof(token), prompt);
			POP_SSTACK;								/* Pop this puppy */	
			break;
		case lex_chk_token:
			rc = LexChkToken(token, sizeof(token));
			break;
		default:
			ERRprintf("AARGH! String/string function failed -- scream at developers\n");
			return(-1);
	}

/* At this point, token contains the string to be put into the variable */
	status = sstackptr[0].status;
	entry  = (GV_ENTRY *) sstackptr[0].str;
	POP_SSTACK;
	if (status != IS_VARENTRY) {
		ERRprintf("AARGH!  This *had to be* a variable entry pointer but isn't.  Developers are toast\n");
		return 0;
	} else if (entry->type == GV_STRING) {
		if (entry->var.stradr == NULL || entry->extra < (int) strlen(token)) entry->var.stradr = realloc(entry->var.stradr, strlen(token)+1);
		strcpy(entry->var.stradr, token);
	} else if (entry->type == GV_STRING_LINK) {
		strscpy(entry->var.stradr, token, entry->extra);
	} else { 
		ERRprintf("AARGH!  Only a string var or string linked var should have been on the stack\n");
	}
	return rc;
}

/* ===========================================================================
-- Routine to handle operating system calls based on string arguments.
-- Makes duplicating code between REAL and COMPLEX unnecessary.
--
-- Usage: HandleOSFnc(unsigned int mycmd);
--
-- Inputs: mycmd - the command code (switch value)
--
-- Returns: value which gets put on the real or complex stack
=========================================================================== */
PRIVATE TMPREAL HandleOSFnc(unsigned int mycmd) {

	int rc=0, ipop=0;

	switch (mycmd) {
		case chdir_me:
			rc = chdir(sstackptr[0].str);		ipop = 1;	break;
		case rmdir_me:
			rc = rmdir(sstackptr[0].str);		ipop = 1;	break;
		case mkdir_me:
#if (defined UNIX || defined LINUX)
			rc = mkdir(sstackptr[0].str, S_IRWXU | S_IRWXG | S_IROTH);	ipop = 1;	break;
#else
			rc = mkdir(sstackptr[0].str);		ipop = 1;	break;
#endif
		case rm_me:
			rc = remove(sstackptr[0].str);	ipop = 1;	break;
		case unlink_me:
			rc = unlink(sstackptr[0].str);	ipop = 1;	break;
		case mv_me:
			rc = rename(sstackptr[1].str, sstackptr[0].str);	ipop = 2;	break;
		default:
			ERRprintf("AARGH! OS function failed -- scream at developers\n");
			rc = -1;
			break;
	}
	while (ipop--) {
		if (sstackptr->status == IS_MALLOC) free(sstackptr->str); 
		sstackptr++;
	}

	return (TMPREAL) rc;
}


/* ===========================================================================
-- Routine to handle the string subroutine calls.  Makes duplicating code 
-- between REAL and COMPLEX unnecessary.  Don't expect (want) string calls 
-- within array, so overhead of the call is irrelevent.
--
-- Usage: HandleNumStringFnc(unsigned int mycmd, int parm) {
--
-- Inputs: mycmd - the command code (switch value)
--         parm  - a single integer parameter required by some fncs
--
-- Returns: value which gets put on the real or complex stack
--
-- Handles:  strcmp_me, stricmp_me, strlen_me, strnlen_me, strcspn_me,
--           strspn_me, strncmp_me, strnicmp_me, lexequal_me
--           hex2int_me, hex2float_me:
=========================================================================== */
PRIVATE TMPREAL HandleNumStringFnc(unsigned int mycmd, INT64 p1, INT64 p2, INT64 p3, INT64 p4, INT64 p5) {

	TMPREAL rcode=0;
	int ipop=0;
	FILE *funit, **pfunit;
	BOOL exists;
#if (defined MSC70)
	struct __stat64 sbuf; 
#else
	struct stat sbuf; 
#endif
	char *aptr;

	switch (mycmd) {
		case rgb_color_me:						/* Depends on PLOT availability */
			rcode = (RGB_Color != NULL) ? (*RGB_Color)(sstackptr[0].str) : 0 ;
			ipop = 1;
			break;
		case strtol_me:
			rcode = strtol(sstackptr->str, NULL, (int) p1);
			ipop = 1;
			break;
		case strcmp_me:
			rcode = strcmp(sstackptr[1].str, sstackptr[0].str); 
			ipop = 2;
			break;
		case stricmp_me:
			rcode = stricmp(sstackptr[1].str, sstackptr[0].str); 
			ipop = 2;
			break;
		case strncmp_me:
			p1 = max(0,p1);
			rcode = strncmp(sstackptr[1].str, sstackptr[0].str, (int) p1);
			ipop = 2;
			break;
		case strnicmp_me:
			p1 = max(0,p1);
			rcode = strnicmp(sstackptr[1].str, sstackptr[0].str, (int) p1); 
			ipop = 2;
			break;
		case strlen_me:
			rcode = (sstackptr[0].str != NULL) ? strlen(sstackptr[0].str) : 0;
			ipop = 1;
			break;
		case strnlen_me:
			rcode = (sstackptr[0].str != NULL) ? strnblen(sstackptr[0].str) : 0;
			ipop = 1;
			break;
		case strcspn_me:
			rcode = strcspn(sstackptr[1].str, sstackptr[0].str); 
			ipop = 2;
			break;
		case strspn_me:
			rcode = strspn(sstackptr[1].str, sstackptr[0].str); 
			ipop = 2;
			break;
		case lexequal_me:												/* Modified extraction from LexEqual code */
			{
				char *tok1, *tok2;
				size_t minlen, len_1, len_2;

				tok1 = sstackptr[1].str;
				tok2 = sstackptr[0].str;
				minlen = (size_t) (p1>0 ? p1 : -p1);			/* If p1 < 0, use memcmp */

				while (isspace(*tok1)) tok1++;					/* Compare from non-white */
				while (isspace(*tok2)) tok2++;					/* Compare from non-white */

				len_1 = strnblen(tok1);
				len_2 = strnblen(tok2);

				if (len_1 < minlen || len_2 < minlen ||
				    (p1 >= 0 && memicmp(tok1, tok2, min(len_1,len_2)) != 0) ||
					 (p1 <  0 &&  memcmp(tok1, tok2, min(len_1,len_2)) != 0) ) {
					rcode = 0;
				} else {											/* Partial or full match!! */
					rcode = (len_1 == len_2) ? 2 : 1;
				}
			}
			ipop = 2;
			break;

		case rexx_ichar:
			rcode = (int) sstackptr->str[0];
			ipop = 1;
			break;

		case hex2int_me:
			rcode = my_base2int(sstackptr->str, 16);
			ipop = 1;
			break;
		case oct2int_me:
			rcode = my_base2int(sstackptr->str, 8);
			ipop = 1;
			break;
		case bin2int_me:
			rcode = my_base2int(sstackptr->str, 2);
			ipop = 1;
			break;
		case base2int_me:
			rcode = my_base2int(sstackptr->str, (int) p1);
			ipop = 1;
			break;

		case hex2float_me:
			rcode = my_hex2float(sstackptr->str);
			ipop = 1;
			break;
		case hex2double_me:
			rcode = my_hex2double(sstackptr->str);
			ipop = 1;
			break;
		case time2double_me:
			rcode = my_time2double(sstackptr->str);
			ipop = 1;
			break;

		case rexx_compare:
			rcode = REXX_compare(sstackptr[1].str, sstackptr[0].str, (int) p1);
			ipop = 2;
			break;
		case rexx_words:
			rcode = REXX_words(sstackptr->str);
			ipop = 1;
			break;
		case rexx_wordindex:
			rcode = REXX_wordindex(sstackptr->str, (int) p1);
			ipop = 1;
			break;
		case rexx_wordlength:
			rcode = REXX_wordlength(sstackptr->str, (int) p1);
			ipop = 1;
			break;
		case rexx_pos:
			rcode = REXX_pos(sstackptr[1].str, sstackptr[0].str, (int) p1);
			ipop = 2;
			break;
		case rexx_lastpos:
			rcode = REXX_lastpos(sstackptr[1].str, sstackptr[0].str, (int) p1);
			ipop = 2;
			break;
		case rexx_abbrev:
			rcode = REXX_abbrev(sstackptr[1].str, sstackptr[0].str, (int) p1);
			ipop = 2;
			break;
		case rexx_wordpos:
			rcode = REXX_wordpos(sstackptr[1].str, sstackptr[0].str, (int) p1);
			ipop = 2;
			break;
		case rexx_verify:
			rcode = REXX_verify(sstackptr[2].str, sstackptr[1].str, sstackptr[0].str, (int) p1);
			ipop = 3;
			break;

		case atof_me:
			rcode = atof(sstackptr->str);
			ipop = 1;
			break;
		case atoi_me:
			rcode = atoi(sstackptr->str);
			ipop = 1;
			break;

		case isatof_me:
			strtod(sstackptr->str, &aptr);
			rcode = (*aptr == '\0' && *sstackptr->str != '\0');
			ipop = 1;
			break;
			
		case isatoi_me:
			strtol(sstackptr->str, &aptr, 10);
			rcode = (*aptr == '\0' && *sstackptr->str != '\0');
			ipop = 1;
			break;
			
		case file_sizeof_me:
		case file_dateof_me:
		case file_isfile_me:
		case file_isdir_me:
#if (defined MSC70)
			exists = _stat64(sstackptr->str, &sbuf) == 0;
#else
			exists = stat(sstackptr->str, &sbuf) == 0;
#endif
			if (! exists) {																		/* Check alts for a directory name */
				char *aptr,fname[PATH_MAX];
				strscpy(fname, sstackptr->str, sizeof(fname));							/* Make temporary copy	 */
				aptr = fname+strlen(fname)-1;													/* And find end			 */
#if (defined CSET2 || defined MSC60 || defined MSC70)
				if (! exists && strncmp(fname, "//", 2) == 0) {							/* Try UNC fix 			 */
					fname[0] = fname[1] = '\\';												/* Change // to \			 */
					exists = _stat64(fname, &sbuf) == 0;
				}
				while (! exists && aptr > fname && (*aptr == '\\' || *aptr == '/')) {
					*(aptr--) = '\0';																/* Strip trailing / or \ (s) */
					exists = _stat64(fname, &sbuf) == 0;
				}
				if (! exists && aptr > fname && *aptr == ':') {							/* Finally how about just c: */
					strcpy(aptr+1, "/");
					exists = _stat64(fname, &sbuf) == 0;
				}
#else
				exists = stat(fname, &sbuf) == 0;
				while (! exists && aptr > fname && *aptr == '/') {
					*(aptr--) = '\0';																/* Strip trailing /		 */
					exists = stat(fname, &sbuf) == 0;
				}
#endif
				exists = exists && S_ISDIR(sbuf.st_mode);									/* Alts only valid on directories */
			}
			switch (mycmd) {
				case file_isfile_me:
					rcode = (exists && S_ISREG(sbuf.st_mode)) ? 1 : 0 ;
					break;
				case file_isdir_me:
					rcode = (exists && S_ISDIR(sbuf.st_mode)) ? 1 : 0 ;
					break;
				case file_sizeof_me:
					rcode = (TMPREAL) ((exists) ? sbuf.st_size :  -1) ;
					break;
				case file_dateof_me:
					rcode = (TMPREAL) ((exists) ? sbuf.st_mtime : -1) ;
					break;
			}
			ipop = 1;
			break;

		case fclose_me:
			pfunit = (FILE **) sstackptr[0].str;	funit  = *pfunit;
			if (funit != stdin && funit != stdout && funit != stderr && funit != NULL) {
				rcode = fclose(funit);
				*pfunit = NULL;												/* Don't try again! */
			} else {
				rcode = EOF;
			}
			ipop = 1;
			break;

		case pclose_me:
			pfunit = (FILE **) sstackptr[0].str;	funit  = *pfunit;
			if (funit != stdin && funit != stdout && funit != stderr && funit != NULL) {
				rcode = pclose(funit);
				*pfunit = NULL;												/* Don't try again! */
			} else {
				rcode = EOF;
			}
			ipop = 1;
			break;

		case feof_me:
			pfunit = (FILE **) sstackptr[0].str;	funit  = *pfunit;
			rcode = (funit == NULL) ? EOF : feof(funit);
			ipop = 1;
			break;

		case ferror_me:
			pfunit = (FILE **) sstackptr[0].str;	funit  = *pfunit;
			rcode = (funit == NULL) ? EOF : ferror(funit);
			ipop = 1;
			break;
			
		case fflush_me:
			pfunit = (FILE **) sstackptr[0].str;	funit  = *pfunit;
			rcode = (funit == NULL) ? EOF : fflush(funit);
			ipop = 1;
			break;
			
		case ftell_me:
			pfunit = (FILE **) sstackptr[0].str;	funit  = *pfunit;
			rcode = (funit == NULL) ? EOF : ftell(funit);
			ipop = 1;
			break;
			
		case fgetc_me:
			pfunit = (FILE **) sstackptr[0].str;	funit  = *pfunit;
			rcode = (funit == NULL) ? EOF : fgetc(funit);
			ipop = 1;
			break;

		case fseek_me:
			pfunit = (FILE **) sstackptr[0].str;	funit  = *pfunit;
			rcode = (funit == NULL) ? EOF : fseek(funit, (int) p1, (int) p2);
			ipop = 1;
			break;

		case fputc_me:
			pfunit = (FILE **) sstackptr[0].str;	funit  = *pfunit;
			rcode = (funit == NULL) ? EOF : fputc((char) p1, funit);
			ipop = 1;
			break;

		case fprintf_me:			/* Only the filehandle and the full formatted string */
			pfunit = (FILE **) sstackptr[1].str;	funit  = *pfunit;
			rcode = (funit == NULL) ? EOF : fputs(sstackptr[0].str, funit);
			if (rcode == 0) rcode = strlen(sstackptr[0].str);
			ipop = 2;
			break;

		case fputs_me:
			pfunit = (FILE **) sstackptr[0].str;	funit  = *pfunit;
			rcode = (funit == NULL) ? EOF : fputs(sstackptr[1].str, funit);
			ipop = 2;
			break;

		default:
			ERRprintf("AARGH! String function failed -- scream at developers\n");
			rcode = -1;
			break;
	}

	while (ipop--) {
		if (sstackptr->status == IS_MALLOC) free(sstackptr->str); 
		sstackptr++;
	}

	return(rcode);
}


/* ===========================================================================
-- Routine to handle the low io function calls.  Makes duplicating code 
-- between REAL and COMPLEX unnecessary.  
--
-- Usage: HandleLowIOFnc(unsigned int mycmd, int p1, int p2, int p3, int p4, int p5);
--
-- Inputs: mycmd - the command code (switch value)
--         p1-p5 - integer parameters (from the real/complex stack)
--
-- Returns: value which gets put on the real or complex stack
--
-- Handles:  
=========================================================================== */
#ifdef NT

typedef struct _FD_LIST {
	int valid;							/* 1 ==> valid entry		*/
	int port;							/* What port was opened */
	int fd;								/* What fd was assigned */
	HANDLE hComm;						/* Windows handle used	*/
} FD_LIST;
	
PRIVATE TMPREAL HandleLowIOFnc(unsigned int mycmd, int fd, int p1, int p2, int p3, int p4, int p5) {

	#define	DEFAULT_COM_PORT	(10)
	#define	MAX_READ_LENGTH	(32768)
	#define	MAX_OPEN_COM		(16)							/* 16 seems like a lot */
	static FD_LIST *fd_list=NULL;								/* Structure for lookup of hComm */
	
	TMPREAL rcode;
	int i, port, ipop, rc;
	char com_port[10], msg[MAX_READ_LENGTH];							/* Maximum read length */
	HANDLE hComm;
	DCB dcb;
	COMMTIMEOUTS timeouts;
	static char parity[5] = {'n', 'o', 'e', 'm', 's'};
	static char *stop[3] = {"1", "1.5", "2"};

	/* First time used, clear the com port list so all marked invalid */
	if (fd_list == NULL) fd_list = calloc(MAX_OPEN_COM, sizeof(*fd_list));

	rcode = 0; ipop = 0;											/* Defaults */
	switch (mycmd) {
		case open_me:
			if (p1 == -1) p1 = _O_RDWR | _O_TEXT;
			if (p2 == -1) p2 = _S_IWRITE | _S_IREAD;
			rcode = _open(sstackptr[0].str, p1, p2);
			ipop = 1;
			break;
		case creat_me:
			if (p1 == -1) p1 = _S_IWRITE | _S_IREAD;
			rcode = _creat(sstackptr[0].str, p1);
			ipop = 1;
			break;
		case write_me:
			rcode = _write(fd, sstackptr[0].str, (unsigned int) strlen(sstackptr[0].str));
			ipop = 1;
			break;
		case read_me:									/* Read from file descriptor		*/
			if (p1 <= 0 || p1 > sizeof(msg)-1) p1 = sizeof(msg)-1;
			p1 = _read(fd, msg, p1);				/* Number actually read */
			if (p1 < 0) p1 = 0;						/* Don't tolerate errors - just return empty string */
			msg[p1] = '\0';							/* Null terminate */
			sstackptr--;
			sstackptr->str = malloc(p1+1); memcpy(sstackptr->str, msg, p1+1);
			sstackptr->status = IS_MALLOC;
			ipop = 0;
			break;
		case query_me:
			rcode = _write(fd, sstackptr[0].str, (unsigned int) strlen(sstackptr[0].str));
			if (p1 <= 0 || p1 > sizeof(msg)-1) p1 = sizeof(msg)-1;
			p1 = _read(fd, msg, p1);				/* Number actually read */
			if (p1 < 0) p1 = 0;						/* Don't tolerate errors - just return empty string */
			msg[p1] = '\0';							/* Null terminate */
			if (sstackptr->status == IS_MALLOC) free(sstackptr->str); 
			sstackptr->str = malloc(p1+1); memcpy(sstackptr->str, msg, p1+1);
			sstackptr->status = IS_MALLOC;
			ipop = 0;
			break;

		case beep_me:
			Beep(p1,p2);
			ipop = 0;
			rcode = 0;
			break;

		case open_comx_me:
			port = (p1<0) ? DEFAULT_COM_PORT : p1 ;
			sprintf(com_port, "\\\\.\\COM%d", port);
			hComm = CreateFile(com_port, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
			if (hComm == INVALID_HANDLE_VALUE) {
				rc = GetLastError();
				FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, rc, 0, msg, sizeof(msg), NULL);
				ERRprintf("ERROR: Unable to open the serial port on %s\n", com_port);
				ERRprintf(msg);
				rcode = -1;
			} else {
				rcode = _open_osfhandle((intptr_t) hComm, 0);
			}
			if (sstackptr[0].str != NULL && rcode > 0) {		/* Request to set parameters (baud, etc.) */
				FillMemory(&dcb, sizeof(dcb), 0); dcb.DCBlength = sizeof(dcb);
				GetCommState(hComm, &dcb);							/* Get existing state and just modify */
				BuildCommDCB(sstackptr[0].str, &dcb);
				SetCommState(hComm, &dcb);
			}
			if (p2 >= 0) {
				timeouts.ReadIntervalTimeout = 50;				/* ms Between chars */
				timeouts.ReadTotalTimeoutMultiplier = 1;		/* ms per char*/
				timeouts.ReadTotalTimeoutConstant = p2;		/* ms overhead -- may be total time before abandoning read also */
				timeouts.WriteTotalTimeoutMultiplier = 1;		/* ms per char	*/
				timeouts.WriteTotalTimeoutConstant = 10;		/* ms overhead */
				SetCommTimeouts(hComm, &timeouts);
			}
			/* Keep track of all the open file and file handles */
			if (rcode > 0) {
				/* See if already in the list */
				for (i=0; i<MAX_OPEN_COM; i++) { if (fd_list[i].valid && fd_list[i].fd == rcode) break; }
				/* If we don't already have the entry, just take first unused entry */
				if (i >= MAX_OPEN_COM) {
					for (i=0; i<MAX_OPEN_COM; i++) { if (! fd_list[i].valid) break; }
				}
				/* If there is an entry, fill in the parameters */
				if (i < MAX_OPEN_COM) {
					fd_list[i].valid = TRUE;
					fd_list[i].fd    = (int) rcode;
					fd_list[i].port  = port;
					fd_list[i].hComm = hComm;
				}
			}
			ipop = 1;
			break;

		case set_baud_me:
			/* Find the correct entry so have the hComm */
			for (i=0; i<MAX_OPEN_COM; i++) { if (fd_list[i].valid && fd_list[i].fd == fd) break; }
			if (i < MAX_OPEN_COM && fd_list[i].hComm != INVALID_HANDLE_VALUE && sstackptr[0].str != NULL) {
				FillMemory(&dcb, sizeof(dcb), 0); 
				dcb.DCBlength = sizeof(dcb);
				GetCommState(fd_list[i].hComm, &dcb);							/* Get existing state and just modify */
				BuildCommDCB(sstackptr[0].str, &dcb);
				rcode = SetCommState(fd_list[i].hComm, &dcb) ? 0 : 1 ;
			} else {
				rcode = -1;
			}
			ipop = 1;
			break;

		case get_baud_me:
			/* Find the correct entry so have the hComm */
			for (i=0; i<MAX_OPEN_COM; i++) { if (fd_list[i].valid && fd_list[i].fd == fd) break; }
			dcb.DCBlength = sizeof(dcb);
			if (i < MAX_OPEN_COM && fd_list[i].hComm != INVALID_HANDLE_VALUE && GetCommState(fd_list[i].hComm, &dcb) != 0) {
				sprintf(msg, "baud=%d parity=%c data=%d stop=%s xon=%s", dcb.BaudRate, 
						  parity[dcb.Parity], dcb.ByteSize, stop[dcb.StopBits], dcb.fOutX?"on":"off");
			} else {
				strcpy(msg, "ERROR: No communication parameters available");
			}
			sstackptr--;
			sstackptr->str = strdup(msg);
			sstackptr->status = IS_MALLOC;
			ipop = 0;
			break;

		case set_timeout_me:
			/* Find the correct entry so have the hComm */
			for (i=0; i<MAX_OPEN_COM; i++) { if (fd_list[i].valid && fd_list[i].fd == fd) break; }
			if (i < MAX_OPEN_COM && fd_list[i].hComm != INVALID_HANDLE_VALUE && GetCommTimeouts(fd_list[i].hComm, &timeouts) != 0) {
				if (p1 >= 0) timeouts.ReadIntervalTimeout = p1;	/* ms Between chars */
				if (p2 >= 0) timeouts.ReadTotalTimeoutMultiplier = p2;		/* ms per char*/
				if (p3 >= 0) timeouts.ReadTotalTimeoutConstant = p3;		/* ms overhead -- may be total time before abandoning read also */
				if (p4 >= 0) timeouts.WriteTotalTimeoutMultiplier = p4;	/* ms per char	*/
				if (p5 >= 0) timeouts.WriteTotalTimeoutConstant = p5;		/* ms overhead */
				rcode = SetCommTimeouts(fd_list[i].hComm, &timeouts) ? 0 : 1 ;
			} else {
				rcode = -1;
			}
			ipop = 0;
			break;

		case get_timeout_me:
			/* Find the correct entry so have the hComm */
			for (i=0; i<MAX_OPEN_COM; i++) { if (fd_list[i].valid && fd_list[i].fd == fd) break; }
			if (i < MAX_OPEN_COM && fd_list[i].hComm != INVALID_HANDLE_VALUE && GetCommTimeouts(fd_list[i].hComm, &timeouts) != 0) {
				if (p1 < 0) {
					TTYprintf(" ReadIntervalTimeout:         %d\n" 
								 " ReadTotalTimeoutMultiplier:  %d\n" 
								 " ReadTotalTimeoutConstant:    %d\n" 
								 " WriteTotalTimeoutMultiplier: %d\n" 
								 " WriteTotalTimeoutConstant:   %d\n", 
								 timeouts.ReadIntervalTimeout, timeouts.ReadTotalTimeoutMultiplier, timeouts.ReadTotalTimeoutConstant,
								 timeouts.WriteTotalTimeoutMultiplier, timeouts.WriteTotalTimeoutConstant);
					rcode = 0;
				} else switch (p1) {
					case 0: rcode = timeouts.ReadIntervalTimeout;			break;
					case 1: rcode = timeouts.ReadTotalTimeoutMultiplier;	break;
					case 2: rcode = timeouts.ReadTotalTimeoutConstant;		break;
					case 3: rcode = timeouts.WriteTotalTimeoutMultiplier;	break;
					case 4: rcode = timeouts.WriteTotalTimeoutConstant;	break;
					default: rcode = -1;
				}
			} else {
				ERRprintf("ERROR: No communication parameters available\n");
				rcode = -1;
			}
			ipop = 0;
			break;
			
		default:
			ERRprintf("AARGH! Low I/O function failed -- scream at developers\n");
			rcode = -1;
			break;
	}

	while (ipop--) {
		if (sstackptr->status == IS_MALLOC) free(sstackptr->str); 
		sstackptr++;
	}

	return(rcode);
}
#endif		/* NT */

/* ===========================================================================
-- Routine to handle the ctype include function calls.  Makes duplicating code 
-- between REAL and COMPLEX unnecessary.
--
-- Usage: HandleCtypeFnc(unsigned int cmd, int iarg)
--
-- Inputs: cmd  - the command code (switch value)
--         iarg - integer (char) argument to the functions
--
-- Returns: return value of the corresponding C function
--
-- Handles:  ctype_xxxxx where xxxx is isalnum() through isxdigit() plus
--           tolower() and toupper()
=========================================================================== */
static int HandleCtypeFnc(unsigned int cmd, int iarg) {
	int rc;
	switch (cmd) {
		case ctype_isalnum:
			rc = isalnum(iarg); break;
		case ctype_isalpha:
			rc = isalpha(iarg); break;
		case ctype_iscntrl:
			rc = iscntrl(iarg); break;
		case ctype_isdigit:
			rc = isdigit(iarg); break;
		case ctype_isgraph:
			rc = isgraph(iarg); break;
		case ctype_islower:
			rc = islower(iarg); break;
		case ctype_isprint:
			rc = isprint(iarg); break;
		case ctype_ispunct:
			rc = ispunct(iarg); break;
		case ctype_isspace:
			rc = isspace(iarg); break;
		case ctype_isupper:
			rc = isupper(iarg); break;
		case ctype_isxdigit:
			rc = isxdigit(iarg); break;
		case ctype_tolower:
			rc = tolower(iarg); break;
		case ctype_toupper:
			rc = toupper(iarg); break;
		default:
			ERRprintf("ERROR: Programmer screwup in HandleCtypeFnc()\n");
			rc = 1;
	}
	return rc;
}

/* ===========================================================================
-- Routine to handle the string subroutine calls.  Makes duplicating code 
-- between REAL and COMPLEX unnecessary.  Don't expect (want) string calls 
-- within array, so overhead of the call is irrelevent.
--
-- Usage: HandleStringFnc(unsigned int mycmd, INT64 p1, INT64 p2, INT64 p3, TMPREAL r1) {
--
-- Inputs: mycmd - the command code (switch value)
--         p1  - first integer required by some fncs
--         p2  - second integer required by some fncs
--         p3  - third integer required by some fncs
--         r1  - first real value required by some fncs
--
-- Returns: string argument on the stack
--
-- Handles:  strcmp_me, stricmp_me, strlen_me, strnlen_me, strcspn_me,
--           strspn_me, strncmp_me, strnicmp_me, lexequal_me
=========================================================================== */
#define	TMPSTR_SIZE	(32768)
PRIVATE int HandleStringFnc(unsigned int mycmd, INT64 p1, INT64 p2, INT64 p3, TMPREAL r1) {

	char *aptr, *bptr, tmpstr[TMPSTR_SIZE];
	char *mode, *fname;
	FILE *funit;
	FILE **pfunit;
	int ipop=0;
	const struct tm *tstruct;
	time_t lt1;

/* Logically separate the stack operations from string returning operations */
	switch (mycmd) {
		case pop_string_stack:
			while (p1--) {
				ssubptr--;
				if (sstack_public[ssubptr].status == IS_MALLOC) free(sstack_public[ssubptr].str);
			}
			return(0);

		case save_string_stack_val:
			sstack_public[ssubptr++] = *sstackptr;
			sstackptr++;
			return(0);

		case load_string_stack_val:			/* Point to a local string argument */
			sstackptr--;
			sstackptr->status = IS_STATIC;	/* Release handled by POP later		*/
			sstackptr->str  = sstack_public[p1].str;
			return(0);
	}

			
/* All other possibilities handled here */
	switch (mycmd) {
		case ctime_me:
			lt1 = p1;
			if (lt1 == -1) lt1 = time(NULL);
			aptr = strdup(ctime(&lt1));
			if ( (bptr = strchr(aptr, '\n')) != NULL) *bptr = '\0';
			ipop = -1;
			break;

		case strftime_me:
			lt1 = p1;
			if (lt1 == -1) lt1 = time(NULL);				
			tstruct = localtime(&lt1);
			strftime(tmpstr, TMPSTR_SIZE, sstackptr[0].str, tstruct);
			aptr = strdup(tmpstr);
			ipop = 0;
			break;

/* On pwd, translate all backslashes to forward slashes */
		case pwd_me:
			ipop = -1;								/* Actually open a space */
			aptr = malloc(PATH_MAX);
			if (getcwd(aptr, PATH_MAX) == NULL) strcpy(aptr, "<unable to determine>");
			while ( (bptr = strchr(aptr, '\\')) != NULL) *bptr = '/';
			break;

		case getenv_me:
			ipop = 0;								/* Space neutral */
			aptr = getenv(sstackptr[0].str);
			aptr = strdup((aptr != NULL) ? aptr : "");
			break;

		case fullpath_me:
			ipop = 0;								/* Space neutral */
			aptr = malloc(PATH_MAX);
			if (SysQualifyPath(aptr, sstackptr[0].str, PATH_MAX) == NULL) strcpy(aptr, "<unable to determine>");
			break;

		case rexx_char:
			ipop = -1;								/* Actually open a space */
			aptr = REXX_char((int) p1);
			break;

		case rexx_concat:
			aptr = REXX_concat(sstackptr[1].str, sstackptr[0].str);
			ipop = 1;
			break;

		case rexx_upcase:
			aptr = REXX_upcase(sstackptr->str);
			ipop = 0;
			break;
		case rexx_lowercase:
			aptr = REXX_lowercase(sstackptr->str);
			ipop = 0;
			break;
		case rexx_word:
			aptr = REXX_subword(sstackptr->str, (int) p1, 1);
			ipop = 0;
			break;
		case rexx_subword:
			aptr = REXX_subword(sstackptr->str, (int) p1, (int) p2);
			ipop = 0;
			break;
		case rexx_space:
			aptr = REXX_space(sstackptr->str, (int) p1, (int) p2);
			ipop = 0;
			break;
		case rexx_justify:
			aptr = REXX_justify(sstackptr->str, (int) p1, (int) p2);
			ipop = 0;
			break;
		case rexx_delstr:
			aptr = REXX_delstr(sstackptr->str, (int) p1, (int) p2);
			ipop = 0;
			break;
		case rexx_delword:
			aptr = REXX_delword(sstackptr->str, (int) p1, (int) p2);
			ipop = 0;
			break;
		case rexx_xrange:
			aptr = REXX_xrange((int) p1, (int) p2);
			ipop = -1;
			break;
		case rexx_translate:
			aptr = REXX_translate(sstackptr[2].str, sstackptr[1].str, sstackptr[0].str, (int) p1);
			ipop = 2;
			break;
		case rexx_substr:
			aptr = REXX_substr(sstackptr->str, (int) p1, (int) p2, (int) p3);
			ipop = 0;
			break;
		case rexx_insert:
			aptr = REXX_insert(sstackptr[1].str, sstackptr[0].str, (int) p1, (int) p2, (int) p3);
			ipop = 1;
			break;
		case rexx_overlay:
			aptr = REXX_overlay(sstackptr[1].str, sstackptr[0].str, (int) p1, (int) p2, (int) p3);
			ipop = 1;
			break;
		case rexx_strip:
			aptr = sstackptr[0].str;
			if (aptr == NULL) {
				p2 = 0x03;
			} else if (*aptr == 'L' || *aptr == 'l') {		/* Leading */
				p2 = 0x01;
			} else if (*aptr == 'T' || *aptr == 't') {		/* Trailing */
				p2 = 0x02;
			} else if (*aptr == 'B' || *aptr == 'b') {		/* Both */
				p2 = 0x03;
			} else if (*aptr == 'A' || *aptr == 'a') {		/* All */
				p2 = 0x04;
			} else {
				p2 = 0x03;												/* Default is BOTH */
			}
			aptr = REXX_strip(sstackptr[1].str, (int) p2, (int) p1);		/* Note reversed p2/p1 */
			ipop = 1;
			break;
		case rexx_reverse:
			aptr = REXX_reverse(sstackptr->str);
			ipop = 0;
			break;
		case rexx_left:
			aptr = REXX_left(sstackptr->str, (int) p1, (int) p2);
			ipop = 0;
			break;
		case rexx_right:
			aptr = REXX_right(sstackptr->str, (int) p1, (int) p2);
			ipop = 0;
			break;
		case rexx_center:
			aptr = REXX_center(sstackptr->str, (int) p1, (int) p2);
			ipop = 0;
			break;
		case rexx_copies:
			aptr = REXX_copies(sstackptr->str, (int) p1);
			ipop = 0;
			break;
		case int2hex_me:
			aptr = my_int2base(p1, 16);
			ipop = -1;
			break;
		case int2oct_me:
			aptr = my_int2base(p1, 8);
			ipop = -1;
			break;
		case int2bin_me:
			aptr = my_int2base(p1, 2);
			ipop = -1;
			break;
		case int2base_me:
			aptr = my_int2base(p1, (int) p2);
			ipop = -1;
			break;

		case float2hex_me:
			aptr = my_float2hex(r1);
			ipop = -1;
			break;
		case double2hex_me:
			aptr = my_double2hex(r1);
			ipop = -1;
			break;

		case hex2bin_me:
			aptr = my_hex2bin(sstackptr->str);
			ipop = 0;
			break;
		case bin2hex_me:
			aptr = my_bin2hex(sstackptr->str);
			ipop = 0;
			break;

		case bin_or_me:					/* Bitwise operations on strings */
		case bin_and_me:
		case bin_xor_me:
			aptr = my_bit_work(sstackptr[0].str, sstackptr[1].str, mycmd-bin_or_me);
			ipop = 1;
			break;
		case hex_or_me:
		case hex_and_me:
		case hex_xor_me:
			aptr = my_hex_work(sstackptr[0].str, sstackptr[1].str, mycmd-hex_or_me);
			ipop = 1;
			break;

		case fopen_me:
			mode   = (sstackptr[0].str == NULL) ? "r" : sstackptr[0].str;
			fname  = sstackptr[1].str;
			if (stricmp(fname, "stdout") == 0) {			/* Default names */
				funit = stdout;
			} else if (stricmp(fname, "stdin") == 0) {
				funit = stdin;
			} else if (stricmp(fname, "stderr") == 0) {
				funit = stderr;
			} else if ( (stricmp(fname, "/dev/tty") == 0) || (stricmp(fname, "con") == 0) ) {
				funit = (strpbrk(mode, "rR") != NULL) ? stdin : stdout ;
			} else {
				funit = fopen(fname, mode);
			}
			if (funit == NULL) ERRprintf("ERROR: File %s failed to open in mode %s.\n", fname, mode);
			POP_SSTACK;												/* Release mode */
			POP_SSTACK;												/* Release filename */
			sstackptr--;											/* Get this back */
			sstackptr->str  = (CHAR *) funit;				/* And return on stack	*/
			sstackptr->status = IS_FUNIT;						/* But better NOT!!!		*/
			return(0);

		case popen_me:
			funit = popen(sstackptr[1].str, (sstackptr[0].str == NULL) ? "r" : sstackptr[0].str);
			if (funit == NULL)
				ERRprintf("ERROR: Pipe request %s failed to open in mode %s\n", sstackptr[1].str, sstackptr[0].str);
			POP_SSTACK;												/* Release mode */
			POP_SSTACK;												/* Release filename */
			sstackptr--;											/* Get this back */
			sstackptr->str  = (CHAR *) funit;				/* And return on stack	*/
			sstackptr->status = IS_FUNIT;						/* But better NOT!!!		*/
			return(0);

		case fgets_me:
			pfunit = (FILE **) sstackptr[0].str;	funit = *pfunit;
			*tmpstr = '\0';
			if (funit != NULL) {
				fgets(tmpstr, sizeof(tmpstr), funit);
				while ( (aptr = strchr(tmpstr, '\n')) != NULL) *aptr = '\0';
			}
			aptr = strdup(tmpstr);
			ipop = 0;
			break;
			
		default:
			ERRprintf("AARGH! String/string function failed -- scream at developers\n");
			return(-1);
	}

	if (ipop > 0) {
		while (ipop--) {
			if (sstackptr->status == IS_MALLOC) free(sstackptr->str); 
			sstackptr++;
		}
	} else if (ipop < 0) {
		while (ipop++) {
			sstackptr--;
			sstackptr->status = IS_STATIC;
		}
	}

/* Okay, possibly delete the current and replace with this value */
	if (sstackptr->status == IS_MALLOC) free(sstackptr->str);
	sstackptr->str  = aptr;
	sstackptr->status = IS_MALLOC;
	return(0);
}

#if 0


	STRING = char(asciicharcode)									Encode as a 1 char string
	STRING = concat(str1, str2, ...)								Combine strings
   BOOL   = abbrev(long_str,short_str,min_match_len)		Is it an abbreviation?
   STRING = copies(short_str, n_copies)                  Duplicate N times
   STRING = d2x(decimal_number)									Convert to hex notation
   INT    = x2d(hexstring)
   INT    = length(string)                               Just alias to strlen
   STRING = left(string , new_length[,pad_character])    left/right justify string
   STRING = right(string, new_length[,pad_character])
   INT    = pos(needle,haystack[,start_search_position])
            strindex(needle_str, haystack_str)
   STRING = reverse(string)
   STRING = strip(long_string[,char_to_strip])           left/right strip
   STRING = substr(string, start_posn, n_chars)          
   INT    = words(string)                                # of ' ' delimited words
   STRING = word(string, word_number)                    stripped of spaces
   INT    = wordpos(needle, haystack, start_word_number)
   STRING = subword(string, first, n_words)
   INT    = verify(string, "allowed chars", start_posn)  1st char NOT in allowed
   STRING = translate(old_string,new_chars,old_chars)    Replaces each old w/ new
	          UPCASE = translate(mixedstring,"ABCDEFGHIJKLMNOPQRSTUVWXYZ",
	                                         "abcdefghijklmnopqrstuvwxyz")
#endif

/* REXX functions */

/* ---------------------------------------------------------------------------
-- Uppercase or lowercase a string
--------------------------------------------------------------------------- */
static char *REXX_upcase(char *str) {
	char *rc;
	unsigned int i;

	rc = strdup(str);
	for (i=0; i<strlen(rc); i++) rc[i] = toupper(rc[i]);
	return(rc);
}

static char *REXX_lowercase(char *str) {
	char *rc;
	unsigned int i;
	
	rc = strdup(str);
	for (i=0; i<strlen(rc); i++) rc[i] = tolower(rc[i]);
	return(rc);
}
			
/* ---------------------------------------------------------------------------
-- Counts the number of words in a string
--------------------------------------------------------------------------- */
static int REXX_words(char *str) {
	int count=0;

	while (*str) {
		while (isspace(*str)) str++;
		if (*str == '\0') break;
		count++;										/* Have another word */
		while (*str && ! isspace(*str)) str++;
	}
	return(count);
}
		
/* ---------------------------------------------------------------------------
-- Returns word position where a particular string is found
--------------------------------------------------------------------------- */
static int REXX_wordpos(char *needle, char *str, int start) {
	int count=0, subcount;
	BOOL midword;
	char *aptr;										/* sub-pointer into needle */

/* First skip over start words in str */
	while (start > 0) {
		while (isspace(*str)) str++;			/* Skip leading spaces */
		if (! *str) return -1;					/* Nothing left, can't match */
		while (*str && ! isspace(*str)) str++;		/* Skip the word */
		start--; count++;
	}

/* Also, skip leading white space in needle */
	while (isspace(*needle)) needle++;
	
/* Okay, now start the comparison and search */
	while (*str) {
		while (isspace(*str)) str++;
		if (*str == '\0') return -1;
		aptr = needle;
		subcount = 0;
		while (TRUE) {
			while (*aptr && ! isspace(*aptr) && *aptr == *str) { aptr++;  str++; }		/* Jump over matching characters */
			midword = isspace(*aptr);										/* Are we possibly midword in needle */
			while (isspace(*aptr)) aptr++;								/* Skip trailing space of needle */
			if (*aptr == '\0' && (*str == '\0' || isspace(*str))) return count;
			if (*str == '\0') return -1;									/* At end, can't match ever */
			subcount++;															/* We've gone a word now in haystack */
			if (isspace(*str)) {												/* We are still matching okay - just multiple words */
				if (! midword) break;										/* Continue only if we were mid-word */
				while (isspace(*str)) str++;								/* Skip whitespace of this word and keep comparing */
			} else {																/* Just skip over rest of word */
				while (*str && ! isspace(*str)) str++;					/* Skip over rest of needle word */
				break;
			}
		}
		count += subcount;
	}
	return -1;
}

#if 0
		if (*str == '\0') break;
		if (count>=start && strncmp(str, szTmp, strlen(szTmp))== 0) {	/* Possible */
			str = str+strlen(szTmp);			/* Is this end of the word? */
			if (*str == '\0' || isspace(*str)) return(count);
		}
		count++;										/* Have another word */
		while (*str && ! isspace(*str)) str++;
	}
#endif
		

/* ---------------------------------------------------------------------------
-- Creates string consisting of sequential characters 
--------------------------------------------------------------------------- */
static char *REXX_xrange(int start, int end) {

	char rc[257], *aptr;
	int i;

	start = start & 0xFF;							/* Keep within range [0,256] */
	end   = end   & 0xFF;							/* Keep within range [0,256] */

	aptr = rc;
	if (start <= end) {
		for (i=start; i<=end; i++) {
			if (i != 0) *(aptr++) = (char) i;
		}
	} else {
		for (i=start; i>=end; i--) {
			if (i != 0) *(aptr++) = (char) i;
		}
	}
	*aptr = '\0';
	return strdup(rc);
}

/* ---------------------------------------------------------------------------
-- Deletes substring of words from string 
--------------------------------------------------------------------------- */
static char *REXX_delword(char *str, int n, int length) {

	char *rc;
	int i,j, len;

	if (n < 0) n = 0;														/* Starting word to delete */
	
/* Length is zero, don't delete anything */
	i = REXX_wordindex(str, n);										/* On what character does How many words in string */
	if (i < 0 || length == 0) return strdup(str);				/* Nothing to delete */

	if (length < 0 || (j=REXX_wordindex(str, n+length)) < 0) {
		rc = malloc(i+1);
		memcpy(rc, str, i);
		rc[i] = '\0';
	} else { 
		len = (int) strlen(str);
		rc = malloc(len-(j-i)+1);
		memcpy(rc, str, i);												/* Copy up to 1 deleted word start */
		memcpy(rc+i, str+j, len-j+1);									/* Includes the terminating '\0' */
	}
	return rc;
}

/* ---------------------------------------------------------------------------
-- Deletes substring from string 
--------------------------------------------------------------------------- */
static char *REXX_delstr(char *str, int n, int length) {

	char *rc;
	int lenmax;

	if (n < 0) n = 0;														/* Starting character to delete */
	lenmax = (int) strlen(str);										/* Current length of string */
	if (n >= lenmax || length == 0) return strdup(str);		/* Nothing gets deleted */

	if (length < 0 || n+length > lenmax) {							/* Just keep first n characters */
		rc = malloc(n+1);
		memcpy(rc, str, n);
		rc[n] = '\0';
		return rc;
	} else {																	/* Split copy */
		rc = malloc(lenmax-length+1);									/* Final size */
		memcpy(rc, str, n);
		memcpy(rc+n, str+n+length, lenmax-length-n+1);			/* Includes the terminating '\0' */
	}
	return rc;
}

/* ---------------------------------------------------------------------------
-- Creates string of words with uniform spacing between words 
--------------------------------------------------------------------------- */
static char *REXX_space(char *str, int n, int pad) {

	char *rc, *aptr, *bptr;
	int i,chars,words;

	if (n < 0) n = 1;
	if (pad <= 0) pad = ' ';

	chars = words = 0;
	aptr = str;
	while (*aptr) {
		while (isspace(*aptr)) aptr++;		/* Skip whitespace */
		if (aptr == '\0') break;
		words++;
		while (*aptr && ! isspace(*aptr)) { aptr++; chars++; }
	}
	if (words == 0) return strdup("");

	aptr = str;
	bptr = rc = malloc(chars+(words-1)*n+1);
	words = 0;
	while (*aptr) {
		while (isspace(*aptr)) aptr++;		/* Skip whitespace */
		if (*aptr == '\0') break;
		if (words != 0) {
			for (i=0; i<n; i++) *(bptr++) = pad;
		}
		words++;
		while (*aptr && ! isspace(*aptr)) { *(bptr++) = *(aptr++); }
	}
	*bptr = '\0';
	return rc;
}

/* ---------------------------------------------------------------------------
-- Inserts substring into existing string
--------------------------------------------------------------------------- */
static char *REXX_insert(char *new, char *target, int start, int length, int pad) {

	char *rc;
	int oldlen, newlen, need;

/* Determine length of the two strings */
	oldlen = (int) strlen(target);
	newlen = (int) strlen(new);

/* And deal with default values */
	if (start < 0) start = 0;
	if (length < 0) length = newlen;
	if (pad <= 0) pad = ' ';

/* Create a string to hold the final result which must be of exactly length */
	need = max(start,oldlen)+length;
	rc = malloc(need+1);
	memset(rc, pad, need);
	rc[need] = '\0';
	if (start != 0) memcpy(rc, target, min(start,oldlen));
	if (newlen > 0 && length > 0) memcpy(rc+start, new, min(length,newlen));
	if (start < oldlen) memcpy(rc+start+length, target+start, oldlen-start);

	return rc;
}

/* ---------------------------------------------------------------------------
-- Overlay substring into existing string
--------------------------------------------------------------------------- */
static char *REXX_overlay(char *new, char *target, int start, int length, int pad) {

	char *rc;
	int oldlen, newlen, need;

/* Determine length of the two strings */
	oldlen = (int) strlen(target);
	newlen = (int) strlen(new);

/* And deal with default values */
	if (start < 0) start = 0;
	if (length < 0) length = newlen;
	if (pad <= 0) pad = ' ';

/* Create a string to hold the final result which must be of exactly length */
	need = max(oldlen, start+length);
	rc = malloc(need+1);
	memset(rc, pad, need);
	rc[need] = '\0';

	if (start != 0) memcpy(rc, target, min(start,oldlen));
	if (newlen > 0 && length > 0) memcpy(rc+start, new, min(length,newlen));
	if (start+length < oldlen) memcpy(rc+start+length, target+start+length, oldlen-start-length);
	return rc;
}

/* ---------------------------------------------------------------------------
-- Creates string of words filling the entire length defined, padded with pad
--------------------------------------------------------------------------- */
static char *REXX_justify(char *str, int length, int pad) {

	char *rc, *aptr, *dest, *src;
	int i,j,words,extra,spaces_used;
	BOOL afterspace;

	if (length <= 0) return strdup("");
	if (pad <= 0) pad = ' ';
	
/* Create a string to hold the final result which must be of exactly length */
	rc = malloc(length+1);

/* Skip leading whitespace and deal with an empty string possibility */
	aptr = str;
	while (isspace(*aptr)) aptr++;		/* Skip leading whitespace */
	if (*aptr == '\0') {						/* Empty string ... just set to pad and return */
		memset(rc, pad, length);
		rc[length+1] = '\0';
		return rc;
	}

/* Copy only as much as could possibly fit within length, counting the number of words */
	words = 0;									/* At least one word in pattern */
	afterspace = TRUE;
	for (i=0; i<length; i++) {
		if (*aptr == '\0') {					/* Past the characters in string, blank pad */
			rc[i] = ' ';						
		} else if (isspace(*aptr)) {		/* On separator between words */
			while (isspace(*aptr)) aptr++;
			rc[i] = ' ';
			afterspace = TRUE;
		} else {
			if (afterspace) words++;
			rc[i] = *(aptr++);
			afterspace = FALSE;
		}
	}
	rc[length] = '\0';								/* Terminate the string */
	
/* Check if we just exactly fit.  That will be easy */
	if (! isspace(rc[length-1]) && (*aptr == '\0' || isspace(*aptr))) {
		if (pad != ' ') {
			while ( (aptr=strchr(rc, ' ')) != NULL) *aptr = pad;
		}
		return rc;
	} 

/* Check to see if we've transferred a partial word.  If so, erase it */
	if (! isspace(rc[length-1])) {			/* Partial word transferred */
		for (i=length-1; i>=0; i--) {
			if (rc[i] == ' ') break;
			rc[i] = ' ';
		}
		words--;										/* Dropped the partial word */
	}

/* Count how many blanks we have at the end of the string */
	for (extra=0, aptr=rc+length-1; isspace(*aptr); aptr--) extra++;

/* Now, distribute if there are any and if the number of words is more than 1 */
	if (extra != 0 && words > 1) {				/* Do we have extra to distribute? */
		memmove(rc+extra, rc, length-extra);	/* Right justify now */
		src = rc+extra; dest = rc;
		spaces_used = 0;
		for (i=0; i<words; i++) {
			while (*src && *src != ' ') *(dest++) = *(src++);
			if (i == words-1) break;
			j = nint( (i+1.0)/(words-1.0)*(words-1.0+extra)) - spaces_used;
			spaces_used += j;
			while (j--) *(dest++) = ' ';
			src++;										/* Skip over the space in the source */
		}
	}
	rc[length] = '\0';
	if (pad != ' ') {
		while ( (aptr=strchr(rc, ' ')) != NULL) *aptr = pad;
	}
	return rc;
}

/* ---------------------------------------------------------------------------
-- Compares two strings returning index of mixmatch, or -1 if fully match
--------------------------------------------------------------------------- */
static int REXX_compare(char *s1, char*s2, char pad) {

	int rc;

	if (pad <= 0) pad = ' ';
	rc = 0;
	while (*s1 && *s2 && *s1==*s2) { s1++; s2++; rc++; }
	if (*s1 != '\0' && *s2 != '\0') return rc;					/* That's what we get */
	if (*s1 != '\0') {
		while (*s1 && *s1 == pad) { s1++; rc++; }
	} else {
		while (*s2 && *s2 == pad) { s2++; rc++; }
	}
	if (*s1 != '\0' || *s2 != '\0') return rc;					/* That's what we get */
	return -1;
}

/* ---------------------------------------------------------------------------
-- Retrieves starting character of the <start> word in string 
--------------------------------------------------------------------------- */
static int REXX_wordindex(char *str, int start) {

	char *aptr;

	if (start < 0) start = 0;
	aptr = str;
	while (*aptr) {
		while (isspace(*aptr)) aptr++;
		if (start == 0 && ! isspace(*aptr)) return (int) (aptr-str);
		start--;
		while (*aptr && ! isspace(*aptr)) aptr++;
	}
	return -1;
}

/* ---------------------------------------------------------------------------
-- Retrieves length of nth word in string, or returns 0 
--------------------------------------------------------------------------- */
static int REXX_wordlength(char *str, int start) {

	int rc;

	if ( (rc = REXX_wordindex(str, start)) < 0) return 0;
	str += rc;
	rc = 0;
	while (*str && ! isspace(*str)) { str++; rc++; }
	return rc;
}

/* ---------------------------------------------------------------------------
-- Retrieves a string of subwords from a string of words
--------------------------------------------------------------------------- */
static char *REXX_subword(char *str, int posn, int count) {

	char *start_posn, *rc;
	size_t ilen;

	if (posn < 0) posn = 0;
	if (count < 0) count = 9999;					/* Up to a thousand words as default */
	start_posn = str;									/* In case string is empty */
	while (*str && count > 0) {
		while (isspace(*str)) str++;				/* Skip leading space		*/
		if (posn == 0) start_posn = str;			/* At starting position		*/
		while (*str && ! isspace(*str)) str++;	/* Go over this word			*/
		posn--;											/* At next position			*/
		if  (posn < 0) count--;						/* And maybe counting wrds	*/
	}

/* Now, duplicate into a string and return */
	ilen = (posn < 0) ? str - start_posn : 0;		/* Handle case of too few words */
	rc = malloc(ilen+1);
	strncpy(rc, start_posn, ilen);
	rc[ilen] = '\0';
	return(rc);
}

/* ---------------------------------------------------------------------------
-- Returns position of first char in str (past posn) that is not in chars
--------------------------------------------------------------------------- */
static int REXX_verify(char *str, char *allowed, char *mode, int start) {

	int i,len;
	BOOL match, inref;

	if (start < 0) start = 0;									/* Require valid parameters */
	if (mode == NULL) mode = "nomatch";						/* Default value */
	match = (*mode == 'M') || (*mode == 'm');				/* Otherwise nomatch default */
	len = (int) strlen(str);
	if (*allowed == '\0') return match ? -1 : start;

	for (i=start; i<len; i++) {
		inref = (strchr(allowed, str[i]) != NULL);
		if (inref && match) return i;							/* Return first character that is in reference */
		if (!inref && ! match) return i;						/* Return first character that is not in reference */
	}
	return -1;
}

/* ---------------------------------------------------------------------------
-- Returns TRUE if every character in str past posn is in chars
--------------------------------------------------------------------------- */
static int REXX_abbrev(char *tok1, char *tok2, int minlen) {

	int len_1,len_2;
	
	len_1 = (int) strlen(tok1);
	len_2 = (int) strlen(tok2);
	if (minlen < 0) minlen = len_2;

	if (len_1 < minlen || len_2 < minlen || len_2 > len_1) {
		return 0;
	} else if (len_2 == 0) {
		return 1;
	} else if (memcmp(tok1, tok2, len_2) != 0) {
		return 0;
	} else if (len_1 != len_2) {				/* Match all but length */
		return 1;
	} else {											/* Complete match!! */
		return 2;
	}
}

/* ---------------------------------------------------------------------------
-- Returns translated string of all old characters to new characters
--------------------------------------------------------------------------- */
static char *REXX_translate(char *str, char *new, char *old, char pad) {
	char *rc, *aptr, *match;
	int i, imax;
	char dflt[256];

	if (pad > 0) {									/* Special conditions if pad is set */
		if (new == NULL) new = "";				/* Default "to" string is empty */
		if (old == NULL) old = dflt;			/* All characters are in the "from" set */
		for (i=0; i<255; i++) dflt[i] = i+1;
		dflt[255] = 0;
	} else if (new == NULL && old == NULL) {
		new = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		old = "abcdefghijklmnopqrstuvwxyz";
	} else if (old == NULL) {
		old = "abcdefghijklmnopqrstuvwxyz";
	} else if (new == NULL) {
		new = "";
	}
	if (pad <= 0) pad = ' ';					/* Default pad character is a space if unspecified */
	imax = (int) strlen(new);

/* Duplicate the string ... return on error */
	if ( (rc = strdup(str)) == NULL) return NULL;
	
/* Go through character by character ... replace old with new or pad if != 0 */
	for (aptr=rc; *aptr; aptr++) {
		if ( (match = strchr(old, *aptr)) != NULL) {
			if ( (i = (int) (match-old)) < imax) {
				*aptr = new[i];
			} else if (pad != 0) {
				*aptr = pad;
			}
		}
	}
	return rc;
}

/* ---------------------------------------------------------------------------
-- Returns substring of string 
--------------------------------------------------------------------------- */
static char *REXX_substr(char *str, int start, int n_chars, char pad) {
	char *rc;

	if ( ((size_t) start) > strlen(str)) start = (int) strlen(str);
	str += start;

/* Default is n_chars = -1 ==> use full length */
	if ( n_chars < 0) n_chars = (int) strlen(str);

/* Create the receiving string */
	rc = malloc(n_chars+1);	
	memset(rc, pad, (size_t) n_chars); rc[n_chars] = '\0';

/* Now, copy what is needed */
	if (n_chars > ((int) strlen(str))) n_chars = (int) strlen(str);
	if (n_chars > 0) memcpy(rc, str, n_chars);
	return(rc);
}

/* ---------------------------------------------------------------------------
-- Returns string stripped of some particular char at either end
-- Mode - 0x01 -> strip leading, 0x02 -> string trailing, 
--        0x03 -> strip both,    0x04 -> strip all
--------------------------------------------------------------------------- */
static char *REXX_strip(char *str, int mode, int achr) {

	char *aptr, *bptr, *rc;
	int ilen;
	
	if (achr == '\0') achr = ' ';						/* Default character */
	
/* Handle removing all occurances first - someways the easiest */
	if (mode == 4) {										/* Remove all occurances of string */
		for (ilen=0,aptr=str; *aptr; aptr++) if (*aptr != achr) ilen++;
		rc = malloc(ilen+1);
		for (aptr=str, bptr=rc; *aptr; aptr++) if (*aptr != achr) *(bptr++) = *aptr;
		*bptr = '\0';
		return rc;
	}

/* Do we strip leading occurances? */
	if (mode == 1 || mode == 3) {						/* Leading strip */
		while (*str == achr && *str) str++;			/* Strip from front */
	}

/* And do we strip trailing occurances */
	aptr = str+strlen(str);
	if (mode == 2 || mode == 3) {
		while (aptr != str && *(aptr-1) == achr) aptr--;
	}
	ilen = (int) (aptr-str);							/* Lenght of valid string */
	rc = malloc(ilen+1);
	strncpy(rc, str, ilen);
	rc[ilen] = '\0';

	return(rc);
}

/* ---------------------------------------------------------------------------
-- Returns reversed string
--------------------------------------------------------------------------- */
static char *REXX_reverse(char *str) {

	char *rc;
	int i,ilen;

	ilen = (int) strlen(str);
	rc = malloc(ilen+1);
	
	for (i=0; i<ilen; i++) rc[i] = str[ilen-i-1];
	rc[ilen] = '\0';
	return(rc);
}

/* ---------------------------------------------------------------------------
-- Returns position of needle in a haystack, starting after offset chars
--------------------------------------------------------------------------- */
static int REXX_pos(char *needle, char *haystack, int offset) {

	char *aptr;
	int rc;

	if ( ((size_t) offset) > strlen(haystack)) offset = (int) strlen(haystack);
	
	aptr = strstr(haystack+offset, needle);
	rc = (aptr == NULL) ? -1 : (int) (aptr-haystack);
	return(rc);
}

/* ---------------------------------------------------------------------------
-- Returns last position of needle in a haystack, starting optionally at start
--   returns the position of the last occurrence of one string, needle, in another,
--   haystack. (See also the POS function.) Returns 0 if needle is the null string
--   or is not found. By default the search starts at the last character of haystack
--   and scans backward. You can override this by specifying start, the point at
--   which the backward scan starts. start must be a positive whole number and
--   defaults to LENGTH(haystack) if larger than that value or omitted.
--
-- If start is negative, it becomes the default maximum length
--------------------------------------------------------------------------- */
static int REXX_lastpos(char *needle, char *haystack, int start) {

	int len_needle, len_haystack;

/* Get lengths of the strings */
	len_haystack = (int) strlen(haystack);							/* Actual length */
	len_needle   = (int) strlen(needle);							/* Actual length */

/* Simple tests - if cutting out more than we have, can't match */
	if (len_needle == 0) return -1;									/* Null string always fails */
	if (start < 0 || start >= len_haystack) start = len_haystack-1;
	if (start+len_needle > len_haystack-1)  start = (len_haystack-len_needle);
	if (start < 0) return -1;

/* Start looking backwards - do double test so first as first is very fast */
	while (start >= 0) {
		if (strncmp(haystack+start, needle, len_needle) == 0) return start;
		start--;
	}
	return -1;																/* Found nothing */
}

/* ---------------------------------------------------------------------------
-- Left justify a string (and pad as necessary)
--------------------------------------------------------------------------- */
static char *REXX_left(char *str, int length, int pad) {

	char *rc, *aptr;
	int i;

	if (length < 0) length = 0;
	if (pad == '\0') pad = ' ';
	rc = malloc(length+1);						/* Allocate and prefill */

	aptr = str;
	for (i=0; i<length; i++) {
		rc[i] = (*aptr != '\0') ? *aptr++ : pad ;
	}

	rc[length] = '\0';
	return(rc);
}


/* ---------------------------------------------------------------------------
-- Right justify a string (and pad as necessary)
--------------------------------------------------------------------------- */
static char *REXX_right(char *str, int length, int pad) {

	char *rc, *aptr;
	int i;

	if (length < 0) length = 0;
	if (pad == '\0') pad = ' ';
	rc = malloc(length+1);						/* Allocate and prefill */

	aptr = str+strlen(str);						/* Puts it at the \0 char */
	for (i=length; i>=0; i--) {
		rc[i] = (aptr >= str) ? *aptr-- : pad ;
	}

	rc[length] = '\0';
	return(rc);
}


/* ---------------------------------------------------------------------------
-- Center justify a string (and pad as necessary)
--------------------------------------------------------------------------- */
static char *REXX_center(char *str, int length, int pad) {

	char *rc;
	int i, ilen;

	if (length < 0) length = 0;
	if (pad == '\0') pad = ' ';
	rc = malloc(length+1);						/* Allocate and prefill */

	for (i=0; i<length; i++) rc[i] = pad;

	ilen = (int) strlen(str);
	if (length > ilen) {
		strncpy(rc+(length-ilen)/2, str, ilen);
	} else {
		strncpy(rc, str+(ilen-length)/2, length);
	}

	rc[length] = '\0';
	return(rc);
}

/* ---------------------------------------------------------------------------
-- Concat two strings
--------------------------------------------------------------------------- */
static char *REXX_concat(char *str1, char *str2) {

	char *rc;

	rc = malloc(strlen(str1)+strlen(str2)+1);
	strcat(strcpy(rc, str1), str2);
	return(rc);
}

/* ---------------------------------------------------------------------------
-- Create a character string from an integer
--------------------------------------------------------------------------- */
static char *REXX_char(int achr) {

	char *rc;

	rc = malloc(2);
	rc[0] = (char) achr;
	rc[1] = '\0';
	return(rc);
}

/* ---------------------------------------------------------------------------
-- Duplicates a character string n times
--------------------------------------------------------------------------- */
static char *REXX_copies(char *str, int copies) {

	char *rc;

	copies = max(copies,0);

	rc = malloc(copies*strlen(str)+1);
	*rc = '\0';
	while (copies--) strcat(rc, str);
	return(rc);
}

/* ---------------------------------------------------------------------------
-- Encodes a decimal number into hexadecimal format
-- Formerly REXX_d2x
--------------------------------------------------------------------------- */
static char *my_int2base(UINT64 ival, unsigned int base) {
	char *aptr, szTmp[128];
	int digit;
	
	if (ival == 0 || base < 2 || base > 36) return strdup("0");

	*(aptr = szTmp+sizeof(szTmp)-1) = '\0';
	do {
		digit = ival % base;
		digit = (digit < 10) ? digit+'0' : digit+'a'-10;
		*(--aptr) = (unsigned char) digit;
		ival /= base;
	} while (ival != 0);
	return strdup(aptr);
}

/* ---------------------------------------------------------------------------
-- Interprets a text string as an arbitrary base integer value.  String
-- is only interpreted as long as characters remain valid for the base.
--------------------------------------------------------------------------- */
static unsigned int my_base2int(char *str, unsigned base) {
	unsigned int rc, ival;

	if (base < 2 || base > 36) return 0;
	while (isspace(*str)) str++;
	if (strnicmp(str, "0x", 2) == 0) {	/* Allow 0xFFFF form */
		base = 16;
		str += 2;
	} else if (tolower(*str) == 'o') {	/* Allow o377 form */
		base = 8;
		str += 1;
	}

	for (rc=0; *str; str++) {
		ival = isdigit(*str) ? *str-'0' : tolower(*str)-'a'+10;
		if (ival < 0 || ival > base-1) break;
		rc = base*rc + ival;
	}
	return(rc);
}

/* ---------------------------------------------------------------------------
-- Routines to convert hex strings to binary and vice-versa.  Only as far
-- as the string is valid in both cases
--------------------------------------------------------------------------- */
static char *my_hex2bin(char *str) {
	char *aptr, szTmp[8192];
	int i, ihex;

	while (isspace(*str)) str++;			/* Skip leading spaces */
	aptr = szTmp;
	while (isxdigit(*str)) {
		ihex = isdigit(*str) ? *str-'0' : tolower(*str)-'a'+10;
		str++;
		for (i=3; i>=0; i--) { aptr[i] = ihex%2+'0'; ihex /= 2; }
		aptr += 4;
	}
	if (aptr == szTmp) *(aptr++) = '0';
	*aptr = '\0';

	return strdup(szTmp);
}

static char *my_bin2hex(char *str) {
	char *aptr, *bit, szTmp[2048];
	int i, ihex;

/* Find end of the binary string - leave bit as next char */
	while (isspace(*str)) str++;
	bit = str;
	while (*bit == '0' || *bit == '1') bit++;
	if (bit == str) return strdup("0");

	aptr = szTmp + sizeof(szTmp) - 1;
	*aptr = '\0';
	while (bit != str) {
		for (i=1,ihex=0; i<=8 && bit!=str; i*=2) ihex = ihex + i*(*(--bit)-'0');
		*(--aptr) = (ihex < 10) ? ihex + '0' : ihex + 'a' - 10;
	}

	return strdup(aptr);
}

/* ---------------------------------------------------------------------------
-- Routines for bit-wise and/or/xor as strings.  Next will be asked for hex.
-- op = 0 ==> OR   1 ==> AND   2 ==> XOR
--------------------------------------------------------------------------- */
static char *my_bit_work(char *str1, char *str2, int op) {
	char *aptr, *b1, *b2, szTmp[8192];
	int i1,i2;

/* Ignore leading white space */
	while (isspace(*str1)) str1++;
	while (isspace(*str2)) str2++;

/* Find end of the strings - leave at last char used */
	b1 = str1;	while (*b1 == '0' || *b1 == '1') b1++;
	b2 = str2;	while (*b2 == '0' || *b2 == '1') b2++;
	if (b1 == str1 && b2 == str2) return strdup("0");

	aptr = szTmp + sizeof(szTmp) - 1;
	*aptr = '\0';
	while (b1 != str1 || b2 != str2) {
		i1 = i2 = 0;
		if (b1 != str1) i1 = *(--b1) - '0';
		if (b2 != str2) i2 = *(--b2) - '0';
		*(--aptr) = ((op == 0) ? (i1 | i2) : (op == 1) ? (i1 & i2) : (i1 ^ i2)) + '0';
	}

	return strdup(aptr);
}

static char *my_hex_work(char *str1, char *str2, int op) {
	char *aptr, *b1, *b2, szTmp[8192];
	int i1,i2;

/* Ignore leading white space */
	while (isspace(*str1)) str1++;
	while (isspace(*str2)) str2++;

/* Find end of the strings - leave at last char used */
	b1 = str1;	while (isxdigit(*b1)) b1++;
	b2 = str2;	while (isxdigit(*b2)) b2++;
	if (b1 == str1 && b2 == str2) return strdup("0");

	aptr = szTmp + sizeof(szTmp) - 1;
	*aptr = '\0';
	while (b1 != str1 || b2 != str2) {
		i1 = i2 = 0;
		if (b1 != str1) { b1--; i1 = isdigit(*b1) ? (*b1-'0') : tolower(*b1)-'a'+10;}
		if (b2 != str2) { b2--; i2 = isdigit(*b2) ? (*b2-'0') : tolower(*b2)-'a'+10;}
		i1 = ((op == 0) ? (i1 | i2) : (op == 1) ? (i1 & i2) : (i1 ^ i2));
		*(--aptr) = (i1 < 10) ? i1 + '0' : i1 + 'a' - 10;
	}

	return strdup(aptr);
}

/* ---------------------------------------------------------------------------
-- Encodes a decimal number into hexadecimal format
--------------------------------------------------------------------------- */
static char *my_float2hex(TMPREAL rval) {
	char szTmp[128];
	union {
		INT32 ival;
		REAL32 rval;
	} u;
	u.rval = (REAL32) rval;
	sprintf(szTmp, "%8.8x", u.ival);
	return(strdup(szTmp));
}

/* ---------------------------------------------------------------------------
-- Interprets a number as a hexadecimal value -- no error return
--------------------------------------------------------------------------- */
static TMPREAL my_hex2float(char *str) {
	char seg1[9];
	union {
		UINT32 ival;
		REAL32 rval;
	} u;
	while (isspace(*str)) str++;
	if (strnicmp(str, "0x", 2) == 0) str += 2;	/* Allow 0xFFFF form */
	strncpy(seg1, str, 8); seg1[8] = '\0';			/* First 8 characters */
	u.ival = strtoul(seg1, NULL, 16);
	return u.rval;
}

/* ---------------------------------------------------------------------------
-- Encodes a decimal number into hexadecimal format
--------------------------------------------------------------------------- */
static char *my_double2hex(TMPREAL rval) {
	char szTmp[128];
	union {
		UINT32 ival[2];
		REAL64 dval;
	} u;
	u.dval = (REAL64) rval;
#if (BYTE_ENDIAN_ORDER == LITTLE_ENDIAN)
	sprintf(szTmp, "%8.8x%8.8x", u.ival[1],u.ival[0]);
#else
	sprintf(szTmp, "%8.8x%8.8x", u.ival[0],u.ival[1]);
#endif
	return(strdup(szTmp));
}

/* ---------------------------------------------------------------------------
-- Interprets a number as a hexadecimal value -- no error return
--------------------------------------------------------------------------- */
static TMPREAL my_hex2double(char *str) {
	char seg1[9], seg2[9];								/* Two 8-character segments */
	union {
		UINT32 ival[2];
		REAL64 dval;
	} u;

/* Copy into two strings so can be interpreted separately */
	while (isspace(*str)) str++;
	if (strnicmp(str, "0x", 2) == 0) str += 2;	/* Allow 0xFFFF form */
	strncpy(seg1, str, 8); seg1[8] = '\0';			/* First 8 characters */
	seg2[0] = '\0';
	if (strlen(str) > 8) { strncpy(seg2, str+8, 8); seg2[8] = '\0'; }

#if (BYTE_ENDIAN_ORDER == LITTLE_ENDIAN)			/* Pain in the BUTT */
	u.ival[1] = strtoul(seg1, NULL, 16);
	u.ival[0] = strtoul(seg2, NULL, 16);
#else
	u.ival[0] = strtoul(seg1, NULL, 16);
	u.ival[1] = strtoul(seg2, NULL, 16);
#endif
	return u.dval;
}

/* ---------------------------------------------------------------------------
-- Interprets a string as a time value (hh:mm:ss.s) - no error return
-- Allow only hour segments in 24 hour mode.
--------------------------------------------------------------------------- */
static TMPREAL my_time2double(char *str) {
	double rc, tmp, factor;
	enum {DIGITS, DECIMALS} mode;
	int icount;

	rc = tmp = 0;
	icount = 0;												/* Number of : seen */
	mode = DIGITS;											/* Working before decimal point */
	while (*str) {
		if (isdigit(*str)) {
			if (mode == DIGITS) {
				tmp = 10*tmp + (*str-'0');
			} else {
				factor *= 10.0;
				tmp = tmp + (*str-'0')/factor;
			}
		} else if (mode == DIGITS && *str == '.') {
			mode = DECIMALS;
			factor = 1.0;
		} else if (mode == DIGITS && icount < 2 && *str == ':') {
			icount++;
			rc = 60*rc + tmp;
			tmp = 0;
		} else {
			break;
		}
		str++;
	}

	rc = 60*rc + tmp;														/* Final end value */
	return rc;
}


/* ---------------------------------------------------------------------------
-- Handles the problem of printf! 
--
-- Encodes a string via a given format and series of arguments which have
-- been evaluated and placed on the appropriate stacks.  Have to backtrack
-- where these begin, etc.  Really painful!
-- Integers handled as the "nint" .
--
-- Usage:   do_sprintf(BOOL is_complex)
--
-- Inputs:  is_complex - are int/real numbers taken from rstackptr or cstackptr?
--
-- Output:  outstring filled with the converted string
--
-- Returns: nothing
--------------------------------------------------------------------------- */
TMPREAL do_sprintf(BOOL is_complex) {

	char			*out,*optr;								/* Output string pointer */
	int			out_len, out_alloc;
	char			myformat[256];							/* Local string for single var format */
	char			*strptr, *aptr;						/* Local pointers */
	int			VarType, VarModifier;				/* Type of encode, and potential modifier */
	int			i, ineed;
	long			il;

	TMPREAL		next_real, next_imag;				/* Specifically for this work */
	char		  *next_str, *format, *types;
	int			sargs, sarg, rargs, rarg;			/* Number of args/ arg point	*/
	BOOL			mismatch, used;

/* Count the number of each type argument */
	types = sstackptr->str;								/* "ssrrsrs" list of types */
	sargs = rargs = 0;
	for (i=0; types[i]!='\0'; i++) {
		if (types[i] == 's') sargs++;
		if (types[i] == 'r') rargs++;
	}
	format = sstackptr[sargs+1].str;					/* Pointer to format string */
	sarg = rarg = 0;										/* Currently in use arg		 */

/* Allocate the initial space */
	out_alloc = 4096;										/* Alloc 4096 initially */
	out_len   = 0;
	out = malloc(out_alloc);
	optr = out; *optr = '\0';							/* Start out blank */

/* Okay, now scan the format */
	mismatch = FALSE;
	while (*format) {										/* Scan entire structure */

		out_len = (int) (optr-out);					/* # of characters so far */
		if (out_alloc-out_len < 2048) {				/* Minimum of 2048 spaces */
			out_alloc += 2048;							/* Grow by a page at a time */
			out = realloc(out, out_alloc);
			optr = out+out_len;
		}

/* Format should not deal with escaped chars - let string manipulation do that */
#if 0
		if (*format == '\\') {
			format++;										/* Skip over the \ 	*/
			switch (*format) {							/* Which form?			*/
				case 'a':
					*(optr++) = '\a'; break;
				case 'b':
					*(optr++) = '\b'; break;
				case 'f':
					*(optr++) = '\f'; break;
				case 'n':
					*(optr++) = '\n'; break;
				case 'r':
					*(optr++) = '\r'; break;
				case 't':
					*(optr++) = '\t'; break;
				case 'v':
					*(optr++) = '\v'; break;
				case '\\':
					*(optr++) = '\\'; break;
				default:
					*(optr++) = '\\';
					*(optr++) = *format;
			}
			format++;										/* And have dealt with char */
		} else
#endif

		if (*format == '%') {
			format++;										/* Dealt with that char		*/
			if (*format == '%') {						/* %% makes %					*/
				*(optr++) = *(format++);
			} else {											/* Real format specifier	*/

				/* Load the values of number/string pointer and increment pointers */
				used = TRUE;								/* Do we use an argument?	*/
				next_real = is_complex ? cstackptr[rargs-rarg-1].x : rstackptr[rargs-rarg-1] ;
				next_imag = is_complex ? cstackptr[rargs-rarg-1].y : 0 ;
				next_str  = sstackptr[sargs-sarg].str;
				
				aptr = myformat;							/* Make the format string	*/
				VarType = VarModifier = '\0';			/* Assume no modifiers		*/
				*(aptr++) = '%';							/* Need the % back now		*/
				while (*format && strchr("+- #.0123456789", *format) != NULL) 
					*(aptr++) = *(format++);
				if (*format && strchr("lLh", *format) != NULL) 
					VarModifier = *(aptr++) = *(format++);
				if (*format) VarType = *(aptr++) = *(format++);
				*aptr = '\0';								/* Terminate my format		*/
				switch (VarType) {
					case 's':
						if (*types != 's') mismatch = TRUE;
						ineed = (int) strlen(next_str);
						out_len = (int) (optr-out);			/* # of characters so far */
						if (ineed+out_len >= out_alloc) {	/* Need more space */
							out_alloc += ineed;					/* Grow by needed amount */
							out = realloc(out, out_alloc);
							optr = out+out_len;
						}
						sprintf(optr, myformat, next_str);
						optr += strlen(optr);
						break;
					case 'p':
						if (*types != 's') mismatch = TRUE;
						sprintf(optr, myformat, next_str);
						optr += strlen(optr);
						break;
					case 'c':								/* character			*/
					case 'd':								/* decimal				*/
					case 'i':								/* integer				*/
					case 'o':								/* Octal constant		*/
					case 'x':								/* Hex constant		*/
					case 'X':
					case 'u':								/* Unsigned decimal	*/
						if (*types != 'r') mismatch = TRUE;
						il = nint(next_real);			/* Convert to long integer */
						if (VarModifier == 'l')
							sprintf(optr, myformat, il);
						else										/* For short and int */
							sprintf(optr, myformat, (int) il);
						optr += strlen(optr);
						break;

					case 'z':								/* complex notation */
					case 'Z':
						if (*types != 'r') mismatch = TRUE;
						aptr--;								/* Get the 'z' char */
						*aptr++ = 'g'-('z'-VarType);	/* Change from z to g format */
						strptr = aptr+1;
						if (myformat[1] != '+') *strptr++ = '+';
						strcpy(strptr, myformat+1);	/* Copy all but % symbol	*/
						*aptr = '%';						/* Replace first \0 with %	*/
						strcat(myformat, "j");			/* And add the char 'j'		*/

						if (VarModifier == 'L')
							sprintf(optr, myformat, next_real, next_imag);
						else
							sprintf(optr, myformat, next_real, next_imag);
						optr += strlen(optr);
						break;
					case 'e':								/* scientific notation */
					case 'E':
					case 'f':								/* Floating point notation */
					case 'g':								/* general notation */
					case 'G':
						if (*types != 'r') mismatch = TRUE;
						if (VarModifier == 'L') 
							sprintf(optr, myformat, next_real);
						else
							sprintf(optr, myformat, next_real);
						optr += strlen(optr);
						break;
					default:
						aptr = myformat; while (*aptr) *(optr++) = *(aptr++);
						used = FALSE;
				}
				if (used) {
					if (*types == 's' && sarg < sargs) sarg++;
					if (*types == 'r' && rarg < rargs) rarg++;
					types++;
				}
			}
		} else {
			*(optr++) = *(format++);
		}

	}
	*optr = '\0';

	if (  is_complex) cstackptr += rargs;
	if (! is_complex) rstackptr += rargs;
	for (i=0; i<sargs+2; i++) {POP_SSTACK;}		/* Pop everything */

	sstackptr--;
	sstackptr->str = strdup(out);
	sstackptr->status = IS_MALLOC;
	out_len = (int) (optr-out); free(out);

	if (mismatch) ERRprintf("WARNING: Format and argument mismatch - I faked it for you\n");
	
	return(out_len);
}

/* Routine to generate 48 bit random number sequences */
/* Only required if not in OS - NT and MAC */

/* ===========================================================================
drand48, erand48, jrand48, lcong48, lrand48, mrand48, nrand48, seed48, or srand48 Subroutine

Purpose

Generate uniformly distributed pseudo-random number sequences.

  double drand48 (void)

  double erand48 (xsubi)
  unsigned short int xsubi[3];

  void lcong48 (Parameter)
  unsigned short int Parameter[7];

  long int lrand48 (void)

  long int mrand48 (void)

  long int nrand48 (xsubi)
  unsigned short int xsubi[3];

  unsigned short int *seed48 (Seed16v)
  unsigned short int Seed16v[3];

  void srand48 (SeedValue)
  long int SeedValue;

Description

  Attention: Do not use the drand48, erand48, jrand48, lcong48, lrand48,
  mrand48, nrand48, seed48, or srand48 subroutine in a multithreaded
  environment. See the multithread alternatives in the drand48_r, erand48_r,  
  srand48_r subroutine article.

  This family of subroutines generates pseudo-random numbers using the
  linear congruential algorithm and 48-bit integer arithmetic.

  The drand48 subroutine and the erand48 subroutine return positive
  double-precision floating-point values uniformly distributed over
  the interval [0.0, 1.0).

  The lrand48 subroutine and the nrand48 subroutine return positive
  long integers uniformly distributed over the interval [0,2**31).

  The mrand48 subroutine and the jrand48 subroutine return signed long
  integers uniformly distributed over the interval [-2**31, 2**31).

  The srand48 subroutine, seed48 subroutine, and lcong48 subroutine
  initialize the random-number generator. Programs must call one of
  them before calling the drand48, lrand48 or mrand48 subroutines. (Although
  it is not recommended, constant default initializer values are supplied
  if the drand48, lrand48 or mrand48 subroutines are called without
  first calling an initialization subroutine.) The erand48, nrand48,
  and jrand48 subroutines do not require that an initialization subroutine

  The previous value pointed to by the seed48 subroutine is stored in
  a 48-bit internal buffer, and a pointer to the buffer is returned
  by the seed48 subroutine. This pointer can be ignored if it is not
  needed, or it can be used to allow a program to restart from a given
  point at a later time. In this case, the pointer is accessed to retrieve
  and store the last value pointed to by the seed48 subroutine, and
  this value is then used to reinitialize, by means of the seed48 subroutine,
  when the program is restarted.

  All the subroutines work by generating a sequence of 48-bit integer
  values, x[i], according to the linear congruential formula:

  x[n+1] = (ax[n] + c)mod m, n is > =*0

  The parameter m = 248; hence 48-bit integer arithmetic is performed.
  Unless the lcong48 subroutine has been called, the multiplier value
  a and the addend value c are:

  a = 5DEECE66D base 16 = 273673163155 base 8

  c = B base 16 = 13 base 8

  Parameters

    xsubi      Specifies an array of three shorts, which, when 
               concatenated together, form a 48-bit integer.

   SeedValue   Specifies the initialization value to begin randomization.
               Changing this value changes the randomization pattern.

   Seed16v     Specifies another seed value; an array of three unsigned 
               shorts that form a 48-bit seed value.

   Parameter   Specifies an array of seven shorts, which specifies the
               initial xsubi value, the multiplier value a and the 
               add-in value c.

Return Values

  The value returned by the drand48, erand48, jrand48, lrand48, nrand48,
  and mrand48 subroutines is computed by first generating the next 48-bit
  x[i] in the sequence. Then the appropriate number of bits, according
  to the type of data item to be returned, are copied from the high-order
  (most significant) bits of x[i] and transformed into the returned
  value.                                					 
  The drand48, lrand48, and mrand48 subroutines store the last 48-bit
  x[i] generated into an internal buffer; this is why they must be
  initialized prior to being invoked.

  The erand48, jrand48, and nrand48 subroutines require the calling
  program to provide storage for the successive x[i] values in the
  array pointed to by the xsubi parameter. This is why these routines
  do not have to be initialized; the calling program places the desired
  initial value of x[i] into the array and pass it as a parameter.

  By using different parameters, the erand48, jrand48, and nrand48 subroutines
  allow separate modules of a large program to generate independent
  sequences of pseudo-random numbers. In other words, the sequence of
  numbers that one module generates does not depend upon how many times
  the subroutines are called by other modules.

  The lcong48 subroutine specifies the initial x[i] value, the multiplier
  value a, and the addend value c. The Parameter array elements Parameter
  [0-2] specify x[i], Parameter[3-5] specify the multiplier a, and
  Parameter[6] specifies the 16-bit addend c. After lcong48 has been
  called, a subsequent call to either the srand48 or seed48 subroutine
  restores the standard a and c specified before.
  The initializer subroutine seed48 sets the value of x[i] to the
  48-bit value specified in the array pointed to by the Seed16v parameter.
  In addition, seed48 returns a pointer to a 48-bit internal buffer
  that contains the previous value of x[i] that is used only by seed48.
  The returned pointer allows you to restart the pseudo-random sequence
  at a given point. Use the pointer to copy the previous x[i] value
  into a temporary array. Then call seed48 with a pointer to this array
  to resume processing where the original sequence stopped.

  The initializer subroutine srand48 sets the high-order 32 bits of
  x[i] to the 32 bits contained in its parameter. The low order 16
  bits of x[i] are set to the arbitrary value 330E16.
=========================================================================== */
#ifdef NT

#include <windows.h>

#define	MASK48	(0xFFFFFFFFFFFFui64)		/* 48 bit mask pattern */
#define	MASK32	(0xFFFFFFFFui64)			/* 32 bit mask pattern */

static int first = 1;					/* For initialization	*/
static __int64
	x0,										/* Previous value			*/
	a=0x5DEECE66Dui64,					/* Multiplier				*/
	c=0xBui64,								/* Additive constant		*/
	mask32 = 0xFFFFFFFFui64;			/* 32 bit mask pattern	*/

static void Init48(void) {
	first = 0;
	x0 = ((((__int64) rand()) & MASK32) << 16) | 0x330Eui64;
	return;
}

static double drand48(void) {
	if (first) Init48();
	x0 = (a*x0 + c) & MASK48;				/* Mod 48 arithmetic */
	return ((double) x0) / ((double) MASK48);
}

static long int lrand48(void) {
	if (first) Init48();
	x0 = (a*x0 + c) & MASK48;				/* Mod 48 arithmetic */
	return  ((long int) ((x0 >> 8) & 0x7FFFFFFFui64));
}

static long int mrand48(void) {
	if (first) Init48();
	x0 = (a*x0 + c) & MASK48;				/* Mod 48 arithmetic */
	return  ((long int) ((x0 >> 8) & 0xFFFFFFFFui64));
}

static void srand48(long int SeedValue) {
	x0 = ((((__int64) SeedValue) & MASK32) << 16) | 0x330Eui64;
	first = 0;									/* Now initialized */
	return;
}

#elif defined MacInToy	/* NT */

static void srand48(long s) {
	srandom(s);
	return;
}

static double drand48(void) {
  return (double) random() / LONG_MAX;
}

static long lrand48(void) {
  return random();
}

static long mrand48(void) {
	return random();
}

#endif /* MAC_OSX */


/* ===========================================================================
-- Special code section for dealing with @solve() calls.  Basically does
-- a subroutine with embedded information.
=========================================================================== */
static GVCMDS *solve_cmds;

static TMPREAL solve_fnc_test(double x, int i, int *err) {
	return subfnc_eval(x, err);
}

static TMPREAL subfnc_eval(double x, int *err) {
	int rc;
	rstack_public[subptr-1] = (TMPREAL) x;
	rc = real_eval(solve_cmds);						/* Will be zero if all okay */
	if (err != NULL) *err = rc;
	return (double) *(rstackptr++);					/* Return and pop value		 */
}

static void z_subfnc_eval(double x, int *err, TMPCOMPLEX *zval) {
	int rc;
	cstack_public[subptr-1].x = (TMPREAL) x;
	cstack_public[subptr-1].y = (TMPREAL) 0;
	rc = complex_eval(solve_cmds);					/* Will be zero if all okay */
	if (err != NULL) *err = rc;
	*zval = *(cstackptr++);								/* Return and pop value		 */
	return;
}

/* ===========================================================================
-- Routine to integrate a function  between limits
--
-- Usage: int GVIntegrate(double *result, TMPREAL (*fnc), double, double b, double eps, int miniter, int maxiter);
--
-- Inputs: result - pointer to variable to receive the answer
--         fnc    - pointer to a function which returns value.
--                    TMPREAL fnc(double x, int *ierr)
--         a,b    - limits of the integration
--         eps    - desired fractional precision
--
-- Output: *result - value of the integral, or 0.0 if any errors
--
-- Return:  0 - successful
--         -1 - error from the function observed
--         -2 - failed to converge to eps precision within iteration limit
=========================================================================== */
static int Power2(int n) {
	int rc=1;
	while (n--) rc *= 2;
	return rc;
}

static int Trapezoid_Refine(TMPREAL *result, TMPREAL (*fnc)(double x, int *ierr), double a, double b, int level) {
	TMPREAL sum, dx;
	int i, ndivs, ierr;

	ndivs = Power2(level);		/* 2^n intervals */
	dx = (b-a)/ndivs;
	sum = 0.0;
	for (i=1; i<ndivs; i+=2) {
		sum += (*fnc)(a+dx*i,&ierr);
		if (ierr != 0) return -1;
	}
	*result = *result/2 + sum/ndivs*(b-a);
	return 0;
}

static int GVIntegrate(double *result, TMPREAL (*fnc)(double x, int *ierr), double a, double b, double eps, 
								int miniter, int maxiter) {
	TMPREAL sum, est, last_sum, last_est;
	int i, rc, ierr;

	/* Validate the paramters */
	if (miniter < 4) miniter = 4;				/* Will do at least 4 integrations */
	if (maxiter < 4) maxiter = 4;
	if (eps <= 0) eps = 1E-6;					/* Don't allow stupid numbers */

/* And start the integration */
	*result = 0.0;									/* Save invalid value */
	sum  = 0.5*(*fnc)(a,&ierr);		if (ierr != 0) return -1;	/* First values */
	sum += 0.5*(*fnc)(b,&ierr);		if (ierr != 0) return -1;
	sum *= (b-a);									/* Initial trapezoidal integral estimate */
	est  = sum;										/* Initial estimate of integral same     */
	rc   = 0;										/* And no errors */
	for (i=1; i<24; i++) {
		last_sum = sum;							/* Keep previous trapezoidal integral estimate		 */
		last_est = est;							/* Keep previous full (Simpson's) integral estimate */
		if (Trapezoid_Refine(&sum, fnc, a,b, i) != 0) return -1;
		est = 4.0*sum/3.0 - last_sum/3.0;	/* Current estimate of the integral (Simpson's)		*/
/*		printf("Sum: %.8g  Delta: %g  Frac: %g\n", sum, sum-last, (sum-last)/sum); */
/*		printf("Est: %.8g  Delta: %g  Frac: %g\n", est, est-last_est, (est-last_est)/est); */
		if (Power2(i) < miniter) continue;	/* Always do at least specified # of evaluations	*/
		if (fabs(est-last_est) <= eps*fabs(est+last_est)/2.0) break;
		if (Power2(i) >= maxiter) { 
			rc = -2; 
/*			TTYprintf("Nmin: %d  Nmax: %d  Eps: %g  Last two estimates: %g	%g\n", miniter, maxiter, eps, est, last_est);  */
			break;
		}
	}
	*result = (double) est;
	return rc;
}

/* ===========================================================================
-- Routine to integrate a function  between limits
--
-- Usage: int GVIntegrate_C(TMPCOMPLEX *result, void (*fnc), double, double b, double eps, int miniter, int maxiter);
--
-- Inputs: result - pointer to variable to receive the answer
--         fnc    - pointer to a function which returns value.
--                    TMPREAL fnc(double x, int *ierr)
--         a,b    - limits of the integration
--         eps    - desired fractional precision
--
-- Output: *result - value of the integral, or 0.0 if any errors
--
-- Return:  0 - successful
--         -1 - error from the function observed
--         -2 - failed to converge to eps precision within iteration limit
=========================================================================== */
static int Trapezoid_Refine_C(TMPCOMPLEX *result, void (*fnc)(double x, int *ierr, TMPCOMPLEX *rc), double a, double b, int level) {
	TMPCOMPLEX sum, val;
	double dx;
	int i, ndivs, ierr;

	ndivs = Power2(level);		/* 2^n intervals */
	dx = (b-a)/ndivs;
	sum.x = sum.y = 0.0;
	for (i=1; i<ndivs; i+=2) {
		(*fnc)(a+dx*i, &ierr, &val);
		sum.x += val.x; sum.y += val.y;
		if (ierr != 0) return -1;
	}
	result->x = result->x/2 + sum.x/ndivs*(b-a);
	result->y = result->y/2 + sum.y/ndivs*(b-a);
	return 0;
}

static int GVIntegrate_C(TMPCOMPLEX *result, void (*fnc)(double x, int *ierr, TMPCOMPLEX *rc), double a, double b, double eps, 
							  int miniter, int maxiter) {
	TMPCOMPLEX sum, val, est, last_sum, last_est;
	int i, rc, ierr;

	/* Validate the paramters */
	if (miniter < 4) miniter = 4;				/* Will do at least 4 integrations */
	if (maxiter < 4) maxiter = 4;
	if (eps <= 0) eps = 1E-6;					/* Don't allow stupid numbers */

/* And start the integration */
	result->x = result-> y = 0.0;				/* Save invalid value */
	/* Initial trapezoidal integral estimate */
	(*fnc)(a,&ierr,&sum);		if (ierr != 0) return -1;	/* First values */
	(*fnc)(b,&ierr,&val);		if (ierr != 0) return -1;
	sum.x = 0.5*(b-a)*(sum.x+val.x);	sum.y = 0.5*(b-a)*(sum.y+val.y);
	est  = sum;										/* Initial estimate of integral same     */
	rc   = 0;										/* And no errors */
	for (i=1; i<24; i++) {
		last_sum = sum;							/* Keep previous trapezoidal integral estimate		 */
		last_est = est;							/* Keep previous full (Simpson's) integral estimate */
		if (Trapezoid_Refine_C(&sum, fnc, a,b, i) != 0) return -1;
		est.x = 4.0*sum.x/3.0 - last_sum.x/3.0;	/* Current estimate of the integral (Simpson's)		*/
		est.y = 4.0*sum.y/3.0 - last_sum.y/3.0;
/*		printf("Sum: %.8g  Delta: %g  Frac: %g\n", sum, sum-last, (sum-last)/sum); */
/*		printf("Est: %.8g  Delta: %g  Frac: %g\n", est, est-last_est, (est-last_est)/est); */
		if (Power2(i) < miniter) continue;	/* Always do at least specified # of evaluations	*/
		if ( (fabs(est.x-last_est.x) <= eps*fabs(est.x+last_est.x)/2.0) &&
			  (fabs(est.y-last_est.y) <= eps*fabs(est.y+last_est.y)/2.0) ) break;
		if (Power2(i) >= maxiter) { 
			rc = -2; 
/*			TTYprintf("Nmin: %d  Nmax: %d  Eps: %g  Last two estimates: %g	%g\n", miniter, maxiter, eps, est, last_est);  */
			break;
		}
	}
	*result = est;
	return rc;
}

/* ===========================================================================
-- Routine to determine the Bessel functions of arbitrary order for real values
--
-- Usage: TMPREAL = JN_ME(nu,x)
--
-- Inputs: nu - order
--          x - argument
--
-- Output: none
--
-- Return:  value of the bessel function
=========================================================================== */
#if 0
setv n = 60
lt 1
pl -f abs(jn(n,x))/(abs(jn(n+1E-14,x))+1E-160)-1 -range 0.6 90

setv n = 2
lt 1
pl -f jn(n,x)-jn(n+1E-14,x) -range 0.1 60 -points 1000

#endif

static TMPREAL JN_ME(TMPREAL nu, TMPREAL z) {
	TMPREAL Cnu, Cnum1, Cnup1, tmp, rc;

/* Handle half integer cases first, and call JN_ME_LOW for -1<n<=1 */
	if (nu == -0.5) {
		return (z == 0) ? REAL_MAX : SQRT(2/PI/z)*COS(z);
	} else if (nu == 0.5) {
		return (z == 0) ? 0.0 : SQRT(2/PI/z)*SIN(z);
	} else if (nu >= 0  && nu < 2) {
		return JN_ME_LOW(nu,z);
	} else if (FABS(z) < FABS(nu)) {						/* Values is below 1E-5 - use expansion */
		return JN_ME_LOW(nu,z);
	}
	
/* Now, just general recursion.  Walk down or up to nu in (0,2) */
/* The function JN_ME_LOW then only has to handle 0<nu<2 */
/* Recursion is really only good for z>nu since otherwise canceling terms */
/* Still a concern about finite precision and roundoff errors */
	if (nu > 0) {												/* Walk down until nu < 2 */
		Cnu = 1; Cnum1 = 0;									/* Coefficients on J(nu,x) and J(nu-1,x) */ 
		while (nu >= 2.0) {
			nu = nu-1;
			tmp = Cnu;
			Cnu = Cnum1 + Cnu*2.0*nu/z;
			Cnum1 = -tmp;
		}
		rc = Cnu*JN_ME(nu,z);
		if (Cnum1 != 0) rc += Cnum1*JN_ME(nu-1,z);
	} else {
		Cnu = 1; Cnup1 = 0;									/* Coefficients on J(nu,x) and J(nu-1,x) */ 
		while (nu < 0) {
			nu = nu+1;
			tmp = Cnu;
			Cnu = Cnup1 + Cnu*2*nu/z;
			Cnup1 = -tmp;
		}
		rc = Cnu*JN_ME(nu,z);
		if (Cnup1 != 0) rc += Cnup1*JN_ME(nu+1,z);
	}
	return rc;
}

#define	JN_MAX_TERMS	50			/* Easily gets me to |x| < 20 */
static TMPREAL JN_ME_LOW(TMPREAL nu, TMPREAL z) {

	TMPREAL mu, P, Q, zp, zp2, chi, tmp, delta, zuse;
	int i, minterms, maxterms;

/* Include these here as well since may show up in window [0,2] explicitly add 1.5 also */
	if (nu == -0.5) {
		return (z == 0) ? REAL_MAX : SQRT(2/PI/z)*COS(z);
	} else if (nu == 0.5) {
		return (z == 0) ? 0.0 : SQRT(2/PI/z)*SIN(z);
	} else if (nu == 1.5) {
		return (z == 0) ? 0.0 : SQRT(2/PI/z)*(SIN(z)/z-COS(z));
	}

/* Switch based on the limit - assymptotic or simple power series */
	zuse = (FABS(nu)<20) ? 18 : FABS(nu)-2;

/* Based on Hankel's Asymptotic Expansion from Abramowitz and Stegan 9.2.5 */
	if (FABS(z) > zuse) {
		mu = 4*nu*nu;								/* mu */
		zp = 8*z;									/* 8z */
		zp2 = (zp*zp);								/* (8z)^2 */
		minterms = (int) (FABS(nu)/2+1);		/* Minimum # of terms to sum */
		maxterms = minterms+20;
		
		P = 1;										/* Strange series to sum for P */
		delta  = -(mu-1)*(mu-9)/2/zp2;		/* First term */
		for (i=0; i<maxterms; i++) {
			P     += delta;
			delta *= -(mu-(4*i+5)*(4*i+5))*(mu-(4*i+7)*(4*i+7)) / zp2 / ((2*i+3)*(2*i+4));
			if (i > minterms && FABS(delta)<FABS(1E-13*P)) break;					/* 10 digits is enough! */
		}

		Q = 0;										/* Strange series to sum for Q */
		delta = (mu-1)/zp;
		for (i=0; i<maxterms; i++) {
			Q     += delta;
			delta *= -(mu-(4*i+3)*(4*i+3))*(mu-(4*i+5)*(4*i+5)) / zp2 / ((2*i+2)*(2*i+3));
			if (i > minterms && FABS(delta)<FABS(1E-13*Q)) break;					/* 10 digits is enough! */
		}
		chi = z-(0.5*nu+0.25)*PI;
		tmp = sqrt(2/PI/z)*(P*cos(chi)-Q*sin(chi));

/* Ascending series from Abramowitz and Stegan 9.1.10 */
	} else if (nu > 170) {													/* Gamma is out of bounds */
		tmp = 0;

	} else {
		/* j(nu,z) = (z/2)^nu * sum (-1)^i (z/2)^2i / i! / gamma(i+nu+1) */
		zp = 1;
		zp2 = z*z/4;
		tmp = 0;
		delta = zp/norm_gamma(nu+1);										/* First term */
		for (i=0; i<JN_MAX_TERMS; i++) {
			tmp += delta;
			delta *= -zp2/(i+1)/(nu+i+1);
			if (FABS(delta) < FABS(1E-13*tmp)) break;					/* 13 digits is enough! */
		}
		tmp = ((tmp>0)?+1:-1) * EXP( LOG(FABS(tmp)) + nu*LOG(z/2) );
		}
	return tmp;
}

/* ===========================================================================
-- Routine to calculate the digamma function of a real argument
--
-- The digamma function (also called psi(x)) is the derivative on the
-- log of the gamma function.  psi(x) = d/dx [ ln gamma(s) ]
--
-- For z<0.5, the reflection formula is used to convert to a positive argument
--          psi(1-x) = psi(x)+pi*cot(pi*x)
--          psi(x) = psi(-x+1)+pi*cot(pi*(-x+1))
-- For z<10, use recursion formula to get an argument >10 for asymptotics
--          psi(z+1) = psi(z)+1/z
--          psi(z) = psi(z+1)-1/z
=========================================================================== */
#define	NTERMS_DIGAMMA	7
static TMPREAL digamma(TMPREAL z) {

	TMPREAL rc, r;
	int i;
	static TMPREAL em = 0.57721566490153286060651209008240;	/* Euler-Mascheroni constant */
	static TMPREAL cf[NTERMS_DIGAMMA] = {0};

/* Initialize the Bernouli to full precision */
	if (cf[0] == 0) {								/* Bernouli # B(2N)/2N */
		cf[0] =  1.0 /  6.0 /  2.0;
		cf[1] = -1.0 / 30.0 /  4.0;
		cf[2] =  1.0 / 42.0 /  6.0;
		cf[3] = -1.0 / 30.0 /  8.0;
		cf[4] =  5.0 / 66.0 / 10.0;
		cf[5] = -691.0 / 2730.0 / 12.0;
		cf[6] =  7.0 /  6.0 / 14.0;
	}

/* First abort at the poles of the function (0 or negative integers) */
	if (z <= 0 && FLOOR(z) == z) return TMPREAL_MAX;

/* Start off with nothing */
	rc = 0;

/* Reduce Z by recursion until in the window -0.5<z<0.5 */
	if (z <= -10.0) {											/* Use reflection formula */
		z = -z+1;												/* Now definitely positive */
		r = z-2*FLOOR(z/2);									/* Deal with the 2*pi looping */
		rc = PI/TAN(PI*r);
	}

/* At this point, z is [-10,infty] */
/* If not an integer < 10, use recursion to get z>10 and then assymptotic expansion */
	if (FLOOR(z) != z || z > 10) {
		while (z<10.0) { rc -= 1/z; z += 1.0; }		/* As many as 20 steps may be required */
		rc += LOG(z) - 0.5/z;
		r = z*z;
		for (i=0; i<NTERMS_DIGAMMA; i++) {				/* And another 7 in the asymptotic series */
			rc -= cf[i]/r;
			r *= z*z;
		}
	/* The integer case - much simpler going to psi(1) = em */
	} else {
		rc += -em;
		for (i=(int)(FLOOR(z)-1); i>0; i--) rc += 1.0/i;
	}

	return rc;
}


/* ===========================================================================
-- Routine to try to return a high precision timer value.  The return
-- should be in units of seconds with the highest precision that is
-- reasonable for the system.  It is guarenteed to be monotonic but not
-- to be absolute.
--
-- Usage: TMPREAL my_timer(BOOL reset)
--
-- Inputs: reset - should system reset the timer to zero return?
--
-- Output: none
--
-- Return: Time in seconds since first call or last reset of the counter
=========================================================================== */
static TMPREAL my_timer(BOOL reset) {
	static BOOL init=FALSE;
#ifdef NT
	static LARGE_INTEGER freq, count0;
	LARGE_INTEGER counts;
	if (! init || reset) {
		init = TRUE;
		QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&count0);
		return 0.0;
	}
	QueryPerformanceCounter(&counts);
	return (freq.QuadPart == 0) ? 0.0 : (1.0*(counts.QuadPart-count0.QuadPart))/freq.QuadPart;
#else
	static int count0;
	if (! init || reset) {
		init = TRUE;
		count0 = time(NULL);
		return 0.0;
	}
	return time(NULL)-count0;
#endif
}

/* ===========================================================================
-- Routine to print out the current cmd from an evaluation operation
=========================================================================== */
static void ShowCmdInfo(int cmd, GVCMDS *cmds) {
	int i;
	for (i=0; math_cmd_help[i].cmd != math_cmd_end; i++) {
		if (math_cmd_help[i].cmd != cmd) continue;
		TTYprintf(" [%3.3d] %-30s  *cmds: 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x\n", cmd, math_cmd_help[i].text, cmds[0], cmds[1], cmds[2], cmds[3]);
		break;
	}
	if (math_cmd_help[i].cmd == math_cmd_end) {
		TTYprintf(" [%3.3d] (no associated text)\n", cmd);
	}
	return;
}

/* ===========================================================================
-- Routine to print out the current stack during an evaluation operation
=========================================================================== */
static void ShowRealCmdStack(TMPREAL *top, TMPREAL *now) {

	size_t i, num_stack, num_args;

	num_stack = (top-now);
	num_args  = subptr;

	for (i=0; i<max(num_stack,num_args); i++) {
		if (i < num_stack) {
			TTYprintf("    rstack[%2d] = %-13LG", i, *(top-1-i));
		} else {
			TTYprintf("    rstack[%2d] = <empty>      ", i);
		}
		if (i < num_args) {
			TTYprintf("    fnc_arg[%2d] = %-13LG\n", i, rstack_public[i]);
		} else {
			TTYprintf("\n");
		}
	}

	return;
}

/* ===========================================================================
-- Routine to print out the current stack during an evaluation operation
=========================================================================== */
static void ShowComplexCmdStack(TMPCOMPLEX *top, TMPCOMPLEX *now) {

	size_t i, num_stack, num_args;

	num_stack = (top-now);
	num_args  = subptr;

	for (i=0; i<max(num_stack,num_args); i++) {
		if (i < num_stack) {
			TTYprintf("    cstack[%2d] = (%-13LG , %-13LG)", i, (top-1-i)->x,(top-1-i)->y);
		} else {
			TTYprintf("    cstack[%2d] = <empty>                        ", i);
		}
		if (i < num_args) {
			TTYprintf("    fnc_arg[%2d] = (%-13LG , %-13LG)\n", i, cstack_public[i].x, cstack_public[i].y);
		} else {
			TTYprintf("\n");
		}
	}

	return;
}
