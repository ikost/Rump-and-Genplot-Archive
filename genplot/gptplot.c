/* gptplot.c */

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
#include "lexp.h"
#include "tplot.h"
#include "gptxtrn.h"
#include "gptdef.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define	InRange(x,y)		( (x>xlow) && (x<xhigh) && (y>ylow) && (y<yhigh) )
#define	InRangeZ(z)			( (z>zlow) && (z<zhigh) )
#define	InRange3D(x,y,z)	( (x>xlow) && (x<xhigh) && (y>ylow) && (y<yhigh) && (z>zlow) && (z<zhigh) )

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static void DrawLinesAndSymbols(REAL *x, REAL *y, int npt, int npoint, int spoint, REAL size);
static void FillRect(REAL x1, REAL x2, REAL y1, REAL y2, 
							REAL z1, REAL z2, REAL z3, REAL z4);
static void FillTriangle(REAL x1,REAL y1,REAL z1, REAL x2, REAL y2, 
								 REAL z2, REAL x3, REAL y3, REAL z3);
typedef struct _CONTOUR_INFO {
	REAL xmin,xmax,ymin,ymax,zmin,zmax;
	REAL dz,dz2,zval;
	double smoothing;
	BOOL smooth;
	enum {SINGLE_CONTOUR, MULTIPLE_CONTOUR} mode;
} CONTOUR_INFO;
static void PlotContour(REAL *x, REAL*y, int npt, CONTOUR_INFO *info);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
static int  npt, ncol, nrow;				/* Don't need to be passed */
static REAL xlow, xhigh, ylow, yhigh, zlow, zhigh;

typedef enum _PLOT_TYPES {NONE, NORMAL, AZIZBARS, PT_LABEL, HISTOGRAM, BARGRAPH, HBAR, STICK, CONTOUR, PLT_BITMAP}
	PLOT_TYPES;
typedef enum _PLOT_OPTIONS {PLOTTYPE, SYMBOL, LTYPE, NPOINT, SPOINT, PEN, SYMSIZE, LINEWIDTH, VECSIZE, L_S, 
	IDENT, IDS, NOIDS, ERRORS, ERRWIDTH, EXCLUDE, SCALES, XYRANGE, PLX, PLY, 
	BOTTOP, LEFTRIGHT, RAINBOW, PALETTE, SCOPE, GRID, SLOWLY,
	CONTOUR_AT, CONTOUR_DZ, CONTOUR_DZ2, CONTOUR_ZSCAN, CONTOUR_SMOOTH, CONTOUR_SPLINE, CONTOUR_RIGID}
	PLOT_OPTIONS;
	
typedef struct _CMTYPE_ {
	char *name;
	int  minlen;
	PLOT_OPTIONS rcode1;										/* Major mode of the option */
	PLOT_TYPES rcode2;										/* Sub-character - generic use int */
} CMTYPE;

static const CMTYPE cmlist[] = {
						{"-aziz",		4, PLOTTYPE,		AZIZBARS},		/* Aziz style plot */
						{"-pt_label",	3, PLOTTYPE,		PT_LABEL},		/* Point labels    */
						{"-histogram",	2, PLOTTYPE,		HISTOGRAM},		/* Histogram plot  */
						{"-bargraph",	2,	PLOTTYPE,		BARGRAPH},		/* Bargraph plot   */
						{"-hbargraph", 3, PLOTTYPE,		HBAR,},			/* Horizontal bar graph */
						{"-stick",		3, PLOTTYPE,		STICK},			/* Stick plot      */
						{"-bitmap",		3, PLOTTYPE,		PLT_BITMAP},	/* Bitmap background */
						{"-contour",	4, PLOTTYPE,		CONTOUR},		/* Contour plot */
						{"-at",			3, CONTOUR_AT,		0},			/* Options for contour plots */
						{"-dz",        3, CONTOUR_DZ,		0},			
						{"-dz2",       4, CONTOUR_DZ2,	0},
						{"-zscan",     5, CONTOUR_ZSCAN, 0},
						{"-zrigid",    5, CONTOUR_RIGID, 0},			/* Rigid contours (line points only) */
						{"-zspline",   5, CONTOUR_SPLINE, 0},			/* Contour plots (default)	*/
						{"-zsmooth",   3, CONTOUR_SMOOTH, 0},			/* Contour smoothing */
						{"-symbol",		4, SYMBOL,			0},
						{"-ltype",		3, LTYPE,			0},
						{"-npoint",		3,	NPOINT,			0},
						{"-spoint",		3, SPOINT,			0},
						{"-linetype",	5, LTYPE,			0},
						{"-pen",			4, PEN,				0},
						{"-color",		4, PEN,				0},
						{"-symsize",	5, SYMSIZE,			0},
						{"-symbolsize",8, SYMSIZE,			0},
						{"-linewidth",	5, LINEWIDTH,		0},
						{"-lw",			3, LINEWIDTH,		0},
						{"-vecsize",	4,	VECSIZE,			0},
						{"-l&s",			4, L_S,				0},
						{"-identify",	4, IDENT,			0},
						{"-ids",			4, IDS,				0},
						{"-noids",		6, NOIDS,			0},
						{"-errx",		5, ERRORS,			0x40},
						{"-errxlow",	6, ERRORS,			0x00},
						{"-errxhigh",	6, ERRORS,			0x01},
						{"-errxminus",	6, ERRORS,			0x00},
						{"-errxplus",	6, ERRORS,			0x01},
						{"-erry",		5, ERRORS,			0x82},
						{"-errylow",	6, ERRORS,			0x02},
						{"-erryhigh",	6, ERRORS,			0x03},
						{"-erryminus",	6, ERRORS,			0x02},
						{"-erryplus",	6, ERRORS,			0x03},
						{"-errwidth",	5, ERRWIDTH,		0},
						{"-exclude",	5, EXCLUDE,			0},
						{"-xscale",		3, SCALES,			1},
						{"-yscale",		3, SCALES,			2},
						{"-xyscale",	3, SCALES,			3},
						{"-rescale",	4, SCALES,			3},
						{"-xrange",    3, XYRANGE,			1},
						{"-yrange",    3, XYRANGE,			2},
						{"-zrange",    3, XYRANGE,			3},
						{"-xyrange",   4, XYRANGE,			4},
						{"-xyzrange",  5, XYRANGE,			5},
						{"-plx",			4, PLX,				1},
						{"-ply",			4, PLY,				1},
						{"-bottom",		4, BOTTOP,			0},
						{"-top",			4, BOTTOP,			1},
						{"-left",		4, LEFTRIGHT,		0},
						{"-right",		4, LEFTRIGHT,		1},
						{"-rainbow",	4, RAINBOW,			0},
						{"-palette",	4, PALETTE,			0},
						{"-grid",		5, GRID,				0},
						{"-scope",		5, SCOPE,			1},
						{"-slow",		4, SLOWLY,			1},
						{"-slower",		6,	SLOWLY,			2},
						{"-slown",		5,	SLOWLY,			0},
						{NULL,			0, PLOTTYPE,		0} };

/* ---------------------------------------------------------------------------

--------------------------------------------------------------------------- */
static char PlotHelp[] = 
"\n"
" Primary command for adding a curve of data.  PLOT draws the axes as well\n"
" as the data (if AUTOAXES enabled) while OVERLAY only adds the curve to the\n"
" existing axes.  All options are available for both commands\n"
"\n"
"   PLot    [<curve> | -f <fnc> | -fit] [-opts]\n"
"   OVerlay [<curve> | -f <fnc> | -fit] [-opts]\n"
"\n"
" The optional curve name, -f <fnc>, or -fit determines the source of the data\n"
" to be drawn.  If none is specified, the data in the main curve will be used.\n"
" The -f and -fit take additional optional arguments equivalent to the CREATE\n"
" command.  These are of the form:\n"
"        -f <fnc> [-range <xlow> <xhigh>] [-points <npt> | -by <deltax>]\n"
" The -fit specification is nearly equivalent to -f fit(x).\n"
"\n"
" Options:\n"
"   -help           | -?                 Prints this help message\n"
"\n"
"   -symbol <sym>                        Draws data using specified symbol\n"
"   -ltype <lt>     | -linetype <lt>     Draws data using specified line type\n"
"   -l&s                                 Draws data as both lines and symbols\n"
"   -pen <icol>     | -color <icol>      Draws data using specified pen color\n"
"   -symsize <val>  | -symbolsize <val>  Sets the size of symbols drawn\n"  
"   -lw <val>       | -linewidth <val>   Sets the linewidth of data drawing\n"
"   -vecsize <val>                       Sets the repeat spacing on dashed lines\n"
"   -npoint <num>                        Use only every ith point for drawing\n"
"   -spoint <num>                        Use only every ith point for symbols only\n"
"                                         plot -l&s -spoint 10\n"
"                                        draws dense line but symbols only every 10th\n"
"   -identify <string>                   Identify using the specified string\n"
"   -ids                                 Draw the current curve ID as identifier\n"
"   -noids                               Don't draw IDS even if AUTOIDS enabled\n"
"\n"
"   -histogram                           Histogram type graph\n"
"   -bargraph                            Bar graph type graph\n"
"   -hbargraph                           Horizontal graph type graph (x versus y)\n"
"   -stick                               Stick type graph\n"
"   -contour                             Contour map (see more options below)\n"
"      -at <val>                         Single contour at specified level\n"
"      -dz <val> -dz2 <val>              Major and minor intervals on contours\n"
"      -zscan <zmin> <zmax>              Start and ending values for contours\n"
"      -zrigid                           (default) Draw only points from contour\n"
"      -zspline                          Fit contour points with spline for curve\n"
"      -zsmooth <strength>               Fit contour points with smoothing spline\n"
"      -rainbow [-zrange <zmin> <zmax>]  Use pretty colors, with defined range\n"
"   -bitmap <surf>                       Bitmap output\n"
"      -zrange <zlow> <zhigh>            Range for color map\n"
"      -palette [AFM | HOT ... ]         Change to color palette for bitmaps\n"
"      -grid <nrow> <ncol>               Specify maximum number of grid elements\n"
"\n"
"   -plx {bottom | top}                  Specify plot against specific X axis\n"
"   -ply {left | right}                  Specify plot against specific Y axis\n"
"   -bottom | -top | -left | -right      Alternate specify of plot axes\n"
"\n"
"   -xscale                              Re-autoscale for X axis\n"
"   -yscale                              Re-autoscale for Y axis\n"
"   -rescale          | -xyscale         Re-autoscale for X and Y axis\n"
"   -xrange <xlow> <xhigh>               Plot against specified range\n"
"   -yrange <ylow> <yhigh>               Plot against specified range\n"
"   -zrange <zlow> <zhigh>               Plot against specified range\n"
"   -xyrange <xl> <xh> <yl> <yh>         Plot against specified XY range\n"
"   -xyzrange <xl> ... <zh>              Plot against specified XYZ range\n"
"\n"
"   -errx <expr>                         Symmetric X error bar expression\n"
"   -errxminus <expr> | -errxlow <expr>  Lower X error bar expression\n"
"   -errxplus  <expr> | -errxhigh <expr> Upper X error bar expression\n"
"   -erry <expr>                         Symmetric Y error bar expression\n"
"   -erryminus <expr> | -errylow <expr>  Lower Y error bar expression\n"
"   -erryplus <expr>  | -erryhigh <expr> Upper X error bar expression\n"
"   -errwidth <val>                      Width of error bar cross-ticks\n"
"\n"
"   -pt_label                            Draw data as point # (text)\n"
"   -exclude [-self | <curve>]           Draw excluding region for points\n"
"\n"
"   -slow | -slower | -slown <msecs>     Delay between drawing points (poor)\n"
"   -rainbow                             Rainbow color plot on 3D\n"
"   -palette [AFM | HOT ... ]            Change rainbow color palette\n"
"   -scope                               Draw a fake scope screen\n"
"   -aziz                                Special error bar drawing for Mr. A\n"
"\n"
" Examples: plot\n"
"           plot -f sin(x) -range -10 10 -points 200 -lt 1 -identify \"sin(x)\"\n"
"           plot c1 -sym FilledCircle -symsize 0.28 -pen Green -ids\n"
"           ov -fit -lt 2 -symsize 0.28 -exclude c1 -identify \"Best Fit\" -pen 2\n"
"           axes foreach (c1 c2 c3 c4) do ov %f\n";

/* ---------------------------------------------------------------------------

--------------------------------------------------------------------------- */
int GptPlotCurve(void) {

	REAL xi, yi, zi, t1, t3;
	int	i, j, n, type;
	REAL *x, *y, *z;
	BOOL local_x=FALSE, local_y=FALSE;					/* Did we allocate temp for X or Y arrays? */
	char token[DFLT_STR_SIZE];

/* Variables for error bars -- 0,1,2,3 correspond to XL, XH, YL, YH */
	GVCMDS *erreqn[4] = {NULL,NULL,NULL,NULL};		/* Equations			*/
	struct {
		int	Enable:1,										/* Are they enabled? */
		      X_Symmetric:1,									/* X bars symmetric?	*/
		      Y_Symmetric:1;
	} ErrorBars={FALSE, FALSE, FALSE};

	static REAL	 errwidth[2] = {0.1f, 0.1f};					/* Parameters	*/

	PLOT_TYPES PlotType = NORMAL;
	CMTYPE *citem;
	
	int		mypen=-1,mylwidth=-1,myltype=-1,mysym=-1;	/* Local changeable	*/
	int		max_rows=-1, max_cols=-1;						/* Local changeable for GRID command */
	int		lwidthhold, penhold;
	int		myplx=Gpt->plx, myply=Gpt->ply;		/* Local changeable	*/
	REAL		xmin,xmax, ymin,ymax, zmin,zmax;
	BOOL		MyXRange=FALSE, MyYRange=FALSE, MyZRange=FALSE;
	REAL		mysymsiz=Gpt->symsiz;					/* Local changeable	*/
	REAL		myvecsize = 0.0f;							/* Local changeable, 0 means no change */
	REAL     VecSave;										/* Save value */
	int		mynpoint = Gpt->npoint;					/* Local changeable	*/
	int		myspoint = 1;								/* Local changeable	*/
	LOGICAL	doids = Gpt->AutoIDs;					/* Local changeable	*/
	LOGICAL	LinesAndSymbols = FALSE;				/* Lines & Symbols	*/
	LOGICAL	SymbolRequested = FALSE;				/* Symbol request as option */
	LOGICAL  RainbowColors = FALSE;
	LOGICAL	DrawScopeAxes = FALSE;					/* Erase and redraw with scope */
	BOOL		Did_Lines, Did_Symbols;					/* Which have been drawn */
	char		ids_local[DFLT_STR_SIZE];				/* Make it big also		*/
	int		ms_delay=-1;								/* Delay between points */
	CONTOUR_INFO Contour_Info = {0.0,0.0, 0.0,0.0, 0.0,0.0, -1.0,-1.0,0.0, 1.0, FALSE, MULTIPLE_CONTOUR};
	LOGICAL	ExcludeSelf = FALSE;
	EXTERN	CURVE *PlotExcludeCurve;				/* From COMPLOT */
	EXTERN	REAL   PlotExcludeRadius;				/* From COMPLOT */
	void     *aptr;

	GVP_CONTINUUMPALETTE RainbowPalette = GV_PAL_DEFAULT;

/* First, look for a help request */
	if (LexCheckHelp("Plot", PlotHelp, NULL)) return(0);

/* Now, a few sanity checks */
	if (GptCurve == NULL && ! Gpt->mode_3d) PlotType = CONTOUR;		/* Will be implied CONTOUR plot */

	if (Gpt->mode_3d)
		PlotSet3DView(Gpt->view_d, Gpt->tilt, Gpt->skew, Gpt->rotate);

	if (Gpt->AutoAxes) {								/* Want axes to be drawn? */
		if (! PlotSystem(1, NULL, NULL)) return(-1);
		GptDrawAxes();
	}

	GptSetRange();
	if (GptCurve != NULL) {
		x    = GptCurve->x;
		y    = GptCurve->y;
		z    = (Gpt->mode_3d) ? GptCurve->z : NULL ;
		ncol = nrow = npt = GptCurve->npt;
		strscpy(ids_local, GptCurve->ids, sizeof(ids_local));
	} else if (GptSurface != NULL) {
		x    = GptSurface->x;
		y    = GptSurface->y;
		z    = GptSurface->z;
		npt  = GptSurface->npt;
		ncol = GptSurface->ncol;
		nrow = GptSurface->nrow;
		strscpy(ids_local, GptSurface->ids, sizeof(ids_local));
	} else {
		ERRprintf("ERROR!!!!  Either the curve or surface must be valid\n");
		return(-1);
	}

/* --- Search for any options on command line */
	SymbolRequested = FALSE;

	while (LexGetOption(token, sizeof(token))) {
		if ( (citem=LexCmdl(token, cmlist, sizeof(CMTYPE))) == NULL) {
			ERRprintf("ERROR: Unrecognized PLOT option (%s)\n", token);
			LexFlush();
			return(1);
		}
		switch (citem->rcode1) {
			case PLOTTYPE:							/* Change type of plot */
				PlotType = citem->rcode2;
				break;
			case CONTOUR_AT:
				Contour_Info.dz   = LexGetReal(1.0, "Single contour level (1.0): ");
				Contour_Info.mode = SINGLE_CONTOUR;
				break;
			case CONTOUR_DZ:
				Contour_Info.dz   = LexGetReal(1.0, "Major contour interval spacing (1.0): ");
				Contour_Info.mode = MULTIPLE_CONTOUR;
				break;
			case CONTOUR_DZ2:
				Contour_Info.dz2  = LexGetReal(0.2f, "Minor contour interval spacing (0.2): ");
				Contour_Info.mode = MULTIPLE_CONTOUR;
				break;
			case CONTOUR_ZSCAN:
				Contour_Info.zmin = LexGetReal( 0.0, "First contour level (0.0): ");
				Contour_Info.zmax = LexGetReal(10.0, "Last contour level (10.0): ");
				break;
			case CONTOUR_RIGID:
				Contour_Info.smooth = FALSE;
				break;
			case CONTOUR_SPLINE:
				Contour_Info.smooth = TRUE;
				Contour_Info.smoothing = 0.0;
				break;
			case CONTOUR_SMOOTH:
				Contour_Info.smooth = TRUE;
				Contour_Info.smoothing = LexGetReal(1.0, "Smoothing strength (1.0): ");
				break;
			case SYMBOL:							/* -SYMBOL */
				if (LexGetMathP(token, sizeof(token), "Symbol: ")) {
					mysym = PlotMatchSymbol(token, -1);
					SymbolRequested = TRUE;
					if (myltype != -1) LinesAndSymbols = TRUE;
				}
				break;
			case LTYPE:
				myltype = LexGetInt(0, "LType: ");
				if (mysym != -1) LinesAndSymbols = TRUE;
				break;
			case PEN:
				if (LexGetMathP(token, sizeof(token), "Pen color: ")) {
					mypen = PlotMatchColor(token, -1);
				}
				break;
			case SYMSIZE:
				mysymsiz = LexGetReal(Gpt->symsiz, "Size: ");
				break;
			case LINEWIDTH:
				mylwidth = nint(7*LexGetReal(1.0f, "LWidth: "));
				break;
			case VECSIZE:
				myvecsize = LexGetReal(0.001f, "Dashed line vector size: ");
				break;
			case NPOINT:
				mynpoint = LexGetInt(1, "NPoint: ");
				mynpoint = max(1,mynpoint);			/* Must be separate since fnc call! */
				break;
			case SPOINT:
				myspoint = LexGetInt(1, "SPoint: ");
				myspoint = max(1,myspoint);			/* Must be separate since fnc call! */
				break;
			case L_S:										/* -L&S (lines and symbols) */
				LinesAndSymbols = TRUE;
				break;
			case IDENT:										/* -Identify */
				LexGetStrExprP(ids_local, sizeof(ids_local), "Id: ");
				doids = TRUE;
				break;
			case IDS:										/* -ids		*/
				doids = TRUE;
				break;
			case NOIDS:										/* -noids	*/
				doids = FALSE;
				break;
			case SCALES:									/* New scaling laws */
				if ((int) citem->rcode2 != 2) {		/* As long as not -YSCALE */
					ArrayMinMax(x, ncol, &Gpt->xmin, &Gpt->xmax);
					PlotAutoScale(Gpt->xmin, Gpt->xmax, &Gpt->xmin, &Gpt->xmax, &t1, &t3, &i);
				}
				if ((int) citem->rcode2 != 1) {		/* As long as not -XSCALE */
					ArrayMinMax(y, nrow, &Gpt->ymin, &Gpt->ymax);
					PlotAutoScale(Gpt->ymin, Gpt->ymax, &Gpt->ymin, &Gpt->ymax, &t1, &t3, &i);
				}
				break;
			case XYRANGE:									/* New scaling laws */
				if ((int) citem->rcode2 == 1 || (int) citem->rcode2 > 3) {	/* X or XY */
					xmin = LexGetReal(Gpt->xmin, "X at left  (old): ");
					xmax = LexGetReal(Gpt->xmax, "X at right (old): ");
					MyXRange = TRUE;
				}
				if ((int) citem->rcode2 == 2 || (int) citem->rcode2 > 3) {	/* Y or XY */
					ymin = LexGetReal(Gpt->ymin, "Y at bottom (old): ");
					ymax = LexGetReal(Gpt->ymax, "Y at top    (old): ");
					MyYRange = TRUE;
				}
				if ((int) citem->rcode2 == 3 || (int) citem->rcode2 > 4) {	/* Z or XYZ */
					zmin = LexGetReal(Gpt->zmin, "Z at bottom (old): ");
					zmax = LexGetReal(Gpt->zmax, "Z at top    (old): ");
					MyZRange = TRUE;
				}
				break;
			case PLX:
				if (LexGetToken(token, sizeof(token))) 
					myplx = max(0, LexSelect(token, "BOTTOM TOP")-1);
				break;
			case PLY:
				if (LexGetToken(token, sizeof(token))) 
					myply = max(0, LexSelect(token, "LEFT RIGHT")-1);
				break;
			case BOTTOP:
				myplx = (int) citem->rcode2;
				break;
			case LEFTRIGHT:
				myply = (int) citem->rcode2;
				break;
			/* Error bar setup here */
			case ERRORS: {
				GVCMDS *myptr;

				if (! LexGetMathP(token, sizeof(token), "Error bar expression: ")) break;
				LEXESCAPE;
				if ( (myptr=GVDupCmds(GVParse(token, NULL))) == NULL) {
					gen_err("Illegal expression for error bars or unable to copy cmds");
					return(1);
				}
				ErrorBars.Enable = TRUE;				/* Something is TRUE */
				if ((int) citem->rcode2 & 0x40) ErrorBars.X_Symmetric = TRUE;
				if ((int) citem->rcode2 & 0x80) ErrorBars.Y_Symmetric = TRUE;
				erreqn[(int) citem->rcode2 & 0x03] = myptr;
				break;
			}
			case ERRWIDTH:									/* Error bar widths */
				errwidth[0] = LexGetReal(errwidth[0], "Cross width on X: ");
				errwidth[1] = LexGetReal(errwidth[1], "Cross width on Y: ");
				break;
			case EXCLUDE:									/* Exclusion */
				if (GptCurve == NULL) {
					ERRprintf("WARNING: -exclude only valid when working with curves\n");
					break;
				}
				if (! LexGetTokenP(token, sizeof(token), "Exclude curve: ")) break;
				if (stricmp(token, "-self") == 0) {
					if (( PlotExcludeCurve = (CURVE *) malloc(sizeof(CURVE))) != NULL) {
						*PlotExcludeCurve = *GptCurve;
						ExcludeSelf       = TRUE;
					}
				} else if (GVGetInfo(token, &type, &aptr)) {			/* Get pointer */
					if (type == GV_2DCURVE || type == GV_3DCURVE) {
						PlotExcludeCurve = *( (CURVE **) aptr);		/* Copy to PLOT */
					}
				} 
				if (PlotExcludeCurve == NULL) {
					gen_err2("EXCLUDE curve cannot be located", token);
					return(1);
				}
				if (PlotExcludeRadius<0.0f) PlotExcludeRadius = -mysymsiz*0.375f;
				break;
			case PALETTE:
				RainbowPalette = GVSelectContinuumPalette(NULL, GV_PAL_DEFAULT);
				if (RainbowPalette == GV_PAL_ERROR) RainbowPalette = GV_PAL_DEFAULT;	/* Just ignore, but don't crash */
				break;
			case RAINBOW:
				RainbowColors = TRUE;
				break;
			case GRID:
				max_rows = LexGetInt(1280, "Maximum number of rows in the bitmap (1280): ");
				max_cols = LexGetInt(1280, "Maximum number of columns in the bitmap (1280): ");
				break;
			case SCOPE:
				DrawScopeAxes = TRUE;
				break;
			case SLOWLY:
				switch ((int) citem->rcode2) {
					case 0:
						ms_delay = LexGetInt(0, "ms delay between pts (0-2000): ");
						break;
					case 1:
						ms_delay = 0; break;
					case 2:
						ms_delay = 30; break;
				}
				break;
		}
	}

/* One strange fix.  -lt 0 -sym x is interpreted as -sym x" */
	if (mysym == 0 && myltype == 0) { myltype = -1; LinesAndSymbols = FALSE; }
	
/* Are we to erase the existing axes and put up a scope axes? */
	if (DrawScopeAxes) {
		if (! PlotSystem(1, NULL, NULL)) return(-1);
		PlotDrawScopeFace();
	}

/* If symbol requested as option, void the line styles */
	if (myplx != Gpt->plx) {
		Gpt->xmin = Gpt->rmins[2*myplx];
		Gpt->xmax = Gpt->rmaxs[2*myplx];
	}
	if (myply != Gpt->ply) {
		Gpt->ymin = Gpt->rmins[2*myply+1];
		Gpt->ymax = Gpt->rmaxs[2*myply+1];
	}

	if (! MyXRange) {xmin = Gpt->xmin; xmax = Gpt->xmax;}
	if (! MyYRange) {ymin = Gpt->ymin; ymax = Gpt->ymax;}
	if (! MyZRange) {zmin = Gpt->zmin; zmax = Gpt->zmax;}

	if (Gpt->mode_3d) {
		PlotSet3DRange(xmin, xmax, ymin, ymax, zmin, zmax);
	} else {
		PlotSetRange(xmin, xmax, ymin, ymax);
	}

	xlow  = xmin - 0.02f*(xmax-xmin);
	xhigh = xmax + 0.02f*(xmax-xmin);
	ylow  = ymin - 0.02f*(ymax-ymin);
	yhigh = ymax + 0.02f*(ymax-ymin);
	zlow  = zmin - 0.02f*(zmax-zmin);
	zhigh = zmax + 0.02f*(zmax-zmin);
	
	OrderPair(&xlow,&xhigh);
	OrderPair(&ylow,&yhigh);
	OrderPair(&zlow,&zhigh);

	if (! PlotSystem(2, NULL, NULL)) {					/* Is plotter "on" */
		if (Gpt->mode_3d) PlotSet3DMode(FALSE);
		return(-1);
	}
	i = 1;
	if (PlotType != PLT_BITMAP) PlotSystem(5, NULL, &i);	/* Increment pen on all but bitmaps */

	if (mypen    >= 0) penhold    = PlotSelectPen(mypen);
	if (mylwidth >  0) lwidthhold = PlotSetLineWidth(mylwidth);
	if (myspoint < mynpoint) myspoint = mynpoint;	/* Spoint can't be less than npoint */
	PlotQueryLineType(NULL, &VecSave);					/* Save vector size for anyone wanting it */

	Did_Lines = Did_Symbols = FALSE;
	
/* Handle any use of non-linear axes.  Assuming X and Y linked are valid */
	if (myplx == 1 && Gpt->xtop == IS_NONLINEAR) {
		REAL *tmp;
		local_x = TRUE;
		tmp = malloc(npt*sizeof(*tmp));
		GVEvalArrayExpr(tmp, npt, "top_to_bottom(x)");
		x = tmp;
	}
	if (myply == 1 && Gpt->yright == IS_NONLINEAR) {
		REAL *tmp;
		local_y = TRUE;
		tmp = malloc(npt*sizeof(*tmp));
		GVEvalArrayExpr(tmp, npt, "right_to_left(y)");
		y = tmp;
	}

/* Handle a few of the strict PlotTypes first */
	if (PlotType == AZIZBARS) {
		if (GptCurve == NULL) goto ExitPlot;
		for (i=0; i<npt-1; i+=2) {
			if (SysChkBreak(FALSE)) break;
			if (z == NULL) {
				PlotMove(x[i],   y[i],   3);
				PlotMove(x[i+1], y[i+1], 2);
			} else {
				PlotMove3D(x[i],   y[i],   z[i],   3);
				PlotMove3D(x[i+2], y[i+1], z[i+1], 2);
			}
		}
		Did_Lines = TRUE;
		goto ExitPlot;

	} else if (PlotType == PT_LABEL) {
		if (GptCurve == NULL) goto ExitPlot;
		for (i=0; i<npt; i+=myspoint) {
			if (SysChkBreak(FALSE)) break;
			if ( InRange(x[i],y[i]) && ((z == NULL) || InRangeZ(z[i])) ) {
				sprintf(token, "%d", i);
				t1 = PlotQueryStringLength(mysymsiz, token, 0);
				if (z == NULL) {
					PlotConvert2DScales(USER_TO_INCH, x[i], y[i], &xi, &yi);
					PlotInchString(xi-0.5f*t1, yi-mysymsiz/2, mysymsiz, token, 0.0f, 0);
				} else {
					PlotConvert3DScales(USER_TO_INCH, x[i], y[i], z[i], &xi, &yi, &zi);
					Plot3DInchString(xi-0.5f*t1, yi-mysymsiz/2, zi, mysymsiz, token, 0.0f, 0);
				}
			}
		}
		Did_Lines = TRUE;
		goto ExitPlot;

/* Draw the surface as a bitmap in rainbow mode */
/* Alternative way in future - GptCreateSurface(GptSurface) */
	} else if (PlotType == PLT_BITMAP) {

		REAL zlow,zhigh,xt[2],yt[2],zt;
		int irow,icol,nrow,ncol, icol_inc, irow_inc;

		if (GptSurface == NULL) {
			ERRprintf("ERROR: Must have a surface to do a bitmap plot\n");
			goto NoBitmap;
		}

		nrow = GptSurface->nrow;
		ncol = GptSurface->ncol;

		if (zmin == zmax) {															/* Do we already have a valid range? */
			ArrayMinMax(z, npt, &zlow, &zhigh);									/* Figure out the range of the data	 */
			PlotAutoScale(zlow, zhigh, &zmin, &zmax, NULL, NULL, NULL);	/* Generic autoscaling based on data */
		}

/* As bitmaps may become extremely large, limit to an acceptable number */
		if (max_rows <= 0) max_rows = 1280;
		if (max_cols <= 0) max_cols = 1280;
		max_rows = max(4, min(max_rows,4096));						/* Reasonable values */
		max_cols = max(4, min(max_cols,4096));
		icol_inc = irow_inc = 1;											/* Limit bitmaps to drawable size */
		while (ncol/icol_inc > max_cols) icol_inc++;
		while (nrow/irow_inc > max_rows) irow_inc++;

		for (icol=0; icol<ncol; icol+=icol_inc) {
			xt[0] = (REAL) ((icol>=icol_inc)     ? (x[icol-icol_inc]+x[icol])/2 : 1.5*x[icol]-0.5*x[icol+icol_inc]) ;
			xt[1] = (REAL) ((icol+icol_inc<ncol) ? (x[icol+icol_inc]+x[icol])/2 : 1.5*x[icol]-0.5*x[icol-icol_inc]) ;
			if (xt[0] > xmax || xt[0] < xmin || xt[1] > xmax || xt[1] < xmin) continue;
			for (irow=0; irow<nrow; irow+=irow_inc) {
				yt[0] = (REAL) ((irow>=irow_inc)     ? (y[irow-irow_inc]+y[irow])/2 : 1.5*y[irow]-0.5*y[irow+irow_inc]) ;
				yt[1] = (REAL) ((irow+irow_inc<nrow) ? (y[irow+irow_inc]+y[irow])/2 : 1.5*y[irow]-0.5*y[irow-irow_inc]) ;
				if (yt[0] > ymax || yt[0] < ymin || yt[1] > ymax || yt[1] < ymin) continue;
				zt = z[irow+icol*nrow];
				PlotFillRect(xt[0], yt[0], xt[1], yt[1], GVSelectContinuumColor(zt, zmin, zmax, RainbowPalette));
			}
		}
NoBitmap:
		Did_Lines = Did_Symbols = FALSE;
		goto ExitPlot;
	}

/* Update the auto-updated symbol and linetypes */
	if (myltype < 0) {
		if (Gpt->AutoLineType) Gpt->linetype = (Gpt->linetype%7) + 1;
		myltype = abs(Gpt->linetype);
	}
	if (mysym < 0) {
		if ((myltype == 0 || LinesAndSymbols) && Gpt->AutoSymbols)  Gpt->symtype = (Gpt->symtype%13) + 1;
		mysym = abs(Gpt->symtype);
	}

/* Decide if we really want to be in 3D */
	if (PlotType == NORMAL && (GptSurface != NULL || z != NULL) ) goto Plot_3D;

/* Subsequent plot types can take both symbols, lines and error bars. */
/* Handle each separately (though symbols and error bars must be together */
	if (PlotType == CONTOUR) {				/* Many things turned off with contours */
		ErrorBars.Enable = FALSE;
		SymbolRequested = FALSE;
		LinesAndSymbols = FALSE;
		if (myltype == 0) myltype = 1;
	} else if (PlotType == HISTOGRAM || PlotType == BARGRAPH || PlotType == HBAR || PlotType == STICK) {
		if (! SymbolRequested && myltype == 0) myltype = 1;
	} else {
		if (SymbolRequested && ! LinesAndSymbols) myltype = 0;
	}
	Did_Symbols = (myltype == 0) || LinesAndSymbols || SymbolRequested;

	if (ErrorBars.Enable) {
		ERRORSYMBOL buf;

		if (! Did_Symbols) mysym = 0;					/* Disable symbols */

		memset(&buf, 0, sizeof(buf));
		buf.isym = mysym;
		buf.symsiz = mysymsiz;
		buf.widthx = errwidth[0];
		buf.widthy = errwidth[1];

		for (i=0; i<npt; i+=myspoint) {
			if (SysChkBreak(FALSE)) break;
			if (InRange(x[i], y[i])) {
				if (ExcludeSelf) PlotExcludeCurve->npt = i;
				buf.x = x[i];
				buf.y = y[i];
				for (j=0; j<4; j++) buf.errs[j] = ((erreqn[j] != NULL) ? GVTrimToReal(GVEvalCmdsI(erreqn[j],i,NULL)) : 0.0f);
				if (ErrorBars.X_Symmetric) buf.errs[1] = buf.errs[0];
				if (ErrorBars.Y_Symmetric) buf.errs[3] = buf.errs[2];
				PlotDrawErrorSymbol(&buf);
				if (ms_delay >= 0) {PlotFlush(); if (ms_delay>0) MilliSleep(ms_delay);}
			}
		}
/* Free the equation lists created for storing commands */
		for (i=0; i<4; i++) GVFreeCmds(erreqn[i]);		/* Free the storage */

	} else if (Did_Symbols) {
		if (mysym == 0) {										/* Special strange case */
			if (myltype == 0) {								/* Just dots				*/
				PlotLine3(x, y, npt, myspoint, &myltype);
			} else {
				Did_Symbols = FALSE;							/* Makes no sense			*/
			}
		} else {
			for (i=0; i<npt; i+=myspoint) {
				if (SysChkBreak(FALSE)) break;
				if (InRange(x[i],y[i])) {
					if (ExcludeSelf) PlotExcludeCurve->npt = i;
					PlotSymbol(x[i], y[i], mysymsiz, (char) abs(mysym));
					if (ms_delay >= 0) {if (ms_delay>0) MilliSleep(ms_delay); PlotFlush();}
				}
			}
		}
	}
	
/* And now the lines if required */
	if (GptCurve == NULL && PlotType != CONTOUR) goto ExitPlot;			/* Nothing to do */
	if (LinesAndSymbols && myltype == 0) myltype = 1;

	if (PlotType == CONTOUR) {
		CONTOUR_DATA rc;
		int maxpts;
		REAL zlow,zhigh,zval,dz,dz2;

		if (GptSurface == NULL) {
			ERRprintf("ERROR: Must have a surface to do a contour plot\n");
			goto NoContours;
		}
		Contour_Info.xmin = xmin; Contour_Info.xmax = xmax;				/* Range for actual plots */
		Contour_Info.ymin = ymin; Contour_Info.ymax = ymax;

		ArrayMinMax(z, npt, &zlow, &zhigh);										/* Figure out the range of the data		*/
		if (zlow == zhigh) goto NoContours;										/* And don't waste time						*/
		if (Contour_Info.zmin != Contour_Info.zmax) {						/* Is the range over-riden by request	*/
			zlow  = min(Contour_Info.zmin, Contour_Info.zmax);
			zhigh = max(Contour_Info.zmin, Contour_Info.zmax);
			PlotAutoScale(zlow, zhigh, NULL, NULL, &dz, &dz2, NULL);		/* Get recommended dz and dz2				*/
		} else {
			PlotAutoScale(zlow, zhigh, &zlow, &zhigh, &dz, &dz2, NULL);	/* Generic autoscaling based on data	*/
		}
		if (Contour_Info.dz   >= 0.0)  dz = Contour_Info.dz;				/* User override of spacings?				*/
		if (Contour_Info.dz2  >= 0.0) dz2 = Contour_Info.dz2;				/* Zero is to not do any					*/

		PlotSetLineType(abs(myltype), myvecsize);
		if (mylwidth < 0) mylwidth = lwidthhold = PlotSetLineWidth(7);

		maxpts = nrow*ncol/4;										/* Should be adequate */
		maxpts = max(  20000, maxpts);							/* But set lower limit */
		maxpts = min(1000000, maxpts);							/* And an upper limit */
		
		if (Contour_Info.mode == SINGLE_CONTOUR) {
			PlotSetLineWidth(nint(1.4*mylwidth));
			zval = Contour_Info.zval = Contour_Info.dz;						/* Value of interest set in dz */
			rc = Enum_Contour_3D_Surface(GptSurface, zval, maxpts);
			if (rc.npt != 0) {
				if (RainbowColors) PlotSelectPen(GVSelectContinuumColor(zval, zlow, zhigh, RainbowPalette));
				for (i=0; i<rc.chain_count; i++) {
					PlotContour(rc.x+rc.chain_list[i], rc.y+rc.chain_list[i], rc.chain_list[i+1]-rc.chain_list[i], &Contour_Info);
				}
				free(rc.x); free(rc.y); free(rc.chain_list);
			}
			PlotSetLineType(1, VecSave);

		} else {					/* Contour_Info.mode == MULTIPLE_CONTOUR */
			if (dz > 0) {
				PlotSetLineWidth(nint(1.4*mylwidth));
				for (zval=zlow; zval<zhigh+0.001*(zhigh-zlow); zval+=dz) {
					Contour_Info.zval = zval;
					rc = Enum_Contour_3D_Surface(GptSurface, zval, maxpts);
					if (rc.npt != 0) {
						if (RainbowColors) PlotSelectPen(GVSelectContinuumColor(zval, zlow, zhigh, RainbowPalette));
						for (i=0; i<rc.chain_count; i++) {
							PlotContour(rc.x+rc.chain_list[i], rc.y+rc.chain_list[i], rc.chain_list[i+1]-rc.chain_list[i], &Contour_Info);
						}
						free(rc.x); free(rc.y); free(rc.chain_list);
					}
				}
			}

			if (dz2 > 0) {
				PlotSetLineWidth(mylwidth);
				for (zval=zlow; zval<zhigh+0.001*(zhigh-zlow); zval+=dz2) {
					Contour_Info.zval = zval;
					if ( dz > 0 && fabs(zlow+dz*nint((zval-zlow)/dz)-zval) < 0.001*(zhigh-zlow)) continue;
					rc = Enum_Contour_3D_Surface(GptSurface, zval, maxpts);
					if (rc.npt != 0) {
						if (RainbowColors) PlotSelectPen(GVSelectContinuumColor(zval, zlow, zhigh, RainbowPalette));
						for (i=0; i<rc.chain_count; i++) {
							PlotContour(rc.x+rc.chain_list[i], rc.y+rc.chain_list[i], rc.chain_list[i+1]-rc.chain_list[i], &Contour_Info);
						}
						free(rc.x); free(rc.y); free(rc.chain_list);
					}
				}
			}
			PlotSetLineType(1, VecSave);
		}

NoContours:
		Did_Lines = TRUE;

	} else if (PlotType == STICK) {
		if (myltype == 0) myltype = 1;
		PlotSetLineType(myltype, myvecsize);

		for (i=0; i<npt; i+=mynpoint) {
			if (SysChkBreak(FALSE)) break;
			if (z == NULL) {
				PlotMove(x[i], 0.0f, 3);
				PlotMove(x[i], y[i], 2);
			} else {
				PlotMove3D(x[i], y[i], 0.0f, 3);
				PlotMove3D(x[i], y[i], z[i], 2);
			}
		}
		PlotSetLineType(1, VecSave);
		Did_Lines = TRUE;

	} else if (PlotType == HISTOGRAM) {
		if (npt <= mynpoint) goto ExitPlot;			/* Nothing to do */

		if (myltype == 0) myltype = 1;				/* Force solid if not */
		PlotSetLineType(myltype, myvecsize);

		t1 = x[0] - (x[mynpoint]-x[0])/2;
		PlotMove(t1, y[0], 3);							/* Shifted first position */
		t1 = (x[0] + x[mynpoint]) / 2;
		for (i=mynpoint; i<npt; i+=mynpoint) {
			if (SysChkBreak(FALSE)) break;
			PlotMove(t1, y[i-mynpoint], 2);
			PlotMove(t1, y[i], 2);
			if (i+mynpoint < npt) t1 = (x[i]+x[i+mynpoint]) / 2;
			n = i;
		}
		PlotMove(x[n]+(x[n]-t1), y[n], 2);
		PlotSetLineType(1, VecSave);
		Did_Lines = TRUE;

	} else if (PlotType == BARGRAPH) {
		if (npt <= mynpoint) goto ExitPlot;			/* Nothing to do */

		if (myltype == 0) myltype = 1;				/* Force solid if not */
		PlotSetLineType(myltype, myvecsize);

		t1 = x[0] - (x[mynpoint]-x[0])/2;
		PlotMove(t1, 0.0f, 3);							/* First position */
		for (i=0; i<npt; i+=mynpoint) {
			if (SysChkBreak(FALSE)) break;
			PlotMove(t1, y[i], 2);
			if (i+mynpoint < npt) {
				t1 = (x[i] + x[i+mynpoint]) / 2;
			} else {
				t1 = x[i] + (x[i]-t1);
			}
			PlotMove(t1, y[i], 2);
			PlotMove(t1, 0.0f, 2);
		}
		PlotSetLineType(1, VecSave);
		Did_Lines = TRUE;

	} else if (PlotType == HBAR) {
		if (npt <= mynpoint) goto ExitPlot;			/* Nothing to do */

		if (myltype == 0) myltype = 1;				/* Force solid if not */
		PlotSetLineType(myltype, myvecsize);

		t1 = y[0] - (y[mynpoint]-y[0])/2;
		PlotMove(0.0f, t1, 3);							/* First position */
		for (i=0; i<npt; i+=mynpoint) {
			if (SysChkBreak(FALSE)) break;
			PlotMove(x[i], t1, 2);
			if (i+mynpoint < npt) {
				t1 = (y[i] + y[i+mynpoint]) / 2;
			} else {
				t1 = y[i] + (y[i]-t1);
			}
			PlotMove(x[i], t1, 2);
			PlotMove(0.0f, t1, 2);
		}
		PlotSetLineType(1, VecSave);
		Did_Lines = TRUE;

	} else if (LinesAndSymbols) {
		PlotSetLineType(abs(myltype), myvecsize);
		DrawLinesAndSymbols(x, y, npt, mynpoint, myspoint, mysymsiz);
		PlotSetLineType(1, VecSave);
		Did_Lines = TRUE;

	} else if (myltype != 0) {
		PlotSetLineType(abs(myltype), myvecsize);
		PlotLine3(x, y, npt, mynpoint, &myltype);
		PlotSetLineType(1, VecSave);
		Did_Lines = TRUE;
	}
	goto ExitPlot;


Plot_3D:
	if (SymbolRequested && ! LinesAndSymbols) myltype = 0;
	if (GptSurface != NULL) {
/* ------------------------------------------------------------
--  irow,icol have the number of lines to draw:  
--      irow lines of constant Y (rrow points of X along each)
--      icol lines of constant X (rcol points of Y along each)
--------------------------------------------------------------- */
		int ii,jj, irow, icol, rrow, rcol, savepen;
		int cmin,cmax, rmin,rmax;										/* Min/max cols/rows to use	*/
		REAL zt;

		/* Scan the row/col positions (y/x) to determine which rows/cols are actually on screen */
		rmin = nrow; rmax = 0;
		for (i=0; i<nrow; i++) {
			if (y[i] < ylow || y[i] > yhigh) continue;
			if (i < rmin) rmin = i;
			if (i > rmax) rmax = i;
		}
		cmin = ncol; cmax = 0;
		for (i=0; i<ncol; i++) {
			if (x[i] < xlow || x[i] > xhigh) continue;
			if (i < cmin) cmin = i;
			if (i > cmax) cmax = i;
		}
		if (cmax < cmin || rmax < rmin) goto No3DToDraw;
		cmax++; rmax++;													/* Now will be ncol/nrow if all show */

		icol = min(Gpt->mesh[0], cmax-cmin);						/* # of constant X cols to do (ncol) */
		irow = min(Gpt->mesh[1], rmax-rmin);						/* # of constant Y rows to do (nrow) */
		rrow = min(Gpt->resolution[0], cmax-cmin);				/* # of X points in each row  (ncol) */
		rcol = min(Gpt->resolution[1], rmax-rmin);				/* # of Y points in each col  (nrow) */
				
		savepen = PlotSelectPen(-1);									/* Query current pen */
		if (Gpt->HideLines == HIDDEN_OFF) {
			for (ii=0; ii<irow; ii++) {								/* Constant Y columns */
				if (SysChkBreak(FALSE)) break;
				i = (int) (rmin + ii/(irow-1.0)*(rmax-rmin-1.0) + 0.5);	/* Actual row */
				for (jj=0; jj<rrow; jj++) {
					j = (int) (cmin + jj/(rrow-1.0)*(cmax-cmin-1.0) + 0.5);
					zt = z[i+j*GptSurface->nrowmax];
					if (RainbowColors) PlotSelectPen(GVSelectContinuumColor(zt, Gpt->zmin, Gpt->zmax, RainbowPalette));
/*					if (RainbowColors) PlotSelectPen((int) (1+14.99*(zt-Gpt->zmax)/(Gpt->zmin-Gpt->zmax))); */
					PlotMove3D(x[j], y[i], zt, jj==0 ? 3 : 2);
				}
			}
			for (jj=0; jj<icol; jj++) {
				if (SysChkBreak(FALSE)) break;
				j = (int) (cmin + jj/(icol-1.0)*(cmax-cmin-1.0) + 0.5);
				for (ii=0; ii<rcol; ii++) {
					i = (int) (rmin + ii/(rcol-1.0)*(rmax-rmin-1.0) + 0.5);
					zt = z[i+j*GptSurface->nrowmax];
					if (RainbowColors) PlotSelectPen(GVSelectContinuumColor(zt, Gpt->zmin, Gpt->zmax, RainbowPalette));
/*					if (RainbowColors) PlotSelectPen((int) (1+14.99*(zt-Gpt->zmax)/(Gpt->zmin-Gpt->zmax))); */
					PlotMove3D(x[j], y[i], zt, ii==0 ? 3 : 2);
				}
			}

/* ------------------------------------------------------------
-- Determine the direction the plot needs to go.  If 
--    rev_x / rev_y     Indexes of X or Y need to be reversed
--     
------------------------------------------------------------ */
		} else if (Gpt->HideLines == HIDDEN_PANEL || Gpt->HideLines == HIDDEN_ON) {
			int i1,i2,j1,j2, di,dj, rev_x, rev_y, skip_x, skip_y;
			REAL z1,z2,z3;
			PlotConvert3DScales(USER_TO_PIXEL, x[0],     y[0],z[0], NULL,NULL,&z1);
			PlotConvert3DScales(USER_TO_PIXEL, x[ncol-1],y[0],z[0], NULL,NULL,&z2);
			PlotConvert3DScales(USER_TO_PIXEL, x[0],y[nrow-1],z[0], NULL,NULL,&z3);
			rev_x = (z2 >= z1);								/* X direction reversed */
			rev_y = (z3 >= z1);								/* Y direction reversed */
			skip_x = (ncol / rrow);							/* Skipping factor */
			skip_y = (nrow / rcol);
					
			if (! RainbowColors) PlotSelectPen(0);		/* Fill erased */
			for (ii=0; ii<irow-1; ii++) {					/* Constant Y columns */
				if (SysChkBreak(FALSE)) break;
				i2 = (rev_y) ? (ii) : (irow-2-ii);
				i1 = (int) (rmin + i2    /(irow-1.0) * (rmax-rmin-1.0) + 0.5);	/* Lower row */
				i2 = (int) (rmin + (i2+1)/(irow-1.0) * (rmax-rmin-1.0) + 0.5);	/* Upper row */
				for (jj=0; jj<icol-1; jj++) {
					j2 = (rev_x) ? (jj) : (icol-2-jj);
					j1 = (int) ( cmin + j2    /(icol-1.0) * (cmax-cmin-1.0) + 0.5);
					j2 = (int) ( cmin + (j2+1)/(icol-1.0) * (cmax-cmin-1.0) + 0.5);
					if (RainbowColors) {
						for (i=i1; i<i2; i+=skip_y) {
							di = min(skip_y, i2-i);
							for (j=j1; j<j2; j+=skip_x) {
								dj = min(skip_x, j2-j);
								FillRect(x[j], x[j+dj], y[i], y[i+di],
											z[i+    j    *GptSurface->nrowmax],
								         z[i+   (j+dj)*GptSurface->nrowmax],
								         z[i+di+ j    *GptSurface->nrowmax],
											z[i+di+(j+dj)*GptSurface->nrowmax]);
							}
						}
					}

					if (! RainbowColors || savepen != 0) {
						PlotBeginPath();
						PlotMove3D(x[j1], y[i1], z[i1+j1*GptSurface->nrowmax], 3);
						for (j=j1+skip_x; j<=j2+(skip_x-1); j+=skip_x) {
							if (j > j2) j = j2;
							PlotMove3D(x[j], y[i1], z[i1+j*GptSurface->nrowmax], 2);
						}
						for (i=i1; i<=i2+(skip_y-1); i+=skip_y) {
							if (i > i2) i = i2;
							PlotMove3D(x[j2], y[i], z[i+j2*GptSurface->nrowmax], 2);
						}
						for (j=j2; j>=j1-(skip_x-1); j-=skip_x) {
							if (j < j1) j = j1;
							PlotMove3D(x[j], y[i2], z[i2+j*GptSurface->nrowmax], 2);
						}
						for (i=i2; i>=i1-(skip_y-1); i-=skip_y) {
							if (i < i1) i = i1;
							PlotMove3D(x[j1], y[i], z[i+j1*GptSurface->nrowmax], 2);
						}
						PlotEndPath();
						if (! RainbowColors) PlotFillPath(0,0);	/* Erase */
						PlotStrokePath(savepen, 0);
					}
				}
			}
		}
		PlotSelectPen(savepen);

No3DToDraw:
		Did_Lines = (myltype != 0);

	} else if (z != NULL) {
		int savepen;
		if (RainbowColors) savepen = PlotSelectPen(-1);		/* Query current pen */
		for (i=0; i<npt; i+=mynpoint) {
			if (SysChkBreak(FALSE)) break;
			if (RainbowColors) PlotSelectPen(GVSelectContinuumColor(z[i], Gpt->zmin, Gpt->zmax, RainbowPalette));
/*			if (RainbowColors) PlotSelectPen((int) (1+14.99*(z[i]-Gpt->zmax)/(Gpt->zmin-Gpt->zmax))); */
			if (myltype == 0 && mysym == 0) {
				if (InRange3D(x[i], y[i], z[i])) {
					PlotMove3D(x[i], y[i], z[i], 3);
					PlotMove3D(x[i], y[i], z[i], 2);
				}
				Did_Lines = TRUE;
			} else if (myltype == 0) {
				if (InRange3D(x[i], y[i], z[i])) {
					Plot3DSymbol(x[i], y[i], z[i], mysymsiz, (char) abs(mysym));
				}
				Did_Symbols = TRUE;
			} else {
				PlotMove3D(x[i], y[i], z[i], i==0 ? 3 : 2);
				Did_Lines = TRUE;
			}
		}
		if (RainbowColors) PlotSelectPen(savepen);
	}
	goto ExitPlot;
	
/* ---------------------------------------------------------------------------
-- For the IDS commands, reset linewidth/pen before so the text is drawn in
-- the default linewidth scale.  However, have legend component drawn with the
-- appropriate linewidth is so desired.  However, reset pen after the PlotID
-- so colors are correct
--------------------------------------------------------------------------- */
ExitPlot:
	if (local_x) free(x);					/* Free allocated space if needed */
	if (local_y) free(y);					/* X or Y may be due to non-linear axis use */

	if (mylwidth > 0) PlotSetLineWidth(lwidthhold);			/* Reset linewidth */
	if (doids) {
		if (Did_Lines && Did_Symbols)	   LexInsText("-l&s");		/* Get legend right */
		if (!(Did_Lines || Did_Symbols)) LexInsText("-nomark");	/* Get legend right */
		PlotID(myltype, mysym, mypen, mylwidth, ids_local);
	}
	if (mypen > 0) PlotSelectPen(penhold);

	if (ExcludeSelf) free(PlotExcludeCurve);
	PlotExcludeCurve = NULL;
	if (Gpt->mode_3d) PlotSet3DMode(FALSE);					/* And make sure it's off */
	PlotFlush();

	if (SysChkBreak(FALSE)) {
		ERRprintf("ERROR: Plot aborted by ^C\n");
		LexFlush();
	}
	return 0;
}

#if 0
static void WriteDebug(char *fname, REAL *x, REAL *y, int npt, char *id, double rval) {
	FILE *funit;
	int i;
	funit = fopen(fname, "w");
	fprintf(funit, "/* %s (%g)\n", id, rval);
	for (i=0; i<npt; i++) fprintf(funit, "%g %g\n", x[i], y[i]);
	fclose(funit);
	return;
}
#endif

/* ===========================================================================
-- Routine to plot contour levels on the screen
=========================================================================== */
static void PlotContour(REAL *x, REAL*y, int npt, CONTOUR_INFO *info) {

	REAL *xi, *yi, *ri, xilast, xinow, yilast, yinow, dr;
	REAL rmin, rmax, rval;
	double xsmooth, ysmooth;
	void *splx, *sply;
	int i,j, npt2, nsub, istart, iend;
	BOOL closed;
	
/* Rigid is trivial - just draw as solid segments */
	if (! info->smooth || npt < 4) {								/* Very simple, just draw it as given segments */
		if (npt < 2) return;
		PlotMove(x[0], y[0], 3);									/* Move to start with pen up */
		for (i=1; i<npt; i++) PlotMove(x[i], y[i], 2);
		PlotMove(x[npt-1],y[npt-1], 3);							/* And lift pen */
		return;
	}

/* For any sort of smooth contour, don't deal with less than 4 points */
	xsmooth = info->smoothing*fabs(info->xmax-info->xmin)/5000.0;		/* Smoothing constants */
	ysmooth = info->smoothing*fabs(info->ymax-info->ymin)/5000.0;

	closed = x[0]==x[npt-1] && y[0]==y[npt-1];								/* Open or closed segment */
	npt2 = closed ? npt+2 : npt;
	xi = calloc(npt2, sizeof(*xi));	yi = calloc(npt2, sizeof(*yi));  ri = calloc(npt2, sizeof(*yi));
	istart = closed ? 1 : 0;

#define	MIN_DR_DISTANCE	(0.02)
	npt2 = istart;
	PlotConvert2DScales(USER_TO_INCH, x[0], y[0], &xilast, &yilast);
	ri[npt2] = 0.0; xi[npt2] = x[0]; yi[npt2] = y[0]; npt2++;
	for (i=1; i<npt; i++) {
		PlotConvert2DScales(USER_TO_INCH, x[i], y[i], &xinow, &yinow);
		dr = (REAL) sqrt(pow(xinow-xilast,2)+pow(yinow-yilast,2));
		if (dr > MIN_DR_DISTANCE) {
			ri[npt2] = ri[npt2-1] + dr;
			xi[npt2] = x[i];	yi[npt2] = y[i];
			xilast = xinow;	yilast = yinow;
			npt2++;
		}
	}
	if (closed) {													/* Duplicate first and last points so curvature maintained */
		xi[0]    = xi[npt2-2];	yi[0] = yi[npt2-2];	ri[0]    = -(ri[npt2-1] - ri[npt2-2]);
		xi[npt2] = xi[2];			yi[npt2] = yi[2];		ri[npt2] = ri[npt2-1] + (ri[2]-ri[1]);
		npt2++;
	}

#define	PT_SPACING	(0.05)
#define	MAX_SMOOTH	(500)

/* As we've eliminated very closely spaced points, might have problem, so check again */
	if (npt2 < 4) {													/* So small, we just do as segments */
		PlotMove(x[0], y[0], 3);									/* Move to start with pen up */
		for (i=1; i<npt; i++) PlotMove(x[i], y[i], 2);
		PlotMove(x[npt-1],y[npt-1], 3);							/* And lift pen */

/* If no smoothing, or if we have too many points to think about smoothing, use SPLINE only */
	} else if (npt2 > MAX_SMOOTH || xsmooth <= 0.0 || ysmooth <= 0.0) {
		if ( (splx = GVFitSpline(NULL, ri, xi, npt2, 0)) == NULL) goto AbortContour;
		if ( (sply = GVFitSpline(NULL, ri, yi, npt2, 0)) == NULL) goto AbortContour;
		iend = closed ? npt2-1 : npt2 ;							/* Run only istart to iend				*/
		PlotMove(xi[istart], yi[istart], 3);					/* Move to starting point				*/
		for (i=istart+1; i<iend; i++) {							/* Which point to do next				*/
			if (ri[i]-ri[i-1] > PT_SPACING) {					/* Do we need intermediate points?	*/
				nsub = (int) ((ri[i]-ri[i-1])/PT_SPACING);	/* Number to insert						*/
				for (j=0; j<nsub; j++) {
					rval = ri[i-1] + (ri[i]-ri[i-1])/(nsub+1)*(j+1);
					PlotMove(GVEvalSpline(splx, rval), GVEvalSpline(sply, rval), 2);
				}
			}
			PlotMove(xi[i], yi[i], 2);								/* And to this point						*/
		}
		if (closed) {
			PlotMove(xi[1], y[1], 2);								/* Relift pen at the end */
			PlotMove(xi[1], y[1], 3);								/* Relift pen at the end */
		} else {
			PlotMove(xi[npt2-1], y[npt2-1], 3);					/* Relift pen at the end */
		}
		free(splx); free(sply);

/* Smooth the segment as data density is inadequate */
	} else {
		if ( (splx = GVFitSmoothSpline(NULL, ri, xi, npt2, (REAL) xsmooth, TRUE)) == NULL) goto AbortContour;
		if ( (sply = GVFitSmoothSpline(NULL, ri, yi, npt2, (REAL) ysmooth, TRUE)) == NULL) goto AbortContour;
		rmin = 0.0; rmax = closed ? ri[npt2-2] : ri[npt2-1];
		nsub = min(1000,max(8,nint((rmax-rmin)/PT_SPACING)));
		PlotMove(GVEvalSpline(splx,0.0), GVEvalSpline(sply,0.0), 3);
		for (i=1; i<nsub; i++) {
			rval = rmin + (rmax-rmin)/(nsub-1)*i;
			PlotMove(GVEvalSpline(splx,rval), GVEvalSpline(sply,rval), 2);
		}
		if (closed) {													/* Close the circuit */
			PlotMove(GVEvalSpline(splx,0), GVEvalSpline(sply,0), 2);
			PlotMove(GVEvalSpline(splx,0), GVEvalSpline(sply,0), 3);
		} else {															/* Lift the pen */
			PlotMove(GVEvalSpline(splx,rmax), GVEvalSpline(sply,rmax), 3);
		}
		free(splx); free(sply);
	}
	
AbortContour:
	free(xi); free(yi); free(ri);
	return;
}

/* ---------------------------------------------------------------------------
-- Routine to draw lines without lines passing through previously drawn symbols.
-- Properly implemented now.  Excludes a circle of diameter size around each of
-- the symbols.  Draws any portion of the lines that are not occluded by these
-- disks.  Properly handles out of order data, etc.
--
-- Usage: DrawLinesAndSymbols(REAL *x, REAL *y, int npt, 
--                            int npoint, int spoint, REAL size);
--
-- Inputs: x,y - pointer to array of points
--         npt - number of points in the curve
--         npoint - draw lines between every npoint'th points
--         spoint - symbols were drawn at every spoint'th point
--         size   - size of the symbol
--
-- Output: Draws the vectors to the active device
--
-- Return: void
--------------------------------------------------------------------------- */
static void DrawLinesAndSymbols(REAL *x, REAL *y, int npt, 
										  int npoint, int spoint, REAL size) {
	
	int i,j,k, npairs;
	REAL x0,y0, x1,y1, x2,y2, r;
	double a,b,c,d, t1,t2;
	BOOL penup;
	struct {
		REAL t1, t2;
	} *excl, tmp;

/* ... Save current line and set to requested type ... */
	npairs = (npt/spoint)+1;													/* How many exclusion zones */
	excl = malloc(npairs*sizeof(*excl));
	
/* ... Now, go through and draw the lines ... */
	if (size <= 0.0f) size = 1E-10f;											/* Avoid problems */
	r = size/2;																		/* Exclusion radius (inches) */
	PlotConvert2DScales(USER_TO_INCH, x[0], y[0], &x1, &y1);			/* First point */

	penup = TRUE;
	for (i=npoint; i<npt; i+=npoint) {										/* Line segments */
		PlotConvert2DScales(USER_TO_INCH, x[i],y[i], &x2,&y2);		/* Position of symbol in inches */
		npairs = 0;																	/* No exclusion pairs yet */
		for (j=0; j<npt; j+=spoint) {											/* And all points */
			PlotConvert2DScales(USER_TO_INCH, x[j],y[j], &x0,&y0);	/* Position of symbol in inches */
			a = (x2-x1)*(x2-x1)+(y2-y1)*(y2-y1);							/* Coefficients in a*t^2+b*t+c = r^2 */
			b = 2*(x1-x0)*(x2-x1)+2*(y1-y0)*(y2-y1);
			c = (x1-x0)*(x1-x0)+(y1-y0)*(y1-y0)-r*r;
			d = b*b-4*a*c;															/* Discriminant */
			if (d < 0) continue;
			if (a == 0) goto no_line;											/* Occurs if x1=x2 and y1=y2 */
			t1 = (-b-sqrt(d))/(2*a);											/* Two solutions */
			t2 = (-b+sqrt(d))/(2*a);
			if (t1 < 0 && t2 > 1) goto no_line;
			if (t1 < 0 && t2 < 0) continue;									/* Not within line segment */
			if (t1 > 1 && t2 > 1) continue; 
			if (t1 < 0) t1 = 0;													/* Limited range */
			if (t2 > 1) t2 = 1;
			for (k=0; k<npairs; k++) {											/* Is it an extension of existing? */
				if (t1 <= excl[k].t1 && t2 >= excl[k].t1) {				/* Encompasses starting point */
					excl[k].t1 = (REAL) t1;
					excl[k].t2 = (REAL) max(excl[k].t2, t2);
					t2 = t1 = -1; break;											/* Done! */
				} else if (t1 <= excl[k].t2 && t2 >= excl[k].t2) {		/* Encompasses ending point */
					excl[k].t2 = (REAL) t2;
					t2 = t1 = -1; break;
				} else if (t1 >= excl[k].t1 && t2 <= excl[k].t2) {
					t2 = t1 = -1; break;
				}
			}
			if (t1 >= 0) { excl[npairs].t1 = (REAL) t1; excl[npairs].t2 = (REAL) t2; npairs++;
			}
		}

		while (npairs > 1) {													/* Bubble sort - essentially TRUE */
			for (j=0; j<npairs-1; j++) {
				if (excl[j].t1 <= excl[j+1].t1) continue;
				tmp = excl[j]; excl[j] = excl[j+1]; excl[j+1] = tmp;
				break;
			}
			if (j==(npairs-1)) break;
		}
		t1 = 0;
		for (j=0; j<npairs; j++) {
			if (t1 < excl[j].t1) {											/* Draw to exclude point */
				if (penup) PlotMoveInch( (REAL) (x1+(x2-x1)*t1), (REAL) (y1+(y2-y1)*t1) ,3);
				PlotMoveInch(x1+(x2-x1)*excl[j].t1,y1+(y2-y1)*excl[j].t1,2);
			}
			t1 = excl[j].t2;													/* Next starting point */
			penup = TRUE;
		}
		if (t1 < 1.0) {
			if (penup) PlotMoveInch( (REAL) (x1+(x2-x1)*t1), (REAL) (y1+(y2-y1)*t1), 3);
			PlotMoveInch(x2,y2,2); 
			penup = FALSE;
		}

no_line:
		x1 = x2;				/* Ready for next line segment */
		y1 = y2;
	}

/* Free memory space, and return */
	free(excl);
	return;
}

/* ===========================================================================
-- Routine to fill a rectangle with colors based on Z values
=========================================================================== */
#define ZCOLOR(zval)  (1+14.99*(zval-Gpt->zmax)/(Gpt->zmin-Gpt->zmax))
#define RZCOLOR(zval) ((REAL) ZCOLOR(zval))

static void FillRect(REAL x1, REAL x2, REAL y1, REAL y2, 
							REAL z1, REAL z2, REAL z3, REAL z4) {

	int i1,i2,i3,i4;							/* Color from corners */

	i1 = (int) ZCOLOR(z1);
	i2 = (int) ZCOLOR(z2);
	i3 = (int) ZCOLOR(z3);
	i4 = (int) ZCOLOR(z4);

	if (i1 == i2 && i2 == i3 && i3 == i4) {
		PlotBeginPath();
		PlotMove3D(x1,y1,z1,3);
		PlotMove3D(x2,y1,z2,2);
		PlotMove3D(x2,y2,z4,2);
		PlotMove3D(x1,y2,z3,2);
		PlotMove3D(x1,y1,z1,2);
		PlotEndPath();
		PlotFillPath(i1, 0);
	} else {
		FillTriangle(x1,y1,z1, x2,y1,z2, x1,y2,z3);
		FillTriangle(x2,y2,z4, x2,y1,z2, x1,y2,z3);
	}
	return;
}


static void FillTriangle(REAL x1,REAL y1,REAL z1, REAL x2, REAL y2, 
								 REAL z2, REAL x3, REAL y3, REAL z3) {

	struct {
		REAL x,y,z;
	} p0, p1, p2;
	REAL cz1, cz2, cz3, f;
	int   iz1, iz2, iz3, i;

/* Order so color(z1) < z2 < z3 */
	cz1 = RZCOLOR(z1);
	cz2 = RZCOLOR(z2);
	cz3 = RZCOLOR(z3);
	if (cz1 > cz3) {
		f = x1; x1 = x3; x3 = f;
		f = y1; y1 = y3; y3 = f;
		f = z1; z1 = z3; z3 = f;
		f = cz1; cz1 = cz3; cz3 = f;
	}
	if (cz2 > cz3) {
		f = x2; x2 = x3; x3 = f;
		f = y2; y2 = y3; y3 = f;
		f = z2; z2 = z3; z3 = f;
		f = cz2; cz2 = cz3; cz3 = f;
	}
	if (cz1 > cz2) {
		f = x2; x2 = x1; x1 = f;
		f = y2; y2 = y1; y1 = f;
		f = z2; z2 = z1; z1 = f;
		f = cz2; cz2 = cz1; cz1 = f;
	}		
	iz1 = (int) cz1;
	iz2 = (int) cz2;
	iz3 = (int) cz3;
	
	p1.x = x1; p1.y = y1; p1.z = z1;				/* Corner start		*/
	p2.x = -1.0f;										/* No second point	*/
		
	iz3 = min(15, iz3);								/* Limit to 15th color */
	iz1 = max(1,  iz1);								/* And limit first one */
	iz2 = max(iz1,min(iz2,iz3));					/* And keep iz2 in range */

	for (i=iz1; i<=iz3; i++) {						/* Do all colors */
		PlotBeginPath();

		p0 = p1;											/* Save start pt for return	*/
		PlotMove3D(p1.x, p1.y, p1.z, 3);			/* Establish start position	*/
		if (p2.x > 0) {								/* Previous edge to follow?	*/
			PlotMove3D(p2.x, p2.y, p2.z, 2);
		}
		if (i == iz2) {								/* Is corner 2 this color?		*/
			PlotMove3D(x2, y2, z2, 2);
		}

		if (i == iz3) {								/* Last one just uses pt 3		*/
			PlotMove3D(x3, y3, z3, 2);
		} else {
			if (i+1 <= iz2) {							/* Point will be along 1-2		*/
				f = ( (i+1)-cz1) / (cz2-cz1);		/* Fraction from 1-2				*/
				p2.x = x2*f + x1*(1-f);
				p2.y = y2*f + y1*(1-f);
				p2.z = z2*f + z1*(1-f);
			} else {
				f = ( (i+1)-cz2) / (cz3-cz2);		/* Fraction along 2-3			*/
				p2.x = x3*f + x2*(1-f);
				p2.y = y3*f + y2*(1-f);
				p2.z = z3*f + z2*(1-f);
			}
			f = ( (i+1)-cz3) / (cz1-cz3);			/* Fraction from 1-3				*/
			p1.x = x1*f + x3*(1-f);
			p1.y = y1*f + y3*(1-f);
			p1.z = z1*f + z3*(1-f);
			PlotMove3D(p2.x, p2.y, p2.z, 2);
			PlotMove3D(p1.x, p1.y, p1.z, 2);
		}
		PlotMove3D(p0.x, p0.y, p0.z, 2);			/* Return to origin */
		PlotEndPath();
		PlotFillPath(i,0);
	}

	return;
}

#if 0
/*============================================================================
-- Function to create a BitMap structure from a surface function
--
-- Usage: void *GptCreateBitmap(SURFACE *surf);
--
-- Inputs: surf - pointer to a valid SURFACE structure
--
-- Output: none
--
-- Return: Pointer to allocated space with valid bitmap.  Data immediately
--         follows the header information.  On error, returns NULL.  The
--         pointer type is (void *) to allow routines to use without having
--         to understand the internal information
============================================================================ */
#pragma pack(2)

typedef struct _GPT_BITMAPFILEHEADER {
	UINT2	bfType;								/* Must be "BM" == 19778					*/
	UINT4	bfSize;								/* File size in bytes						*/
	UINT2	bfReserved[2];						/* Unused - must be zero					*/
	UINT4	bfOffBits;							/* Offset to where bitmap data starts	*/
} GPT_BITMAPFILEHEADER;

typedef struct _GPT_BITMAPINFOHEADER {
	UINT4 biSize;								/* Size of this header in bytes			*/
	UINT4	biWidth;								/* Width of image in pixels				*/
	UINT4 biHeight;							/* Height of image in pixels				*/
	UINT2	biPlanes;							/* # planes of target device (must=0)	*/
	UINT2	biBitCount;							/* Number of bits per pixel				*/
	UINT4	biCompression;						/* Compression type (0 => none)			*/
	UINT4	biSizeImage;						/* Bytes in image data (0 ok if uncompressed) */
	UINT4	biXPelsPerMeter;					/* Obvious, but usually zero				*/
	UINT4	biYPelsPerMeter;					/* Obvious, but usually zero				*/
	UINT4	biClrUsed;							/* # colors used. 0 => use biBitCount	*/
	UINT4	biClrImportant;					/* # colors important.  0 => all			*/
} GPT_BITMAPINFOHEADER;

typedef struct _GPT_RGBQUAD {				/* Order of bytes for a color index		*/
	BYTE rgbs[4];
} GPT_RGBQUAD;

#pragma pack()

void *GptCreateBitmap(SURFACE *surf) {

	int i,j,ii,jj,ineed, nrow,ncol, nrowdim,ncoldim, rgb;
	UCHAR *data;
	REAL *z,zval;
	GPT_BITMAPINFOHEADER *bmih;

	if (surf == NULL) return(NULL);

	nrowdim = surf->nrow;
	ncoldim = surf->ncol;						/* Dimensioned size */
	nrow =  nrowdim;								/* Number of rows unconstrained			*/
	ncol = (ncoldim+2) & 0xFFFC;				/* Will force it closest multiple of 4 */
	
	ineed = sizeof(*bmih) + 3*nrow*ncol;	/* Total space required for header and data */
	if ( (bmih = calloc(ineed, 1)) == NULL) {
		ERRprintf("ERROR: Unable to allocate a bitmap info header structure\n");
		return(NULL);
	}
	bmih->biSize = sizeof(*bmih);				/* Size of this header in bytes			*/
	bmih->biWidth = ncol;						/* Width of image in pixels				*/
	bmih->biHeight = nrow;						/* Height of image in pixels				*/
	bmih->biPlanes = 1;							/* # planes of target device (must=0)	*/
	bmih->biBitCount = 24;						/* Number of bits per pixel				*/
	bmih->biCompression = 0;					/* Compression type (0 => none)			*/
	bmih->biSizeImage = nrow * ncol * 3;	/* Image data Bytes (0 ok if uncompressed) */
	bmih->biXPelsPerMeter = 0;					/* Obvious, but usually zero				*/
	bmih->biYPelsPerMeter = 0;					/* Obvious, but usually zero				*/
	bmih->biClrUsed = 0;							/* # colors used. 0 => use biBitCount	*/
	bmih->biClrImportant = 0;					/* # colors important.  0 => all			*/

	z = surf->z;
	data = ((BYTE *) bmih) + sizeof(*bmih);	/* Where does data start */
	for (i=0; i<nrow; i++) {
		ii = min(i,nrowdim-1);
		for (j=0; j<ncol; j++) {
			jj = min(j,ncoldim-1);
			zval = z[jj*surf->nrow+ii];
			rgb = GVSelectContinuumColor(zval, zminz, zmaxz, -1);	/* zminz,zmaxz defined in gptdef.h */
			*data++ = B_FROM_RGB(rgb);			/* Set into correct color format */
			*data++ = G_FROM_RGB(rgb);
			*data++ = R_FROM_RGB(rgb);
		}
	}

	{
		GPT_BITMAPFILEHEADER bmfh;
		FILE *funit;

		bmfh.bfType = 19778;
		bmfh.bfSize = sizeof(bmfh)+ineed;
		bmfh.bfReserved[0] = bmfh.bfReserved[1] = 0;
		bmfh.bfOffBits = sizeof(bmfh)+sizeof(*bmih);
		funit = fopen("test.bmp", "wb");
		fwrite(&bmfh, 1, sizeof(bmfh), funit);
		fwrite(bmih, 1, ineed, funit);
		fclose(funit);
	}

	return (void *) bmih;
}
#endif
