#ifndef __tplot
   #define __tplot

#define GPT_NUM_SYMBOLS		13						/* Number of symbols defined		*/
#define GPT_NUM_LTYPES		7						/* Number of linetypes defined	*/

typedef	struct _plot_palette {
	int   num_entries;								/* Number of entries			*/
	int   num_pens;									/* How many are user pens	*/
	INT32 rgb[1];										/* Color values				*/
} PLOT_PALETTE;

void		PlotInitialize(void);					/* Initialize TPLOT		*/
void		PlotReset(LOGICAL FullReset);			/* Reset all parms		*/
void		PlotResetAnnote(LOGICAL FullReset);	/* (done by PlotReset)	*/

int		PlotSelectDevice(char *name, int *status); /* Select/open device	*/
LOGICAL	PlotOpenDevice(void);					/* Open  a device					*/
void	   PlotCloseDevice(void);					/* Close a device					*/
void		PlotConformDevice(void);				/* Conform device to common	*/

int		PlotSelectTablet(char *name, int ip[]);	/* Select/open tablet	*/
LOGICAL	PlotOpenTablet(void);					/* Open  a tablet					*/
void     PlotCloseTablet(void);					/* Close a tablet					*/
LOGICAL	PlotQueryTablet(int key, int *x, int *y, int *achr);

void		PlotFrame(void);							/* Frame the device (obsolete) */
void		PlotErase(void);							/* Erase the frame (obsolete)  */
void		PlotNewPage(int panel);					/* Select a new page draw	*/
void		PlotFlush(void);							/* Flush plot buffers	*/
void		PlotShutDown(void);						/* Shut down everything	*/
void		PlotInformAxesLimits(int color);		/* Tell device region of axes */

void		PlotEnterTextMode(void);							/* Return to A/N mode	*/

void		PlotSetPenSpeed(INTEGER spd, INTEGER pens);	/* Set pen speed			*/

LOGICAL	PlotSetRange(REAL xl,REAL xh,REAL yl,REAL yh);	/* Set range		*/
LOGICAL	PlotSetUserMode(LOGICAL flag);					/* Select user coord	*/
void		PlotSetOrigin(REAL xnew, REAL ynew);			/* Set new origin		*/
void		PlotSetSize(REAL xnew, REAL ynew);				/* Set new plot size	*/
void		PlotSetMargin(REAL xl, REAL yl, REAL xh, REAL yh);		/* Set new margins	*/
REAL		PlotSetFactor(REAL f);								/* Set reduction		*/
LOGICAL	PlotSetXYFlip(LOGICAL flag);						/* Set flipxy flag	*/
int		PlotSetPageOrientation(int mode);				/* Set page orient	*/
void		PlotSetClip(INTEGER *key, REAL parms[]);		/* Set clip range		*/
int		PlotSetSymbolClip(BOOL key);						/* Set symbol clip	*/
INTEGER	PlotSetLineWidth(INTEGER idat);					/* Set line width		*/
INTEGER	PlotSetLineType(INTEGER ltyp, REAL vsize);	/* Set dash repeats	*/
void		PlotQueryLineType(INTEGER *ltype, REAL *vsize);

PLOT_PALETTE *PlotSetPalette(PLOT_PALETTE *palette);
#define	PlotSelectPen(i)	PlotSelectColor(i)
INT32		PlotSelectColor(INT32 rgb);						/* Select a color			*/
INTEGER	PlotMatchColor(char *color, INTEGER dflt);	/* Match user request	*/
INTEGER	PlotMatchSymbol(char *symbol, INTEGER dflt);	/* Match user request	*/
INTEGER	PlotSelectFont(INTEGER font);						/* Select a  font			*/
LOGICAL	PlotRequestHardCopy(void);							/* Do graphics print		*/
INTEGER	PlotSetVisibility(INTEGER idat);					/* Vector visibility		*/
LOGICAL	PlotSetAutoFlush(LOGICAL ldat);					/* Set autoflush flag	*/

void		PlotEraseRegion(REAL x1, REAL yy1, REAL x2, REAL y2);
void		PlotFillRect(REAL x1, REAL y1, REAL x2, REAL y2, int color);

void		PlotAutoScaleZ  (REAL fmin, REAL fmax, REAL *smin, REAL *smax, REAL *dx, REAL *dx2, INTEGER *m, REAL zforce);
void		PlotAutoScale   (REAL fmin, REAL fmax, REAL *smin, REAL *smax, REAL *dx, REAL *dx2, INTEGER *m);
void		PlotAutoLogScale(REAL fmin, REAL fmax, REAL *smin, REAL *smax, REAL *dx, REAL *dx2, INTEGER *m);

LOGICAL	PlotLoadFonts(INTEGER key);

void		PlotSymbol(REAL x, REAL y, REAL height, CHAR ich);
void		PlotString(REAL x, REAL y, REAL height, CHAR *str, REAL theta, INTEGER n);
void		PlotInchString(REAL x, REAL y, REAL height, CHAR *str, REAL theta, INTEGER n);
void		PlotNumber(REAL x, REAL y, REAL height, REAL rval, REAL theta, INTEGER nopt);

void		Plot3DSymbol(REAL x, REAL y, REAL z, REAL height, CHAR ich);
void		Plot3DString(REAL x, REAL y, REAL z, REAL height, CHAR *str, REAL theta, INTEGER n);
void		Plot3DInchString(REAL x, REAL y, REAL z, REAL height, CHAR *str, REAL theta, INTEGER n);
void		Plot3DNumber(REAL x, REAL y, REAL z, REAL height, REAL rval, REAL theta, INTEGER nopt);

REAL		PlotQueryStringLength(REAL pheigh, char *str, INTEGER n);
LOGICAL	PlotID(INTEGER ltype, INTEGER sym, INTEGER ipen, INTEGER lwidth, CHAR *id);

typedef struct _PLOT_AXIS_LABELS {
	INTEGER nmajor,nminor;			/* Number of entries in the list for major/minor tick marks */
	REAL csize;							/* Size - if <=0, use csmax */
	REAL *major, *minor;				/* Array of values where tick marks (major/minor) should be drawn */
	char **labels;						/* Array of the labels for each major tick mark */
} PLOT_AXIS_LABELS;

void		PlotDrawScopeFace(void);
void		PlotAxis(REAL x, REAL y, INTEGER itype, REAL size, REAL dx, REAL dx2, 
			 CHAR *title, INTEGER n, INTEGER m, INTEGER color, PLOT_AXIS_LABELS *labels);
void		PlotLogAxis(REAL x, REAL y, INTEGER itype, REAL size, REAL dx, REAL dx2, 
			 CHAR *title, INTEGER n, INTEGER m, INTEGER color, PLOT_AXIS_LABELS *labels);
void		PlotNonLinearAxis(REAL x, REAL y, INTEGER itype, REAL size, REAL dx, REAL dx2, 
			 CHAR *title, INTEGER n, INTEGER m, INTEGER color, PLOT_AXIS_LABELS *labels, REAL (*conv)(REAL x2) );

LOGICAL	PlotSystem(INTEGER key, CHAR *cmd, INTEGER *parms);
LOGICAL	PlotSgraph(const char *token, REAL value);
LOGICAL	PlotAnnote(LOGICAL autoflag);

LOGICAL	HardCopy(INTEGER key);

typedef struct _ERRORSYMBOL {
	REAL x,y;				/* x,y position												*/
	REAL errs[4];			/* xm,xp, ym,yp errors										*/
	REAL widthx;			/* length (inches) of cross bar on X error (y dir)	*/
	REAL widthy;			/* length (inches) of cross bar on Y error (x dir)	*/
	REAL symsiz,loadsiz;	/* Symbol size (inches)										*/
	CHAR isym,loadsym;	/* Symbol type (index)										*/
	REAL obuf[4];			/* Work buffer													*/
} ERRORSYMBOL;

void		PlotYPlot(REAL yarray[], INTEGER npt, REAL xstart, REAL dx, INTEGER ltype, INTEGER sym, REAL symsize, INTEGER ipt, INTEGER mode);
void		PlotLine2(REAL x[], REAL y[], INTEGER npt, INTEGER *ltype1);
void		PlotLine3(REAL x[], REAL y[], INTEGER npt, INTEGER npoint, INTEGER *ltype1);
void		PlotLine1(REAL x[], REAL y[], INTEGER npt, INTEGER ipt, INTEGER itype, INTEGER isym, REAL symsize);
void		PlotDrawErrorSymbol(ERRORSYMBOL *buf);
void		PlotCircle(REAL x, REAL y, REAL radius);
INTEGER	PlotFill(INTEGER key, REAL x, REAL y);
void		PlotText(INTEGER row, INTEGER col, CHAR *text);

int		PlotBeginPath(void);
int		PlotEndPath(void);
int		PlotFillPath(int color, int mode);
int		PlotStrokePath(int color, int linewidth);

LOGICAL	PlotAllocateWindow(void **Window);
LOGICAL	PlotAllocateDevice(void **Device);
void		PlotResetWindow(void *Window, LOGICAL FullReset);	
void		PlotResetDevice(void *Device, LOGICAL FullReset);	

void		PlotSetScaling(REAL x0, REAL xf, REAL yy0, REAL yf);
void		PlotQueryPosn(REAL *x, REAL *y, REAL *f);
void		PlotQuerySymbolExtent(INTEGER ich, REAL obuf[]);

void		PlotMove(REAL xp, REAL yp, INTEGER ipen);
void		PlotMove3D(REAL xp, REAL yp, REAL zp, INTEGER ipen);
void		PlotMoveInch(REAL xp, REAL yp, INTEGER ipen);
void		PlotMove3DInch(REAL xp, REAL yp, REAL zp, INTEGER ipen);

LOGICAL	PlotPushState(void);
LOGICAL	PlotPopState(void);

void		PlotCursor(REAL *x, REAL *y, INTEGER *aptr);
LOGICAL	PlotTrackingCursor(REAL x[],REAL y[], INTEGER npt, INT *inow, INTEGER *iret);
LOGICAL	PlotBoxCursor(REAL *x1, REAL *yy1, REAL *x2, REAL *y2, INTEGER *achr);

void		ArrayMinMax(REAL *x, INT npt, REAL *xmin, REAL *xmax);
LOGICAL	OrderPair(REAL *x1, REAL *x2);

LOGICAL	PlotSet3DMode(LOGICAL mode);
void		PlotSet3DView(REAL r, REAL xd, REAL yd, REAL zd);
void		PlotSet3DRange(REAL xmin, REAL xmax, REAL ymin, REAL ymax, REAL zmin, REAL zmax);

#define	USER_TO_INCH	1
#define	INCH_TO_USER	2
#define	USER_TO_PIXEL	3
#define	PIXEL_TO_USER	4
#define	INCH_TO_PIXEL	5
#define	PIXEL_TO_INCH	6
#define	NORM_TO_INCH	7
#define	INCH_TO_NORM	8
#define	USER_TO_GRID	9
#define	INCH_TO_GRID	10

void		PlotConvert2DScales(int mode, REAL x, REAL y, REAL *xp, REAL *yp);
void		PlotConvert3DScales(int mode, REAL x, REAL y, REAL z, REAL *xp, REAL *yp, REAL *zp);

/* --------- ROUTINE INTENDED FOR INTERNAL CONSUMPTION ONLY ------------ */
void		HardCopyCloseSave(void);
void		PlotFixInternal(void);
void		Plot3DTransform(int key, REAL gp[3][3], REAL gp_old[3][3]);
void		Plot3DMultiply(int key, REAL gp[4][4]);

#endif /* __tplot */
