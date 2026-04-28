/* GVSolve.c - routine to solve for equation root */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define	_POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <float.h>
#include <math.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ===========================================================================
-- Van Wijngaarden-Dekker-Brent Method
--
-- Given a function f(x) and a range [xl,xh] containing a root, determines
-- the zero of the function using a combination of bisection, secant and
-- false position methods.
--
-- Usage: int GVSolve(REAL *root, int Index, SOLVE_PARMS *parms, int *ic);
--
-- Inputs: root  - pointer to variable to receive the result
--         Index - index used for accessing array pointers within the function
--         parms - pointer to structure containing
--                   double LowerBound, UpperBound  - bounds on the solution
--                   double Guess                   - initial guess
--                   double epsilon                 - precision required
--                   int maxiter                    - maximum # of iterations
--                   GVCMDS *cmds                   - function to solve
--                   TMPREAL (*fnc)(REAL root, int idx, int *err)
--                 either fnc or cmds must be NULL
--         ic    - pointer to variable receiving number of iterations required
--                 set to NULL if iteraction count is not required
--
-- Output: *root - root of the function, or half window if invalid behavior
--         *ic   - if ic is not NULL, number of iterations used to solve
--
-- Returns:  0  ==> Root successfully found
--          -1  ==> Root not bracketed within interval
--          -2  ==> Invalid function
--          -3  ==> No solution found within maxiter iterations
--
-- Reference: Press et.al Sec 9.3ff
--
-- Notes: This routine is passed the address of a function, or a string of
--        parsed commands, which are evaluated at each guess.  One of the
--        structures parms->cmds or parms->fnc must be NULL (but not both).
--
--        Routine will not immediately fail if f(a) and f(c) have the same
--        sign.  Instead, looks for a sign reveral between the interval using
--        boring division search.  If successful, will return the smaller
--        root (but not necessarily the smallest root).
--
-- Mod 4/1/2010: root is now double throughout expression use
=========================================================================== */
int GVSolve(double *root, int Index, SOLVE_PARMS *parms, int *ic) {
	
	int    iter,i,ierr,rcode;
	int	 UseCmds;

	GVCMDS *cmds;								/* Local copies from SOLVE_PARMS */
	TMPREAL (*fnc)(double root2, int idx, int *err);

	TMPREAL a,b,c;								/* Coordinates							*/
	TMPREAL fa,fb,fc;							/* Function value at a,b,c			*/
	TMPREAL delta,								/* Correction to root estimate	*/
		     l_delta;							/* Last correct to root estimate */
	TMPREAL xm;									/* Bounding interval width			*/
	TMPREAL P,Q,R,S,T;						/* Args for quadratic interpolation */

	TMPREAL tol1, tol=1000*REAL_MIN;		/* THIS REALLY IS REAL_MIN, not TMPREAL */

	cmds = parms->cmds;						/* Use parsed cmds, or function call */
	fnc  = parms->fnc;						/* Have copy of both present			 */
	UseCmds = (cmds != NULL);				/* Use cmds or fnc?		*/

	a = min(parms->LowerBound, parms->UpperBound);	/* Start a < c as limits */
	b =     parms->Guess;
	c = max(parms->LowerBound, parms->UpperBound);

	*root = (double) a;						/* Evaluate function at lower limit	*/
	if (fnc==NULL && cmds==NULL) {rcode = -2; goto AllReturn;}

 	fa = UseCmds ? GVEvalCmdsI(cmds,Index,&ierr) : (*fnc)(*root,Index,&ierr);
	if (ierr == 0) {
		*root = (double) c;
		fc = UseCmds ? GVEvalCmdsI(cmds,Index,&ierr) : (*fnc)(*root,Index,&ierr);
	}
	if (ierr != 0)	{rcode = -2;        goto AllReturn;}	/* Evaluate error? */
	if (fa == 0)	{rcode =  0; b = a; goto AllReturn;}	/* Root at edge?	 */
	if (fc == 0)	{rcode =  0; b = c; goto AllReturn;}	/* Other edge?		 */

/* ===========================================================================
 * Have a problem if both fa and fc are positive (or negative).  Easiest is to
 * abandon the search and return an error.  But let's spend some time.  First, 
 * maybe the initial guess has an opposite sign.  If so, can just use it.
 * =========================================================================== */
	if (fa*fc > 0 && b > a && b < c) {							/* b might be valid */
		*root = (double) b;
		fb = UseCmds ? GVEvalCmdsI(cmds,Index,&ierr) : (*fnc)(*root,Index,&ierr);
		if (ierr != 0) {rcode = -2; goto AllReturn;}
		if (fa*fb < 0) {												/* Okay, root between a and b ... reset values */
			c = *root;													/* There are now roots between a and b, and b and c */
			fc = fb;
			b = (a+c)/2;												/* And there is a root in between now */
		}
	}

/* ===========================================================================
 * Otherwise, search the space between a and c looking for a sign reversal.  
 * Limit to the same number of iterations that would have been tolerated 
 * otherwise.
 * =========================================================================== */
	if (fa*fc > 0) {													/* No root in the interval? */
		for (i=0; i<parms->MaxIterate; i++) {
			*root = a + (c-a)*(rand()+1.0)/(RAND_MAX+1.0);	/* Random point in interval */
			fb = UseCmds ? GVEvalCmdsI(cmds,Index,&ierr) : (*fnc)(*root,Index,&ierr);
/*			TTYprintf("Trying at %f with value %f\n", (double) *root, (double) fb); */
			if (ierr != 0) {rcode = -2; goto AllReturn;}
			if (fa*fb < 0) break;
		}
		if (fa*fb > 0) {												/* Nope ... give up now and return smallest edge */
			b = (fabs(fa) < fabs(fc)) ? a : c ;					/* One with smallest abs error */
			rcode = -1;        
			goto AllReturn;
		}
		c = *root;														/* There are now roots between a and b, and b and c */
		fc = fb;
		b = (a+c)/2;													/* And there is a root in between now */
	}

/* If we have a guess, determine it's value as well */
	if (b > a && b < c) {											/* Check guess		 */
		*root = (double) b;
		fb = UseCmds ? GVEvalCmdsI(cmds,Index,&ierr) : (*fnc)(*root,Index,&ierr);
		if (ierr != 0) {rcode = -2; goto AllReturn;}
	} else {																/* Else, use midpoint */
		b  = (a+c)/2;
		*root = (double) b;
		fb = UseCmds ? GVEvalCmdsI(cmds,Index,&ierr) : (*fnc)(*root,Index,&ierr);
		if (ierr != 0) {rcode = -2; goto AllReturn;}
	}

/* Now, start the iteration process */
	for (iter=1; iter<=parms->MaxIterate; iter++) {

/* ... Keep root between b&c.  If not, move a to c and continue linear */
		if (fb*fc > 0) {						/* b&c same sign, dump c */
			c = a; fc = fa;					/* Leaves values a,b,a	 */
			l_delta = delta = b-a;			/* Last change intervals */
		}
/* ... b must be close to root than c.  If not, exchange b/c and forget a */
		if (fabs(fc) < fabs(fb)) {			/* Put nearest value in center */
			a = b; fa = fb;
			b = c; fb = fc;
			c = a; fc = fa;
		}
		xm = (c-b);								/* Interval width		*/

/* ... Check the convergence criteria, possibly done or minimal change? */
		tol1 = parms->epsilon*fabs(b) + tol;
		if ( (fabs(xm) <= tol1) || (fb == 0) ) break;	/* Are we done? */

/* Attempt inverse quadratic (linear) as long as b is closer to root than a */
		if ( (fabs(l_delta) >= tol1/2) && (fabs(fa) > fabs(fb)) ) {
			S = fb/fa;							/* Notation of eqns 9.3.1ff		*/
			if (a == c) {						/* Two point secant method only	*/
				P = xm*S;						/* Next estimate is x + P/Q		*/
				Q = S-1;
			} else {								/* Three point interpolation		*/
				T = fa/fc;						/* Notation of eqns 9.3.1ff		*/
				R = fb/fc;
				P = S*(xm*T*(R-T)-(b-a)*(1-R));
				Q = (T-1)*(R-1)*(S-1);
			}
			if (P<0) {Q=-Q; P=-P;}			/* Invert so compare below safe	*/
			if (2*P < min(3*xm*Q-fabs(tol1*Q/2), fabs(l_delta*Q))) {
				l_delta = delta;				/* Save old for superlinearity	*/
				delta = P/Q;					/* Accept as going superlinear	*/
			} else {
				l_delta = delta = xm/2;		/* Interpolate failed, bisect		*/
			}
		} else {									/* Limit changing slowly, bisect	*/
			l_delta = delta = xm/2;
		}
		a  = b; fa = fb;						/* Move previous best guess to a */
		if (fabs(delta) > tol1/2) {		/* Estimate large enough, do it! */
			b = b+delta;
		} else {									/* Correction too small			 */
			if (xm <  0) b = b-tol1/2;		/* add tol1/2*sign(xm) instead */
			if (xm >= 0) b = b+tol1/2;
		}
		*root = (double) b;					/* Calculate value at new guess	*/
		fb = UseCmds ? GVEvalCmdsI(cmds,Index,&ierr) : (*fnc)(*root,Index,&ierr);
		if (ierr != 0) break;
	}

	if (iter >= parms->MaxIterate) {		/* Potential error returns */
		rcode = -3;
	} else if (ierr != 0) {					/* Bad function				*/
		rcode = -2;
	} else {										/* All successful				*/
		rcode = 0;
	}

AllReturn:
	if (ic != NULL) *ic = iter;
	*root = (double) b;
	return(rcode);
}


#ifdef JUNK
	char *aptr;
	else if (*fnc == '=') {
		ERRprintf("ERROR: Let's keep our heads on straight, folks (%s)\n", fnc);
		return(NOMORE);
	} else if (fnc[strlen(fnc)-1] == '=') {	/* Of a= b type */
		if (! LexGetMathP(tok, sizeof(tok), "Value: (abort) ")) return(OKAY);
		sprintf(fnc+strlen(fnc)-1, "-(%s)", tok);
	} else if (LexChkToken(tok, sizeof(tok)) && (*tok == '=')) {
		LexGetMath(tok, sizeof(tok));
		aptr = tok+1;									/* Actual value it will equal */
		if (*aptr == '\0') {							/* Separate = sign */
			if (! LexGetMathP(tok, sizeof(tok), "Value: (abort) ")) return(OKAY);
			aptr = tok;
		}
		sprintf(fnc+strlen(fnc), "-(%s)", aptr);
	} else {												/* Possible form a(x)=b(x) */
		aptr = fnc;
		while ( (aptr = strchr(aptr,'=')) != NULL) {
			if (strchr("!^<>", *(aptr-1)) != NULL) {
				aptr++; continue;
			} else if (strchr("=<>", *(aptr+1)) != NULL) {
				aptr += 2; continue;
			} else {									/* Okay, we've located the break */
				strcpy(tok, aptr+1);				/* Second half of equality			*/
				sprintf(aptr, "-(%s)", tok);
				break;
			}
		}
	}
#endif
