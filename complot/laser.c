/* HPRASTER.F77 -- Driver for HPRASTER type devices */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

#ifdef SLAVE
	#define Laser_Driver SlaveDriver
#endif

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#ifdef NT
	#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "complot.h"

#include "drvclass.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#ifdef MSC60
   #define	STARTCOMMAND	"start/n "
	#define	SORTPROGRAM		"vsort.run"
	#define	SORTOPTIONS		""
#elif NT
	#define	SORTPROGRAM		"vsort.exe"
#elif OS2
	#define	STARTCOMMAND	""
	#define	SORTPROGRAM		"vsort.run"
	#define	SORTOPTIONS		""
#else
	#define	STARTCOMMAND	""
	#define	SORTPROGRAM		"vsort.run"
	#define	SORTOPTIONS		" -silent"
#endif
		
#define	LIMIT(i,low,high)		(min((high) , max((i),(low))))
#define	nint(x)					((int) ((x)+0.5))

#define	ENDPLT			32766			/* End of plot flag */
#define	ENDBLK			32765			/* End of block flag */
#define	MY_BLOCK_SIZE	2048			/* Bytes/block for F77 OPEN */

#define	VectorsPerRecord	MY_BLOCK_SIZE/sizeof(VECTOR)	/* Vectors in a record */

#define	FL_SINGLEDOTS		0x01					/* Single dots (really!)		*/
#define	FL_NOFORMFEED		0x02					/* Suppress FF on output		*/
#define	FL_NORESET			0x04					/* Suppress RESET on output	*/
#define	FL_NOLINEWIDTH		0x08					/* Suppress wide lines			*/
#define	FL_NOCOMPRESS		0x10					/* Suppress compression			*/
#define	FL_NOBLANKS			0x20					/* Suppress blank lines			*/
#define	FL_IMMEDIATEPAGE	0x40					/* Print pages immediately		*/
#define	FL_ROTATE180		0x80					/* Rotate page by 180 degrees	*/
#define	FL_ROTATE90			0x100					/* Rotate by 90 degrees			*/

#define	SingleDots	(blk->IO_Flag & FL_SINGLEDOTS)	/* Single dots (real)	*/
#define	NoFormFeed	(blk->IO_Flag & FL_NOFORMFEED)	/* Suppress FF output	*/
#define	NoReset		(blk->IO_Flag & FL_NORESET)		/* Suppress RESET			*/
#define	NoLineWidth	(blk->IO_Flag & FL_NOLINEWIDTH)	/* Suppress wide lines	*/
#define	NoCompress	(blk->IO_Flag & FL_NOCOMPRESS)	/* Suppress compression	*/
#define	NoBlanks		(blk->IO_Flag & FL_NOBLANKS)		/* Suppress blank lines	*/
#define	Rotate180	(blk->IO_Flag & FL_ROTATE180)		/* Rotate page 180		*/
#define	Rotate90		(blk->IO_Flag & FL_ROTATE90)		/* Rotate page 90			*/

typedef struct _vector {
	short	x ,y;								/* X,Y value */
	short	dx,dy;							/* Movement  */
	short fl;								/* Color & linewidth */
} VECTOR;

typedef struct _DEVPARS {				/* Sub-device definitions within class */
	int xperinch, yperinch;
	int xmax, ymax;
	int numpens;
	int FlagOrMask;
} DEVPARS;

typedef struct _DEVICEPARMS {			/* Class identification		 */
	int drvtype;							/* Code from drvclass.h		 */
	int ndev;								/* Number of sub-devs valid */
	DEVPARS *devpar;						/* Sub-parameter table			*/
} DEVICEPARMS;
	
#define	TRANSFERINFO_VERSION	0x0201	/* Version 2.01 */
typedef struct _TransferInfo {
	int		Version;							/* Transfer info version	*/
	int		Class;							/* Device class				*/
	int		SubDevice,						/* Sub-device specified		*/
				IO_Flag;							/* I/O flag options			*/
	char		IO_Chan[DFLT_STR_SIZE];		/* I/O channel information */
	int		xperinch,yperinch;			/* Resolutions					*/
	int		xmax,ymax;						/* Maximum X,Y values		*/
	int		numpens;							/* Number of pens used		*/
} TRANSFERINFO;

PRIVATE TRANSFERINFO *TransferInfo;

typedef struct _DRVBLOCK {					/* These need to quasi-static			*/
	INTEGER	Class,							/* Driver class required				*/
				SubDevice,						/* Sub-device specified					*/
				IO_Flag;							/* I/O flag options						*/
	char		IO_Chan[DFLT_STR_SIZE];		/* I/O channel information				*/
	char		fname[PATH_MAX];				/* Vector file name						*/
	int		lunit;							/* Unit opened for vectors				*/
	INTEGER	xperinch,yperinch;			/* Resolutions								*/
	INTEGER	xmax,ymax;						/* Maximum X,Y values					*/
	INTEGER	numpens;							/* Number of pens implemented			*/
	INTEGER	xl,yl,xh,yh;					/* Clipping limits						*/
	REAL		lwscale;							/* 150 dpi linewidth convert factor */

	INTEGER	bufptr,							/* Buffer pointer */
				num_vects,						/* Number of vectors written */
				color,							/* Current drawing color */
				linwidth;						/* Current drawing width */

	VECTOR	vb1[VectorsPerRecord];		/* Vector block */
	VECTOR	vbjunk;							/* One more so write(MY_BLOCK_SIZE) safe */
} DRVBLOCK;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */
LOGICAL Laser_Driver(int key, DRVBLOCK *DriverBlock, DSP *dsp);

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
PRIVATE LOGICAL DrawMarker(DRVBLOCK *blk, int isym, int x, int y, int size, int angle);
PRIVATE void invect(DRVBLOCK *blk, VECTOR *v1);
PRIVATE void veceof(DRVBLOCK *blk);
PRIVATE void vecwrt(DRVBLOCK *blk);

static BOOL RequestSort(char *fname);
static void StopSortProcess(void);
static BOOL RestartSortProcess(void);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ============================================================================
-- Usage Guide:
--
--     RASTER - COMPLOT driver for HPLASER JET II
--
--     Usage: LOG = DRVRAS(cmd, PARMS)
--
--     Inputs: cmd   - Command - See DRIVER.INS    for definitions
--             PARMS - Variable dimensioned array with parameters transfer
--                     Type and direction depend on command.
--
--     Output: DRVRAS - Success of operation
--
-- Supported devices: 
--          LaserJet, PaintJet, DeskJet, DumbJet
--
-- Secondary Parameters:
--          +01 ==> Single dot, rather than 5 dots, on SYM 0 LT 0 draws
--          +02 ==> Suppress final FF when closing
--          +04 ==> Suppress reset sequences on open/close
--          +08 ==> Suppress linewidth control (use single pixel)
--          +16 ==> Suppress compression (if possible)
--          +32 ==> Suppress blank lines at top of scan
--          +64 ==> Generate plots at each frame operation!
============================================================================ */
DEVPARS LaserJetParms[] = {	
			{300, 300, 3180, 2400, 1,  0},						/* 0 -> 300x300 DPI */
			{300, 300, 3180, 2400, 1,  0},						/* 1 -> 300x300 DPI */
			{150, 150, 1590, 1200, 1,  0},						/* 2 -> 150x150 DPI */
			{100, 100, 1060,  800, 1,  FL_NOLINEWIDTH},		/* 3 -> 100x100 DPI */
			{ 75,  75,  795,  600, 1,  FL_NOLINEWIDTH} };	/* 4 ->  75x 75 DPI */
DEVPARS LaserJet4Parms[] = {	
			{600, 600, 6360, 4800, 1,  0},						/* 0 -> 600x600 DPI */
			{600, 600, 6360, 4800, 1,  0},						/* 1 -> 600x600 DPI */
			{300, 300, 3180, 2400, 1,  0},						/* 2 -> 300x300 DPI */
			{200, 200, 2120, 1600, 1,  0},						/* 3 -> 200x200 DPI */
			{150, 150, 1590, 1200, 1,  0},						/* 4 -> 150x150 DPI */
			{100, 100, 1060,  800, 1,  FL_NOLINEWIDTH},		/* 5 -> 100x100 DPI */
			{ 75,  75,  795,  600, 1,  FL_NOLINEWIDTH} };	/* 6 ->  75x 75 DPI */
DEVPARS DeskJetParms[] = {	
			{300, 300, 3180, 2400, 1,  0},						/* 0 -> 300x300 DPI */
			{300, 300, 3180, 2400, 1,  0},						/* 1 -> 300x300 DPI */
			{150, 150, 1590, 1200, 1,  0},						/* 2 -> 150x150 DPI */
			{100, 100, 1060,  800, 1,  FL_NOLINEWIDTH},		/* 3 -> 100x100 DPI */
			{ 75,  75,  795,  600, 1,  FL_NOLINEWIDTH} };	/* 4 ->  75x 75 DPI */
DEVPARS DeskJetCParms[] = {	
			{300, 300, 3180, 2400, 7,  0},						/* 0 -> 300x300 DPI */
			{300, 300, 3180, 2400, 7,  0},						/* 1 -> 300x300 DPI */
			{150, 150, 1590, 1200, 7,  0},						/* 2 -> 150x150 DPI */
			{100, 100, 1060,  800, 7,  FL_NOLINEWIDTH},		/* 3 -> 100x100 DPI */
			{ 75,  75,  795,  600, 7,  FL_NOLINEWIDTH} };	/* 4 ->  75x 75 DPI */
DEVPARS PaintJetParms[] = {
			{180, 180, 1908, 1440, 1,  0},						/* 0 -> 180x180 DPI */
			{180, 180, 1908, 1440, 3,  0},						/* 1 -> 180x180 DPI */
			{180, 180, 1908, 1440, 7,  0},						/* 2 -> 180x180 DPI */
			{180, 180, 1908, 1440, 15, 0},						/* 3 -> 180x180 DPI */
			{ 90,  90,  954,  720, 1,  FL_NOLINEWIDTH},		/* 4 ->  90x 90 DPI */
			{ 90,  90,  954,  720, 3,  FL_NOLINEWIDTH},		/* 5 ->  90x 90 DPI */
			{ 90,  90,  954,  720, 7,  FL_NOLINEWIDTH},		/* 6 ->  90x 90 DPI */
			{ 90,  90,  954,  720, 15, FL_NOLINEWIDTH} };	/* 7 ->  90x 90 DPI */
DEVPARS QuietJetParms[] = {	
			{192, 192, 2036, 1536, 1,  0},						/* 0 -> 192x192 DPI */
			{192, 192, 2036, 1536, 1,  0},						/* 1 -> 192x192 DPI */
			{ 96,  96, 1018,  768, 1,  FL_NOLINEWIDTH} };	/* 2 ->  96x 96 DPI */

DEVPARS MX80Parms[] = {												/* MX 80 type printers */
			{ 72,  60,  800,  480, 1,  FL_NOLINEWIDTH},		/* 0 ->  72x 60 DPI */
			{ 72,  60,  800,  480, 1,  FL_NOLINEWIDTH},		/* 1 ->  72x 60 DPI */
			{ 72, 120,  800,  960, 1,  FL_NOLINEWIDTH},		/* 2 ->  72x120 DPI */
			{216, 120, 2400,  960, 1,  FL_NOLINEWIDTH} };	/* 3 -> 216x120 DPI */
DEVPARS OkidataParms[] = {											/* Okidata printers */
			{ 72,  72,  800,  576, 1,  FL_NOLINEWIDTH} };
DEVPARS LQ500Parms[] = {											/* 24 pin EPSON LQ-500 */
			{216,  60, 2160,  480, 1,  FL_NOLINEWIDTH},		/* 0 ->  216x60 DPI */
			{216,  60, 2160,  480, 1,  FL_NOLINEWIDTH},		/* 1 ->  216x60 DPI */
			{216, 120, 2160,  960, 1,  FL_NOLINEWIDTH},		/* 2 ->  216x120 DPI */
			{216, 180, 2160, 1440, 1,  0} };						/* 3 ->  216x180 DPI */
DEVPARS GeminiParms[] = {					/* GEMINI 10-x printers */
			{ 72,  60,  800,  480, 1,  FL_NOLINEWIDTH},		/* 0 ->   72x 60 DPI	*/
			{ 72,  60,  800,  480, 1,  FL_NOLINEWIDTH},		/* 1 ->   72x 60 DPI	*/
			{ 72, 120,  800,  960, 1,  FL_NOLINEWIDTH},		/* 2 ->   72x120 DPI	*/
			{144, 120, 1600,  960, 1,  FL_NOLINEWIDTH} };	/* 3 ->  144x120 DPI	*/
DEVPARS LQ800Parms[] = {					/* 24 pin NEC P2200XE printer */
			{180,  60, 2160,  480, 1,  FL_NOLINEWIDTH},		/* 0 ->  180x 60 DPI */
			{180,  60, 2160,  480, 1,  FL_NOLINEWIDTH},		/* 1 ->  180x 60 DPI */
			{180, 120, 2160,  960, 1,  FL_NOLINEWIDTH},		/* 2 ->  180x120 DPI	*/
			{180, 180, 2160, 1440, 1,  0},						/* 3 ->  180x180 DPI */
			{180, 360, 2160, 2880, 1,  0} };						/* 4 ->  180x360 DPI */
DEVPARS TiffParms[] = {					/* TIFF format output */
			{300, 300, 3180, 2400, 1,  0},						/* 0 -> 300x300 DPI */
			{300, 300, 3180, 2400, 1,  0},						/* 1 -> 300x300 DPI */
			{150, 150, 1590, 1200, 1,  0},						/* 2 -> 150x150 DPI */
			{100, 100, 1060,  800, 1,  FL_NOLINEWIDTH},		/* 3 -> 100x100 DPI */
			{ 75,  75,  795,  600, 1,  FL_NOLINEWIDTH},		/* 4 ->  75x 75 DPI */
			{ 72,  72,  763,  576, 1,  FL_NOLINEWIDTH} };	/* 5 ->  72x 72 DPI */

DEVICEPARMS DeviceParms[] = {
/*	------------------- icode	ndev	Parameters -------- */
							{LJET_II,	4, LaserJetParms},
							{LJET_III,	4, LaserJetParms},
							{LJET_IV,	6, LaserJet4Parms},
							{IBM4019,   4, LaserJetParms},
							{DJET,		4, DeskJetParms},
							{DJET_PLUS,	4, DeskJetParms},
							{DJET_500,	4, DeskJetParms},
							{DJET_500C,	4, DeskJetCParms},
							{DJET_550C,	4, DeskJetCParms},
							{PAINTJET,  7, PaintJetParms},
							{QUIETJET,  2, QuietJetParms},
							{MX80,      3, MX80Parms},
							{OKIDATA,   1, OkidataParms},
							{LQ500,	   3, LQ500Parms},
							{GEMINI,	   3, GeminiParms},
							{LQ800,	   4, LQ800Parms},
							{IBM_X24E,  3, LQ800Parms},
							{TIFF_B,		5, TiffParms},
							{-1,        0, NULL} };


/* ======================================================================== */
LOGICAL Laser_Driver(int key, DRVBLOCK *DriverBlock, DSP *dsp) {

	int	   ix,iy;							/* Random variables */
	VECTOR	v1;								/* Vector insertion block */
	DRVBLOCK *blk;								/* Pointer to local block */

/* -------------------------------------------------------------
   Handle first part of initialization separate.  Allocate memory 
   and start up the I/O channel process with its own junk
-------------------------------------------------------------- */
	if (key == INIFNC) {
		if ((blk = *dsp->ini.DriverBlock = malloc(sizeof(DRVBLOCK))) == NULL) return(FALSE);
	} else {
		blk = DriverBlock;						/* My local copy (so can change) */
	}


/* -------------------------------------------------------------
   Rest of module is one huge case statement;
-------------------------------------------------------------- */
	switch (key) {

/* --------------- Initialize ----------------------- */
		case INIFNC:
		{
			struct _DEVICEPARMS *devptr;
			DEVPARS  *devpar;

/* ...   Class defines format as LaserJet, DeskJet, IBM4019, ... */
			for (devptr=DeviceParms; devptr->drvtype != -1; devptr++) {
				if (devptr->drvtype == dsp->ini.Class) break;
			}
			if (devptr->drvtype == -1) return(FALSE);

			blk->Class     = dsp->ini.Class;							/* Major class */
			blk->SubDevice = LIMIT(dsp->ini.SubDevice, 0, devptr->ndev);
			blk->IO_Flag   = dsp->ini.Options;						/* I/O options */
			strcpy(blk->IO_Chan, dsp->ini.IO_Channel);			/* I/O channel */

			devpar = &devptr->devpar[blk->SubDevice];
			dsp->ini.xperinch = blk->xperinch =	devpar->xperinch;	/* Copy info */
			dsp->ini.yperinch = blk->yperinch = devpar->yperinch;
			dsp->ini.xmax     = blk->xmax     = devpar->xmax;
			dsp->ini.ymax     = blk->ymax     = devpar->ymax;
			blk->numpens      = max(1, min(dsp->ini.NumberPens, devpar->numpens));
			dsp->ini.Capabilities = DEV_CAP_GRAPHICS |		/* Supports graphs */
											DEV_CAP_MARKERS  |		/* Does some markers */
											DEV_CAP_CLIP;				/* Will get clip msgs */
			blk->IO_Flag     |= devpar->FlagOrMask;			/* Add forced options */

/* ----------------------------------------------------------------------------
-- Initialize local variables.  Linewidth must be scaled by DPI now so
-- create a lwscale which converts .001" to actual dots
---------------------------------------------------------------------------- */
			blk->lwscale    = dsp->ini.xperinch / 1000.0f;	/* 0.001" average pixels */
			if (NoLineWidth) blk->lwscale = 0;
			blk->linwidth   = max(1,nint(7*blk->lwscale));	/* Start at .007" width */
			blk->color      = 1;									/* Start with pen 1 */
			blk->xl = blk->yl = 0;								/* Clipping limits	*/
			blk->xh = blk->yh = 32000;
			blk->lunit      = 0;									/* Reset for first page */
			blk->num_vects  = 0;									/* Nothing written		*/
			blk->bufptr     = -1;								/* Nothing stored */
			break;
		}
			
/* -------------------- draw vector ----------------------------------------*/
		case LINFNC:										/* Draw line */
			v1.x  = dsp->line.x1;						/* Convert to (X,Y,dX,dY) */
			v1.y  = dsp->line.y1;
			v1.dx = dsp->line.x2 - dsp->line.x1;
			v1.dy = dsp->line.y2 - dsp->line.y1;
			invect(blk,&v1);
			break;

/* -------------------- frame --------------------------------------------- */
		case FRMFNC:								/* End of frame */
			veceof(blk);							/* Close the frame (if needed)	*/
			break;

/* -------------------- end ------------------------------------------------- */
		case ENDFNC:									/* End of plot				*/
			veceof(blk);								/* Sort and close down! */
			free(blk);									/* Free my memory usage */	
#ifdef CLOSEHANDLE
			StopSortProcess();
#endif
			break;

/* -------------------- change color -------------------- */
		case COLFNC:								/* Set color */
			blk->color = min(blk->numpens, max(0,dsp->col.brush.closest_index));
			break;

/* -------------------- point plot ----------------------------------------- */
		case PNTFNC:												/* Plot a single point */

			ix = dsp->point.x;
			iy = dsp->point.y;

			if (SingleDots) {						/* Single dot? */
				v1.x = ix;
				v1.y = iy;
				v1.dx = v1.dy = 0;
				invect(blk,&v1);
			} else {
				v1.x = max(ix-1, 1);		/* Result looks like */
				v1.y = iy;						/*         *			*/
				v1.dx = 2;						/*        ***			*/
				v1.dy = 0;						/*         *			*/
				invect(blk,&v1);					/* Horizontal 3 dots */
				v1.x += 1;
				v1.y  = max(iy-1,1);
				v1.dx = 0;
				v1.dy = 2;
				invect(blk,&v1);				/* Vertical 3 dots	*/
			}
			break;

/* -------------------- Set line linewidth --------------------  */
		case LWFNC:										/* Line width function */
			blk->linwidth = max(1, nint( blk->lwscale * dsp->lw.linewidth ));
			break;


/* -------------------- erase --------------------------------------------- */
/* -------------------- alphanumeric mode --------------------------------- */
/* -------------------- flush -------------------- implicit anmode -------- */
/* -------------------- change speed -------------------------------------- */
/* -------------------- set visibility ------------------------------------ */
		case ERSFNC:					/* Erase screen */
		case ANMFNC:					/* Exit plot mode to alphanumerics mode */
		case FLSFNC:					/* Flush all buffers */
		case SPDFNC:					/* Set pen speed */
		case VISFNC:					/* Set visibility (light, dark, complement) */
			break;

/* -------------------- Draw a symbol marker ------------------ */
		case DRAWMARKER:
			return(DrawMarker(blk, dsp->mark.isym, dsp->mark.x, dsp->mark.y, dsp->mark.size, dsp->mark.angle));

/* -------------------- Set IOCTL flags -----------------------  */
		case IOCTL:						/* Transfer of information only */
			break;

/* -------------------- Receive the current clipping ---------- */
		case TELLCLIP:						/* Just save clip limits */
			blk->xl = dsp->clip.xl;
			blk->yl = dsp->clip.yl;
			blk->xh = dsp->clip.xh;
			blk->yh = dsp->clip.yh;
			break;

		default:
			return(FALSE);
			
	}
	return(TRUE);
}


/* ============================================================================
-- Usage Guide:
--
--     DrawMarker - possibly draw one of the markers (closed one important)
--
--     Usage: LOGICAL DrawMarker(blk, isym, x,y, size, angle)
--
--     Inputs: isym  - symbol (Use logical defines MARK_SQUARE, ...)
--             x,y   - center position
--             size  - size (in device coordinates)
--             angle - angle relative to long page horizontal (0,90,180,270)
--
--     Output: TRUE  - successfully drawn
--             FALSE - not drawn - try other methods
--
-- This is really painful, especially handling the rotated stuff.  We are
-- efficient generating only dx vectors except for the rotated star which
-- generates dy vectors just because the rotation would be too painful.
============================================================================ */
PRIVATE LOGICAL DrawMarker(DRVBLOCK *blk, int isym, int x, int y, int size, int angle) {

	typedef enum _MYSYM {NONE, UP, LEFT, RIGHT, DOWN,	
								STAR,	STAR_90, STAR_180, STAR_270, 
								CIRCLE, SQUARE} MYSYM;
								
	MYSYM isp = NONE;

	static const struct {
		int sym;
		MYSYM type[4];					/* No rotate, 180, flip, 180+flip */
	} transforms[] = {
		{MARK_FILLED_TRIANGLE,			{UP,		LEFT,		DOWN,		RIGHT}},
		{MARK_FILLED_LEFTTRIANGLE,		{LEFT,	DOWN,		RIGHT,	UP}	},
		{MARK_FILLED_RIGHTTRIANGLE,	{RIGHT,	UP,		LEFT,		DOWN}	},
		{MARK_FILLED_STAR,				{STAR,	STAR_90, STAR_180,STAR_270} },
		{MARK_FILLED_SQUARE,				{SQUARE,	SQUARE, SQUARE, SQUARE}		},
		{MARK_FILLED_CIRCLE,				{CIRCLE,	CIRCLE, CIRCLE, CIRCLE}		}
	};
	#define	NUM_TYPES	6 

	int i, j, i1, i2, i3, i190, i286, hold, xa, xb, xs,xe, ys,ye,yss, dx,dy, edge;
	VECTOR	v1;								/* Vector insertion block */

	angle = (angle+45)/90;							/* Convert angle to 0...3		*/
	if (angle < 0 || angle > 3) angle = 0;

	for (i=0; i<NUM_TYPES; i++) {
		if (transforms[i].sym == isym) {
			isp = transforms[i].type[angle];
			break;
		}
	}
	if (isp == NONE) return(FALSE);

	hold = blk->linwidth;
	blk->linwidth = 1;

	switch (isp) {

		case STAR_180:
		case STAR:   /* upper pt (0.0, 0.286)  left pts (+-0.286, 0.095) */
						 /* lower pts (+-0.190, -0.238) */
			i1   =        (int) (-0.238 * size - 0.5);
			i2   = max(1, (int) ( 0.286 * size + 0.5));
			i3   =        (int) ( 0.095 * size + 0.5);		/* Cross bar */
			i190 =        (int) ( 0.190 * size + 0.5);		/* 0.190 posn */
			i286 =        (int) ( 0.286 * size + 0.5);		/* 0.286 posn */
			for (i=i1; i<=i2; i++) {
				ys = (isp == STAR) ? (y + i) : (y - i) ;
				if (ys < blk->yl || ys > blk->yh) continue;
				if (i <= i3) {											/* Below crossbar */
					xa = -(i190*(i2-i)+(i2-i1)/2) / (i2-i1);	/* Start posn */
					xb = -i190 + ((i286+i190)*(i-i1)+(i3-i1)/2) / (i3-i1);
					for (j=0; j<2; j++) {							/* Do both halves */
						xs = x + ((j == 0) ? xa : -xb ) ;
						xe = x + ((j == 0) ? xb : -xa ) ;
						if (xs <= blk->xh && xe >= blk->xl) {
							if (xs < blk->xl) xs = blk->xl;
							if (xe > blk->xh) xe = blk->xh;
							dx = xe-xs;
							v1.y  = ys;
							v1.dy = 0;
							v1.x  = xs;
							v1.dx = dx;
							invect(blk, &v1);
						}
					}
				} else {
					xa = (i190*(i2-i)+(i2-i1)/2) / (i2-i1);	/* Start posn */
					xs = x-xa;
					xe = x+xa;
					if (xs <= blk->xh && xe >= blk->xl) {
						if (xs < blk->xl) xs = blk->xl;
						if (xe > blk->xh) xe = blk->xh;
						dx = xe-xs;
						v1.y  = ys;
						v1.dy = 0;
						v1.x  = xs;
						v1.dx = dx;
						invect(blk, &v1);
					}
				}
			}
			break;
			
/* Do the rotated star as a STAR and then just put in a DY vector */
		case STAR_90:
		case STAR_270:	/* upper pt (0.0, 0.286)  left pts (+-0.286, 0.095) */
							/* lower pts (+-0.190, -0.238) */
			i1   =        (int) (-0.238 * size - 0.5);
			i2   = max(1, (int) ( 0.286 * size + 0.5));
			i3   =        (int) ( 0.095 * size + 0.5);		/* Cross bar */
			i190 =        (int) ( 0.190 * size + 0.5);		/* 0.190 posn */
			i286 =        (int) ( 0.286 * size + 0.5);		/* 0.286 posn */
			for (i=i1; i<=i2; i++) {
				ys = (isp == STAR_90) ? (x - i) : (x + i) ;
				if (ys < blk->xl || ys > blk->xh) continue;
				if (i <= i3) {											/* Below crossbar */
					xa = -(i190*(i2-i)+(i2-i1)/2) / (i2-i1);	/* Start posn */
					xb = -i190 + ((i286+i190)*(i-i1)+(i3-i1)/2) / (i3-i1);
					for (j=0; j<2; j++) {							/* Do both halves */
						xs = y + ((j == 0) ? xa : -xb ) ;
						xe = y + ((j == 0) ? xb : -xa ) ;
						if (xs <= blk->yh && xe >= blk->yl) {
							if (xs < blk->yl) xs = blk->yl;
							if (xe > blk->yh) xe = blk->yh;
							dx = xe-xs;
							v1.y  = xs;
							v1.dy = dx;
							v1.x  = ys;
							v1.dx = 0;
							invect(blk, &v1);
						}
					}
				} else {
					xa = (i190*(i2-i)+(i2-i1)/2) / (i2-i1);	/* Start posn */
					xs = y-xa;
					xe = y+xa;
					if (xs <= blk->yh && xe >= blk->yl) {
						if (xs < blk->yl) xs = blk->yl;
						if (xe > blk->yh) xe = blk->yh;
						dx = xe-xs;
						v1.y  = xs;
						v1.dy = dx;
						v1.x  = ys;
						v1.dx = 0;
						invect(blk, &v1);
					}
				}
			}
			break;

		case CIRCLE:
			edge = max(3, (int) (0.40 * size + 0.5));
			for (i=-edge/2; i<=edge/2; i++) {
				xs = x + i;
				if (xs < blk->xl || xs > blk->xh) continue;
				dy = (int) (sqrt( (edge/2)*(edge/2) - i*i) + 0.5);
				if (y-dy > blk->yh || y+dy < blk->yl) continue;
				ys = max(y-dy, blk->yl);
				dy = min(y+dy, blk->yh) - ys;
				v1.y  = ys;
				v1.dy = dy;
				v1.x  = xs;
				v1.dx = 0;
				invect(blk, &v1);
			}
			break;

		case SQUARE:
			edge  = max(3, (int) (0.380 * size + 0.5));
			dy    = edge/2;
			if (y-dy > blk->yh || y+dy < blk->yl) break;
			ys    = max(y-dy, blk->yl);
			dy    = min(y+dy, blk->yh) - ys;
			v1.dx = 0;
			v1.y  = ys;
			v1.dy = dy;
			for (i=-edge/2; i<=edge/2; i++) {
				xs = x + i;
				if (xs < blk->xl || xs > blk->xh) continue;
				v1.x  = xs;
				invect(blk, &v1);
			}
			break;

		case UP:
			edge = max(3, (int) ( 0.428 * size + 0.5));
			yss  = y  -   (int) ( 0.214 * size + 0.5);
			if (yss > blk->yh) break;				/* Bottom edge */
			ys   = max(yss, blk->yl);				/* Start of all */
			i1   =        (int) (-0.238 * size - 0.5);
			i2   = max(1, (int) ( 0.238 * size + 0.5));
			for (i=i1; i<=i2; i++) {
				xs = x + i;
				if (xs < blk->xl || xs > blk->xh) continue;
				dy = (edge*(i2-abs(i))+i2/2) / i2;			/* True length	*/
				ye = yss + dy;										/* True ending */
				if (ye < blk->yl) continue;					/* No drawing	*/
				if (ye > blk->yh) ye = blk->yh;				/* End at right */
				dy = ye - ys;
				v1.y  = ys;
				v1.dy = dy;
				v1.x  = xs;
				v1.dx = 0;
				invect(blk, &v1);
			}
			break;

		case DOWN:
			edge = max(3, (int) ( 0.428 * size + 0.5));
			yss  = y  +   (int) ( 0.214 * size + 0.5);
			if (yss < blk->yl) break;				/* Top edge */
			ys   = min(yss, blk->yh);				/* Start of all */
			i1   =        (int) (-0.238 * size - 0.5);
			i2   = max(1, (int) ( 0.238 * size + 0.5));
			for (i=i1; i<=i2; i++) {
				xs = x + i;
				if (xs < blk->xl || xs > blk->xh) continue;
				dy = (edge*(i2-abs(i))+i2/2) / i2;			/* True length	*/
				ye = yss - dy;										/* True ending */
				if (ye > blk->yh) continue;					/* No drawing	*/
				if (ye < blk->yl) ye = blk->yl;				/* End at right */
				dy = ye - ys;
				v1.y  = ys;
				v1.dy = dy;
				v1.x  = xs;
				v1.dx = 0;
				invect(blk, &v1);
			}
			break;

		case RIGHT:
			edge = max(3, (int) ( 0.476 * size + 0.5));
			i1   =        (int) (-0.214 * size - 0.5);
			i2   = max(1, (int) ( 0.214 * size + 0.5));
			for (i=i1; i<=i2; i++) {
				xs = x + i;
				if (xs < blk->xl || xs > blk->xh) continue;
				dy = (edge*(i2-i)+(i2-i1)/2) / (i2-i1) / 2;
				if (y-dy > blk->yh || y+dy < blk->yl) continue;
				ys = max(y-dy, blk->yl);
				dy = min(y+dy, blk->yh) - ys;
				v1.y  = ys;
				v1.dy = dy;
				v1.x  = xs;
				v1.dx = 0;
				invect(blk, &v1);
			}
			break;

		case LEFT:
			edge = max(3, (int) ( 0.476 * size + 0.5));
			i1   = max(1, (int) ( 0.214 * size + 0.5));
			i2   =        (int) (-0.214 * size - 0.5);
			for (i=i1; i>=i2; i--) {
				xs = x + i;
				if (xs < blk->xl || xs > blk->xh) continue;
				dy = (edge*(i2-i)+(i2-i1)/2) / (i2-i1) / 2;
				if (y-dy > blk->yh || y+dy < blk->yl) continue;
				ys = max(y-dy, blk->yl);
				dy = min(y+dy, blk->yh) - ys;
				v1.y  = ys;
				v1.dy = dy;
				v1.x  = xs;
				v1.dx = 0;
				invect(blk, &v1);
			}
			break;

		case NONE:
			break;
	}

	blk->linwidth = hold;
	return(TRUE);
}


/* ============================================================================
--     INVECT - Insert v1[] into vector block in order.  Use binary sort.
--
--     Usage:  invect(VECTOR *v1);
--
--     Inputs: none
--
--     Output: blk->vb1 - vector inserted into buffer in order of X start
--
--     Calls:  vecwrt - VB1 buffer written to file on LUNIT if full
--
-- ... Note, the X starting coordinate will actually be stored as X-LW/2 so the
--     sort is performed correctly later.
============================================================================ */
PRIVATE void invect(DRVBLOCK *blk, VECTOR *v) {
	
	INTEGER index, lower, upper, ix;
	short tmp;
	VECTOR v1;

	v1 = *v;

	if (Rotate180) {						/* Handle 180 degree rotation */
		v1.x = max(0, blk->xmax - v1.x);
		v1.y = max(0, blk->ymax - v1.y);
		v1.dx = -v1.dx;
		v1.dy = -v1.dy;
	}

	if (Rotate90) {						/* Handle 90 degree rotation */
		tmp = v1.y;
		v1.y = v1.x;
		v1.x = max(0, blk->ymax-tmp);
		tmp   = v1.dy;
		v1.dy = v1.dx;
		v1.dx = -tmp;
	}

	if (v1.dx < 0) {						/* Make positive x direction */
		v1.x += v1.dx;
		v1.y += v1.dy;
		v1.dx = -v1.dx;
		v1.dy = -v1.dy;
	}
	
	blk->num_vects++;							/* Increment # of vectors			*/
	ix = v1.x - (blk->linwidth-1)/2;		/* Insertion (and sort) X value	*/
	
/* ... Find point where this one goes in! */
	lower = 0;									/* Lower range where should go	*/
	upper = blk->bufptr;						/* Upper element where should go */
	while (lower <= upper) {
		index = (lower+upper)/2;
		if (blk->vb1[index].x > ix)		/* Is index new lower or upper? */
			upper = index-1;					/* upper point is */
		else
			lower = index+1;
	}

/* ... We've found the place, move everybody up and insert */
	index = (lower+upper+1)/2;					/* Point to insert vector */
	blk->bufptr++;									/* Bufptr points to new end */
	if (blk->bufptr > index)					/* Do we have to move elements? */
		memmove(&blk->vb1[index+1], &blk->vb1[index], (blk->bufptr-index)*sizeof(VECTOR));
		
	blk->vb1[index].x  = ix;
	blk->vb1[index].y  = v1.y;
	blk->vb1[index].dx = v1.dx;
	blk->vb1[index].dy = v1.dy;
	blk->vb1[index].fl = (blk->color<<8) | blk->linwidth;	/* Encode current values */

	if (blk->bufptr == VectorsPerRecord-1) vecwrt(blk);	/* Output if necessary */
	return;
}


/* ============================================================================
--     VECEOF - End of vector file frame - clear out and be ready for another
--
--     Usage:  CALL VECEOF()
--
--     Inputs: none
--
--     Output: Puts a EOF flag in the vector file, closes file, and starts
--             process to sort and print
============================================================================ */
PRIVATE void veceof(DRVBLOCK *blk) {

	if (blk->num_vects != 0) {										/* Have we put something in? */
		blk->bufptr++;
		blk->vb1[blk->bufptr].x        = ENDPLT;				/* Rest ignored */
		blk->vb1[VectorsPerRecord-1].x = ENDPLT;				/* Mark so easy to find !!! */
		blk->vb1[VectorsPerRecord-1].y = blk->num_vects;	/* Number of vectors total */
		vecwrt(blk);													/* Flush buffer */
		close(blk->lunit);

/* Make two tries to write to started process.  Restart in between */
		TTYputs("Requesting sort ... ");
		if (! RequestSort(blk->fname)) {							/* Try existing process */
			if (! RestartSortProcess() || ! RequestSort(blk->fname)) {
				ERRprintf("FAILED\n"
							 "ERROR: Sort failed and file will not be printed unless you manually\n"
							 "       run " SORTPROGRAM " on the file: %s\n", blk->fname);
			}
		}
		TTYputs("\n");
	}

	blk->lunit     = 0;										/* Reset for next page	*/
	blk->num_vects = 0;										/* Nothing written		*/
	blk->bufptr    = -1;										/* Nothing stored */
	return;
}

/* ============================================================================
-- Subroutine to output the current frame buffer.
--
-- Usage:  VECWRT()
--
-- Inputs: /RASTR/VB1    - Information buffer
--         /RASTR/LUNIT  - Unit to write
--
-- Output: /RASTR/BUFPTR - set back to zero
--	  VECWRT        - success of the write
============================================================================ */
PRIVATE void vecwrt(DRVBLOCK *blk) {

	char *aptr;

	if (blk->lunit == 0) {							/* Need to open new file? */
		if ( (aptr=SysTmpFilename(NULL, ".vec")) == NULL) {
			strcpy(blk->fname, "test.fil");
		} else {
			strcpy(blk->fname, aptr); 
			free(aptr);
		}
		if ( (blk->lunit=open(blk->fname,O_CREAT|O_TRUNC|O_BINARY|O_WRONLY,S_IRWXU)) == -1) 
			gen_err2("Unable to create temporary file -- ABORTING RASTER", blk->fname);
		else {
			if ( (TransferInfo = (TRANSFERINFO *) malloc(MY_BLOCK_SIZE)) != NULL) {
				TransferInfo->Version    = TRANSFERINFO_VERSION;
				strcpy(TransferInfo->IO_Chan, blk->IO_Chan);
				TransferInfo->Class      = blk->Class;
				TransferInfo->SubDevice  = blk->SubDevice;
				TransferInfo->IO_Flag    = blk->IO_Flag;
				if (Rotate90) {						/* Handle 90 degree rotation */
					TransferInfo->xperinch   = blk->yperinch;
					TransferInfo->yperinch   = blk->xperinch;
					TransferInfo->xmax       = blk->ymax;
					TransferInfo->ymax       = blk->xmax;
				} else {
					TransferInfo->xperinch   = blk->xperinch;
					TransferInfo->yperinch   = blk->yperinch;
					TransferInfo->xmax       = blk->xmax;
					TransferInfo->ymax       = blk->ymax;
				}
				TransferInfo->numpens    = blk->numpens;
				if (write(blk->lunit, (char *) TransferInfo, MY_BLOCK_SIZE) != MY_BLOCK_SIZE) {
					gen_err2("Unable to write info block -- ABORTING RASTER", blk->fname);
					close(blk->lunit); blk->lunit = -1;
				}
				free(TransferInfo);
			} else {
				gen_err("Unable to temporarily allocate memory -- ABORTING RASTER");
				close(blk->lunit); blk->lunit = -1;
			}
		}
	}

	if (blk->lunit != -1) {							/* Do we have a valid unit? */
		if (write(blk->lunit, (char *) blk->vb1, MY_BLOCK_SIZE) != MY_BLOCK_SIZE) {
			gen_warn("Error in vector write -- ABORTING! (RASTER)");
			close(blk->lunit); blk->lunit = -1;
		}
	}

	blk->bufptr = -1;									/* Reset bufptr */
	return;
}


/* ===========================================================================
-- Routines for handling external start of the sort process.  Will be
-- handled differently depending on OS.  For time being, is a spawn process.
--
-- Routines: BOOL RequestSort(char *fname);
--				 void StopSortProcess();
--				 BOOL RestartSortProcess();
=========================================================================== */
#ifndef NT
static FILE *SortProgHandle=NULL;	/* Sorting program popen handle	*/

static BOOL RequestSort(char *fname) {
	if (SortProgHandle != NULL && fprintf(SortProgHandle,"%s\n", fname) > 0) {
		fflush(SortProgHandle);
		return(TRUE);
	}
	return(FALSE);
}

static void StopSortProcess(void) {
	if (SortProgHandle != NULL) {
		fputs("QUIT\n", SortProgHandle);
		pclose(SortProgHandle);
		SortProgHandle = NULL;
	}
	return;
}

static BOOL RestartSortProcess(void) {
	char cmdline[PATH_MAX], SortProgram[PATH_MAX];

	StopSortProcess();										/* Stop if already running */

	SysResolveDyntName(SortProgram, SORTPROGRAM, sizeof(SortProgram));
	sprintf(cmdline, "%s%s%s", STARTCOMMAND, SortProgram, SORTOPTIONS);
	TTYputs("starting ... ");
	if (access(SortProgram, X_OK) != 0) {
		TTYprintf("**** FAILED **** FAILED **** FAILED ****\n"
					 "  Sorting program does not exist or is non-executable\n"
					 "  --  %s -- Reload from original distribution\n", SortProgram);
		return(FALSE);
	}
	if ( (SortProgHandle = (FILE *) popen(cmdline, "w")) == NULL) {
		TTYprintf("**** FAILED **** FAILED **** FAILED ****\n"
					 "  popen() failed to start background sort process.  \"%s\" (errno=%d)\n", cmdline, errno);
		return(FALSE);
	}
	return(TRUE);
}

#else

static HANDLE API_handle=INVALID_HANDLE_VALUE;	/* API pipe handle	*/
static int  API_unit=0;									/* C-format file descriptor of pipe */
static FILE *SortProgHandle=NULL;					/* popen() handle		*/

static BOOL RequestSort(char *fname) {
	char cmd[PATH_MAX];
	if (API_unit == 0) return(FALSE);
	sprintf(cmd, "%s\n", fname);
	if ( _write(API_unit, cmd, (int) strlen(cmd)) != (int) strlen(cmd)) return(FALSE);
	return(TRUE);
}

static void StopSortProcess(void) {
	if (API_unit != 0) {
		_write(API_unit, "QUIT\n", 5);
		DisconnectNamedPipe(API_handle);
		pclose(SortProgHandle); SortProgHandle = NULL;
		API_unit = 0; API_handle = INVALID_HANDLE_VALUE;
	}
	return;
}

static BOOL RestartSortProcess(void) {

	static int ntry=0;
	char cmdline[PATH_MAX], SortProgram[PATH_MAX], MyPipeName[PATH_MAX];

	StopSortProcess();										/* Stop if already running */

/* Step 1 - identify the program that needs to be run */
	SysResolveDyntName(SortProgram, SORTPROGRAM, sizeof(SortProgram));
	TTYputs("starting ... ");
	if (access(SortProgram, X_OK) != 0) {
		TTYprintf("FAILED.\n  Sort program (%s) does not exist or is non-executable\n", SortProgram);
		return(FALSE);
	}

/* Step 2 - create an API handle for named pipe access */
	ntry = (ntry==0) ? getpid() : ntry+1;										/* Start with process ID and increment */
	sprintf(MyPipeName,  "\\\\.\\pipe\\vs_%5.5i.NT",  ntry);
	if ( (API_handle = CreateNamedPipe(MyPipeName, 
												  PIPE_ACCESS_OUTBOUND,				/* Outbound only (NO_INHERIT?) */
												  PIPE_TYPE_BYTE | PIPE_WAIT,		/* Byte mode w/ blocking	*/
												  1,										/* Only one instance			*/
												  1024, 1024,							/* Big enough both ways		*/
												  500, 									/* 1/2 second timeout		*/
												  NULL) )								/* No security descriptor	*/
		  == INVALID_HANDLE_VALUE) {
		ERRprintf("ERROR: Unable to create vector sort pipe (%i)\n", GetLastError());
		API_unit = 0; API_handle = INVALID_HANDLE_VALUE;
		return(FALSE);
	}  else if ( (API_unit = _open_osfhandle((long) API_handle, _O_BINARY)) == -1) {
		ERRprintf("ERROR: Could not convert API_handle into C file descriptor\n");
		DisconnectNamedPipe(API_handle);	API_unit = 0; API_handle = INVALID_HANDLE_VALUE;
		return(FALSE);
	}

/* Step 3 - start the child process */
	sprintf(cmdline, "start \"vsort slave\" /BELOWNORMAL /MIN \"%s\" \"%s\"", SortProgram, MyPipeName);
	if ( (SortProgHandle = (FILE *) popen(cmdline, "w")) == NULL) {
		ERRprintf("ERROR: Command line apparently failed\n    %s\n", cmdline);
		DisconnectNamedPipe(API_handle);	API_unit = 0; API_handle = INVALID_HANDLE_VALUE;
		return(FALSE);
	}

/* Step 4 - wait for the child to connect to the pipe given */
	if (! ConnectNamedPipe(API_handle, NULL)) {
		ERRprintf("ERROR: Nobody connected to my pipe (%i)\n", GetLastError());
		DisconnectNamedPipe(API_handle);	API_unit = 0; API_handle = INVALID_HANDLE_VALUE;
		pclose(SortProgHandle); SortProgHandle = NULL;
		return(FALSE);
	}

	return(TRUE);
}

#endif
