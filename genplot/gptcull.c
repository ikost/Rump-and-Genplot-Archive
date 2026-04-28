/* gpt_do_cull */

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
#include "lexp.h"
#include "tplot.h"
#include "gptxtrn.h"

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
void GptSetRange(void);

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */


/* ============================================================================
-- Subroutine to determine value of the knots for a SPLINE fit to a curve
--
-- Usage:  LOGICAL = CULL$(x,y,npt,nptmax)
--
-- Inputs: x,y,npt - curve
--
-- Output: Compresses data to fewer points by various techniques
============================================================================ */
#define	KEEP		1
#define	DELETE	2
#define	ZERO		3
#define	WINDOW	4
#define	COMPRESS	5
#define	SQUISH	6
#define	AVERAGE	7

static char CullHelp[] = 
 "\n"
 " Command to delete (or keep) data points based on some criteria.\n"
 "\n"
 "   CULL <mode> <parms>\n"
 "\n"
 "  <mode> - how to modify the data.  Choices are:\n"
 "     keep     <criteria> - keep points matching the criteria\n"
 "     delete   <criteria> - delete points matching the criteria\n"
 "     zero     <criteria> - set (y or z) value of points matching to zero"
 "     window   <criteria> - set (y or z) value of points not matching to zero\n"
 "     squish   <num>      - keep only every <num> point from the dataset\n"
 "     average  <num>      - average every <num> points (x,y and possibly z)\n"
 "     compress <num>      - sum every <num> points ... set x to value at midpoint\n"
 "\n"
 "  <criteria> - How to select points fro keep, delete, zero or window\n"
 "     range <xlow> <xhigh> <ylow> <yhigh> [<zlow> <zhigh>]\n"
 "     xrange <xlow> <xhigh>\n"
 "     yrange <ylow> <yhigh>\n"
 "     zrange <zlow> <zhigh>\n"
 "     irange <ilow> <ihigh>\n"
 "     cursor      - draws box cursor and uses x,y extent of rectangle\n"
 "     box         - synonymous with cursor\n"
 "     for <expr>  - arbitrary mathematical expression of x,y[,z]\n"
 "\n"
 " Examples: cull keep xrange -100 1000\n"
 "           3d cull keep range -10 10 -20 20 0 1000\n"
 "           cull delete for x^2+y^2>r^2\n"
 "           cull window for x^2+y^2>r^2\n"
;

int gpt_do_cull(void) {

	REAL		xmin=-REAL_MAX, xmax=REAL_MAX,		/* Range to fit */
				ymin=-REAL_MAX, ymax=REAL_MAX,
				zmin=-REAL_MAX, zmax=REAL_MAX;
	INT		imin=0, imax=GVI_MAX_LENGTH;
	REAL		*x, *y, *z, *xsave, *ysave, *zsave, xtmp, ytmp, ztmp;
	int		ierr, errcnt=0;
	INT		i,j,inc, nnew=0;						/* Indexing points and npt */
	char		token[LONG_STR_SIZE];
	int		imode = KEEP;							/* Assume KEEP  mode */
	LOGICAL	range = TRUE;							/* Assume RANGE mode */
	LOGICAL	istrue;
	GVCMDS  *mathcmds;								/* For "FOR" mode		*/
	
	x = xsave = GptCurve->x;
	y = ysave = GptCurve->y;
	z = zsave = GptCurve->z;

/* First, look for a help request */
	if (LexCheckHelp("Cull", CullHelp, NULL)) return(OKAY);

	if (LexGetTokenP(token, sizeof(token), "Cull mode [KEEP|delete|zero|window]: "))
		imode = LexSelect(token, "KEEP DELETE ZERO WINDOW COMPRESS SQUISH AVERAGE");
	LEXESCAPE;

	if (imode == COMPRESS || imode == SQUISH || imode == AVERAGE) {
		inc = LexGetInt(1, "Every Ith point: ");
		if (LexEscape(TRUE)) return(OKAY);
		if (inc <= 1) return(OKAY);
		for (i=0; i<GptCurve->npt; i+=inc) {
			if (imode == SQUISH) {							/* Single point */
				xtmp = *x; ytmp = *y;
				if (z != NULL) ztmp = *z;
			} else if (i+inc > GptCurve->npt) {			/* Done anyway	*/
				break;
			} else if (imode == AVERAGE) {
				xtmp = ytmp = ztmp = 0;
				for (j=0; j<inc; j++) {xtmp += x[j]; ytmp += y[j];}
				if (z != NULL) for (j=0; j<inc; j++) ztmp += z[j];
				xtmp /= inc; ytmp /= inc; ztmp /= inc;	/* Average X & Y */
			} else {
				ytmp = ztmp = 0;
				for (j=0; j<inc; j++) ytmp += y[j];
				if (z != NULL) for (j=0; j<inc; j++) ztmp += z[j];
				xtmp = x[inc/2];
			}
			*xsave++ = xtmp; *ysave++ = ytmp; nnew++;
			x += inc; y += inc;
			if (z != NULL) {*zsave++ = ztmp; z += inc;}
		}
		GptCurve->npt = nnew;
		return(OKAY);
	} else if (imode <= 0) {
		gen_err2("Unrecognized CULL mode", token);
		return(NOMORE);
	} 

	if (! LexGetTokenP(token, sizeof(token), "[range|xrange|yrange|zrange|irange|cur|for|ABORT] ")) return(OKAY);
	LEXESCAPE;
	i = LexSelect(token,"RANGE XRANGE YRANGE ZRANGE IRANGE CURSOR BOX FOR");
	switch (i) {
		case 1:										/* RANGE */
		case 2:										/* XRANGE */
			xmin = LexGetReal(xmin, "Lower X limit: ");
			LEXESCAPE;
			xmax = LexGetReal(xmax, "Upper X limit: ");
			LEXESCAPE;
			if (i != 1) break;
		case 3:										/* YRANGE */
			ymin = LexGetReal(ymin, "Lower Y limit: ");
			LEXESCAPE;
			ymax = LexGetReal(ymax, "Upper Y limit: ");
			LEXESCAPE;
			if (i != 1 || z == NULL) break;
		case 4:
			if (z == NULL) {
				ERRprintf("DUMMY: Put z in the curve and you can process based on its value!\n");
				return NOMORE;
			}
			zmin = LexGetReal(zmin, "Lower Z limit: ");
			LEXESCAPE;
			zmax = LexGetReal(zmax, "Upper Z limit: ");
			LEXESCAPE;
			break;
		case 5:
			imin = LexGetInt(imin, "Lower I limit: ");
			LEXESCAPE;
			imax = LexGetInt(imax, "Upper I limit: ");
			LEXESCAPE;
			break;
		case 6:								/* Box or Cursor mode */
		case 7:
			GptSetRange();
			if (! PlotBoxCursor(&xmin, &ymin, &xmax, &ymax, NULL)) {
				gen_err("Box cursor not implemented on this device");
				return(NOMORE);
			}
			gpt_SetBoxCursorCoords(xmin, ymin, xmax, ymax);
			break;
		case 8:
			range = FALSE;
			if (! LexGetMath(token, sizeof(token))) 
				LexPromptStr(token, sizeof(token), "Condition: ");
			if (*token == '\0' || LexEscape(TRUE)) return(OKAY);
			if ((mathcmds = GVParse(token, NULL)) == NULL) {
				ERRprintf("ERROR: Condition is not a legal math expression\n");
				return(NOMORE);
			}
			break;
		default:
			gen_err2("Unrecognized CULL range", token);
			return(NOMORE);
	}

/* Make sure the values are in order */
	OrderPair(&xmin,&xmax);
	OrderPair(&ymin,&ymax);
	OrderPair(&zmin,&zmax);

/* ------------------------------------- */
/* Okay - now do the operation requested */
/* ------------------------------------- */
	for (i=0; i<GptCurve->npt; i++) {
		if (range) {
			istrue = (*x >= xmin) && (*x <= xmax) && (*y >= ymin) && (*y <= ymax) && (i >= imin) && (i <= imax);
			if (z != NULL && istrue) istrue = (*z >= zmin) && (*z <= zmax);
		} else {
			istrue = (GVEvalCmdsI(mathcmds, i, &ierr) > 0.0);
			if (ierr != 0) {
				if (! (errcnt++)) gen_warn("Illegal expression: I'm assuming FALSE!");
			}
		}
		switch (imode) {
			case DELETE:
				istrue = !istrue;
			case KEEP:
				if (istrue) {
					*(xsave++) = *x;
					*(ysave++) = *y;
					if (z != NULL) *(zsave++) = *z;
					nnew++;
				}
				break;
			case WINDOW:
				istrue = !istrue;
			case ZERO:
				if (istrue) {
					if (z != NULL) *z = 0.0f;
					else				*y = 0.0f;
				}
				nnew++;
				break;
		}
		x++; y++;
		if (z != NULL) z++;
	}

	if (errcnt > 0)
		TTYprintf("WARNING: %i expression errors occurred.  All asssumed FALSE -- HA HA!", errcnt);

	GptCurve->npt = nnew;
	return(OKAY);
}
