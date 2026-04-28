/* **************************************************************** */
/* xslave.c - Mike Uttormark 11/13/91                               */
/*                                                                  */
/* Slave process to manage window part of X driver.                 */
/*                                                                  */
/* Needs to be a separate process from Genplot for 2 reasons.       */
/*   1) Window needs to autonomous so that it can handle resize and */
/*      redraw events.                                              */
/*   2) genplot blocks on reads to the console, prohibiting #1.     */
/*                                                                  */
/* Implementation strategy:                                         */
/*   1) Set up X-event handlers for resize, redisplay , etc, with   */
/*      GenplotX widget.                                            */
/*   2) Add the possiblity of reading stdin to list of events so    */
/*      can get drawing commands from master (genplot).             */
/*   3) Enter XAppMainLoop and wait for events.                     */
/*                                                                  */
/* **************************************************************** */

#define _POSIX_SOURCE
#include "preload.h"

/* #define NOISY */

#ifdef NOISY
	#define REPORT(s,n) fprintf(stderr,s,n);fflush(stderr);
#else
	#define REPORT(s,n)
#endif

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "xslavex.h"

#include "mytypes.h"
#include "extends.h"
#include "complot.h"

#define PIXEL_CVT     1.0/6.0

#define NO_CURSOR     0
#define POINT_CURSOR  1
#define BOX_CURSOR    2

/* **************************************************************** */
/* these constants define the pseudo-resolution, height and width.  */
/* these are the values we tell the upper-level stuff we have, then */
/* have to convert to what really exists.                           */
/* **************************************************************** */

#define PS_RESOLUTION 3000		/* dpi */
#define PS_WIDTH      10		/* inches */
#define PS_HEIGHT     7.5		/* inches */

/* **************************************************************** */
/* function prototypes                                              */
/* **************************************************************** */

static XtInputCallbackProc HandleInput(XtPointer client_data,
	int *source, XtInputId *id);

static XtEventHandler MyKeyPress(Widget w, XtPointer client_data,
	XEvent *event, Boolean *can_continue);

static XtEventHandler MyButtonPress(Widget w, XtPointer client_data,
	XEvent *event, Boolean *can_continue);

static XtEventHandler MyButtonRelease(Widget w, XtPointer client_data,
	XEvent *event, Boolean *can_continue);

static XtEventHandler MyPointerMotion(Widget w, XtPointer client_data,
	XEvent *event, Boolean *can_continue);

static XtEventHandler MyEnterWindow(Widget w, XtPointer client_data,
	XEvent *event, Boolean *can_continue);

static XtEventHandler MyLeaveWindow(Widget w, XtPointer client_data,
	XEvent *event, Boolean *can_continue);

static void make_rectangle(void);
static void highlight_rectangle(void);
static void highlight_xhair(void);
static void find_pos_length(int x1, int x2, int *pos, int *length);

static void send_result(LOGICAL result);
static LOGICAL get_data(void *buf, size_t size, char *msg, FILE *infile);
static LOGICAL send_data(void *buf, size_t size, char *msg, FILE *file);

/* **************************************************************** */
/* default values for size of window we create.                     */
/* **************************************************************** */

static Arg genplotXArgs[] = {	/* The arguments to the GenplotX widget */
	{XtNwidth,	800},
	{XtNheight,	600}
};

static Widget		toplevel, plot;
static XtAppContext	app;
static Display		*display;
static Screen		*screen;
static int		screen_number;

static int		numpens = 15;

static int		waiting_for_cursor = FALSE;
static int		cursor_mode = NO_CURSOR;
static XPoint		point1;
static XPoint		point2;
static XPoint     pointr={0,0};
static unsigned int	button;
static unsigned int	keycode;
static int		have_button;
static int     in_bounds=0;    /* 1 means pointer in our window */
static GC		cursor_gc;
static struct {
	int	x;
	int	y;
	int	dx;
	int	dy;}	rectangle;

int main(int argc, char **argv) {
	int		i;
	char		**my_argv;
	int		my_argc;

	/* see if we are connected to driver */
	/* If we are not, print a warning. */
	/* if we are, ignore SIGINT since parent traps it and we */
	/* don't want to be inadvertantly killed. */

	if (isatty(fileno(stdin)))
		fprintf(stderr,
		"Warning: this program is not meant to be run stand-alone.\n"
		"Behavior of the display upon exit may be unpredictable.\n");
	else{
		signal(SIGINT,  SIG_IGN);
		signal(SIGTSTP, SIG_IGN);
	}

	/* create argv list with my name as first arg */

	my_argv = (char **) XtMalloc((argc+1) * sizeof(char *));
	my_argv[0] = "genplot";
	for(i = 1; i < argc; i++) my_argv[i] = argv[i];
	my_argv[argc] = NULL;
	my_argc = argc;

	/* create a top level for this application */

	REPORT("Making top level\n",i);
	toplevel = XtAppInitialize(&app, "Genplot", (XrmOptionDescList) NULL,
			0, &my_argc, my_argv, (String *) NULL,
			(ArgList) NULL, 0);

	/* put the driver widget in */

	REPORT("Creating plot\n",i);
	plot = XtCreateManagedWidget("xdriver", 
			genplotXWidgetClass, toplevel, 
			genplotXArgs, XtNumber(genplotXArgs));

	/* make everything appear */

	XtRealizeWidget(toplevel);

	display = XtDisplay(plot);
	screen  = XtScreen(plot);
	screen_number = XScreenNumberOfScreen(screen);

	/* register stdin for input event handling */

	XtAppAddInput(app, fileno(stdin), 
			(XtPointer) XtInputReadMask,
			(XtInputCallbackProc) HandleInput, (XtPointer) stdin);

	XtAddEventHandler(plot, KeyPressMask ,FALSE,
			(XtEventHandler) MyKeyPress, (XtPointer) 0);

	XtAddEventHandler(plot, ButtonPressMask ,FALSE,
			(XtEventHandler) MyButtonPress, (XtPointer) 0);

	XtAddEventHandler(plot, ButtonReleaseMask ,FALSE,
			(XtEventHandler) MyButtonRelease, (XtPointer) 0);

	XtAddEventHandler(plot, PointerMotionMask ,FALSE,
			(XtEventHandler) MyPointerMotion, (XtPointer) 0);

	XtAddEventHandler(plot, EnterWindowMask ,FALSE,
			(XtEventHandler) MyEnterWindow, (XtPointer) 0);

	XtAddEventHandler(plot, LeaveWindowMask ,FALSE,
			(XtEventHandler) MyLeaveWindow, (XtPointer) 0);

	XtAppMainLoop(app);	/* wait for events */

	return(1);		/* should never get here */
}

/* **************************************************************** */
/* routine to handle input from master process                      */
/* **************************************************************** */

static Dimension	width;
static Dimension	height;

static float		xscale;
static float		yscale;

static XtInputCallbackProc HandleInput(XtPointer client_data,
	int *source, XtInputId *id) {

	char		*abuf;
	INTEGER		key, n;
	DSP		dsp;
	FILE		*infile;
	DspPntfnc	*point_buf;
	DspLinfnc	*line_buf;

	Arg		QueryArgs[2];
	XPoint		*points;
	XSegment	*lines;
	XEvent		event;

	int		i;
	float		xdpi, ydpi;
	float		use_x, use_y;

	infile = (FILE *) client_data;	/* convert to file pointer */

/* Read the next "operation" from the input stream */

	if (fread(&key, sizeof(key), 1, infile) <= 0) {
		if (feof(infile)) {
			exit(0);												/* we're done */
		} else {
			XtAppWarning(app, "Slave read error.");
			return(0);
		}
	}
	REPORT("xslave read %d bytes.\n",4);

	switch (key) {
		case -1:													/* Special key		*/
			fread(&i, sizeof(i), 1, infile);				/* Number of bytes */
			abuf = malloc(i);									/* Make space		*/
			fread(abuf, i, 1, infile);						/* And dummy data	*/
			free(abuf);
			break;

	 	case INIFNC:											/* Initialize */
		if (! get_data(&dsp, sizeof(dsp.ini), "initialization", infile)) {
			send_result(FALSE);
			break;
		}

		numpens = max(1, min(15, dsp.ini.NumberPens));
		dsp.ini.NumberPens = numpens;

		/* see how big the window really is */

		i = 0;
		XtSetArg(QueryArgs[i], XtNwidth,  &width); 	i++;
		XtSetArg(QueryArgs[i], XtNheight, &height);	i++;

		XtGetValues(plot, QueryArgs, i);

		/* get display resolution from Display macros */

		xdpi = (float) DisplayWidth(display, screen_number) /
			(float) DisplayWidthMM(display, screen_number) * 25.4;
		ydpi = (float) DisplayHeight(display, screen_number) / 
				(float) DisplayHeightMM(display, screen_number) * 25.4;

		/* compute our scale factors */

		use_x = width/xdpi;		/* width in inches */
		use_y = height/ydpi;

		/* maintain aspect ratio with pseudo-size */

		if (use_x > use_y*PS_WIDTH/PS_HEIGHT) {
			use_x = use_y*PS_WIDTH/PS_HEIGHT;
		} else {
			use_y = use_x*PS_HEIGHT/PS_WIDTH;
		}

		/* scale factors for pixels */

		xscale = use_x / PS_WIDTH  / PS_RESOLUTION * xdpi;
		yscale = use_y / PS_HEIGHT / PS_RESOLUTION * ydpi;

		/* return pseudo-size to upper levels */

		dsp.ini.xperinch = PS_RESOLUTION;
		dsp.ini.yperinch = PS_RESOLUTION;
		dsp.ini.xmax     = PS_RESOLUTION * PS_WIDTH;
		dsp.ini.ymax     = PS_RESOLUTION * PS_HEIGHT;
		dsp.ini.DriverBlock = NULL;
		dsp.ini.Capabilities = DEV_CAP_GRAPHICS |		/* Supports graphs	*/
									  DEV_CAP_CURSOR;			/* Can do cursors		*/

		send_result(TRUE);
		send_data((char *) &dsp, sizeof(dsp.ini), "intialization", stdout);
		break;

	case LINFNC:	/* Draw lines */

		/* Get # of lines, allocate space, and read the data */

		if (! get_data(&n, sizeof(n),"line count", infile)) break;
		line_buf = (DspLinfnc *) XtMalloc(n*sizeof(*line_buf));
		if (! get_data(line_buf, n*sizeof(*line_buf), "lines", infile))
			break;

		lines = (XSegment *) XtMalloc(n*sizeof(XSegment));

		for (i=0; i<n; i++) {
			lines[i].x1 = line_buf[i].x1*xscale;
			lines[i].y1 = line_buf[i].y1*yscale;
			lines[i].x2 = line_buf[i].x2*xscale;
			lines[i].y2 = line_buf[i].y2*yscale;
		}

		GenplotXDrawLines(plot, lines, n);
		XtFree((char *) line_buf);
		XtFree((char *) lines);
		break;

	case PNTFNC:	/* Draw points */

		/* Get # of points, allocate space, and read the data */

		if (! get_data(&n, sizeof(n), "point count", infile)) break;
		point_buf = (DspPntfnc *) XtMalloc(n*sizeof(*point_buf));
		if (! get_data(point_buf, n*sizeof(*point_buf), "points", infile))
			break;

		points = (XPoint *) XtMalloc(n*sizeof(XPoint));

		for (i=0; i<n; i++) {
			points[i].x = point_buf[i].x*xscale;
			points[i].y = point_buf[i].y*yscale;
		}

		GenplotXDrawPoints(plot, points, n);
		XtFree((char *) point_buf);
		XtFree((char *) points);
		break;

	case COLFNC:	/* change pen color */
		if (get_data(&dsp, sizeof(dsp.col), "color", infile)) 
			GenplotXChangePen(plot, max(1, min(dsp.col.brush.closest_index, numpens)));
		break;

	case LWFNC: /* change line width */
		if (get_data(&dsp, sizeof(dsp.line), "line", infile)) 
		   GenplotXLineWidth(plot, (int) (dsp.lw.linewidth*PIXEL_CVT+0.5));
		break;

	case ERSFNC:	/* erase screen */
		GenplotXErase(plot);
		break;

	case ANMFNC:								/* Basically do-nothing */
		break;

	case FLSFNC:	/* flush graphics buffers */
		GenplotXFlush(plot);
		send_result(TRUE);					/* Acknowledge to force done */
		break;

	case ENDFNC:	/* close device */
		XtUnmapWidget(toplevel);
		XtDestroyApplicationContext(app);
		exit(0);

	case CURFNC:	/* read cursor position */

		/* get rid of pending button press events */

		while(XCheckTypedEvent(XtDisplay(plot), ButtonPress, &event));

		/* tell buttonpress handler to return data */

		waiting_for_cursor = TRUE;
		cursor_mode = POINT_CURSOR;
		have_button = FALSE;
		GenplotXGetGC(plot, &cursor_gc, BOX_CURSOR_GC);

		point1 = pointr;
		if (in_bounds) highlight_xhair();
		break;

	case CURBOX:	/* cursor box */

		/* get rid of pending button press and release events */

		while(XCheckTypedEvent(XtDisplay(plot), ButtonPress, &event));
		while(XCheckTypedEvent(XtDisplay(plot), ButtonRelease, &event));

		/* tell button press and release handlers to return data */

		waiting_for_cursor = TRUE;
		cursor_mode = BOX_CURSOR;
		have_button = FALSE;

		/* get graphics context for line drawing */

		GenplotXGetGC(plot, &cursor_gc, BOX_CURSOR_GC);
		break;

	default:										/* Unimplemented just guess nothing */
		break;
	}
	return(0);
}

/* **************************************************************** */
static XtEventHandler MyKeyPress(Widget w, XtPointer client_data,
	XEvent *event, Boolean *can_continue) {

	DspCurfnc	cursor;
	KeySym	 keysym;

	if (!waiting_for_cursor) return((XtEventHandler) 0);
	if (cursor_mode == BOX_CURSOR) return((XtEventHandler) 0);
	if (have_button) return((XtEventHandler) 0);

	keycode  = event->xkey.keycode;
	keysym   = XKeycodeToKeysym(display,keycode,0);
	if (IsModifierKey(keysym)) 			/* This means shift, alt, etc. */
	  return((XtEventHandler) 0);

	highlight_xhair();						/* Turn off last drawn crosshair */

	point1.x = event->xkey.x;
	point1.y = event->xkey.y;
	have_button = TRUE;

	waiting_for_cursor = FALSE;
	cursor_mode = NO_CURSOR;

	GenplotXRescalePoints(w, &point1, 1);

	cursor.x = point1.x/xscale;
	cursor.y = point1.y/yscale;

	/* Weird keys (esc, enter) have normal ASCII values but with
	   0xFF00 or'd in: this construct reverts them to the normal ASCII */
	cursor.achr = keysym & 0xFF;

	send_result(TRUE);
	send_data(&cursor, sizeof(cursor), "cursor", stdout);

	return((XtEventHandler) 0);
}
 
static XtEventHandler MyButtonPress(Widget w, XtPointer client_data,
	XEvent *event, Boolean *can_continue)
{
	DspCurfnc	cursor;

   REPORT("Button Press.\n",1);
	if (!waiting_for_cursor) return((XtEventHandler) 0);
	if (have_button) return((XtEventHandler) 0);

	if (cursor_mode == POINT_CURSOR) {
		highlight_xhair();						/* Turn off last drawn crosshair */
		GenplotXReleaseGC(w, &cursor_gc);
	}

	point1.x = event->xbutton.x;
	point1.y = event->xbutton.y;
	button   = event->xbutton.button;
	have_button = TRUE;

	if (cursor_mode == BOX_CURSOR){	/* start outlining */
		point2 = point1;
		make_rectangle();
		highlight_rectangle();
		return((XtEventHandler) 0);
	}

	waiting_for_cursor = FALSE;
	cursor_mode = NO_CURSOR;

	GenplotXRescalePoints(w, &point1, 1);

	cursor.x = point1.x/xscale;
	cursor.y = point1.y/yscale;
	cursor.achr = '0' + button - 1;

	send_result(TRUE);
	send_data(&cursor, sizeof(cursor), "cursor", stdout);

	return((XtEventHandler) 0);
}

static XtEventHandler MyButtonRelease(Widget w, XtPointer client_data,
	XEvent *event, Boolean *can_continue)
{
	DspCurbox	cursor;

	if (!waiting_for_cursor) return((XtEventHandler) 0);
	if (cursor_mode != BOX_CURSOR) return((XtEventHandler) 0);
	if (!have_button) return((XtEventHandler) 0);
	if (event->xbutton.button != button) return((XtEventHandler) 0);
	
	waiting_for_cursor = FALSE;
	cursor_mode = NO_CURSOR;

	point2.x = event->xbutton.x;
	point2.y = event->xbutton.y;

	make_rectangle();		/* undo last highlight */
	highlight_rectangle();

	GenplotXReleaseGC(w, &cursor_gc);
	
	GenplotXRescalePoints(w, &point1, 1);
	GenplotXRescalePoints(w, &point2, 1);

	point1.x /= xscale;
	point1.y /= yscale;
	point2.x /= xscale;
	point2.y /= yscale;

	find_pos_length(point1.x, point2.x, &(cursor.x1), &(cursor.x2));
	find_pos_length(point1.y, point2.y, &(cursor.y1), &(cursor.y2));
	cursor.x2 += cursor.x1;
	cursor.y2 += cursor.y1;

	cursor.achr = '0' + button - 1;

	send_result(TRUE);
	send_data(&cursor, sizeof(cursor), "box cursor", stdout);

	return((XtEventHandler) 0);
}

static XtEventHandler MyPointerMotion(Widget w, XtPointer client_data,
	XEvent *event, Boolean *can_continue)
{

	if (in_bounds) {
	   REPORT("Pointer Motion.\n",1);
	} else {
		fprintf(stderr,"Yow! PointerMotion without an EnterWindow!\n");
		fflush(stderr);
	}
	pointr.x = event->xmotion.x;
	pointr.y = event->xmotion.y;
	if (!waiting_for_cursor) {
		in_bounds = 1;
		return((XtEventHandler) 0);
	}
	if (cursor_mode == BOX_CURSOR) {
	  if (!have_button) {
 		 in_bounds = 1;
		 return((XtEventHandler) 0);
	  }

	  if (in_bounds) highlight_rectangle();	/* undo last highlight */

	  point2.x = event->xmotion.x;	/* get new position */
	  point2.y = event->xmotion.y;

	  make_rectangle();		/* put it in the new spot */
	  highlight_rectangle();

	} else if (cursor_mode == POINT_CURSOR) {

	  if (in_bounds) highlight_xhair();		/* undo last highlight */
	  point1.x = event->xmotion.x;	/* get new position */
	  point1.y = event->xmotion.y;
	  highlight_xhair();
	}
	in_bounds = 1;
	return((XtEventHandler) 0);
}

static XtEventHandler MyEnterWindow(Widget w, XtPointer client_data,
	XEvent *event, Boolean *can_continue)
{
   REPORT("Window Entry (%d).\n",in_bounds);
   in_bounds = 1;
	if (!waiting_for_cursor) return((XtEventHandler) 0);
	if (cursor_mode != POINT_CURSOR) return((XtEventHandler) 0);

	point1.x = event->xmotion.x;	/* get new position */
	point1.y = event->xmotion.y;
	highlight_xhair();

	return((XtEventHandler) 0);
}

static XtEventHandler MyLeaveWindow(Widget w, XtPointer client_data,
	XEvent *event, Boolean *can_continue)
{
   REPORT("Window Exit (%d).\n",in_bounds);
   in_bounds = 0;
	if (!waiting_for_cursor) return((XtEventHandler) 0);
	if (cursor_mode != POINT_CURSOR) return((XtEventHandler) 0);
   highlight_xhair();			/* Crosshairs off while cursor isn't here */
	return((XtEventHandler) 0);
}

static void make_rectangle(void)
{
	find_pos_length(point1.x, point2.x, &(rectangle.x), &(rectangle.dx));
	find_pos_length(point1.y, point2.y, &(rectangle.y), &(rectangle.dy));
	return;
}

static void highlight_rectangle(void)
{
	XDrawRectangle(XtDisplay(plot), XtWindow(plot), cursor_gc,
			rectangle.x, rectangle.y, rectangle.dx, rectangle.dy);
	return;
}

static void highlight_xhair(void)
{
   GenplotHighlight_xhair(plot,&cursor_gc,point1.x,point1.y);
	return;
}

#if 0
/* This routine cannot be compiled here because it needs
   X primitives w->core.width and w->core.height
   see file xslavex.c            */
void GenplotHighlight_xhair(Widget w, GC *gc, int px, int py)
{
	XDrawLine(XtDisplay(w), XtWindow(w), *gc,
			0, py, w->core.width, py);
	XDrawLine(XtDisplay(w), XtWindow(w), *gc,
			px, 0, px, w->core.height);
	return;
}
#endif

static void find_pos_length(int x1, int x2, int *pos, int *length)
{
	if (x1 < x2){
		*pos = x1;
		*length = x2-x1;}
	else{
		*pos = x2;
		*length = x1-x2;}
	return;
}

	
/* **************************************************************** */

static void send_result(LOGICAL result) {

	send_data(&result, sizeof(result),"result", stdout);
	return;
}

static LOGICAL get_data(void *buf, size_t size, char *msg, FILE *infile) {

	char s[256];

	if (0 >= fread(buf, size, 1, infile)){
		sprintf(s,"Error reading %s data.\n",msg);
		XtAppWarning(app, s);
		return(FALSE);
	}
	REPORT("xslave read %d bytes.\n",size);
	return(TRUE);
}

static LOGICAL send_data(void *buf, size_t size, char *msg, FILE *file) {

	char	s[256];

	if (0>= fwrite(buf, size, 1, file)){
		sprintf(s, "Error sending %s data.\n",msg);
		XtAppWarning(app, s);
		return(FALSE);
	}
	fflush(file);
	REPORT("xslave sent %d bytes.\n",size);
	return(TRUE);
}
