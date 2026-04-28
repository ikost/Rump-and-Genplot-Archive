/* PlotAutoScale */

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
#include <float.h>

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

/* ============================================================================
--     PlotAutoScale - Autoscaling routine for axis ranges.
--
--     A useful routine to automatically scale data range to a "nice" range
--     for plotting.  Takes given min and max values and modifies them to
--     even ranges of a suggested major tick length and associated minor tick
--     length.  For example: 1.37 to 18.33 returns
--                           0.00 to 20.00    dx = 5.0    dx2 = 1.0
--     The lower limit will be forced to 0 if FMIN < FMAX*ZFORCE (zero force)
--     The variable lies in common with default value of 0.3 .
--
--     Usage:     PlotAutoScale(FMIN,FMAX,SMIN,SMAX,DX,DX2,M)
--
--     Inputs:    FMIN  -  Current minimum of data to be plotted
--                FMAX  -  Current maximum of data to be plotted
--                Common array ZFORCE for zero forcing.
--
--     Output:    SMIN  -  Corrected minimum
--                SMAX  -  Corrected maximum
--                DX    -  Recommended major tick mark spacing
--                DX2   -  Recommended minor tick mark spacing
--                M     -  Necessary labelling mode (integer or float, etc.)
--                      <0 => Integer mode
--                      >0 => Float mode with M digits past decimal
--
--     NOTES:     If FMIN = FMAX, an error condition detected.  Output leaves
--                FMIN unchanged and default type values to FMAX. Programs
--                then continues as if these values were given.
============================================================================= */
/* Logic: If normalized range df >= dxlim[i], use the dx,dx2 values */

static double dxlim[]  = {7.001,3.501, 1.401, 0.99999};		/* Limits	*/
static double dxuse[]  = {2.0,  1.0,   0.5,   0.2};			/* DX			*/
static double dx2use[] = {0.5,  0.2,   0.1,	 0.05};			/* DX2		*/

void PlotAutoScale(REAL fmin, REAL fmax, REAL *smin, REAL *smax, REAL *dx, REAL *dx2, INTEGER *m) {
	PlotAutoScaleZ(fmin, fmax, smin, smax, dx, dx2, m, PlotWindow->zforce);
	return;
}

void PlotAutoScaleZ(REAL fmin, REAL fmax, REAL *SMIN, REAL *SMAX, REAL *DX, REAL *DX2, INTEGER *M, REAL zforce) {

	double	smin, smax, dx, dx2;					/* Local values of return values */
	double	smin_old, smax_old;					/* For iteration until stable		*/
	int		iter=4;									/* Allow 4 iterations to solve */
	INTEGER	m;

	double	temp, df, factor;						/* Local variables*/
	LOGICAL	negate  = FALSE;						/* Coordinates not switched	 */
	LOGICAL	reverse = FALSE;						/* No reverse of SMIN and SMAX */
	INTEGER	i;
	long int	i4;
	
/* Initialize parameters */
	smin = fmin;										/* Output = Input for now */
	smax = fmax;

/* ... Check for bad parameters and modify as necessary */
	if (smin == smax) {						/* Default conditions */
	  	if (smin < 0)							/* If negative, even interval about 0 */
			smax = -smin;
		else if (smin > 0)					/* Positive, double interval */
			smax = 2*smin;
		else										/* 0, go from [0,1] */
			smax = 1;
	} else if (smin > smax) {				/* Always keep SMIN < SMAX */
		temp = smax;
		smax = smin;
		smin = temp;
		reverse = TRUE;						/* Remember to reverse at end again */
	}

	if (smax < 0) {							/* Also always work with positive #'s */
		temp = smax;							/* Negate and switch the pairs */
		smax = -smin;
		smin = -temp;
		negate = TRUE;							/* Remember at the end */
	}

/* ... Start an iterative loop until we arrive at a stable choice */
	do {
		smin_old = smin;						/* Save current smin/smax	*/
		smax_old = smax;						/* Use to compare at end	*/

/* ... Reset SMIN to 0 if is positive and < ZFORCE*SMAX.  Keeps scales "nice"  */
		if ( (smin > 0) && (smin < zforce*smax)) smin = 0;

/* ... We want to get MAX-MIN between 1 and 10 for the rest of the routines. */
		df = smax - smin;							/* Length of interval 			*/
		factor = 1;									/* No divisors						*/
		while (df < 1)	  {						/* Get it to range of [1,10]; */
		  	df = 10*df;
			factor /= 10;
		}
		while (df > 10.01) {
		  	df = df/10;
			factor *= 10;
		}

#ifdef __LINUX__
		printf("");	/* I get weird stuff after error recovery without this - LRD */
#endif

/* ... Choose the major and minor tick spacing */
		df = floor(df*1000.0+0.49) / 1000.0;	/* Round off to thousandth */
		for (i=0; i<4; i++) {						/* Do the remaining tests (nbd) */
			if (df >= dxlim[i]) {
				dx  = dxuse[i]  * factor;
				dx2 = dx2use[i] * factor;
				break;
			}
		}

/* Now, take the given DX and move the endpoints slightly to make axis
   start and end on a division.  Allow a little play for roundoff. */

		i4 = (long int) (smax/dx + 0.99);	/* New end, now at an even boundary */
		smax = dx*i4;
		i4 = (long int) (smin/dx + 0.01);	/* 1% overrun at this end also */
		temp = dx*i4;
		smin = (smin<0) ? temp-dx : temp ;	/* Correct if we are negative */

	} while ( (fabs(smin_old-smin)+fabs(smax_old-smax))/fabs(smax-smin) > REAL_EPSILON && --iter);
	if (iter == 0) gen_warn("Unable to stabilize autosc - tell developers they blew it");

/* And undo and changes from beginning */
	if (negate) {								/* Did we negate to keep positive */
		temp = smax;
		smax = -smin;
		smin = -temp;
	}
	if (reverse) {								/* Had we interchanged coordinates */
		temp = smax;
		smax = smin;
		smin = temp;
	}

/* Determine labelling mode to be suggested */
	if (dx >= 1) {								/* Set integer mode if DX > 1.0 */
		m = -1;
	} else if (dx>=1.0e-9 && dx<1.0e10) { /* # of digits needed to see dx (safe) */
		m = (INTEGER) (-log10(dx)+0.99);	/* 0.99 takes care of 0.1 0.01 etc. */
	} else {										/* Otherwise will be scientific */
		m = 0;
	}

	if (SMIN != NULL) *SMIN = (REAL) smin;		/* And return local values */
	if (SMAX != NULL) *SMAX = (REAL) smax;
	if (DX   != NULL) *DX   = (REAL) dx;
	if (DX2  != NULL) *DX2  = (REAL) dx2;
	if (M    != NULL) *M    = m;
	return;

}

/* =============================================================================
--     SUBROUTINE AUTOLG - Autoscaling routine for axis ranges.
--
--     A useful routine to automatically scale data range to a "nice" range
--     for plotting.  Takes given min and max values and modifies them to
--     even ranges of a suggested major tick length and associated minor tick
--     length.  For example: 1.37 to 18.33 returns
--                           0.00 to 20.00    dx = 5.0    dx2 = 1.0
--     The lower limit will be forced to 0 if FMIN < FMAX*ZFORCE (zero force)
--     The variable lies in common with default value of 0.3 .
--     New version to create the proper conditions for logarithmic axis using
--     LOGAXIS.
--
--     Usage:     CALL AUTOLG(FMIN,FMAX,SMIN,SMAX,DX,DX2,M)
--
--     Inputs:    FMIN  -  Current minimum of data to be plotted
--                FMAX  -  Current maximum of data to be plotted
--                Common array ZFORCE for zero forcing.
--
--     Output:    SMIN  -  Corrected minimum
--                SMAX  -  Corrected maximum
--                DX    -  Recommended major tick mark spacing
--                DX2   -  Recommended minor tick mark spacing
--                  -4  -  Label 1.1-5.0 by .1 and 5.2-9.8 by .2
--                  -3  -  Label 1,2,3,4,5,6,7,8,9,10
--                  -2  -  Label 1,2,5
--                  -1  -  label 1,5
--                   1  -  label decades
--                   0  -  no ticks
--                M     -  Necessary labelling mode (integer or float, etc.)
--                      <0 => Integer mode
--                      >0 => Float mode with M digits past decimal
--
--     NOTES:     If FMIN = FMAX, an error condition detected.  Output leaves
--                FMIN unchanged and default type values to FMAX. Programs
--                then continues as if these values were given.
============================================================================= */
void PlotAutoLogScale(REAL fmin, REAL fmax, REAL *SMIN, REAL *SMAX, REAL *DX, REAL *DX2, INTEGER *M) {
	
	INTEGER	i;
	double	temp;
	LOGICAL	negate  = FALSE;						/* Coordinates not switched	 */
	LOGICAL	reverse = FALSE;						/* No reverse of SMIN and SMAX */
	double	smin, smax, dx, dx2;					/* Local values of return values */
	REAL		ssmin, ssmax, ddx;
	
/* Initialize parameters */
	smin = fmin;										/* Output = Input for now */
	smax = fmax;

/* ... Modify coordinates as necessary */
	if (smin > smax) {						/* Always keep SMIN < SMAX */
		temp = smax;
		smax = smin;
		smin = temp;
		reverse = TRUE;						/* Remember to reverse at end again */
	}

	if (smax < 0) {							/* Also always work with positive #'s */
		temp = smax;							/* Negate and switch the pairs */
		smax = -smin;
		smin = -temp;
		negate = TRUE;							/* Remember at the end */
	}

/* ......................................................................
     Want to label every decade, or so.
     Maximum of 6 major labels across the axis
 ......................................................................... */
	smax = nint(smax+0.4999);				/* Same thing (FINE) */
	smin = nint(smin-0.4999);				/* Integer (be careful of roundoff) */
	if (smin == smax) smax = smin+1;		/* Don't allow */

/* ... Choose the major and minor tick spacing  */
/*	dx = ( (INTEGER) ((smax-smin)/6.999) ) + 1;	*/ /* DX simple - no more than 6 */
	if (smax-smin < 7) {
		dx = 1;
	} else {
		PlotAutoScaleZ(fmin, fmax, &ssmin, &ssmax, &ddx, NULL, NULL, 0.0);
		dx = ddx;
		smin = ssmin;
		smax = ssmax;
		negate = reverse = FALSE;			/* Would have been already handled by PlotAutoScaleZ */
	}

/* Much more difficult for minor tick marks ... depends on range of the data */
	i = nint(fabs(smax-smin));				/* Number of decades (smin/smax may be non-sequenced now) */
	if (i <= 1) {								/* 1 decades or less, lots of labels */
		dx2 = -5;
	} else if (i <= 2) {						/* 2 decades or less, lots of labels */
		dx2 = -4;
	} else if (i <= 8) {						/* 8 decades or less, label every */
		dx2 = -3;
	} else if (i <= 20) {					/* 20 decdes, label 2 and 5 */
		dx2 = -2;
	} else if (i <= 35) {					/* 35 decades, label 5 and 10 */
		dx2 = -1;
	} else if (i <= 50) {					/* Label only the decades */
		dx2 = 1;
	} else {
		dx2 = 0;
	}

/* And undo and changes from beginning */
	if (negate) {								/* Did we negate to keep positive */
		temp = smax;
		smax = -smin;
		smin = -temp;
	}
	if (reverse) {								/* Had we interchanged coordinates */
		temp = smax;
		smax = smin;
		smin = temp;
	}

	if (SMIN != NULL) *SMIN = (REAL) smin;		/* And return local values */
	if (SMAX != NULL) *SMAX = (REAL) smax;
	if (DX   != NULL) *DX   = (REAL) dx;
	if (DX2  != NULL) *DX2  = (REAL) dx2;
	if (M    != NULL) *M    = -1;					/*	Integer mode only */
	return;
}
