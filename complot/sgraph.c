/* sgraph.c */

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
#include "lexp.h"								/* Needs GV routines */
#include "tplot.h"
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

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ============================================================================
-- Usage Guide:
--
--     SUBROUTINE SGRAPH - Routine to set random variables in COMPLOT common
--
--     This is a kludge routine to set some occasionally changed variables in
--     the COMPLOT.INS common blocks which programs need.  It is meant mainly
--     as a temporary measure till suitable routines are named and defined.
--     However, appears it may stay around for quite a while.
--     Too bad we didn't record when this 'temporary kludge' got started.
--     As of 3/87, it is quite a permanent fixture within this package.
--
--     Usage:     LOGICAL SGRAPH(PARAMETER,VALUE)
--
--     Inputs:    PARAMETER - Character string of variable to set (CHARACTER)
--                VALUE     - Value to put into parameter (REAL*4)
--
--     Output:    SGRAPH - .TRUE.  -> Able to set that parameter
--                         .FALSE. -> Not in the list probably
--
--     Note: Since this expects a real parameter, all integer values should
--           be sent with care so truncation doesn't cause it to fail, ie.
--           the value 1 should be sent as 1.001 or something similar.
--
--     Example: CALL SGRAPH('ZFORCE',0.1)
============================================================================ */
LOGICAL PlotSgraph(const char *token, REAL value) {

	PlotAnnoteSet(-1);									/* Initialize if needed */

	if ( token == NULL || *token == '\0' || (stricmp(token, "list") == 0) ) {
		TTYprintf("Symbol Parameters:\n"
			" SUSIZ:  %-10.4g SUBOFF: %-10.4g SUPOFF: %-10.4g\n",
			PL_Symbols.ScriptSize, PL_Symbols.SubscriptOffset, PL_Symbols.SuperscriptOffset);
		TTYprintf("Axis Parameters:\n"
			" TBOFF:  %-10.4g TTOFF:  %-10.4g TLOFF:  %-10.4g TROFF:  %-10.4g\n",
			PL_Axis.TitleOffset.Bottom, PL_Axis.TitleOffset.Top, PL_Axis.TitleOffset.Left, PL_Axis.TitleOffset.Right);
		TTYprintf(" CSMIN:  %-10.4g CSMAX:  %-10.4g TSIZE:  %-10.4g\n",
			PL_Axis.MinLabelSize, PL_Axis.MaxLabelSize, PL_Axis.TitleSize);
		TTYprintf(" TICK:   %-10.4g TICK2:  %-10.4g AXCOLR: %-3.0f        AXWIDTH: %-9.3g\n",
			PL_Axis.MajorTick, PL_Axis.MinorTick, PL_Axis.Color.Line , PL_Axis.Width.Line);
		TTYprintf("Identify Parameters:\n"
			" IDLSKP: %-10.4g IDTSKP: %-10.4g IDSIZE: %-10.4g\n", 
			PlotWindow->ID.leftskip, PlotWindow->ID.topskip, PlotWindow->ID.size);
		TTYprintf(" IDSPAC: %-10.4g IDLSIZ: %-10.4g\n",
			PlotWindow->ID.spacing, PlotWindow->ID.linesize);
		TTYprintf("Other Parameters:\n"
			" ZFORCE: %-10.4g \n", PlotWindow->zforce);
		PlotAnnoteSet(0);											/* List ANNOTE vars */
	} else {
		char tokname[VARNAME_STR_SIZE], tokval[DFLT_STR_SIZE];
		strcpy(tokname, "$"); strcat(tokname, token);
		sprintf(tokval, "%g", (double) value);
		return(GVSetValue(tokname, tokval));
	}
	return(TRUE);
}
