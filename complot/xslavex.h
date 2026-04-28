/* Public header file for GenplotX Widget
   Mike Uttormark - 11/7/91

   The following note is required by the original authors. */

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

/* Make it safe to include this file more than once. */

#ifndef XSLAVEX_H
#define XSLAVEX_H

/* XslaveX is derived from Core, so no need to include the superclass
   public header file.  */

/* New Resources */

#define XtNlineWidth "lineWidth"
#define XtCLineWidth "LineWidth"
#define XtNlineStyle "lineStyle"
#define XtCLineStyle "LineStyle"
#define XtNcapStyle  "capStyle"
#define XtCCapStyle  "CapStyle"
#define XtNjoinStyle "joinStyle"
#define XtCJoinStyle "JoinStyle"
#define XtNmaintainAspectRatio "maintainAspectRatio"
#define XtCMaintainAspectRatio "MaintainAspectRatio"
#define XtNcursor "cursor"
#define XtNcursorColor "cursorColor"
#define XtCCursorColor "CursorColor"
#define XtNcursorLineWidth "cursorLineWidth"
#define XtCCursorLineWidth "CursorLineWidth"

#define XtNpen1 "pen1"
#define XtCPen1	"Pen1"
#define XtNpen2 "pen2"
#define XtCPen2	"Pen2"
#define XtNpen3 "pen3"
#define XtCPen3	"Pen3"
#define XtNpen4 "pen4"
#define XtCPen4	"Pen4"
#define XtNpen5 "pen5"
#define XtCPen5	"Pen5"
#define XtNpen6 "pen6"
#define XtCPen6	"Pen6"
#define XtNpen7 "pen7"
#define XtCPen7	"Pen7"
#define XtNpen8 "pen8"
#define XtCPen8	"Pen8"
#define XtNpen9 "pen9"
#define XtCPen9	"Pen9"
#define XtNpen10 "pen10"
#define XtCPen10 "Pen10"
#define XtNpen11 "pen11"
#define XtCPen11 "Pen11"
#define XtNpen12 "pen12"
#define XtCPen12 "Pen12"
#define XtNpen13 "pen13"
#define XtCPen13 "Pen13"
#define XtNpen14 "pen14"
#define XtCPen14 "Pen14"
#define XtNpen15 "pen15"
#define XtCPen15 "Pen15"
#define XtNpen16 "pen16"
#define XtCPen16 "Pen16"

#define NCOLORS 16

/* External reference to the class record pointer */

extern WidgetClass genplotXWidgetClass;

/* Type definition for label widgets */
typedef struct _GenplotXRec *GenplotXWidget;

/* Parameters for GetGC function */

#define DEFAULT_GC     1
#define CURRENT_GC     2
#define BOX_CURSOR_GC  3


/* Method declarations */

extern void GenplotXDrawPoints(Widget w, XPoint *points, int n);
extern void GenplotXDrawLines(Widget w, XSegment *lines, int n);
extern void GenplotXErase(Widget w);
extern void GenplotXChangePen(Widget w, int pen);
extern void GenplotXLineWidth(Widget w, int lw);
extern void GenplotXFlush(Widget w);
extern void GenplotXRescalePoints(Widget w, XPoint *point, int n);
extern void GenplotXGetGC(Widget w, GC *gc, int which);
extern void GenplotXReleaseGC(Widget w, GC *gc);
extern void GenplotHighlight_xhair(Widget w, GC *gc, int x, int y);

/* End of preprocessor directives */

#endif /* XSLAVEX_H */
