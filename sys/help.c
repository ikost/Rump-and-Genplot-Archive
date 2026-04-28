/* HELP.F77 */

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
#include <unistd.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define	HELP_SIGNATURE	0x1234		/* Magic word identifying .hlc file	*/

typedef struct _TOPIC_RECORD {
	short cmdrecord;						/* Command record						*/
	short cmdstartlen;					/* Start and length (mod 100)		*/
	short DataRecord;						/* Record where data starts		*/
	short nlines;							/* Number of lines					*/
	short lastptr;							/* Caller pointer						*/
	short subptr;							/* Subtopic pointer					*/
	short ntopics;							/* Number of TOPICS					*/
	short lencmd;							/* Number of chars in topic		*/
} TOPIC_RECORD;

typedef struct _INFO_RECORD {			/* First record, 80 bytes long	*/
	short NumberCommands;				/* Number of commands in file		*/
	short	HeaderRecord;					/* Where first record info is		*/
	short	HeaderID;						/* Signature ID word					*/
	short	SwapNumberCommands;			/* Same information byte reverse */
	short	SwapHeaderRecord;
	short	SwapHeaderID;					/* Signature ID byte reversed		*/
	short	dummy[34];						/* Fill out structure to 80 byte	*/
} INFO_RECORD;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
PRIVATE BOOL GetInfo(int itopic, char subject[]);
PRIVATE void GetLine(int LineWanted, int StartRecord, char *outtok);
PRIVATE BOOL ReadRecord(int record, void *buffer);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
PRIVATE	char			*DefaultHelpModule=NULL;	/* Default help module */
PRIVATE	FILE			*funit;							/* Open file */
PRIVATE	TOPIC_RECORD topic;							/* Info on the current topic */

/*----------------------------------------------------------------
-- Special character to hold structure of each help token
---------------------------------------------------------------- */
#define	CTRL_A			0x01
#define	CTRL_B			0x02
#define	CTRL_D			0x04
#define	CTRL_E			0x05
#define	CTRL_F			0x06
#define	CTRL_N			0x0E
#define	CTRL_P			0x10
#define	CTRL_U			0x15
#define	CTRL_Z			0x1A
#define	ESCAPE			0x1B

#define	CTRLHOME			0x177						/* Control home code */

#define	TOPLINE			0x10						/* Special attributes */
#define	TEXTLINES		0x11
#define	SEPARATOR		0x12
#define	TOPICS			0x13						/* Collection of TOPICS */
#define	CURRENTTOPIC	0x14						/* Red selection */
#define	NEXTINFO			0x18
#define	HELPLINE			0x17
#define	SUBJECTTEXT		0x16
#define	INPUTLINES		0x17

#define	PUTINFO			ScrPutString(nrows, 1, \
"   <^D> Go topic         <^U> Up level        <ESC> Exit Help       <        >", \
HELPLINE)

#ifdef TEST
void main(void) {
	CONInitialize();
	SysHelpRequest("genplot", NULL);
	return;
}

#endif

/* ============================================================================
-- Routine to establish a default help module in case someone wants to call
-- help with no specific name.
--
-- Syntax:  void SysHelpSetModule(const char *name);
--
-- Inputs:  name - name of the module to use by default.  NULL clears previous.
--
-- Returns: nothing
============================================================================ */
void SysSetHelpModule(const char *name) {
	free(DefaultHelpModule); DefaultHelpModule=NULL;
	if (name != NULL) DefaultHelpModule = strdup(name);
	return;
}

/* ============================================================================
-- Almost system independent help routine for providing on-line assistance for
-- major packages.  The help is organized as a tree oriented, cursor selected
-- topic function.  Each help topic may have sub-trees which will be listed
-- with the help.
--
-- Syntax:   BOOL SysHelpRequest(const char *module, const char *query);
--
-- Inputs: module  - character name of program.  Help file <module>.hlc must be
--                   found in current or system default search directory.
--                   If <module> is NULL, default help module is used.
--
--         query   - Initial subject to start help on.  Full tree is always
--                   available.  NULL starts at the full tree.
--
-- See the companion program HLPGEN which generates .hlc files.
============================================================================ */
BOOL SysHelpRequest(const char *module, const char *query) {

/* -- Local Variables -- */
	int CurrentTopic, SubTopic;				/* Current and sub-topic elements */
	int	nstored,									/* How many topics in HELP */
			irow,ir,ic,								/* Row,Column pointers */
			ilen,										/* Length of HELP topic (chrs) */
			inow,ishow,								/* Following cursor */
			irowBegin,ichr,
			i,j,ndisp,nskip;
	int	icnt=1, nperline=1;					/* Initialize to avoid errors */
	int	nrows;									/* Number rows on screen */

	BOOL TopicHighlighted=FALSE, HaveTopics=FALSE;
	char		token[80], MySubject[80], SearchTopic[9];
	char		*iptr, *optr;

/* ------------------------------------
-- ... Open the .HLC file from disk
------------------------------------ */
	SearchTopic[8] = '\0';
	if (query  != NULL) strscpy(SearchTopic, query, 8);
	if (module == NULL) module = DefaultHelpModule;
	if (module == NULL) return(FALSE);					/* Nothing to find */

	SysAddExt(strcpy(token,module), ".hlc");			/* Make .hlc name */
	SysResolveDyntName(token, token,sizeof(token));	/* Resolve the name */
	if ( (funit=fopen(token, "rb")) == NULL) {
		gen_err2("Cannot find the .hlc help file", token);
		return(FALSE);
	}
	if (! GetInfo(-1, NULL)) goto ReadError;			/* Read header */
	nstored = topic.nlines;									/* Special line */

/*-------------------------
-- ... Initialization now  
------------------------- */
	nrows = ScrInfo->rows;
	ScrClearAttrib(D_NORMAL);
	PUTINFO;

ReEnterLoop:
	irow = 1;											/* First row is available */
	CurrentTopic = 0;									/* Currently on topic 0 */
	ishow = 1;

/* --------------------------------------------------------
-- ... What topic to consider? 
-------------------------------------------------------- */
	ilen = (int) strnblen(SearchTopic);						/* Length of query */
	if (ilen != 0 && *SearchTopic != '?') {
		for (i=0; i<nstored; i++) {							/* Look through now */
			if (! GetInfo(i, MySubject)) goto ReadError;	/* Get topic record */
			if (strnicmp(MySubject,SearchTopic,ilen) == 0) {	/* Is this it? */
				CurrentTopic = i;
				goto DrawTopic;
			}
		}
		ScrClearAttrib(D_NORMAL);
		PUTINFO;
		strcat(strcpy(token, "No information on "), SearchTopic);
		ScrPutString(1,1, token, TOPLINE);
		irow = 2;
		memset(SearchTopic,' ', 8);
		goto DrawOthers;
	}

/* ============================================================================
-- ... Repeat code here  200 => topic info   300 => list additional TOPICS
============================================================================ */

/* -----------------------------------------------------------
-- ... Print out the lines of info corresponding to topic IPTR
----------------------------------------------------------- */
DrawTopic:	
	memset(SearchTopic,' ', 8);
	irow = 1;													/* Start row 1 */
	ScrClearAttrib(D_NORMAL);
	PUTINFO;
	if (! GetInfo(CurrentTopic, MySubject)) goto ReadError;	/* Get info for sure */
	if (topic.nlines > 1) {
		GetLine(1, topic.DataRecord, token);
		ScrPutString(1,1, token, TOPLINE);
		for (i=2; i<topic.nlines; i++) {
			GetLine(i, topic.DataRecord, token);
			ScrPutString(i	,1, token, TEXTLINES);
		}
		irow = topic.nlines;
	}

/* ----------------------------------------------------------
-- ... And now for the additional TOPICS we can print about
---------------------------------------------------------- */
DrawOthers:
	if (irow == 1) {												/* Erase screen? */
		ScrClearAttrib(D_NORMAL);
		PUTINFO;
	} else {
		memset(token,'=',79); token[79]='\0';
		ScrPutString(irow++,1,token,SEPARATOR);
	}

	if (! GetInfo(CurrentTopic, MySubject)) goto ReadError;	/* Get info */
	HaveTopics = (topic.subptr > 0);
	ilen = 0;												/* No char mode now */
	irowBegin = irow;										/* Row TOPICS begin */

	if (HaveTopics) {										/* Count # TOPICS */
		SubTopic = topic.subptr;						/* Sub-topic pointer */
		if (! GetInfo(SubTopic, MySubject)) goto ReadError;	/* Get sub-topic info */
		icnt = topic.ntopics;							/* Number of TOPICS */
		ndisp = topic.lencmd;							/* Length of cmds */
		nperline = 80/(ndisp+2);						/* How many / line */
		nskip = 80/nperline;								/* Skip per point */

		optr = token;									/* Start blank */
		for (i=0; i<icnt; i++) {						/* Print them */
			if (! GetInfo(SubTopic+i, MySubject)) goto ReadError;
			if (optr != token) for (j=0; j<nskip-ndisp; j++) *(optr++) = ' ';
			iptr = MySubject;
			for (j=0; j<ndisp; j++) *(optr++) = *(iptr++);
			if (optr+nskip-token > 79) {
				*optr = '\0';
				ScrPutString(irow++,1, token,TOPICS);
				optr = token;
				irow = min(nrows-1, irow);			/* Next row! */
			}
		}
		if (optr != token) {								/* Info left on line */
			*optr = '\0';
			ScrPutString(irow++,1, token,TOPICS);
		}

		if (ishow >= SubTopic && ishow < SubTopic+icnt) {
			inow = ishow-SubTopic;									/* Which to show */
		} else {
			ishow = SubTopic;											/* Start on top */
			inow = 0;
		}
	}

CheckForKey:
	if ( (ichr = CONchkchr()) == -1) {						/* No character pressed? */
		if (HaveTopics) {
			if (! GetInfo(ishow, MySubject)) goto ReadError;	/* Which to show? */
			ir = irowBegin+inow/nperline;						/* Which line */
			ic = nskip*( inow % nperline)+1;					/* Which column */
			MySubject[ndisp] = '\0';
			ScrPutString(ir,ic, MySubject,CURRENTTOPIC);	/* Bold it */
			if (topic.nlines > 0) {
				GetLine(0,topic.DataRecord,token);			/* Get the data */
				token[strnblen(token)] = '\0';
				ScrPutString(irow,2,token,NEXTINFO);
			}
			TopicHighlighted = TRUE;
		} else {
			ir = irow;
			ic = 1;
		}
		ScrPutString(nrows,70,SearchTopic,SUBJECTTEXT);	/* Rewrite TEXT info */
		ScrSetPosn(nrows,70+ilen,D_NORMAL);
		ichr = CONgetc();											/* And, now get a char */
	}

	if (TopicHighlighted) {
		ScrPutString(ir,ic,MySubject,TOPICS);				/* Unbold it */
		ScrSetPosn(irow, 1, D_NORMAL);
		ScrEraseLine(0);
		TopicHighlighted = FALSE;
	}

	switch (ichr) {										/* Intepret the key now */
		case VIRTUAL_LEFT:
		case CTRL_B:
         inow = (inow+icnt-1) % icnt;
			break;
		case VIRTUAL_RIGHT:
		case CTRL_F:
         inow = (inow+1) % icnt;
			break;
		case VIRTUAL_DOWN:
		case CTRL_N:
			if (inow+nperline < icnt)
				inow = inow+nperline;
			else {
            inow = (inow+1) % nperline;
            if (inow >= icnt) inow = 0;
			}
			break;
		case VIRTUAL_UP:
		case CTRL_P:
		case CTRL_Z:
			if (inow >= nperline) 
            inow = inow-nperline;					/* Next position */
			else {
            inow = min( (inow+nperline-1) % nperline,icnt-1);
				while (inow+nperline < icnt) inow = inow+nperline;
			}
			break;
		case VIRTUAL_END:
		case CTRL_E:
			inow = icnt-1;
			break;
		case VIRTUAL_HOME:								/* Lowest level info */
		case CTRL_A:
			inow = 0;
			break;
		default:
			goto Line_420;
		}
      ilen = 0;											/* Character mode off */
		memset(SearchTopic, ' ', 8);
		ishow = inow+SubTopic;							/* Subject pointer */
		goto CheckForKey;

/* ------------------------
-- ... UP or DOWN the tree
------------------------ */
Line_420:
	switch (ichr) {
		case VIRTUAL_PAGEUP:
		case CTRL_U:
			if (! GetInfo(CurrentTopic, MySubject)) goto ReadError;			/* Switch to last pointer */
			ishow = CurrentTopic;										/* Try to show me */
			CurrentTopic = topic.lastptr;
			goto DrawTopic;
		case '\n':
		case '\r':
		case VIRTUAL_PAGEDOWN:
		case CTRL_D:
			if (! HaveTopics) {
				RingBell();
				goto CheckForKey;
			}
			CurrentTopic = ishow;
			goto DrawTopic;
		case CTRLHOME:										/* Level 0,0 */
			CurrentTopic = 0;
			goto DrawTopic;
		case ESCAPE:
			fclose(funit);
			ScrSetPosn(nrows,1,D_NORMAL);
			ScrEraseLine(0);								/* Erase the line */
			ScrSetPosn(irow+1,1,D_NORMAL);			/* Final position */
			return(TRUE);
		case '\b':											/* Backspace on token */
			ilen = max(0,ilen-1);
			SearchTopic[ilen] = ' ';
			break;
		case '?':
			ScrPutString(12,14,"  =======================  ",INPUTLINES);
			ScrPutString(13,14,"  | TOPIC: ____________ |  ",INPUTLINES);
			ScrPutString(14,14,"  =======================  ",INPUTLINES);
			ScrSetPosn(13,25,INPUTLINES);
			UserInput(NULL ,SearchTopic, sizeof(SearchTopic));
			goto ReEnterLoop;
		default:
			if (ichr >= ' ' && ichr <= 0x7E && ilen < 8) {
				SearchTopic[ilen++] = (char) ichr;
			} else {													/* All others ignored */
				RingBell();
				goto CheckForKey;
			}
	}

	if (! HaveTopics) goto CheckForKey;			/* No TOPICS to look for - ignore */

/* ----------------------------------------
-- ... Search for string match!
---------------------------------------- */
	if (ilen == 0) {										/* No string, top! */
		inow = 0;
		ishow = SubTopic;
		goto CheckForKey;
	}
	j = inow;												/* Save inow */
	for (inow = 0; inow<icnt-1; inow++) {
		ishow = inow+SubTopic;
		if (! GetInfo(ishow, MySubject)) goto ReadError;
		if (strnicmp(MySubject, SearchTopic, ilen) == 0) goto CheckForKey;
	}
	ilen--; SearchTopic[ilen] = ' ';
	RingBell();												/* Not found */
	inow = j;												/* Same place! */
	ishow = inow+SubTopic;
	goto CheckForKey;										/* Display this one */

ReadError:
	gen_err("Unexpected READ error on help file");
	fclose(funit);
	return(FALSE);
}

/* ============================================================================
-- Routine to return the header information on the i'th topic in list
--
-- Usage:  BOOL = GetInfo(ITOPIC, Mysubject)
--
-- Inputs: itopic - Topic requested.
--                  If -1, request for information only
--         
-- Output: Sets the *topic pointer to valid topic header.
--
-- Returns information on topic specified by number.  0 => first topic which
-- has the count.  From there, 1,2 etc. are stored in order in the header.
============================================================================ */
PRIVATE BOOL GetInfo(int itopic, char subject[]) {

	static TOPIC_RECORD TopicRecord[5];	/* 5 topics per 80 byte record */
	static char SubjectInfo[80];			/* Encoded subject information */
	static int FirstTopicRecord,			/* Offset to information */
				  InMemory_1,					/* Record in TopicRecord */
				  InMemory_2;					/* Record in SubjectInfo */

	int i,j;

	if (itopic == -1) {											/* Initialize			*/
		INFO_RECORD info;											/* First record		*/
		InMemory_1 = InMemory_2 = -1;							/* Nothing saved yet	*/
		if (! ReadRecord(1, &info)) return(FALSE);		/* Read first record	*/
		if (info.HeaderID == HELP_SIGNATURE) {				/* Which byte order	*/
			topic.nlines     = info.NumberCommands;		/* As written first?	*/
			FirstTopicRecord = info.HeaderRecord;			/* Where INFO_RECORD	*/
		} else if (info.SwapHeaderID == HELP_SIGNATURE) {
			topic.nlines     = info.SwapNumberCommands;	/* Or byte reversed	*/
			FirstTopicRecord = info.SwapHeaderRecord;		/* Info here reverse	*/
		} else {
			gen_err("Unable to find signature word in help file");
			return(FALSE);
		}
		return(TRUE);
	}

/* --- 5 topics per record; i=record with topic header; j=element in record */
	i = FirstTopicRecord+itopic/5;							/* 5 topics/record			*/
	j = itopic%5;										/* Where in the record		*/
	if (i != InMemory_1) {
		InMemory_1 = i;
		if (!ReadRecord(InMemory_1, &TopicRecord)) return(FALSE);
	}
	topic = TopicRecord[j];							/* Topic information			*/
	if (topic.cmdrecord != InMemory_2) { 		/* Where is subject stored	*/
		InMemory_2 = topic.cmdrecord;
		if (! ReadRecord(InMemory_2, &SubjectInfo)) return(FALSE);
	}

/* Decode the subject itself */
	i  = topic.cmdstartlen % 100;				/* Char posn of subject		*/
	j  = topic.cmdstartlen / 100;				/* And length (in chars)	*/
	memset(subject, ' ', 79); subject[79] = '\0';
	strncpy(subject, SubjectInfo+i-1, j);	/* Really is strncpy here! */
	return(TRUE);

}

/* ===========================================================================
-- The GetLine retrieves a line of encoded text from the help file.  
--
-- Usage: GetLine(int line, int StartRecord, char *token)
--
-- Inputs: line       - number of record wanted.  0 => first
--         DataRecord - Record where data starts
--
-- Output: token - uncompressed text from the record list.
============================================================================ */
PRIVATE void GetLine(int LineWanted, int StartRecord, char *outtok) {
	
	char result[80], *optr, *iptr;
	int i;

	static char Record_1[80], *aptr;
	static int MyStartRecord=-1,		/* Start record I was working with */
				  CurrentRecord,			/* Current record in memory */
				  LastRetrieved;			/* Last line retrieved */

/* First, check if still looking through the same set or if must start over */
	if ( (StartRecord != MyStartRecord) ||
	     (LineWanted  <= LastRetrieved)) {					/* Must restart */
		CurrentRecord = MyStartRecord = StartRecord;
		if (! ReadRecord(CurrentRecord, Record_1)) goto ReadError;
		LastRetrieved = -1;										/* Didn't get previous */
		aptr = Record_1;											/* Okay, let's start */
	}

/* Now, just copy through until I get the one I want */
	do {
		optr = result;
		do {
			if (aptr-Record_1 >= 80) {
				if (! ReadRecord(++CurrentRecord, Record_1)) goto ReadError;
				aptr = Record_1;
			}
			*(optr++) = *aptr;
		} while (*(aptr++));
	} while (++LastRetrieved < LineWanted);

/* Now, unpack the string */
	iptr = result; optr = outtok;
	while (*iptr) {
		if (*iptr != 0x01) {							/* Is it expandable space */
			*(optr++) = *(iptr++);
		} else {
			iptr++;											/* Next is the count */
			for (i=*(iptr++); i; i--) *(optr++) = ' ';
		}
	}
	while (optr-outtok < 79) *(optr++) = ' ';
	*optr = '\0';
	return;

ReadError:
	gen_err("Internal HELP$ error (001)");
	*outtok = '\0';
	return;
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
PRIVATE BOOL ReadRecord(int record, void *buffer) {

   int rc;

	if (fseek(funit, 80L*(record-1), SEEK_SET) != 0) {
		ERRprintf("ERROR: Seek record %i failed on file\n", record);
		return(FALSE);
	} else if ( (rc = (int) fread(buffer, 80, 1, funit)) != 1) {
		ERRprintf("ERROR: Buffer read returned %i for record %i\n", rc, record);
		sleep(1);
		return(FALSE);
	}
	return(TRUE);
}
