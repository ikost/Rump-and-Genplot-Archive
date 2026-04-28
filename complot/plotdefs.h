/* complot.ins - Common block for general graphics routines

 Important Notes:
   1. CLIP.ASM has definition of FIXPLOT also
   2. CPLTX3 is required for HCOPY to reestablish environment.  Any new
      variables in CPLTX3 should be added to the end to ensure compatability
      with old HCOPY files.
   3. Version # of common block CPLTX3 set as cpltvr.  Should be changed each
      time variables added to CPLTX3 for HCOPY.
-------------------------------------------------------------------------- */

#ifndef _INC_PLOTDEFS
#define _INC_PLOTDEFS

#define	NTYPE				6			/* Number of linetypes current available */
#define	SVBUFSIZE		1024		/* Size of save buffer */
#define	SVNSAVES			20			/* Number of marks saved */
#define	N_MAP_COLORS	16			/* Number of mapped colors */

#include <limits.h>						/* Need PATH_MAX in this file */

/* ----------------------------------------------------------------
   Various typedefs for use below
------------------------------------------------------------------- */
typedef struct _DRVBLOCK DRVBLOCK;		/* Dummy declaration */
typedef LOGICAL (PLOTDRIVER) (INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp);

int PlotFindDriver(char *drivername, PLOTDRIVER **driver, INTEGER *class);

/* These are the colors of the default 16 color palette */
#define	C_WHITE			(MY_RGB(255,255,255))		/* 0 */
#define	C_BLACK			(MY_RGB(0,0,0))				/* 1 */
#define	C_RED				(MY_RGB(255,0,0))				/* 2 */
#define	C_GREEN			(MY_RGB(0,255,0))				/* 3 */
#define	C_BLUE			(MY_RGB(0,0,255))				/* 4 */
#define	C_MAGENTA		(MY_RGB(255,0,255))			/* 5 */
#define	C_CYAN			(MY_RGB(0,255,255))			/* 6 */
#define	C_YELLOW			(MY_RGB(255,255,0))			/* 7 */
#define	C_DARKRED		(MY_RGB(127,0,0))				/* 8 */
#define	C_DARKGREEN		(MY_RGB(0,127,0))				/* 9 */
#define	C_DARKBLUE		(MY_RGB(0,0,127))				/* 10 */
#define	C_DARKMAGENTA	(MY_RGB(127,0,127))			/* 11 */
#define	C_DARKCYAN		(MY_RGB(0,127,127))			/* 12 */
#define	C_DARKYELLOW	(MY_RGB(127,127,0))			/* 13 */
#define	C_DARKGRAY		(MY_RGB( 77, 77, 77))		/* 14 */
#define	C_PALEGRAY		(MY_RGB(153,153,153))		/* 15 */

/* ID of the original mapping for symbols in character set 0 */
#define S_OPENSQUARE				0
#define S_OPENCIRCLE				1
#define S_OPENTRIANGLE			2
#define S_CROSS					3
#define S_X							4
#define S_OPENDIAMOND			5
#define S_OPENSTAR				6
#define S_FILLEDSQUARE			7
#define S_FILLEDCIRCLE			8
#define S_FILLEDTRIANGLE		9
#define S_FILLEDLEFTTRIANGLE	10
#define S_ASTERISK				11
#define S_FILLEDRIGHTTRIANGLE	12
#define S_FILLEDSTAR				13
#define S_STAROFDAVID			14

/* Filled in with text corresponding to names of original (ordinal) symbols */
#define	NUMBER_OF_SYMBOLS		15
#ifdef SYMBOL_C_SOURCE
	EXPORT	const char *SymbolNames[NUMBER_OF_SYMBOLS];			/* Names (based on original)	*/
	EXPORT			int	SymbolMap[NUMBER_OF_SYMBOLS+1];			/* Order symbols (w/ extra)	*/
	EXPORT			REAL	SymbolScale[NUMBER_OF_SYMBOLS+1];		/* Symbol scaling (in use order) */
#else
	IMPORT	const char *SymbolNames[NUMBER_OF_SYMBOLS];			/* Names (based on original)	*/
	IMPORT			int	SymbolMap[NUMBER_OF_SYMBOLS+1];			/* Order symbols (w/ extra)	*/
	IMPORT			REAL	SymbolScale[NUMBER_OF_SYMBOLS+1];		/* Symbol scaling (in use order) */
#endif
/* Handle color names via a lookup table which is initialized from data file */
typedef struct _COLORTABLE {
	char *name;
	INT32 color;
} COLORTABLE;

COLORTABLE *ColorTable;						/* Listing of known color names	*/

/* ----------------------------------------------------------------
   Driver information.  Only enough to restart same one if desired 
------------------------------------------------------------------- */
#define	IO_CHANNEL_SIZE	80
#define	DRIVER_NAME_SIZE	40
		
typedef struct _PLT_DEVICEINFO {
	LOGICAL Initialized;							/* Is device initialized			*/
	PLOTDRIVER *dsptch;							/* Plot dispatch routine			*/
	void    *DriverBlock;						/* Pointer for driver parms		*/
	CHAR    DriverName[DRIVER_NAME_SIZE];	/* Device driver name				*/
	CHAR    IO_Channel[IO_CHANNEL_SIZE];	/* I/O channel as specified		*/
	INTEGER Class;									/* Primary device class value		*/
	INTEGER SubDevice;							/* Sub-device within the device	*/
	INTEGER Options;								/* Specific bit options to drive */
	INTEGER NumberPens;							/* Number of pens for device		*/
	INTEGER xperinch,yperinch;					/* X,Y dots per inch					*/
	INTEGER xmax, ymax;							/* Maximum X,Y on page				*/
	INTEGER flag;									/* Random flag in device			*/
	INTEGER StatusBits;							/* Capabilities status bits		*/
	INTEGER ypmax;									/* Max actual plotter Y pixels	*/
	INTEGER Autoflush;							/* Flush screen on AXIS, etc?		*/
	INTEGER	Capabilities;						/* Capabilities bitmap				*/
} PLT_DEVICEINFO;

/* ---------------------------------------------------------
    Plotting environment variables (local to each screen)
--------------------------------------------------------- */
typedef enum _ORIENTATION {LANDSCAPE=0, PORTRAIT=270, INV_LANDSCAPE=180, INV_PORTRAIT=90} ORIENTATION;

typedef struct _PLT_WINDOWINFO {
	INTEGER  identifier,					/* Identifer specifying is Window	*/
				sequence;					/* Sequence number of Window			*/
	struct _PLT_WINDOWINFO *LastWindow;	/* Last window active					*/

	ORIENTATION orient;

/* size, margin and origin are linked to expression evaluator as arrays */
	REAL		factr,					/* Overall plot scale factor					*/
				xsize, ysize,			/* Size of plot user wants to use			*/
				xmarg[2], ymarg[2],	/* Margins in the X and Y directions		*/
				xorg, yorg,				/* X, Y origin (inches)							*/
				clpxl,clpxh,			/* Clipping band for X coordinate (in)		*/
				clpyl,clpyh,			/* Clipping band for Y coordinate (in)		*/
				xloc, yloc;				/* X, Y current location (inches)			*/

	PLOT_PALETTE *palette;		/* Mapping pen # to RGB type color			*/
	INT32		AxesFillColor;		/* Axes fill color								*/
	INT32		PageFillColor;		/* Page fill color								*/
	INT32		AreaFillColor;		/* Area fill color								*/
	INT32		colour;				/* Current pen color								*/

	INTEGER 	visib,				/* visibility (light, dark, complement)	*/
				jstyle,				/* Line style for plotting						*/
				spdsav,				/* Default pen speed								*/
				lintyp,				/* Current line type								*/
				chrmap,				/* Character map (SYMBOL)						*/
				clpmod;				/* Clip mode 0=>box, 1=>max, 2=>user vals */
	LOGICAL	clipsymbols;		/* Should symbols in graph be clipped		*/
	REAL		zforce;				/* Zero forcing for scaling      (AUTOSC) */

	LOGICAL	usrnbl;				/* Are user coordinate shift enabled		*/
	LOGICAL	mode_3d;				/* 3D plotting mode?								*/
	REAL		xoff, yoff,			/* X, Y offset       - user coordinates	*/
				xfact, yfact;		/* X, Y scale factor - user coordinates	*/
	REAL		xmin, xmax,			/* Range of the X axis							*/
				ymin, ymax,			/* Range of the Y axis							*/
				zmin, zmax;			/* Range of the Z axis							*/
	REAL		rotate[3];			/* Rotation angles								*/
	REAL		view_d;				/* Viewing distance								*/

	INTEGER	dshpen,				/* Current state of dashed pen (2=>down)	*/
				segbgn,				/* Index in SEGLNS to beginning segments	*/
				segend,				/* Index in SEGLNS for end of segments		*/
				iseg;					/* Current index into SEGLNS					*/
	REAL		seglen,				/* Length of current segment					*/
				patsiz;				/* Base size of pattern segment (0.003in)	*/

/* -------------------------------------------------------------------------
-- Data past SAVE not restored by save.c common block control.  In particular,
-- positioning info for timestamp and ID is static relative to hc undo
-------------------------------------------------------------------------- */
	struct _SAVE {
		FILE		*Unit;				/* File unit for saves				*/
		char		Name[PATH_MAX];	/* Filename of the save unit		*/
		LOGICAL	On;					/* Is save currently enabled		*/
		int		*svptr;				/* Pointer into block				*/
		int		block[SVBUFSIZE];	/* Buffer for commands				*/
		long		curr_pos;
		long		undo_pos;
		char		savcur[32];
		struct _marks {
			char	savcur[32];
			long	posn;
		} Marks[SVNSAVES];
		long		LastRestorePosn;
	} Save;

	struct {							/* Legend Information							*/
		REAL leftskip;				/* Space left from  left axis line  (IDS) */
		REAL topskip;				/* Space left under top  axis line  (IDS) */
		REAL size;					/* Height of characters (inches)    (IDS) */
		REAL spacing;				/* Line spacing (IDSIZE units)      (IDS) */
		REAL linesize;				/* Length of example line           (IDS) */
		REAL yp;						/* Last distance plotted				(IDS) */
		REAL vecsize;				/* Vector size on repeat				(IDS)	*/
		REAL len_max;				/* Longest string drawn					(IDS) */
	} ID;

	struct {							/* Time Stamp Parameters						*/
		REAL X,Y,Size;				/* Position and size of timestamp			*/
		CHAR Encode[DFLT_STR_SIZE];	/* "format" for the timestamp info	*/
	} TimeStamp;					/* Parms for time stamp				(PLSUPP) */

} PLT_WINDOWINFO;

/* ---------------------------------------------------------
    Axis parameters (which can be changed)
--------------------------------------------------------- */
typedef struct _AXIS_PARMS {
	REAL MajorTick;				/* Major tick mark length */
	REAL MinorTick;				/* Minor tick mark length */
	REAL MinLabelSize;			/* Minimum character size */
	REAL MaxLabelSize;			/* Maximum character size */
	REAL TitleSize;				/* Size of the title      */
	struct {
		REAL Bottom,Top;			/* Top and bottom title offsets     (AXIS) */
		REAL Left,Right;			/* Left and right title offsets     (AXIS) */
	} TitleOffset;					/* Offset of titles on axis			(AXIS) */
	struct {
		INTEGER Line, Major, Minor, Labels, Title, Tertiary;
	} Color;							/* Color of components of axis		(AXIS) */
	struct {
		REAL Line, Major, Minor, Labels, Title, Tertiary;
	} Width;							 /* Width of components of axis		(AXIS) */
} AXIS_PARMS;
	
/* -------------------------------------------------------------------
    FIXPLOT variables definition

	These variables and parameters are modified everytime someone changes the
	device or active window, or changes parameters affecting the window.
	fixplot() should be called after modification.

   WARNING: If variables are modified, confirm changes in CLIP.ASM or
            corresponding routine.
--------------------------------------------------------------------- */
typedef struct _FIXPARMS {

	LOGICAL	IsValid;					/* Is this structure valid? */
	
	REAL		xscale, yscale;		/* X, Y plotter scales (dots per inch) */
	REAL     xhigh,  yhigh;			/* X, Y high limits (inches)				*/
	REAL		factr;					/* Actual scaling factor used				*/

	REAL		pclpxl, pclpyl,		/* Lower clip window in pixels			*/
				pclpxh, pclpyh,		/* Upper clip window in pixels			*/
				patx,   paty;			/* Conversion from prescaled to PATSIZ */

	INTEGER	fixmod;					/* Pen mode variable							*/

	REAL		ui[4][4],				/* From user to inches						*/
				iu[4][4],				/* Invert inches to user					*/
				ip[4][4],				/* From inches to plotter pixels			*/
				ig[4][4],				/* Inch to grid (no portrait/landscape) */
				up[4][4],				/* From user to pixels						*/
				gp[4][4],				/* General intermediate transform		*/
				ug[4][4];				/* User to grid (no portrait/landscape) */

	INTEGER	orient_angle;			/* Orientation angle (PORTRAIT, etc)	*/

} FIXPARMS;
	
/* -------------------------------------------------------------------
    Symbol superscripting parameters
-------------------------------------------------------------------- */
typedef struct _SYMPARMS {
	REAL	  SubscriptOffset;	/* Offset down for subscript		(SYMBOL) */
	REAL	  SuperscriptOffset;	/* Offset up for superscript		(SYMBOL) */
	REAL	  ScriptSize;			/* Subscript height ratio			(SYMBOL) */
	INTEGER DefaultCharMap;		/* Default character set map		(SYMBOL)	*/
} SYMPARMS;
	
/* -------------------------------------------------------------------
    Sub-Page layout parameters
-------------------------------------------------------------------- */
typedef struct _SUBPAGE {
	BOOL	active, init, erase;		/* Is it enabled, initialized?		*/
	BOOL	need_erase;					/* Do we need to erase next shot?	*/
	BOOL	page_set;					/* Is a specific page requested?		*/
	int	page;							/* Page 0 = 0x0, 1 = 0x1, ...			*/
	int	nrows, ncols;				/* In Y and in X							*/
	REAL	factr, xshift,yshift;	/* Internal values set					*/
} SUBPAGE;

/* -------------------------------------------------------------------
    Global Accessible parameter tables.  Initialized in rst_comp.c
-------------------------------------------------------------------- */
#ifdef RST_COMP_C_SOURCE
	EXPORT PLT_DEVICEINFO *PL_Device;		/* Device structure pointer	*/
	EXPORT PLT_DEVICEINFO *PL_Tablet;		/* Tablet structure pointer	*/
	EXPORT PLT_WINDOWINFO *PlotWindow;		/* Complot Window pointer		*/
	EXPORT AXIS_PARMS PL_Axis;					/* Axis parameters				*/
	EXPORT FIXPARMS	PL_Plot;					/* Plotting scales parms		*/
	EXPORT SYMPARMS	PL_Symbols;				/* Symbol sub/super parms		*/
#else
	IMPORT PLT_DEVICEINFO *PL_Device;		/* Device structure pointer	*/
	IMPORT PLT_DEVICEINFO *PL_Tablet;		/* Tablet structure pointer	*/
	IMPORT PLT_WINDOWINFO *PlotWindow;		/* Complot Window pointer		*/
	IMPORT AXIS_PARMS PL_Axis;					/* Axis parameters				*/
	IMPORT FIXPARMS	PL_Plot;					/* Plotting scales parms		*/
	IMPORT SYMPARMS	PL_Symbols;				/* Symbol sub/super parms		*/
#endif

EXTERN SUBPAGE    PL_SubPage;				/* Sub-page layout parms		*/
EXTERN CURVE		*PlotExcludeCurve;	/* Plot exclusion curve			*/
EXTERN REAL			PlotExcludeRadius;	/* Radius to exclude around	*/
EXTERN INTEGER		PL_segind[];			/* Segmented lines (index)		*/
EXTERN INTEGER		PL_seglns[];			/* Segmented lines (segments)	*/

#define	DEVICE	PL_Device				/* Need PL_Device so passable name	*/
													/* Code written with DEVICE though	*/
#define	TABLET	PL_Tablet				/* Do same with Tablet for now		*/

/* ---------------------------- */
/* Prototypes for save routines */
/* ---------------------------- */
int  SV_Request(int key, FILE *stream);
void SV_Plot(FILE *stream, LOGICAL NewFrame);
void SV_PutCmd(int key, UINT nintb, UINT nreal, UINT nchar);
void SV_PutInt(int ival);
void SV_PutReal(REAL rval);
void SV_PutStr(char *str, UINT nchars);

/* ------------------------------------------------- */
/* Internal prototypes not generally for outside use */
/* ------------------------------------------------- */
#ifdef _INC_COMPLOT										/* Needed due to DspBrush */
	void PlotSetBrush(INT32 color, DspBrush *brush);
#endif
	LOGICAL PlotInBounds(REAL x, REAL y, REAL z);
void PlotAnnoteSet(int key);

EXTERN CHAR PlotDeviceDatFilename[PATH_MAX];		/* For setdev.c & plsystem.c only */

#if (defined NT && defined NEED_COMPLOT_MUTEX_INFO)
	EXTERN char		Complot_Mutex_Name[32];
	EXTERN HANDLE	Complot_Mutex_Handle;
#endif

#endif /* _INC_PLOTDEFS */
