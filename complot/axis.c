/* AXIS.F77 and NLINAX.F77 */

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
#include "tplot.h"
#include "extends.h"
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
static int ndigit(double rnum);
PRIVATE void DoAxes(REAL xu, REAL yu, INTEGER itype, REAL sizeu, REAL udx, REAL udx2, 
			   CHAR *title, INTEGER tlen, INTEGER mu, REAL (*conv)(REAL x), PLOT_AXIS_LABELS *labels,
				INTEGER color, INTEGER axtype);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */
extern REAL Plot_Z_Default;					/* Default Z value on 2D plot	 */
extern REAL Plot_Z_Inch_Default;				/* Private in plot/axis/symbol */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
#define	PLOTMOVE(x,y,n)		PlotMove((REAL) (x), (REAL) (y), n)

/* ============================================================================
--     Main axis plotting routine.
--     Compatability with complot is maintained by calling this AXIS.
--     Draws either a X or Y axis with specified major and minor tick marks
--     and title.  Can turn off minor tick or labels if desired.
--
--     Usage:     CALL   AXIS(X,Y,ITYPE,SIZE,DX,DX2,TITLE,N,M)
--     Usage:     CALL NLINAX(X,Y,ITYPE,SIZE,DX,DX2,TITLE,N,M,CONV)
--     Usage:     CALL LOGAXIS(X,Y,ITYPE,SIZE,DX,DX2,TITLE,N,M)
--
--     Inputs:    X       -  X coordinate (user) of axis start
--                Y       -  Y coordinate (user) of axis start
--                ITYPE   -  Bit specifications for functionality
--                           0 (001) => Y axis type function
--                           1 (002) => Use brief labeling mode
--                           2 (004) => Turn ticks mark in instead of out
--                           3 (008) => Label ticks marks vertically vs. horiz
--                           4 (016) => Suppress labelling of tick marks
--                           5 (032) => Suppress minor tick marks
--                           6 (064) => Suppress all tick marks
--                           7 (128) => Suppress everything (ie. do nothing)
--                           8 (256) => Use special logarithmic labelling mode
--                           9 (512) => Don't justify the first/last labels
--                         10 (1024) => Draw full grid at major & minor ticks
--                SIZE    -  Length of axis (user coordinates)
--                DX      -  Spacing of major tick marks (labelled)
--                DX2     -  Spacing of minor tick marks (unlabelled)
--                           Linear and Non-linear
--                              <>0 draw at specified value
--                              = 0 not drawn
--                           Log
--                              >0 -> Draw at given spacing 10**n type
--                               0 -> No minor tick marks
--                              <0 -> -1 Draw at 5
--                                    -2 Draw at 2,5
--                                    -3 Draw at 2,3,4,5,6,7,8,9
--                                    -4 Draw at 1.1,1.2 ... 5.0, 5.2, ... 9.8
--                TITLE   -  Hollerith label for the axis.  No argument should
--                           be a single space (' ')
--                N       -  + OR - number of characters in the title
--                           + draws labels and title to left or below axis
--                           - draws labels and title to right or above axis
--                           0 no title or labels are drawn
--                M       -  describes the type of labels to be drawn at space
--                           - non zero implies integer format of labels
--                           + floating point with m digits to right of decimal
--                           >10 => Disable labeling (same as +16 above)
--                           0 auto determination of decimal mode
--                CONV    -  Entry point to subroutine to do the non-linear
--                           conversion from XU to scaled units.
--                           ie. XPLOT = CONV(XU) in user coordinates
--
--     NOTES:  1. The program will automatically enter a scientific mode if
--                too many digits would be drawn on the axis labels.  The size
--                of the label character is varied depending on number digits.
--                If N=0, the tick marks will be drawn on the "outside" of the
--                graph.
--                Note that ITYPE = 6 is X axis with tick marks in and Brief
--                labelling of tick marks.
--
--             2. A typical publication quality plot has X axis mode 4, and
--                Y mode 13 for the labelling.
--
--             3. For LOG axes, it is assumed the axis coordinate represents
--                the common logarithm of the variable.  ie. 18 => 10**18.
--
--     Programmers: The handling of X and Y axis in a single coherent routine
--                  creates many pains.  The schemo is to exchange X and Y
--                  directly for the Y routine and handle different movements
--                  for labels and tickmarks.  Several logical and integer
--                  variables specify the direction and type of axis.
--
-- April 29, 1987 - Added key 32 to ITYPE above.  Modified handling of TMINOR.
--
-- April 25, 1988 - Removed disabling of exponentials labels for M>10.  Now
--                  must use the key +16
============================================================================= */
#define LIN		0											/* Linear axes */
#define NLIN	1											/* Non-linear axes */
#define LOG		2											/* Logarithmic axes */

#define label_shift			0.7f			/*  Labels below axes (csize) */
#define label_extra_shift	0.6f			/* Extra shift if unjustified */
#define title_shift			0.8f			/* Shift of title below labels */
#define log_shrink			0.7f			/* Logarithmic shrink */

void PlotAxis(REAL x, REAL y, INTEGER bflag, REAL size, REAL dx, REAL dx2, 
			 CHAR *title, INTEGER tlen, INTEGER m, INTEGER color, PLOT_AXIS_LABELS *labels) {
	DoAxes(x, y, bflag, size, dx, dx2, title, tlen, m, NULL, labels, color, LIN);
	return;
}

void PlotLogAxis(REAL x, REAL y, INTEGER bflag, REAL size, REAL dx, REAL dx2, 
			    CHAR *title, INTEGER tlen, INTEGER m, INTEGER color, PLOT_AXIS_LABELS *labels) {
	DoAxes(x, y, bflag, size, dx, dx2, title, tlen, m, NULL, labels, color, LOG);
	return;
}

void PlotNonLinearAxis(REAL x, REAL y, INTEGER bflag, REAL size, REAL dx, REAL dx2, 
			 CHAR *title, INTEGER tlen, INTEGER m, INTEGER color, PLOT_AXIS_LABELS *labels, REAL (*conv)(REAL xt) ) {
	DoAxes(x, y, bflag, size, dx, dx2, title, tlen, m, conv, labels, color, NLIN);
	return;
}

/* ----------------------------------------------------------------------------
   Common entry point for all axes routines
---------------------------------------------------------------------------- */
PRIVATE void DoAxes(REAL xu, REAL yu, INTEGER bflag, REAL sizeu, REAL udx, REAL udx2, 
						  CHAR *title, INTEGER tlen, INTEGER mu, REAL (*conv)(REAL x), PLOT_AXIS_LABELS *labels,
						  INTEGER color, INTEGER axtype) {

	int		i, j, flipall,								/* Dummy variables */
				width_hold, color_hold;					/* Current color/width */
	double	dx,dx2,										/* Spacing of tick marks */
				X1,X2,Y1,Y2,Y3,Y4,Y5,Y6,dy,ytop,		/* Temporary coordinates */
				xstart,xend, ystart,zstart,size,		/* Starting point and length */
				xs_user,										/* Real scale xstart, xend */
				xhighm,										/* Upper end */
				xfact2,										/* Scale conversion terms */
				theta,										/* Angle of axis (Y = 90) */
				thdraw,										/* Angle for labelling */
				length,lenmax,								/* Length of symbol */
				xdraw,ydraw,								/* Coordinates for labelling */
				xleft,xright,								/* Boundaries for labels */
				fctr,											/* Labeling factor */
				csize,ts,									/* Character and title size */
				xmajor=0.0f,								/* Position of tick marks */
				xminor=0.0f,								/* Position of tick marks */
				xhold,temp,ytmp,							/* Temp variables */
				ymid,xmid;									/* Mid-range of plot region */
	REAL		ui[4][4],									/* User to inches */
				gm[4][4];									/* Work transform */
	int		iexp,m,										/* Exponents in labels */
				isn,											/* Sign of title and # chrs */
				idx,											/* Index counter for determining type of log tertiary labels */
				imode;										/* Mode of log ticks */
	long int index;										/* Tick mark indicies */
	CHAR		buf[21],buf2[21];							/* Conversion buffer */
	LOGICAL	brief,										/* Brief labeling in effect? */
				tickin,										/* Tick marks in? */
				rotate,										/* Rotated labelling? */
				dotitle,										/* Is the title to be drawn */
				do_tertiary_labels,						/* Are tertiary logarithmic labels drawn */
				logdec,										/* Label log as power 10 only */
				justify,										/* Justify left/right labels */
				grid,											/* Add grid lines */
				ztype,										/* Z-axis type axis */
				tertiary,									/* Tertiary grid mark? */
				xtype,ytype,								/* Type of axis */
				ilab,											/* Any labeling on? */
				sci,											/* Scientific mode labeling */
				tminor,tmajor,								/* Tick marks left to do */
				first,										/* First tick mark? */
				lstone,										/* Last tick mark? */
				logticks,									/* Strange logithmic ticks */
				above,										/* Is title above tick labels */
				logtmp;

	INTEGER	savek, lthold;								/* Status saving variables */
	LOGICAL	usrsav;
	REAL		save[4];

	struct {INTEGER Line, Major, Minor, Labels, Title, Tertiary; } Color;
	struct {INTEGER Line, Major, Minor, Labels, Title, Tertiary; } Width;

/* Inline function definition to handle interchange of X and Y coordinates */
#define	xt(x,y,z)	(ui[0][0]*(x)+ui[0][1]*(y)+ui[0][2]*(z)+ui[0][3])
#define	yt(x,y,z)	(ui[1][0]*(x)+ui[1][1]*(y)+ui[1][2]*(z)+ui[1][3])
#define	zt(x,y,z)	(ui[2][0]*(x)+ui[2][1]*(y)+ui[2][2]*(z)+ui[2][3])

#define xpl(x,y)	(xtype ? (x) : (y))				/* Switch coordinates */
#define ypl(x,y)	(xtype ? (y) : (x))				/* Switch coordinates */

/* ... Copy the user parameters */
	dx  = (udx  > 0.0f) ? udx  : -udx ;				/* Require SPACE positive */
	dx2 = (udx2 > 0.0f) ? udx2 : -udx2;				/* Require SPACE2 positive */
	m   = mu;												/* Labelling mode */
	width_hold = PlotWindow->jstyle;					/* Save line width */
	color_hold = PlotWindow->colour;					/* And color */

/* Make local values of the line widths and colors for parts of the axis */
	Width.Line     = (INTEGER) (width_hold * PL_Axis.Width.Line     + 0.5f);
	Width.Major    = (INTEGER) (width_hold * PL_Axis.Width.Major    + 0.5f);
	Width.Minor    = (INTEGER) (width_hold * PL_Axis.Width.Minor    + 0.5f);
	Width.Labels   = (INTEGER) (width_hold * PL_Axis.Width.Labels   + 0.5f);
	Width.Title    = (INTEGER) (width_hold * PL_Axis.Width.Title    + 0.5f);
	Width.Tertiary = (INTEGER) (width_hold * PL_Axis.Width.Tertiary + 0.5f);

/* Choose all the colors ... settings of $AXCOLR[] overrides individual parameters */
	Color.Line     = (PL_Axis.Color.Line > 0)     ? PL_Axis.Color.Line     : (color > 0) ? color : color_hold;
	Color.Major    = (PL_Axis.Color.Major > 0)    ? PL_Axis.Color.Major    : (color > 0) ? color : color_hold;
	Color.Minor    = (PL_Axis.Color.Minor > 0)    ? PL_Axis.Color.Minor    : (color > 0) ? color : color_hold;
	Color.Labels   = (PL_Axis.Color.Labels > 0)   ? PL_Axis.Color.Labels   : (color > 0) ? color : color_hold;
	Color.Title    = (PL_Axis.Color.Title > 0)    ? PL_Axis.Color.Title    : (color > 0) ? color : color_hold;
	Color.Tertiary = (PL_Axis.Color.Tertiary > 0) ? PL_Axis.Color.Tertiary : (color > 0) ? color : color_hold;

/* ... Determine modes from value of BFLAG */
	if ((bflag & 0xC000) != 0) gen_warn("BFLAG out of bounds (AXIS)");
	do_tertiary_labels = (bflag & 0x2000) ? FALSE : TRUE ;	/* 8192 => disable log tertiary labels */
	dotitle  = (bflag & 0x1000) ? FALSE : TRUE ; /* 4096 => Disable title (n also) */
	ztype    = (bflag & 0x800) ? TRUE : FALSE ;	/* 2048 => Z axis type				*/
	grid		= (bflag & 0x400) ? TRUE : FALSE ;	/* 1024 => Add full grid			*/
	justify	= (bflag & 0x200) ? FALSE : TRUE ;	/*  512 => Justify first/last		*/
	logdec	= (bflag & 0x100) ? FALSE : TRUE ;	/*  256 => use decimal on LOG		*/
	if         (bflag & 0x080) return;				/*  128 => Alternate do_nothing	*/
	tmajor	= (bflag & 0x040) ? FALSE : TRUE ;	/*   64 => No tickmarks at all	*/
	tminor	= (bflag & 0x020) ? FALSE : TRUE ;	/*   32 => supress minor ticks	*/
	ilab		= (bflag & 0x010) ? FALSE : TRUE ;	/*   16 => suppress tick labels	*/
	rotate	= (bflag & 0x008) ? TRUE : FALSE ;	/*    8 => Rotate to std. Y		*/
	tickin	= (bflag & 0x004) ? TRUE : FALSE ;	/*    4 => Tick marks in			*/
	brief		= (bflag & 0x002) ? TRUE : FALSE ;	/*    2 => Brief labeling mode?	*/
	ytype		= (bflag & 0x001) ? TRUE : FALSE ;	/*    1 => Y type axis				*/
	xtype    = ! ytype;									/* Only one or the other */

	if (! tmajor) tminor = FALSE;						/* Can't have minor w/o major */
	if (tlen == 0) dotitle = FALSE;					/* Also can disable by N value */

/* ... Other old ways to handle special modes */
	if (labels == NULL) {								/* If labels specified, just accept */
		if (m > 10)		  ilab	= FALSE;				/* Disable labels */
		if (dx2 == 0.0f) tminor	= FALSE;				/* Disable minor ticks */
		if (dx  == 0.0f) tmajor = FALSE;
		logticks = (axtype == LOG) && (udx2 < 0.0f) && tminor;
	}
	justify  = justify && tickin && (! rotate);	/* Justify labels? */

/* ... To save routines, for Y axis, just switch parameters with X coordinates
   ... And pretend its X axis.  Makes for a real pain later though. */
	for (i=0; i<4; i++) {for (j=0; j<4; j++) ui[i][j] = PL_Plot.ui[i][j];}

	zstart  = 0;											/* No one uses Z yet */
	theta   = 0;											/* Normal angles */
	if (xtype) {											/* This is really a X axis */
		xs_user = xu;
		ystart  = yu;
		ymid    = 0.5*(PlotWindow->clpyh + PlotWindow->clpyl) - PlotWindow->yorg;
	} else {													/* Y axis */
		xs_user = yu;										/* Fake x <--> y */
		ystart  = xu;
		ymid    = 0.5*(PlotWindow->clpxh + PlotWindow->clpxl) - PlotWindow->xorg;
		theta   = 90;										/* Rotate labels 90 degrees */
		X1 = ui[0][0]; ui[0][0] = ui[1][1]; ui[1][1] = (REAL) X1;	/* Exchange */
		X1 = ui[0][3]; ui[0][3] = ui[1][3]; ui[1][3] = (REAL) X1;
	}

	thdraw = (rotate) ? 90-theta : theta ;			/* And set drawing angle */

/* ... For the axis, increase the drawing window to maximum.
   ... Necessary so axis doesn't get clipped as vectors would.
   ... Also, turn off the USER processing so work in real inches. (LABELS) */
	savek = -1;
	PlotSetClip(&savek,save);							/* Determine clip mode now */
	i = 1; PlotSetClip(&i,save);						/* Set to maximum size */
	usrsav = PlotSetUserMode(FALSE);					/* Turn off and save */
	lthold = PlotSetLineType(1,0.0f);				/* Solid lines */

/* ... For ease, always draw from less to greater - Move size variable */
	if (sizeu < 0.0f) {									/* Is it negative */
		size = -sizeu;										/* Copy negative into area */
		xs_user = xs_user + sizeu;						/* Other end to start from */
	} else {
		size = sizeu;
	}

	xstart = xs_user;										/* begin/end with nonlinear */
	xend   = xs_user+size;
	if (axtype == NLIN) {
		xstart = (*conv)((REAL) xstart);
		xend   = (*conv)((REAL) xend);
	}

/* Some special 3D checks so labels always come out looking okay */
	if (PlotWindow->mode_3d) {
		flipall = 0;										/* 1 ==> do [0], 2 ==> [1] */
		if (! ztype) {										/* X and Y axis labelling	*/
			if (PL_Plot.ig[0][0] < 0) flipall |= 1;	/* Need to inverse X sense */
			if (PL_Plot.ig[1][1] < 0) flipall |= 2;	/* Same for Y */
		} else {
			if (rotate) thdraw = -90;					/* Invert sense of rotation */
			if (PL_Plot.ig[1][0] < 0) flipall |= 1;	/* X should move up page */
			if (PL_Plot.ig[0][1] > 0) flipall |= 2;	/* +Y should move left */
		}
		if (flipall != 0) {
			for (i=0; i<4; i++) for (j=0; j<4; j++) gm[i][j] = (i==j) ? 1.0f : 0 ;
			if (flipall & 0x01) {
				for (j=0; j<4; j++) ui[0][j] = -ui[0][j];	/* Invert meaning of X */
				gm[0][0] = -1;
			}
			if (flipall & 0x02) {
				for (j=0; j<4; j++) ui[1][j] = -ui[1][j];	/* Invert meaning of X */
				gm[1][1] = -1;
			}
			Plot3DMultiply(2, gm);						/* Apply to inch->pixel array */
		}
		Plot_Z_Inch_Default = (REAL) zt(xstart, ystart, zstart);
		Plot_Z_Default = Plot_Z_Inch_Default;		/* Because usrmode = FALSE */
	}

/* ... Check if scientific mode needed. Switch automatically if so.
   ... Automatically go if values below 0.0001 or above 10000. */
	xhighm = xs_user + size*1.0001;					/* Other end of axis (safety) */
	if (labels == NULL) {								/* Our work or defined? */
		temp   = (REAL) max(max(1000*REAL_MIN, fabs(xs_user)), fabs(xhighm));
		if (axtype == LOG) {
			sci = FALSE;										/* No scientific yet */
			fctr = 1.0f;										/* Factor is 1 */
		} else if ( (temp < .9999e-03f) || (temp >= .99999e04f) ) {
			sci = TRUE;											/* Now scientific */
			iexp = ((INTEGER)(log10(temp)+100)) -100;	/* Truncate downward */
			fctr = (REAL) pow(10.0,(double) iexp);		/* Funny factor now */
		} else {
			sci = FALSE;										/* No scientific yet */
			fctr = 1.0f;
		}

/* ... Maybe determine number of significant digits - if M==0 or if in scientific */
		if (axtype == LOG) {
			m = -1;												/* Always integer mode */
		} else if ( (m == 0) || sci) {
			X1 = (REAL) max(fabs(dx/fctr),1000*REAL_MIN);
			if (X1 >= 1.0f)
				m = -1;											/* Integer mode */
			else
				m = (INTEGER) (-log10(X1) + 0.99);		/* Simple way to get number */
		}

/* This is when labels != NULL  -- just set some defaults so safe in code */
	} else {
		sci = FALSE;
		fctr = 1.0f;
	}

/* ... Status of N.  If = 0, set labels off and force plot to draw tick
   ... marks to "outside" of plot.  Not zero, set direction of line to
   ... sign of N and determine size of all characters.  Have to estimate how
	... much movement occurs with change in X.  This is xfact2 here. */
	if (tlen == 0) {										/* N=0 still forces no labels */
		isn = 1;												/* Assume direction positive */
		if (ztype) {										/* Z is above an X axis */
			isn = -1;
		} else if (yt(xs_user, ystart, zstart) > ymid) {	/* Past mid, reverse ticks */
			isn = -1;	
		}
		ilab = FALSE;										/* No labels at all */
	} else {
		isn = (tlen > 0) ? 1 : -1;						/* Direction is sign of N */
		if (ztype) isn = -isn;							/* Invert if Z axis */
		xfact2 = sqrt(ui[0][0]*ui[0][0] + ui[1][0]*ui[1][0] + ui[2][0]*ui[2][0]);
		if (labels != NULL) {							/* If labels specified, assume user knows all */
			csize = labels->csize;
			if (csize <= 0) csize = PL_Axis.MaxLabelSize;
		} else if (rotate) {								/* If rotated, use height */
			i = (int) (fabs(size/dx)+0.99);			/* Number of major labels */
			if (i==0) i=1;
			csize = (REAL) 0.75f*fabs(xfact2*(xend-xstart)/i);
			csize = min(csize,PL_Axis.MaxLabelSize);
			if (abs(m) > 10) csize = 0.0f;				/* Turn off if >10 decimals */
		} else if (axtype != LIN) {
			csize = PL_Axis.MaxLabelSize;
			if (abs(m) > 10) csize = 0.0f;				/* Turn off if >10 decimals */
		} else {
			i = ndigit(temp/fctr) + 2;						/* Total characters */
			if (m > 0) i += m+1;								/* Add decimals */
			csize = (REAL) fabs(xfact2*dx/i);
			csize = min(csize, PL_Axis.MaxLabelSize);	/* Size of label */
		}
		if (csize < fabs(PL_Axis.MinLabelSize)) {		/* Do we need to change size */
			csize = fabs(PL_Axis.MinLabelSize);
			if (PL_Axis.MinLabelSize <= 0.0f) ilab = FALSE;
		}
		ts = PL_Axis.TitleSize;								/*	Title size and direction */
	}

/* ............................................................................ 
   ... Draw the basic line */
	if (axtype != NLIN) {
		Y1   = yt(xs_user, ystart, zstart);
		ytop = yt(xs_user, ystart+isn*sizeu, zstart);		/* Top axis level (kludge!) */
	} else {
		Y1   = yt(xs_user, ystart, zstart);
		ytop = yt(xs_user, ystart+isn*fabs(xend-xstart), zstart);		/* Top axis level (kludge!) */
	}

	X1 = xt(xstart, ystart, zstart);
	X2 = xt(xend,   ystart, zstart);
	xmid = 0.5f*(X1+X2);

/* ... Check color and linewidth, modify if necessary and draw line */
	PlotSetLineWidth(Width.Line);						/* Set appropriate width */
	PlotSelectPen(Color.Line);							/* Set appropriate pen */
	PLOTMOVE(xpl(X1,Y1),ypl(X1,Y1),3);
	PLOTMOVE(xpl(X2,Y1),ypl(X2,Y1),2);					/* Go over line twice */
	PLOTMOVE(xpl(X1,Y1),ypl(X1,Y1),2);					/* to make it darker */

/* ... And now set the boundaries for labels */
	xleft  = min(X1,X2);
	xright = max(X1,X2);

/* ... Are we to draw major tick marks? If yes, determine starting point. */
	if (labels == NULL) {
		if (tmajor) {											/* User wants tick marks */
			index = (long int) ((xs_user/dx) + 0.99f);	/* First one */
			if (xs_user < -0.99f*dx) index--;				/* Correct when negative */
			xmajor = dx*index;								/* And it's point */
		} else if (axtype == LOG) {
			goto TitleOnly;									/* Skip to title print! */
		}
	}

/* ... Do the same for minor tick marks.  Eliminate within 0.05*dx2 of major */
	if (labels == NULL) {
		if (logticks) {										/* Strange LOG ticks */
			xminor = xmajor-1;								/* Start off by 1 */
			imode = min(5,nint(dx2));						/* Which labeling mode */
		} else if (tminor) {
			index = (long int) (xs_user/dx2 + 0.99f);	/* First one */
			if (xs_user < -0.99f*dx2) index--;
			if ( fabs(xmajor-index*dx2) < 0.01f*dx ) index++;	/* Don't start on mark */
			xminor = dx2*index;
		}
	}

/* ... A number of corrections for the direction etc.  HOKEY-POKEY.
   ... Y2    = Starting height of labels
   ... Y3,Y4 = End of Major and Minor tick marks
   ... Y5    = End of sub-major tick mark for LOG axis type */

	if (tickin) {											/* Tick marks in */
		Y3 = Y1 + isn*PL_Axis.MajorTick;
		Y4 = Y1 + isn*PL_Axis.MinorTick;
	} else {
		Y3 = Y1 - isn*PL_Axis.MajorTick;
		Y4 = Y1 - isn*PL_Axis.MinorTick;
	}
	Y5 = (Y4+2*Y3)/3;										/* Medium size tick (LOG) */
	Y6 = (2*Y4+Y3)/3;										/* Medium size tick (LOG) */

/* ... Y level for labelling  - Many modes */
	above = (ytype && (isn == 1)) || (xtype && (isn == -1));
	if ( (axtype==LOG) && rotate) {					/* Special rotated LOG mode */
		if (isn == 1)										/* Drawing to left? */
			Y2 =  -0.5f;
		else if (tickin)									/* Drawing to right w/ticks in */
			Y2 = +0.5f;
		else
			Y2 =  0.25f;
	} else {
		if (justify || rotate) {						/* Space below axes */
			Y2 = -isn*label_shift;
		} else {												/* For non-overlapping labels */
			Y2 = -isn*label_extra_shift;
		}
		if (! rotate) {									/* Make room for character? */
			if (! above) {									/* Make room for char itself */
				Y2 -= isn;									/* Character */
				if (axtype==LOG) Y2 -= 0.5f*isn;		/* Exponent */
			}
			if (xtype)										/* Little axymmetry */
				Y2 += 0.1;
			else
				Y2 -= 0.1;
		}
	}

	Y2 = Y1 + Y2*csize;									/* Real inches */
	if (! tickin) Y2 -= isn*PL_Axis.MajorTick;	/* And include tick marks */

	if (! rotate) {										/* And correct label bounds */
		xleft  = xleft  - 0.4*csize;
		xright = xright + 0.4*csize;
	}

/* Do the tick marks and labels now */
	if (labels != NULL) {
		double xmin,xmax;												/* Limits on values */
		xmin = xs_user;												/* In user space */
		xmax = xs_user+1.0001*size;								/* Epsilon more to get end point */
		if (labels->major  == NULL) tmajor = FALSE;			/* What is possible to do? */
		if (labels->minor  == NULL) tminor = FALSE;
		if (labels->labels == NULL) ilab   = FALSE;

		lenmax = 0.0f;										/* Initialize */

/* Do the minor tick marks fist.  But don't do if same as a major tick */
		if (tminor) {
			ytmp = Y4;														/* Assume Y4 length of tick */
			PlotSetLineWidth(Width.Minor);							/* User choice */
			PlotSelectPen(Color.Minor);								/* Set appropriate pen */
			for (i=0; i<labels->nminor; i++) {
				xminor = labels->minor[i];
				if (xminor < xmin || xminor > xmax) continue;	/* Outside of range, not needed */

				if (labels->major != NULL) {
					for (j=0; j<labels->nmajor; j++) { if (labels->major[j] == xminor) break; }
					if (j < labels->nmajor) continue;
				}

				X1 = xminor;
				if (axtype == NLIN) X1 = (*conv)((REAL) X1);
				X1 = xt(X1, ystart, zstart);							/* Convert to inches */
				if (grid) {
					PLOTMOVE(xpl(X1,ytop),ypl(X1,ytop),3);
					if (tickin) {
						PLOTMOVE(xpl(X1,Y1),ypl(X1,Y1),2);
					} else {
						PLOTMOVE(xpl(X1,ytmp),ypl(X1,ytmp),2);
					}
				} else {
					PLOTMOVE(xpl(X1,Y1),ypl(X1,Y1),3);			/* Starting on axis */
					PLOTMOVE(xpl(X1,ytmp),ypl(X1,ytmp),2);		/* Other end */
				}
			}	/* for (i=0; i<labels->nminor; i++) */
		}	/* if (tminor) */
			
		/* Now the major tick marks and labels */
		if (tmajor) {
			PlotSetLineWidth(Width.Major);							/* User choice */
			PlotSelectPen(Color.Major);								/* Set appropriate pen */

			for (i=0; i<labels->nmajor; i++) {
				xmajor = labels->major[i];
				if (xmajor < xmin || xmajor > xmax) continue;	/* Outside of range, not needed */

				X1 = xmajor;
				if (axtype == NLIN) X1 = (*conv)((REAL) X1);
				X1 = xt(X1, ystart, zstart);							/* Convert to inches */
				if (grid) {
					PLOTMOVE(xpl(X1,ytop),ypl(X1,ytop),3);
					if (tickin) {
						PLOTMOVE(xpl(X1,Y1),ypl(X1,Y1),2);
					} else {
						PLOTMOVE(xpl(X1,Y3),ypl(X1,Y3),2);
					}
				} else {
					PLOTMOVE(xpl(X1,Y1),ypl(X1,Y1),3);				/* Starting on axis */
					PLOTMOVE(xpl(X1,Y3),ypl(X1,Y3),2);				/* Other end */
				}
				
				if (! ilab) continue;									/* No labels, just continue with marks */
				strcpy(buf, labels->labels[i]);						/* Text to draw */
				
				length = PlotQueryStringLength((REAL) csize, buf, 0);		/* Plot length */
				lenmax=max(length,lenmax);								/* Maximum length */
				if (rotate) {												/* Painful operation */
					xdraw = -0.5f;
					if (ytype || ztype) xdraw = 0.5f;
					xdraw = X1-0.8f*csize*xdraw;						/* Center it (Aspect) */
					if (ztype) {
						ydraw = Y2+length;								/* Move it down (?) */
					} else {
						ydraw = Y2-0.5f*(isn+1)*length;				/* Move it down (?) */
					}
				} else {
					xdraw = X1 - 0.5*length;							/* Simple horizontal */
					ydraw = Y2;												/* Really goes there */
				}
				if (justify) {												/* Keep OK */
					if (xdraw < xleft) xdraw = xleft;
					if (xdraw+length > xright) xdraw = xright-length;
				}

				PlotSetLineWidth(Width.Labels);							/* Set width */
				PlotSelectPen(Color.Labels);								/* Set pen */
				PlotString((REAL) xpl(xdraw,ydraw),(REAL) ypl(xdraw,ydraw),(REAL) csize, buf, (REAL) thdraw,0);

			}	/* for (i=0; i<labels->nmajor; i++) */
		}	/* if (tmajor) */

	} else {		/* labels == NULL */
/*------------------------------------------------
 ... This is a repeat forever, Do major tick mark
-------------------------------------------------- */
		lenmax = 0.0f;										/* Initialize */
		first = TRUE;										/* First time special */
		if (xminor < xmajor) xmajor -= dx;			/* Start one back */

/* ... Start with the smaller minor or major tick mark */

		while (tmajor || tminor) {
			if (tmajor) {
				if (xmajor > xhighm) {					/* Out of plotter? */
					tmajor = FALSE;						/* None anymore */
					continue;								/* Still can do minors! */
				} else if ( xmajor-xs_user >= -1E-5*(fabs(xmajor)+fabs(xs_user)) ) {
					X1 = xmajor;							/* Convert to plot inches */
					if (axtype==NLIN) X1 = (*conv)((REAL) X1);
					X1 = xt(X1, ystart, zstart);
					PlotSetLineWidth(Width.Major);		/* Set appropriate width */
					PlotSelectPen(Color.Major);			/* Set appropriate pen */
					if (grid) {
						PLOTMOVE(xpl(X1,ytop),ypl(X1,ytop),3);
						if	(tickin) 
							PLOTMOVE(xpl(X1,Y1),ypl(X1,Y1),2);
						else
							PLOTMOVE(xpl(X1,Y3),ypl(X1,Y3),2);
					} else {
						PLOTMOVE(xpl(X1,Y1),ypl(X1,Y1),3);			/* Starting on axis */
						PLOTMOVE(xpl(X1,Y3),ypl(X1,Y3),2);			/* Other end */
					}
					
/* ........ Do I want to draw the label?  Always first and last labeled. */
					lstone = xmajor+dx > xhighm;					/* Is this the last one */
					if (ilab && (!brief || first || lstone)) {
						first = FALSE;									/* No longer first */
						xhold = xmajor/fctr;							/* Value to draw */
						if (axtype == LOG) {
							i = nint(xmajor);							/* Exponent value */
							if (i < -3 || i > 4 || logdec) {
								sprintf(buf, "10^~%i", nint(xmajor));	/* Encode exponent */
							} else if (i >= 0) {
								strcpy(buf,"10000");
								buf[i+1] = '\0';						/* Truncate on correct 0 */
							} else if (i >= -2) {
								strcpy(buf,"001");
								buf[2+i] = '.';
							} else {
								strcpy(buf,".001");
							}
						} else if (m < 0) {							/* Encode as integer */
							sprintf(buf, "%i", (int) nint(xhold));
						} else {											/* Encode as real number */
							CHAR formt[20];							/* Format buffer */
							sprintf(formt, "%%.%if", m);			/* Should create ".7f" */
							sprintf(buf, formt, xhold);			/* And encode as necessary */
						}
						
						logtmp = (lstone && rotate && sci);
						if (logtmp) {												/* Add x10 SCI stuff? */
							sprintf(buf2, "x10^~%i", iexp);					/* Encode exponent */
							strcat(buf, buf2);									/* Concatenate */
						}
						length = PlotQueryStringLength((REAL) csize,buf,0);		/* Plot length */
						if (!logtmp) lenmax=max(length,lenmax);			/* Maximum length */
						
						if (rotate) {												/* Painful operation */
							xdraw = -0.5f;
							if (ytype || ztype) xdraw = 0.5f;
							xdraw = X1-0.8f*csize*xdraw;						/* Center it (Aspect) */
							if (ztype) {
								ydraw = Y2+length;								/* Move it down (?) */
							} else {
								ydraw = Y2-0.5f*(isn+1)*length;				/* Move it down (?) */
							}
							if (lstone && sci) xdraw = xdraw - .4*csize;
						} else {
							xdraw = X1 - 0.5*length;							/* Simple horizontal */
							ydraw = Y2;												/* Really goes there */
						}
						if (justify) {												/* Keep OK */
							if (xdraw < xleft) xdraw = xleft;
							if (xdraw+length > xright) xdraw = xright-length;
						}
						
						PlotSetLineWidth(Width.Labels);							/* Set width */
						PlotSelectPen(Color.Labels);								/* Set pen */
						PlotString((REAL) xpl(xdraw,ydraw),(REAL) ypl(xdraw,ydraw),(REAL) csize, buf, (REAL) thdraw,0);
					}
				}
				if (logticks) xminor=xmajor;			/* Minors start here */
				xmajor += dx;								/* Point to next major tick */
			}
			
/* ----------------------------------------
   ... Any minor ticks left to be done
   ---------------------------------------- */
			if (! tminor) continue;							/* No minor to do, check major */
			idx = 1;												/* First one */

			while (TRUE) {
				double log_subtick_value;
				ytmp = Y4;										/* Assume Y4 length of tick */
				tertiary = FALSE;								/* Tertiary mark? */
				*buf = '\0';									/* Blank out buf */
				
				if (logticks) {								/* Absolutely simple??? */
					do {
						ytmp     = Y4;							/* Assume Y4 length of tick */
						tertiary = FALSE;						/* Tertiary mark? */
						*buf     = '\0';						/* Blank out buf */
						switch (imode) {
							case 1:								/* Mode=1, do 5,10 */
								if (idx == 1) {
									X1 = 5.0f;
								} else {
									X1 = 2.0f;
									ytmp = Y3;
								}
								idx = 3 - idx;
								break;
							case 2:								/* Mode=2 */
								X1 = (idx!=2) ? 2.0f : 2.5f;	/* Does 1->2 and 5->10, then 2->5 */
								if (idx == 3) ytmp = Y3;		/* Major marks */
								idx = (idx % 3) + 1;				/* and repeat */
								break;
							case 3:
								idx = 2 + ( (idx-1) % 9);		/* 2,3,4,5 ... 10 -- 2,3,4 ... */
								X1 = (1.0*idx)/(idx-1);			/* 2, 3/2, 4/3, 5/4, 6/5, ... */
								if (idx == 5) {
									ytmp = Y5;					/* For 5 strcpy(buf,"5"); */
								} else if (idx == 10) {
									ytmp = Y3;					/* For 10 */
								} else {
									tertiary = TRUE;			/* Tertiary mark? */
								}
								break;
							case 4:								/* For ranges with less than 2 decade */
								if (idx <= 15) {
									X1 = (1.0+0.2*idx)/(0.8+0.2*idx);	/* 1.2,1.4,...,3.0 */
								} else {
									X1 = (4.0+0.5*(idx-15))/(4.0+0.5*(idx-16));
								}
								if (idx == 27) {					/* Decades */
									ytmp = Y3;
								} else if (idx == 17) {		/* Half decade */
									ytmp = Y5;					/* strcpy(buf, "5"); */
								} else if (idx==5 || idx==10 || idx==15 || idx==19 || idx==21 || idx==23 || idx==25) {	/* 2 marks */
									ytmp = Y6;					/* strcpy(buf, "2");	*/
								} else {
									tertiary = TRUE;
								}
								idx = (idx % 27) + 1;
								break;
							case 5:								/* For ranges with less than 1 decade */
								if (idx <= 40) {
									X1 = (1.0+0.1*idx)/(0.9+0.1*idx);			/* 1.1,1.2,... 4.9,5.0 */
								} else {
									X1 = (5.0+0.2*(idx-40))/(5.0+0.2*(idx-41));		/* 5.2,5.4,...,9.8,10.0 */
								}
								if (idx == 65) {					/* Decades */
									ytmp = Y3;
								} else if (idx == 40) {		/* Half decade */
									ytmp = Y5;					/* strcpy(buf, "5"); */
								} else if (idx==10 || idx==20 || idx==30 || idx==45 || idx==50 || idx==55 || idx==60) {	/* 2 marks */
									ytmp = Y6;					/* strcpy(buf, "2");	*/
								} else {
									tertiary = TRUE;
								}
								idx = (idx % 65) + 1;
								break;
						}
						xminor = xminor + log10(X1);							/* New value XMINOR */
					} while (xminor < xs_user);								/* Loop until valid */

/* =============================================================================================
 * Label the tertiary marks when requested and appropriate with very short logarithmic axes
 * ========================================================================================== */
					if (do_tertiary_labels) {
						double axis_length;
						static struct {
							double value;
							double max_axis_length;
							char *text;
						} TertList[] = {
							{1.2, 0.9999, "1.2"},		/* Add 1.2, 1.4, 1.6 when length is < 1.0 units */
							{1.4, 0.9999, "1.4"},
							{1.6, 0.9999, "1.6"}, 
							{1.8, 0.9999, "1.8"}, 
							{2.0, 0.9999, "2.0"},		/* Also the 2.0, 2.5, 3.0, 3.5, 4.0 and 4.5 */
							{2.5, 0.9999, "2.5"},
							{3.0, 0.9999, "3.0"},
							{3.5, 0.9999, "3.5"},
							{4.0, 0.9999, "4.0"}, 
							{4.5, 0.9999, "4.5"},

							{1.1, 0.5, "1.1"},			/* When 0.5 or smaller, also do the 1.1, 1.3, ... 1.9 */
							{1.3, 0.5, "1.3"}, 
							{1.5, 0.5, "1.5"},
							{1.7, 0.5, "1.7"},
							{1.9, 0.5, "1.9"},

							{5.0, 5.5, "5"},				/* For up to 5.5, add the "5" label */
							{2.0, 4.5, "2"},				/* For up to 4.5, add the "2" label */
							{3.0, 2.5, "3"},				/* For up to 2.5, add 3,4,6,8 */
							{4.0, 2.5, "4"},
							{6.0, 2.5, "6"},
							{8.0, 2.5, "8"},
							{7.0, 1.5, "7"},				/* If less than 1.5 length, also do 7 and 9 */
							{9.0, 1.5, "9"}
						};
						log_subtick_value = pow(10, xminor-floor(xminor));		/* Which sub-tick is this */
						axis_length = fabs(xend-xstart);
						for (i=0; i<sizeof(TertList)/sizeof(TertList[0]); i++) {
							if (fabs(log_subtick_value-TertList[i].value) < 0.01 && axis_length <= TertList[i].max_axis_length) {
								strcpy(buf, TertList[i].text);
								break;
							}
						}
					}
				}
				
/* Now, back to normal work */
				if (xminor > xhighm) {										/* Done? */
					tminor = FALSE;
					break;														/* Try another major or quit */
				} else if (xminor > (xmajor-0.008*dx)) {				/* For now? */ 
					if (xminor < (xmajor+0.008*dx)) xminor=xminor+dx2;
					break;														/* Try another major */
				} else {
					X1 = xminor;												/* Convert inches */
					if (axtype == NLIN) X1 = (*conv)((REAL) X1);
					X1 = xt(X1, ystart, zstart);							/* Convert to inches */
					if (tertiary) {											/* Set width */
						PlotSetLineWidth(Width.Tertiary);				/* Minimum width */
						PlotSelectPen(Color.Tertiary);					/* Set appropriate pen */
					} else {
						PlotSetLineWidth(Width.Minor);					/* User choice */
						PlotSelectPen(Color.Minor);						/* Set appropriate pen */
					}
					if (grid) {
						PLOTMOVE(xpl(X1,ytop),ypl(X1,ytop),3);
						if (tickin) 
							PLOTMOVE(xpl(X1,Y1),ypl(X1,Y1),2);
						else
							PLOTMOVE(xpl(X1,ytmp),ypl(X1,ytmp),2);
					} else {
						PLOTMOVE(xpl(X1,Y1),ypl(X1,Y1),3);				/* Starting on axis */
						PLOTMOVE(xpl(X1,ytmp),ypl(X1,ytmp),2);			/* Other end */
					}
					
					if ( (*buf != '\0') && ilab && (! brief) ) {
						length = PlotQueryStringLength((REAL) (log_shrink*csize), buf, (int) strlen(buf));	/* Plot length */
						lenmax = max(length,lenmax);						/* Maximum length */
						if (rotate) {											/* Painful operation */
							xdraw = -0.5f;
							if (ytype) xdraw = 0.5f;
							xdraw = X1-0.8f*log_shrink*csize*xdraw;	/* Center it (Aspect) */
							ydraw = Y2-0.5f*(isn+1)*length;				/* Move it down (?) */
						} else {
							xdraw = X1 - 0.5f*length;						/* Simple horizontal */
							ydraw = Y2;
							if ( (Y1-ydraw) > 0)								/* Displace to axis */
								ydraw += 0.85f*csize;
							else
								ydraw -= 0.85f*csize;
						}
						PlotSetLineWidth(Width.Labels);					/* Set width */
						PlotSelectPen(Color.Labels);						/* Set pen */
						PlotString((REAL) xpl(xdraw,ydraw),(REAL) ypl(xdraw,ydraw),(REAL) (log_shrink*csize),buf,(REAL) thdraw, (int) strlen(buf));
					}
					if (! logticks) xminor += dx2;
				}
			}
		}

/* ----------------------------------------
 ... Draw x10 and power if in scientific mode (never for LOG axis) */
		if ((ilab && sci) && (! rotate) ) {				/* Worry about it now */
			csize = csize * 0.75f;
			sprintf(buf,"x10^~%i", iexp);					/* Convert exponent to ASCII */
			length = PlotQueryStringLength((REAL) csize,buf,0);	/* Determine maximum length */
			X1 = xmajor - dx;									/* Over last major mark */
			if (axtype == NLIN) X1 = (*conv)((REAL) X1);
			X1 = xt(X1, ystart, zstart);
			xdraw = X1 - 0.5*length;
			ydraw = Y2 - isn*2.1*csize;					/* Over big char or exponent */
			PlotSetLineWidth(Width.Labels);				/* Set appropriate width */
			PlotSelectPen(Color.Labels);					/* Set appropriate pen */
			PlotString((REAL) xpl(xdraw,ydraw),(REAL) ypl(xdraw,ydraw),(REAL) csize,buf,(REAL) thdraw,0);
		}

/*  End of labels != NULL case switch */
	}

/* ----------------------------------------
 ... Determine the position of the title.  Careful of orientation.
 ... Add additional offset from user in T(BTLR)OFF set */

TitleOnly:

	if (dotitle) {
		length = PlotQueryStringLength(PL_Axis.TitleSize, title, abs(tlen));
		xdraw = xmid - 0.5*length;						/* In the middle always */

		dy = title_shift*ts;								/* Initial shift */
		if (rotate) {										/* Rotated labels */
			dy = dy + lenmax;								/* Shift by label length */
		} else {												/* Not rotated labels */
			if (above) dy = dy + 1.3*csize;			/* Shift by label size (exp?) */
		}

		if (! above) dy = -dy-ts;						/* Other direction + char size */
		if (ytype) dy = -dy;
		ydraw = Y2 + dy;

		if (ytype) {										/* Add user requested offsets */
			if (isn == 1) 
				ydraw -= PL_Axis.TitleOffset.Left;		/* Left Y axis */
			else
				ydraw += PL_Axis.TitleOffset.Right;		/* Right Y axis */
		} else {
			if (isn == 1) 
				ydraw -= PL_Axis.TitleOffset.Bottom;	/* Bottom X axis */
			else
				ydraw += PL_Axis.TitleOffset.Top;		/* Top X axis */
		}

		PlotSetLineWidth(Width.Title);					/* Set appropriate width */
		PlotSelectPen(Color.Title);						/* Set appropriate pen */
		PlotString((REAL) xpl(xdraw,ydraw),(REAL) ypl(xdraw,ydraw),(REAL) ts,title,(REAL) theta,abs(tlen));
	}

/* ... Reset parameters for main plot routines */
	PlotSetLineWidth(width_hold);						/* Restore linestyle */
	PlotSelectPen(color_hold);
	PlotSetLineType(lthold,0.0f);						/* Back to old lines */
	PlotSetClip(&savek,save);							/* Restore status */
	PlotSetUserMode(usrsav);							/* User mode reset */

	if (DEVICE->Autoflush) PlotFlush();				/* Do we autoflush */
	PlotFixInternal();

	if (PlotWindow->mode_3d) Plot_Z_Default = Plot_Z_Inch_Default = 0;

	return;
}


/*
=============================================================================
--     FUNCTION NDIGIT - Determines the number of digits left of decimal
--
--     Function to find the number of digits to the left of a decimal number.
--     Always returns >= 1.  Used internally for axis labelling by axis1.
--
--     Usage:     INTEGER = NDIGIT(RNUM)
--
--     Inputs:    RNUM  -  REAL number to be determined
--
--     Output:    NDIGIT  -  Number of digits to left of decimal point
--                           includes negative sign if necessary.
============================================================================= */
static int ndigit(double rnum) {
	
	int NumDigits;
	
	if (rnum != 0.0f) {
		NumDigits = (INTEGER) (1.00001 + log10(fabs(rnum)));
		if (NumDigits < 1) NumDigits = 1;				/* Always assume 1 digit */
	} else
		NumDigits = 1;
	
	if (rnum < 0.0f) NumDigits++;						/* Add sign character */
	return(NumDigits);
}
