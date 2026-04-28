/* NUMBER.F77 - Plot a Number */

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
--     NUMBER - Plot a number
--
--     Usage: CALL NUMBER(X, Y, HEIGHT, IV, THETA, NOPT)
--
--     Inputs: X,Y    - Starting coordinate (may be 999., 999.)
--             HEIGHT - Height of characters in inches
--             IV     - Integer or floating number to be plotted
--             THETA  - Angle to plot at
--             NOPT   - Type of number to plot
--                      <0 => Number is an integer
--                     >=0 => Floating with NOPT digits past decimal
--
--     Note: Maximum length of output is 20 characters. Easily changed.
============================================================================= */
void PlotNumber(REAL x, REAL y, REAL height, REAL rval, REAL theta, INTEGER nopt) {

	CHAR out[21];										/* Output array */
	CHAR formt[20];									/* Format buffer */

	if (nopt >= 0) {									/* Encode the number in OUT */
		sprintf(formt, "%%.%if", nopt);			/* Should create ".7f" */
		sprintf(out, formt, rval);					/* And encode as necessary */
	} else
		sprintf(out, "%i", (INTEGER) rval);		/* Encode the integer */

	PlotString(x,y, height, out, theta, 0);
	return;

}

void Plot3DNumber(REAL x, REAL y, REAL z, REAL height, REAL rval, REAL theta, INTEGER nopt) {

	CHAR out[21];										/* Output array */
	CHAR formt[20];									/* Format buffer */

	if (nopt >= 0) {									/* Encode the number in OUT */
		sprintf(formt, "%%.%if", nopt);			/* Should create ".7f" */
		sprintf(out, formt, rval);					/* And encode as necessary */
	} else
		sprintf(out, "%i", (INTEGER) rval);		/* Encode the integer */

	Plot3DString(x,y,z, height, out, theta, 0);
	return;

}
