/*  ------------------------------------------------------------------------ */
/*  --------                                              ------------------ */
/*  -------- COPYRIGHT 1989 (c) Computer Graphics Service ------------------ */
/*  --------                                              ------------------ */
/*  --------    The source code to RUMP may be freely     ------------------ */
/*  --------  modified as long as this copyright notice   ------------------ */
/*  --------          is included and unchanged.          ------------------ */
/*  ------------------------------------------------------------------------ */

/*  tplot.c */

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
#include <string.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "rump.h"
#include "tplot.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define LINEWIDTH 1    /*   Please FIX ME !   */

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
/* My share of the global vars     */
/* ------------------------------- */

/* ------------------------------- */
/* Private global vars             */
/* ------------------------------- */


/*      SUBROUTINE TSTMRK */
/* ===========================================================================
--  Usage Guide:
--
--      SUBROUTINE TSTMRK
--     Subroutine TSTMRK is used for testing of subroutine MARK
--  Quick: Allows for testing of subroutine MARK
--
--     INPUTS:   Terminal entry of parameters in call to MARK
--
--     OUTPUTS:  Call to Mark, printout of returned parameters
--
--     COMMON BLOCKS:     None
--     CALLED FROM:       APLOT
--     CALLS:             MARK
--
=========================================================================== */
void RbsTestMark(void) {

/*  -- Local Variables -- */
	REAL ener, height;
	int key, ssize;
	char token[DFLT_STR_SIZE];

/*  -- Code begin -- */
	TTYputs("Entry to Subroutine MARK\n");
	key = LexGetInt(0,"Type of mark? (abort) ");
	if (key == 0) return;

	ener = LexGetReal(0., "Energy? (0) ");
	height = LexGetReal(0., "Height? (0) ");
	if (! LexGetTokenP(token, sizeof(token), "String? (blank) ")) strcpy(token," ");
	ssize = LexGetInt(1, "Length of string? ");
	if (ssize >= sizeof(token)) ssize = sizeof(token)-1;
	token[ssize] = '\0';
	RbsMark(key, ener, height, token);
	PlotFlush();

	TTYprintf("Key: %3d   Energy: %1.14.5g   Height: %14.5g", key, ener, height);
	type2(" String: ",token);

	return;
}


/* ===========================================================================
--  Usage Guide:
--
--      SUBROUTINE MARK (KEY, ENERGY, HEIGHT, STRING)
--  Subroutine MARK is a general utility for marking on the plot -
--  a high level program to keep RUMP away from the hassle of
--  plotting conventions
--  Quick: General utility for marking on the plot
--
--  * * * * NOTE TO MYSELF: as theoretically nice as it is to put all the
--                          stuff in one routine, the "cursor" code really
--                          gets in the way.  It doesn't belong here:
--                          make it a separate routine.
--
--
--    INPUTS:
--     KEY       Key to action desired:
--               1 - Plot and label crosshair
--               2 - Plot and label tickmark
--               3 - Just plot tickmark
--               4 - First element of a WHATISIT
--               5 - Following elements of WHATISIT
--               6 - DELETED - Cursor return (corrects log/sqrt scales)
--               7 - Left-pointing arrow and Text
--               8 - Title in upper left
--               9 - Arbitrary height tickmark and label
--              10 - Left-hand elements of WHATISIT
--              11 - Right-hand elements of WHATISIT
--
--     ENERGY    Energy involved (MeV)
--     HEIGHT    Height involved (counts/keV/uC/msr or counts)
--     STRING    Packed string for plotting (usually)
--
--     OUTPUTS:  Plotted via Complot
--
--     COMMON BLOCKS:     RUMP, GRAPHICS
--     CALLED FROM:       APLOT, TSTMRK, ANLYTC
--     CALLS:             NORMK, PLSUPP, Complot
--
=========================================================================== */
typedef struct _myvec {
	REAL dx,dy;
	int pen;
} MYVEC;

static MYVEC cross[] = {{.0f,.3f,3}, {.0f,-.3f,2}, {-.3f,.0f,3}, {.3f,.0f,2} };
static MYVEC arrow[] = {{.1f,.1f,2}, {.1f,-.1f,3}, { .0f,.0f,2}, {.3f,.0f,2} };


void RbsMark(int key, REAL energy, REAL height, char *string) {

	REAL x_1, x_2, x_3, y_1, y_2, y_3, ht, g2, c2, xout, yout;
	static REAL what_xmin, what_xmax;			/*  For Whatisit State */
	int j, slen;
	MYVEC *sequence;

/*  .. Preliminary computations */
	if (key != MK_CUR && key != MK_TIK) {
		g2 = 0;													/* Presumed 1/2 string length */
		c2 = 0.5f * Rmp->chrsiz;							/* Also useful parameter		*/
		if ( (slen = (int) strlen(string)) > 0)		/* Measure length of string	*/
			g2 = PlotQueryStringLength(Rmp->chrsiz,string,slen) / 2;
	}

	if (key == MK_CUR) *string = 27;						/* Make it an escape string	*/
	if (! PlotSystem(2, NULL, NULL)) return;

	RbsChgpen(FALSE);
	PlotSetRange(Rmp->emin,Rmp->emax,Rmp->ymin,Rmp->ymax);		/* in case someone else left mess */
	PlotSetLineType(1, 0.0);					/* ditto */

/*  * * * * Plot and label crosshair (key 1 or 7) */

	switch(key) {
		case MK_XHR:
		case MK_LAR: 
			if ((energy < Rmp->emin) || (energy > Rmp->emax)) return;
			ht = height;
			if (Rmp->raw) ht=ht/RbsNormK(ibuf);
			if ((Rmp->linear == GR_LOG) && (ht > 0.0))
				ht = (REAL) log10(ht);
			if ((Rmp->linear == GR_SQR) && (ht >= 0.0))
				ht = (REAL) sqrt(ht);
			if (ht >= Rmp->ymax || ht < Rmp->ymin) return;
			PlotConvert2DScales(USER_TO_INCH, energy,ht, &x_1,&y_1);
			PlotSetUserMode(FALSE);
			if (key != MK_LAR)   {
				xout = x_1 - g2;	/*  Text position for crosshair */
				yout = y_1 + 0.30f + c2;
				sequence = cross;	/*  Data structure for crosshair */
			} else {
				xout = x_1 + .5f;	/*  Text position for left arrow */
				yout = y_1 - c2;
				sequence = arrow;	/*  Data structure for left arrow */
			}
			for (j=0; j < 4 ; j++ ) {
				PlotMove(x_1+sequence[j].dx,y_1+sequence[j].dy,sequence[j].pen);
			}
			goto MarkExit;

/*  ... Plot element marker */
		case MK_TKL:
			if ((energy < Rmp->emin) || (energy > Rmp->emax)) return;

			if (height > 0.0) {
				ht = height;
				if (Rmp->raw) ht /= RbsNormK(ibuf);
				if ((Rmp->linear == GR_LOG) && (ht > 0.0))
					ht = (REAL) log10(ht);
				if ((Rmp->linear == GR_SQR) && (ht >= 0.0))
					ht = (REAL) sqrt(ht);
			} else {
				ht = Rmp->ymin;
			}

			PlotSetLineType(4, 0.0);		/* Dotted line marker */
			PlotMove(energy, ht, 3);		/* Energy plot always accurate */
			ht += (Rmp->ymax-Rmp->ymin)/20.0f;
			PlotMove(energy, ht, 2);
			PlotSetLineType(1,0.0);

			/* ... Move the label itself up just slightly */
			PlotConvert2DScales(USER_TO_INCH, energy, ht, &x_1, &y_1);
			PlotConvert2DScales(INCH_TO_USER, x_1-g2, y_1+c2, &xout, &yout);
			goto MarkExit;

/*  ... Plot energy marker (Key 2 or 3) */
		case MK_TIK:
		case MK_TKH:  
			if ((energy < Rmp->emin) || (energy > Rmp->emax)) return;

			if (key == MK_TKH) {
				ht = height;
				if (Rmp->raw) ht=ht/RbsNormK(ibuf);
				if ((Rmp->linear == GR_LOG) && (ht > 0.0))
					ht = (REAL) log10(ht);
				if ((Rmp->linear == GR_SQR) && (ht >= 0.0))
					ht = (REAL) sqrt(ht);
				ht = min ( .95f*Rmp->ymax+.05f*Rmp->ymin, max ( .05f*Rmp->ymax+.95f*Rmp->ymin, ht ) );
			}

			PlotSetLineType(4,0.0);
			PlotMove(energy,Rmp->ymin,3);	/*  Energy plot always accurate */
			if (key == MK_TIK) {
				PlotMove(energy,Rmp->ymin+(Rmp->ymax-Rmp->ymin)/20.0f,2);
			} else {
				PlotMove(energy,ht,2);
			}
			PlotSetLineType(1,0.0);

			PlotFlush();	/*  Cleanup */
			return;

/* ... WHATISIT processing (Key 4 or 5) */

		case MK_WH1:
		case MK_WH2:  
		case MK_WHL:
		case MK_WHR:
			x_1 = x_2 = energy;
			y_1 = Rmp->ymin;
			y_2 = Rmp->ymin+0.05f*(Rmp->ymax-Rmp->ymin);
			PlotConvert2DScales(USER_TO_INCH, x_1,y_1, &x_1,&y_1);
			PlotConvert2DScales(USER_TO_INCH, x_2,y_2, &x_2,&y_2);
			y_3 = y_2 + y_2 - y_1;						/*  Next line goes 2x as far */

			if (key == MK_WH1)   {					/*  Never shift the first one */
				what_xmin = x_1 - g2 - Rmp->chrsiz;
				what_xmax = x_1 + g2 + Rmp->chrsiz;
				x_3 = x_1;
			} else if (key == MK_WHL) {					/*  Left side case		*/
					x_3 = min(x_1, what_xmin-g2);			/*  Shift if necessary	*/
					what_xmin = x_3 - g2 - Rmp->chrsiz;	/*  Always adjust xmin	*/
			} else {												/*  Right side case		*/
					x_3 = max(x_1, what_xmax+g2);			/*  Shift if necessary	*/
					what_xmax = x_3 + g2 + Rmp->chrsiz;	/*  Always adjust xmax	*/
			}

			PlotSetUserMode(FALSE);
			PlotMove(x_1, y_1, 3);						/* Start w/ line up				*/
			PlotMove(x_2, y_2, 2);						/* Line down up a bit			*/
			PlotMove(x_3, y_3, 2);						/* Line down to under element */

			xout = x_3 - g2;
			yout = y_3 + c2;
			goto MarkExit;
			
/* ... Key 8: Simple text */
		case MK_TIT:  
			PlotConvert2DScales(USER_TO_INCH, Rmp->emin,Rmp->ymax, &x_1,&y_1);
			PlotSetUserMode(FALSE);
			PlotString(x_1+.2f,y_1-.4f,0.2f,string,0.0, (int) strlen(string));
			PlotSetUserMode(TRUE);
			PlotFlush();
			return;

		default:
			TTYprintf("Internal ERROR: unimplemented key (%d) passed to MARK.\n",key);
			return;
	}

/*  * * * * Most code exits here (not cursor) */
MarkExit:  
	PlotString(xout,yout,Rmp->chrsiz,string,0.0, (int) strlen(string));
	PlotSetUserMode(TRUE);
	PlotFlush();
	return;
}

/* ===========================================================================
-- Cursor function Pulled out of Mark
--
-- Usage: int RbsCursor(REAL *energy, REAL *height, int *button);
--
-- Inputs: none
--
-- Output: energy - if not NULL, energy at cursor or 0 on error
--         height - if not NULL, height at cursor or 0 on error
--         button - if not NULL, button pressed.  <ESC> on error.
--
-- Returns: button pressed to exit cursor, or <ESC> on no cursor.
=========================================================================== */
int RbsCursor(REAL *energy, REAL *height, int *button) {

	REAL xcurs,ycurs;
	int rcode;
 
	PlotSetRange(Rmp->emin,Rmp->emax, Rmp->ymin,Rmp->ymax);		/* in case of earlier mess */

/* Call cursor if plotter is on and a cursor is available */
	if (PlotSystem(2, NULL, NULL) && PlotSystem(3, NULL, NULL)) {
		PlotCursor(&xcurs, &ycurs, &rcode);
		if (Rmp->linear == GR_LOG) ycurs = (REAL) pow(10.0,ycurs);
		if (Rmp->linear == GR_SQR) ycurs = ycurs*ycurs;
	} else {
		xcurs = ycurs = 0;
		rcode = 0x1B;								/* Return <ESC> on error */
	}

	if (energy != NULL) *energy = xcurs;
	if (height != NULL) *height = ycurs;
	if (button != NULL) *button = rcode;

	return(rcode);
}


/* ===========================================================================
--  Usage Guide:
--
--      BOOL FUNCTION AXDRAW(IBF)
--
--  Quick: Routine to draw the axis for RUMP program
--
--     INPUTS:   IBF      Number of buffer which determines scales
--
--     OUTPUTS:  Plot of the axis
--
--     COMMON BLOCKS:     RUMP, GRAPHICS
--     CALLED FROM:       APLOT
--     CALLS:             RANGE, PLSUPP, CHGPEN, NORMK, Complot
--
=========================================================================== */
int RbsAxdraw(SPECTRUM *ibf) {

	int mx,my,mex,nchar,i,itype;
	REAL dx,dx2,ex,ex2,dy,dy2, x1,x2;
	int onflag;

	static char *labtxt[] = {
		"Normalized Yield",				/* Yield (#/uC/keV/msr)	*/	/* Linear */
		"Counts",
		"Yield",
		"Yield (#/uC/keV/msr)^{1/2}",										/* SQRT */
		"Counts^{1/2}",
		"Yield^{1/2}",
		"Yield log(#/uC/keV/msr)",											/* LOG */
		"log_{10}(Counts)",
		"log"
	};

/* ... Go get a new frame (RET on error) */
	if ( ! PlotSystem(1, NULL, NULL)) return(FALSE);
	PlotSelectPen(1);							/*  AXIS ALWAYS IN BLACK */
	PlotInformAxesLimits(-1);				/* Possibly color inner region */
	PlotFlush();

/* ... Set X Scale */
	PlotAutoScale(Rmp->chmin,Rmp->chmax,&x1,&x2,&dx,&dx2,&mx);
	if (! Rmp->forcex)   {
		Rmp->chmin = x1;
		Rmp->chmax = x2;
	}
	Rmp->emin = (REAL) RBSENERGY(Rmp->chmin, ibf);
	Rmp->emax = (REAL) RBSENERGY(Rmp->chmax, ibf);

/* ... Set Y Scale */
	if (Rmp->linear == GR_LOG)   {
		Rmp->ymin = Rmp->raw ? 0.0f : (REAL) log10(RbsNormK(ibf));
		if (Rmp->cospec == 0)   {
			Rmp->ymax = RbsRange(Rmp->chmin,Rmp->chmax,ibf);
			if (Rmp->ymax <= 0) Rmp->ymax = 1.0f;
			Rmp->ymax = (REAL) (Rmp->ymin + log10(Rmp->ymax));
			PlotAutoScale(Rmp->ymin,Rmp->ymax,&Rmp->ymin,&Rmp->ymax,&dy,&dy2,&my);
		} else {
			Rmp->ymax = (REAL) log10(Rmp->cospec);
			PlotAutoScale(Rmp->ymin,Rmp->ymax,&Rmp->ymin,&x2,&dy,&dy2,&my);	/*  Don't round top */
     }

	} else {															/*  Linear or Sqrt */
		Rmp->ymin = 0.0;
		Rmp->ymax = Rmp->cospec;
		if (Rmp->cospec == 0)   {
			Rmp->ymax = RbsRange(Rmp->chmin,Rmp->chmax,ibf);
			if (! Rmp->raw) Rmp->ymax = Rmp->ymax * RbsNormK(ibf);
			if (Rmp->linear == GR_SQR) Rmp->ymax = (REAL) sqrt(Rmp->ymax);
			PlotAutoScale(Rmp->ymin,Rmp->ymax,&Rmp->ymin,&Rmp->ymax,&dy,&dy2,&my);
		} else {														/* User chosen range */
			if (Rmp->linear == GR_SQR) Rmp->ymax = (REAL) sqrt(Rmp->ymax);
			PlotAutoScale(Rmp->ymin,Rmp->ymax,&x1,&x2,&dy,&dy2,&my);	/* Don't round ends */
		}
	}

/* ... Plot the lower X axis */
	PlotSetRange(Rmp->chmin,Rmp->chmax,Rmp->ymin,Rmp->ymax);
	onflag = (Rmp->labels == GR_ON);
	itype = 0;														/* NORMAL X AXIS */
	nchar = 0;
	if (onflag) nchar=7;
	if (! Rmp->mtick) dx2 = 0.0;										/* MINOR TICK MARKS */
	if (Rmp->labels == GR_BRF) {
		dx2 = 0.0;	/*  NO MINOR */
		itype = 2;	/*  BRIEF X AXIS */
		nchar = 7;
	}
	PlotAxis(Rmp->chmin,Rmp->ymin,itype,Rmp->chmax-Rmp->chmin,dx,dx2,"Channel",nchar,mx, 0, NULL);

/* ... Draw the left hand Y axis */

	itype = 13;														/* NORMAL Y AXIS */
	nchar = 0;
	if (! Rmp->mtick) dy2 = 0.0;										/* MINOR TICK MARKS */
	if (Rmp->labels == GR_BRF) {
		dy2 = 0.0;
		itype = 15;													/* BRIEF Y AXIS */
	}

	i = 0;
	if (Rmp->linear == GR_LIN) i = 0;		/* Choose which Y axis label to use */
	if (Rmp->linear == GR_SQR) i = 3;
	if (Rmp->linear == GR_LOG) i = 6;

	if (Rmp->raw) {								/* Next one up is for RAW mode */
		 i++;
	} else if (Rmp->labels == GR_BRF) {		/* Second up is for brief labels */
		i += 2;
	}

	nchar = 0;
	if (Rmp->labels != GR_OFF) nchar = (int) strlen(labtxt[i]);
	PlotAxis(Rmp->chmin,Rmp->ymin,itype,Rmp->ymax-Rmp->ymin,dy,dy2,labtxt[i],nchar,my, 0, NULL);

/* ... Plot the upper X axis ... */

	PlotAutoScale(Rmp->emin,Rmp->emax,&x1,&x2,&ex,&ex2,&mex);

	PlotSetRange(Rmp->emin,Rmp->emax,Rmp->ymin,Rmp->ymax);				/* Do energy axis on top */
	itype = 4;												/* Normal X axis again */
	nchar = 0;
	if (onflag) nchar = -12;
	if (! Rmp->mtick) ex2 = 0.0;								/* MINOR TICK MARKS */
	if (Rmp->labels == GR_BRF) {
		ex2 = 0.0;
		itype = 6;											/*  BRIEF X AXIS */
		nchar = -6;											/*  NO LABEL */
	}
	PlotAxis(Rmp->emin,Rmp->ymax,itype,Rmp->emax-Rmp->emin,ex,ex2, "Energy (MeV)",nchar,mex, 0, NULL);

/* ... Draw the right hand Y axis */
	PlotAxis(Rmp->emax,Rmp->ymin,5,Rmp->ymax-Rmp->ymin,dy,dy2," ",0,50, 0,NULL);

/* ... Do the time/date marker */
#if 0
	if (onflag) {
		dater = date_a(temp);
		time_a(temp);
		temp[5] = '  ';
		sstr_a(temp[5],10,-1,' ');
		PlotSymbol(0.05,0.05,.11,temp,0.0,17);
	}
#endif

/*  ***** Final Cleanup ***** */
/* axtrak;	 Hook into (usually null) process */

	if (Rmp->AutoLineType) Rmp->linetype = Rmp->linetypestart;
	if (Rmp->AutoSymbols)  Rmp->symtype  = Rmp->symtypestart;
	RbsChgpen(FALSE);									/*  RESET PEN TO NORMAL MODE */
	for (i=0; i <= RbsNumBuf ; i++ )
		if (buffers[i] != NULL) buffers[i]->iddone = FALSE;

	PlotID(0,0,0,0,"**RESET**");	/*  Point to first slot */
	PlotFlush();
	return(TRUE);										/*  Basically successful */
}

/* void axtrak{ return; } */

/* ===========================================================================
--  Usage Guide:
--
--      SUBROUTINE CHGPEN(AUTO)
--
--  Quick: Changes color of pen if necessary
--
--     INPUTS:   AUTO     BOOLEAN VARIABLE INDICATING IF COLOR CHANGE VIA AUTO
--                        MODE SHOULD OCCUR THIS TIME.
--
--     COMMON BLOCKS:     GRAPHICS
--     CALLED FROM:       APLOT, AXDRAW, PLDATA
--     CALLS:             PLSUPP
--
=========================================================================== */
void RbsChgpen(int automode){

	int parm;

/*  -- Code begin -- */
	parm = automode ? 1 : 0;
	PlotSystem(5, NULL, &parm);
	return;
}

/* ===========================================================================
--  Usage Guide:
--
--      SUBROUTINE PLDATA(CHMIN,CHMAX,IBF)
--
--  Quick: Actually outputs the spectrum to the plot software
--
--     INPUTS:   CHMIN    CHANNEL NUMBER OF STARTING POINT
--               CHMAX    CHANNEL NUMBER OF ENDING POINT
--               IBF      BUFFER NUMBER TO PLOT
--
--     OUTPUTS:  Plotted data, if it is within range
--
--     COMMON BLOCKS:     RUMP, GRAPHICS
--     CALLED FROM:       APLOT, ANLYTC
--     CALLS:             CHGPEN, IDS, NORMK, Complot
--
=========================================================================== */
void RbsPldata(PLOT_PARMS *parms) {

/*  -- Local Variables -- */
	int	i, ist, iend;
	REAL xmin, xmax, xplmin, x, ymint, ymaxt, do_min, do_max;
	int	lwidthhold, penhold;
	int   linetype, symtype, npoint;
	REAL symsize, shift, offset;
	SPECTRUM *ibf;
	
/*  -- Code begin -- */
	ibf  = parms->ibf;
	xmin = parms->xmin;
	xmax = parms->xmax;
	linetype = (parms->linetype >= 0) ? parms->linetype : Rmp->linetype ;
	symtype  = (parms->symbol   >= 0) ? parms->symbol   : Rmp->symtype ;
	symsize  = (parms->symsize  >  0) ? parms->symsize  : Rmp->SymSize ;
	npoint   = (parms->npoint   >  0) ? parms->npoint   : Rmp->npoint ;
	shift    = parms->shift;
	offset   = parms->offset;

	if (xmin == 0 && xmax == 0) {
		xmin = ibf->first;
		xmax = ibf->first + ibf->npt - 1;
	} else if (xmin >= xmax) {
		ERRprintf("ERROR: Plot range out of bounds (PLDATA)\n");
		return;
	}

/*  *** PLOT VIA ENERGY RATHER THAN CHANNELS.  ALL WE NEED TO DO IS */
/*  *** MODIFY THE X PARMS IN SET SUBROUTINE CALL. */
	ymint = Rmp->ymin;
	ymaxt = Rmp->ymax;
	if (! Rmp->raw) {									/* Correct ymint for raw mode */
		x = 1.0f/RbsNormK(ibf);
		if (Rmp->linear == GR_SQR) {				/* Square root correction	*/
			ymint = (REAL) ((ymint-offset)*sqrt(x));
			ymaxt = (REAL) ((ymaxt-offset)*sqrt(x));
		} else if (Rmp->linear == GR_LOG) {		/* Logarithmic correction	*/
			ymint = (REAL) (ymint + log10(x) - offset);
			ymaxt = (REAL) (ymaxt + log10(x) - offset);
		} else {											/* Linear correction			*/
			ymint = (ymint-offset)*x;
			ymaxt = (ymaxt-offset)*x;
		}
	}

/* This is the channel equivalent of the energy range for this buffer */
	if (Rmp->xaxis_mode == X_CHANNEL) {		/* Do via channel # only? */
		do_min = Rmp->chmin; 
		do_max = Rmp->chmax;
	} else {											/* Or correct energy scales */
		do_min = RBSCNNLE(Rmp->emin,ibf);
		do_max = RBSCNNLE(Rmp->emax,ibf);
	}
	do_min -= shift; do_max -= shift;		/* Handle shift request */
	PlotSetRange(do_min, do_max, ymint,ymaxt);

	xmin = max(xmin, do_min);					/* Don't bother outside of window */
	xmax = min(xmax, do_max);
	if (xmin > xmax) return;					/* Nothing to do! */

	ist = (int) (xmin - ibf->first + 0.5);	/* xmin is lowest channel */
	if (ist < 0) ist = 0;
	xplmin = ibf->first + ist;					/* X value corresponding to xmin */
	iend = (int) (xmax - ibf->first + 0.5);
	if (iend > ibf->npt-1) iend = ibf->npt-1;
	i = iend - ist + 1;

/* Change pen and linewidth if necessary */
	if (parms->pen       >= 0) penhold    = PlotSelectPen(parms->pen);
	if (parms->linewidth >  0) lwidthhold = PlotSetLineWidth(parms->linewidth);

/* Do the plot */
	PlotYPlot(ibf->counts+ist, i, xplmin, 1.0, linetype, symtype, symsize, npoint, Rmp->linear);
	PlotFlush();														/* Clear point mode */

/* Do the identifier if required */
	if (parms->doids || (Rmp->autoid && !ibf->iddone)) {
		PlotID(linetype, symtype, -1, LINEWIDTH, (*ibf->id != '\0') ? ibf->id : ibf->filename);
		if (ibf == ALTBUF)  SimDrawSample();	/*  Simulation list on plot */
		ibf->iddone = TRUE;
	}
	if (parms->localids) PlotID(linetype, symtype, -1, LINEWIDTH, parms->ids);

/* And reset things as necessary */
	if (parms->pen       >= 0) PlotSelectPen(penhold);
	if (parms->linewidth >  0) PlotSetLineWidth(lwidthhold);
	PlotFlush();

#if 0
	if (ibf == ALTBUF) ibf = MAINBUF;      /* Never point at ALT after plot */
#endif
	return;
}


/* ===========================================================================
--  Usage Guide:
--
--      SUBROUTINE SWMODE(NEWLIN)
--     Subroutine to switch modes of operation: modify YMIN and YMAX to reflect
--     the new value of LINEAR.
--  Quick: Switches y-axis mode (lin/log/sqrt)
--
--     INPUTS:   NEWLIN   Desired value of variable LINEAR
--
--     OUTPUTS:  New values for LINEAR, plot scaling
--
--     COMMON BLOCKS:     RUMP, GRAPHICS
--     CALLED FROM:       APLOT
--     CALLS:             None
--
=========================================================================== */
void RbsSwmode(VERTMODE newlin) {

/* ... First, convert existing ymin/ymax to a linear scale */
	if (Rmp->linear == GR_SQR)   {
		Rmp->ymin = Rmp->ymin * Rmp->ymin;
		Rmp->ymax = Rmp->ymax * Rmp->ymax;
	} else if (Rmp->linear == GR_LOG)   {
		Rmp->ymin = 0.0;
		Rmp->ymax = (REAL) pow(10.0,Rmp->ymax);
	}

/* ... YMAX and YMIN are now linear.  Change them now to new state */
	if (newlin == GR_SQR) {
		Rmp->ymin = (REAL) sqrt(Rmp->ymin);
		Rmp->ymax = (REAL) sqrt(Rmp->ymax);
	} else if (newlin == GR_LOG) {
		Rmp->ymax = (REAL) log10(Rmp->ymax);
		Rmp->ymin = Rmp->ymax - 6.0f;
	}

	Rmp->linear = newlin;
	return;
}

/* ===========================================================================
--  Usage Guide:
--
--      FUNCTION RANGE(XMIN,XMAX,IBF)
--     This functions simply finds the maximum counts over a range of
--     channels.
--  Quick: Finds maximum counts over a range of channels
--
--     INPUTS:   CHMIN    Lower limit of channels desired
--               CHMAX    Upper limit of channels desired
--               IBF      Buffer number for scanning
--
--     OUTPUTS:  RANGE    The Maximum counts in the specified range
--
--     COMMON BLOCKS:     RUMP
--     CALLED FROM:       AXDRAW
--     CALLS:             None
--
=========================================================================== */
REAL RbsRange(REAL xmin, REAL xmax, SPECTRUM *ibf) {

	int ist,iend,i;
	REAL range;

/*  -- Code begin -- */
	range = 0.0;
   ist = (int) (xmin - ibf->first + 1.5);
	if (xmin > xmax  || ist >= ibf->npt) {
		gen_warn("Range parameters out of bounds");
		return(0.0);
	}
   if (ist < 1) ist = 1;
   iend = (int) (xmax - ibf->first + 1.5);
	if (iend > ibf->npt) iend = ibf->npt;
	for (i=ist; i<=iend ; i++) {
      if (range < ibf->counts[i]) range = ibf->counts[i];
	}
	return(range);

}

/* ===========================================================================
--  Usage Guide:
--
--      REAL FUNCTION GETMEV(KEY, DEF, CMSG, KMSG)
--  GETMEV is a utility for cursor or keyboard input of data for
--  support of the RUMP RBS analysis program.  The function
--  result is the Energy (MeV) entered, either by cursor or keyboard
--  input of the channel number.  Parameters are:
--  Quick: Queries user for energy via cursor or keyboard
--
--     KEY       Sum of the following subkeys:
--               1: Take input if possible from cursor; this flag gets
--               reset (ARGUMENT IS MODIFIED) if the escape key is
--               pressed in response, or if the plotter/cursor flag
--               combination is not all on.
--               2: Draw a tickmark at the resulting energy.
--
--     DEF       Default channel to be used in keyboard input.
--               Not used for cursor input.
--
--     CMSG      Message given to prompt cursor input.
--               If null ( meaning ' ' ) nothing is TYPERed, so no
--               extra blank line appears.
--
--     KMSG      Prompt message passed along to LEXP in the case of
--               keyboard input.
--
--     COMMON BLOCKS:     RUMP, GRAHPICS
--     CALLED FROM:       ANLYTC, NEWPROF
--     CALLS:             MARK
--
=========================================================================== */
REAL RbsGetMeV(int *key, REAL def, char *cmsg, char *kmsg) {

	REAL result, yval;

/* Are we to try the cursor input mode? */
	if (*key & 0x01) {								/* Request cursor input */
		if (! PlotSystem(3, NULL, NULL)) {		/* Cursor available?		*/
			*key &= ~0x01;
		} else {
			if (cmsg != NULL) {TTYputs(cmsg); TTYflush();}
			if (RbsCursor(&result, &yval, NULL) == 0x1B) *key &= ~0x01;
			if (cmsg != NULL) TTYputs("\n");
		}
	}

/* If cursor failed, or if requested, use simple input via command line */
	if (! (*key & 0x01)) {							/* No longer cursor mode */
		result = (REAL) RBSENERGY(LexGetReal(def,kmsg), ibuf);
		yval   = 0;
	}

/* And, are we to graph this mess? */
	if (*key & 0x02) RbsMark(MK_TIK, result, yval, NULL);
	return(result);
}

/*  Quick: Graphical display of simulation structure */
/* void disp() { } Deleted here for the time being */
