/* genplot.c */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

#define	GENPLOT_NOTICE_NAME		"genplot.notice"

/* #ifdef SHOW_CRITICAL_WARNINGS	*/		/* Print critical warning on startup */

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#if (defined CSET2 || defined MSC60)
	#define INCL_DOS
	#include <os2.h>
#endif
	
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <limits.h>
#include <ctype.h>
#include <math.h>
#include <float.h>
#include <unistd.h>
#include <time.h>
#include <sys/utsname.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#define	GENPLOT_C_SOURCE
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"
#include "tplot.h"
#include "genplot.h"							/* My function definitions */
#include "gptxtrn.h"							/* External call functions	*/
#include "gptdef.h"							/* Common block variables	*/

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define ALLOW_CURVE				0x01		/* Command allows -CURVE option		*/
#define ALLOW_IMPLIED_CURVE	0x02		/* Command has implied -CURVE			*/
#define CREATE_CURVE				0x04		/* Create 2D or 3D curve if not		*/
#define SURFACE_OK				0x08		/* Command can deal with a surface	*/

#define	DEFAULT_CURVE_SIZE	2048		/* If need to create, size used		*/

typedef enum _OPS1 {
	FUNCTION,									/* External function */
	GPT_SETRANGE,								/* Simple commands	*/
	GPT_SPLINE,	GPT_NLSFIT, GPT_FIT,
	GPT_READ,	GPT_WRITE,
	GPT_CONFIG, GPT_ANNOTE
} OPS1;

typedef struct _CMTYPE {					/* Command structure		*/
	const char *command;
	int minlen;
	OPS1 type;
	int (*fnc)(void);
	int flags;
} CMTYPE;

typedef struct _CMTRANS {					/* Translated commands	*/
	const char *command;
	int minlen;
	const char *replace;
} CMTRANS;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
PRIVATE int gpt_do_simplefncs(OPS1 key);

static int GptConfig(int key);
#ifdef CSET2
	PRIVATE int gpt_do_view(void);
#endif
PRIVATE int gpt_do_help(void);
PRIVATE int gpt_do_help_list(void);
PRIVATE int gpt_do_status(void);
PRIVATE int gpt_do_parms(void);
PRIVATE int gpt_do_reset(void);
PRIVATE int gpt_do_default(void);
PRIVATE int gpt_do_quit(void);
PRIVATE int gpt_do_return(void);
PRIVATE int gpt_do_null(void);
PRIVATE int gpt_do_axis(void);
PRIVATE int gpt_do_symbol(void);
PRIVATE int gpt_do_symsize(void);
PRIVATE int gpt_do_linetype(void);
PRIVATE int gpt_do_npoint(void);
PRIVATE int gpt_do_ids(void);
PRIVATE int gpt_do_identify(void);
PRIVATE int gpt_do_autoids(void);
PRIVATE int gpt_do_scope(void);
PRIVATE int gpt_do_plot(void);
PRIVATE int gpt_do_overlay(void);
PRIVATE int gpt_do_archive(void);
PRIVATE int gpt_do_retrieve(void);
PRIVATE int gpt_do_exchange(void);
PRIVATE int gpt_do_region(void);
PRIVATE int gpt_do_autorange(void);
PRIVATE int gpt_do_labels(void);
PRIVATE int gpt_do_autox(void);
PRIVATE int gpt_do_autoy(void);
PRIVATE int gpt_do_plx(void);
PRIVATE int gpt_do_ply(void);
PRIVATE int gpt_do_logaxis(void);
PRIVATE int gpt_do_xtop(void);
PRIVATE int gpt_do_yright(void);
PRIVATE int gpt_do_force(void);
PRIVATE int gpt_do_autoaxis(void);
PRIVATE int gpt_do_boxmode(void);
PRIVATE int gpt_do_subticks(void);
PRIVATE int gpt_do_inticks(void);
PRIVATE int gpt_do_axctrl(void);
PRIVATE int gpt_do_create(void);
PRIVATE int gpt_do_create_surface(void);
PRIVATE int gpt_do_show(void);
PRIVATE int gpt_do_version(void);
PRIVATE int gpt_do_push(void);
PRIVATE int gpt_do_pop(void);

PRIVATE int gpt_do_3Dview(void);				/* 3D functions */
PRIVATE int gpt_do_3Drotate(void);
PRIVATE int gpt_do_3Dtilt(void);
PRIVATE int gpt_do_3Dmesh(void);
PRIVATE int gpt_do_3Dresol(void);
PRIVATE int gpt_do_3Dhidden(void);
PRIVATE int gpt_do_3Dbox(void);

PRIVATE void gpt_link_vars(int itype);

PRIVATE int GetAxisName(void);
PRIVATE int CheckForCurve(char *Curve, const char *Default, int flags);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of global externals    */
/* ------------------------------- */
EXPORT GPTTYPE *Gpt = NULL;
EXPORT CURVE	*GptCurve=NULL;		/* Points to a curve structure	*/
EXPORT SURFACE	*GptSurface=NULL;		/* Genplot surface					*/

char		GptUserModule[PATH_MAX] = "";
LOGICAL (*GptUserCmd)  (int key, char *cmd, char *Curve) = NULL;
LOGICAL (*GptUserFnc)  (char *Curve)                     = NULL;
LOGICAL (*GptUserRead) (char *Filename, char *Curve)     = NULL;
LOGICAL (*GptUserWrite)(char *Filename, char *Curve)     = NULL;

char		GptMainCurve[VARNAME_STR_SIZE];		/* Default main curve				*/
char		GptUseCurve[VARNAME_STR_SIZE];		/* Pointer to alternate				*/

#if (defined OS2 || defined NT)
	EXPORT char GptSearchPath[LONG_STR_SIZE] = "";
	EXPORT char GptSearchExts[DFLT_STR_SIZE] = ".dat;.data";
#else
	EXPORT char GptSearchPath[LONG_STR_SIZE] = "";
	EXPORT char GptSearchExts[DFLT_STR_SIZE] = ".dat:.data";
#endif

EXPORT int (*GptRumpLink)(void) = NULL;

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
IMPORT const char GenplotRevisionLevel[];			/* Defined in serial.c */
IMPORT const char GenplotLinkDate[];				/* Defined in serial.c */
IMPORT const REAL GptVersionNumber;					/* Defined in serial.c */
IMPORT const char GenplotRevisionDate[];			/* Defined in revision.h */

/* ============================================================================
-- Usage:  int Genplot(char *default);
--
-- Inputs: default - symbolic name of a curve in GV memory
--
-- Output: none
--
-- Note: It is assumed that the current X,Y and NPT have been established as a
--       curve already by either GENPLT or GENPLT2.  This routine does not
--       handle any of that operation.
--
-- LARRY DOOLITTLE 12/15/82
-- MIKE THOMPSON   12/10/88
============================================================================ */
/* --------------- GENPLOT Command Summary  --------------------- */          

/* -------------------------------------------------------------------
-- cmlist_limited is special - only main GENPLOT will accept these
-- commands.  The accept flag is thus irrelevent.
------------------------------------------------------------------- */
PRIVATE const CMTYPE cmlist_limited[] =		/* Help and Status		*/
	{	
#ifdef CSET2
		{"help",			 -4,	FUNCTION, gpt_do_view,		SURFACE_OK},
#endif
		{"help",			 1,	FUNCTION, gpt_do_help,		SURFACE_OK},
		{"?",				 1,	FUNCTION, gpt_do_help_list,SURFACE_OK},
		{"version",		-3,	FUNCTION, gpt_do_version,	SURFACE_OK},
		{"reset",		 5,	FUNCTION, gpt_do_reset,		SURFACE_OK},
		{"default",		 7,	FUNCTION, gpt_do_default,	0},
		{"3d_mode",		 2,	FUNCTION, gpt_do_3d,			0},
		{"2d_mode",		 2,	FUNCTION, gpt_do_2d,			0},
		{"annote:",		 3,	GPT_ANNOTE, NULL,				SURFACE_OK},
		{"annotate:",	-6,	GPT_ANNOTE, NULL,				SURFACE_OK},
		{"genplot:",	-6,	FUNCTION, gpt_do_null,		SURFACE_OK},
		{"xgenplot:",	-6,	FUNCTION, gpt_do_null,		SURFACE_OK},
		{"genplt:",		-3,	FUNCTION, gpt_do_null,		SURFACE_OK},
		{"quit",			 4,	FUNCTION, gpt_do_quit,		SURFACE_OK},
		{"return",		 3,	FUNCTION, gpt_do_return,	SURFACE_OK},
		{NULL,			 0,	FUNCTION, NULL,				0} };
PRIVATE const CMTYPE cmlist1[] =						/* Plot/Data describe */
	{	{"axis",			 2,	FUNCTION, gpt_do_axis,		0},	/* Handled internally */
		{"axes",			-3,	FUNCTION, gpt_do_axis,		0},	/* Handled internally */
		{"plot",			 2,	FUNCTION, gpt_do_plot,		0},	/* Handled internally */
		{"overlay",		 2,	FUNCTION, gpt_do_overlay,	0},	/* Handled internally */
		{"grid",			 4,	FUNCTION, gpt_do_grid,		0},
		{"zoom",			 4,	FUNCTION, gpt_do_zoom,		0},
		{"unzoom",		 3,	FUNCTION, gpt_do_unzoom,	0},
		{"symbol",		 2,	FUNCTION, gpt_do_symbol,	SURFACE_OK},
		{"symsize",		 4,	FUNCTION, gpt_do_symsize,	SURFACE_OK},
		{"symbolsize", -7,	FUNCTION, gpt_do_symsize,	SURFACE_OK},
		{"ltype",		 2,	FUNCTION, gpt_do_linetype,	SURFACE_OK},
		{"linetype",	-5,	FUNCTION, gpt_do_linetype,	SURFACE_OK},
		{"npoint",		 2,	FUNCTION, gpt_do_npoint,	SURFACE_OK},
		{"cursor",		 3,	FUNCTION, gpt_do_cursor,	ALLOW_IMPLIED_CURVE},
		{"ids",			 3,	FUNCTION, gpt_do_ids,		SURFACE_OK | ALLOW_IMPLIED_CURVE},
		{"identify",	 5,	FUNCTION, gpt_do_identify,	SURFACE_OK},
		{"legend",		-3,	FUNCTION, gpt_do_identify,	SURFACE_OK},
		{"autoids",		 6,	FUNCTION, gpt_do_autoids,	SURFACE_OK},
		{"scope",		 5,	FUNCTION, gpt_do_scope,		0},
		{NULL,			 0,	FUNCTION, NULL,					0} };
PRIVATE const CMTYPE cmlist2[] =						/* Read/Simple Manipulation */
	{	{"read",			 2,	GPT_READ, NULL,				0},	/* curve in code */
		{"get",			-3,	GPT_READ, NULL,				0},	/* curve in code */
		{"write",		 2,	GPT_WRITE, NULL,				0},	/* curve in code */
		{"save",			-3,	GPT_WRITE, NULL,				0},	/* curve in code */
		{"status",		 2,	FUNCTION, gpt_do_status,	ALLOW_IMPLIED_CURVE | SURFACE_OK},
		{"archive",		 4,	FUNCTION, gpt_do_archive,	0},
		{"retrieve",	 4,	FUNCTION, gpt_do_retrieve,	0},
		{"show",			 2,	FUNCTION, gpt_do_show,		ALLOW_IMPLIED_CURVE | SURFACE_OK},
		{"exchange",	 2,	FUNCTION, gpt_do_exchange,	ALLOW_IMPLIED_CURVE},
		{"trade",		-5,	FUNCTION, gpt_do_exchange,	ALLOW_IMPLIED_CURVE},
		{"sort",			 4,	FUNCTION, gpt_do_sort,		ALLOW_IMPLIED_CURVE},
		{"fix_grid",	 5,	FUNCTION, gpt_do_fixgrid,	ALLOW_IMPLIED_CURVE},
		{"cull_data",	 4,	FUNCTION, gpt_do_cull,		ALLOW_IMPLIED_CURVE},
		{"edit_data",	 4,	FUNCTION, gpt_do_editdata,	ALLOW_IMPLIED_CURVE},
		{NULL,			 0,	FUNCTION, NULL,				0} };
PRIVATE const CMTYPE cmlist3[] =						/* Axis labelling/range control */
	{	{"setup",		 5,	FUNCTION, gpt_do_setup,		SURFACE_OK},
		{"parms",		 3,	FUNCTION, gpt_do_parms,		SURFACE_OK},
		{"parameters",	-4,	FUNCTION, gpt_do_parms,		SURFACE_OK},
		{"region",		 3,	FUNCTION, gpt_do_region,	SURFACE_OK},
		{"autorange",	 5,	FUNCTION, gpt_do_autorange,SURFACE_OK},
		{"set",			-3,	FUNCTION, gpt_do_region,	SURFACE_OK},
		{"scale",		-3,	FUNCTION, gpt_do_region,	SURFACE_OK},
		{"range",		-5,	FUNCTION, gpt_do_region,	SURFACE_OK},
		{"label",		 3,	FUNCTION, gpt_do_labels,	SURFACE_OK},
		{"lable",		-5,	FUNCTION, gpt_do_labels,	SURFACE_OK},	/* Lisa's misspelling */
		{"autox",		 5,	FUNCTION, gpt_do_autox,		SURFACE_OK},
		{"aux",			-3,	FUNCTION, gpt_do_autox,		SURFACE_OK},
		{"autoy",		 5,	FUNCTION, gpt_do_autoy,		SURFACE_OK},
		{"auy",			-3,	FUNCTION, gpt_do_autoy,		SURFACE_OK},
		{"plx",			 3,	FUNCTION, gpt_do_plx,		SURFACE_OK},
		{"ply",			 3,	FUNCTION, gpt_do_ply,		SURFACE_OK},
		{"logaxis",		 3,	FUNCTION, gpt_do_logaxis,	SURFACE_OK},
		{"logarithm",	-5,	FUNCTION, gpt_do_logaxis,	SURFACE_OK},
		{"xtop",			 2,	FUNCTION, gpt_do_xtop,		SURFACE_OK},
		{"yright",		 2,	FUNCTION, gpt_do_yright,	SURFACE_OK},
		{"force",		 4,	FUNCTION, gpt_do_force,		SURFACE_OK},
		{"autoaxis",	 5,	FUNCTION, gpt_do_autoaxis,	SURFACE_OK},
		{"autoaxes",	-7,	FUNCTION, gpt_do_autoaxis,	SURFACE_OK},
		{"boxmode",		 3,	FUNCTION, gpt_do_boxmode,	SURFACE_OK},
		{"subticks",	 4,	FUNCTION, gpt_do_subticks,	SURFACE_OK},
		{"inticks",		 3,	FUNCTION, gpt_do_inticks,	SURFACE_OK},
		{"axctrl",		 3,	FUNCTION, gpt_do_axctrl,	SURFACE_OK},
		{"axcontrol",	-4,	FUNCTION, gpt_do_axctrl,	SURFACE_OK},
		{"axiscontrol",-5,	FUNCTION, gpt_do_axctrl,	SURFACE_OK},
		{"setrange",	-8,	GPT_SETRANGE, NULL,			0},
		{NULL,			 0,	FUNCTION, NULL,				0} };
PRIVATE const CMTYPE cmlist4[] =						/* Analysis, Transform, Create */
	{	{"create",		 2,	FUNCTION, gpt_do_create,	ALLOW_CURVE | CREATE_CURVE},
		{"lsqfit",		 4,	FUNCTION, gpt_do_lsqfit,	ALLOW_IMPLIED_CURVE},
		{"fit",			 3,	GPT_FIT,		NULL,				ALLOW_IMPLIED_CURVE},
		{"nlsfit",		 -4,	GPT_NLSFIT,	NULL,				ALLOW_IMPLIED_CURVE},
		{"spline",		 -6,	GPT_SPLINE, NULL,				ALLOW_IMPLIED_CURVE},
		{"transform",	 2,	FUNCTION, gpt_do_transform,ALLOW_IMPLIED_CURVE},
		{"matrix",      6,   FUNCTION, gpt_do_matrix,   ALLOW_IMPLIED_CURVE | SURFACE_OK},
		{"user",			 4,	FUNCTION, gpt_do_user,		0},
		{"load",			 4,	FUNCTION, gpt_do_load,		SURFACE_OK},
		{"configure",	 4,	GPT_CONFIG, NULL,				SURFACE_OK},
		{"push_state",	 4,	FUNCTION, gpt_do_push,		SURFACE_OK},
		{"pop_state",	 3,	FUNCTION, gpt_do_pop,		SURFACE_OK},
		{NULL,			 0,	FUNCTION, NULL,				0} };
PRIVATE const CMTYPE cmlist5[] =						/* 3D specific commands */
	{	{"view",			 4,	FUNCTION, gpt_do_3Dview,	SURFACE_OK},
		{"rotate",		 3,	FUNCTION, gpt_do_3Drotate,	SURFACE_OK},
		{"tilt",			 4,	FUNCTION, gpt_do_3Dtilt,	SURFACE_OK},
		{"mesh",			 4,	FUNCTION, gpt_do_3Dmesh,	SURFACE_OK},
		{"resolution",	 5,	FUNCTION, gpt_do_3Dresol,	SURFACE_OK},
		{"hidden_lines",3,	FUNCTION, gpt_do_3Dhidden,	SURFACE_OK},
		{"3dbox",		 3,	FUNCTION, gpt_do_3Dbox,		0},
/*		{"surfaces",	 4,	FUNCTION, gpt_do_3Dsurfaces,SURFACE_OK},	*/
		{"3d_grid",		 4,	FUNCTION, gpt_do_3Dgrid,	ALLOW_CURVE},
		{NULL,			 0,	FUNCTION, NULL,				0} };
/* GRID, FUNCTION, Z_SEARCH, CONTOUR, SHOW_SURFACE */

PRIVATE const CMTRANS cmtrans[] =					/* Aliased commands */
	{	{"digitize",			 3,	"read %digitize"},
		{"nlsfit",				 3,	"fit nlsfit"},
		{"fft",					 3,	"transform fft"},
		{"smooth",				 2,	"transform smooth"},
		{"smooth_fft",			 7,	"transform fft_smooth"},
		{"fft_smooth",			-5,	"transform fft_smooth"},
		{"gauss_smooth",		 7,	"transform gauss_smooth"},
	   {"smooth_gaussian",	-8,	"transform gauss_smooth"},
		{"filter_fft",			 6,	"transform filter_fft"},
		{"fft_filter",			-5,	"transform filter_fft"},
		{"autocorrelation",	 5,	"transform autocorrelation"},
		{"correlation",		 4,	"transform correlation"},
		{"convolution",		 4,	"transform convolution"},
		{"deconvolution",		 6,	"transform deconvolution"},
		{"deconvolve",		  -10,	"transform deconvolve"},
		{"autocorrelate",	  -13,	"transform autocorrelate"},
		{"correlate",			-9,	"transform correlate"},
		{"convolve",			-8,	"transform convolve"},
		{NULL,					 0,	NULL} };

PRIVATE int	ReturnCode=0;

/* ===========================================================================
-- Returns: 0xHHLL - LL is the normal return code.  Values normally given as
--                      EXIT_FAILURE (-fail option)
--                      EXIT_SUCCESS (-succeed and default)
--                      user value   (-rcode option)
--                   HH is the source of the exit.
--                      00 => return via a "RETURN" command
--                      01 => return via a "QUIT" command
=========================================================================== */
#ifdef SHOW_CRITICAL_WARNINGS

static long SecondsSinceCompile(void) {

	struct tm tptr;
	char monthstr[8];
	static char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
									 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

	sscanf(__DATE__, "%s %d %d", monthstr,      &tptr.tm_mday, &tptr.tm_year);
	sscanf(__TIME__, "%d:%d:%d", &tptr.tm_hour, &tptr.tm_min,  &tptr.tm_sec);
	tptr.tm_year -= 1900;
	tptr.tm_wday  = tptr.tm_yday  = tptr.tm_isdst = 0;

	for (tptr.tm_mon=0; tptr.tm_mon<11; tptr.tm_mon++) 
		if (stricmp(monthstr, months[tptr.tm_mon]) == 0) break;

	return( (long) difftime(time(NULL), mktime(&tptr)) );
}

#endif

int Genplot(char *tokin) {

	int	rcode;										/* Random integer code */
	char	token[DFLT_STR_SIZE], *prompt;
	static LOGICAL FirstTime=TRUE;

	CMTYPE *citem;										/* Item found in search */

/* ------------------
-- Code start
------------------ */
	strcpy(GptMainCurve, "$PLOT");					/* Default name				*/
	if (tokin != NULL && *tokin != '\0')			/* Do we have otherwise?	*/
		strcpy(GptMainCurve, tokin);
			
	if (FirstTime) {										/* Is this the first time */
		FILE *funit;										/* For the welcome file */
		char filename[PATH_MAX];						/* And file/text output	*/

		FirstTime = FALSE;								/* So now we've done it */

		CONInitialize();									/* Make sure all initialized */
		LexInitialize();									/* All the way up */
		PlotInitialize();
		LexSystem(U_INIT, NULL);
		PlotSystem(U_INIT, NULL, NULL);

 		SysResolveDyntName(filename, GENPLOT_NOTICE_NAME, sizeof(filename));
 		if ( (funit=fopen(filename, "r")) != NULL) {
 		   while (fgets(filename, sizeof(filename), funit) != NULL) 
 			   TTYputs(filename);
 		   fclose(funit);
		}

		GptConfig(U_INIT);
		if ( (Gpt = (GPTTYPE *) malloc(sizeof(GPTTYPE))) == NULL) panic;
		Gpt->LastGpt = NULL;								/* No earlier copies */
		Gpt->push_status = 0;
		gpt_do_version();									/* Give revision information */
		GptReset(TRUE);									/* And reset Genplot */

#ifdef REAL_IS_DOUBLE
		ERRprintf("INFO: Double precision beta version - report problems to mot1@cornell.edu\n");
#endif
		
#ifdef SHOW_CRITICAL_WARNINGS
		if (SecondsSinceCompile() < 7*24*60*60)
			TTYprintf(
				" I have added semaphores to try to protect the drawing thread\n"
				" in DEV PM.  This should eliminate the PM corruption and crash\n"
				" of the graph when redrawing.  If you still have problems with\n"
				" the graph getting corrupted, let me know and I'll try again.\n"
				"\n");
#endif

	}

	SysSetHelpModule("genplot");						/* Set Help module */

/* ----------------------------------------------------------------------------
-- > > > Come here for processing of next command
---------------------------------------------------------------------------- */
	while (TRUE) {

		prompt = (! Gpt->mode_3d) ? "GENPLOT: " : "GENPLOT[3D]: " ;
		if (! LexGetTokenP(token, sizeof(token), prompt)) continue;
		if (LexAlias(token, sizeof(token))) continue;	/* Check for alias */

		strcpy(GptUseCurve, GptMainCurve);					/* Default use curve */

		rcode = NOTKNOWN;
		if (GptUserCmd != NULL) rcode = (*GptUserCmd)(0, token, GptUseCurve);

		if (rcode == NOTKNOWN) {
			if ( (citem = LexCmdl(token, cmlist_limited, sizeof(CMTYPE))) != NULL) {
				GptLinkXYZ(GptUseCurve);						/* Relink X,Y,Z */
				if (citem->type == FUNCTION) {
					rcode = (*citem->fnc)();
				} else {
					rcode = gpt_do_simplefncs(citem->type);
				}
			} else if ( (rcode = GptMainCommands(0, token)) == NOTKNOWN) {
				GptLinkXYZ(GptMainCurve);						/* Relink X,Y,Z */
				if (PlotSystem(0,token,0) || LexSystem(0,token)) rcode = OKAY;
			}
		}
		if (rcode == UNIMPLEMENTED) {
			ERRprintf("ERROR: Sorry.  %s is not implemented yet\n", token);
		} else if (rcode == RETURN) {
			return(ReturnCode);
		} else if (rcode == NOPLOTTER) {
			ERRprintf("ERROR: %s requires a plotting device.  Use DEV command first.\n", token);
		} else if (rcode != OKAY && rcode != NOMORE) {
			ERRprintf("ERROR: %s command was not recognized/bad\n", token);
		}
		if (rcode != OKAY) {
			if (SysChkBreak(FALSE)) ERRputs("WARNING: GENPLOT command aborted by ^C\n");
			LexFlush();
		}
	}
	return(0);										/* Input stream closed */
}

/* ===========================================================================
-- Actual processing of the commands - lookup - performed here.  This has
-- the subset of commands that do not terminate system.
--
-- Returns: NOTKNOWN on any unrecognized operation.
=========================================================================== */
int GptMainCommands(int key, char *token) {

	int rcode;
	CMTYPE *citem;										/* Item found in search */
	CMTRANS *ctrans;									/* Item found in search */

	if (key == 0) {
								 citem = LexCmdl(token, cmlist1, sizeof(CMTYPE));
		if (citem == NULL) citem = LexCmdl(token, cmlist2, sizeof(CMTYPE));
		if (citem == NULL) citem = LexCmdl(token, cmlist3, sizeof(CMTYPE));
		if (citem == NULL) citem = LexCmdl(token, cmlist4, sizeof(CMTYPE));
		if (citem == NULL) citem = LexCmdl(token, cmlist5, sizeof(CMTYPE));
		if (citem != NULL) {
			rcode = NOMORE;											/* Assume error */
			if (CheckForCurve(GptUseCurve, GptMainCurve, citem->flags) == 0) {
				GptLinkXYZ(GptUseCurve);							/* Relink X,Y,Z */
				if (! (citem->flags & SURFACE_OK) && GptCurve == NULL) {
					ERRprintf("ERROR: %s cannot deal with a SURFACE descriptor (%s)\n", citem->command, GptUseCurve);
				} else if (citem->type == FUNCTION) {
					rcode = (*citem->fnc)();
				} else {
					rcode = gpt_do_simplefncs(citem->type);
				}
			}
		} else if ( (ctrans = LexCmdl(token, cmtrans, sizeof(CMTRANS))) != NULL) {
			LexInsText(ctrans->replace);
			rcode = OKAY;
		} else {
			rcode = NOTKNOWN;
		}
	} else if (key == U_HELP) {
		LexCmdlPrint(cmlist1, sizeof(CMTYPE), "Plotting and Data Descriptions:");
		LexCmdlPrint(cmlist2, sizeof(CMTYPE), "Data Reading - Simple Manipulation:");
		LexCmdlPrint(cmlist3, sizeof(CMTYPE), "Axis Labeling and Range Control:");
		LexCmdlPrint(cmlist4, sizeof(CMTYPE), "Analysis, Transforms, Creation:");
		LexCmdlPrint(cmlist5, sizeof(CMTYPE), "3D specific commands:");
		rcode = OKAY;
	} else if (key < 0) {
		rcode = OKAY;
	} else {
		rcode = NOTKNOWN;
	}
	return(rcode);
}


/* ===========================================================================
-- Routine takes a curve name and (1) obtains a pointer to it and (2) links
-- the variables X,Y,Z, NPT etc.
--
-- Returns: 0 - successful linking in the requested curve
--          1 - successful linking in GptMainCurve, but request failed
--          2 - Curve failed, GptMainCurve failed, relinked GptMainCurve
--          3 - Everything failed, unable to link anything useful
--         -1 - Variable existed, but was not a CURVE or SURFACE
--
-- If the curve specified by *Curve does not exist (as if it had been
-- deleted), then try to link GptMainCurve.  If this fails, then reallocate
-- GptMainCurve as a default sized 2 or 3d curve.
=========================================================================== */
int GptLinkXYZ(char *Curve) {

	int itype, rcode=0;
	CURVE **aptr=NULL;
	char *cname;							/* Name of curve that is linked - GVValidateGenplotVars() */
	
	if (GVGetInfo(Curve, &itype, (void **) &aptr) &&
		(itype == GV_2DCURVE || itype == GV_3DCURVE || itype == GV_SURFACE) ) {
		cname = Curve;
		rcode = 0;
	} else if (GVGetInfo(GptMainCurve, &itype, (void **) &aptr)) {
		cname = GptMainCurve;
		rcode = 1;
	} else {
		ERRprintf("AARGH: Main curve appears to have disappeared - trying to reallocate\n");
		if (Gpt->mode_3d) {
			if (! GVAlloc3DCurve(GptMainCurve, GVF_USER, DEFAULT_CURVE_SIZE)) return(3);
		} else {
			if (! GVAlloc2DCurve(GptMainCurve, GVF_USER, DEFAULT_CURVE_SIZE)) return(3);
		}
		if (! GVGetInfo(GptMainCurve, &itype, (void **) &aptr)) return(3);
		cname = GptMainCurve;
		rcode = 2;
	}

	if (itype == GV_2DCURVE || itype == GV_3DCURVE) {
		GptCurve = *aptr;													/* Curve structure */
		GptSurface = NULL;
		GVLinkArray("X", GVF_INTERNAL, GptCurve->x, GptCurve->nptmax, &GptCurve->npt);
		GVLinkArray("Y", GVF_INTERNAL, GptCurve->y, GptCurve->nptmax, &GptCurve->npt);
		if (itype == GV_3DCURVE) {
			GVLinkArray("Z", GVF_INTERNAL, GptCurve->z, GptCurve->nptmax, &GptCurve->npt);
		} else {
			GVDeallocate("Z");
		}
		GVLinkInt("NPT", GVF_INTERNAL, &GptCurve->npt);
		GVValidateGenplotVars("curve", "NPT", cname);
		GVDeallocate("NCOL");
		GVDeallocate("NROW");
		GVLinkString("IDS", GVF_INTERNAL,  GptCurve->ids, sizeof(GptCurve->ids));
	} else if (itype == GV_SURFACE) {
		GptSurface = *( (SURFACE **) aptr);
		GptCurve   = NULL;
		GVLinkArray("X", GVF_INTERNAL, GptSurface->x, GptSurface->ncolmax, &GptSurface->ncol);
		GVLinkArray("Y", GVF_INTERNAL, GptSurface->y, GptSurface->nrowmax, &GptSurface->nrow);
		GVLinkArray("Z", GVF_INTERNAL, GptSurface->z, GptSurface->nptmax,  &GptSurface->npt);
		GVLinkInt("NCOL", GVF_INTERNAL, &GptSurface->ncol);
		GVLinkInt("NROW", GVF_INTERNAL, &GptSurface->nrow);
		GVLinkInt("NPT",  GVF_INTERNAL, &GptSurface->npt);
		GVLinkString("IDS", GVF_INTERNAL,  GptSurface->ids, sizeof(GptSurface->ids));
	} else {
		rcode = -1;
	}

	return(rcode);
}

/* ============================================================================
--     Routine to set the range for plot
--
--     Usage: CALL SETRNG$
--	     CALL SETRNG$2(IX,IY)
--
--     Output: Sets the common blocks
--             IX,IY - index into RMINS etc. for X and Y axis currently active
============================================================================ */
void GptSetRange(void) {
	
	int ix, iy;									/* Index for X, index for Y */

	ix = (Gpt->BoxMode) ? 2*Gpt->plx    : BOTTOM;
	iy = (Gpt->BoxMode) ? 2*Gpt->ply +1 : LEFT;

	Gpt->xmin = Gpt->rmins[ix];    Gpt->xmax = Gpt->rmaxs[ix];
	Gpt->ymin = Gpt->rmins[iy];    Gpt->ymax = Gpt->rmaxs[iy];
	Gpt->zmin = Gpt->rmins[ZAXIS]; Gpt->zmax = Gpt->rmaxs[ZAXIS];
	PlotSetRange(Gpt->xmin, Gpt->xmax, Gpt->ymin, Gpt->ymax);
	
	gpt_link_vars(0x01);
	return;
}

/* ============================================================================
-- Subroutine to handle the BDAT_COMMON settings for the program.  Resets
-- all changable parameters in the common block.
--
-- Usage: CALL RST$GENP
--
-- Inputs: none
--
-- Output: Reset common block variables to initial values
============================================================================ */
void GptReset(LOGICAL FullReset) {

	int i;
	static const CREATE createdflt = {
		{"@min(x)", "@max(x)", "200", "0", "sin(x)"},
		{"@min(x)", "@max(x)", "@min(y)", "@max(y)", "-1", "-1", "x*x-y*y"} };

	Gpt->AutoSymbols  = FALSE;				/* No auto changing symbols	*/
	Gpt->AutoLineType = FALSE;				/* No auto changing linetypes */
	Gpt->AutoIDs      = FALSE;				/* No automatic IDS				*/
	Gpt->ForceRegions = 0;
	Gpt->MinorTicks   = TRUE;
	Gpt->InTicks      = TRUE;
	Gpt->BoxMode      = TRUE;
	Gpt->AutoAxes	   = TRUE;
	Gpt->npoint       = 1;
	Gpt->linetype = Gpt->linetypestart = 0;	/* Current/initialized line type */
	Gpt->symtype  = Gpt->symtypestart  = 3;	/* Initial symbol is type 3		*/

	Gpt->plx = 0;
	Gpt->ply = 0;
	Gpt->AutoFlag = 0x001F;					/* All axes autoscaled */
	Gpt->yright   = IS_OFF;
	Gpt->xtop     = IS_OFF;
	Gpt->symsiz   = 0.14f;					/* Default symbol size */
	Gpt->xmin = Gpt->ymin = 0.0f;
	Gpt->xmax = Gpt->ymax = 1.0f;

	strcpy(BottomTitle, "X Axis");
	strcpy(TopTitle,    "Extra X Axis");
	strcpy(LeftTitle,   "Y Axis");
	strcpy(RightTitle,  "Extra Y Axis");
	strcpy(ZTitle,		  "Z Axis");

	strcpy(Gpt->XYZ_descriptor[0], "Data column 1");
	strcpy(Gpt->XYZ_descriptor[1], "Data column 2");
	strcpy(Gpt->XYZ_descriptor[2], "Data column 3");

	for (i=5; i--;) {
		Gpt->color[i]   = 0;								/* Default colors */
		Gpt->rmins[i]   = 0.0f;
		Gpt->rmaxs[i]   = 1.0f;
		Gpt->udx[i]     = 0.0f;
		Gpt->udx2[i]    = 0.0f;
		Gpt->umx[i]     = 0;
		Gpt->uys[i]     = Gpt->uxs[i]   = 0.0f;	/* Start at 0,0 on axes */
		Gpt->uxl[i]     = 1.0f;							/* All axes full length	*/
		Gpt->logtype[i] = FALSE;						/* And no logarithmic labelling */

		Gpt->do_user_labels[i] = FALSE;				/* Disable user specified axis labeling */
		*Gpt->user_labels[i].major = '\0';
		*Gpt->user_labels[i].minor = '\0';
		*Gpt->user_labels[i].text  = '\0';
		*Gpt->user_labels[i].csize = '\0';
	}
	Gpt->uxs[RIGHT] = Gpt->uys[TOP] = 1.0f;		/* Top/Right axes don't really start at 0,0 */

	Gpt->axmode[TOP]   = Gpt->axmode[BOTTOM] = AXIS_TICKS_INWARD;
	Gpt->axmode[LEFT]  = Gpt->axmode[RIGHT]  = AXIS_Y_TYPE|AXIS_TICKS_INWARD|AXIS_VERTICAL;
	Gpt->axmode[ZAXIS] =                      AXIS_TICKS_INWARD;

	Gpt->OpMode = 0;										/* No options */

	Gpt->mode_3d = FALSE;
	Gpt->tilt    = -60.0f;								/* 3D orientations */
	Gpt->rotate  =  30.0f;
	Gpt->skew    =   0.0f;
	Gpt->view_d  =   0.0f;
	Gpt->mesh[0] = Gpt->mesh[1] = 25;				/* 25 lines along structure */
	Gpt->resolution[0] = Gpt->resolution[1] = 100;	/* 100 points				 */
	Gpt->HideLines = HIDDEN_OFF;

	Gpt->createcall = createdflt;						/* Direct call of create() */
	Gpt->createfnc  = createdflt;						/* In a plot -f form			*/
	Gpt->createfit  = createdflt;						/* In a plot -fit form		*/
	Gpt->createuse = &Gpt->createcall;

	if (FullReset) {
		GptFreeUserDLL(TRUE);							/* Free user module */
	}

	gpt_link_vars(0xFF);
	return;
}

/* ============================================================================
-- Subroutine to shut down GENPLOT and associated processes.
--
-- Usage: void GptShutDown()
--
-- Inputs: none
--
-- Output: Free's DLL, unconfigures, downs lexp and plot
============================================================================ */
void GptShutDown(void) {
	GptFreeUserDLL(TRUE);								/* Terminate user DLL		*/
	GptConfig(U_QUIT);
	LexSystem(U_QUIT, NULL);
	PlotShutDown();
	return;
}

/* ------------------------------------------------------------------------ */
static char PushHelp[] = 
"\n"
" Command to push current state of internal common block to allow restore\n"
" at a later time.  There are several internal blocks - some easier than\n"
" others to control.  The result has its uses, but care must be taken to\n"
" fully understand.\n"
"\n"
"   PUSH_State [-genplot] [-vars] [-complot] [-all]\n"
"   POP_State\n"
"\n"
"   Options:\n"
"        -genplot  => Saves common block with most drawing definitions\n"
"                     such as axes labels, ranges, modes, etc.\n"
"        -complot  => Saves plot boundaries, some pen characteristics.\n"
"                     If HCOPY enabled, will pause and open a new one\n"
"        -vars     => Equivalent to a set_local.  On pop, all variables\n"
"                     allocated since the set_local will be deallocated.\n"
"                     Changes to existing variables will not be undone\n"
"                     however.\n"
"        -all      => guess\n"
"\n"
"  The default is -all.  Each POP_State will undo the changes.  They may\n"
"  be nested as desired.  Use of PUSH_State and POP_State is primarily for\n"
"  complex macros which want to minimize their impact on user environment.\n"
"\n"
"  Typical use:\n"
"     genplot push_state -genplot -vars\n"
"     reset -genplot\n"
"         ... lots of work ...\n"
"     genplot pop_state\n";

PRIVATE int gpt_do_push(void) {

	GPTTYPE *tmp;
	char token[OPTION_STR_SIZE];
	int status;

	if (LexCheckHelp("Push_State", PushHelp, NULL)) return(OKAY);

	status = 0x00;												/* All by default */
	
	while (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-genplot", 4)) {
			status |= 0x01;
		} else if (LexEqual(token, "-complot", 4)) {
			status |= 0x02;
		} else if (LexEqual(token, "-vars", 4) || LexEqual(token, "-variables", 4)) {
			status |= 0x04;
		} else if (LexEqual(token, "-full", 2)) {
			status |= 0xFF;
		} else {
			ERRprintf("ERROR: Unrecognized PUSH_STATE option ignored: %s\n", token);
		}
	}
	if (status == 0) status = 0xFF;

	if ( (tmp = (GPTTYPE *) malloc(sizeof(GPTTYPE))) == NULL) {
		gen_err("Unable to allocate memory for state push");
		return(NOMORE);
	}

	*tmp = *Gpt;
	tmp->LastGpt = Gpt;
	tmp->push_status = status;
	Gpt = tmp;

	if (status & 0x02) PlotPushState();
	if (status & 0x04) GVSetLocal();

	return(OKAY);
}

/* ------------------------------------------------------------------------ */
PRIVATE int gpt_do_pop(void) {

	GPTTYPE *tmp;
	int status;

	if ( (tmp = Gpt->LastGpt) == NULL) {
		ERRprintf("ERROR: No state currently pushed - but no harm done\n");
	} else {
		status = Gpt->push_status;
		free(Gpt);
		Gpt = tmp;
		if (status & 0x02) PlotPopState();
		if (status & 0x04) GVEndLocal();
	}
	return(OKAY);
}

/* ------------------------------------------------------------------------ */
PRIVATE int gpt_do_version(void) {

	TTYprintf("\n"
"/=============================================================================\\\n"
"|                 GENPLOT [ANSI C Revision %17s]                 |\n"
"|     (c) 1988-2002 Computer Graphic Service, Ltd.  All rights reserved       |\n"
"|                     Serial Number: %-17s                        |\n"
"|                     Revision Date: %-25s                |\n"
"|                     Compile Date:  %-25s                |\n"
"\\=============================================================================/\n"
"\n", GenplotRevisionLevel, SysSerialNumber, GenplotRevisionDate, GenplotLinkDate);
	return(OKAY);	
}

/* ------------------------------------------------------------------------ */
PRIVATE int gpt_do_help_list(void) {
/*	if (isatty(fileno(stdin))) ScrClearAttrib(D_NORMAL); */
	if (GptUserCmd != NULL) (*GptUserCmd)(U_HELP, NULL, NULL);
	LexCmdlPrint(cmlist_limited, sizeof(CMTYPE), "General commands, status and help:");
	LexCmdlPrint(cmlist1, sizeof(CMTYPE), "Plotting and Data Descriptions:");
	LexCmdlPrint(cmlist2, sizeof(CMTYPE), "Data Reading - Simple Manipulation:");
	LexCmdlPrint(cmlist3, sizeof(CMTYPE), "Axis Labeling and Range Control:");
	LexCmdlPrint(cmlist4, sizeof(CMTYPE), "Analysis, Transforms, Creation:");
	LexCmdlPrint(cmlist5, sizeof(CMTYPE), "3D specific commands:");
	PlotSystem(U_HELP, NULL, 0);
	LexSystem(U_HELP, NULL);
	return(OKAY);
}

/* . . . . HELP . . . . */
PRIVATE int gpt_do_help(void) {
	char token[DFLT_STR_SIZE];
	LexGetRest(token, sizeof(token));
	if (SysHelpRequest("genplot", token)) return(OKAY); 
	return(gpt_do_help_list()); 
}

#ifdef CSET2
/* . . . . VIEW HELP . . . . */
PRIVATE int gpt_do_view(void) {
	char path[PATH_MAX], token[DFLT_STR_SIZE], cmdline[PATH_MAX];
	LexGetRest(token, sizeof(token));
	SysResolveDyntName(path, "genplot.inf", sizeof(path));
	sprintf(cmdline, "start view %s %s", path, token);
	SysSystem(cmdline);
	return(OKAY);
}
#endif

/* ------------------------------------------------------------------------ */
PRIVATE int gpt_do_status(void) {

	REAL dxmin,dxmax;

	if (GptCurve != NULL) {
		TTYprintf(" Curve ID: %s\n", GptCurve->ids);
		TTYprintf(" Current data consists of %i points. Max: %i\n", GptCurve->npt, GptCurve->nptmax);
		ArrayMinMax(GptCurve->x, GptCurve->npt, &dxmin, &dxmax);
		TTYprintf("   X minimum = %12.5g maximum = %12.5g\n", (double) dxmin, (double) dxmax);
		ArrayMinMax(GptCurve->y, GptCurve->npt, &dxmin, &dxmax);
		TTYprintf("   Y minimum = %12.5g maximum = %12.5g\n", (double) dxmin, (double) dxmax);
		if (GptCurve->z != NULL) {
			ArrayMinMax(GptCurve->z, GptCurve->npt, &dxmin, &dxmax);
			TTYprintf("   Z minimum = %12.5g maximum = %12.5g\n", (double) dxmin, (double) dxmax);
		}
	} else if (GptSurface != NULL) {
		TTYprintf(" Surface ID: %s \n", GptSurface->ids);
		TTYprintf(" Mesh: %d x %d   Maximum mesh: %d x %d   (X columns, Y rows)\n",
			GptSurface->ncol, GptSurface->nrow, GptSurface->ncolmax, GptSurface->nrowmax); 
		ArrayMinMax(GptSurface->x, GptSurface->ncol, &dxmin, &dxmax);
		TTYprintf("   X minimum = %12.5g maximum = %12.5g\n", (double) dxmin, (double) dxmax);
		ArrayMinMax(GptSurface->y, GptSurface->nrow, &dxmin, &dxmax);
		TTYprintf("   Y minimum = %12.5g maximum = %12.5g\n", (double) dxmin, (double) dxmax);
		ArrayMinMax(GptSurface->z, GptSurface->npt, &dxmin, &dxmax);
		TTYprintf("   Z minimum = %12.5g maximum = %12.5g\n", (double) dxmin, (double) dxmax);
	}
	return(OKAY);
}

/* ------------------------------------------------------------------------ */
PRIVATE int gpt_do_parms(void) {

	int i,j;
	char *ac[5];
	const char *TypeLabs[]={"OFF",  "ON",  "COPY",   "NONLINEAR"};

	TTYputs("\n");
	if (GptUserCmd != NULL) (*GptUserCmd)(U_PARM, NULL, NULL);

/* Set up ac[] to point to appropriate character strings */
	for (i=0;i<5;i++) {
		ac[i] = "Fixed";
		if (Gpt->AutoFlag & 0x001<<i) ac[i] = "Auto";
		if (Gpt->AutoFlag & 0x100<<i) ac[i] = "Window";
	}

	if (! Gpt->BoxMode) TTYputs(" Non-box mode enabled\n");
	TTYprintf(" Plotting X against %s scale and Y against %s scale\n",
		Gpt->plx == 0 ? "Lower" : "Upper", Gpt->ply == 0 ? "Left" : "Right");
	TTYprintf(" Ranging: Bott: %s   Left: %s   Top: %s   Right: %s   Z: %s\n", ac[0],ac[1],ac[2],ac[3], ac[4]);
	TTYprintf(" Top axis: %s     Right axis: %s\n", TypeLabs[Gpt->xtop], TypeLabs[Gpt->yright]);
	TTYprintf(" Bottom:   %s\n", BottomTitle);
	TTYprintf(" Left:     %s\n", LeftTitle);
	TTYprintf(" Top:      %s\n", TopTitle);
	TTYprintf(" Right:    %s\n", RightTitle);
	TTYprintf(" Bottom range: %13.6g%13.6g    Top: %13.6g%13.6g\n", xminb,xmaxb, xmint,xmaxt);
	TTYprintf(" Left range  : %13.6g%13.6g  Right: %13.6g%13.6g\n", yminl,ymaxl, yminr,ymaxr);
	if (Gpt->ForceRegions != 0) 
	  	TTYprintf(" Regions FORCED to specified values for some axes: %#2.2x\n", Gpt->ForceRegions);
	i = (! Gpt->AutoLineType) ? Gpt->linetypestart : -( (Gpt->linetypestart%GPT_NUM_LTYPES) + 1) ;
	j = (! Gpt->AutoSymbols)  ? Gpt->symtypestart  : -( (Gpt->symtypestart%GPT_NUM_SYMBOLS) + 1) ;
	TTYprintf(" Linetype: %3d  Symbol: %4d  Npoint: %3d\n", i,j,Gpt->npoint);
	TTYprintf(" Tilt: %.1f  Rotation: %.1f  Skew: %.1f   View: %.1f\n",
				 Gpt->tilt, Gpt->rotate, Gpt->skew, Gpt->view_d);
	GptConfig(U_PARM);
	TTYputs("\n");
	PlotSystem(U_PARM, NULL, NULL);
	LexSystem (U_PARM, NULL);
	TTYputs("\n");
	return(OKAY);
}


/* ------------------------------------------------------------------------ */
static char ResetHelp[] = 
"\n"
" Command to reset components to known state for clean macro execution.\n"
"\n"
"   RESET [-full | -partial]  [-genplot] [-complot] [-lexp] [-all] [-silent]\n"
"\n"
"   Options:\n"
"        -full | -partial  => Applies to all other options.  The FULL\n"
"                             does a more thorough reset which is very\n"
"                             seldom required.  PARTIAL is the default.\n"
"        -genplot           => Resets the GENPLOT paramters.\n"
"        -complot           => Resets the graphics screen paramters\n"
"        -lexp              => Resets the command process paramerers\n"
"        -all               => Resets all of the processors\n"
"        -silent | -quiet   => Doesn't print out the version information\n"
"\n"
"  The default is -all.  It is generally safe to do a GENPLOT RESET -SILENT at\n"
"  the beginning of all macros to reset the system to a standard configuration.\n"
"  GENPLOT RESET -GENPLOT is sometimes more appropriate.\n"
"\n"
"  Typical use:\n"
"     genplot reset -silent\n"
"     genplot reset -genplot\n"
"  The leading GENPLOT is to ensure we drop back to correct command level\n";

/* ------------------------------------------------------------------------ */
PRIVATE int gpt_do_reset(void) {

	char token[OPTION_STR_SIZE];
	int FullReset = FALSE;
	int type;

/* Check for help request */
	if (LexCheckHelp("Reset", ResetHelp, NULL)) return(OKAY);

	type = 0;
	if (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-full", 2)) {
			FullReset = TRUE;
		} else if (LexEqual(token, "-partial", 2)) {
			FullReset = FALSE;
		} else if (LexEqual(token, "-lexp", 2)) {
			type |= 0x01;
		} else if (LexEqual(token, "-complot", 2)) {
			type |= 0x02;
		} else if (LexEqual(token, "-genplot", 2)) {
			type |= 0x04;
		} else if (LexEqual(token, "-all", 4)) {
			type |= 0xFF;
		} else if (LexEqual(token, "-silent", 2) || LexEqual(token, "-quiet", 2)) {
			type |= 0x80;
		} else {
			ERRprintf("ERROR: Unrecognized RESET option ignored: %s\n", token);
		}
	}
	if (type == 0) type = 0xFF;
	if (type == 0x80) type = 0x7F;						/* No options but silent */

	if (type & 0x04 && GptUserCmd != NULL) (*GptUserCmd)(U_RESET, NULL, NULL);
	if (type & 0x04) GptConfig(U_RESET);
	if (type & 0x01) LexReset(FullReset);				/* Handles LexSystem also	*/
	if (type & 0x02) PlotReset(FullReset);				/* Does PlotSystem also		*/
	if (type & 0x04) GptReset(FullReset);

	if (type == 0xFF) gpt_do_version();					/* Give the revision information */
	return(OKAY);
}

/* ------------------------------------------------------------------------ */
PRIVATE int gpt_do_default(void) {
	char token[VARNAME_STR_SIZE];
	int itype;
	if (! LexGetTokenP(token, sizeof(token), "Switch to curve: ")) return(OKAY);
	LEXESCAPE;
	if (GVGetInfo(token, &itype, NULL)) {
		if ( (itype == GV_2DCURVE) || (itype == GV_3DCURVE) ) {
			strcpy(GptMainCurve, token);
			return(OKAY);
		}
	}
	gen_err2("Specified DEFAULT name is not a curve", token);
	return(NOMORE);
}

/* -------- set return code via command options ---------- */
PRIVATE void set_return_code(void) {
	char token[OPTION_STR_SIZE];

	if (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-fail", 2)) {
			ReturnCode = EXIT_FAILURE;
		} else if (LexEqual(token, "-succeed", 2)) {
			ReturnCode = EXIT_SUCCESS;
		} else if (LexEqual(token, "-rcode", 3)) {
			ReturnCode = LexGetInt(EXIT_SUCCESS, "Return code: ");
		} else {
			ReturnCode = EXIT_FAILURE;
		}
	} else {
		ReturnCode = EXIT_SUCCESS;
	}
	return;
}
		

/* ---------- Return ---------- */					/* RETURN		*/
PRIVATE int gpt_do_return(void) {
	set_return_code();
	return(RETURN);
}

/* ---------- Quit Command  ---------- */			/* QUIT			*/
PRIVATE int gpt_do_quit(void) {
	set_return_code();
	ReturnCode |= 0x0100;								/* Indicate return via QUIT */
	GptShutDown();											/* Shutdown everything		*/
	return(RETURN);
}

/* ----------  NULL  ---------- */					/* GENPLOT		*/
PRIVATE int gpt_do_null(void) {
	return(OKAY);
}

/* ------------------------------------------------------- */
/* . . . . OVERLAY   => imode=0, Draw normal curve . . . . */
/* ------------------------------------------------------- */
PRIVATE int gpt_do_overlay(void) {

	LOGICAL lhold, ltmp;
	lhold = Gpt->AutoAxes;
	Gpt->AutoAxes = FALSE;									/* Fake overlay as PLOT */
	ltmp  = gpt_do_plot();
	Gpt->AutoAxes = lhold;
	return(ltmp);
}


/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
PRIVATE int gpt_do_plot(void) {

	char OptionVar[VARNAME_STR_SIZE+5], token[DFLT_STR_SIZE];
	int  i, itype, rcode;
	char *aptr;
	LOGICAL ltmp;

	strcpy(GptUseCurve, GptMainCurve);					/* Start using main */

	if (LexGetToken(token, sizeof(token))) {			/* Possible curve? */
		if (LexEqual(token, "-CURVE", 3) || LexEqual(token, "-SURFACE", 5)) {
			if (! LexGetTokenP(GptUseCurve, sizeof(GptUseCurve), "Curve name (abort): ")) 
				return(OKAY);
			LEXESCAPE;
			i = 2;
		} else if (LexEqual(token, "-FUNCTION", 2)) {
			i = 1;
		} else if (LexEqual(token, "-FIT", 4)) {
			i = 3;
		} else {
			ltmp = GVGetInfo(token, &itype, NULL);
			if (ltmp && (itype == GV_2DCURVE || itype == GV_3DCURVE || itype == GV_SURFACE)) {
				strcpy(GptUseCurve, token);
				i = 2;
			} else {
				i = 0;
				LexBackup();
			}
		}
		
		if (i == 1 || i == 3) {							/* Create functions */
			Gpt->createuse = (i == 1) ? &Gpt->createfnc : &Gpt->createfit;
			if (! Gpt->mode_3d) {						/* 2D functions */
				strcpy(GptUseCurve, "$$TMP$$");
				strcpy(OptionVar, "FUNCTION:OPTS");
				if (i == 3) LexInsText("fit(x)");
				rcode = gpt_do_create(); 
			} else {
				GVDeallocate("$$S_TMP$$");				/* Make sure not there! */
				strcpy(GptUseCurve, "$$S_TMP$$");
				strcpy(OptionVar, "FUNCTION:OPTS");
				if (i == 3) LexInsText("fit(x,y)");
				LexInsText("$$S_TMP$$");
				rcode = gpt_do_create_surface();
			}
			Gpt->createuse = &Gpt->createcall;		/* And reset now */
			if (rcode != OKAY) return(rcode);
		} else {
			strcat(strcpy(OptionVar, GptUseCurve), ":OPTS");
		}
	} else {
		strcat(strcpy(OptionVar, GptUseCurve), ":OPTS");
	}

	if ( (aptr=GVFindString(OptionVar)) != NULL) LexInsText(aptr);	/* Options */

	GptLinkXYZ(GptUseCurve);										/* Link for plot/axis */
	rcode = GptPlotCurve();
	if (rcode > 0) return(NOMORE);
	if (rcode < 0) return(NOPLOTTER);
	return(OKAY);									/* Otherwise == 0 */
}

/* ---------------------- */
/* . . . . SYMBOL . . . . */
/* ---------------------- */
PRIVATE int gpt_do_symbol(void) {
	char token[DFLT_STR_SIZE];
	int i;

	if (LexGetMathP(token, sizeof(token), "Symbol number for point plotting mode (3)? ")) {
		if (! LexEscape(TRUE)) {
			i = PlotMatchSymbol(token, 3);
			if (Gpt->linetype != 0) gen_warn("Set LTYPE = 0 for symbols");
			Gpt->AutoSymbols = (i < 0);
			Gpt->symtype = Gpt->AutoSymbols ? ( (-i+GPT_NUM_SYMBOLS-2)%GPT_NUM_SYMBOLS) + 1 : i ;
			Gpt->symtypestart = Gpt->symtype;
		}
	}
	return(OKAY);
}

/* . . . . SYMBOL SIZE for symbol plotting . . . . */
PRIVATE int gpt_do_symsize(void) {
	REAL x;
	x = LexGetReal(0.18f, "Symbol size (0.18): ");
	if (! LexEscape(TRUE)) Gpt->symsiz = x;
	return(OKAY);
}

/* . . . . LTYPE . . . . */
PRIVATE int gpt_do_linetype(void) {
	int i;
	i = LexGetInt(1, "Line type (1): ");
	if (! LexEscape(TRUE)) {
		Gpt->AutoLineType = (i < 0);
		Gpt->linetype = Gpt->AutoLineType ? ((-i+GPT_NUM_LTYPES-2)%GPT_NUM_LTYPES) + 1 : i ;
		Gpt->linetypestart = Gpt->linetype;
	}
	return(OKAY);
}

/* ... NPOINT command.  Plot only every so many data point */
PRIVATE int gpt_do_npoint(void) {
	int i;
	i = LexGetInt(1, "Plot every n-th point: ");
	if (! LexEscape(TRUE)) Gpt->npoint = max(1,i);
	return(OKAY);
}


/* ... IDS command - Add symbol with text on screen */
PRIVATE int gpt_do_ids(void) {
	PlotID(Gpt->linetype, Gpt->symtype, -1, 0, GptCurve->ids);
	return(OKAY);
}

/* ... IDENTIFY command - Add symbols and text */
PRIVATE int gpt_do_identify(void) {
	char token[LONG_STR_SIZE];
	
	if (LexGetOption(token, sizeof(token))) {
		LexBackup();
		*token = '\0';
	} else if (! LexGetStrExpr(token, sizeof(token))) {
		LexPromptStr(token, sizeof(token), "Text: ");
	}
	PlotID(Gpt->linetype, Gpt->symtype, -1, 0, token);
	return(OKAY);
}

/* ... AUTOIDS function */
PRIVATE int gpt_do_autoids(void) {
	LOGICAL ltmp;
	ltmp = LexOnOff(TRUE, "Autoids function mode (ON): ");
	if (! LexEscape(TRUE)) Gpt->AutoIDs = ltmp;
	return(OKAY);
}

/* ... SCOPE drawing function */
PRIVATE int gpt_do_scope(void) {
	if (! PlotSystem(1, NULL, NULL)) return(-1);
	PlotDrawScopeFace();
	return(OKAY);
}


/* ============================================================================
-- Function to show a selected portion of the data
--
-- Default    SHOW <xlow> <xhigh>
--
-- Optional:  show [-range | -xrange] <xlow> <xhigh>
--                  -yrange           <ylow> <yhigh>
--                  -zrange           <ylow> <yhigh>  (obviously 3D only)
--                  -irange           <ilow> <ihigh>
--                  -for <condition>
--
-- The -xrange and -yrange are obvious.  The -irange uses point numbers
-- starting from 0 to npt-1.  The <for> is an arbitrary condition similar
-- to the CULL command.
============================================================================ */
static char ShowHelp[] =
" Prints the X,Y data to the screen, possibly in a limited range.  If no\n"
" curve is specified, the main curve is assumed.  If no options are given,\n"
" the function defaults to showing data within a specified X range.\n"
"\n"
"   SHow [<curve>] <xlow> <xhigh>\n"
"   SHow [<curve>] [-options]\n"
"\n"
"   -help | -?             Prints this help message\n"
"\n"
"   -xrange <low> <high>   Limit data to satisfying specified range\n"
"   -yrange <low> <high>   Limit data shown\n"
"   -zrange <low> <high>   Limit data shown\n"
"   -irange <ilow> <imax>  Show only data within index range\n"
"\n"
"   -for <condition>       Show data satisfying a condition\n"
"\n"
" Examples: show / /                /* Show all data\n"
"           show -for sin(x*y)<0.2  /* Complex condition\n";

PRIVATE int gpt_do_show(void) {

	REAL xlow=-REAL_MAX, ylow=-REAL_MAX, zlow=-REAL_MAX,
		  xhigh=REAL_MAX, yhigh=REAL_MAX, zhigh=REAL_MAX;
	int  ilow=0, ihigh=INT_MAX;
	
	REAL *x,*y,*z;
	int   i;
	enum {RANGE, CONDITION, NOTSPEC} type = NOTSPEC;
	char token[DFLT_STR_SIZE];
	GVCMDS  *mathcmds;								/* For "FOR" mode		*/

/* Check for help request */
	if (LexCheckHelp("Show", ShowHelp, NULL)) return(OKAY);

/* Scan for options */
	while (LexGetOption(token, sizeof(token))) {
		switch (LexSelect(token, "-range -xrange -yrange -zrange -irange -for")) {
			case 1:
			case 2:
				type = RANGE;
				xlow  = LexGetReal(-REAL_MAX, "Lower X bound (minimum): ");
				xhigh = LexGetReal( REAL_MAX, "Upper X bound (maximum): ");
				break;
			case 3:
				type = RANGE;
				ylow  = LexGetReal(-REAL_MAX, "Lower Y bound (minimum): ");
				yhigh = LexGetReal( REAL_MAX, "Upper Y bound (maximum): ");
				break;
			case 4:
				type = RANGE;
				zlow  = LexGetReal(-REAL_MAX, "Lower Z bound (minimum): ");
				zhigh = LexGetReal( REAL_MAX, "Upper Z bound (maximum): ");
				break;
			case 5:
				type = RANGE;
				ilow  = LexGetInt(0,       "First point number (0): ");
				ihigh = LexGetInt(INT_MAX, "Last point number (max): ");
				break;
			case 6:
				type = CONDITION;
				if (! LexGetMath(token, sizeof(token))) 
					LexPromptStr(token, sizeof(token), "Condition: ");
				if (*token == '\0' || LexEscape(TRUE)) return(OKAY);
				if ((mathcmds = GVParse(token, NULL)) == NULL) {
					ERRprintf("ERROR: Condition is not a legal math expression\n");
					return(NOMORE);
				}
				break;
			default:
				if (type == NOTSPEC) {			/* Assume old format (like -20) */
					LexBackup();
					goto NoOptions;
				}
				ERRprintf("ERROR: %s is not a recognized SHOW option\n", token);
				return(NOMORE);
		}
	}

/* If no options have been given, assume -xrange and collect now */
NoOptions:
	if (type == NOTSPEC) {						/* No options given */
		type = RANGE;
		xlow  = LexGetReal(-REAL_MAX, "Lower X bound (minimum): ");
		xhigh = LexGetReal( REAL_MAX, "Upper X bound (maximum): ");
	}

/* Okay, now deal with it */
	if (GptCurve == NULL) return(OKAY);
	x = GptCurve->x;
	y = GptCurve->y;
	z = GptCurve->z;
	for (i=0; i < GptCurve->npt; i++) {
		if (type == RANGE) {
			if (i < ilow || i > ihigh || x[i] < xlow || x[i] > xhigh || y[i] < ylow || y[i] > yhigh) continue;
			if (z != NULL && (z[i] < zlow || z[i] > zhigh)) continue;
		} else {
			if (GVEvalCmdsI(mathcmds, i, NULL) <= 0.0) continue;
		}
		if (z == NULL) {
			TTYprintf("I: %5i  X: %14.7g     Y: %14.7g\n", i, x[i], y[i]);
		} else {
			TTYprintf("I: %5i  X: %14.7g     Y: %14.7g     Z: %14.7g\n", i, x[i], y[i], z[i]);
		}
		if (SysChkBreak(FALSE)) return(OKAY);
	}
	TTYputc('\n');
	return(OKAY);
}


PRIVATE int gpt_do_exchange(void) {
	int i,type=0;
	REAL tmp, *x, *y;
	char token[OPTION_STR_SIZE];

	if (LexGetOption(token, sizeof(token))) {
		if (stricmp(token,"-xy") == 0 || stricmp(token,"-yx") == 0)
			type = 0;
		else if (stricmp(token,"-xz") == 0 || stricmp(token,"-zx") == 0)
			type = 1;
		else if (stricmp(token,"-yz") == 0 || stricmp(token,"-zy") == 0)
			type = 2;
		else {
			ERRprintf("ERROR: %s is an unrecognized exchange option\n", token);
			return(NOMORE);
		}
	}

	if (GptCurve != NULL) {
		x = (type == 0 || type == 1) ? GptCurve->x : GptCurve->z;
		y = (type == 0 || type == 2) ? GptCurve->y : GptCurve->z;
		if (x == NULL || y == NULL) {
			gen_err("Z variable invalid currently");
			return(NOMORE);
		}
		i = GptCurve->npt;
		while (i--) { tmp = *x; *(x++) = *y; *(y++) = tmp; }
	}
	return(OKAY);
}


/* . . . . AUTORANGE . . . . */
PRIVATE int gpt_do_autorange(void) {
	char token[OPTION_STR_SIZE];
	int  i;

	if (! LexGetTokenP(token,sizeof(token), 
		 "Autorange: [ALL X Y Z Bottom Left Top Right] (abort): ")) 
	  	return(OKAY);
	LEXESCAPE;

	i = LexSelect(token, "BOTTOM LEFT TOP RIGHT ZAXIS XAXIS YAXIS ALL");
	if (i <= 0) {
		gen_err2("AUTORANGE - Invalid axis specified", token);
		return(NOMORE);
	} else if (i == 8) {
		Gpt->AutoFlag = 0x001F;							/* All auto, none window */
		return(OKAY);
	} else if (i >= 6) {									/* X -> BOTTOM, Y -> LEFT */
	   i -= 5;
	}
	i--;														/* Convert to bit index */
	Gpt->AutoFlag |=  (0x001<<i);
	Gpt->AutoFlag &= ~(0x100<<i);
	return(OKAY);
}


/* . . . . REGION . . . . */
PRIVATE int gpt_do_region(void) {
	char token[OPTION_STR_SIZE];
	int  i, docnt, list[3];
	REAL xmin, xmax;

	i = GetAxisName();											/* Get Axis request */
	LEXESCAPE;

	if (i == -2) return(NOMORE);
	if (i == -1) return(OKAY);

	if (i == -3) {													/* All automatic */
		Gpt->AutoFlag = 0x001F;	
		return(OKAY);

	} else if (i == -4) {										/* Do 2D set		*/
		docnt = 2;
		list[1] = 0;												/* Do X axis		*/
		list[0] = 1;												/* Do Y axis		*/

	} else if (i == -5) {										/* Full set of 3D */
		docnt = 3;						
		list[2] = 0;												/* Do X axis		*/
		list[1] = 1;												/* Do Y axis		*/
		list[0] = 4;												/* And finally Z	*/

	} else {															/* Requesting a single */
		docnt = 1;
		list[0] = i;
	}

	while (docnt--) {
		i = list[docnt];
		
		xmin = Gpt->rmins[i]; xmax = Gpt->rmaxs[i];
		if (LexGetTokenP(token, sizeof(token),
			  " [ <lower limit> | window | auto ] (unchanged): ")) {
			LEXESCAPE;
			if (LexEqual(token, "AUTO", 2)) {
				Gpt->AutoFlag |=  (0x001<<i);						/* Turn on  AUTO	 */
				Gpt->AutoFlag &= ~(0x100<<i);						/* Turn off WINDOW */
				continue;
			} else if (LexEqual(token, "WINDOW", 3)) {
				Gpt->AutoFlag |=  (0x100<<i);						/* Turn off AUTO	 */
				Gpt->AutoFlag &= ~(0x001<<i);						/* Turn on  WINDOW */
				if (i == 0 || i == 2) {
					if (Gpt->AutoFlag & 0x200) Gpt->AutoFlag = (Gpt->AutoFlag & ~0x200) | 0x002;
					if (Gpt->AutoFlag & 0x800) Gpt->AutoFlag = (Gpt->AutoFlag & ~0x800) | 0x008;
				} else {
					if (Gpt->AutoFlag & 0x100) Gpt->AutoFlag = (Gpt->AutoFlag & ~0x100) | 0x001;
					if (Gpt->AutoFlag & 0x400) Gpt->AutoFlag = (Gpt->AutoFlag & ~0x400) | 0x004;
				}
				return(OKAY);
			}
			LexBackup();
			xmin = LexGetReal(xmin, "Lower range limit: ");
			LEXESCAPE;
		}
		LEXESCAPE;
		xmax = LexGetReal(xmax, "Upper limit (unchanged): ");
		LEXESCAPE;

		Gpt->rmins[i] = xmin;
		Gpt->rmaxs[i] = xmax;
		Gpt->AutoFlag &= ~(0x101<<i);								/* Clear flag bits */
	}

	return(OKAY);
}

/* . . . . PLX . . . . */
PRIVATE int gpt_do_plx(void) {
	char token[OPTION_STR_SIZE];
	int  i;

	if (LexGetTokenP(token, sizeof(token), "Plot X against [BOTTOM Top]: ")) {
		LEXESCAPE;
		if ( (i=LexSelect(token, "BOTTOM TOP")) <= 0) {
			gen_err("Illegal choice -- try it again dummy");
			return(NOMORE);
		}
		Gpt->plx = i-1;
	} else
		Gpt->plx = 0;
	return(OKAY);
}

/* . . . . PLY . . . . */
PRIVATE int gpt_do_ply(void) {
	char token[OPTION_STR_SIZE];
	int  i;

	if (LexGetTokenP(token, sizeof(token), "Plot Y for [LEFT Right]: ")) {
		LEXESCAPE;
		if ( (i=LexSelect(token, "LEFT RIGHT")) <= 0) {
			gen_err2("Illegal response -- try it again dummy", token);
			return(NOMORE);
		}
		Gpt->ply = i-1;
	} else
		Gpt->ply = 0;
	return(OKAY);
}

/* . . . . AUTOX . . . . */
PRIVATE int gpt_do_autox(void) {
	char token[OPTION_STR_SIZE];
	int i;
/*                           OFF    BOTTOM    TOP     BOTH   */
	static int orbits[4]  = {0x0000, 0x0001,  0x0004,  0x0005};
	static int BITMASK    = ~0x0505;

	if (! LexGetTokenP(token,sizeof(token),"[BOTH Top Bot off]: ")) strcpy(token, "both");
	LEXESCAPE;

	if (stricmp(token, "bot") == 0) strcpy(token, "bottom");
	if ( (i = LexSelect(token, "OFF BOTTOM TOP BOTH")) <= 0) {
		gen_err2("Unrecognized AUTOX mode -- try it again dummy", token);
		return(NOMORE);
	}
	Gpt->AutoFlag = (Gpt->AutoFlag & BITMASK) | orbits[i-1];
	return(OKAY);
}


/* . . . . AUTOY . . . . */
PRIVATE int gpt_do_autoy(void) {
	char token[OPTION_STR_SIZE];
	int i;
/*                           OFF    LEFT     RIGHT     BOTH   */
	static int orbits[4]  = {0x0000, 0x0002,  0x0008,  0x000A};
	static int BITMASK    = ~0x0A0A;

	if (! LexGetTokenP(token,sizeof(token),"[BOTH Left Right off]: ")) strcpy(token, "both");
	LEXESCAPE;

	if ( (i = LexSelect(token, "OFF LEFT RIGHT BOTH")) <= 0) {
		gen_err2("Unrecognized AUTOY mode -- try it again dummy", token);
		return(NOMORE);
	}
	Gpt->AutoFlag = (Gpt->AutoFlag & BITMASK) | orbits[i-1];
	return(OKAY);
}


/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
PRIVATE int gpt_do_labels(void) {
	int i;

	i = GetAxisName();											/* Get Axis request */
	LEXESCAPE;
	if (i == -1) return(OKAY);
	if (i <   0) return(NOMORE);								/* Invalid name */

	if (! LexGetStrExpr(Gpt->titles[i], sizeof(Gpt->titles[0])))
		LexPromptStr(Gpt->titles[i], sizeof(Gpt->titles[0]), "Title: ");
	return(OKAY);
}

/* . . . . LOGARITHM - Turn on or off log mode of axis labeling */
PRIVATE int gpt_do_logaxis(void) {
	int i;
	LOGICAL ltmp;

	i = GetAxisName();											/* Get Axis request */
	LEXESCAPE;
	if (i == -1) return(OKAY);
	if (i <   0) return(NOMORE);								/* Invalid name */

	ltmp = LexChoice(FALSE, "ON;YES", "OFF;NO", "Logarithmic labeling mode (on|yes|OFF|no): ");
	if (! LexEscape(TRUE)) Gpt->logtype[i] = ltmp;
	return(OKAY);
}


/* . . . . FORCE . . . . Force the user scales */
PRIVATE int gpt_do_force(void) {

	char token[OPTION_STR_SIZE];
	int  pattern=0;

	while (TRUE) {
		if (! LexGetTokenP(token, sizeof(token), 
		  "Force your scaling choice? [bottom|top|left|right|x|y|z] {NO|yes}: "))
			strcpy(token, "no");
		LEXESCAPE;

		switch (LexSelect(token, 
				"BOTTOM TOP LEFT RIGHT XAXIS YAXIS ZAXIS YES ON NO OFF")) {
 			case 8:
 			case 9:
				if (pattern == 0) pattern = -1;
				Gpt->ForceRegions |= pattern;
				return(OKAY);
			case 10:
			case 11:
				if (pattern == 0) pattern = -1;
				Gpt->ForceRegions &= ~pattern;
				return(OKAY);
			case 1:
			case 5:
				pattern |= (0x01 << BOTTOM);	break;
			case 2:
				pattern |= (0x01 << TOP);		break;
			case 3:
			case 6:
 				pattern |= (0x01 << LEFT);		break;
 			case 4:
 				pattern |= (0x01 << RIGHT);	break;
 			case 7:
 				pattern |= (0x01 << ZAXIS);	break;
			default:
				gen_err2("Unrecognized FORCE mode -- try it again dummy", token);
				return(NOMORE);
		}
	}
	panic; return(0);
}
	
/* . . . . PLOTMODE - Turn on or off autoaxis on plot command . . . . */
PRIVATE int gpt_do_autoaxis(void) {
	LOGICAL ltmp;
	ltmp = LexOnOff(TRUE, "Auto-drawing of axis on plot (ON|off): ");
	if (! LexEscape(TRUE)) Gpt->AutoAxes = ltmp;
	return(OKAY);
}

/* ... BOXMODE - Specifies box or crossing axis for the drawing */
PRIVATE int gpt_do_boxmode(void) {
	LOGICAL ltmp;
	ltmp = LexYesNo(TRUE, "Box Mode (YES)? ");
	if (!LexEscape(TRUE)) Gpt->BoxMode = ltmp;
	return(OKAY);
}

/* . . . . SUBTICKS . . . . */
static char SubticksHelp[] =
" Determines whether minor tickmarks are drawn for all axes.  Default has the\n"
" minor tick marks enabled, but should be disabled for publications.  AXCTRL\n"
" can be used to set for individual axes.\n"
"\n"
"   SUBTicks [on | off]\n"
"\n"
" Examples: subticks off               /* Publication mode\n";

PRIVATE int gpt_do_subticks(void) {
	LOGICAL ltmp;
	int i;

	if (LexCheckHelp("Subticks", SubticksHelp, NULL)) return(OKAY);

	ltmp = LexOnOff(TRUE, "Subticks mode (ON): ");
	LEXESCAPE;

	Gpt->MinorTicks = ltmp;
	for (i=0; i<5; i++) {
		if (ltmp)   
			Gpt->axmode[i] &= ~AXIS_NO_SUBTICKS;			/* Reset flag for subticks	*/
		else
			Gpt->axmode[i] |=  AXIS_NO_SUBTICKS;			/* Set flag for no subticks */
	}
	return(OKAY);
}

/* ----------------------- */
/* . . . . INTICKS . . . . */
/* ----------------------- */
static char InticksHelp[] =
" Determines whether the tickmarks are drawn into or out of the graph area.\n"
" Default is normally into the graph - sometimes it is useful to draw out\n"
" to avoid competing with data.  AXCTRL can be used to set for individual axes.\n"
"\n"
"   INTicks [yes | no]\n"
"\n"
" Examples: inticks no                  /* No subticks on bottom only\n";

PRIVATE int gpt_do_inticks(void) {
	LOGICAL ltmp;
	int i;

	if (LexCheckHelp("Inticks", InticksHelp, NULL)) return(OKAY);

	ltmp = LexYesNo(TRUE, "Turn tick marks in (YES)? ");
	LEXESCAPE;

	Gpt->MinorTicks = ltmp;
	for (i=0; i<5; i++) {
		if (ltmp)   
			Gpt->axmode[i] |= AXIS_TICKS_INWARD;
		else
			Gpt->axmode[i] &= ~AXIS_TICKS_INWARD;
	}
	return(OKAY);
}

/* ------------------------------------- */
/* . . . . AXCTRL - Axis control . . . . */
/* ------------------------------------- */
static char AxctrlHelp[] =
" Sets internal parameters to more precisely control the character of the\n"
" axes drawn by GENPLOT.  These parameters allow nearly every element of\n"
" the axes to be controlled by the user.  The modes are complex and sometimes\n"
" non-obvious.  Some experimentation with the behavior will be required :-)\n"
"\n"
"   AXCtrl [X | Y | Z | Bottom | Left | Top | Right]   [-options | <bflag>]\n"
"\n"
"   Options: All options may be negated by -no<option> format\n"
"      -brief         Label only first/last axis labels\n"
"      -inticks       Tick marks go into graph area     (use -nointicks)\n"
"      -rotate        Rotate labels 90 degrees\n"
"      -label         Include tick mark labels          (use -nolabel)\n"
"      -title         Include title on axis             (use -notitle)\n"
"      -color <pen>   Change the color of axis (full)   (use -color 0 to reset)\n"
"      -ticks         Include tickmarks                 (use -noticks)\n"
"      -subticks      Include subticks                  (use -nosubticks)\n"
"      -on | -off     Enable or disable axis drawing\n"
"      -justify       Keep first/last label within axis (use -nojustify)\n"
"      -grid          Draw grid on major/minor tick marks\n"
"      -speclog       Draw log scales with .001 to 1000 as numbers\n"
"      -tertiary      Draw tertiary labels (2,5) on log axes (use -notertiary)\n"
"      -dx <val>      Set spacing of major tick marks   (0 goes to auto)\n"
"      -dx2 <val>     Set spacing of minor tick marks   (0 goes to auto)\n"
"                        For log axes\n"
"                           -1  => label 1,5 only\n"
"                           -2  => label 1,2,5 only\n"
"                           -3  => label 1,2,3,...9 subticks\n"
"                           -4  => label 1.2,1.4,...3.8,4.0,4.5,5.0,...9.0,9.5\n"
"                           -5  => label 1.1,1.2,...4.9,5.0,5.2,5.4,...9.6,9.8\n"
"      -mx <num>      Set mode for tick mark labeling\n"
"                        0  => autoselect labeling\n"
"                       <0  => force integer mode only\n"
"                       >0  => number of digits past decimal\n"
"      -xstart <val>  Fractional distance along X for axis start (0-1)\n"
"      -ystart <val>  Fractional distance along Y for axis start (0-1)\n"
"      -length <val>  Fractional length of drawn axis (0-1)\n"
"                        These are used to draw partial axes\n"
"\n"
"      -user <major> <minor> <text> <csize>\n"
"      -nouser\n"
"           <major> - name of a REAL array with values of major tick marks\n"
"           <minor> - name of a REAL array with values of minor tick marks\n"
"           <text>  - name of a STRING array with labels for each major tick mark\n"
"           <csize> - size of tick mark labels (0 => use configured size)\n"
"\n"
"   It it also possible to set the logical values as a single binary flag\n"
"   value <bflag>.  However, the internal values are not guarenteed to any\n"
"   user except the program developer.\n"
"\n"
" Examples: axctrl bot -nosubticks                 /* No subticks on bottom only\n"
"           axctrl bot -xstart 0.5 -length 0.5     /* Half length axis\n"
"           axctrl top -off                        /* No top axis\n"
"           reg left -1 1 logar left on axctrl left -speclog\n"
"           axctrl bot -dx 5 -dx2 2                /* Incommensurate ticks\n"
"           reg bot -1000 1000 axctrl bot -nojustify\n";

PRIVATE int gpt_do_axctrl(void) {

	int axmode, iaxis, k;
	LOGICAL invert, NoOptions=TRUE;
	REAL t1;
	char token[OPTION_STR_SIZE];
	char *achr;
	
	typedef struct _CMTYPE2 {
		CHAR *command;
		INTEGER minlen;
		INTEGER rcode;
	} CMTYPE2;

/* ... Options for AXCTRL command */
	static const CMTYPE2 options[] =
	{	{"brief",		2, 1},	{"inticks",		2, 2},	{"rotate",		2, 3},
		{"label",		3,-4},	{"subticks",	3,-5},	{"ticks",		2,-6},
		{"off",			3, 7},	{"on",			1,-7},
		{"speclog",		4, 8},	{"justify",		4,-9},	{"grid",			1,10},
		{"title",      3, -12}, {"tertiary",	4,-13},
	   {"user",			2, 20},
		{"dx",			2,21},	{"dx2",			3,22},	{"mx",			2,23},
		{"xstart",		2,24},	{"ystart",		2,25},	{"length",		3,26},
		{"color",		3,27},
		{NULL,			0, 0} };
	CMTYPE2 *citem;

/* Check for help request */
	if (LexCheckHelp("AXCTRL - Axis Control", AxctrlHelp, NULL)) return(OKAY);

	iaxis = GetAxisName();										/* Get Axis request */
	LEXESCAPE;
	if (iaxis == -1) return(OKAY);
	if (iaxis <   0) return(NOMORE);							/* Invalid name here */

	axmode = Gpt->axmode[iaxis];
	
	while (LexGetOption(token, sizeof(token))) {		/* Parse all options */
		LEXESCAPE;
		NoOptions = FALSE;
		achr = token+1;
		invert = (strnicmp(achr, "NO", 2) == 0);		/* Is this a "no" form	*/
		if (invert) achr += 2;								/* Skip over the no		*/
		if ( (citem = LexCmdl(achr, options, sizeof(CMTYPE2))) == NULL) {
			gen_err2("Illegal option for AXCTRL", token);
			return(NOMORE);
		}
		k = citem->rcode;
		if (k < 20) {											/* Flag settings? (+/-) */
			if (invert) k = -k;
			if (k > 0) {
				axmode |= (0x0001 << k);
			} else {
				axmode &= ~(0x0001 << -k);
			}
		} else if (k == 20) {								/* User labeling options */
			if (invert) {
				Gpt->do_user_labels[iaxis] = FALSE;
			} else {
				if (! LexGetTokenP(Gpt->user_labels[iaxis].major, sizeof(Gpt->user_labels[iaxis].major), "REAL array of major tick mark values: "))   *Gpt->user_labels[iaxis].major = '\0';
				if (! LexGetTokenP(Gpt->user_labels[iaxis].minor, sizeof(Gpt->user_labels[iaxis].minor), "REAL array of minor tick mark values: "))   *Gpt->user_labels[iaxis].minor = '\0';
				if (! LexGetTokenP(Gpt->user_labels[iaxis].text,  sizeof(Gpt->user_labels[iaxis].text),  "STRING array of major tick mark labels: ")) *Gpt->user_labels[iaxis].text = '\0';
				if (! LexGetMathP(Gpt->user_labels[iaxis].csize,  sizeof(Gpt->user_labels[iaxis].csize), "Expression for label size (inches): "))      strcpy(Gpt->user_labels[iaxis].csize, "0.0");
				Gpt->do_user_labels[iaxis] = TRUE;
			}
		} else if (k >= 21 && k <= 26) {
			t1 = LexGetReal(0.0f, "Value to use: ");
			LEXESCAPE;
			if (k == 21) 
				Gpt->udx[iaxis] = t1;
			else if (k == 22) 
				Gpt->udx2[iaxis] = t1;
			else if (k == 23)
				Gpt->umx[iaxis] = nint(t1);
			else if (k == 24) 
				Gpt->uxs[iaxis] = t1;
			else if (k == 25)
				Gpt->uys[iaxis] = t1;
			else if (k == 26)
				Gpt->uxl[iaxis] = t1;
		} else if (k == 27) {								/* Axis color */
			Gpt->color[iaxis] = LexGetInt(0, "Pen color for axis: ");
		} else {
			ERRprintf("ERROR: Unrecognized case in AXCTRL command (%d).  Tell stupid developer\n", k);
		}
	}
	if (NoOptions) axmode = LexGetInt(axmode, "Axis control mode (unchanged): ");
	if (! LexEscape(TRUE)) Gpt->axmode[iaxis] = axmode;
	return(OKAY);
}


/* ---------------------------------------------------------------------------
-- Function to handle the surface create command.  Parses command line further
-- and calls the low level routine to implement.
---------------------------------------------------------------------------- */
PRIVATE int gpt_do_create_surface(void) {

	REAL xmin, xmax, ymin, ymax, *x, *y, *z;
	REAL xval;
	INT  i, nrow, ncol;
	int  ierr, itype, create_grid=FALSE;
	char token[DFLT_STR_SIZE], name[VARNAME_STR_SIZE], equation[LONG_STR_SIZE], *aptr;
	SURFACE **surf=NULL;

	REAL *xsave=NULL, *ysave=NULL;					/* Save important stuff */
	char optsave[80];

	CREATE_3D *parms;

/* Make a local pointer to the parameter set of use */
	parms = &Gpt->createuse->surf;

/* Get name of surface to create */
	if (! LexGetTokenP(name, sizeof(name), "Surface name (abort): ")) {
	  return(OKAY);
	} else if (GVGetInfo(name, &itype, (void **) &surf)) {
		if (itype != GV_SURFACE) {
			ERRprintf("ERROR: %s already exists but is not a surface\n", name);
			return(NOMORE);
		}
		nrow = (*surf)->nrow;							/* Start w/ old values	*/
		ncol = (*surf)->ncol;
		ArrayMinMax((*surf)->x, nrow, &xmin, &xmax);
		ArrayMinMax((*surf)->y, ncol, &ymin, &ymax);
	} else {
		surf = NULL;
	}

/* ... Must tolerate "z = f(x)", "z= f(x,y)", "z =f(x)" and "z=f(x,y)"	*/
/* ... First, get an equation and see if one of the above forms			*/
/* ... = sign in format z =f(x,y) handled by LexGetMath directly			*/
	*equation = '\0';										/* No expression yet		*/
	if (LexGetMath(equation, sizeof(equation))) {
		aptr = equation;
		if (*aptr == '/') {
			strcpy(equation, parms->equation);
		} else if (strchr("zZ",*aptr) != NULL) {	/* Z can be header		*/
			aptr++;
			if (*aptr == '\0') {							/* z = f(x) | z =f(x) z f(x) */
				*equation = '\0';
			} else if (*aptr == '=') {					/* y==7 y= f(x) y=f(x)	*/
				aptr++;
				if (*aptr == '\0') {						/* y= f(x) or similar	*/
					*equation = '\0';
				} else if (strchr("<>=", *aptr) == NULL) { /* Not z==x type */
					memmove(equation, aptr, strlen(aptr)+1);
				}
			}
		}
	}

/* -- Get the equation */
	if (*equation == '\0' && ! LexGetMath(equation, sizeof(equation))) {
		LexPromptStr(equation, sizeof(equation), "Z(x,y) = ");
		if (*equation == '/') strcpy(equation, parms->equation);
	}
	if (GVChkParse(equation, NULL) == NULL) goto BadEquation;
	strcpy(parms->equation, equation);				/* Save for next time */

	while (LexChkToken(token, sizeof(token))) {	/* Parse options */
		aptr = token;
		if (*aptr == '-' || *aptr == '/') aptr++;
		i = LexSelect(aptr, "ROWS COLUMNS XRANGE YRANGE RANGE GRID COLS MESH");
		if (i == 7) i = 2;								/* Make COLS the same as COLUMNS */
		if (i == 8) i = 6;								/* Make MESH the same as GRID */
		if (i <= 0) break;								/* Not an option */
		LexGetToken(token, sizeof(token));

		create_grid = TRUE;								/* Any option ==> new grid */
		if (i == 1 || i == 6) {
			if (LexGetMathP(token, sizeof(token), "Number of constant Y rows on surface: ")) {
				strcpy(parms->tok_row, token);
				nrow = nint(GVTrimToDouble(GVEvalExpr(parms->tok_row, &ierr)));
				if (ierr != 0) goto BadRange;
			}
		}
		if (i == 2 || i == 6) {
			if (LexGetMathP(token, sizeof(token), "Number of constant X columns on surface: ")) {
				strcpy(parms->tok_col, token);
				ncol = nint(GVTrimToDouble(GVEvalExpr(parms->tok_col, &ierr)));
				if (ierr != 0) goto BadRange;
			}
		}
		if (i == 3 || i == 5) {
			if (LexGetMathP(token, sizeof(token), "Minimum value of X: ")) {
				strcpy(parms->x_min, token);
				xmin = GVTrimToReal(GVEvalExpr(parms->x_min, &ierr));
				if (ierr!=0) goto BadRange;
			}
			if (LexGetMathP(token, sizeof(token), "Maximum value of X: ")) {
				strcpy(parms->x_max, token);
				xmax = GVTrimToReal(GVEvalExpr(parms->x_max, &ierr));
				if (ierr!=0) goto BadRange;
			}
		}
		if (i == 4 || i == 5) {
			if (LexGetMathP(token, sizeof(token), "Minimum value of Y: ")) {
				strcpy(parms->y_min, token);
				ymin = GVTrimToReal(GVEvalExpr(parms->y_min, &ierr));
				if (ierr!=0) goto BadRange;
			}
			if (LexGetMathP(token, sizeof(token), "Maximum value of Y: ")) {
				strcpy(parms->y_max, token);
				ymax = GVTrimToReal(GVEvalExpr(parms->y_max, &ierr));
				if (ierr!=0) goto BadRange;
			}
		}
	}

	if (surf == NULL) {
		create_grid = TRUE;
		nrow = nint(GVTrimToDouble(GVEvalExpr(parms->tok_row, &ierr))); if (ierr!=0) goto BadRange;
		ncol = nint(GVTrimToDouble(GVEvalExpr(parms->tok_col, &ierr))); if (ierr!=0) goto BadRange;
		xmin = GVTrimToReal(GVEvalExpr(parms->x_min, &ierr)); if (ierr!=0) goto BadRange;
		xmax = GVTrimToReal(GVEvalExpr(parms->x_max, &ierr)); if (ierr!=0) goto BadRange;
		ymin = GVTrimToReal(GVEvalExpr(parms->y_min, &ierr)); if (ierr!=0) goto BadRange;
		ymax = GVTrimToReal(GVEvalExpr(parms->y_max, &ierr)); if (ierr!=0) goto BadRange;
	}
	if (ncol <= 1) ncol = Gpt->mesh[0];
	if (nrow <= 1) nrow = Gpt->mesh[1];
	if (nrow <= 1 || ncol <= 1) goto BadRange;

	if (xmin == xmax) {
		xmax = 2*xmax;
		if (xmax == 0) xmax = 1.0f;
		create_grid = TRUE;
	}
	if (ymin == ymax) {
		ymax = 2*ymax;
		if (ymax == 0) ymax = 1.0f;
		create_grid = TRUE;
	}

	if (surf != NULL) {
		if (nrow != (*surf)->nrow || ncol != (*surf)->ncol) {
			GVDeallocate(name);
			surf = NULL;
		} else if (nrow != (*surf)->nrowmax || ncol != (*surf)->ncolmax) {
			xsave = malloc(sizeof(*xsave)*ncol);
			ysave = malloc(sizeof(*ysave)*nrow);
			memcpy(xsave, (*surf)->x, sizeof(*xsave)*ncol);
			memcpy(ysave, (*surf)->y, sizeof(*ysave)*nrow);
			strscpy(optsave, (*surf)->options, sizeof(optsave));
			GVDeallocate(name);
			surf = NULL;
		} else {
			(*surf)->nrow = nrow;
			(*surf)->ncol = ncol;
			(*surf)->npt  = nrow*ncol;
		}
	}
	if (surf == NULL) create_grid = TRUE;			/* Will need new grid */

	if (surf == NULL && ! GVAllocSurface(name, GVF_USER, nrow, ncol)) {
		gen_err("Unable to allocate space for CREATE SURFACE function");
		return(NOMORE);
	} else if (GVGetInfo(name, &itype, (void **) &surf) && itype!=GV_SURFACE) {
		ERRprintf("ERROR: %s already exists but is not a surface\n", name);
		return(NOMORE);
	}

	x = (*surf)->x;										/* X array				*/
	y = (*surf)->y;										/* Y array				*/
	z = (*surf)->z;										/* Z matrix				*/

/* Create the row and column values */
	if (create_grid) {
		if (xsave != NULL) {
			memcpy((*surf)->x, xsave, sizeof(*xsave)*ncol);
			free(xsave);
		} else {
			for (i=0; i<ncol; i++) x[i] = xmin + (xmax-xmin)*i/(ncol-1);
		}
		if (ysave != NULL) {
			memcpy((*surf)->y, ysave, sizeof(*ysave)*nrow);
			free(ysave);
		} else {
			for (i=0; i<nrow; i++) y[i] = ymin + (ymax-ymin)*i/(nrow-1);
		}
	}
		
	GVLinkReal ("X", 0, &xval);
	GVLinkArray("Y", 0, y, nrow, NULL);
	strscpy((*surf)->ids, equation, sizeof((*surf)->ids));
	if (*optsave != '\0') strscpy((*surf)->options, optsave, sizeof((*surf)->options));

	for (i=0; i<ncol; i++) {
		if (SysChkBreak(FALSE)) break;
		xval = x[i];
		GVLinkArray("Z", 0, z+i*(*surf)->nrowmax, nrow, NULL);
		if (! GVSetValue("Z", equation) ) {
			ERRprintf("ERROR: CREATE aborted because of multiple errors or ^C\n");
			break;
		}
	}
	return (SysChkBreak(FALSE)) ? NOMORE : OKAY ;

BadRange:
	ERRprintf("ERROR: Invalid expression for XMIN,XMAX,YMIN,YMAX,NCOL or NROW:\n"
		"   xmin: %20s   xmax: %s\n"
      "   ymin: %20s   ymax: %s\n"
		"   nrow: %20s   ncol: %s\n", 
		parms->x_min, parms->x_max, parms->y_min, parms->y_max, parms->tok_row, parms->tok_col);
	return(NOMORE);

BadEquation:
	ERRprintf("ERROR: Equation does not parse: %s\n"
		"    Usage: CREATE -SURFACE <S1> Z = f(x,y) [options]\n",
		equation);
	return(NOMORE);
}

/* ---------------------------------------------------------------------------
-- Function to handle the "CREATE" command.  Parses command line further and
-- calls the low level routine to implement.
---------------------------------------------------------------------------- */
static char CreateHelp[] = 
"\n"
" Command to create a curve or surface based on analytical expression\n"
"\n"
"   CREATE [y [=] ] f(x) [-opts]\n"
"   CREATE -surface <surf> [z [=] ] f(x,y) [-opts]\n"
"\n"
" For a normal curve, options are:\n"
"   -from <xlow>               - Low limit of created data\n"
"   -to   <xhigh>              - Upper limit of created data\n"
"   -range <xlow> <xhigh>      - Lower/upper limits\n"
"   -points <npt>              - Number of evenly spaced points\n"
"   -by <dx>                   - Spacing between points\n"
" All values may be expression that are retained between calls to create.\n"
" If mutually exclusive options are used, only the last given is retained.\n"
"\n"
" For creating a surface\n"
"   -xrange <xmin> <xmax>      - Extent of the X variable\n"
"   -yrange <ymin> <ymax>      - Extent of the Y variable\n"
"   -range <xmin> <xmax> <ymin> <ymax>\n"
"   -rows    <nrow>            - Number of rows of constant Y\n"								  
"   -columns <ncol>            - Number of columns of constant X\n"								  
"   -grid <rows> <cols>        - Both at once\n"
"   -mesh <rows> <cols>        - Synonymous with -grid\n"
" As before, only the last specification is retained\n"
"\n"
" Examples: create y = sin(x) -range -10 10 -points 200\n"
"           create -surface s1 x^2-y^2 -range -1 1 -1 1 -grid 51 51\n";

PRIVATE int gpt_do_create(void) {

	CREATE_2D *parms;
	REAL rxmin, rxmax, xmin, xmax, dx, *x;
	INT  i, nptme;
	LOGICAL zmode = FALSE;								/* Default is Y mode */
	int  ierr, itype;
	char token[DFLT_STR_SIZE], equation[LONG_STR_SIZE], *aptr;

/* First, look for a help request */
	if (LexCheckHelp("Create", CreateHelp, NULL)) return(OKAY);

/* Look for creation of a surface */
	if (LexChkToken(token, sizeof(token))) {
		aptr = (*token == '-') ? token+1 : token;
		if (LexEqual(aptr, "surface", 4)) {
			LexGetToken(token, sizeof(token));
			return(gpt_do_create_surface());
		}
	}

/* Make a local pointer to the parameters to use */
	parms = &Gpt->createuse->fnc;
	
/* ... Must tolerate "y = x", "y= x", "y =x" and "y=x"				*/
/* ... First, get an equation and see if one of the above forms	*/
/* ... = sign in format y =f(x) handled by LexGetMath directly		*/
	*equation = '\0';										/* No expression yet		*/
	if (LexGetMath(equation, sizeof(equation))) {
		aptr = equation;
		if (*aptr == '/') {
			strcpy(equation, parms->equation);
		} else if (strchr("yYzZ",*aptr) != NULL) {	/* If yz can be header	*/
			aptr++;
			if (*aptr == '\0') {							/* y = f(x) | y =f(x) y f(x) */
				zmode = (tolower(*equation) == 'z');
				*equation = '\0';
			} else if (*aptr == '=') {					/* y==7 y= f(x) y=f(x)	*/
				aptr++;
				if (*aptr == '\0') {						/* y= f(x) or similar	*/
					zmode = (tolower(*equation) == 'z');
					*equation = '\0';
				} else if (strchr("<>=", *aptr) == NULL) { /* Not y==x type */
					zmode = (tolower(*equation) == 'z');
					memmove(equation, aptr, strlen(aptr)+1);
				}
			}
		}
	}

/* -- Get the equation */
	if (*equation == '\0' && ! LexGetMath(equation, sizeof(equation))) {
		LexPromptStr(equation, sizeof(equation), zmode ? "Z(x,y) = " : "Y(x) = ");
		if (*equation == '/') strcpy(equation, parms->equation);
	}
	if (GVChkParse(equation, NULL) == NULL) goto BadEquation;
	strcpy(parms->equation, equation);

/* Parse options, collecting for the moment */
	while (LexGetToken(token, sizeof(token))) {			/* Parse options */
		aptr = token;
		if (*aptr == '-' || *aptr == '/') aptr++;
		i = LexSelect(aptr, "BY POINTS FROM TO RANGE");
		if (i <= 0) {										/* Unknown or ambiguous */
			LexBackup();
			break;
		} else if (i == 1) {
			if (LexGetMathP(token, sizeof(token), "Step size: ")) 
				strcpy(parms->tok_dx, token);
		} else if (i == 2) {
			if (LexGetMathP(token, sizeof(token), "Number of points: ")) {
				strcpy(parms->tok_npt, token);
				strcpy(parms->tok_dx, "0");
			}
		} else {
			if (i != 4) {									/* Either 3 or 5 */
				if (LexGetMathP(token, sizeof(token), "Minimum value of X: "))
					strcpy(parms->x_min, token);
			}
			if (i != 3) {									/* Either 4 or 5 */
				if (LexGetMathP(token, sizeof(token), "Maximum value of X: "))
					strcpy(parms->x_max, token);
			}
		}
	}

/* Evaluate the expressions for the options */
	xmin = GVTrimToReal(GVEvalExpr(parms->x_min, &ierr)); if (ierr != 0) goto BadRange;
	xmax = GVTrimToReal(GVEvalExpr(parms->x_max, &ierr)); if (ierr != 0) goto BadRange;
	if (xmin == xmax) {
		xmax = 2*xmax;
		if (xmax == 0) xmax = 1.0f;
	}
	dx = GVTrimToReal(GVEvalExpr(parms->tok_dx, &ierr)); 
	if (ierr != 0) goto BadRange;
	if (dx != 0) {
		nptme = nint(fabs((xmax-xmin)/dx)) + 1;
	} else {
		nptme = nint(GVTrimToDouble(GVEvalExpr(parms->tok_npt, &ierr)));
		if (ierr != 0 || nptme <= 0) goto BadRange;
	}
	nptme = min(GVI_MAX_LENGTH, nptme);

	if (stricmp(GptUseCurve, "$$TMP$$") == 0) GVDeallocate(GptUseCurve);
	if (GVGetInfo(GptUseCurve, &itype, NULL)) {
		if ( (itype != GV_2DCURVE) && (itype != GV_3DCURVE) ) {
			gen_err2("Specified name already exists but is not a curve", GptUseCurve);
			return(NOMORE);
		}
	} else if (! GVAlloc2DCurve(GptUseCurve, GVF_USER, nptme)) {
		gen_err("Unable to allocate space for CREATE function");
		return(NOMORE);
	}

	if (GptLinkXYZ(GptUseCurve) != 0) {
		ERRprintf("HUH? Curve %s should be there or should have been created\n", GptUseCurve);
		return(NOMORE);
	}

	if (nptme > GptCurve->nptmax) {						/* Need to resize?	*/
		if (GVResize(GptUseCurve, nptme)) {					/* Did it succeed?	*/
			GptLinkXYZ(GptUseCurve);							/* Relink now			*/
		} else {
			ERRprintf("WARNING: Too fine an increment chosen.  NPTMAX used instead.\n");
			nptme = GptCurve->nptmax;
		}				
	}

	rxmin = min(xmin, xmax);								/* Limit for safety */
	rxmax = max(xmin, xmax);
	rxmin = rxmin - 1.0E-7f*(rxmax-rxmin);
	rxmax = rxmax + 1.0E-7f*(rxmax-rxmin);

	if (dx==0 && nptme>1) dx=(xmax-xmin)/(nptme-1);	/* DX value */
	x = GptCurve->x;											/* X pointer */
	for (i=0; i<nptme; i++) {
		x[i] = xmin + dx*i;									/* X value */
		if (x[i] < rxmin || x[i] > rxmax) break;
	}
	GptCurve->npt = i;										/* Now, actual number */

	strscpy(GptCurve->ids, equation, sizeof(GptCurve->ids));
	if (! GVSetValue("Y", equation)) {
		ERRprintf("ERROR: CREATE aborted because of multiple errors or ^C\n");
		return(NOMORE);
	}
	return(OKAY);
	
BadRange:
	ERRprintf("ERROR: Invalid expression for XMIN, XMAX, DX or NPT:\n"
		"   xmin: %s   xmax: %s\n"
		"   dx:   %s   npt:  %s\n", parms->x_min, parms->x_max, parms->tok_dx, parms->tok_npt);
	return(NOMORE);

BadEquation:
	ERRprintf("ERROR: Equation does not parse: %s\n"
		"    Usage: CREATE Y = f(x) [FROM x1] [TO x2] [{BY dx | POINTS npt}]\n",
		equation);
	return(NOMORE);
}

/* ---------------------- */
/* . . . .  XTOP  . . . . */
/* ---------------------- */
PRIVATE int gpt_do_xtop(void) {
	char token[OPTION_STR_SIZE];
	LOGICAL ltmp;
	int i;
	
	ltmp = LexGetTokenP(token, sizeof(token), "[OFF On Bottom Nonlinear]: ");
	LEXESCAPE;
	if (ltmp) {
		i = LexSelect(token,"OFF ON BOTTOM YOUDRAW NONLINEAR");
		if (i <= 0) {
			gen_err2("Unrecognized mode for XTOP/YRIGHT", token);
			return(NOMORE);
		}
		Gpt->xtop = min(3, i-1);				/* IS_OFF, IS_ON, IS_COPY, IS_NONLINEAR */
	} else
		Gpt->xtop = IS_OFF;
	return(OKAY);
}

/* ---------------------- */
/* . . . . YRIGHT . . . . */
/* ---------------------- */
PRIVATE int gpt_do_yright(void) {
	char token[OPTION_STR_SIZE];
	LOGICAL ltmp;
	int i;
	
	ltmp = LexGetTokenP(token, sizeof(token), "[OFF On Left Nonlinear]: ");
	LEXESCAPE;
	if (ltmp) {
		i = LexSelect(token,"OFF ON LEFT YOUDRAW NONLINEAR");
		if (i <= 0) {
			gen_err2("Unrecognized mode for XTOP/YRIGHT", token);
			return(NOMORE);
		}
		Gpt->yright = min(3, i-1);		/* IS_OFF, IS_ON, IS_COPY, IS_NONLINEAR */
	} else {
		Gpt->yright = IS_OFF;
	}
	return(OKAY);
}



/* ===========================================================================
-- Routine to handle simple requests so as to avoid more functions
=========================================================================== */
PRIVATE int gpt_do_simplefncs(OPS1 key) {

	int rcode=TRUE;

	switch (key) {
		case GPT_SETRANGE:						/* Just set range in place */
			GptSetRange();
			break;
		case GPT_CONFIG:
			rcode = (GptConfig(0) == 0) ? OKAY : NOMORE;
			break;
		case GPT_READ:
			if ( (rcode = GptRead(GptUseCurve)) == 0) gpt_do_status();
			rcode = (rcode >= 0) ? OKAY : NOMORE;
			break;
		case GPT_WRITE:
			rcode = (GptWrite(GptUseCurve) >= 0) ? OKAY : NOMORE;
			break;
		case GPT_ANNOTE:
			GptSetRange();
			if (! PlotAnnote(TRUE)) TTYputs("ANNOTE: Automatic return\n");
			break;
		case GPT_SPLINE:							/* Alias to run spline */
			LexInsText("fit spline");
			break;
		case GPT_NLSFIT:							/* Alias to nlsfit */
			LexInsText("fit nlsfit");
			break;
		case GPT_FIT:								/* Alias to fit */
			rcode = (GptFit() >= 0) ? OKAY : NOMORE;
			break;
		case FUNCTION:								/* Nop - handled elsewhere */
			break;
	}
	return(rcode);
}


/* ============================================================================
-- Routine to check if next token is a CURVE identifier and returns either 
-- name specified or default.
--
-- Usage:  call CheckForCurve(name,default)
--
-- Inputs: default - default name to be used if no next token or not curve
--
-- Output: name    - name of a valid curve or DEFAULT if none requested
============================================================================ */
PRIVATE int CheckForCurve(char *Curve, const char *Default, int flags) {

	char token[DFLT_STR_SIZE];
	int itype;
	
	strcpy(Curve, Default);
	if ( ! (flags & (ALLOW_CURVE | ALLOW_IMPLIED_CURVE)) ) return(0);

	if (LexChkToken(token, sizeof(token))) {
		if (LexEqual(token, "-CURVE", 3) || ( (flags & SURFACE_OK) && LexEqual(token, "-SURFACE", 5)) ) {
			LexGetToken(token, sizeof(token));			/* Strip it from list */
			if (! LexGetTokenP(token, sizeof(token), "Curve/surface name (MAIN): ")) 
				return(0);
CheckAgain:
			if (! GVGetInfo(token, &itype, NULL) ||
				( (itype != GV_2DCURVE) && (itype != GV_3DCURVE) &&
				 !( (itype == GV_SURFACE) && (flags & SURFACE_OK) ) ) ) {
				if (flags & CREATE_CURVE) {
					if ( ! (Gpt->mode_3d ? GVAlloc3DCurve(token, GVF_USER, DEFAULT_CURVE_SIZE) : GVAlloc2DCurve(token, GVF_USER, DEFAULT_CURVE_SIZE)) ) {
						ERRprintf("ERROR: Unable to allocate %s on the fly\n", token);
						return(-1);
					}
					flags &= ~CREATE_CURVE;
					goto CheckAgain;
				} else {
					ERRprintf("ERROR: %s does not exist as a curve (or surface)\n", token);
					return(-1);
				}
			}
			strcpy(Curve, token);

		} else if (flags & ALLOW_IMPLIED_CURVE) {
			if (GVGetInfo(token, &itype, NULL)) {
				if (itype == GV_2DCURVE || itype == GV_3DCURVE
					|| ( (itype == GV_SURFACE) && (flags & SURFACE_OK)) ) {
					LexGetToken(token, sizeof(token));	/* Really get it now */
					strcpy(Curve, token);					/* And use it			*/
				}
			}
		}
	}
	return(0);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
PRIVATE void gpt_link_vars(int itype) {

	static int major=2,minor=0;

	if (itype & 0x01) {							/* Link the position vars */
		int ix, iy;									/* Index for X, index for Y */
		ix = (Gpt->BoxMode) ? 2*Gpt->plx    : BOTTOM;
		iy = (Gpt->BoxMode) ? 2*Gpt->ply +1 : LEFT;
		GVLinkReal("$XMIN", GVF_HIDDEN | GVF_INTERNAL, &Gpt->rmins[ix]);
		GVLinkReal("$XMAX", GVF_HIDDEN | GVF_INTERNAL, &Gpt->rmaxs[ix]);
		GVLinkReal("$YMIN", GVF_HIDDEN | GVF_INTERNAL, &Gpt->rmins[iy]);
		GVLinkReal("$YMAX", GVF_HIDDEN | GVF_INTERNAL, &Gpt->rmaxs[iy]);
		GVLinkReal("$ZMIN", GVF_HIDDEN | GVF_INTERNAL, &Gpt->rmins[ZAXIS]);
		GVLinkReal("$ZMAX", GVF_HIDDEN | GVF_INTERNAL, &Gpt->rmaxs[ZAXIS]);
	}
	if (itype & 0x80) {							/* Link the initial work values */
		major = (int) GptVersionNumber;
		minor = (int) ( 100*(GptVersionNumber-major)+0.5);
		GVLinkReal("$GENPLOT", GVF_HIDDEN | GVF_INTERNAL | GVF_CONSTANT, (REAL *) &GptVersionNumber);
		GVLinkReal("$VERSION", GVF_HIDDEN | GVF_INTERNAL | GVF_CONSTANT, (REAL *) &GptVersionNumber);
		GVLinkInt("$VERSION:MAJOR", GVF_HIDDEN | GVF_INTERNAL | GVF_CONSTANT, &major);
		GVLinkInt("$VERSION:MINOR", GVF_HIDDEN | GVF_INTERNAL | GVF_CONSTANT, &minor);
		GVLinkArray("$RMINS",  GVF_HIDDEN | GVF_INTERNAL, Gpt->rmins, 5, NULL);
		GVLinkArray("$RMAXS",  GVF_HIDDEN | GVF_INTERNAL, Gpt->rmaxs, 5, NULL);
		GVLinkString("$X_descriptor", GVF_INTERNAL | GVF_HIDDEN, Gpt->XYZ_descriptor[0], sizeof(Gpt->XYZ_descriptor[0]));
		GVLinkString("$Y_descriptor", GVF_INTERNAL | GVF_HIDDEN, Gpt->XYZ_descriptor[1], sizeof(Gpt->XYZ_descriptor[1]));
		GVLinkString("$Z_descriptor", GVF_INTERNAL | GVF_HIDDEN, Gpt->XYZ_descriptor[2], sizeof(Gpt->XYZ_descriptor[2]));
	}
/*	TTYprintf("Version number: %f\n", GptVersionNumber); */
	return;
}


/* ---------------------------- */
/* . . . . AXIS command . . . . */
/* ---------------------------- */
static char AxisHelp[] =
" The AXIS (or AXES) command simply draws the axes corresponding to the\n"
" current settings for scales and labels.  It is normally implicitly\n"
" called as part of the PLOT command as long as AUTOAXIS is set true.\n"
" However, it can be explicitly called followed by a series of OVERLAY\n"
" commands to add features to the graph.  There are no options for this\n"
" command.\n"
"\n"
"   AXIS | AXES\n"
"\n"
"Example:\n"
"   axis\n";

PRIVATE int gpt_do_axis(void) {
	
/* Check for help request */
	if (LexCheckHelp("AXIS - Draw graph axes", AxisHelp, NULL)) return(OKAY);

	if (! PlotSystem(1, NULL, NULL)) return(NOPLOTTER);
	GptDrawAxes();
	return(OKAY);
}
	
/* ---------------------------------- */
/* . . . . 3D specifi command . . . . */
/* ---------------------------------- */
PRIVATE int gpt_do_3Dview(void) {

	REAL rotate, tilt, skew;

	rotate = LexGetReal(Gpt->rotate, "Rotation about Z axis (no change): ");
	LEXESCAPE;
	tilt = LexGetReal(Gpt->tilt, "Tilt about X axis (no change): ");
	LEXESCAPE;
	skew = LexGetReal(Gpt->skew, "Skew about Y axis (no change): ");
	LEXESCAPE;
	Gpt->rotate = rotate;
	Gpt->tilt   = tilt;
	Gpt->skew   = skew;
	PlotSet3DView(Gpt->view_d, Gpt->tilt, Gpt->skew, Gpt->rotate);
	return(OKAY);
}

PRIVATE int gpt_do_3Dtilt(void) {

	REAL tilt;
	tilt = LexGetReal(Gpt->tilt, "Tilt about X axis (no change): ");
	LEXESCAPE;
	Gpt->tilt   = tilt;
	PlotSet3DView(Gpt->view_d, Gpt->tilt, Gpt->skew, Gpt->rotate);
	return(OKAY);
}

PRIVATE int gpt_do_3Drotate(void) {

	REAL rotate;
	rotate = LexGetReal(Gpt->rotate, "Rotation about Z axis (no change): ");
	LEXESCAPE;
	Gpt->rotate = rotate;
	PlotSet3DView(Gpt->view_d, Gpt->tilt, Gpt->skew, Gpt->rotate);
	return(OKAY);
}

PRIVATE int gpt_do_3Dmesh(void) {
	int i0,i1;
	i0 = LexGetInt(Gpt->mesh[0], "Number of constant X columns to draw (no change): ");
	LEXESCAPE;
	i1 = LexGetInt(Gpt->mesh[1], "Number of constant Y rows to draw (no change): ");
	LEXESCAPE;
	Gpt->mesh[0] = i0;
	Gpt->mesh[1] = i1;
	return(OKAY);
}
	
PRIVATE int gpt_do_3Dresol(void) {
	int i0,i1;
	i0 = LexGetInt(Gpt->resolution[0], "Number of points in X on each constant Y line (no change): ");
	LEXESCAPE;
	i1 = LexGetInt(Gpt->resolution[1], "Number of points in Y on each constant X line (no change): ");
	LEXESCAPE;
	Gpt->resolution[0] = i0;
	Gpt->resolution[1] = i1;
	return(OKAY);
}
	
PRIVATE int gpt_do_3Dhidden(void) {
	LOGICAL ltmp;
	char token[OPTION_STR_SIZE];
	int i;

	ltmp = LexGetTokenP(token, sizeof(token), "Hidden line suppression (on|panel|off - no change): ");
	LEXESCAPE;
	if (ltmp) {
		i = LexSelect(token,"OFF NO ON YES PANEL");
		if (i <= 0) {
			ERRprintf("ERROR: Unrecognized mode (%s) for HIDDEN\n", token);
			return(NOMORE);
		}
		if (i == 1 || i == 2) Gpt->HideLines = HIDDEN_OFF;
		if (i == 3 || i == 4) Gpt->HideLines = HIDDEN_ON;
		if (i == 5)           Gpt->HideLines = HIDDEN_PANEL;
	}
	return(OKAY);
}

PRIVATE int gpt_do_3Dbox(void) {				/* Draw a 3D box */
	int i;
	struct {
		REAL x,y,z;
		int  ipen;
	} v[] = {
		{0.0f,0.0f,0.0f, 3}, {1.0f,0.0f,0.0f, 2}, {1.0f,1.0f,0.0f, 2}, {0.0f,1.0f,0.0f, 2}, {0.0f,0.0f,0.0f, 2},
		{0.0f,0.0f,1.0f, 2}, {1.0f,0.0f,1.0f, 2}, {1.0f,1.0f,1.0f, 2}, {0.0f,1.0f,1.0f, 2}, {0.0f,0.0f,1.0f, 2},
		{1.0f,0.0f,0.0f, 3}, {1.0f,0.0f,1.0f, 2},
		{0.0f,1.0f,0.0f, 3}, {0.0f,1.0f,1.0f, 2}, 
		{1.0f,1.0f,0.0f, 3}, {1.0f,1.0f,1.0f, 2},
		{-1.f,-1.f,-1.f, -1}
	};
			
	PlotSet3DRange(-0.0001f,1.0001f, -0.0001f,1.0001f, -0.0001f,1.0001f);
	PlotSet3DView(Gpt->view_d, Gpt->tilt, Gpt->skew, Gpt->rotate);
	for (i=0; v[i].ipen > 0; i++)	PlotMove3D(v[i].x, v[i].y, v[i].z, v[i].ipen);
	PlotFlush();
	return(OKAY);
}

/* ============================================================================
-- ... Function to query user for one of the axis types
--     [ BOTTOM LEFT TOP RIGHT  or  ZAXIS ]
--
--     Usage:   int = GetAxisName(void);
--
--     Inputs:  none
--
--     Output:  none
--
--		 Returns: number of the axis  1 => BOTTOM       3 => TOP
--                                  2 => LEFT         4 => RIGHT
--                                  5 => ZAXIS
--                                 -1 => nothing     
--                                 -2 => error
--                                 -3 => AUTO
--                                 -4 => 3DRANGE (sets all)
============================================================================ */
PRIVATE int GetAxisName(void) {

	typedef struct _CMAXIS {					/* Command structure		*/
		const char *command;
		int minlen;
		int rcode;
	} CMAXIS;

	static const CMAXIS cmlist[] =						/* Plot/Data describe */
	{	{"auto",			1,	-3},
		{"x",				1,	 0},
		{"y",				1,	 1},
		{"z",				1,	 4},
		{"bottom",		1,	 0},
		{"left",			1,	 1},
		{"top",			1,	 2},
		{"right",		1,	 3},
		{"2DRange",    2, -4},
		{"3DRange",		2,	-5},
		{NULL,			0,	 0}
	};

	CMAXIS *citem;

	char token[OPTION_STR_SIZE];
	
	if (! LexGetTokenP(token,sizeof(token), "[X Y Z Bottom Left Top Right]: ")) 
		return(-1);
	citem = LexCmdl(token, cmlist, sizeof(CMAXIS));

	if (citem == NULL) {
		ERRprintf("ERROR: %s is an unrecognized axis name\n", token);
		return(-2);
	}
	return(citem->rcode);
}

/* ============================================================================
-- Function to archive a curve to memory for later retrieval
--
-- Usage: LOGICAL = arc_data(x,y,npt,nptmax,ids)
--
-- Inputs: x,y,npt,nptmax,ids - Main memory curve
--
-- Output: (1) Requests a curve name <name>
--         (2) Creates dynamic variables <name>:X, <name>:Y, <name>:NPT
--         (3) Copies main curve to the CURVE <name>
============================================================================ */
static char ArchiveHelp[] =
" Copies the current curve into a GENPLOT variable (saves copy).  This\n"
" is only local and is lost when GENPLOT exits.  But the curve can be\n"
"\n"
"   ARCHive <cname> [-append]\n"
"\n"
" The -append option appends the current data to the end of an existing\n"
" curve.  The existing curve must have the same 2D/3D type\n"
"\n"
"Examples:\n"
"   archive c1\n";

PRIVATE int gpt_do_archive(void) {

	char name[VARNAME_STR_SIZE], token[OPTION_STR_SIZE];
	int  itype, nkeep;
	CURVE **aptr=NULL, *target, *source;
	BOOL append=FALSE, mode_3d;
	
/* Check for help request */
	if (LexCheckHelp("ARCHIVE - Save curve as a variable", ArchiveHelp, NULL)) return(OKAY);

/* Set default options */
	source  = GptCurve;
	mode_3d = (source->z != NULL) ? GV_3DCURVE : GV_2DCURVE;		/* Will we be doing 3D? */
	append  = FALSE;															/* Initially no append	*/

/* Get the curve to file away as */
	if (! LexGetTokenP(name, sizeof(name), "Curve name: ")) return(OKAY);
	LEXESCAPE;
	
/* And check for options */
	while (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-append", 2)) {
			append = TRUE;
		} else {
			ERRprintf("ERROR: %s in an unrecognized archive option\n", token);
			return(NOMORE);
		}
	}
		
/* Do the append mode first since I know how to do it */
	if (append) {															/* Special case */
		if (! GVGetInfo(name, &itype, (void **) &aptr)) {
			ERRprintf("ERROR: The curve (%s) does not already exist.\n", name);
			return(NOMORE);
		} else if (itype != mode_3d) {
			ERRprintf("ERROR: Current curve (2D/3D) does not match existing curve (%s)\n", name);
			return(NOMORE);
		}
		target = (CURVE *) (*aptr);							/* Target location */
		if (target->npt+source->npt > target->nptmax) {
			nkeep = target->npt+source->npt;				/* Minimum required size */
			nkeep += nkeep/5;									/* Add 20% so not everytime */
			if (! GVResize(name, nkeep)) {
				ERRprintf("ERROR: Unable to increase the size of %s\n", name);
				return(NOMORE);
			} else if (! GVGetInfo(name, &itype, (void **) &aptr)) {
				ERRprintf("ERROR: Huh? Had link to %s but lost it on resize\n", name);
				return(NOMORE);
			}
			target = (CURVE *) (*aptr);						/* Reset target location */
		}
		nkeep = target->npt;

	} else {
		if (! ((GptCurve->z!=NULL) ? GVAlloc3DCurve(name,GVF_USER,GptCurve->npt) : GVAlloc2DCurve(name,GVF_USER,GptCurve->npt)) ) {
			ERRprintf("ERROR:  Unable to alloc a 2D/3D curve for %s\n", name);
			return(NOMORE);
		} else if (! GVGetInfo(name, &itype, (void **) &aptr) || itype != mode_3d) {
			ERRprintf("ERROR: Thought I allocated the curve, but it isn't expected type\n");
			return(NOMORE);
		}
		target = (CURVE *) (*aptr);							/* Target location */
		strcpy(target->ids, source->ids);					/* Copy the options and id for full append */
		strcpy(target->options, source->options);
		nkeep = 0;
	}

/* And do the copy of the data, potentially offset by nkeep */
	memcpy(target->x+nkeep, source->x, source->npt*sizeof(*source->x));
	memcpy(target->y+nkeep, source->y, source->npt*sizeof(*source->y));
	if (mode_3d == GV_3DCURVE) memcpy(target->z+nkeep, source->z, source->npt*sizeof(*source->z));
	target->npt = source->npt + nkeep;

	return(OKAY);
}

/* ============================================================================
-- Function to retrieve a curve to current status
--
-- Usage: LOGICAL = ret_data(name,X,Y,NPT,NPTMAX)
--
-- Inputs: NPTMAX - Maximum number of points allowed in X,Y curve
--         NAME   - Name of curve to retrieve
--
-- Outputs: X,Y,NPT - Requested curve from memory
============================================================================ */
static char RetrieveHelp[] =
" Copies a saved GENPLOT variable curve into the main work curve.  Normally\n"
" this curve would have been created by an ARCHIVE command.\n"
"\n"
"   RETRieve <cname> [-append]\n"
"\n"
" The -append option appends the curve onto the current data.\n"
"\n"
" Notes:\n"
"  (1) For convenience, the command can also retrieve an array.  X values are\n"
"      set to the point number.\n"
"  (2) For simple retrieve, GENPLOT takes on the 2D/3D character of cname\n"
"  (3) For -append mode, the Z value is either ignored or set to point number.\n"
"\n"
"Examples:\n"
"   retr c1\n";

PRIVATE int gpt_do_retrieve(void) {

	char name[VARNAME_STR_SIZE], token[OPTION_STR_SIZE];
	INT i, istart;
	int itype, ipt;
	void **aptr=NULL;
	CURVE *curve;
	ARRAY *array;
	REAL *x, *y, *z;

/* Check for help request */
	if (LexCheckHelp("RETRIEVE - Restore a saved curve into main curve", RetrieveHelp, NULL)) return(OKAY);

/* And get the name */
	if (! LexGetTokenP(name, sizeof(name), "Curve name: ")) return(OKAY);
	LEXESCAPE;

/* Determine whether to append or just retrieve */
	if (LexGetOption(token, sizeof(token))) {
		if (! LexEqual(token, "-append", 3)) {
			ERRprintf("ERROR: %s in an unrecognized retrieve option", token);
			return(NOMORE);
		}
		istart = GptCurve->npt;						/* Try copying to next point */
	} else {
		istart = 0;
	}

/* See if the archived curve name really exists */
	if (! GVGetInfo(name, &itype, (void **) &aptr)) {
		gen_err2("Requested name does not exist", name);
		return(NOMORE);
	}

/* Determine address of arrays to append */
	if (itype == GV_2DCURVE || itype == GV_3DCURVE) {
		curve = (CURVE *) (*aptr);							/* Curve structure */
		strcpy(GptCurve->ids, curve->ids);				/* Copy new structure */
		strcpy(GptCurve->options, curve->options);
		x   = curve->x;										/* Memory locations */
		y   = curve->y;
		z   = curve->z;
		ipt = curve->npt;										/* Number of points */

	} else if (itype == GV_ARRAY || itype == GV_ARRAY_LINK) {
		array = (ARRAY *) (*aptr);
		sprintf(GptCurve->ids, "Array: %s", name);
		*GptCurve->options = '\0';
		x = NULL;
		y = array->x;
		z = NULL;
		ipt = *array->size;

	} else {
		gen_err2("Requested name is not a curve or an array", name);
		return(NOMORE);
	}

/* Switch GptCurve to 2D/3D to retrieve this curve - if not appending */
	if (istart == 0) {							
		if (z!=NULL && GptCurve->z==NULL && GVModifyCurve(GptUseCurve,GV_3DCURVE)) {
			GptLinkXYZ(GptUseCurve);							/* Relink now */
			Gpt->mode_3d = TRUE;
			TTYputs("Curve now 3-dimensional\n");
		} else if (z==NULL && GptCurve->z!=NULL && GVModifyCurve(GptUseCurve,GV_2DCURVE)) {
			GptLinkXYZ(GptUseCurve);							/* Relink now */
			Gpt->mode_3d = FALSE;
			TTYputs("Curve now 2-dimensional\n");
		}
	}
		
/* Increase size if necessary */
	if (istart + ipt > GptCurve->nptmax) {
		if (! GVResize(GptUseCurve, istart+ipt)) {
			gen_err2("Unable to increase size to allow retrieve", GptUseCurve);
			return(NOMORE);
		}
		GptLinkXYZ(GptUseCurve);							/* Relink now */
	}
	GptCurve->npt = istart + ipt;

/* And, either copy over or assign as point number */
	if (x != NULL) memcpy(GptCurve->x + istart, x, sizeof(REAL)*ipt);
	if (y != NULL) memcpy(GptCurve->y + istart, y, sizeof(REAL)*ipt);
	if (x == NULL) for (i=istart; i<GptCurve->npt; i++) GptCurve->x[i] = (REAL) i;
	if (y == NULL) for (i=istart; i<GptCurve->npt; i++) GptCurve->y[i] = (REAL) i;
	if (GptCurve->z != NULL) {
		if (z != NULL)	memcpy(GptCurve->z + istart, z, sizeof(REAL)*ipt);
		if (z == NULL) for (i=istart; i<GptCurve->npt; i++) GptCurve->z[i] = (REAL) i;
	}

	return(OKAY);
}


/* ============================================================================
-- Subroutine to set configuration parameters.
--
-- Usage: int GptConfig(int key)
--
-- Inputs: key - Normal key codes
--               0       - enter command entry mode
--               U_INIT  - initialize
--               U_RESET - reset
--               U_HELP  - ignored (internal process has help list)
--               U_PARM  - parameters listing
--               U_QUIT  - prepare to quit
--
-- Returns: 0 => success
--
-- Output: Reset common block variables to initial values
============================================================================ */
static int GptConfig(int key) {

	typedef enum _OPS2 {
		CF_HELP, CF_PARM, CF_RETURN,
			CF_VALUE, CF_DPATH, CF_DEXTS, CF_TABCOMPLETION, CF_MACHINE
	} OPS2;

	typedef struct _CMTYPE2 {
		CHAR *command;
		INTEGER minlen;
		OPS2 rcode;
		int bit;
	} CMTYPE2;

	static const CMTYPE2 options[] = {
		{"?",						1, CF_HELP, 0},	{"help",				  -1,  CF_HELP, 0},
		{"status",				4,	CF_PARM, 0},	{"parms",			  -4,	 CF_PARM, 0},
		{"parameters",		  -4,	CF_PARM, 0},

		{"value",					4,  CF_VALUE,  0},
		{"confirmoverwrite",		7,  CF_VALUE, +OpConfirmOverwrite},
		{"binarywrite",			6,  CF_VALUE, +OpBinaryWrite},
		{"asciiwrite",				5,  CF_VALUE, -OpBinaryWrite},
		{"noconfirmoverwrite",	7,  CF_VALUE, -OpConfirmOverwrite},
		{"nobinarywrite",			9,  CF_VALUE, -OpBinaryWrite},
		{"noasciiwrite",			8,  CF_VALUE, +OpBinaryWrite},
		{"tabcompletion",			3,	 CF_TABCOMPLETION, 0},

		{"datapath",			5,  CF_DPATH, 0},		{"dpath", -5, CF_DPATH,0},
		{"dataexts",			5,	 CF_DEXTS, 0},
		{"machine_info",		7,	 CF_MACHINE, 0},
		{"genplot",				3,  CF_RETURN, 0},	{"xgenplot", -3,  CF_RETURN, 0},
		{"return",				3,  CF_RETURN, 0},
		{NULL,					0,  CF_RETURN, 0}
	};

	char token[LONG_STR_SIZE], *aptr;
	int i;
	LOGICAL DoneAtLeastOne;
	const CMTYPE2 *citem;

	switch (key) {
		case U_INIT:											/* First time init */
			break;
		case U_RESET:
			break;
		case U_PARM:
			TTYprintf(" Data file search path: %s\n", GptSearchPath);
			TTYprintf(" Data file extensions:  %s\n", GptSearchExts);
			break;
	}
	if (key != 0) return(0);
	DoneAtLeastOne = FALSE;					/* No commands executed this loop */

	while (TRUE) {
		if (! LexGetTokenP(token, sizeof(token), "Configuration subcommand: ")) {
			continue;
		} else if (LexAlias(token, sizeof(token))) {
			continue;							/* Check for alias */
		} else {
			aptr = token;
			if (*aptr == '-') aptr++;
			if ( (citem = LexCmdl(aptr, options, sizeof(CMTYPE2))) == NULL) {
				if (LexSystem(0, token)) continue;
				if (DoneAtLeastOne) {
					LexBackup();
					return(0);
				} else {
					ERRprintf("ERROR: %s unrecognized CONFIGURE sub-command\n"
								 "       Use RETURN to return to main level\n", token);
					LexFlush();
					continue;
				}
			}
		}

		DoneAtLeastOne = TRUE;				/* Done one command this loop */
		switch (citem->rcode) {
			case CF_HELP:
				LexCmdlPrintEx(options, sizeof(CMTYPE2), 13, "Configure Subcommands:");
				LexSystem(U_HELP, NULL);
				break;
			case CF_PARM:
				TTYprintf("Valid configuration parameters (* ==> active):\n");
				for (citem=options; citem->command != NULL; citem++) {
					if (citem->minlen < 0) continue;
					strlwr(strcpy(token, citem->command));
					if (strncmp(token, "no", 2) == 0) continue;
					for (i=0; i<citem->minlen; i++) token[i] = toupper(token[i]);
					if (citem->rcode == CF_VALUE && citem->bit != 0) {
						i = (Gpt->OpMode & abs(citem->bit)) != 0;
						if (citem->bit < 0) i = !i;
						TTYprintf(" %c %s\n", (i) ? '*' : ' ', token);
					} else if (citem->rcode == CF_DPATH) {
						TTYprintf("   %s --> %s\n", token, GptSearchPath);
					} else if (citem->rcode == CF_DEXTS) {
						TTYprintf("   %s --> %s\n", token, GptSearchExts);
					} else if (citem->rcode == CF_TABCOMPLETION) {
						TTYprintf("   %s --> %s\n", token, (TabCompletionMode == FULL_COMPLETE) ? "Full" : "Partial");
					} else {
						TTYprintf("   %s\n", token);
					}
				}
				TTYprintf("Some parameters may be disabled with NO, such as NOCONFIRM\n");

				TTYprintf("\nConfiguration parameters (set at initialization)\n");
				TTYprintf("  Support File Search Path: %s\n", SysGetSysSearchPath());
#ifdef NT
				SysResolveDyntName(token, "genplot_.ini", sizeof(token));
				TTYprintf("  Most likely genplot_.ini: %s\n", token);
#endif
				TTYprintf("\n");

				break;
			case CF_RETURN:
				return(0);

			case CF_MACHINE:
				{
					struct utsname info;
					TTYprintf("Uname() function returns: %d\n", uname(&info));
					TTYprintf("   OS:        %s\n", info.sysname);
					TTYprintf("   Domain:    %s\n", info.nodename);
					TTYprintf("   Processor: %s\n", info.machine);
					TTYprintf("   Release:   %s\n", info.release);
					TTYprintf("   Version:   %s\n", info.version);
				}
				break;
				
			case CF_VALUE:
				if (citem->bit == 0) {							/* Specific set value */
					Gpt->OpMode = LexGetInt(Gpt->OpMode, "Config option word (unchanged): ");
				} else if (citem->bit > 0) {					/* Either set option */
					Gpt->OpMode |= citem->bit;
				} else {												/* Or clear option */
					Gpt->OpMode &= ~abs(citem->bit);
				}
				break;
			case CF_DPATH:
				if (LexGetSearchPathP(token, sizeof(token), "Search path(s) for reading data files (unchanged): "))
					strcpy(GptSearchPath, token);
				break;
			case CF_DEXTS:
				if (LexGetSearchPathP(token, sizeof(token), "Extension list for data file search (unchanged): "))
					strcpy(GptSearchExts, token);
				break;
			case CF_TABCOMPLETION:
				if (LexGetTokenP(token, sizeof(token), "Tab filename completion mode (Partial | Full): ")) {
					if (LexEqual(token, "Partial", 1)) {
						TabCompletionMode = PARTIAL_COMPLETE;
					} else if (LexEqual(token, "Full", 1)) {
						TabCompletionMode = FULL_COMPLETE;
					} else {
						ERRprintf("ERROR: Invalid type of filename completion modes (%s)\n", token);
					}
				}
				break;
			default:
				ERRprintf("ERROR: Developers blew it again (PERT)\n");
				break;
		}
	}
	panic; return(1);
}
