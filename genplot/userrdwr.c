/* <routine name> */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"
#include "gptxtrn.h"

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

/*============================================================================
-- User function to process data (or otherwise)
--
-- Usage: LOG = GptDefaultUserFnc(char *UseCurve);
--
-- Inputs: Filename - file to read
--	        UseCurve - "name of curve" from Genplot
--
-- Externals: CURVE GptCurve -- linkage to the current GENPLOT curve
--
-- Output: GptCurve->x, GptCurve->y - Filled x,y data buffers
--         GptCurve->npt            - Number of points in data set
--         GptCurve->ids            - ID of structure, or filename
--
-- Returns:  0 ==> all is okay
--          -1 ==> error during read
--          +1 ==> requested exit from read
============================================================================ */
int GptDefaultUserFnc(char *UseCurve) {
	ERRprintf("ERROR: No USER function currently loaded - use USER -LOAD first\n");
	return(FALSE);
}

/*============================================================================
--     Function to read data files for genplot
--
--     Usage: LOG = GptDefaultUserRead(char *FileName, char *UseCurve);
--
--     Inputs: Filename - file to read
--	            UseCurve - "name of curve" from Genplot
--
--     Externals: CURVE GptCurve -- linkage to the current GENPLOT curve
--
--     Output: GptCurve->x, GptCurve->y - Filled x,y data buffers
--             GptCurve->npt            - Number of points in data set
--             GptCurve->ids            - ID of structure, or filename
--
--     Returns:  0 ==> all is okay
--              -1 ==> error during read
--              +1 ==> requested exit from read
============================================================================ */
int GptDefaultUserRead(char *FileName, char *UseCurve) {
	ERRprintf("ERROR: No USER read currently loaded - use USER -LOAD first\n");
	return(-1);
}

/*============================================================================
--     Function to write data files from genplot
--
--     Usage: LOG = GptDefaultUserWrite(char *FileName, char *UseCurve);
--
--     Inputs: Filename - file to write
--	            UseCurve - "name of curve" from Genplot
--
--     Externals: CURVE GptCurve -- linkage to the current GENPLOT curve
--
--     Inputs: GptCurve->x, GptCurve->y - Filled x,y data buffers
--             GptCurve->npt            - Number of points in data set
--             GptCurve->ids            - ID of structure, or filename
--
--     Returns:  0 ==> all is okay
--              -1 ==> error during write
--              +1 ==> requested exit from write
============================================================================ */
int GptDefaultUserWrite(char *FileName, char *UseCurve) {
	ERRprintf("ERROR: No USER write currently loaded - use USER -LOAD first\n");
	return(-1);
}
