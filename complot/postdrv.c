/* DSPTCH - Dispatch routine for all graphics functions */
/* ============================================================================
--     PostScript_Driver - COMPLOT driver for Postscript Laser Printer
--
--     Usage: LOG = PostScript_Driver(cmd, PARMS)
--
--     Inputs: cmd   - Command - See driver.ins for definitions
--             PARMS - Variable dimensioned array with parameters for transfer
--                     Type and direction depend on command.
--
--     Output: drvps - Success of operation
--
-- ... 6/9/88 - MOT
--     Modified initialization sequences to allow for inclusion into other
--     programs.  Two devices now, one portrait orientation and one landscape.
--
-- ... 7/20/88 - MOT
--     Added 2 SETMITERLIMIT to switch to bevels for lines closer than 60
--     degrees.
--
-- ... 11/1/89 - MOT
--     Added color support via RGB mode.  User can edit the prolog to change
--     the color mapping as desired.
--
-- ... 1/14/92 - MJU
--     Serious re-vamp.
--     Converted to Conforming PostScript 3.0.  Conforms to Encapsulated
--     PostScript if only *one* page is in file, but the lacks proper
--     header: %!PS-Adobe-3.0 EPSF-3.0
--     Could probably implement as another sub-device but would be hard
--     to tell if only one page were contained in file.
--
-- ... 1/16/92 - MJU/MOT
--     Fixed very subtle bug.  dvi2ps redefines 'restore' to be a macro
--     which does several things, including redefinining another symbol.
--     this crashed since i had allocated my own dictionary, and had
--     filled it up with my own defs.  solution is to do my def-ing,
--     then redefine 'def' to put stuff in the proper dictionary, then
--     return to using my dictionary.  the sequence is:
--     /def {currentdict end 3 1 roll def begin} def
--     (DVIPS SHOULD BE SHOT FOR DOING THIS!!! - MOT)
--
--     REMOVED 4/20/95 - see below.
--
-- ... 1/17/91 - MJU
--     Added 3rd subdevice for portrait mode, not shifted.  this produces
--     output which is intended to be inserted into other postscript files.
--     the plot will appear at the insertion point, and extend down and to
--     the right.  note that landscape mode can also be inserted, but
--     goes up and to the right from the insertion point.
--
-- ... 01/15/93 - MOT
--     Slightly modified code so I understand MJU's (excellent) driver.  Added
--     encapsulated postscript capability.  Devices 6-10 are encapsulated,
--	    while 1-5 are basic.  Only 1-3 and 5-7 currently set unique.
--
-- ... late 94 - MOT
--     Major rewrite with DCS conforming comments.
--
-- ... 01/10/95 - MOT
--     Rewrite of character drawing routines to use Postscript fonts
--
-- ... 4/20/95 - MOT (See comment above on 1/6/92 modification)
--     Redefinition of def to avoid overfilling my stack has other subtle
--        problems that now break with Level II to Tektronix Phaser.  This
--        crashe occurs when the top dictionary is redefined between
--        operations - the popping of the stack removes critical definitions
--        in findfont (aargh).  In Level II postscript, the stack is now 
--        allowed  to grow beyond the initial size, so the stack overfill is
--        no longer the overriding concern (although still a problem for Level
--        I printers).  For these, we will just have to assume that they don't
--        use too MUCH extra space on the stack.  Defined new symbol
--        STACK_EXTRA_SPACE as unused count on my local dictionary.
--
-- Changed handling of points.  Had a problem where a line with a very
-- high density of points would become a collection of point-draws because
-- of roundoff error.
--
-- 8/18/95 - Increased number of colors to full 15.  Modified code to be
--           consistent in use of the sizeof() instead of the parameter.
--
-- 8/8/96 - Started thinking about conditional code to append a TIFF header
--          onto encapsulated postscript output.  The necessary command line
--          for OS/2 is:
--   gsos2 -dNOPAUSE                   -- Avoid the pause on page drawing
--         -dSAFER                     -- disallow file control
--         -r72x72                     -- use points (postscript standard)
--         -sDEVICE=tiffg4             -- appropriate driver (it works)
--         -g512x476                   -- size of output (from bounding box)
--         -c -52 -143 translate       -- shift to bounding box lower left
--         -f %filename%               -- input filename (-f terminates -c)
--         -c quit                     -- to terminate the job
--
-- Added FilledRectangle to the functions
============================================================================ */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

#ifdef SLAVE
	#define PostScript_Driver SlaveDriver
#endif

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "complot.h"
#include "io_chan.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define STACK_EXTRA_SPACE	30					/* Extra space to avoid problems	*/

#define DefaultLineWidth	7					/* Default linewidth					*/
#define DPI						1200.0f			/* Pixel resolution/inch			*/
#define PPI						72.0f				/* PostScript points per inch		*/
#define MARGIN					0.25				/* x, y margins in inches			*/
#define LBUFSIZE           128				/* size of line buffer in points */

/* **************************************************************** */
/* this structure holds the parameters which implement the          */
/* rotations and translations for the various subdevices            */
/*                                                                  */
/* the orientation string describes the page orientation.  these    */
/* strings are defined by the PostScript Document Structure         */
/* Convention, and must currently (v3.0) be either "Landscape" or   */
/* "Portrait" (case sensitive).                                     */
/*                                                                  */
/* the logical page size (lxsize, lysize) is defined as             */
/*    lxsize = xxsz*pxsize + xysz*pysize                            */
/* if (pxsize, pysize) are the physical page sizes                  */
/*                                                                  */
/* the logical page shifts are                                      */
/*     lxshift = xxsh*pxsize + xysh*pysize                          */
/* **************************************************************** */
typedef enum _ORIENT {LANDSCAPE, PORTRAIT} ORIENT;

typedef struct _SUBPARMS {
	enum	  {NORMAL, EPS} type;				/* Normal PS or encapsulated	*/
	ORIENT  orient;								/* Nominal orientation			*/
	int     rotation;								/* angle of rotation				*/
	int     xshift,yshift;						/* x,y offset for margin		*/
	int     xxsh,xysh,yxsh,yysh;				/* shift parameters				*/
	int     xxsz,xysz,yxsz,yysz;				/* size parameters				*/
} SUBPARMS;

typedef struct _PROCS {					/* Procedure definitions in PROLOG */
	char *string;
} PROCS;

typedef struct _POINT {					/* Point buffering		*/
	int	x,y;
} POINT;

typedef struct _BOX {					/* Bounding boxes */
	int llx, lly, urx, ury;
} BOX;

typedef struct _PAPERSIZE {			/* Paper sizes allowed */
	double	x,y;
} PAPERSIZE;

typedef struct _FONTINFO {
	char *name;
	char *def;
	int family, cp, attrib;
} FONTINFO;

static FONTINFO FontNames[] = {
	{"Times-Roman",				"tms_rm",	DSP_FAMILY_TMSRM, DSP_CP_ASCII, DSP_ATTRIB_NORMAL},
	{"Times-Italic",				"tms_it",	DSP_FAMILY_TMSRM, DSP_CP_ASCII, DSP_ATTRIB_ITAL},
	{"Times-Bold",					"tms_bd",	DSP_FAMILY_TMSRM, DSP_CP_ASCII, DSP_ATTRIB_BOLD},
	{"Times-BoldItalic",			"tms_bdit",	DSP_FAMILY_TMSRM, DSP_CP_ASCII, DSP_ATTRIB_BOLD | DSP_ATTRIB_ITAL},
	{"Helvetica",					"helv_rm",	DSP_FAMILY_HELV,  DSP_CP_ASCII, DSP_ATTRIB_NORMAL},
	{"Helvetica-Oblique",		"helv_it",	DSP_FAMILY_HELV,  DSP_CP_ASCII, DSP_ATTRIB_ITAL},
	{"Helvetica-Bold",			"helv_bd",	DSP_FAMILY_HELV,  DSP_CP_ASCII, DSP_ATTRIB_BOLD},
	{"Helvetica-BoldOblique",	"helv_bdit",DSP_FAMILY_HELV,  DSP_CP_ASCII, DSP_ATTRIB_BOLD | DSP_ATTRIB_ITAL},
	{"Symbol",						"symbols",	DSP_FAMILY_TMSRM, DSP_CP_MATH,  DSP_ATTRIB_NORMAL}
};

#define NUMFONTS	(sizeof(FontNames) /sizeof(FONTINFO))

typedef struct _DRVBLOCK {				/* These need to quasi-static		*/
	void     *IO_Block;              /* Carry over to IO channel		*/
	LOGICAL  dirty;                  /* has page been written to yet?	*/
	LOGICAL  header;                 /* has header been written yet?	*/
	LOGICAL	running;						/* Is the code still running?		*/
	DspBrush	brush;						/* Current color information		*/
	DspBrush PageBrush;					/* Background page color			*/
	int		*palette;					/* Color palette (pens) to use	*/
	int		palette_size;				/* Number of entries in the palette */
	int	   linwid,                 /* Line width parameter				*/
				numpens,						/* Number of pens to use			*/
				ipage,                  /* Current page number				*/
				rotate,                 /* Is page rotated					*/
				page_x, page_y,         /* physical page size				*/
				xshift, yshift,         /* offsets for orientations		*/
				xmax, ymax,					/* "plot" limits incoming			*/
				lincnt;                 /* points in line buffer			*/
	int		xl,yl,xh,yh;				/* Clipping limits					*/
	int		CurrentFont;				/* Current font engaged				*/
	int		Orientation;				/* Orientation at frame command	*/
	POINT		linbuf[LBUFSIZE];			/* the buffer							*/
	BOX      page_bounds, doc_bounds;
#ifdef GNU_EXTENSIONS
	long int boundposn;					/* Position in file of EPSF bounding box text */
	BOOL		fillposnset;				/* Fill position is set */
	long int fillposn;					/* Position in file of page fill */
#else
	fpos_t	boundposn;					/* Position in file of EPSF bounding box text */
	BOOL		fillposnset;				/* Fill position is set */
	fpos_t	fillposn;					/* Position in file of page fill */
#endif
	SUBPARMS *sbp;                   /* sub device parms */
} DRVBLOCK;


/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */
LOGICAL PostScript_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp);

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static FONTINFO *FindMatchingFont(int font);
static void SetDrawColor(IO_BLOCK *io, DRVBLOCK *blk, DspBrush brush, char *start, char *end);
static void PS_header(IO_BLOCK *io, DRVBLOCK *blk);
static void PS_new_page(IO_BLOCK *io, DRVBLOCK *blk);
static void PS_end_page(IO_BLOCK *io, DRVBLOCK *blk);
static void PS_end_file(IO_BLOCK *io, DRVBLOCK *blk);
static void find_bounds(DRVBLOCK *blk, BOX *from, BOX *to);
static void buffer_point(IO_BLOCK *io, DRVBLOCK *blk, INTEGER x, INTEGER y);
static void flush_line_buffer(IO_BLOCK *io, DRVBLOCK *blk);

static PAPERSIZE PaperSize[] = {
						{8.0,		10.5},			/* 8.5x11 paper */
						{8.0,		13.5},			/* 8.5x14 paper */
						{10.5,	16.5},			/* 11x17  paper */
						{10.5,   10.5}				/* For EPS output */
};

static DspBrush DefaultBrush = {-1, -1, MY_RGB(0,0,0)};			/* Default pen */

#define	DEFAULT_PALETTE_SIZE	16
static INT32 DEFAULT_PALETTE[DEFAULT_PALETTE_SIZE] = {
						MY_RGB(255, 255, 255),		/* Pen 0  (white)			*/
						MY_RGB(  0,   0,   0),		/* Pen 1  (black)			*/
						MY_RGB(255,   0,   0),		/* Pen 2  (red)			*/
						MY_RGB(  0, 255,   0),		/* Pen 3  (green)			*/
						MY_RGB(  0,   0, 255),		/* Pen 4  (blue)			*/
						MY_RGB(255,   0, 255),		/* Pen 5  (magenta)		*/
						MY_RGB(  0, 255, 255),		/* Pen 6  (cyan)			*/
						MY_RGB(255, 255,   0),		/* Pen 7  (yellow)		*/
						MY_RGB(128,   0,   0),		/* Pen 8  (darkred)		*/
						MY_RGB(  0, 128,   0),		/* Pen 9  (darkgreen)	*/
						MY_RGB(  0,   0, 128),		/* Pen 10 (darkblue)		*/
						MY_RGB(128,   0, 128),		/* Pen 11 (darkmagenta)	*/
						MY_RGB(  0, 128, 128),		/* Pen 12 (darkcyan)		*/
						MY_RGB(128, 128,   0),		/* Pen 13 (darkyellow)	*/
						MY_RGB( 77,  77,  77),		/* Pen 14 (darkgray)		*/
						MY_RGB(153, 153, 153)		/* Pen 15 (palegray)		*/
};

static PROCS procs[] = {
	{"/B {bind def} def"},									/* Define (only) bind def */
	{"/Snap {transform .25 sub round .25 add exch .25 sub round .25 add exch itransform} B"},
	{"/m {Snap moveto} B"},									/* Move to posn	*/
	{"/l {Snap lineto} B"},									/* Draw line		*/
	{"/w {setlinewidth} B"},								/* Set line width	*/
	{"/f {fill} B"},											/* Fill object		*/
	{"/s {stroke} B"},										/* Mark line		*/
	{"/gs {gsave} B"},										/* Save graph state */
	{"/gr {grestore} B"},									/* Restore state	*/
	{"/r {setrgbcolor} B"},									/* Set RGB color	*/
	{"/n {newpath} B"},										/* Start new path	*/
	{"/cp {closepath} B"},									/* Close path		*/
	{"/c {currentlinewidth 2 div 0 360 arc f} B"},	/* Circle			*/

/*---------------------------------------------------------------------------
-- \label := <pixel_length> <string> <angle> <xstart> <ystart>
--
-- Causes string to be drawn beginning at xstart/ystart of exact pixel_length
-- Size of font set to ensure this consraint.
--
	{"/label {gs moveto currentpoint translate rotate\n"
		"\t\tdup stringwidth pop 3 -1 roll exch div dup scale\n"
		"\t\tshow gr} B"},
--------------------------------------------------------------------------- */		

/*---------------------------------------------------------------------------
-- \chr := <string> <size> <angle> <xstart> <ystart>
--
-- Causes string to be drawn at <size> pixels centered at xstart/ystart.
-- Used for single characters put at specified position.
--------------------------------------------------------------------------- */		
	{"/chr {gs moveto currentpoint translate rotate\n"
		"\t\t300 div dup scale show gr} B"},

	{"/3dchr {gs moveto currentpoint translate rotate\n"
		"\t\t300 div dup scale concat show gr} B"},

	{"/scl {3 -1 roll dup dup 5 2 roll mul exch 3 -1 roll mul exch} def"},

	{"/m_circ {n pop 0.34 mul 0 360 arc cp s} B"},

	{"/m_f_circ {n pop 0.20 mul 0 360 arc cp f} B"},

	{"/m_f_square {gs n 4 2 roll translate rotate\n"
		"\t\t-0.190  0.190 scl m -0.190 -0.190 scl l\n"
		"\t\t 0.190 -0.190 scl l  0.190  0.190 scl l f\n"
		"\t\tpop gr} B"},

	{"/m_square {gs n 4 2 roll translate rotate\n"
		"\t\t-0.238 -0.238 scl m -0.238  0.238 scl l\n"
		"\t\t 0.238  0.238 scl l  0.238 -0.238 scl l\n"
		"\t\t-0.238 -0.238 scl l s\n"
		"\t\tpop gr} B"},

	{"/m_triangle {gs n 4 2 roll translate rotate\n"
		"\t\t0.000  0.286 scl m -0.333 -0.286 scl l\n"
		"\t\t0.333 -0.286 scl l  0.000  0.286 scl l s\n"
		"\t\tpop gr} B"},

	{"/m_cross {gs n 4 2 roll translate rotate\n"
		"\t\t 0.000  0.333 scl m 0.000 -0.333 scl l s\n"
		"\t\t-0.333  0.000 scl m 0.333  0.000 scl l s\n"
		"\t\tpop gr} B"},
 
	{"/m_x {gs n 4 2 roll translate rotate\n"
		"\t\t-0.238  0.238 scl m  0.238 -0.238 scl l s\n"
		"\t\t 0.238  0.238 scl m -0.238 -0.238 scl l s\n"
		"\t\tpop gr} B"},

	{"/m_diamond {gs n 4 2 roll translate rotate\n"
		"\t\t0.000  0.476 scl m -0.286  0.000 scl l\n"
		"\t\t0.000 -0.476 scl l  0.286  0.000 scl l\n"
		"\t\t0.000  0.476 scl l s\n"
		"\t\tpop gr} B"},

	{"/m_star {gs n 4 2 roll translate rotate\n"
		"\t\t  0.000  0.429 scl m -0.095  0.143 scl l\n"
		"\t\t -0.381  0.143 scl l -0.143 -0.048 scl l\n"
		"\t\t -0.238 -0.333 scl l  0.000 -0.143 scl l\n"
		"\t\t  0.238 -0.333 scl l  0.143 -0.048 scl l\n"
		"\t\t  0.381  0.143 scl l  0.095  0.143 scl l\n"
		"\t\t  0.000  0.429 scl l s\n"
		"\t\tpop gr} B"},

	{"/m_f_triangle {gs n 4 2 roll translate rotate\n"
		"\t\t  0.000  0.214 scl m -0.238 -0.214 scl l\n"
		"\t\t  0.238 -0.214 scl l  0.000  0.214 scl l f\n"
		"\t\tpop gr} B"},

	{"/m_f_lefttriangle {gs n 4 2 roll translate rotate\n"
		"\t\t -0.214  0.000 scl m  0.214 -0.238 scl l\n"
		"\t\t  0.214  0.238 scl l -0.214  0.000 scl l f\n"
		"\t\tpop gr} B"},

	{"/m_asterisk {gs n 4 2 roll translate rotate\n"
		"\t\t  0.000  0.286 scl m  0.000 -0.286 scl l s\n"
		"\t\t -0.238  0.143 scl m  0.238 -0.143 scl l s\n"
		"\t\t  0.238  0.143 scl m -0.238 -0.143 scl l s\n"
		"\t\tpop gr} B"},

	{"/m_f_righttriangle {gs n 4 2 roll translate rotate\n"
		"\t\t  0.214  0.000 scl m -0.214  0.238 scl l\n"
		"\t\t -0.214 -0.238 scl l  0.214  0.000 scl l f\n"
		"\t\tpop gr} B"},

	{"/m_f_star {gs n 4 2 roll translate rotate\n"
		"\t\t 0.000  0.286 scl m  -0.190 -0.238 scl l\n"
		"\t\t 0.286  0.095 scl l  -0.286  0.095 scl l\n"
      "\t\t 0.190 -0.238 scl l   0.000  0.286 scl l f\n"
		"\t\tpop gr} B"},

	{"/m_davidstar {gs n 4 2 roll translate rotate\n"
		"\t\t 0.000  0.381 scl m -0.333 -0.190 scl l\n"
		"\t\t 0.333 -0.190 scl l  0.000  0.381 scl l s\n"
		"\t\t 0.000 -0.381 scl m  0.333  0.190 scl l\n"
		"\t\t-0.333  0.190 scl l  0.000 -0.381 scl l s\n"
		"\t\tpop gr} B"},

/* ===========================================================================
-- Font handling routines (painful recoding of vector for example)
=========================================================================== */
	{"/SF {findfont exch scalefont} B"},				/* Make scaled font */

/*    0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
/* 8  €      ‚   ƒ   „   …   †   ‡   ˆ   ‰   Š   ‹   Œ      Ž    */
/* 9     ‘   ’   “   ”   •   –   —   ˜   ™   š   ›   œ      ž   Ÿ */
/* A      ¡   ¢   £   ¤   ¥   ¦   §   ¨   ©   ª   «   ¬   ­   ®   ¯ */
/* B  °   ±   ²   ³   ´   µ   ¶   ·   ¸   ¹   º   »   ¼   ½   ¾   ¿ */
/* C  À   Á   Â   Ã   Ä   Å   Æ   Ç   È   É   Ê   Ë   Ì   Í   Î   Ï */
/* D  Ð   Ñ   Ò   Ó   Ô   Õ   Ö   ×   Ø   Ù   Ú   Û   Ü   Ý   Þ   ß */
/* E  O'  á   â   ã   ä   å   æ   ç   è   é   ê   ë   ì   í   î   ï */
/* F  ð   ñ   ò   ó   ô   õ   ö   ÷   ø   ù   ú   û   ü   ý   þ   ÿ */
	{"/chcp_850 [\n"
	"\t\t16#00/space         16#01/SS000000      16#02/SS010000       16#03/heart\n"
	"\t\t16#04/diamond       16#05/club          16#06/spade          16#07/bullet\n"
   "\t\t16#08/SM570001      16#09/SM750000      16#0a/SM750002       16#0b/male\n"
   "\t\t16#0c/female        16#0d/musicalnote   16#0e/musicalnotedbl 16#0f/SM690000\n"
	"\t\t16#10/SM590000      16#11/SM630000      16#12/SM760000       16#13/exclamdbl\n"
	"\t\t16#14/paragraph     16#15/section       16#16/SM700000       16#17/SM770000\n"
	"\t\t16#18/arrowup       16#19/arrowdown     16#1a/arrowright     16#1b/arrowleft\n"
	"\t\t16#1c/rightangle    16#1d/arrowboth     16#1e/SM600000       16#1f/SV040000\n"
	"\t\t16#80/Ccedilla      16#81/udieresis     16#82/eacute         16#83/acircumflex\n"
	"\t\t16#84/adieresis	    16#85/agrave        16#86/aring          16#87/ccedilla\n"
	"\t\t16#88/ecircumflex   16#89/edieresis     16#8a/egrave         16#8b/idieresis\n"
	"\t\t16#8c/icircumflex   16#8D/igrave        16#8e/Adieresis      16#8f/Aring\n"
	"\t\t16#90/Eacute        16#91/ae            16#92/AE             16#93/ocircumflex\n"
	"\t\t16#94/odieresis     16#95/ograve        16#96/ucircumflex    16#97/ugrave\n"
	"\t\t16#98/ydieresis     16#99/Odieresis     16#9a/Udieresis      16#9b/oslash\n"
	"\t\t16#9c/sterling      16#9d/Oslash        16#9e/multiply       16#9f/florin\n"
   "\t\t16#a0/aacute        16#a1/iacute        16#a2/oacute         16#a3/uacute\n"
	"\t\t16#a4/ntilde        16#a5/Ntilde        16#a6/ordfeminine    16#a7/ordmasculine\n"
	"\t\t16#a8/questiondown  16#a9/registered    16#aa/logicalnot     16#ab/onehalf\n"
	"\t\t16#ac/onequarter    16#ad/exclamdown    16#ae/guillemotleft  16#af/guillemotright\n"
	"\t\t16#b0/SF140000      16#b1/SF150000      16#b2/SF160000       16#b3/SF110000\n"
	"\t\t16#b4/SF090000      16#b5/Aacute        16#b6/Acircumflex    16#b7/Agrave\n"
   "\t\t16#b8/copyright     16#b9/SF230000      16#ba/SF240000       16#bb/SF250000\n"
	"\t\t16#bc/SF260000      16#bd/cent          16#be/yen            16#bf/SF030000\n"
	"\t\t16#c0/SF020000      16#c1/SF070000      16#c2/SF060000       16#c3/SF080000\n"
	"\t\t16#c4/SF100000      16#c5/SF050000      16#c6/atilde         16#c7/Atilde\n"
	"\t\t16#c8/SF380000      16#c9/SF390000      16#ca/SF400000       16#cb/SF410000\n"
	"\t\t16#cc/SF420000      16#cd/SF430000      16#ce/SF440000       16#cf/currency\n"
	"\t\t16#d0/eth           16#d1/Eth           16#d2/Ecircumflex    16#d3/Edieresis\n"
	"\t\t16#d4/Egrave        16#d5/dotlessi      16#d6/Iacute         16#d7/Icircumflex\n"
	"\t\t16#d8/Idieresis     16#d9/SF040000      16#da/SF010000       16#db/SF610000\n"
	"\t\t16#dc/SF570000      16#dd/brokenbar     16#de/Igrave         16#df/SF600000\n"
	"\t\t16#e0/Oacute        16#e1/germandbls    16#e2/Ocircumflex    16#e3/Ograve\n"
	"\t\t16#e4/otilde        16#e5/Otilde        16#e6/micro          16#e7/thorn\n"
	"\t\t16#e8/Thorn         16#e9/Uacute        16#eA/Ucircumflex    16#eB/Ugrave\n"
	"\t\t16#eC/yacute        16#eD/Yacute        16#ed/overline       16#ef/acute\n"
	"\t\t16#f0/hyphen        16#f1/plusminus     16#f2/underscoredbl  16#f3/threequarters\n"
	"\t\t16#f4/paragraph     16#f5/section       16#f6/divide         16#f7/cedilla\n"
	"\t\t16#f8/degree        16#f9/dieresis      16#fa/periodcentered 16#fb/onesuperior\n"
	"\t\t16#fc/threesuperior 16#fd/twosuperior   16#fe/filledbox      16#ff/.notdef\n"
	"\t] def"},

	{"/reencsmalldict 13 dict def"},

	{"/ReEncode {\n"
	"\t\treencsmalldict begin\n"
	"\t\t/newcodesandnames exch def /newfontname exch def /basefontname exch def\n"
	"\t\t/basefontdict basefontname findfont def\n"
	"\t\t/newfont basefontdict maxlength dict def\n"
	"\t\tbasefontdict \n"
	"\t\t\t{exch dup /FID ne\n"
	"\t\t\t\t{dup /Encoding eq\n"
	"\t\t\t\t\t{exch dup length array copy\n"
	"\t\t\t\t\t\tnewfont 3 1 roll put}\n"
	"\t\t\t\t\t{exch newfont 3 1 roll put}\n"
	"\t\t\t\t\tifelse\n"
	"\t\t\t\t}\n"
	"\t\t\t\t{pop pop}\n"
	"\t\t\t\tifelse\n"
	"\t\t\t} forall\n"
	"\t\tnewfont /FontName newfontname put\n"
	"\t\tnewcodesandnames aload pop\n"
	"\t\tnewcodesandnames length 2 idiv\n"
	"\t\t\t{newfont /Encoding get 3 1 roll put}\n"
	"\t\t\trepeat\n"
	"\t\tnewfontname newfont definefont pop\n"
	"\t\tend} B"}

   };

static SUBPARMS subparms[] = {
   { NORMAL, LANDSCAPE, -90, (int) (MARGIN*DPI+0.5), (int) ( MARGIN*DPI+0.5),
			0,0,0,1,		0,1,1,0 },
   { NORMAL, PORTRAIT,    0, (int) (MARGIN*DPI+0.5), (int) ( MARGIN*DPI+0.5),
			0,0,0,0,		1,0,0,1 },
   { NORMAL, LANDSCAPE, -90, (int) (MARGIN*DPI+0.5), (int) (-MARGIN*DPI-0.5),
			0,0,0,0,		0,1,1,0 },
   { NORMAL, LANDSCAPE, -90, (int) (MARGIN*DPI+0.5), (int) (-MARGIN*DPI-0.5),
			0,0,0,0,		0,1,1,0 },  /* Reserved for expansion */
   { NORMAL, LANDSCAPE, -90, (int) (MARGIN*DPI+0.5), (int) (-MARGIN*DPI-0.5),
			0,0,0,0,		0,1,1,0 },  /* Reserved for expansion */
   { EPS,    LANDSCAPE, -90, (int) (MARGIN*DPI+0.5), (int) ( MARGIN*DPI+0.5),
			0,0,0,1,		0,1,1,0 },
   { EPS,    PORTRAIT,    0, (int) (MARGIN*DPI+0.5), (int) ( MARGIN*DPI+0.5),
			0,0,0,0,		1,0,0,1 },
   { EPS,    LANDSCAPE, -90, (int) (MARGIN*DPI+0.5), (int) (-MARGIN*DPI-0.5),
			0,0,0,0,		0,1,1,0 },
   { EPS,    LANDSCAPE, -90, (int) (MARGIN*DPI+0.5), (int) (-MARGIN*DPI-0.5),
			0,0,0,0,		0,1,1,0 },
   { EPS,    LANDSCAPE, -90, (int) (MARGIN*DPI+0.5), (int) (-MARGIN*DPI-0.5),
			0,0,0,0,		0,1,1,0 },  /* Reserved for expansion */
   { EPS,    LANDSCAPE, -90, (int) (MARGIN*DPI+0.5), (int) (-MARGIN*DPI-0.5),
			0,0,0,0,		0,1,1,0 }	/* Reserved for expansion */
   };

static struct {
	int type;
	char *name;
} markers[] = {	
						{MARK_SQUARE, 					"m_square"},
						{MARK_CIRCLE, 					"m_circ"},
						{MARK_TRIANGLE,				"m_triangle"},
						{MARK_CROSS,					"m_cross"},
						{MARK_X,							"m_x"},
						{MARK_DIAMOND,					"m_diamond"},
						{MARK_STAR,						"m_star"},
						{MARK_FILLED_SQUARE, 		"m_f_square"},
						{MARK_FILLED_CIRCLE, 		"m_f_circ"},
						{MARK_FILLED_TRIANGLE,		"m_f_triangle"},
						{MARK_FILLED_LEFTTRIANGLE, "m_f_lefttriangle"},
						{MARK_ASTERISK,				"m_asterisk"},
						{MARK_FILLED_RIGHTTRIANGLE,"m_f_righttriangle"},
						{MARK_STAROFDAVID,			"m_davidstar"},
						{MARK_FILLED_STAR,			"m_f_star"},
						{-1, NULL} };

#define NUMSIZES	(sizeof(PaperSize)/sizeof(PAPERSIZE))	/* Number of sizes	*/
#define NUMPROCS	(sizeof(procs)    /sizeof(PROCS))		/* Number of procs	*/
#define NUMDEVS	(sizeof(subparms) /sizeof(SUBPARMS))	/* Number of devices */

/* ----------------------------------------------------------------------- */
LOGICAL PostScript_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp) {

   DRVBLOCK		*blk;
   IO_BLOCK		*io;
   INTEGER		i,itmp,idev,isub;
   SUBPARMS		*sbp;
	PAPERSIZE	*paper;
	int			dx,dy,dtmp,ichr;

/* -------------------------------------------------------------
   Handle first part of initialization separate.  Allocate memory 
   and start up the I/O channel process with its own junk
-------------------------------------------------------------- */
	if (key == INIFNC) {
		if ((blk = *dsp->ini.DriverBlock = malloc(sizeof(DRVBLOCK))) == NULL) return(FALSE);
		blk->IO_Block = IO_OpenChannel("POSTSCRIPT", dsp->ini.IO_Channel, IOC_BINARY, IOF_NOFLOW);
		if ( (io = blk->IO_Block) == NULL) return(FALSE);
	} else {
		blk = DriverBlock;						/* My local copy (so can change) */
		io  = blk->IO_Block;
		if ( (! blk->running) && (key != ENDFNC)) return(FALSE);
		if (IO_ferror(io) != 0) {
			ERRputs("ERROR: POSTSCRIPT output device is in error -- did the pipe break?\n"
					  "       Shutting down device.  Reinitialize plot device before continuing\n");
			blk->running = FALSE;				/* No longer running */
			return(FALSE);
		}
	}


/* -------------------------------------------------------------
   Rest of module is one huge case statement;
-------------------------------------------------------------- */
	switch (key) {

/* --------------- Initialize ----------------------- */
		case INIFNC:

			idev = max(1, min(NUMDEVS, dsp->ini.SubDevice));/* Sub device spec */
			isub = max(1, min(NUMSIZES, dsp->ini.Options));	/* Paper Index */

			sbp   = (subparms+idev-1);								/* Device parameters */
			paper = (PaperSize+isub-1);							/* Paper specification */

         dsp->ini.xperinch = (INTEGER) DPI;					/* pixels/inch */
         dsp->ini.yperinch = (INTEGER) DPI;					/* pixels/inch */

         /* set various parms based on sub-device */

         blk->sbp      = sbp;										/* Pointer to parameters */
			blk->page_x	  = (INTEGER) (DPI*paper->x + 0.5);	/* Pixels in X */
         blk->page_y   = (INTEGER) (DPI*paper->y + 0.5);	/* Pixels in Y */
         blk->rotate   = sbp->rotation;
         blk->xshift   = sbp->xshift+sbp->xxsh*(blk->page_x)+sbp->xysh*(blk->page_y);
         blk->yshift   = sbp->yshift+sbp->yxsh*(blk->page_x)+sbp->yysh*(blk->page_y);

         blk->ipage    = 0;
         blk->linwid   = DefaultLineWidth;
			blk->numpens  = max(1, dsp->ini.NumberPens);
         blk->brush    = DefaultBrush;
			blk->PageBrush = DefaultBrush;
         blk->dirty    = FALSE;
			blk->header   = FALSE;
			blk->running  = TRUE;
         blk->lincnt   = 0;
			blk->xl = blk->yl = 0;
			blk->xh = blk->yh = 32000;
			blk->CurrentFont = -1;
			blk->Orientation = 0;								/* Landscape orientation */
			
			blk->palette = DEFAULT_PALETTE;
			blk->palette_size = DEFAULT_PALETTE_SIZE;
			
			dsp->ini.xmax = blk->xmax = sbp->xxsz*(blk->page_x) + sbp->xysz*(blk->page_y);
			dsp->ini.ymax = blk->ymax = sbp->yxsz*(blk->page_x) + sbp->yysz*(blk->page_y);
			dsp->ini.Capabilities = DEV_CAP_GRAPHICS  |	/* Supports graphs	*/
											DEV_CAP_MARKERS   |	/* Can do markers    */
											DEV_CAP_FONTS	   |	/* Can do characters	*/
											DEV_CAP_3DFONTS   |  /* Can do 3D chars	*/
											DEV_CAP_GREEKFONT |	/* Can do greek		*/
											DEV_CAP_CLIP;			/* Can use clip msgs	*/

         break;

/* -------------------- palette --------------------------------------------
-- Palette command is ignored if it follows any command that creates the
-- header blocks.  Must immediately follow the INIFNC to be useful.
---------------------------------------------------------------------------- */
		case PALETTE:			
			if (! blk->header) {								/* Have we sent the header? */
				if (blk->palette != DEFAULT_PALETTE) free(blk->palette);				/* Dump if older, but not default */
				blk->palette_size = dsp->palette.num_entries;
				blk->palette = malloc(blk->palette_size*sizeof(*blk->palette));
				for (i=0; i<blk->palette_size; i++) blk->palette[i] = dsp->palette.rgb[i];
			}
			break;

/* -------------------- point plot -----------------------------------------
---------------------------------------------------------------------------- */
      case PNTFNC:											/* Plot a single point */

         if (! blk->dirty) PS_new_page(io, blk);	/* If ! dirty, new page */
         flush_line_buffer(io, blk);					/* Flush undrawn lines */

         IO_fprintf(io,"\t%d %d c\n", dsp->point.x, dsp->point.y);

         i = blk->linwid/2;						/* update page bounding boxes */
         blk->page_bounds.llx = min(blk->page_bounds.llx, dsp->point.x-i);
         blk->page_bounds.urx = max(blk->page_bounds.urx, dsp->point.x+i);
         blk->page_bounds.lly = min(blk->page_bounds.lly, dsp->point.y-i);
         blk->page_bounds.ury = max(blk->page_bounds.ury, dsp->point.y+i);

         break;

      case LINFNC:											/* Draw line */

         if (! blk->dirty) PS_new_page(io, blk);	/* mark page as dirty */
			i = blk->lincnt - 1;								/* Buffer position */

			if (i < 0) {											/* Buffer is empty */
            blk->lincnt = 0;                          /* place in buffer */
            buffer_point(io, blk, dsp->line.x1, dsp->line.y1);
            buffer_point(io, blk, dsp->line.x2, dsp->line.y2);

         } else if ( (blk->linbuf[i].x == dsp->line.x1) &&	/* Contiguous line? */
							(blk->linbuf[i].y == dsp->line.y1) ) {
				buffer_point(io, blk, dsp->line.x2, dsp->line.y2);

			} else if ( (blk->linbuf[i].x == dsp->line.x2) &&	/* Contiguous line? */
						   (blk->linbuf[i].y == dsp->line.y2) ) {
				buffer_point(io, blk, dsp->line.x1, dsp->line.y1);

			} else {															/* New line */
				flush_line_buffer(io, blk);							/* Draw old */
				buffer_point(io, blk, dsp->line.x1, dsp->line.y1);
				buffer_point(io, blk, dsp->line.x2, dsp->line.y2);
			}

         /* keep track of maximum extents.  include line widths */
         i = blk->linwid/2;
         blk->page_bounds.llx = min(blk->page_bounds.llx,
				min(dsp->line.x1-i,dsp->line.x2-i));
         blk->page_bounds.urx = max(blk->page_bounds.urx,
				max(dsp->line.x1+i,dsp->line.x2+i));
         blk->page_bounds.lly = min(blk->page_bounds.lly,
				min(dsp->line.y1-i,dsp->line.y2-i));
         blk->page_bounds.ury = max(blk->page_bounds.ury,
				max(dsp->line.y1+i,dsp->line.y2+i));
         break;

/* -------------------- erase -------------------- */
      case ERSFNC:               /* Erase screen */
			blk->PageBrush = dsp->erase.brush;
         break;                  /* Impossible   */

/* -------------------- flush -------------------- implicit anmode -------- */
      case FLSFNC:               /* Flush all buffers */
         break;

/* -------------------- frame -------------------- */
      case FRMFNC:								/* End of frame */
         if (blk->dirty) {						/* Have we written anything */
				flush_line_buffer(io, blk);	/* put everything out */
				PS_end_page(io, blk);			/* write out end of page stuff */
			}
			blk->Orientation = dsp->frame.orient;	/* Current orientation */
         break;

/* -------------------- axeslimit -------------------- */
		case AXESLIMIT:
         if (! blk->dirty) {
				PS_new_page(io, blk);					/* mark page as dirty */
			} else {
				flush_line_buffer(io, blk);
			}
			if (IS_RGB(dsp->axes.area_brush.rgb)) {			/* Area fill required? */
				SetDrawColor(io, blk, dsp->axes.area_brush, "\tgs ", " n\n");
				IO_fprintf(io, "\t\t%d %d m %d %d l %d %d l\n\t\t%d %d l %d %d l\n\t\tcp f gr\n",
							  dsp->axes.area_x1, dsp->axes.area_y1,		dsp->axes.area_x2, dsp->axes.area_y1, 
							  dsp->axes.area_x2, dsp->axes.area_y2,		dsp->axes.area_x1, dsp->axes.area_y2,
							  dsp->axes.area_x1, dsp->axes.area_y1);
				blk->page_bounds.llx = min(min(blk->page_bounds.llx, dsp->axes.area_x1), dsp->axes.area_x2);
				blk->page_bounds.urx = max(max(blk->page_bounds.urx, dsp->axes.area_x1), dsp->axes.area_x2);
				blk->page_bounds.lly = min(min(blk->page_bounds.lly, dsp->axes.area_y1), dsp->axes.area_y2);
				blk->page_bounds.ury = max(max(blk->page_bounds.ury, dsp->axes.area_y1), dsp->axes.area_y2);
			}
			if (IS_RGB(dsp->axes.axes_brush.rgb)) {
				SetDrawColor(io, blk, dsp->axes.axes_brush, "\tgs ", " n\n");
				IO_fprintf(io, "\t\t%d %d m %d %d l %d %d l\n\t\t%d %d l %d %d l\n\t\tcp f gr\n",
						  dsp->axes.axes_x1, dsp->axes.axes_y1,		dsp->axes.axes_x2, dsp->axes.axes_y1, 
						  dsp->axes.axes_x2, dsp->axes.axes_y2,		dsp->axes.axes_x1, dsp->axes.axes_y2,
						  dsp->axes.axes_x1, dsp->axes.axes_y1);
				blk->page_bounds.llx = min(min(blk->page_bounds.llx, dsp->axes.axes_x1), dsp->axes.axes_x2);
				blk->page_bounds.urx = max(max(blk->page_bounds.urx, dsp->axes.axes_x1), dsp->axes.axes_x2);
				blk->page_bounds.lly = min(min(blk->page_bounds.lly, dsp->axes.axes_y1), dsp->axes.axes_y2);
				blk->page_bounds.ury = max(max(blk->page_bounds.ury, dsp->axes.axes_y1), dsp->axes.axes_y2);
			}

			break;

/* -------------------- end -------------------- */
      case ENDFNC:								/* End of plot */

         if (blk->dirty) {
				flush_line_buffer(io, blk);	/* put everything out */
				PS_end_page(io, blk);			/* write out end of page stuff */
			}
			PS_end_file(io, blk);				/* End of File stuff */
			IO_CloseChannel(io);
			free(blk->palette);
			free(blk);								/* Free memory usage */ 
         break;

/* -------------------- change color -------------------- */
      case COLFNC:                        /* Set color */

			blk->brush = dsp->col.brush;		/* Get the color */
			if (blk->dirty) {						/* Deal with if open */
				flush_line_buffer(io, blk);					/* plot pending vecs	*/
				SetDrawColor(io, blk, blk->brush, "\t", "\n");
			}
         break;

/* -------------------- alphanumeric mode -------------------- */
      case ANMFNC:               /* Exit plot mode to alphanumerics mode */
         break;

/* -------------------- change speed -------------------- */
      case SPDFNC:               /* Set pen speed */
         break;

/* -------------------- set visibility -------------------- */
/* Note: visibility is pretty much meaningless since The PostScript Imaging
   Model defines color as opaque, so that complementing is impossible. */
   
      case VISFNC:               /* Set visibility (light, dark, complement) */
         break;

/* -------------------- read cursor -------------------- */
      case CURFNC:               /* Read cursor function */
         return(FALSE);

/* -------------------- Begin panel --------------------  */
      case PANFNC:               /* Panel function */
         return(FALSE);

/* -------------------- End panel --------------------  */
      case POFFNC:               /* Panel off function */
         return(FALSE);


/* -------------------- Set line linewidth --------------------  */
      case LWFNC:                            /* Line width function */

         blk->linwid = min(255,max(0,dsp->lw.linewidth));
         if (blk->dirty) {
				flush_line_buffer(io, blk);
				IO_fprintf(io, "\t%d w\n", blk->linwid);
			}
         break;

/* -------------------- Character output -----------------
 ... <length> (string) <angle> <x> <y> label
   chrset    - character set                       x  - x position (start)
   size      - nominal character size (pixels)     y  - y position (start)
   angle     - angle to draw
   pixel_len - length in pixels (X)
   matrix    - for 3D, transformation of x,y movements (additional CTM)
   *string   - pointer to string
 ------------------------------------------------------- */
#define	DEG_TO_RADS(x)		((x)*3.141592654/180)

      case DRAWCHAR:										/* Text drawing functions */
		case DRAW3DCHAR:

         if (! blk->dirty) PS_new_page(io, blk);
         flush_line_buffer(io, blk);					/* Flush undrawn lines */

			if (blk->CurrentFont != dsp->drwchr.font) {
				blk->CurrentFont = dsp->drwchr.font;	/* Accept this one */
				IO_fprintf(io, "\t%s setfont\n", FindMatchingFont(dsp->drwchr.font)->def);
			}

			ichr = ((int) dsp->drwchr.chr) & 0x00FF;				/* Clean way to make unsigned */
			if (isalnum(ichr) && ichr < 0x007F) {
				IO_fprintf(io, "\t(%c) ", ichr);
			} else {
				IO_fprintf(io, "\t(\\%3.3o) ", ichr);
			}

			/* in 3D mode, add the concat matrix for transformation */
			if (key == DRAW3DCHAR) IO_fprintf(io, "[ %.3f %.3f %.3f %.3f 0 0 ] ",
				  dsp->drwchr.matrix[0], dsp->drwchr.matrix[1],
				  dsp->drwchr.matrix[2], dsp->drwchr.matrix[3]);
														 
			/* Now, in RPN format, other parameters and draw command */
			itmp = dsp->drwchr.angle;
			IO_fprintf(io, "%i %i %i %i %s\n",	
						  dsp->drwchr.size, itmp, dsp->drwchr.x, dsp->drwchr.y,
						 (key == DRAWCHAR) ? "chr" : "3dchr");

			if (key == DRAWCHAR) {				/* Try to be accurate on bounding box */
				for (i=0; i<4; i++) {
					if (i == 0) {
						dx = dy = 0;
					} else if (i == 1) {
						dx = (int) (dsp->drwchr.size * cos(DEG_TO_RADS(dsp->drwchr.angle)));
						dy = (int) (dsp->drwchr.size * sin(DEG_TO_RADS(dsp->drwchr.angle)));
					} else if (i == 2) {
						dtmp = dx;
						dx = -dy;				/* -sin	*/
						dy = dtmp;				/*  cos	*/
					} else {
						dtmp = dy+dx;			/* cos-sin */
						dy   = dy-dx;			/* cos+sin */
						dx   = dtmp;
					}
					blk->page_bounds.llx = min(blk->page_bounds.llx, dsp->drwchr.x+dx);
					blk->page_bounds.urx = max(blk->page_bounds.urx, dsp->drwchr.x+dx);
					blk->page_bounds.lly = min(blk->page_bounds.lly, dsp->drwchr.y+dy);
					blk->page_bounds.ury = max(blk->page_bounds.ury, dsp->drwchr.y+dy);
				}
			} else {									/* In 3D, just be conservative */
				dx = dy = dsp->drwchr.size;
				blk->page_bounds.llx = min(blk->page_bounds.llx, dsp->drwchr.x-dx);
				blk->page_bounds.urx = max(blk->page_bounds.urx, dsp->drwchr.x+dx);
				blk->page_bounds.lly = min(blk->page_bounds.lly, dsp->drwchr.y-dy);
				blk->page_bounds.ury = max(blk->page_bounds.ury, dsp->drwchr.y+dy);
			}
				
         break;

/* -------------------- Draw a symbol marker ------------------ */
		case DRAWMARKER:
			if (! blk->dirty) PS_new_page(io, blk);
         flush_line_buffer(io, blk);					/* Flush undrawn lines */

			for (i=0; markers[i].type!=-1; i++) {
				if (markers[i].type == dsp->mark.isym) {
					LOGICAL doclip;
					flush_line_buffer(io, blk);
					doclip = dsp->mark.x-dsp->mark.size/2 < blk->xl ||
					  			dsp->mark.x+dsp->mark.size/2 > blk->xh ||
					  			dsp->mark.y-dsp->mark.size/2 < blk->yl ||
					  			dsp->mark.y+dsp->mark.size/2 > blk->yh;
					if (doclip) IO_fprintf(io,
									"\t gs %i %i m %i %i l %i %i l %i %i l cp clip\n",
									 blk->xl, blk->yl,   blk->xh, blk->yl,  blk->xh, blk->yh,
									 blk->xl, blk->yh);
					IO_fprintf(io, "\t %i %i %i %i %s\n", dsp->mark.x, dsp->mark.y,
								  dsp->mark.size, dsp->mark.angle, markers[i].name);
					if (doclip) IO_fprintf(io, "\t gr\n");

					blk->page_bounds.llx = min(blk->page_bounds.llx, dsp->mark.x-dsp->mark.size/2);
					blk->page_bounds.urx = max(blk->page_bounds.urx, dsp->mark.x+dsp->mark.size/2);
					blk->page_bounds.lly = min(blk->page_bounds.lly, dsp->mark.y-dsp->mark.size/2);
					blk->page_bounds.ury = max(blk->page_bounds.ury, dsp->mark.y+dsp->mark.size/2);

					return(TRUE);
				}
			}
			return(FALSE);

/* -------------------- Draw a symbol marker ------------------ */
		case FILLEDRECT:

         if (! blk->dirty) PS_new_page(io, blk);	/* mark page as dirty */
         flush_line_buffer(io, blk);					/* Flush undrawn lines */

			SetDrawColor(io, blk, dsp->rect.brush, "\tgs ", "\n");
			IO_fprintf(io, "\t %i %i m %i %i l %i %i l %i %i l\n\t\tcp f gr\n",
						  dsp->rect.x1, dsp->rect.y1, dsp->rect.x1, dsp->rect.y2,
						  dsp->rect.x2, dsp->rect.y2, dsp->rect.x2, dsp->rect.y1);

         blk->page_bounds.llx = min(blk->page_bounds.llx, min(dsp->rect.x1, dsp->rect.x2));
         blk->page_bounds.urx = max(blk->page_bounds.urx, max(dsp->rect.x1, dsp->rect.x2));
         blk->page_bounds.lly = min(blk->page_bounds.lly, min(dsp->rect.y1, dsp->rect.y2));
         blk->page_bounds.ury = max(blk->page_bounds.ury, max(dsp->rect.y1, dsp->rect.y2));
         break;

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

/* ===========================================================================
-- From passed font and currently defined given fonts, select best match to
-- use in describing the characters
=========================================================================== */
static FONTINFO *FindMatchingFont(int font) {

	int i, family, cp, attrib;
	FONTINFO *best=NULL;

	family = font & DSP_FAMILY_MASK;
	cp     = font & DSP_CP_MASK;
	attrib = font & DSP_ATTRIB_MASK;

/* Scan for matches */
	for (i=0; i<NUMFONTS; i++) {
		if (FontNames[i].cp != cp) continue;	/* CP must match */
		if (best == NULL) {
			best = &FontNames[i];
		} else if (best->family != family && FontNames[i].family == family) {
			best = &FontNames[i];					/* This one better now */
		} else if ((best->attrib & attrib) < (FontNames[i].attrib & attrib)) {
			best = &FontNames[i];
		}
	}
	if (best == NULL) best = FontNames;
	return(best);
}


/* ---------------------------------------------------------------------------
-- Routine to write the PostScript DCS header
--
-- Usage: PS_header(IO_BLOCK *io, DRVBLOCK *blk)
--
-- Inputs: io,blk - information blocks
--
-- Output: will write Header, Prolog and PageSetup sections of DCS.
--
-- Returns: void
--
-- Notes:	Conforms to 3.0 standard.  Will do EPSF if requested.
--------------------------------------------------------------------------- */
static void PS_header(IO_BLOCK *io, DRVBLOCK *blk) {

   int i, icol;
	time_t tod;
	ORIENT orient;

/* ... print out the header portion */
	if (blk->sbp->type == EPS) {
		IO_fputs("%!PS-Adobe-3.0 EPSF-3.0\n", io);
	} else {
		IO_fputs("%!PS-Adobe-3.0\n", io);
	}

	time(&tod);

	orient = blk->sbp->orient;					/* Determine present orientation */
	if (blk->Orientation == 90 || blk->Orientation == 270)
		orient = (orient == LANDSCAPE) ? PORTRAIT : LANDSCAPE;
	
	IO_fprintf(io, "%%%%Creator: MOTPLOT DSC Conforming PostScript Driver (2.01)\n"
                  "%%%%CreationDate: %s"
                  "%%%%For: %s\n"
                  "%%%%DocumentData: Clean7Bit\n"
                  "%%%%LanguageLevel: 1\n"
                  "%%%%Orientation: %s\n",
		asctime(localtime(&tod)), getlogin(), 
		( (orient == LANDSCAPE) ? "Landscape" : "Portrait") );

	if (blk->sbp->type == EPS) {
#ifdef GNU_EXTENSIONS
	   blk->boundposn = ftell(io->funit);
#else
		IO_fgetpos(io, &blk->boundposn);
#endif
		IO_fprintf(io, "%%%%BoundingBox: %6.5i %6.5i %6.5i %6.5i\n", 0,0,360,360);
		IO_fputs("%%Pages: 1\n", io);
	} else {
		IO_fprintf(io,	"%%%%BoundingBox: (atend)\n"
							"%%%%Pages: (atend)\n"
							"%%%%PageOrder: Ascend\n");
	}

#if 0
	IO_fputs("%%DocumentNeededResources: font", io);
	for (i=0, ilen=31; i<NUMFONTS; i++) {
		if (ilen + 1 + strlen(FontNames[i].name) >= 79) {
			IO_fputs("\n%%+", io);
			ilen = 5;
		}
		IO_fprintf(io, " %s", FontNames[i].name);
		ilen += 1 + strlen(FontNames[i].name);
	}
	IO_fputs("\n", io);
#endif

	IO_fputs("%%DocumentSuppliedResources: procset GENPLOT 1.0 0\n", io);
	IO_fputs("%%EndComments\n", io);

/* ------------------------------
-- First comes prolog section.  Supposed to be only /def's, but have to put
-- the dictionary define here also, or it doesn't make sense to make one.
--
-- We make our own dictionary.  this is required since we don't know
-- how much room is left in the current one.  it also aids in
-- restoring the state of the stacks at the end
--------------------------------- */

   IO_fputs("%%BeginProlog\n", io);

   IO_fprintf(io, "%d dict begin\n", NUMPROCS+blk->palette_size+2*NUMFONTS+STACK_EXTRA_SPACE);

	IO_fputs("%%BeginResource: procset GENPLOT 1.0 0\n", io);
	IO_fputs("% Copyright (c) 1995 Computer Graphic Service, Ltd.\n", io);
	for (i=0; i<NUMPROCS; i++) {
		IO_fprintf(io, "\t%s\n", procs[i].string);
	}
   for (i=0; i<blk->palette_size; i++) {
		icol = blk->palette[i];
      IO_fprintf(io, "\t/p%1.1i {%5.3f %5.3f %5.3f r} B\n",
                 i, R_FROM_RGB(icol)/255.0, G_FROM_RGB(icol)/255.0, B_FROM_RGB(icol)/255.0);
	}
	for (i=0; i<NUMFONTS; i++) {
		if (FontNames[i].cp != DSP_CP_ASCII) {
			IO_fprintf(io, "\t/%s {300 /%s SF} B\n", FontNames[i].def, FontNames[i].name);
		} else {
			IO_fprintf(io, "\t/%s_c {0} def\n"
							"\t/%s {%s_c 0 eq {/%s /%s_ chcp_850 ReEncode /%s_c {1} def} if\n"
							"\t\t300 /%s_ SF} B\n",
			FontNames[i].def, FontNames[i].def, FontNames[i].def, FontNames[i].name,
			FontNames[i].def, FontNames[i].def, FontNames[i].def);
		}
	}

/* Removed as fatal bug 20/Apr/95 - MOT */
#if 0
	IO_fputs("\t/def {currentdict end 3 1 roll def begin} def\n", io);	/* MUST BE LAST ONE */
#endif

	IO_fputs("%%EndResource\n", io);
   IO_fputs("%%EndProlog\n", io);

/* execute commands to set graphics state to default */
	IO_fputs("%%BeginSetup\n", io);
#if 0
	IO_fputs("%%IncludeResource: font", io);
	for (i=0, ilen=23; i<NUMFONTS; i++) {
		if (ilen + 1 + strlen(FontNames[i].name) >= 79) {
			IO_fputs("\n%%+", io);
			ilen = 5;
		}
		IO_fprintf(io, " %s", FontNames[i].name);
		ilen += 1 + strlen(FontNames[i].name);
	}
	IO_fputs("\n", io);
#endif

	IO_fprintf(io,"\t/documentlevel save def\n"			/* Save initial state */
					  "\t0 setgray\n"								/* 100% dark			 */
					  "\t[] 0 setdash\n"							/* solid lines			 */
					  "\tnewpath\n"								/* start new path		 */
					  "\t2 setlinecap\n"							/* square end lines	 */
					  "\t0 setlinejoin\n"						/* mitered joints		 */
					  "\t2 setmiterlimit\n"						/* >=60 degree miters */
					  "\t%f %f scale\n"							/* resize				 */
					  "\t%d %d translate\n"						/* move					 */
					  "\t%d rotate\n",							/* rotate				 */
		PPI/DPI, PPI/DPI, blk->xshift, blk->yshift, blk->rotate);
	if (blk->Orientation == 90 || blk->Orientation == 270) {		/* Add 180 */
		IO_fprintf(io, "\t%d %d translate\n"				/* 1 page shift		 */
							"\t180 rotate\n",						/* Inversion			 */
			blk->xmax, blk->ymax);
	}
	IO_fputs("%%EndSetup\n", io);

/* reset document bounds */
   blk->doc_bounds.llx = blk->doc_bounds.lly = 32000;
   blk->doc_bounds.urx = blk->doc_bounds.ury = 0;

	blk->header = TRUE;
	return;
}

/* ---------------------------------------------------------------------------
-- Routine to initialize a new page 
--
-- Usage:  PS_new_page(IO_BLOCK *io, DRVBLOCK *blk)
--
-- Inputs: io,blk - information blocks
--
-- Output: Writes DCS Page Comments and initializes graphics state
--
-- Returns: void
--
-- Notes:  Saves page state for restore by PS_start_page();
--------------------------------------------------------------------------- */
static void PS_new_page(IO_BLOCK *io, DRVBLOCK *blk) {

	if (! blk->header) PS_header(io, blk);		/* Initialize now if not before */

	if ( (blk->sbp->type == EPS) && (blk->ipage >= 1) ) {
		ERRprintf("ERROR: Encapsulated Postscript Files must contain exactly 1 page.\n"
			       "       All new graphics (vectors, pen changes) will be ignored by the driver.\n"
					 "       Reopen the device (DEVICE command) to replace existing graph\n");
		blk->running = FALSE;
		return;
	}

	blk->ipage++;										/* increment page count			*/
	IO_fprintf(io, "%%%%Page: %d %d\n"			/* page number						*/
						"%%%%PageBoundingBox: (atend)\n"
						"/pagelevel save def\n"		/* save-restore surround page */
						"\t%i w\n",						/* set line width					*/
						blk->ipage, blk->ipage, blk->linwid);
	SetDrawColor(io, blk, blk->brush, "\t", "\n");

/* Fill the page, flood filling if appropriate, saving position as well */
	blk->fillposnset = FALSE;						/* Assume we will ignore		*/
	if (IS_RGB(blk->PageBrush.rgb)) {
		SetDrawColor(io, blk, blk->PageBrush, "\tgs ", " n\n");
		if (blk->sbp->type == EPS) {
#ifdef GNU_EXTENSIONS
			blk->fillposn = ftell(io->funit);
#else
			IO_fgetpos(io, &blk->fillposn);
#endif
			blk->fillposnset = TRUE;				/* Assume we will ignore		*/
		}
		IO_fprintf(io,  "\t\t%6.5d %6.5d m %6.5d %6.5d l\n\t\t%6.5d %6.5d l %6.5d %6.5d l %6.5d %6.5d l\n\t\tcp f gr\n",
					  0,0, 32000,0, 32000,32000, 0,32000, 0,0);
	}

/* ... Reset page bounds ... */
	blk->page_bounds.llx = blk->page_bounds.lly = 32000;
	blk->page_bounds.urx = blk->page_bounds.ury = 0;

	blk->dirty = TRUE;
	return;
}

/* ---------------------------------------------------------------------------
-- Routine to terminate a graphics page and fill in page trailer
--
-- Usage: PS_end_page(IO_BLOCK *io, DRVBLOCK *blk)
--
-- Inputs: io,blk - information blocks
--
-- Output: Write trailer, with page bounding box specification.
--
-- Returns: void
--
-- Notes:	Restores page state as saved by PS_start_page();
--------------------------------------------------------------------------- */
static void PS_end_page(IO_BLOCK *io, DRVBLOCK *blk) {

	int i;
   BOX    bounds;
   BOX    *page, *doc;

   if (! blk->dirty) return;

   page = &(blk->page_bounds);
   doc  = &(blk->doc_bounds);

/* If we have a page color, add a .2" margin to reported size */
	if (IS_RGB(blk->PageBrush.rgb)) {			/* True if index or RGB */
		i = (int) (DPI/5);							/* 0.2" margins */
		page->llx -= i;	page->lly -= i;
		page->urx += i;	page->ury += i;
	}

   find_bounds(blk, page, &bounds);

   IO_fprintf(io, "showpage\n"
                  "pagelevel restore\n"
                  "%%%%PageTrailer\n"
                  "%%%%PageBoundingBox: %i %i %i %i\n",
      bounds.llx, bounds.lly, bounds.urx, bounds.ury);

   /* update document bounds */

   doc->llx = min(doc->llx,page->llx);
   doc->lly = min(doc->lly,page->lly);
   doc->urx = max(doc->urx,page->urx);
   doc->ury = max(doc->ury,page->ury);

   blk->dirty = FALSE;
	return;
}

/* ---------------------------------------------------------------------------
-- Routine to terminate a graphics file and fill in the trailer
--
-- Usage: PS_end_file(IO_BLOCK *io, DRVBLOCK *blk)
--
-- Inputs: io,blk - information blocks
--
-- Output: Write trailer, with bounding box specification.
--
-- Returns: void
--
-- Notes:	Pops dictionary off the stack
--------------------------------------------------------------------------- */
static void PS_end_file(IO_BLOCK *io, DRVBLOCK *blk) {

   BOX   bounds;

	if (! blk->header) return;			/* If not initialized, do nothing! */

	if (blk->sbp->type == EPS && blk->fillposnset) {
#ifdef GNU_EXTENSIONS
		if (IO_fseek(io, blk->fillposn, SEEK_SET) == 0) {
#else
		if (IO_fsetpos(io, &blk->fillposn) == 0) {
#endif
			IO_fprintf(io, "\n\t\t%6.5d %6.5d m %6.5d %6.5d l\n\t\t%6.5d %6.5d l %6.5d %6.5d l %6.5d %6.5d l\n\t\tcp f gr\n",
						  blk->doc_bounds.llx, blk->doc_bounds.lly, 
						  blk->doc_bounds.urx, blk->doc_bounds.lly, 
						  blk->doc_bounds.urx, blk->doc_bounds.ury, 
						  blk->doc_bounds.llx, blk->doc_bounds.ury, 
						  blk->doc_bounds.llx, blk->doc_bounds.lly);
			IO_fseek(io, 0L, SEEK_END);
		}
	}

/* Reset the bounds now if appropriate */
   if (blk->ipage > 0) {
      find_bounds(blk, &(blk->doc_bounds), &bounds);
	} else {
      bounds.llx = bounds.lly = bounds.urx = bounds.ury = 0;
	}

/* Print trailer information, specifically removing my dictionary */
/* Give deferred info, Pages and BoundingBox in particular */

	IO_fputs("%%Trailer\n"
				"documentlevel restore\n"
				"end\n", io);
	if (blk->sbp->type != EPS) {
		IO_fprintf(io, "%%%%Pages: %i\n" 
							"%%%%BoundingBox: %i %i %i %i\n",
			blk->ipage, bounds.llx, bounds.lly, bounds.urx, bounds.ury);
		IO_fputs("%%EOF\n", io);
	}

/* If EPSF format, have to go back and fill in the bounding box info */
	if (blk->sbp->type == EPS) {
#ifdef GNU_EXTENSIONS
		if (IO_fseek(io, blk->boundposn, SEEK_SET) == 0) {
#else
		if (IO_fsetpos(io, &blk->boundposn) == 0) {
#endif
			IO_fprintf(io, "%%%%BoundingBox: %6.5i %6.5i %6.5i %6.5i\n",
				bounds.llx, bounds.lly, bounds.urx, bounds.ury);
			IO_fseek(io, 0L, SEEK_END);
		}
	}
	return;
}

/* ---------------------------------------------------------------------------
-- Routine to take user coordinate (DPI with rotation) and determine the
-- default postscript coordinates.  
--
-- Usage: find_bounds(DRVBLOCK *blk, BOX *from, BOX *to)
--
-- Inputs: blk  - information blocks
--         from - address of user space bounding box
--
-- Output: to   - address of destination bounding box
--
-- Returns: void
--
-- Notes:	The bounding box must be specified in the original coordinates.
--				The offset of 250 is accounted for in IXMAX,IYMAX w/ another 1/4"
--				on top.  The rotation is the hard part!
--------------------------------------------------------------------------- */
static void find_bounds(DRVBLOCK *blk, BOX *from, BOX *to) {

	int llx, lly, urx, ury;

   to->llx = to->urx = blk->xshift;				/* account for offsets first */
   to->lly = to->ury = blk->yshift;

/* Deal with the arbitrary 180 degree rotation on "portrait mode" */
	if (blk->Orientation == 90 || blk->Orientation == 270) {		/* Add 180 */
		urx = blk->xmax - from->llx;		/* These are inverted via translate */
		llx = blk->xmax - from->urx;
		ury = blk->ymax - from->lly;
		lly = blk->ymax - from->ury;
	} else {
		llx = from->llx;
		urx = from->urx;
		lly = from->lly;
		ury = from->ury;
	}

	if (blk->rotate == -90) {				/* Rotation effects    */
		to->llx += lly;
		to->urx += ury;
		to->lly -= urx;
		to->ury -= llx;
	} else {
		to->llx += llx;
		to->lly += lly;
		to->urx += urx;
		to->ury += ury;
	}

	/* see if bounds make sense */
	if (to->llx > to->urx) to->llx = to->urx = 0;
	if (to->lly > to->ury) to->lly = to->ury = 0;

	/* put margins on all sides, translate to native units */

	to->llx = (int) ((PPI/DPI) * (to->llx) - MARGIN*PPI); 
	to->lly = (int) ((PPI/DPI) * (to->lly) - MARGIN*PPI);
	to->urx = (int) ((PPI/DPI) * (to->urx) + MARGIN*PPI);
	to->ury = (int) ((PPI/DPI) * (to->ury) + MARGIN*PPI);

	return;
}

/* ---------------------------------------------------------------------------
-- Routine to buffer line drawing points rather than doing I/O every step.
--
-- Usage: buffer_point(IO_BLOCK *io, DRVBLOCK *blk, INTEGER x, INTEGER y)
--
-- Inputs: x,y    - point to add to end of current line
--         io,blk - information blocks
--
-- Output: May do I/O if buffer is full.
--
-- Returns: void
--
-- Notes:
--------------------------------------------------------------------------- */
static void buffer_point(IO_BLOCK *io, DRVBLOCK *blk, INTEGER x, INTEGER y) {

	int oldcnt;

   if (blk->lincnt < 0) blk->lincnt = 0;

	if (blk->lincnt == LBUFSIZE) {						/* buffer full */
		oldcnt = blk->lincnt;
		flush_line_buffer(io, blk);
		if (oldcnt > 2) {
			blk->linbuf[0] = blk->linbuf[oldcnt-2];
			blk->linbuf[1] = blk->linbuf[oldcnt-1];
			blk->lincnt = 2;
		}
   }

	blk->linbuf[blk->lincnt].x = x;
	blk->linbuf[blk->lincnt].y = y;
	blk->lincnt++;
	return;
}

/* ---------------------------------------------------------------------------
-- Routine to flush the line buffer before some operation would change 
-- graphics state.
--
-- Does a moveto first point, then lineto the rest of the coordinates.  
-- Finally strokes the line.
--------------------------------------------------------------------------- */
static void flush_line_buffer(IO_BLOCK *io, DRVBLOCK *blk) {

	int i, ido=0;

	if (blk->lincnt > 0) {
		IO_fprintf(io, "\t%d %d m\n", blk->linbuf[0].x, blk->linbuf[0].y);
		for (i=1; i<blk->lincnt; i++) {
			if (blk->linbuf[i].x != blk->linbuf[i-1].x || blk->linbuf[i].y != blk->linbuf[i-1].y) {
				IO_fprintf(io, "\t%d %d l\n", blk->linbuf[i].x, blk->linbuf[i].y);
				ido++;
			}
		}
		if (ido == 0) {							/* No real line!! */
			IO_fprintf(io,"\t%d %d c\n", blk->linbuf[0].x, blk->linbuf[0].y);
		} else {										/* Real line, stroke */
			IO_fputs("\ts\n", io);
		}
	}

	blk->lincnt = 0;
	return;
}


/* ===========================================================================
-- Routine to set the color by either a pen number (if match) or by setting
-- the actual RGB values
=========================================================================== */
static void SetDrawColor(IO_BLOCK *io, DRVBLOCK *blk, DspBrush brush, char *start, char *end) {

	int index;
	
	if (start != NULL) IO_fprintf(io, start);

/* Index values => use pen.  But don't allow to go beyond that specified by
-- the user earlier.  Independent of the palette.
-- Otherwise, RGB value.  Transparent is tough */
	if (brush.index >= 0) {
		index = brush.index;
		if (index > blk->palette_size) index = blk->palette_size-1;
		if (index > blk->numpens)      index = blk->numpens;
		IO_fprintf(io, "p%i", index);
	} else if (IS_RGB(brush.rgb)) {
		IO_fprintf(io, "%5.3f %5.3f %5.3f r", R_FROM_RGB(brush.rgb)/255.0, G_FROM_RGB(brush.rgb)/255.0, B_FROM_RGB(brush.rgb)/255.0);
	} else if (IS_TRANSPARENT(brush.rgb)) {
		IO_fprintf(io, "p0");
	}

	if (end   != NULL) IO_fprintf(io, end);
}
