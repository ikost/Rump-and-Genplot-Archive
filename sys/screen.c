/* screen.c */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

#if (defined OS2 || defined NT)			/* Lines per screen */
	#define MAXLINE	25
#else
	#define MAXLINE	24
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
#define	SCREEN_C_SOURCE
#include "mytypes.h"
#include "extends.h"

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
/* My share of the globar vars     */
/* ------------------------------- */
PRIVATE SCRINFO ScrInfo_Default={MAXLINE,80, -1,-1, 0,MAXLINE-1};
EXPORT SCRINFO *ScrInfo=&ScrInfo_Default;

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ---------------------------------------------------------------------------
-- Subroutine to scroll screen (either up or down)
--
-- Usage: call scrl$(nlines,start,end)
--	       call scrl$att(nlines,start,end,attr)
--
-- Inputs: nlines - Number of lines to scroll (+ => up, - => down)
--		              0 => erase the entire region
--         start  - Starting line number
--	        end    - Ending line number
--	        attr   - Attribute to fill region with
--
-- Some checks are made as to the validity of movement.  Basically, it will
-- move the cursor and scroll screen if necessary.  Blank lines are filled
-- with attribute of current cursor position.
--
-- This routine is meant to simulate the <ESC>D and <ESC>M commands from
-- VT100 protocol.
--
-- Notes: 1) The cursor is not moved by this operation
--        2) The scrolled region is always filled with blank attribute
---------------------------------------------------------------------------  */
void ScrScroll(int key, int firstrow, int lastrow) {

	char tok2[3];
	int  i,j,rw1,rw2,row;

	rw1 = min(MAXLINE,max(min(firstrow,lastrow),1));	/* Limit the range */
	rw2 = min(MAXLINE,max(max(firstrow,lastrow),1));

	if (key > 0) {
		row = rw2;								/* Set cursor at bot */
		strcpy(tok2, "\027D");				/* Scroll down command */
		j = key;
	} else {
		row = rw1;								/* Set cursor at top */
		strcpy(tok2, "\027M");				/* Scroll down command */
		j = -key;
		if (j == 0) j = rw2-rw1+1;
	}

	fprintf(stdout, "\027" "7\027[%i;1f", row);

	if (rw1 == rw2) {							/* Same => erase it */
		fputs("\027[0K", stdout);			/* Erase the line */
	} else {
		fprintf(stdout,"\027[%i;%ir", rw1, rw2);
		for (i=0; i<j; i++) fputs(tok2, stdout);
		fprintf(stdout,"\027[1;%ir", MAXLINE);	/* Restore scroll region */
	}

	fputs("\027" "8", stdout);				/* Restore cursor posn */
	return;
}

/* ---------------------------------------------------------------------------
-- Subroutine to erase portion of current line
--
-- Usage: call esclin$(int key)
--
-- Inputs: n - 0 => Erase from active position to end of line (inclusive)
--	            1 => Erase from start of line to active position (inclusive)
--             2 => Erase all of line, inclusive
--
-- This routine is meant to simulate the <ESC>nK of VT100 protocol
--
-- Notes: 1) The cursor is not moved by this operation
--        2) The scrolled region is always filled with blank attribute
---------------------------------------------------------------------------  */
void ScrEraseLine(int key) {
#ifdef VT100
	char str[4]="\0332K";
	str[2] = (char) (key+'0');
	CONputs(str);
#else
	if (key == 0) {
		CONputs("\033[K");
	} else if (key == 2) {
		CONputs("\033[s\033[;1H\033[K\033[u");
	}
#endif
	return;
}

/* ---------------------------------------------------------------------------
-- Subroutine to erase portion of current page
--
-- Usage: call escpag$(n)
--
-- Inputs: n - 0 => Erase from current position to end of screen (inclusive)
--	            1 => Erase from start of screen to current position (inclusive)
--             2 => Erase all of screen
--
-- This routine is meant to simulate the <ESC>nJ of VT100 protocol
--
-- Notes: 1) The cursor is not moved by this operation
--        2) The scrolled region is always filled with blank attribute
---------------------------------------------------------------------------  */
void ScrErasePage(int key) {
	static char str[4]="\033" "2J";
	str[2] = (char) (key+'0');
	CONputs(str);
	return;
}

#ifdef JUNK

/* ------------------------------------------------------------------------- */
-- Routine of questional use, old F77 routines --
/* ------------------------------------------------------------------------- */

/* ============================================================================
-- Subroutine to return the current cursor position
--
-- Syntax: call getpos$(row,column,attrib)
--
-- Output: row    - current row
--         column - current column
--         attrib - current attribute
============================================================================ */
void getpos$(int *row, int *column, int *attrib) {

	int i,j;

	*row = *column = *attrib = 0;				/* Initial guess */

	fputs("\027[6n", stdout);					/* Request position */
	fflush(stdout);
	
	i = getc(stdin);								/* Better be the <ESC> */
	i = getc(stdin);								/* Better be the <[> */
	j = 4;											/* no more than 3 characters */
	while ( (i = getc(stdin)) != ';' && j--) *row = (*row)*10 + i-'0';
	if (j == 0) return;							/* Failure */
	j = 4;
	while ( (i = getc(stdin)) != ';' && j--) *column = (*column)*10 + i-'0';
	return;
}
#endif
