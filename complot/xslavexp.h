/* Private header file for GenplotX Widget
   Mike Uttormark - 11/7/91

   The following header is required by the original authors. */

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

#ifndef XSLAVEXP_H
#define XSLAVEXP_H

/* Include the public header file for GenplotX */
#include "xslavex.h"

typedef struct{
	int		max;
	int		cnt;
	XSegment	*segs;
} SEG_BUF;

typedef struct{
	int		max;
	int		cnt;
	XPoint		*pnts;
} PNT_BUF;

typedef struct{
	XGCValues	*xgcv;
} PEN_BUF;

typedef union{
	SEG_BUF		seg;
	PNT_BUF		pnt;
	PEN_BUF		pen;
} DATA_UNION;

typedef char   CmdCode;

typedef struct _cmd_struct_ *CMDPTR;

typedef struct _cmd_struct_ {
	CmdCode		code;
	CMDPTR		next;
	CMDPTR		prev;
	DATA_UNION	data;
} CMD;

#define POINT 1
#define LINE  2
#define PEN   3

#define CMD_NULL (CMDPTR) NULL

/* Define the GenplotX instance part */
/* These are the static variables associated with each instance
   of the widget */

#define PBUFFERSIZE   128
#define SBUFFERSIZE   128

typedef struct {

	/* New resource fields */

	int	line_width;
	int	line_style;
	int	cap_style;
	int	join_style;

	Boolean	keep_aspect;
	float	aspect_ratio;

	Cursor	cursor;
	Pixel	cursor_color;
	int	cursor_line_width;

	Pixel	pens[NCOLORS+1];

	/* New internal fields */

	GC	default_gc;	/* Original graphics context */
	GC	current_gc;	/* Current graphics context */

	CMDPTR	cmd_head;
	CMDPTR	cur_cmd;

	Dimension	orig_width;
	Dimension	orig_height;
	Dimension	cur_height;
	Dimension	cur_width;

	float		scalex;
	float		scaley;

	Boolean		can_draw;

	int		pnt_cnt;
	XPoint		pnt_buf[PBUFFERSIZE];

	int		seg_cnt;
	XSegment	seg_buf[SBUFFERSIZE];

} GenplotXPart;

/* Define the full instance record */

typedef struct _GenplotXRec {
	CorePart	core;
	GenplotXPart	genplotx;
} GenplotXRec;

/* typedef for method functions */

typedef void (*PFV)();  /* pointer to function returns void */

/* Define class part structure */

typedef struct {
	PFV		draw_points;
	PFV		draw_lines;
	PFV		erase;
	PFV		change_pen;
	PFV		rescale_points;
	PFV		line_width;
	PFV		flush;
	PFV		get_gc;
	PFV		release_gc;
	XtPointer	extension;
} GenplotXClassPart;

/* Define the full class record */

typedef struct _GenplotXClassRec {
	CoreClassPart		core_class;
	GenplotXClassPart	genplotx_class;
} GenplotXClassRec, *GenplotXWidgetClass;

/* External definition for class record */

extern GenplotXClassRec genplotXClassRec;

/* End of preprocessor directives */
#endif /* XSLAVEXP_H */
