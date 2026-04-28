/*  ------------------------------------------------------------------------ */
/*  ---------                                              ----------------- */
/*  --------- COPYRIGHT 1989 (c) Computer Graphics Service ----------------- */
/*  ---------                                              ----------------- */
/*  ---------    The source code to RUMP may be freely     ----------------- */
/*  ---------  modified as long as this copyright notice   ----------------- */
/*  ---------          is included and unchanged.          ----------------- */
/*  ------------------------------------------------------------------------ */

/*  aplot.c */

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
typedef enum _OPS1 {
	RP_ANNOTE,		RP_LTYPE,		RP_SYMBOL,		RP_FORCEX,		RP_SUBTICKS,
	RP_PLOT,			RP_REPLOT,		RP_OVERLAY,		RP_BLOWUP,		RP_EXPAND,
	RP_COMPARE,		RP_GET,			RP_REREAD,		RP_IDS,			RP_IDENTIFY,
	RP_AXIS,			RP_MARK,			RP_REGION,		RP_COUNTS,		RP_LABELS,
	RP_AUTOIDS,		RP_LINEAR,		RP_LOG,			RP_SQRT,			RP_NORMAL,
	RP_RAW,			RP_PLX,			RP_NPOINTS,    RP_SYMSIZE
} OPS1;

typedef struct _CMTYPE {
	char *name;
	int  minlen;
	OPS1  rcode;
} CMTYPE;


static CMTYPE cmlist1[] = {
	{"annotate",	3,	RP_ANNOTE},		{"annote",		-6,	RP_ANNOTE},
	{"ltype",		2,	RP_LTYPE},		{"linetype",	 5,	RP_LTYPE},
	{"symbol",		2,	RP_SYMBOL},
	{"symsize",		4, RP_SYMSIZE},	{"symbolsize",	-7,	RP_SYMSIZE},
	{"forcex",		4,	RP_FORCEX},
	{"subticks",	5,	RP_SUBTICKS},
	{NULL,			0,	RP_NPOINTS}
};

static CMTYPE cmlist2[] = {
	{"plot",			2,	RP_PLOT},
	{"replot",		2,	RP_REPLOT},
	{"overlay",		2,	RP_OVERLAY},
	{"blowup",		2,	RP_BLOWUP},
	{"expand",		2,	RP_EXPAND},
	{"read",			4,	RP_GET},			{"get",			-2,	RP_GET},				
	{"reread",		3,	RP_REREAD},
	{"compare",		4,	RP_COMPARE},
	{"ids",			2,	RP_IDS},	
	{"identify",	5,	RP_IDENTIFY},	{"legend",		-3,	RP_IDENTIFY},
	{"axis",			2,	RP_AXIS},		{"axes",			-4,	RP_AXIS},
												{"mark",			-2,	RP_MARK},
	{"region",		3,	RP_REGION},
	{"counts",		2,	RP_COUNTS},
	{"labels",		3,	RP_LABELS},
	{"autoids",		2,	RP_AUTOIDS},
	{"linear",		2,	RP_LINEAR},
	{"log",			2,	RP_LOG},
	{"sqrt",			2,	RP_SQRT},
	{"normalize",	2,	RP_NORMAL},
	{"raw",			2,	RP_RAW},
	{"plx",			2, RP_PLX},
	{"npoints",		2,	RP_NPOINTS},
	{NULL,			0,	RP_NPOINTS}
};


/*      BOOL FUNCTION APLOT(KEY, TOKE) */
/* ===========================================================================
--  Usage Guide:
--      BOOL FUNCTION APLOT(KEY, TOKE)
--
--     APLOT performs RUMP commands which deal with the plot.
--     The ones which are not RUMP specific are passed off to PLSUPP.
--     This routine does not deal directly with the plot library -
--     everything goes through routines in TPLOT.F77.
--  Quick: Processor for user's plot commands
--
--   It is one of the five command processors called by RUMP itself.
--
--   INPUTS:   KEY    Chooses the operation:  0 = Process command
--                                           -1 = Initialize
--                                           -2 = Reset
--                                           -3 = List Commands
--                                           -4 = Display Parameters
--                                           -5 = Turn Off
--            TOKE   is the command (Character string) for the case KEY = 0
--  OUTPUT:   LOG    (Function value) True only for Key = 0 and the command
--                   was found and action was attempted.
--  COMMON BLOCKS:     RUMP, GRAPHICS
--  CALLED FROM:       RUMP
--  CALLS:             PLSUPP, TSTMRK, RBSPLT
--                     GETBUF, SWMODE, ORDERPAIR, IDS
--
=========================================================================== */
int RbsAplot(int key, char *command) {

/*  -- Local Variables -- */
	int i,j, gmkey;
	REAL x1;
	SPECTRUM *tmp;
	CMTYPE *cmd;
	int logtmp;
	char token[DFLT_STR_SIZE];

/*  -- Code begin -- */
	if (key != 0) {						/* handle the U$xxxx keys */
		switch (key) {
			case U_INIT:							/* Initialize and reset		*/
			case U_RESET: 
				Rmp->linear	    = GR_LIN;		/*  Want linear scales */
				Rmp->labels     = GR_ON;
				Rmp->xaxis_mode = X_ENERGY;

				Rmp->ymin   = 0;		Rmp->ymax = 3000;
				Rmp->chmin  = 100;	Rmp->chmax = 600;
				Rmp->emin   = 0.5;	Rmp->emax  = 3.0;
				Rmp->cospec = 0;

				Rmp->forcex = FALSE;				/* Don't force scales		*/
				Rmp->autoy  = TRUE;				/* Autoscale counts scale	*/
				Rmp->mtick  = TRUE;				/* Draw minor tick marks	*/
				Rmp->autoid = FALSE;				/* Don't do ID automatic	*/
				Rmp->raw    = FALSE;				/* Draw normalized data		*/

				Rmp->chrsiz = 0.16f;				/* Default character size	*/

				Rmp->AutoLineType = FALSE;
				Rmp->AutoSymbols  = FALSE;
				Rmp->linetype = Rmp->linetypestart = 0;
				Rmp->symtype  = Rmp->symtypestart  = 0;
				Rmp->npoint = 1;

				PlotSgraph("zforce", 0.1f);	/*  For old time's sake */
				break;

			case U_HELP:							/* Our share of the help */
				LexCmdlPrint(cmlist2, sizeof(CMTYPE), "RUMP plotting:");
				LexCmdlPrint(cmlist1, sizeof(CMTYPE), NULL);
				break;

			case U_PARM:							/* Our share of parm listing */
				TTYprintf("\n Min-Max Channels: %7.1f %7.1f     ",Rmp->chmin,Rmp->chmax);
				if (Rmp->cospec != 0) {
					TTYprintf("Max Counts:  %9.2f\n",Rmp->cospec);
				} else {
					TTYprintf("Max Counts:  Autoscaled\n");
				}
				if (Rmp->forcex) 
					TTYputs(" Channel scales are forced to user specified values\n");
				if (Rmp->xaxis_mode == X_CHANNEL)
					TTYputs(" Plots use channel numbers instead of energy as abscissa\n");

				if (Rmp->linear == GR_LIN) TTYprintf(" Scale is linear,");
				if (Rmp->linear == GR_SQR) TTYprintf(" Scale is square root of");
				if (Rmp->linear == GR_LOG) TTYprintf(" Scale is logarithmic in");
				if (Rmp->raw) {
					TTYprintf(" raw counts.\n");
				} else {
					TTYprintf(" normalized yield.\n");
				}

				i = (! Rmp->AutoLineType) ? Rmp->linetypestart : -( (Rmp->linetypestart%GPT_NUM_LTYPES) + 1) ;
				j = (! Rmp->AutoSymbols)  ? Rmp->symtypestart  : -( (Rmp->symtypestart%GPT_NUM_SYMBOLS) + 1) ;
				TTYprintf(" Linetype: %3d  Symbol: %4d  Npoint: %3d  SymSize: %.3f\n", 
					i, j, Rmp->npoint, Rmp->SymSize);
				if (Rmp->labels == GR_OFF)	TTYprintf(" Axis labels are disabled.\n");
				if (Rmp->labels == GR_BRF)	TTYprintf(" Brief labeling in effect.\n");
				if (! Rmp->mtick)			TTYprintf(" Minor tick marks are disabled.\n");
				if (Rmp->autoid)			TTYprintf(" Auto IDS mode is enabled.\n");
				break;

			case U_QUIT:								/* Exit */
				break;

			default:
				if (key > 0) {
					ERRprintf("ERROR: Unknown key (%d) passed to APLOT\n", key);
					return(FALSE);
				}
		}
		return(TRUE);

/* ================ Plot layout commands ===================== */
	} else if ( (cmd = LexCmdl(command, cmlist1, sizeof(CMTYPE))) != NULL) {
		switch (cmd->rcode) {

/* --- ANNOTATE  -  General user capability to annotate the plot. */
			case RP_ANNOTE:
				PlotSetRange(Rmp->chmin,Rmp->chmax,Rmp->ymin,Rmp->ymax);
				PlotAnnote(TRUE);
				return(TRUE);

/* --- LINETYPE  -  Change the linetype used for plotting */
			case RP_LTYPE:
				i = LexGetInt(-99, "Linetype (0=points, -n = AUOTCHANGE) (Unchanged) ");
				if (! LexEscape(TRUE) && i != -99) {
					Rmp->AutoLineType = (i < 0);
					Rmp->linetype = Rmp->AutoLineType ? ((-i+GPT_NUM_LTYPES-2)%GPT_NUM_LTYPES) + 1 : i ;
					Rmp->linetypestart = Rmp->linetype;
				}
				return(TRUE);

/* --- SYMSIZE - New symbol size for plots */
			case RP_SYMSIZE:
				x1 = LexGetReal(-987.125, "Symbol size (no change, 0.10 recommended): ");
				if (! LexEscape(TRUE) && x1 != -987.125) Rmp->SymSize = x1;
				return(TRUE);
				
/* --- SYMBOL  -  New symbol for plotting */
			case RP_SYMBOL:
				if (LexGetMathP(token, sizeof(token), "Symbol number for point plotting mode (3)? ")) {
					if (! LexEscape(TRUE)) {
						i = PlotMatchSymbol(token, 3);
						if (Rmp->linetype != 0) gen_warn("Set LTYPE = 0 for symbols");
						Rmp->AutoSymbols = (i < 0);
						Rmp->symtype = Rmp->AutoSymbols ? ( (-i+GPT_NUM_SYMBOLS-2)%GPT_NUM_SYMBOLS) + 1 : i ;
						Rmp->symtypestart = Rmp->symtype;
					}
				}
				return(TRUE);

/* --- FORCEX - Ask if to force the x axis values to specified values */
			case RP_FORCEX: 
				Rmp->forcex = LexYesNo(FALSE,"Force X PlotAxis scales (NO) ");
				return(TRUE);

/* --- SUBTICK - Subdivision tick marks, on or off */
			case RP_SUBTICKS: 
				Rmp->mtick = LexOnOff(TRUE, "Minor tick marks (ON|off)?  ");
				return(TRUE);

/* --- default */
			default:
				return(FALSE);   /* Should not happen */
		}


/* ================ General spectrum plot commands ===================== */
	} else if ( (cmd = LexCmdl(command, cmlist2, sizeof(CMTYPE))) != NULL) {
		switch (cmd->rcode) {

/* ... PLOT - plot specified buffer/file */
			case RP_PLOT: 
				tmp = ibuf;
				if ( (tmp = RbsPlot(PLT_QY | PLT_AX | PLT_OV, tmp)) == NULL ) goto nomore;
				if (tmp != ALTBUF) ibuf = tmp;	/*  Leave IBUF alone if pl 0 */
				return(TRUE);

/* ... REPLOT - replot the current file with the new axis or something */
			case RP_REPLOT: 
				if ( RbsPlot(PLT_AX | PLT_OV,ibuf) == NULL ) goto nomore;
				return(TRUE);

/* ... OVERLAY - Overlay another plot on the same axis */
			case RP_OVERLAY: 
				tmp = ibuf;
				if ( (tmp = RbsPlot(PLT_QY | PLT_OV, tmp)) == NULL) goto nomore;
				if (tmp != ALTBUF) ibuf = tmp;	/*  Leave IBUF alone if ov 0 */
				return(TRUE);

/* ... COMPARE - compares current buffer with simulation  REP OV 0 */
			case RP_COMPARE: 
				if (RbsPlot(PLT_AX | PLT_OV, ibuf) != NULL) {
					i = Rmp->linetype;
					if (Rmp->linetype == 0) Rmp->linetype = 1;
					logtmp = (RbsPlot(PLT_OV, ALTBUF) != NULL);
					if (i == 0) Rmp->linetype = 0;
					if (logtmp) return(TRUE);
				}
				goto nomore;

/* ... BLOWUP  -  Blows up a desired region of the plot */
			case RP_BLOWUP: 
				if ( RbsPlot(PLT_BL,ibuf)==NULL) goto nomore;
				return(TRUE);

/* ... EXPAND  -  Expand the plot with a cursor identify */
			case RP_EXPAND: 
				Rmp->chmin = LexGetReal(-987.125, "Lower channel in expanded region (cursor): ");
				if (Rmp->chmin == -987.125) {
					gmkey = 1;
					Rmp->chmin = RBSCNNLE(RbsGetMeV(&gmkey, 0.0,
						"Place cursor at new channel limits",
						"Channel RbsRange of expand?  "), ibuf);
					Rmp->chmax = RBSCNNLE(RbsGetMeV(&gmkey, 0.0, " ",
						"Upper channel of expand?  "), ibuf);
				} else {
					Rmp->chmax = LexGetReal(600.0, "Upper channel of expansion (600): ");
				}
				OrderPair(&Rmp->chmin, &Rmp->chmax);
				if ( RbsPlot(PLT_AX | PLT_OV, ibuf)==NULL) goto nomore;
				return(TRUE);

/* ... GET  -  read the .rbs file from disk */
			case RP_GET: 
				if ((ibuf = RbsGetBuf("File Name: ",ibuf)) != NULL) return(TRUE);
				ibuf = MAINBUF;
				goto nomore;

/* ... REREAD - Gets the file back after possible modification */
			case RP_REREAD:
				if (strlen(ibuf->filename) == 0) goto nomore; /*  Anything there */
				strscpy(token, ibuf->filename, sizeof(token));
				SysReplaceExt(ibuf->filename, ".old");
				if ( (tmp = RbsRdFile(token)) == NULL) goto nomore;
				RbsBufferScroll(tmp);
				ibuf = MAINBUF;                          /* Point to this one */
				return(TRUE);

/* ... IDS - put a short section of the linetype on graph and id */
			case RP_IDS: 
				if ( ! PlotSystem(2, NULL, NULL))   {	  /* Have to have plotter */
					gen_err("Device not enabled");
					goto nomore;
				}
				PlotID(Rmp->linetype, Rmp->symtype, -1, 0, (*ibuf->id != '\0') ? ibuf->id : ibuf->filename);
				if (ibuf == ALTBUF) SimDrawSample();	/*  Simulation list on plot */
				return(TRUE);

/* ... IDENTIFY - put a short section of the linetype on graph and id */
			case RP_IDENTIFY: 
				if (LexGetOption(token, sizeof(token))) {
					LexBackup();
					*token = '\0';
				} else if (! LexGetToken(token, sizeof(token))) {
					LexPromptStr(token, sizeof(token), "Text (desc): ");
					if (*token == '\0') strscpy(token, ibuf->id, sizeof(token));
				}
				PlotID(Rmp->linetype, Rmp->symtype, -1, 0, token);
				return(TRUE);

/* ... AXIS - New axis drawn here.  Clears screen and resets origin */
			case RP_AXIS: 
				if (RbsPlot(PLT_AX, ibuf) != NULL) return(TRUE);	/*  Set to last buffer */
				goto nomore;

/* ... Test the mark routine */
			case RP_MARK:  
				RbsTestMark();
				return(TRUE);

/* ... REGION  -  Set the desired channel region
-- Modify command so tolerates command structure of GENPLOT
-- Allow region [Left | Right | Top | Bottom | X | Y]
-- as well as immediate form previous used.
------------------------------------------------------------ */
			case RP_REGION: 
				if (LexGetToken(token, sizeof(token))) {
					i = LexSelect(token, "bottom channel x top energy left counts y");
				} else {
					i = 1;
				}

				if (i <= 3) {
					if (i <= 0) LexBackup();
					Rmp->chmin = LexGetReal(100.0,"Min/Max channels? (100)  ");
					Rmp->chmax = LexGetReal(600.0,"Maximum channel? (600)  ");
				} else if (i <= 5) {
					Rmp->chmin = RBSCNNLE(LexGetReal(0.5, "Min/Max energy? (0.5 MeV)  "), ibuf);
					Rmp->chmax = RBSCNNLE(LexGetReal(3.0, "Maximum energy? (3.0 MeV)  "), ibuf);
				} else {
					LexGetMathP(token, sizeof(token), "Lower counts (ignored): ");
					Rmp->cospec = LexGetReal(0.0,"Maximum counts (0 = AUTOSCALE)? (0)  ");
				}

				if (Rmp->chmin == Rmp->chmax) Rmp->chmax = max(2*Rmp->chmin, 1);
				OrderPair(&Rmp->chmin, &Rmp->chmax);
				return(TRUE);

/* ... COUNTS  -  Set the desired maximum counts to be displayed */
			case RP_COUNTS: 
				Rmp->cospec = LexGetReal(0.0,"Maximum counts (0 = AUTOSCALE)? (0)  ");
				return(TRUE);

/* ... LABELS  -  Set mode of axis labeling */
			case RP_LABELS: 
				if (LexGetTokenP(token, sizeof(token), "Axis label mode? (ON) ")) {
					switch (LexSelect(token,"off brief on")) {
						case 1:
							Rmp->labels = GR_OFF; break;
						case 2:
							Rmp->labels = GR_BRF; break;
						case 3:
							Rmp->labels = GR_ON; break;
						default:
							ERRprintf("ERROR: %s is an unrecognized LABELS specifier\n", token);
							goto nomore;
					}
				} else {
					Rmp->labels = GR_ON;
				}
				return(TRUE);

/* ... AUTOID - Enable the ids feature for every plot */
			case RP_AUTOIDS: 
				Rmp->autoid = LexOnOff(FALSE,"Auto IDS mode? (OFF)  ");
				return(TRUE);

/* ... LINEAR - Specify linear plot subsequent data */
			case RP_LINEAR:  
				RbsSwmode(GR_LIN);
				return(TRUE);

/* ... LOG - Specify logarithmic plot of data */
			case RP_LOG:
				RbsSwmode(GR_LOG);
				return(TRUE);

/* ... SQRT - Specify square root plot of data */
			case RP_SQRT:
				RbsSwmode(GR_SQR);
				return(TRUE);

/* ... NORMALIZED - Specifies desire to normalize the data with charge, */
/*                  solid angle and correction factor.                  */
			case RP_NORMAL:
				Rmp->raw = FALSE;
				return(TRUE);

/* ... RAW - Specifies only number of actual counts is to be output */
			case RP_RAW: 
				Rmp->raw = TRUE;
				return(TRUE);

/* ... PLX - Specify either counts or energy as X plotting scale */
			case RP_PLX:
				if (LexGetTokenP(token, sizeof(token), "Plot counts versus [Energy | Channel] (no change): ")) {
					switch (LexSelect(token, "energy channel")) {
						case 1:
							Rmp->xaxis_mode = X_ENERGY; break;
						case 2:
							Rmp->xaxis_mode = X_CHANNEL; break;
						default:
							ERRprintf("ERROR: %s is an unrecognized PLX specifier\n", token); 
							goto nomore;
					}
				}
				return(TRUE);

/* ... NPOINTS - Specifies every i-th point to be used */
			case RP_NPOINTS: 
				Rmp->npoint = LexGetInt(1,"Every I-th point will be used for I = ? (1)  ");
				if (Rmp->npoint <= 0) Rmp->npoint = 1;
				return(TRUE);

/* ... default */
			default: 
				return(FALSE);				    /* Should not happen */
		}
	}

	return(FALSE);								/*  Unknown command */

/*  * * * PROBLEM RETURN * * * (Otherwise known as NOMORE) */
nomore:  
	LexFlush();
	return(TRUE);
}


/* ===========================================================================
--  Usage Guide:
--
--      SUBROUTINE RBSPLT(KEY,IBF)
--
--     Subroutine PLTIT overlays a buffer on the plot
--
--     Quick: Intermediate level spectrum plot routine
--
--     INPUTS:   KEY      to what operations to perform.
--                        sum of any combination of the following:
--                  PLT_QY  Query user for 'File or buffer'
--                           (will modify second argument in this case)
--                  PLT_AX  Put a new axis on the screen
--                  PLT_OV  Put the curve on the plot
--                  PLT_BL  Blowup a section of this data under user control
--
--               IBF      Buffer number for plotting, if no PLT_QY key
--                        (use NULL when in PLT_QY mode)
--
--     OUTPUT:   IBF      Buffer pointer, esp. if PLT_QY key used
--               Output is mostly just plotted on the graph.
--
--     COMMON BLOCKS:     RUMP, GRAPHICS
--     CALLED FROM:       APLOT, SPLOT, SIM
--     CALLS:             CHGPEN, PLDATA, AXDRAW, GETMEV, CHKSIM, PLSUPP
--
=========================================================================== */
typedef struct _CMTYPE1_ {
	char *name;
	int  minlen;
	enum {SYMBOL, LTYPE, PEN, SYMSIZE, LINEWIDTH, L_S, IDENT, IDS, NOIDS,
			NPOINT, SHIFT, OFFSET} rcode;
} CMTYPE1;

static const CMTYPE1 cmlist[] = {
						{"-symbol",		4, SYMBOL},
						{"-ltype",		3, LTYPE},
						{"-linetype",	5, LTYPE},
						{"-pen",			4, PEN},
						{"-color",		4, PEN},
						{"-symsize",	5, SYMSIZE},
						{"-symbolsize",8, SYMSIZE},
						{"-linewidth",	5, LINEWIDTH},
						{"-lw",			2, LINEWIDTH},
						{"-l&s",			4, L_S},
						{"-identify",	4, IDENT},
						{"-ids",			4, IDS},
						{"-noids",		6, NOIDS},
						{"-npoints",	3, NPOINT},
						{"-shift",		3, SHIFT},
						{"-offset",		4, OFFSET},
						{NULL,			0, SYMBOL} };

SPECTRUM *RbsPlot(int key, SPECTRUM *ibf) {

/*  -- Local variables -- */
	int gmkey;
	REAL tmp, x1, x2;

	char token[DFLT_STR_SIZE];
	CMTYPE1 *citem;
	PLOT_PARMS parms;
	BOOL SymbolRequested;

/* --- If no buffer provided, then query must be specified */
	if (ibf == NULL) key |= PLT_QY;			/* Must query if we have nothing */

/* --- Figure out which buffer */
	if (key & PLT_QY) {							/* Must we get a buffer? */
		if ( (ibf = RbsGetBuf("File or Buffer: (Same) ", ibf)) == NULL)
			return(NULL);
	}

/* --- Search for any options on command line */
	if (ibf == ALTBUF)  SimCheck(NULL);
	
/* --- Look for options on the command line  */
	parms.ibf       = ibf;
	parms.pen       = -1;								/* Default values */
	parms.linewidth = -1;
	parms.linetype  = -1;
	parms.symbol    = -1;
	parms.symsize   = -1;
	parms.npoint    = -1;
	parms.shift     = parms.offset = 0;				/* No shift of plot */
	parms.doids     = FALSE;							/* Could be request later */
	parms.localids  = FALSE;
	parms.Line_Syms = FALSE;
	SymbolRequested = FALSE;

	while (LexGetOption(token, sizeof(token))) {
		if ( (citem=LexCmdl(token, cmlist, sizeof(CMTYPE1))) == NULL) {
			ERRprintf("ERROR: %s is not recognized as a valid option\n", token);
			LexFlush();
			return(NULL);
		}
		switch (citem->rcode) {
			case SYMBOL:							/* -SYMBOL */
				if (LexGetMathP(token, sizeof(token), "Symbol: ")) {
					parms.symbol = PlotMatchSymbol(token, -1);
					SymbolRequested = TRUE;
				}
				break;
			case LTYPE:
				parms.linetype = LexGetInt(0, "LType: ");
				break;
			case PEN:
				if (LexGetMathP(token, sizeof(token), "Pen color: ")) 
					parms.pen = PlotMatchColor(token, -1);
				break;
			case SYMSIZE:
				parms.symsize = LexGetReal(0.35f, "Size: ");
				break;
			case LINEWIDTH:
				parms.linewidth = nint(7*LexGetReal(1.0, "Width: "));
				break;
			case L_S:										/* -L&S (lines and symbols) */
				parms.Line_Syms = TRUE;
				break;
			case IDENT:										/* -Identify */
				LexGetStrExprP(parms.ids, sizeof(parms.ids), "Id: ");
				parms.localids = TRUE;
				break;
			case IDS:										/* -ids		*/
				parms.doids = TRUE;
				break;
			case NOIDS:										/* -noids	*/
				parms.doids = FALSE;
				break;
			case NPOINT:
				parms.npoint = LexGetInt(1, "Every n-th point plotted: ");
				break;
			case OFFSET:
				parms.offset = LexGetReal(0.0, "Offset of plot (in counts): ");
				break;
			case SHIFT:
				parms.shift = LexGetReal(0.0, "Shift of plot (in channels): ");
				break;
		}
	}
	if (SymbolRequested && ! parms.Line_Syms) parms.linetype = 0;

/* --- Prepare the screen, either by drawing an axis or changing pens */
	if (key & PLT_AX)   {
		if (! RbsAxdraw(ibf)) return(NULL);			/*  Draw axis if needed */
	} else {
		if (! PlotSystem(2, NULL, NULL)) {
			ERRprintf("WARNING: Plotter was uninitialized\n");
			if (! PlotSystem(1, NULL, NULL)) return(NULL);	/* And frame device */
		}
	}

/* --- For blowup, we have some questions to ask the user */
	if (key & PLT_BL)   {
		if (Rmp->linear == GR_LOG)   {
			ERRprintf("ERROR: Blowup is not implemented with logarithmic axes\n");
			return(NULL);
		}

		x1 = LexGetReal(-987.125, "Channels to be blown up (cursor): ");
		if (x1 == -987.125) {
			gmkey = 1;
			x1 = RBSCNNLE(RbsGetMeV(&gmkey, 100.0,
				"Use cursor to define region to be blown up.",
				"Lower channel for blow up. (100)  "), ibf);
			x2 = RBSCNNLE(RbsGetMeV(&gmkey, 200.0," ",
				"Upper channel for blow up. (200)  "), ibf);
			if (gmkey == 1)
				TTYprintf(" Channels selected %6.1f to %6.1f\n",x1,x2);
		} else  {
			x2 = LexGetReal(600.0, "Upper channel of blow up (600): ");
		}
		OrderPair(&x1, &x2);							/* Assure X1 < X2 */

		tmp = LexGetReal(0.0, "Expansion factor? (abort)  ");
		if (tmp == 0 || (Rmp->linear == GR_SQR && tmp < 0))   {
			gen_err("Invalid expansion factor");
			return(NULL);
		}
		if (Rmp->linear == GR_SQR) tmp = (REAL) sqrt(tmp);
		Rmp->ymax = Rmp->ymax / tmp;				/*  Modify y scale properly	*/
		RbsChgpen(FALSE);								/*  Same color pen				*/
		parms.xmin = x1; parms.xmax = x2;		/*  Plot the region				*/
		RbsPldata(&parms);
		Rmp->ymax = Rmp->ymax * tmp;				/*  Restore scales				*/

/* --- Put the data on the plot (non-blowup case) */
	} else if (key & PLT_OV) {
		if (parms.symbol == -1 && parms.linetype == -1) {		/* Defaults */
			if (Rmp->AutoSymbols)  Rmp->symtype  = (Rmp->symtype%GPT_NUM_SYMBOLS) + 1;
			if (Rmp->AutoLineType) Rmp->linetype = (Rmp->linetype%GPT_NUM_LTYPES) + 1;
		}
		if (parms.pen == -1) RbsChgpen(TRUE);			/* Change pen if needed	*/
		parms.xmin = parms.xmax = 0;						/* Plot whole region		*/
		RbsPldata(&parms);									/* Plot all the data		*/
	}

	return(ibf);												/* Successful plot */
}
