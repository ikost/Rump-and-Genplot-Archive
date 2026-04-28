/* DSPTCH - Dispatch routine for all graphics functions */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

#ifdef SLAVE
	#define Null_Driver SlaveDriver
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

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "complot.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

typedef struct _DRVBLOCK DRVBLOCK;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
LOGICAL Null_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ---------------------------------------------------------------------------
-- NullDriver - Null driver for testing software
--
-- Usage: LOGICAL Null_Driver
--
-- Inputs: cmd   - Command - See COMPLOT.INS for definitions
--         PARMS - Variable dimensioned array with parameters for transfer
--                 Type and direction depend on command.
--
-- Output: DRVNUL - Success of operation
--
-- NOTE: DRVNUL is a character stream device.  Output channels may be
--       selected in DEVICES.DAT.  The RS-232 line format is enabled
--       with CR/LF following each line output.
--------------------------------------------------------------------------- */
LOGICAL Null_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp) {

	if (key == INIFNC) {							/* Initialize */
		dsp->ini.xperinch = 1000;
		dsp->ini.yperinch = 1000;
		dsp->ini.xmax     = 11000;
		dsp->ini.ymax     = 8500;
		dsp->ini.Capabilities = DEV_CAP_GRAPHICS  |	/* Supports graphs	*/
										DEV_CAP_MARKERS   |	/* Can do markers    */
										DEV_CAP_FONTS	   |	/* Can do characters  */
										DEV_CAP_GREEKFONT |	/* Can do greek characters */
										DEV_CAP_CLIP;			/* Will get clip msgs */
	}
	return(TRUE);
}
