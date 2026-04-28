/* PLSUBS.F77  - Low level calls to drivers */

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

#include "lexp.h"
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
void sv_in0(int code);
void sv_in1(int code, int i1);
void sv_in2(int code, int i1, int i2);
void sv_rl1(int code, REAL x1);
void sv_rl2(int code, REAL x1, REAL x2);
void sv_rl4(int code, REAL x1, REAL x2, REAL x3, REAL x4);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */


/* ===========================================================================
--  PlotSelectPage - Force next graph to a new page even if in SubPage mode
--
--  Usage: PlotNewPage(int page)
--
--  Inputs: page - pseudo-page number on sheet
--                  -1 ==> new page
--                   0 ==> next valid page
--                  +n ==> specific page number (mod rows*cols)
--
--  Output:  internal parameter settings only
--
--  Returns: nothing
--
--  Notes: (1) Replaces both FRAME and ERASE in earlier versions.  This single
--             routine has task of providing a new drawing surface, either the
--             entire screen or just the sub-plot area.
--         (2) Most important on VERSATEC and PRINTRONIX type devices to begin
--             a new page.  Does automatic flush of buffers before terminating
--             the plot page.  Resets all plotting parameters to initialization.
--             FRAME does an implicit FLUSH first. - In the low level drivers.
--         (3) Informs driver whether in flipxy or normal mode BEFORE frame
--             command.  Thus frame can decide to handle differently
--         (4) More complex now with SubPages.  Basically, FRAME will handle
--             all.  If request for frame is made with page beyond the valid
--             count, the page will be framed and erased.   Otherwise, neither.
=========================================================================== */
void PlotNewPage(int page) {		/* Select a new page */
	
	DspFrmfnc dsp;
	DspErsfnc erase;

/* Set up the commands in case we need to send them later */
	dsp.orient = (int) PlotWindow->orient;		/* Must be 0, 90, 180, 270 */
	if (PlotWindow->PageFillColor >= 0) {
		PlotSetBrush(PlotWindow->PageFillColor, &erase.brush);
	} else {											/* Define no page coloring */
		PlotSetBrush(CLR_NOMARK, &erase.brush);
	}

/* Simple case w/ single plot on page.  Clear HCOPY, FRAME and ERASE */
	if (! PL_SubPage.active) {
		if (PlotWindow->Save.On) SV_Request(SVKEY_ERASE, NULL);		/* Full reset */
		(*DEVICE->dsptch)(FRMFNC, DEVICE->DriverBlock, (DSP *) &dsp);
		(*DEVICE->dsptch)(ERSFNC, DEVICE->DriverBlock, (DSP *) &erase);
		return;
	}

/* Complex case -- Subpages are active */
	if (page < 0) {						/* This is request for new physical page */
		if (! PL_SubPage.page_set) PL_SubPage.page = 0;
		PL_SubPage.page_set   = TRUE;
		PL_SubPage.need_erase = TRUE;
	} else if (page == 0) {
		if (! PL_SubPage.page_set) PL_SubPage.page++;
		PL_SubPage.page_set = FALSE;
		if (PL_SubPage.page >= PL_SubPage.ncols*PL_SubPage.nrows) {
			PL_SubPage.page = 0;
			PL_SubPage.need_erase = TRUE;
		}
	} else {
		PL_SubPage.page = (page-1) % (PL_SubPage.ncols*PL_SubPage.nrows);
		PL_SubPage.page_set = TRUE;
	}
	PlotFixInternal();					/* Set in place now */

	if (PL_SubPage.need_erase) {
		PL_SubPage.need_erase = FALSE;
		if (PlotWindow->Save.On) SV_Request(SVKEY_ERASE, NULL);	/* Full reset */
		(*DEVICE->dsptch)(FRMFNC, DEVICE->DriverBlock, (DSP *) &dsp);
		(*DEVICE->dsptch)(ERSFNC, DEVICE->DriverBlock, (DSP *) &erase);
	} else {
		sv_in1(SV_NEWPAGE, page);					/* This is the basic command */
	}

	return;
}

/* ===========================================================================
--     ERASE - Erase the display on devices capable of it.
--
--    Usage: CALL ERASE
--
-- Call is obsolete.  Use PlotSelectPage instead.  No longer in tplot.h.
=========================================================================== */
void PlotErase(void) {

	DspErsfnc erase;
	
	ERRprintf("DEVELOPMENT NOTE: PlotErase was called unexpectedly\n");

	if (PL_SubPage.active) return;			/* Handled by FRAME only */

	if (PlotWindow->Save.On) SV_Request(SVKEY_ERASE, NULL);
	PlotSetBrush(CLR_NOMARK, &erase.brush);
	(*DEVICE->dsptch)(ERSFNC, DEVICE->DriverBlock, (DSP *) &erase);
	return;
}


/* =============================================================================
--     PlotFrame - Advance to next plot frame for appropriate devices.
--
--     Usage: CALL PlotFrame
--
-- Notes: (1) Most important on VERSATEC and PRINTRONIX type devices to begin
--            a new page.  Does automatic flush of buffers before terminating
--            the plot page.  Resets all plotting parameters to initialization.
--            FRAME does an implicit FLUSH first. - In the low level drivers.
--        (2) Informs driver whether in flipxy or normal mode BEFORE frame
--            command.  Thus frame can decide to handle differently
--        (3) More complex now with SubPages.  Basically, FRAME will handle
--            all.  If request for frame is made with page beyond the valid
--            count, the page will be framed and erased.   Otherwise, neither.
--
-- Call is obsolete.  Use PlotSelectPage instead.  No longer in tplot.h.
============================================================================= */
void PlotFrame(void) {

	DspFrmfnc dsp;

	ERRprintf("DEVELOPMENT NOTE: PlotFrame was called unexpectedly\n");

	if (PlotWindow->Save.On) SV_Request(SVKEY_ERASE, NULL);	/* And reset HCOPY */
	dsp.orient = (int) PlotWindow->orient;		/* Must be 0, 90, 180, 270 */
	(*DEVICE->dsptch)(FRMFNC, DEVICE->DriverBlock, (DSP *) &dsp);

	return;
}


/* =============================================================================
--     PlotFlush - Subroutine to flush the internal buffer of devices.
--
--     Usage: PlotFlush();
--
--     Notes: Most device drivers include an internal working array to hold
--            plot commands and minimize calls to I/O routines.  This routine
--            forces an output of those buffers.  Implicit in the call is also
--            a call to reset the device to alphanumerics mode if available.
--            Should be called after any plotting and before TERMINAL I/O.
--            Flush is also an implicit ANMODE.
============================================================================= */
void PlotFlush(void) {

	sv_in0(SV_FLUSH);													/* Handle HCOPY */
	(*DEVICE->dsptch)(FLSFNC, DEVICE->DriverBlock, NULL);	/* Flush the buffers	*/
	return;
}


/* ============================================================================
--     PlotSelectPen - Routine to set the color of subsequent plotting
--
--     Usage: INTEGER = PlotSelectPen(IDAT)
--
--     Inputs: IDAT - Color to be used subsequently
--                    Negative numbers will not change the current color
--
--     Output: COLOR - Previous color used
============================================================================= */
#if 0
INTEGER PlotSelectPen(INTEGER idat) {

	DspColfnc dsp;
	INTEGER ihold;
	
	ihold = PlotWindow->colour;
	if (idat >= 0) {							/* Is this more than a query? */
		if (idat != ihold) {
			sv_in1(SV_COLOR, idat);			/* Handle HCOPY */
			PlotWindow->colour = idat;
			dsp.pen   = idat;
			dsp.color = (idat < PlotWindow->palette->num_entries) ? PlotWindow->palette->rgb[idat] : idat;
			(*DEVICE->dsptch)(COLFNC, DEVICE->DriverBlock, (DSP *) &dsp);	/* Set it in place */
		}
	}
	return(ihold);
}
#endif

/* ============================================================================
--     PlotSendPalette - Routine to send current palette to driver
--
--     Usage: PLOT_PALETTE *PlotSetPalette(PLOT_PALETTE *palette);
--
--     Inputs: palette - Palette to replace current default.  May be same
--                       as current palette in which case it will be resent
--                       to the driver (incorporating changes if appropriate)
--                  If NULL, nothing will be sent.  Query of palette only.
--
--     Output: none
--
--     Return: Pointer to current color palette
============================================================================= */
PLOT_PALETTE *PlotSetPalette(PLOT_PALETTE *palette) {

	int i, isize;
	DspPalette *dsp;

	if (palette != NULL && palette != PlotWindow->palette) {		/* Request to replace */
		isize = palette->num_entries;
		if (isize < 2)	{														/* Not allowed */
			ERRprintf("ERROR: Attempt to replace current color palette with fewer than 2 entries\n");
			palette = NULL;
		} else {
			isize = sizeof(PLOT_PALETTE) + isize*sizeof(palette->rgb[0]);
			free(PlotWindow->palette);
			PlotWindow->palette = malloc(isize);
			memcpy(PlotWindow->palette, palette, isize);
			GVLinkIntArray("$PALETTE", GVF_INTERNAL | GVF_HIDDEN , PlotWindow->palette->rgb, PlotWindow->palette->num_entries, NULL);
		}
	}

/* And send the palette out */
	if (palette != NULL) {
		dsp = malloc(sizeof(DspPalette) + palette->num_entries*sizeof(dsp->rgb[0]));
		dsp->num_entries = palette->num_entries;
		for (i=0; i<dsp->num_entries; i++) dsp->rgb[i] = palette->rgb[i];
		(*DEVICE->dsptch)(PALETTE, DEVICE->DriverBlock, (DSP *) dsp);
		free(dsp);
	}

	return(PlotWindow->palette);
}

/* ============================================================================
--     PlotSelectPen - Routine to set the color of subsequent plotting
--
--     Usage: INTEGER = PlotSelectColor(RGB)
--
--     Inputs: RGB - Color to be used
--
--     Output: COLOR - Previous color used
--
-- Three values are passed to the driver.  The pen number (if given as a
-- pen value), the RGB color requested (look up for pen values), and the
-- closed matching pen (palette) index to the requested color.
============================================================================= */
INT32 PlotSelectColor(INT32 color) {

	INTEGER ihold;
	DspColfnc dsp;

	ihold = PlotWindow->colour;
	if (color >= 0) {									/* Is this more than a query? */
		if (color != ihold) {
			sv_in1(SV_COLOR, color);				/* Handle HCOPY */
			PlotWindow->colour = color;
			PlotSetBrush(color, &dsp.brush);
			dsp.pen = dsp.brush.closest_index;
			(*DEVICE->dsptch)(COLFNC, DEVICE->DriverBlock, (DSP *) &dsp);
		}
	}

	return(ihold);
}

/* Simple routine to locate the closest pen within the current palette.
-- Necessary to handle drivers that don't handle RGB at all.  Pen 0 is
-- not allowed since it is not a real color, except if it matches exactly */
static int MatchRGB(INT32 rgb) {					/* Find closest match in set */

	int i, r,g,b, dmin, index;

/* First, check for exact match */
	for (i=0; i<PlotWindow->palette->num_entries; i++) {
		if (rgb == PlotWindow->palette->rgb[i]) return(i);
	}

/* Then look for nearest match - not allowing 0 to be matched */
	dmin = 256*256*3;				/* Max will be 255*255*3 in reality */
	index = 1;
	for (i=1; i<PlotWindow->palette->num_entries; i++) {
		r = R_FROM_RGB(rgb) - R_FROM_RGB(PlotWindow->palette->rgb[i]);
		g = G_FROM_RGB(rgb) - G_FROM_RGB(PlotWindow->palette->rgb[i]);
		b = B_FROM_RGB(rgb) - B_FROM_RGB(PlotWindow->palette->rgb[i]);
		if (r*r+g*g+b*b < dmin) {
			index = i; 
			dmin = r*r+g*g+b*b;
		}
	}
	return(index);
}
	
void PlotSetBrush(INT32 color, DspBrush *brush) {

	if (IS_PEN(color)) {							/* Just a palette index value	*/
		if (color >= PlotWindow->palette->num_entries) color = PlotWindow->palette->num_entries-1;
		brush->index = brush->closest_index = color;
		brush->rgb   = PlotWindow->palette->rgb[color];
	} else if (IS_TRANSPARENT(color)) {
		brush->index = brush->closest_index = -1;
		brush->rgb = CLR_TRANSPARENT;
	} else if (IS_NOMARK(color)) {
		brush->index = brush->closest_index = -1;
		brush->rgb = CLR_NOMARK;
	} else {											/* Only left is RGB select */
		brush->index = -1;						/* No real match */
		brush->closest_index = MatchRGB(color);
		brush->rgb = color;
	}
/*	TTYprintf("Calling brush with values: index=%i closest_index=%i rgb=%8.8x\n", brush->index, brush->closest_index, brush->rgb); */
	return;
}


/* =============================================================================
--     PlotSetAutoFlush - Set automatic flushing of buffers in some commands
--
--     Usage: LOGICAL = PlotSetAutoFlush(mode)
--
--     Inputs: mode - .TRUE.  => Autoflush on AXIS, etc. calls
--                    .FALSE. => Do not flush graphics buffers on return
--
--     Output: PlotSetAutoFlush - Previous mode
============================================================================ */
LOGICAL PlotSetAutoFlush(LOGICAL ldat) {

	LOGICAL hold;

	hold = DEVICE->Autoflush;
	DEVICE->Autoflush = ldat;
	return(hold);
}


/* =============================================================================
--     PlotSetLineWidth - Routine to set the line width of subsequent plotting
--
--     Usage: INTEGER = PlotSetLineWidth(IDAT)
--
--     Inputs: IDAT - Line style to be used subsequently
--                    Non-positive numbers will not affect the line style
--
--     Output: LSTYLE - Previous line style used
============================================================================= */
INTEGER PlotSetLineWidth(INTEGER idat) {
	
	INTEGER hold;
	DspLWfnc dsp;
	
/* If 0, set to 1 as minimum allowed */
	if (idat == 0) idat = 1;

	hold = PlotWindow->jstyle;
	if (idat > 0) {												/* More than a query? */
		if (idat != hold) {
			sv_in1(SV_LINESTYLE, idat);						/* Handle HCOPY */
			dsp.linewidth = PlotWindow->jstyle = idat;	/* Install value   */
			(*DEVICE->dsptch)(LWFNC, DEVICE->DriverBlock, (DSP *) &dsp);	
		}
	}
	return(hold);
}


/* =============================================================================
--     PlotSetVisibility - Function to set the visibility attribute for device
--
--     Usage: INTEGER = PlotSetVisibility(idat)
--
--     Inputs: idat - Type of visibility wanted
--                    1 -> normal visible line
--                    0 -> invisible line (erase)
--                   -1 -> complement bits along the way
--
--     Output: VISIBL - Old value
============================================================================= */
INTEGER PlotSetVisibility(INTEGER idat) {

	INTEGER hold;
	DspVisfnc dsp;

	hold = PlotWindow->visib;
	if (idat != hold) {
		sv_in1(SV_VISIBLE, idat);										/* Handle HCOPY */
		dsp.visible = PlotWindow->visib = idat;
		(*DEVICE->dsptch)(VISFNC, DEVICE->DriverBlock, (DSP *) &dsp);	/* Calls the dispatcher */
	}
	return(hold);
}


/* =============================================================================
--     PlotRequestHardCopy - Attempt to print the graphics screen (few devices)
--
--     Usage: PlotRequestHardCopy(void)
--
--     Inputs: none
--
--     Output: Send appropriate code to device to make a hardcopy if possible
--             TEK4010 - Sends <ESC><SUB> or whatever
--             IBMPC   - Does an INT 3 vector
============================================================================= */
LOGICAL PlotRequestHardCopy(void) {
	(*DEVICE->dsptch)(GRPFNC, DEVICE->DriverBlock, NULL);		/* And call for a print */
	return(TRUE);
}


/* =============================================================================
--     PlotEnterTextMode - Returns graphics terminals to alphanumerics mode.
--
--     Usage: PlotEnterTextMode(void);
--
--     Notes: A number of other routines have an implicit ANMODE call also.
--            This routine is especially important in raster graphics terminals
--            The routine will do an automatic flush of buffers also.
============================================================================= */
void PlotEnterTextMode(void) {
	
	(*DEVICE->dsptch)(ANMFNC, DEVICE->DriverBlock, NULL);		/* Send request */
	return;
}


/* =============================================================================
--     PlotSetPenSpeed - Routine to set the pen speed on some devices
--
--     Usage: PlotSetPenSpeed(SPD, PEN)
--
--     Inputs: SPD - New speed for PEN
--             PEN - Pen to be applied to (0 => ALL)
--
--     Notes: Really only useful for HP pen plotters.  Remember to have both
--            variables in call.
============================================================================= */
void PlotSetPenSpeed(INTEGER spd, INTEGER pen) {

	DspSpdfnc dsp;
	
	PlotWindow->spdsav = spd;							/* Save as default speed */
	dsp.speed = spd;										/* Set up for dsptch */
	dsp.pens  = pen;
	sv_in2(SV_SPEED, spd, pen);						/* Handle HCOPY */
	(*DEVICE->dsptch)(SPDFNC, DEVICE->DriverBlock, (DSP *) &dsp);
	return;
}


/* ============================================================================
--     PlotQueryDashes - Routine to query linetype and vector size
--
--     Usage: PlotQueryLineType(&ltyp,&vsize)
--
--     Output: LTYPE - Line type (if not null)
--             VSIZE - vector repeat length (if not null)
============================================================================= */
void PlotQueryLineType(INTEGER *ltype, REAL *vsize) {

	if (ltype != NULL) *ltype = PlotWindow->lintyp;	/* Old line type */
	if (vsize != NULL) *vsize = PlotWindow->patsiz;	/* Repeat in repeat size */
	return;
}


/* =============================================================================
--     PlotSetLineType - Routine to draw subsequent lines as dashed segments
--
--     Usage: INTEGER = PlotSetLineType(LTYP,VSIZE)
--
--     Inputs: LTYP  - New type line desired
--                0 => No change
--             VSIZE - Basic size of repeat spacing (default 0.003")
--              <=0 => No changes
--
--     Output: DASHES - Previous line selected
============================================================================= */
INTEGER PlotSetLineType(INTEGER ltyp, REAL vsize) {

	INTEGER hold,i;

	if (PlotWindow->Save.On) {						/* HCOPY commands */
		SV_PutCmd(SV_DASHES, 1,1,0);
		SV_PutInt(ltyp);
		SV_PutReal(vsize);
	}

	hold = PlotWindow->lintyp;							/* Save for return */
	if (ltyp != 0) {										/* Use absolute value */
		PlotWindow->lintyp = min(abs(ltyp), NTYPE+1);
		if (vsize > 0.0f) PlotWindow->patsiz = vsize;
	}

	if ( (i=PlotWindow->lintyp) > 1) {							/* Not solid line		*/
		PlotWindow->segbgn = PL_segind[i-2];					/* Set up parameters */
		PlotWindow->segend = PL_segind[i-1] - 1;				/* Lookup into SEGLNS */
		PlotWindow->iseg   = PlotWindow->segbgn;				/* Start with first segment */
		PlotWindow->seglen = (REAL) PL_seglns[PlotWindow->iseg];	/* Length of first segment	*/
		PlotWindow->dshpen = 2;										/* Current mode */
	}
	PlotFixInternal();
	return(hold);
}

/* =============================================================================
--     PlotEraseRegion - Region erase
--
--     Usage: PlotEraseRegion(X,Y,X1,Y1)
--
--     Inputs: X,Y  -  Lower left
--             X1,Y1 - Upper right
============================================================================= */
void PlotEraseRegion(REAL x1, REAL y1, REAL x2, REAL y2) {

	DspRegfnc dsp;											/* For dispatch */
	REAL xp1,yp1,xp2,yp2;
	
	sv_rl4(SV_REGERASE, x1,y1, x2,y2);				/* Handle HCOPY */

/* ... Here we go, move request point to work variables, and run */
	PlotConvert2DScales(USER_TO_PIXEL, x1,y1, &xp1,&yp1);
	PlotConvert2DScales(USER_TO_PIXEL, x2,y2, &xp2,&yp2);
	dsp.x1 = (INTEGER) (xp1+0.5);
	dsp.y1 = (INTEGER) (yp1+0.5);
	dsp.x2 = (INTEGER) (xp2+0.5);
	dsp.y2 = (INTEGER) (yp2+0.5);
	(*DEVICE->dsptch)(REGFNC, DEVICE->DriverBlock, (DSP *) &dsp);		/* Call region function */
	return;
}

/* =============================================================================
--  void PlotFillRect(REAL x1, REAL y1, REAL x2, REAL y2, int color);
--
--  Draw a filled rectangle (assuming driver can handle)
--
--     Usage:     PlotFillRect(x1, y1, x2, y2, color);
--
--     Inputs:    x1,y1  x2,y2  - Corners of the rectangle
--                color         - color to fill with
--
--     Output:    Sends information message to driver
============================================================================= */
void PlotFillRect(REAL x1, REAL y1, REAL x2, REAL y2, int color) {

	DspFillRect dsp;

	if (PlotWindow->Save.On) {
		SV_PutCmd(SV_FILLRECTANGLE, 1,4,0);
		SV_PutReal(x1);
		SV_PutReal(y1);
		SV_PutReal(x2);
		SV_PutReal(y2);
		SV_PutInt(color);
	}

	PlotConvert2DScales(USER_TO_PIXEL, x1,y1, &x1, &y1);
	PlotConvert2DScales(USER_TO_PIXEL, x2,y2, &x2, &y2);

	dsp.x1 = (int) (min(x1, x2)+1.5);			/* Make them one less */
	dsp.x2 = (int) (max(x1, x2)+1.5);
	dsp.y1 = (int) (min(y1, y2)-0.5);			/* Make these one less */
	dsp.y2 = (int) (max(y1, y2)-0.5);

	if (color < 0) color = CLR_NOMARK;
	PlotSetBrush(color, &dsp.brush);

	(*DEVICE->dsptch)(FILLEDRECT, DEVICE->DriverBlock, (DSP *) &dsp);
	return;
}



/* ---------------------
   Internal routines
--------------------- */
void sv_in2(int code, int i1, int i2) {

	if (PlotWindow->Save.On) {
		SV_PutCmd(code, 2,0,0);					/* Need space for 2 integers */
		SV_PutInt(i1);
		SV_PutInt(i2);
	}
	return;
}
	
void sv_in1(int code, int i1) {

	if (PlotWindow->Save.On) {
		SV_PutCmd(code, 1,0,0);						/* Need space for 2 integers */
		SV_PutInt(i1);
		return;
	}
	return;
}

void sv_in0(int code) {
	if (PlotWindow->Save.On) SV_PutCmd(code, 0,0,0);
	return;
}

void sv_rl1(int code, REAL x1) {
	if (PlotWindow->Save.On) {
		SV_PutCmd(code, 0,1,0);
		SV_PutReal(x1);
	}
	return;
}

void sv_rl2(int code, REAL x1, REAL x2) {
	if (PlotWindow->Save.On) {
		SV_PutCmd(code, 0,2,0);
		SV_PutReal(x1);
		SV_PutReal(x2);
	}
	return;
}

void sv_rl4(int code, REAL x1, REAL x2, REAL x3, REAL x4) {
	if (PlotWindow->Save.On) {
		SV_PutCmd(code, 0,4,0);
		SV_PutReal(x1);
		SV_PutReal(x2);
		SV_PutReal(x3);
		SV_PutReal(x4);
	}
	return;
}
