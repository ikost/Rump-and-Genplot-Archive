/* Common definitions needed for COMPLOT creating */

#ifndef _INC_COMPLOT
#define _INC_COMPLOT

/* ---------------------------------------------------------------------------
-- Device capabilities
--
-- DEV_CAP_GRAPHICS - driver supports graphics drawing commands
-- DEV_CAP_TABLET   - driver supports tablet position commands
-- DEV_CAP_CURSOR   - driver supports some/all cursor calls
-- DEV_CAP_MARKERS  - set if driver has ability to draw some of the
--                    marker symbols.  FALSE return on undrawable ones.
-- DEV_CAP_FONTS    - set if driver can draw the simple code page character
--                    itself without program intervention.  FALSE return
--                    on undrawable ones.
-- DEV_CAP_SYMBOLS  - set if driver can draw the special symbols (map 0).
--                    FALSE return on undrawable ones.
-- DEV_CAP_CLIP	  - accepts hardware clipping requests
--------------------------------------------------------------------------- */
#define	DEV_CAP_GRAPHICS	0x01	/* Device supports graphics display calls */
#define	DEV_CAP_TABLET		0x02	/* Device supports digitizer tablet calls */
#define	DEV_CAP_CURSOR		0x04	/* Device supports cursor calls */
#define	DEV_CAP_MARKERS	0x08	/* Can do most markers (point symbols) in driver */
#define	DEV_CAP_FONTS		0x10	/* Can do PS Helv/TmsRmn text fonts in driver */
#define	DEV_CAP_GREEKFONT	0x20	/* Can do PS SymbolSet math/greek fonts in driver */
#define	DEV_CAP_SYMBOLS	0x40
#define	DEV_CAP_CLIP		0x80
#define	DEV_CAP_3DFONTS	0x100	/* Can do skewed fonts in 3D drawing mode */

/* ---------------------------------------------------
  Commands through DSPTCH to display and plot drivers
 ----------------------------------------------------- */
#define	INFFNC			0		/* Return information on device	*/
#define	INIFNC			1		/* Initialize							*/
#define	LINFNC			2		/* Draw line							*/
#define	ERSFNC			3		/* Erase screen						*/
#define	FLSFNC			4		/* Flush all buffers					*/
#define	FRMFNC			5		/* End of frame						*/
#define	ENDFNC			6		/* End of plot							*/
#define	COLFNC			7		/* Set color							*/
#define	ANMFNC			8		/* Exit plot mode to alphanumerics mode */
#define	SPDFNC			9		/* Set pen speed						*/
#define	VISFNC			10		/* Set visibility (light, dark, complement) */
#define	CURFNC			11		/* Read cursor function				*/
#define	PNTFNC			12		/* Plot a single point				*/
#define	PANFNC			13		/* Panel function						*/
#define	POFFNC			14		/* Panel off function				*/
#define	LWFNC				15		/* Line width function				*/
#define	GRPFNC			16		/* Graphics screen dump function	*/
#define	PAGFNC			17		/* Select page function				*/
#define	REGFNC			18		/* Region erase function			*/
#define	FILFNC			19		/* Region fill function				*/
#define	TXTFNC			21		/* Text drawing functions			*/
#define	IOCTL				22		/* Transfer of information only	*/
#define	CURTRK			23		/* Tracking cursor					*/
#define	CURBOX			24		/* Box cursor							*/
#define	DRAWCHAR			25		/* Draw single character			*/
#define	DRAW3DCHAR		26		/* Draw tilt/skewed character		*/
#define	DRAWMARKER		27		/* Draw marker at position			*/
#define	TELLCLIP			28		/* Tell driver about clip bound	*/
#define	BEGINPATH		30		/* Begin a path definition			*/
#define	ENDPATH			31		/* End a path definition			*/
#define	FILLPATH			32		/* Fill previously defined path	*/ 
#define	STROKEPATH		33		/* Stroke previously defined path */
#define	AXESLIMIT		34		/* Tell driver axes region			*/
#define	FILLEDRECT		35		/* Draw a filled rectangle			*/
#define	PALETTE			36		/* Send the user desired palette	*/
#define	ABORTFNC			50		/* Abort an operation				*/

#define	TABINIFNC		51		/* Tablet initialization			*/
#define	TABENDFNC		52		/* Tablet close down					*/
#define	TABCLEARMODE	53		/* Clear input mode/empty buffer	*/
#define	TABGETPOSN		54		/*	Report current position			*/
#define	TABGETPOINT		55		/* Get a single point				*/
#define	TABGETSWITCH	56		/* Get a switched stream point	*/
#define	TABGETSTREAM	57		/* Get a stream point				*/

/* ----------------------------
   SAVE (HCOPY) command codes
 ------------------------------ */
#define	SV_INIT					1				/* Initialize */
#define	SV_PLOT					2				/* Plot line segment */
#define	SV_PLOTDOT				3				/* Plot dot */
#define	SV_FLUSH					4				/* Flush */
#define	SV_FACTOR				5				/* Factor */
#define	SV_COLOR					6				/* Color */
#define	SV_DASHES				7				/* Dashes */
#define	SV_USRMOD				8				/* Usrmod */
#define	SV_SPEED					9				/* Speed */
#define	SV_OFFSET				10				/* Offset (coordinates values) */
#define	SV_SET					11				/* Set */
#define	SV_ORIGIN				13				/* Origin */
#define	SV_MARGIN				14				/* Margin */
#define	SV_SIZE					15				/* Size */
#define	SV_ERASE					16				/* Erase */
#define	SV_NEWPAGE				17          /* Select a new page */
#define	SV_PANELON				18				/* Panel on */
#define	SV_PANELOFF				19				/* Panel off */
#define	SV_LINESTYLE			20				/* Line style */
#define	SV_VISIBLE				21				/* Visibility */
#define	SV_REGERASE				22				/* Region erase */
#define	SV_FILL					23				/* Fill */
#define	SV_FLIPXY				24				/* Flip XY */
#define	SV_CLIP					25				/* New clip routine */
#define	SV_SYMBOL				26				/* Symbol routine */
#define	SV_PLOT3D				27				/* 3-D plot request */
#define	SV_MARK					29				/* Draw a single symbol at point */
#define	SV_SET3DMODE			30				/* Set 3D mode active/inactive */
#define	SV_SET3DVIEW			31				/* Set 3D view */
#define	SV_SET3DRANGE			32				/* Set 3D range */
#define	SV_SET3DTRANSFORM		33				/* Set arb. additional transform */
#define	SV_RESET3DTRANSFORM	34				/* Reset additional transform */
#define	SV_3DMULTIPLY			35				/* pre/post multiply array		*/
#define	SV_ORIENT				36				/* Orient the world				*/
#define	SV_BEGINPATH			37				/* Begin a path definition		*/
#define	SV_ENDPATH				38				/* End a path definition		*/
#define	SV_FILLPATH				39				/* Fill a defined path			*/
#define	SV_STROKEPATH			40				/* Stroke a defined path		*/
#define	SV_AXESREGION			41				/* Tell driver axes region		*/
#define	SV_FILLRECTANGLE		42				/* Draw a filled rectangle		*/
#define	SV_FRAME					43				/* Frame (obsolete)				*/
#define	SV_CLIPSYMBOLS			44				/* Clipsymbols						*/

#define	SV_NULL					99				/* NULL (end) */

#define SVERR_BADKEY				0x0001		/* Invalid key to save routines	*/
#define SVERR_NOUNIT				0x0002		/* No unit currently define		*/
#define SVERR_BADREAD			0x0003		/* Bad read from disk				*/

#define SVKEY_MAX					0x0400		/* Maximum value it can have */
#define SVKEY_READ_COMMON		0x0200		/* Read common block */
#define SVKEY_FLUSH				0x0100		/* Flush buffers */
#define SVKEY_CLOSE				0x0080		/* Close unit */
#define SVKEY_CLEAR				0x0040		/* Clear unit */
#define SVKEY_SET_UNIT			0x0020		/* Set unit in place */
#define SVKEY_RESET				0x0010		/* Reset everything */
#define SVKEY_GET_END			0x0008		/* Go to end of the unit */
#define SVKEY_ON					0x0004		/* Turn it on */
#define SVKEY_WRITE_COMMON		0x0002		/* Write common block */
#define SVKEY_OFF					0x0001		/* Turn it off */
#define SVKEY_INITIALIZE		(SVKEY_SET_UNIT | SVKEY_RESET   | SVKEY_ON | SVKEY_WRITE_COMMON)
#define SVKEY_APPEND				(SVKEY_SET_UNIT | SVKEY_GET_END | SVKEY_ON | SVKEY_WRITE_COMMON)
#define SVKEY_SAVE				(SVKEY_FLUSH | SVKEY_CLOSE)
#define SVKEY_ERASE				(SVKEY_RESET | SVKEY_WRITE_COMMON)

#define COMPLOT_VERSION 6			/* Current revision */

/* ===========================================================================
   Common typedef's for COMPLOT device transfers

	By convention, so pipes work okay, if there is a (CHAR *) element in the
	structure, it must be the first element.  Multiple (CHAR *) are not
	supported except for the DspInifnc which is handled separately.

	Entry types:
		info   - intended as information to driver only.  Should not be changed.
		modify - initial value passed to driver.  Driver may modify as desired.
		query  - no initial or default value set.  Driver expected to set.
=========================================================================== */
typedef struct _dsp_inifnc {
	CHAR   *IO_Channel;			/* Pointer to the IO channel string	(info)	*/
	void  **DriverBlock;			/* Pointer to Driver parm block		(modify) */
	CHAR	 *Driver;				/* Requested driver (for pipes)		(info)	*/
	INT	  Class;					/* Requested major class				(info)	*/
	INT	  SubDevice;			/* Requested sub-device in class		(info)	*/
	INT	  Options;				/* Requested options						(info)	*/

	INT	  NumberPens;			/* Requested # of pens					(modify) */
	INT	  xperinch,yperinch;	/* Returned x/y resolution				(query)	*/
	INT	  xmax,ymax;			/* Returned max x/y pixels				(query)	*/
	INT	  Capabilities;		/* Device capabilities					(query)	*/
#if (defined OS2)					/* Handle of parent						(info)	*/
	ULONG   hwnd_Parent;			/* Active Window (LHANDLE in OS/2)				*/
	ULONG	  hwnd_Focus;			/* Focus Window (LHANDLE in OS/2)				*/
#elif (defined NT)
	void	 *hwnd_Parent;			/* Active Window (PVOID in NT)					*/
	void	 *hwnd_Focus;			/* Focus Window (PVOID in NT)						*/
	void	 *hwnd_Graph;			/* Graph window return (PVOID in NT)			*/
	char	 Window_Title[32];	/* Title that should be displayed in window	*/
	char	 Active_Mutex[32];	/* Name of mutex semaphore which marks program end */
#endif
} DspInifnc;

typedef struct _dsp_brush {	/* Used as internal component of several	*/
	int index;						/* But not as itself anywhere					*/
	int closest_index;
	INT32 rgb;
} DspBrush;

typedef struct _dsp_linfnc {
	INT x1;
	INT y1;
	INT x2;
	INT y2;
} DspLinfnc;

typedef struct _dsp_ersfnc {
	DspBrush brush;
} DspErsfnc;

typedef struct _dsp_frmfnc {
	INT orient;
} DspFrmfnc;

typedef struct _dsp_colfnc {
	INT32 pen;							/* Request pen # (if simple)	*/
	DspBrush brush;					/* Includes pallete info		*/
} DspColfnc;

typedef struct _dsp_spdfnc {
	INT speed;
	INT pens;
} DspSpdfnc;

typedef struct _dsp_visfnc {
	INT visible;
} DspVisfnc;

typedef struct _dsp_curfnc {
	INT x;
	INT y;
	int achr;
	void (*display)(INT ix, INT iy, CHAR **str);
} DspCurfnc;

typedef struct _dsp_pntfnc {
	INT x;
	INT y;
} DspPntfnc;

typedef struct _dsp_lwfnc {
	INT linewidth;
} DspLWfnc;

typedef struct _dsp_pagfnc {
	INT UsePage;
	INT ShowPage;
} DspPagfnc;

typedef struct _dsp_ioctl {
	CHAR    *str;
	INT ioctl;
} DspIOCTL;

typedef struct _dsp_regfnc {
	INT x1;
	INT y1;
	INT x2;
	INT y2;
} DspRegfnc;

typedef struct _dsp_filfnc {
	INT x;
	INT y;
} DspFilfnc;

typedef struct _dsp_fillpath {
	int color;
	int mode;
} DspFillPath;

typedef struct _dsp_strokepath {
	int color;
	int linewidth;
} DspStrokePath;

typedef struct _dsp_txtfnc {
	CHAR    *str;
	INT row;
	INT col;
} DspTxtfnc;

typedef struct _dsp_curbox {
	INT x1;
	INT y1;
	INT x2;
	INT y2;
	int achr;
} DspCurbox;

typedef struct _dsp_axeslimits {
	int axes_x1,axes_y1, axes_x2,axes_y2;	/* x,y coordinates of axes    */
	DspBrush axes_brush;							/* Includes pallete info		*/
	int area_x1,area_y1, area_x2,area_y2;	/* x,y coordinates of full area */
	DspBrush area_brush;							/* Area fill brush				*/
} DspAxesLimit;

typedef struct _dsp_fillrect {
	int x1,y1, x2,y2;								/* x,y coordinates of corners */
	DspBrush brush;								/* Includes pallete info		*/
} DspFillRect;

typedef struct _dsp_palette {
	int num_entries;								/* Number of pens defined		*/
	INT32 rgb[1];									/* List of the RGB values		*/
} DspPalette;

/* Font specifications must be in upper 8 bits */
#define	DSP_FAMILY_MASK		0xF000		/* Family (Times, Helv, Hersh)	*/
#define	DSP_CP_MASK				0x0F00		/* Code page (ASCII, MATH)			*/
#define	DSP_ATTRIB_MASK		0x00FF		/* Bold, italics, script, gothic	*/

/* Font families */
#define	DSP_FAMILY_HERSHEY	0x1000		/* Mapping for 0-10 still work  */
#define	DSP_FAMILY_HELV		0x2000		/* These names are internal fonts */
#define	DSP_FAMILY_TMSRM		0x3000		/* And may be specified as needed */
#define	DSP_FAMILY_GOTHIC		0x4000
#define	DSP_FAMILY_SCRIPT		0x5000

/* Font code pages */
#define	DSP_CP_ASCII			0x0100		/* Default is ASCII					*/
#define	DSP_CP_MATH				0x0200		/* Alternate to use GREEK/MATH	*/
#define	DSP_CP_SYMBOL			0x0300		/* True symbols (square, circle)	*/
#define	DSP_CP_ODDGREEK		0x0400		/* Alternate to use oddgreek set */

/* Attribute specification must be in lower 24 bits */
#define	DSP_ATTRIB_NORMAL		0x000
#define	DSP_ATTRIB_ITAL		0x001
#define	DSP_ATTRIB_BOLD		0x008

typedef struct _dsp_drawchar {
	CHAR    chr;				/* Character to print		(cp 850)	*/
	INT	  x,y;				/* Position					   (pixels)	*/
	INT	  font;				/* Font/Attribute to use   (above)	*/
	INT	  size;				/* Nominal size				(pixels)	*/
	INT	  angle;				/* Angle from horizontal  (degrees)	*/
	REAL	  matrix[4];		/* Transform matrix (3D)	(CTM)		*/
} DspDrawChar;

typedef struct _dsp_drawmarker {
	INT	 	x,y;				/* Center position - pixels			*/
	INT	 	size;				/* Marker scaled size - pixels		*/
	INT	 	isym;				/* Marker selection						*/
	INT	 	angle;			/* Angle from horizontal (degrees)	*/
} DspDrawMarker;

#define	MARK_SQUARE							0
#define	MARK_CIRCLE							1
#define	MARK_TRIANGLE						2
#define	MARK_CROSS							3
#define	MARK_X								4
#define	MARK_DIAMOND						5
#define	MARK_STAR							6
#define	MARK_FILLED_SQUARE				7
#define	MARK_FILLED_CIRCLE				8
#define	MARK_FILLED_TRIANGLE				9
#define	MARK_FILLED_LEFTTRIANGLE		10
#define	MARK_ASTERISK						11
#define	MARK_FILLED_RIGHTTRIANGLE		12
#define	MARK_FILLED_STAR					13
#define	MARK_STAROFDAVID					14
#define	MARK_LAST							14

typedef struct _dsp_clipset {
	INT	  xl,yl;				/* Lower left  corner */
	INT	  xh,yh;				/* Upper right corner */
} DspClipSet;

typedef union _DSP {
	DspInifnc ini;
	DspLinfnc line;
	DspErsfnc erase;
	DspFrmfnc frame;
	DspColfnc col;
	DspSpdfnc spd;
	DspVisfnc vis;
	DspCurfnc cur;
	DspPntfnc point;
	DspLWfnc  lw;
	DspPagfnc page;
	DspIOCTL  ioctl;
	DspRegfnc reg;
	DspFilfnc fill;
	DspFillPath fillpath;
	DspStrokePath strokepath;
	DspTxtfnc text;
	DspCurbox curbox;
	DspDrawChar drwchr;
	DspDrawMarker mark;
	DspClipSet clip;
	DspAxesLimit axes;
	DspFillRect rect;
	DspPalette palette;
} DSP;


#endif		/* _INC_COMPLOT */
