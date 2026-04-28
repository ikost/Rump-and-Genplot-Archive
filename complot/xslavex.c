/* GenplotX Widget 
   Mike Uttormark - 11/7/91

   adapted from Label Widget from aswente-swick widget set
   which is available under anonymous ftp as
   pub/X11/contrib/aswente-swick.examples.tar.Z on gatekeeper.dec.com or 
   contrib/aswente-swick.examples.tar.Z on expo.lcs.mit.edu.

   The following header is required by the original source. */

/***********************************************************
Copyright 1990 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute these examples for any
purpose and without fee is hereby granted, provided that the above
copyright notice appear in all copies and that both that copyright
notice and this permission notice appear in supporting documentation,
and that the name of Digital not be used in advertising or publicity
pertaining to distribution of the software without specific, written
prior permission.

DIGITAL AND THE AUTHORS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS, IN NO EVENT SHALL DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT
OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
OR PERFORMANCE OF THIS SOFTWARE.

******************************************************************/
#define _POSIX_SOURCE
#include "preload.h"

#include <sys/types.h>			/* Put first so caddr_t has better chance */
#include <stdio.h>
#include <X11/Xos.h>				/* Needed for string manipulation */
#include <X11/IntrinsicP.h>	/* Intrinsics header file */
#include <X11/StringDefs.h>	/* Resource string definitions */
#include <X11/Xatom.h>			/* For selection atoms */
#include "xslavexp.h"			/* Label private header file */

/* BLOODY COMPILER ERRORS - AIX can't get NULL as 0 or (void *) 0 straight! */
#ifdef AIX_C
	#undef  XtIsRealized
	#define XtIsRealized(object) (XtWindowOfObject(object) != (Window) 0)
#endif

/* #define NOISY */

#ifdef NOISY
#define REPORT(s) fprintf(stderr,"%s.\n",s);
#else
#define REPORT(s)
#endif

#define ALLOC_SIZE  128
#define GCMASK	( GCForeground | GCBackground | GCLineWidth | GCLineStyle | \
		  GCCapStyle | GCJoinStyle )

#define GCMASK_CUR ( GCMASK | GCFunction )

/* **************************************************************** */
/* **************************************************************** */
/* The following is a list of the widget resources that can be      */
/* modified by the user via .Xdefaults or command line options.     */
/*                                                                  */
/* Name             Type        Default      Description            */
/* ---------------------------------------------------------------- */
/* background       Pixel       WM Default   Color of background    */
/* foreground       Pixel       WM Default   Color of pen1          */
/* lineWidth        Integer     1            pixel width of lines   */
/* maintainAspectRatio  Boolean true         keep original aspect   */
/* cursor           Cursor      crosshair    cursor to use          */
/* pen1             Pixel                    same as foreground     */
/* pen2             Pixel       red                                 */
/* pen3             Pixel       blue                                */
/* pen4             Pixel       green                               */
/* pen5             Pixel       orange                              */
/* pen6             Pixel       yellow                              */
/* pen7             Pixel       brown                               */
/* pen8             Pixel       salmon                              */
/* pen9             Pixel       grey                                */
/* pen10            Pixel       pink                                */
/* pen11            Pixel       light blue                          */
/* pen12            Pixel       aquamarine1                         */
/* pen13            Pixel       orange red                          */
/* pen14            Pixel       goldenrod1                          */
/* pen15            Pixel       beige                               */
/* pen16            Pixel       light salmon                        */
/*                                                                  */
/* The following are defined and used, but should be left alone.    */
/*                                                                  */
/* lineStyle        Integer                                         */
/* capStyle         Integer                                         */
/* joinStyle        Integer                                         */
/* **************************************************************** */
/* **************************************************************** */

#define GOffset(field) XtOffsetOf(GenplotXRec, genplotx.field)
#define COffset(field) XtOffsetOf(GenplotXRec, core.field)

static XtResource resources[] = {
	{XtNbackground, XtCBackground, XtRPixel, sizeof(Pixel),
		COffset(background_pixel), XtRString,
		(XtPointer) "black"},
	{XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
		GOffset(pens[1]), XtRString,
		(XtPointer) "white"},
	{XtNlineWidth, XtCLineWidth, XtRInt, sizeof(int),
		GOffset(line_width), XtRImmediate, (XtPointer) 0},
	{XtNlineStyle, XtCLineStyle, XtRInt, sizeof(int),
		GOffset(line_style), XtRImmediate, (XtPointer) LineSolid},
	{XtNcapStyle, XtCCapStyle, XtRInt, sizeof(int),
		GOffset(cap_style), XtRImmediate, (XtPointer) CapButt},
	{XtNjoinStyle, XtCJoinStyle, XtRInt, sizeof(int),
		GOffset(join_style), XtRImmediate, (XtPointer) JoinMiter},
	{XtNmaintainAspectRatio, XtCMaintainAspectRatio, XtRBoolean,
		sizeof(Boolean), GOffset(keep_aspect), XtRImmediate,
		(XtPointer) TRUE},
	{XtNcursor, XtCCursor, XtRCursor, sizeof(Cursor),
		GOffset(cursor), XtRString, (XtPointer) "crosshair"},
	{XtNcursorColor, XtCCursorColor, XtRPixel, sizeof(Pixel),
		GOffset(cursor_color), XtRString, (XtPointer) "white"},
	{XtNcursorLineWidth, XtCCursorLineWidth, XtRInt, sizeof(int),
		GOffset(cursor_line_width), XtRImmediate, (XtPointer) 0},
	{XtNpen2, XtCPen2, XtRPixel, sizeof(Pixel),
		GOffset(pens[2]), XtRString, (XtPointer) "firebrick1"},
	{XtNpen3, XtCPen3, XtRPixel, sizeof(Pixel),
		GOffset(pens[3]), XtRString, (XtPointer) "green"},
	{XtNpen4, XtCPen4, XtRPixel, sizeof(Pixel),
		GOffset(pens[4]), XtRString, (XtPointer) "blue"},
	{XtNpen5, XtCPen5, XtRPixel, sizeof(Pixel),
		GOffset(pens[5]), XtRString, (XtPointer) "pink1"},
	{XtNpen6, XtCPen6, XtRPixel, sizeof(Pixel),
		GOffset(pens[6]), XtRString, (XtPointer) "cyan"},
	{XtNpen7, XtCPen7, XtRPixel, sizeof(Pixel),
		GOffset(pens[7]), XtRString, (XtPointer) "yellow"},
	{XtNpen8, XtCPen8, XtRPixel, sizeof(Pixel),
		GOffset(pens[8]), XtRString, (XtPointer) "red4"},
	{XtNpen9, XtCPen9, XtRPixel, sizeof(Pixel),
		GOffset(pens[9]), XtRString, (XtPointer) "ForestGreen"},
	{XtNpen10, XtCPen10, XtRPixel, sizeof(Pixel),
		GOffset(pens[10]), XtRString, (XtPointer) "DodgerBlue4"},
	{XtNpen11, XtCPen11, XtRPixel, sizeof(Pixel),
		GOffset(pens[11]), XtRString, (XtPointer) "DeepPink1"},
	{XtNpen12, XtCPen12, XtRPixel, sizeof(Pixel),
		GOffset(pens[12]), XtRString, (XtPointer) "cyan3"},
	{XtNpen13, XtCPen13, XtRPixel, sizeof(Pixel),
		GOffset(pens[13]), XtRString, (XtPointer) "sandy brown"},
	{XtNpen14, XtCPen14, XtRPixel, sizeof(Pixel),
		GOffset(pens[14]), XtRString, (XtPointer) "grey50"},
	{XtNpen15, XtCPen15, XtRPixel, sizeof(Pixel),
		GOffset(pens[15]), XtRString, (XtPointer) "grey85"},
	{XtNpen16, XtCPen16, XtRPixel, sizeof(Pixel),
		GOffset(pens[16]), XtRString, (XtPointer) "tomato3"}
	};

#undef GOffset
#undef COffset

/* **************************************************************** */
/* Forward declarations of all functions                            */
/* **************************************************************** */

static GC GetNormalGC(GenplotXWidget gxw);
static GC GetCursorGC(GenplotXWidget gxw);

static void ClassInitialize(void);
static void ClassPartInitialize(WidgetClass widget_class);
static void Initialize(Widget request, Widget new, ArgList args, 
	Cardinal *num_args);

static void Redisplay(Widget w, XEvent *event, Region region);
static void Destroy(Widget w);
static void Resize(Widget w);

static void Enqueue_Points(GenplotXWidget gxw, XPoint *points, int n);
static void Enqueue_Lines(GenplotXWidget gxw, XSegment *segs, int n);
static void Flush_Queues(GenplotXWidget gxw, Boolean points, Boolean lines);

static void scale_points(XPoint *old, int cnt, XPoint *new, 
	GenplotXWidget gxw);
static void scale_segs(XSegment *old, int cnt, XSegment *new, 
	GenplotXWidget gxw);

static Boolean SetValues(Widget old, Widget request, Widget new,
	ArgList args, Cardinal *num_args);
static XtGeometryResult QueryGeometry(Widget w, XtWidgetGeometry *proposed,
	XtWidgetGeometry *desired);

static CMDPTR GetCmd(GenplotXWidget gxw, CmdCode code);
static void FreeCmds(GenplotXWidget gxw);

static void DrawPoints(Widget w, XPoint *point, int n);
static void DrawLines(Widget w, XSegment *line, int n);
static void Erase(Widget w);
static void ChangePen(Widget w, int pen);
static void RescalePoints(Widget w, XPoint *point, int n);
static void ChangeLineWidth(Widget w, int lw);
static void FlushPlot(Widget w);
static void MyGetGC(Widget w, GC *gc, int which);
static void MyReleaseGC(Widget w, GC *gc);

/* **************************************************************** */
/* Class record declaration                                         */
/* **************************************************************** */

GenplotXClassRec genplotXClassRec = {
    /* Core class part */
  {
    /* superclass	     */	(WidgetClass) &widgetClassRec,
    /* class_name	     */	"GenplotX",
    /* widget_size	     */	sizeof(GenplotXRec),
    /* class_initialize      */ ClassInitialize,
    /* class_part_initialize */	ClassPartInitialize,
    /* class_inited          */	FALSE,
    /* initialize	     */	Initialize,
    /* initialize_hook       */	NULL,		
    /* realize		     */	XtInheritRealize,
    /* actions		     */	NULL,
    /* num_actions	     */	0,
    /* resources	     */	resources,
    /* num_resources	     */	XtNumber(resources),
    /* xrm_class	     */	NULLQUARK,
    /* compress_motion	     */	TRUE,
    /* compress_exposure     */	XtExposeCompressMultiple,
    /* compress_enterleave   */	TRUE,
    /* visible_interest	     */	FALSE,
    /* destroy		     */	Destroy,
    /* resize		     */	Resize,
    /* expose		     */	Redisplay,
    /* set_values	     */	SetValues,
    /* set_values_hook       */	NULL,			
    /* set_values_almost     */	XtInheritSetValuesAlmost,  
    /* get_values_hook       */	NULL,			
    /* accept_focus	     */	NULL,
    /* version		     */	XtVersion,
    /* callback offsets      */ NULL,
    /* tm_table              */ NULL,
    /* query_geometry	     */	QueryGeometry,
    /* display_accelerator   */ NULL,
    /* extension    	     */ NULL
  },
    /* GenplotX class part	*/
  {
    /* draw_point	     */ DrawPoints,
    /* draw_line             */ DrawLines,
    /* erase                 */ Erase,
    /* change_pen            */ ChangePen,
    /* rescale_point         */ RescalePoints,
    /* change_linewidth      */ ChangeLineWidth,
    /* flush                 */ FlushPlot,
    /* get_gc                */ MyGetGC,
    /*                       */ MyReleaseGC,
    /* extension    	     */ (XtPointer) NULL
  }
};

/* **************************************************************** */
/* Class record pointer                                             */
/* **************************************************************** */

WidgetClass genplotXWidgetClass = (WidgetClass) &genplotXClassRec;

/* **************************************************************** */
/* Class initialization procedure                                   */
/*                                                                  */
/* This procedure gets called (once) before any GenplotX widgets    */
/* created.                                                         */
/* **************************************************************** */

static void ClassInitialize()
{

    /* Register a converter for string to justification */

	REPORT("ClassInitialize starting and ending");

	return;
}

/* **************************************************************** */
/* Class part initialization procedure                              */
/*                                                                  */
/* This function gets called if any sub-classes to this widget are  */
/* written and need GenplotXPart stuff looked at.                   */
/* **************************************************************** */

static void ClassPartInitialize(WidgetClass widget_class)
{
/*	register GenplotXWidgetClass 	wc;
	GenplotXWidgetClass 		super;			*/

	REPORT("ClassPartInitialize starting and ending");

	return;
}

/* **************************************************************** */
/* Get graphics context                                             */
/*                                                                  */
/* This routine sets the parts of the graphics context i use and    */
/* returns a pointer to the result.                                 */
/* **************************************************************** */

static GC GetNormalGC(GenplotXWidget gxw)
{
	Widget		w;
	XGCValues	values;
	GC		gc;

	REPORT("GetNormalGC starting");

	w = (Widget) gxw;

	values.background = gxw->core.background_pixel;
	values.foreground = gxw->genplotx.pens[0];
	values.line_width = gxw->genplotx.line_width;
	values.line_style = gxw->genplotx.line_style;
	values.cap_style  = gxw->genplotx.cap_style;
	values.join_style = gxw->genplotx.join_style;

	gc = XCreateGC(XtDisplay(w), XtWindow(w), GCMASK, &values);

	REPORT("GetNormalGC ending");

	return(gc);
}

static GC GetCursorGC(GenplotXWidget gxw)
{
	Widget		w;
	XGCValues	values;
	GC		gc;

	REPORT("GetCursorGC starting");

	w = (Widget) gxw;

	values.function   = GXxor;
	values.background = gxw->core.background_pixel;
	values.foreground = gxw->genplotx.cursor_color ^ values.background;
	values.line_width = gxw->genplotx.cursor_line_width;
	values.line_style = gxw->genplotx.line_style;
	values.cap_style  = gxw->genplotx.cap_style;
	values.join_style = gxw->genplotx.join_style;

	gc = XCreateGC(XtDisplay(w), XtWindow(w), GCMASK_CUR, &values);

	REPORT("GetCursorGC ending");

	return(gc);
}

/* **************************************************************** */
/* Initialization procedure                                         */
/*                                                                  */
/* This procedure gets called to initialize every new GenplotX      */
/* widget.  Fills in the local data fields.                         */
/* **************************************************************** */

static void Initialize(Widget request, Widget new, ArgList args,
	Cardinal *num_args)
{
	GenplotXWidget	gxw;
	CorePart	*gxc;
	GenplotXPart	*gxg;

	REPORT("Initialize starting");

	gxw = (GenplotXWidget) new;
	gxc = &(gxw->core);
	gxg = &(gxw->genplotx);


	gxg->cmd_head = CMD_NULL;	/* no commands yet */
	gxg->cur_cmd  = CMD_NULL;

	gxg->can_draw = FALSE;		/* no window yet */

	gxg->pnt_cnt = 0;		/* no stuff in buffers */
	gxg->seg_cnt = 0;

	REPORT("Initialize ending");
	return;

}

/* **************************************************************** */
/* Set Values checker                                               */
/*                                                                  */
/* This function gets called after the application has made a call  */
/* to XtSetValues to change some of the resources.  This routine    */
/* decides if the new values are reasonable and returns whether the */
/* widget needs to be redisplayed as a result of any changes.       */
/* **************************************************************** */

static Boolean SetValues(Widget old, Widget request, Widget new,
	ArgList args, Cardinal *num_args)
{

#ifdef OLD_CODE

	GenplotXWidget	oldgxw;
	GenplotXWidget	newgxw;
	Boolean		redraw;

	REPORT("SetValues starting");

	oldgxw = (GenplotXWidget) old;
	newgxw = (GenplotXWidget) new;

	redraw = FALSE;

#define NE(field) (oldgxw->field != newgxw->field)

	/* If graphics context resources have changed, update GC */

	if (NE(core.background_pixel) || 
	    NE(genplotx.pens[0]) ||
	    NE(genplotx.pens[1]) ||
	    NE(genplotx.line_width) ||
	    NE(genplotx.line_style) ||
	    NE(genplotx.cap_style)  ||
	    NE(genplotx.join_style) ) {
		XtReleaseGC((Widget) newgxw, oldgxw->genplotx.default_gc);
		newgxw->genplotx.default_gc = GetNormalGC(newgxw);
		redraw = TRUE;}

#undef NE

#else

	REPORT("SetValues starting");

#endif

	REPORT("SetValues ending");
	return(TRUE);
}

/* **************************************************************** */
/* Destroy function                                                 */
/* This function gets called just before we are destroyed.          */
/* We should return all space we allocated.                         */
/* **************************************************************** */

static void Destroy(Widget w)
{
	register GenplotXWidget gxw;

	REPORT("Destroy starting");

	gxw = (GenplotXWidget) w;

	FreeCmds(gxw);

	XFreeGC(XtDisplay(w), gxw->genplotx.default_gc);
	XFreeGC(XtDisplay(w), gxw->genplotx.current_gc);

	REPORT("Destroy ending");
	return;
}

/* **************************************************************** */
/* The next two functions take points (segments) in the original    */
/* coordinate system and scale them to the current coordinates.     */
/* **************************************************************** */

static void scale_points(XPoint *old, int cnt, XPoint *new, 
	GenplotXWidget gxw)
{
	register float		scalex, scaley;
	register Dimension	height;

	scalex = gxw->genplotx.scalex;
	scaley = gxw->genplotx.scaley;
	height = gxw->genplotx.cur_height;

	for(;cnt > 0;cnt--,old++,new++){
		new->x = old->x*scalex;
		new->y = height - old->y*scaley;}
	return;
}

static void scale_segs(XSegment *old, int cnt, XSegment *new, 
	GenplotXWidget gxw)
{
	register float		scalex, scaley;
	register Dimension	height;

	scalex = gxw->genplotx.scalex;
	scaley = gxw->genplotx.scaley;
	height = gxw->genplotx.cur_height;

	for(;cnt > 0;cnt--,old++,new++){
		new->x1 = old->x1*scalex;
		new->x2 = old->x2*scalex;
		new->y1 = height - old->y1*scaley;
		new->y2 = height - old->y2*scaley;
	}
	return;
}

/* **************************************************************** */
/* the following routines manage the point and segment buffers.     */
/* **************************************************************** */

static void Enqueue_Points(GenplotXWidget gxw, XPoint *points, int n)
{
	Widget	w;

	/* see if any room left */

	if (gxw->genplotx.pnt_cnt+n >= PBUFFERSIZE) Flush_Queues(gxw,TRUE,FALSE);

	/* see if too big to fit into buffer */

	if (n > PBUFFERSIZE){
		w = (Widget) gxw;
		XDrawPoints(XtDisplay(w), XtWindow(w),
			gxw->genplotx.current_gc, points, n, 
			CoordModeOrigin);
		}
	else {
		for (;n > 0;n--,points++)
		  gxw->genplotx.pnt_buf[gxw->genplotx.pnt_cnt++] = *points;
	}
	return;
}


static void Enqueue_Lines(GenplotXWidget gxw, XSegment *segs, int n)
{
	Widget	w;

	/* see if any room left */

	if (gxw->genplotx.seg_cnt+n >= SBUFFERSIZE) Flush_Queues(gxw, FALSE, TRUE);

	/* see if too big to fit in buffer */

	if (n > SBUFFERSIZE){
		w = (Widget) gxw;
		XDrawSegments(XtDisplay(w), XtWindow(w),
			gxw->genplotx.current_gc, segs, n);
		}
	else {
		for(;n > 0;n--,segs++)
		  gxw->genplotx.seg_buf[gxw->genplotx.seg_cnt++] = *segs;
	}

	return;
}

static void Flush_Queues(GenplotXWidget gxw, Boolean points, Boolean lines)
{
	Widget	w;

	w = (Widget) gxw;

/* Problem fix 8/3/93 Larry Doolittle:
-- added the && (gxw->genplotx.<counter> != 0) conditions, which
-- seems to avoid crashes in the Draw functions.
-- With this change, my version (Linux X11R5 with XS3-0.4 server)
-- correctly does all necessary buffering so that plots can be
-- started (and finished) immediately after creating
-- the device, and even before the window is placed!
-- Finally, I can have GTERM=x and auto-start the device
-- on the first "plot" command!
-- */
	if (points && (gxw->genplotx.pnt_cnt != 0)) {
		REPORT("XDrawPoints start");
		XDrawPoints(XtDisplay(w), XtWindow(w),
			gxw->genplotx.current_gc, gxw->genplotx.pnt_buf,
			gxw->genplotx.pnt_cnt, CoordModeOrigin);
		gxw->genplotx.pnt_cnt = 0;
	}

	if (lines && (gxw->genplotx.seg_cnt != 0)){
		REPORT("XDrawSegments start");
		XDrawSegments(XtDisplay(w), XtWindow(w),
			gxw->genplotx.current_gc, gxw->genplotx.seg_buf,
			gxw->genplotx.seg_cnt);
		gxw->genplotx.seg_cnt = 0;
		REPORT("XDrawSegments end");
	}
	return;
}


/* **************************************************************** */
/* Expose procedure                                                 */
/* This function gets called when we are exposed by the window      */
/* manager.                                                         */
/* **************************************************************** */

static void Redisplay(Widget w, XEvent *event, Region region)
{
	GenplotXWidget	gxw;
	GenplotXPart	*gxg;
	CorePart	*gxc;
	XSegment	*segs;
	XPoint		*pnts;
	CMDPTR		c;
			
	REPORT("Redisplay starting");

	gxw = (GenplotXWidget) w;
	gxg = &(gxw->genplotx);
	gxc = &(gxw->core);

	/* ************************************************ */
	/* Several things are initialized here (instead of  */
	/* in one of the initialization routines, above)    */
	/* since we are not guaranteed to have a valid      */
	/* window until here.                               */
	/* ************************************************ */

	if (!gxg->can_draw){
		gxg->can_draw = TRUE;

		/* install the new cursor for the window */

		XDefineCursor(XtDisplay(w), XtWindow(w), gxg->cursor);

		/* see if pen1 and background are different colors */

		gxg->pens[0] = gxg->pens[1];	/* foreground gets pen 1 */

		if (gxc->background_pixel == gxg->pens[1])
			XtWarning("pen1 and background are same color.");

		/* Get the graphics context */

		gxg->default_gc = GetNormalGC(gxw);
		gxg->current_gc = GetNormalGC(gxw);

		/* If no size specified, use default one */

		if (gxc->width  == 0) gxc->width  = 400;
		if (gxc->height == 0) gxc->height = 300;

		gxg->cur_width  = gxg->orig_width  = gxc->width;
		gxg->cur_height = gxg->orig_height = gxc->height;

		gxg->scalex = 1.0;
		gxg->scaley = 1.0;

		gxg->aspect_ratio = gxg->orig_width/ (float) gxg->orig_height;
	}

	/* Execute command list */

	/* Restore Graphics Context */

	XCopyGC(XtDisplay(w), gxg->default_gc, GCMASK, gxg->current_gc);

	segs = (XSegment *) NULL;
	pnts = (XPoint *) NULL;

	for(c = gxg->cmd_head; c != CMD_NULL;c = c->next)
		switch(c->code){
		case POINT:
			pnts = (XPoint *) XtRealloc((char *)pnts,
				c->data.pnt.cnt*sizeof(XPoint));
			scale_points(c->data.pnt.pnts,c->data.pnt.cnt,pnts,gxw);
			XDrawPoints(XtDisplay(w), XtWindow(w),gxg->current_gc,
				pnts,c->data.pnt.cnt,CoordModeOrigin);
			break;
		case LINE:
			segs = (XSegment *)  XtRealloc((char *) segs,
				c->data.seg.cnt*sizeof(XSegment));
			scale_segs(c->data.seg.segs,c->data.seg.cnt,segs,gxw);
			XDrawSegments(XtDisplay(w), XtWindow(w),
				gxg->current_gc,segs,c->data.seg.cnt);
			break;
		case PEN:
			XChangeGC(XtDisplay(w), gxg->current_gc, GCMASK,
				c->data.pen.xgcv);
			break;
		default:
			XtWarning("Strange command found (Redisplay).\n");
			break;
		}

	XtFree((char *) pnts);
	XtFree((char *) segs);

	REPORT("Redisplay ending");
	return;
}

/* **************************************************************** */
/* Resize function                                                  */
/*                                                                  */
/* This function gets called when we get resized by the window      */
/* manager.                                                         */
/* **************************************************************** */

static void Resize(Widget w)
{
	GenplotXWidget	gxw;
	GenplotXPart	*gxg;
	CorePart	*gxc;
	Dimension	usable_width, usable_height;

    /* If widget is realized, clear and redisplay */

	REPORT("Resize starting");

	if (!XtIsRealized(w)) return;

	gxw = (GenplotXWidget) w;
	gxg = &(gxw->genplotx);
	gxc = &(gxw->core);

	/* try not to go invisible */

	if (gxc->height <= 0) gxc->height = 1;
	if (gxc->width  <= 0) gxc->width  = 1;

	/* see if we really have changed size */

	if (gxg->cur_height == gxc->height &&
	    gxg->cur_width  == gxc->width) {
		REPORT("Resize ending");
		return;
	}

	/* see if we need to maintain aspect ratio of drawing */

	if (!gxg->keep_aspect){
		gxg->scalex = gxc->width/  (float) gxg->orig_width;
		gxg->scaley = gxc->height/ (float) gxg->orig_height;
		}
	else {
		if (gxc->width > gxc->height*gxg->aspect_ratio){
			usable_height = gxc->height;
			usable_width  = usable_height*gxg->aspect_ratio;}
		else{
			usable_width  = gxc->width;
			usable_height = usable_width/gxg->aspect_ratio;}

		gxg->scalex = usable_width/  (float) gxg->orig_width;
		gxg->scaley = usable_height/ (float) gxg->orig_height;
		}

	gxg->cur_width  = gxc->width;
	gxg->cur_height = gxc->height;

	/* we really have changed size, so erase and redraw */

	XClearWindow(XtDisplay(w), XtWindow(w));
	(*(XtClass(w)->core_class.expose))(w,(XEvent *) NULL, (Region) NULL);

	REPORT("Resize ending");
	return;
}   

/* **************************************************************** */
/* QueryGeometry procedure                                          */
/*                                                                  */
/* This function gets called when someone else wants to know how    */
/* big we would like to be.                                         */
/*                                                                  */
/* Could probably be a little smarter about 0 size widgets but will */
/* allow those bozos to get what they ask for.                      */
/* **************************************************************** */

static XtGeometryResult QueryGeometry(Widget w, XtWidgetGeometry *proposed,
	XtWidgetGeometry *desired)
{
	REPORT("QueryGeometry starting and ending");

	return XtGeometryYes; 	/* anything goes */
}

/* **************************************************************** */
/* **************************************************************** */
/* **** The following functions are user access points to the  **** */
/* **** Widget functions.  The routines check to see that the  **** */
/* **** pointer actually points to a GenplotX widget and then  **** */
/* **** calls the routine stored in the class structure.  By   **** */
/* **** using the class routine, users can install their own   **** */
/* **** functions just once and inherit the other stuff.       **** */
/* **************************************************************** */
/* **************************************************************** */

void GenplotXDrawPoints(Widget w, XPoint *point, int n)
{
	register GenplotXWidgetClass gxwc;

	XtCheckSubclass(w, genplotXWidgetClass, NULL);

	gxwc = (GenplotXWidgetClass) XtClass(w);

	(*gxwc->genplotx_class.draw_points)(w, point, n);
	return;
}

void GenplotXDrawLines(Widget w, XSegment *line, int n)
{
	register GenplotXWidgetClass gxwc;

	XtCheckSubclass(w, genplotXWidgetClass, NULL);

	gxwc = (GenplotXWidgetClass) XtClass(w);

	(*gxwc->genplotx_class.draw_lines)(w, line, n);
	return;
}

void GenplotXErase(Widget w)
{
	register GenplotXWidgetClass gxwc;

	XtCheckSubclass(w, genplotXWidgetClass, NULL);

	gxwc = (GenplotXWidgetClass) XtClass(w);

	(*gxwc->genplotx_class.erase)(w);
	return;
}

void GenplotXChangePen(Widget w, int pen)
{
	register GenplotXWidgetClass gxwc;

	XtCheckSubclass(w, genplotXWidgetClass, NULL);

	gxwc = (GenplotXWidgetClass) XtClass(w);

	(*gxwc->genplotx_class.change_pen)(w, pen);
	return;
}

void GenplotXLineWidth(Widget w, int lw)
{
	register GenplotXWidgetClass gxwc;

	XtCheckSubclass(w, genplotXWidgetClass, NULL);

	gxwc = (GenplotXWidgetClass) XtClass(w);

	(*gxwc->genplotx_class.line_width)(w, lw);
	return;
}

void GenplotXFlush(Widget w)
{
	register GenplotXWidgetClass gxwc;

	XtCheckSubclass(w, genplotXWidgetClass, NULL);

	gxwc = (GenplotXWidgetClass) XtClass(w);

	(*gxwc->genplotx_class.flush)(w);
	return;
}

void GenplotXRescalePoints(Widget w, XPoint *point, int n)
{
	register GenplotXWidgetClass gxwc;

	XtCheckSubclass(w, genplotXWidgetClass, NULL);

	gxwc = (GenplotXWidgetClass) XtClass(w);

	(*gxwc->genplotx_class.rescale_points)(w, point, n);
	return;
}

void GenplotXGetGC(Widget w, GC *gc, int which)
{
	register GenplotXWidgetClass gxwc;

	XtCheckSubclass(w, genplotXWidgetClass, NULL);

	gxwc = (GenplotXWidgetClass) XtClass(w);

	(*gxwc->genplotx_class.get_gc)(w, gc, which);
	return;
}

void GenplotXReleaseGC(Widget w, GC *gc)
{
	register GenplotXWidgetClass gxwc;

	XtCheckSubclass(w, genplotXWidgetClass, NULL);

	gxwc = (GenplotXWidgetClass) XtClass(w);

	(*gxwc->genplotx_class.release_gc)(w, gc);
	return;
}

/* Used by cross-hair cursor code in xslave.c
   This routine has to be compiled here because of its use of
   X primitives w->core.width and w->core.height
   "Written" by Larry Doolittle, doolittle@cebaf.gov  8/23/94    */
void GenplotHighlight_xhair(Widget w, GC *gc, int px, int py)
{
	XDrawLine(XtDisplay(w), XtWindow(w), *gc,
			0, py, w->core.width, py);
	XDrawLine(XtDisplay(w), XtWindow(w), *gc,
			px, 0, px, w->core.height);
	return;
}

/* **************************************************************** */
/* **************************************************************** */
/* **** implementation functions                               **** */
/* ****                                                        **** */
/* **** these functions are the default implementations of the **** */
/* **** user access procedures.                                **** */
/* **************************************************************** */
/* **************************************************************** */

static void DrawPoints(Widget w, XPoint *points, int n)
{
	GenplotXWidget	gxw;
	CMDPTR		c;
	XPoint		*copy;
	int		i;

	REPORT("DrawPoints starting");

	gxw = (GenplotXWidget) w;

	/* see if on draw point command */

	c = GetCmd(gxw, POINT);

	/* see if buffer full */

	if (c->data.pnt.cnt+n >= c->data.pnt.max){
		c->data.pnt.max += (ALLOC_SIZE > n) ? ALLOC_SIZE : n;
		c->data.pnt.pnts = (XPoint *) XtRealloc((char *) c->data.pnt.pnts,
			sizeof(XPoint)*c->data.pnt.max);}

	/* put the points into the buffer */

	for(i = 0;i < n; i++)
		c->data.pnt.pnts[c->data.pnt.cnt++] = points[i];

	/* do the plotting */

	if (gxw->genplotx.can_draw){
		copy = (XPoint *) XtMalloc(n*sizeof(*copy));
		scale_points(points, n, copy, gxw);
		Enqueue_Points(gxw, copy, n);
		XtFree((char *) copy);
	}

	REPORT("DrawPoints ending");
	return;
}

static void DrawLines(Widget w, XSegment *lines, int n)
{
	GenplotXWidget	gxw;
	CMDPTR		c;
	XSegment	*copy;
	int		i;

	REPORT("DrawLines starting");

	gxw = (GenplotXWidget) w;

	c = GetCmd(gxw, LINE);

	/* see if buffer full */

	if (c->data.seg.cnt+n >= c->data.seg.max){
		c->data.seg.max += (ALLOC_SIZE > n) ? ALLOC_SIZE : n;
		c->data.seg.segs = (XSegment *) XtRealloc((char *) c->data.seg.segs,
			sizeof(XSegment)*c->data.seg.max);}

	/* put the line segments into the buffer */

	for(i = 0; i < n; i++)
		c->data.seg.segs[c->data.seg.cnt++] = lines[i];

	/* do the plotting */

	if (gxw->genplotx.can_draw){
		copy = (XSegment *) XtMalloc(n*sizeof(*copy));
		scale_segs(lines, n, copy, gxw);
		Enqueue_Lines(gxw, copy, n);
		XtFree((char *) copy);
	}

	REPORT("DrawLines ending");
	return;
}

static void Erase(Widget w)
{
	GenplotXWidget	gxw;

	gxw = (GenplotXWidget) w;

	REPORT("Erase starting");

	FreeCmds(gxw);

	gxw->genplotx.pnt_cnt = 0;		/* discard queued stuff */
	gxw->genplotx.seg_cnt = 0;

	if (gxw->genplotx.can_draw){
		XCopyGC(XtDisplay(w), gxw->genplotx.default_gc, GCMASK,
			gxw->genplotx.current_gc);
		XClearWindow(XtDisplay(w), XtWindow(w));
	}
	REPORT("Erase ending");
	return;
}

static void FlushPlot(Widget w)
{
	REPORT("FlushPlot starting");

	Flush_Queues((GenplotXWidget) w, TRUE, TRUE);

	XFlush(XtDisplay(w));

	REPORT("FlushPlot ending");
	return;
}

static void ChangeLineWidth(Widget w, int lw)
{
	GenplotXWidget	gxw;
	CMDPTR		c;
	
	REPORT("ChangeLineWidth starting");

	gxw = (GenplotXWidget) w;

	c = GetCmd(gxw, PEN);

	/* line width 0 is fast implementation of line width 1 */

	if (lw <= 1) lw = 0;

	gxw->genplotx.line_width = lw;

	c->data.pen.xgcv->line_width = lw;

	if (gxw->genplotx.can_draw){
		Flush_Queues(gxw, TRUE, TRUE);
		XChangeGC(XtDisplay(w), gxw->genplotx.current_gc, GCMASK,
			c->data.pen.xgcv);
	}

	REPORT("ChangeLineWidth ending");
	return;
}

static void ChangePen(Widget w, int pen)
{
	GenplotXWidget	gxw;
	CMDPTR		c;
	Pixel		color;
	
	REPORT("ChangePen starting");

	gxw = (GenplotXWidget) w;

	c = GetCmd(gxw, PEN);

	if (pen < 0 || pen > NCOLORS) pen = 1;
	if (pen == 0)
		color = gxw->core.background_pixel;
	else
		color = gxw->genplotx.pens[pen];

	gxw->genplotx.pens[0]        = color;
	c->data.pen.xgcv->foreground = color;

	if (gxw->genplotx.can_draw){
		Flush_Queues(gxw, TRUE, TRUE);
		XChangeGC(XtDisplay(w), gxw->genplotx.current_gc, GCMASK,
			c->data.pen.xgcv);
	}

	REPORT("ChangePen ending");
	return;
}

static void RescalePoints(Widget w, XPoint *point, int n)
{
	GenplotXWidget		gxw;
	register float		scalex, scaley;
	register Dimension	height;
	
	REPORT("RescalePoint starting");

	gxw = (GenplotXWidget) w;
	scalex = gxw->genplotx.scalex;
	scaley = gxw->genplotx.scaley;
	height = gxw->genplotx.cur_height;

	for(;n > 0;n--,point++){
		point->x /= scalex;
		point->y = (height - point->y)/scaley;}

	REPORT("RescalePoint ending");
	return;
}

static void MyGetGC(Widget w, GC *gc, int which)
{
	GenplotXWidget	gxw;

	REPORT("GetGC starting");

	gxw = (GenplotXWidget) w;

	switch(which){
	case DEFAULT_GC:
		*gc = GetNormalGC(gxw);
		break;
	case BOX_CURSOR_GC:
		*gc = GetCursorGC(gxw);
		break;
	case CURRENT_GC:
	default:
		*gc = GetNormalGC(gxw);
		XCopyGC(XtDisplay(w), gxw->genplotx.current_gc, GCMASK, *gc);
		break;
	}

	REPORT("GetGC ending");
	return;
}

static void MyReleaseGC(Widget w, GC *gc)
{
/*	GenplotXWidget	gxw; */

	REPORT("GetGC starting");

	XFreeGC(XtDisplay(w), *gc);
	
	REPORT("GetGC ending");
	return;
}

static void FreeCmds(GenplotXWidget gxw)
{
	CMDPTR		c, n;
	GenplotXPart	*gxg;

	gxg = &(gxw->genplotx);

	for(c = gxg->cmd_head; c != CMD_NULL; c = n){
		n = c->next;
		switch(c->code){
		case POINT:
			XtFree((char *) c->data.pnt.pnts);
			break;
		case LINE:
			XtFree((char *) c->data.seg.segs);
			break;
		case PEN:
			XtFree((char *) c->data.pen.xgcv);
			break;
		default:
			XtWarning("Strange command encountered (cmd_free).\n");
		}
		XtFree((char *) c);
	}

	gxg->cmd_head = CMD_NULL;
	gxg->cur_cmd  = CMD_NULL;
	return;
}

/* **************************************************************** */
/* This routine returns a pointer to the current command structure  */
/* which can accept data of type 'code'.  A new structure will not  */
/* have to be allocated if either of the following is true:         */
/* 1) the last command in the list matches the desired one.         */
/* 2) the command is not PEN, and a command which matches the       */
/*    desired one is found after the last PEN command.              */
/* Allocated structures will be linked into the list and have       */
/* appropriate default values supplied.                             */
/*                                                                  */
/* By searching backwards for similar commands, redrawing is faster */
/* since all lines of similar color and width can be drawn with one */
/* call.  Same with points.                                         */
/*                                                                  */
/* Note: the following can be coded without the 'goto', but is much */
/* harder to understand.                                            */
/* **************************************************************** */

static CMDPTR GetCmd(GenplotXWidget gxw, CmdCode code)
{
	CMDPTR	c;

	c = gxw->genplotx.cur_cmd;

	if (c == CMD_NULL) goto allocate_one;	/* none yet */

	if (c->code == code) return(c);		/* match, return it */

	/* now see if we want a PEN command, since it didn't match */
	/* we will have to allocate.                               */

	if (code == PEN) goto allocate_one;	/* pen must be last */

	/* now we have something besides a PEN command.       */
	/* search back in list for command.  must find before */
	/* finding a PEN command                              */

	for (;c != CMD_NULL;c = c->prev){
		if (c->code == PEN) goto allocate_one;	/* found pen, no match */
		if (c->code == code) return(c);}	/* match */

	/* need to allocate one */

allocate_one:

	c = (CMDPTR) XtMalloc(sizeof(CMD));

	c->code = code;
	c->next = CMD_NULL;

	/* allocate data and give default values */

	switch(code){
	case POINT:
		c->data.pnt.max = ALLOC_SIZE;
		c->data.pnt.cnt = 0;
		c->data.pnt.pnts = (XPoint *) XtMalloc(ALLOC_SIZE*sizeof(XPoint));	
		break;
	case LINE:
		c->data.seg.max = ALLOC_SIZE;
		c->data.seg.cnt = 0;
		c->data.seg.segs = (XSegment *) XtMalloc(ALLOC_SIZE*sizeof(XSegment));
		break;
	case PEN:
		c->data.pen.xgcv = (XGCValues *) XtMalloc(sizeof(XGCValues));
		c->data.pen.xgcv->background = gxw->core.background_pixel;
		c->data.pen.xgcv->foreground = gxw->genplotx.pens[0];
		c->data.pen.xgcv->line_width = gxw->genplotx.line_width;
		c->data.pen.xgcv->line_style = gxw->genplotx.line_style;
		c->data.pen.xgcv->cap_style  = gxw->genplotx.cap_style;
		c->data.pen.xgcv->join_style = gxw->genplotx.join_style;
		break;
	default:
		XtWarning("Strange command code encountered (GetCmd).\n");
	}

	/* determine where to link in */

	if (gxw->genplotx.cur_cmd == CMD_NULL){
		gxw->genplotx.cmd_head = c;
		c->prev = CMD_NULL;}
	else{
		gxw->genplotx.cur_cmd->next = c;
		c->prev = gxw->genplotx.cur_cmd;}

	/* remember that we are last */

	gxw->genplotx.cur_cmd = c;

	return(c);
}
