/* find_drv.c - routine to find and return device driver from name */

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

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "tplot.h"
#include "complot.h"
#include "plotdefs.h"

#include "drvclass.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */
PLOTDRIVER	Null_Driver;
PLOTDRIVER	PostScript_Driver;
PLOTDRIVER	HPGL_Driver;
PLOTDRIVER	Tektronix_Driver;
PLOTDRIVER	XML_Driver;
PLOTDRIVER	Debug_Driver;
PLOTDRIVER	Pipes_Driver;
PLOTDRIVER	X_Driver;
PLOTDRIVER	WPG_Driver;
PLOTDRIVER	Laser_Driver;

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
typedef struct _DEVPAIR {
	CHAR *name;
	PLOTDRIVER *routine;
	int class;
} DEVPAIR;

static DEVPAIR DevList[] = 
		  { {"HP-GL",		HPGL_Driver,			-1}		/* HP-GL language */
			,{"TEKTRX",		Tektronix_Driver,		-1}		/* Tektrx plotters */
			,{"POSTDRV",	PostScript_Driver,	-1}		/* Postscript driver */
/* ---- */
			,{"LASER",		Laser_Driver,			-1}			/* LaserJet			*/
			,{"LASERJET",	Laser_Driver,			LJET_II}
			,{"LASERJET3",	Laser_Driver,			LJET_III}
			,{"LASERJET4",	Laser_Driver,			LJET_IV}
			,{"LJET_II",	Laser_Driver,			LJET_II}
			,{"LJET_III",	Laser_Driver,			LJET_III}
 			,{"LJET_IV",	Laser_Driver,			LJET_IV}
			,{"DESKJET",	Laser_Driver,			DJET}			/* DeskJet			*/
			,{"DJET",		Laser_Driver,			DJET}
			,{"DJET_500",	Laser_Driver,			DJET_500}
			,{"DJET_PLUS",	Laser_Driver,			DJET_PLUS}
			,{"DJET_500C",	Laser_Driver,			DJET_500C}
			,{"DJET_550C",	Laser_Driver,			DJET_550C}
			,{"PAINTJET",	Laser_Driver,			PAINTJET}	/* PaintJet			*/
			,{"QUIETJET",	Laser_Driver,			QUIETJET}	/* QuietJet			*/
			,{"IBM4019",	Laser_Driver,			IBM4019}		/* IBM HPmode		*/
			,{"MX80",		Laser_Driver,			MX80}			/* Epson MX-80		*/
			,{"OKIDATA",	Laser_Driver,			OKIDATA}		/* Okidata			*/
			,{"LQ500",		Laser_Driver,			LQ500}		/* LQ500 printer	*/
			,{"LQ800",		Laser_Driver,			LQ800}		/* Epson 24 pin	*/
			,{"GEMINI",		Laser_Driver,			GEMINI}		/* Gemini			*/
			,{"X24E",		Laser_Driver,			IBM_X24E}	/* IBM X24E 24pin	*/
			,{"TIFF",		Laser_Driver,			TIFF_B}		/* Tiff driver		*/
/* ---- */
			,{"XDRIVER",	X_Driver,				0}	/* X Windows Driver	*/
			,{"WPDRV",		WPG_Driver,				0}	/* Word Perfect Driver */
			,{"REGIS",		Debug_Driver,			0}	/* Regis for DEC VT240 */

			,{"XML",			XML_Driver,				0} /* XML driver (Ian)	*/

			,{"DEBUG",		Debug_Driver,			0}	/* Null driver			*/
			,{"NULDRV",		Null_Driver,			0}	/* Null driver			*/
			,{NULL,			NULL,						0}	/* End of list			*/
		};


/* ===========================================================================
-- PlotFindDriver - returns appropriate pointer to operating routine
-- 
-- Usage: LOGICAL PlotFindDriver(char *drivername);
-- 
-- Inputs: drivername - ASCII name of the driver from devices.dat
-- 
-- Returns: Returns pointer to 
--         operating parameters such as pixels per inch etc.
=========================================================================== */
int PlotFindDriver(char *drivername, PLOTDRIVER **routine, int *class) {

	DEVPAIR *Ptr;

/* See if it can be satisfied using the pipe driver */
	if (*drivername == '!') {
		*class = 0;
		*routine = Pipes_Driver;
		return(0);
	}

/* If not, search our list, returning false if nothing found */
	for (Ptr=DevList; Ptr->name != NULL; Ptr++) {
		if (stricmp(drivername, Ptr->name) == 0) break;
	}
	if (Ptr->class != -1) *class = Ptr->class;
	*routine = Ptr->routine;
	return( (Ptr->routine == NULL) ? -1 : 0);
}
