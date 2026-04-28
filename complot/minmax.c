/* minmax.f77 - Routine to determine min and max of an array. */

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
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <math.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"

#include "tplot.h"
#include "complot.h"
#include "plotdefs.h"

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

/* =============================================================================
--     MINMAX  - Return minimum and maximum values in an array sent by user.
--     MINMAX2 - Return minimum and maximum values excluding NAN and \infty
--
--     Usage: CALL MINMAX (X,NPT,XMIN,XMAX)
--            CALL MINMAX2(X,NPT,XMIN,XMAX)
--
--     Inputs: X - Array of point
--             NPT - Number of data points
--
--     Output: XMIN - Minimum value in the array
--             XMAX - Maximum value in the array
============================================================================= */
void ArrayMinMax(REAL *x, INT npt, REAL *xmin, REAL *xmax) {

	if (npt <= 0) {
		*xmax = *xmin = 0.0f;
	} else {
		*xmax = *xmin = *x;
		while (npt--) {
			if (*x > *xmax) *xmax = *x;
			if (*x < *xmin) *xmin = *x;
			x++;
		}
	}
	return;
}

/* ============================================================================
--     Simple routine to order two elements so first is the largest
--
--     Usage: LOGICAL FUNCTION ORDER(X1,X2)
--
--     Output: X1,X2 set so X1 < X2
--             ORDER - .TRUE. if switch necessary
============================================================================= */
LOGICAL OrderPair(REAL *x1, REAL *x2) {
	
	REAL t1;
	if (*x1 > *x2) {
		t1 = *x1;
		*x1 = *x2;
		*x2 = t1;
		return(TRUE);
	}
	return(FALSE);
}
