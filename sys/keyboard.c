/* keyboard.c */

/* ---------------------------------------------------------------------------
-- Originally, only used to handle keyboard translation from multi-char strokes
-- to pseudo keys.  Now also stores basic terminal control information so don't
-- need access to varying termcap files.
--
-- Organization similar to termcap-files.  Contain input control sequence data
-- used to read input character strings, and simple screen manipulation
--------------------------------------------------------------------------- */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

#undef DEBUG									/* Leave defined for debugging */
/* #define TEST	*/							/* For local testing via main at end */

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#if defined GNU_C || defined AIX_C
   #include <termios.h>
#endif

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define	TABLEORGSIZE		50			/* Initial table size					*/
#define	TABLEINCSIZE		25			/* Increment size on table				*/
#define	SEQTIMELIMIT		500		/* ms between chars in a sequence	*/
#define	MAGIC					0x1237	/* Magic signature indicating valid	*/

typedef struct _BRANCH {				/* Key branching table entry	*/
	struct _BRANCH *next;				/* Next entry, same level		*/
	char key;								/* Key to match					*/
	int code;								/* Code to return if match		*/
	struct _BRANCH *nest;				/* First entry of next level	*/
} BRANCH;

typedef struct _CODES {					/* List of recognized keys		*/
	char *name;								/* Key name (ASCII)				*/
	int	code;								/* Code needed by GENPLOT		*/
} CODES;

typedef struct _KEYTABLE {				/* Table of internal info		*/
	int		signature;					/* Magic signature as ID		*/
	int		NumEntries;					/* # filled entries in Table	*/
	int		MaxEntries;					/* # total entries in  Table	*/
	char		setup[DFLT_STR_SIZE];	/* Initialization string		*/
	char		exit[DFLT_STR_SIZE];		/* Termination string			*/
	BRANCH	Table[TABLEORGSIZE];		/* Branching table list			*/
} KEYTABLE;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */
int   Kgetent(char *termtype, void **entry);
int	Kfree(void **entry);
void	Kinit(void *entry);
int   Kgetkey(void *entry);
void	Kprintdata(void *entry);

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static BRANCH *PutKey(KEYTABLE *keytable, BRANCH *key, int mchr);
static int		ExpandTable(KEYTABLE **entry, BRANCH **keyadr);
static int		InsertKeyString(void **entry, char *string, int code);
static void		PrintBranch(BRANCH *key, int indent);
#ifdef TEST
	#define TTYprintf printf
#endif

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */
int CONwaitchr(int msecs);					/* *** WARNING *** FROM low_io.c */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
static BRANCH EmptyBranch = {NULL, 0, 0, NULL};		/* Empty Branch for init */

#define	DO_SETUP	-1
#define	DO_EXIT	-2

#ifdef FINISH_THIS
  cols=nn  number of columns
  lines=nn number of lines

  clear=\E[2J\E[H Clears the screen, leaving the cursor in the home position.
  cub1=\^H        Moves the cursor one space to the left, such as backspace.
  cup=\E[%d;%dH   Addresses the cursor using (row, column) format.

Strings to set screen attributes - each should reset before setting if appropriate
  col_default - default color setup if none found
  col_normal  - normal text output
  col_input   - input from keyboard
  col_bold    - bold text
  col_prompt  - prompts for input
  col_error   - error messages
  col_filein  - input coming from files
  col_alt     - alternate input mode
  col_res     - reserved color mode

Strings to set screen attributes for help session (ONLY)
  col_titles  - titles on help screens
  col_text    - text
  col_seps    - separators
  col_if      - input field
  col_cif     - current input field
  col_cic     - current input char
  col_sec     - secondary input field
  col_help    - help field
  col_ihelp   - input field help

static CODES screencodes[] = {
	{"col_default",	0},			{"col_normal",		0x20},		{"col_input",		0x21},
	{"col_bold",		0x22},  		{"col_prompt",		0x23},		{"col_error",		0x24},
	{"col_filein",		0x25},		{"col_alt",			0x26},		{"col_res",			0x27},

	{"col_titles",		0x10},		{"col_text",		0x11},		{"col_seps",		0x12},
	{"col_if",			0x13},		{"col_cif",			0x14},		{"col_cic",			0x15},
	{"col_sec",			0x16},		{"col_help",		0x17},		{"col_ihelp",		0x18},

erase_line
clear_screen


	{"eol",			ERASE_EOL		  },	{"home",			GO_HOME				},
	{"clear",		CLEAR


d0|vt100|vt100-am|vt100|dec vt100:\
	:cr=^M:do=^J:nl=^J:bl=^G:co#80:li#24:cl=50\E[;H\E[2J:\
	:le=^H:bs:am:cm=5\E[%i%d;%dH:nd=2\E[C:up=2\E[A:\
	:ce=3\E[K:cd=50\E[J:so=2\E[7m:se=2\E[m:us=2\E[4m:ue=2\E[m:\
	:md=2\E[1m:mr=2\E[7m:mb=2\E[5m:me=2\E[m:is=\E[1;24r\E[24;1H:\
	:rs=\E>\E[?3l\E[?4l\E[?5l\E[?7h\E[?8h:ks=\E[?1h\E=:ke=\E[?1l\E>:\
	:rf=/usr/lib/tabset/vt100:ku=\EOA:kd=\EOB:kr=\EOC:kl=\EOD:kb=^H:\
	:ho=\E[H:k1=\EOP:k2=\EOQ:k3=\EOR:k4=\EOS:ta=^I:pt:sr=5\EM:vt#3:xn:\
	:sc=\E7:rc=\E8:cs=\E[%i%d;%dr:

#endif

static CODES keycodes[] = {
	{"setup",		DO_SETUP				},	{"init",			DO_SETUP				},
	{"exit",			DO_EXIT				},	{"reset",		DO_EXIT				},
	{"bs",			'\b'					},	{"tab",			'\t'					},
	{"esc",			0x1B					},	{"space",		' '					},
	{"nl",			'\n'					},	{"del",			0x7F					},
	{"comma",		','					},

	{"left",			VIRTUAL_LEFT		},	{"up",			VIRTUAL_UP        },
	{"right",		VIRTUAL_RIGHT		},	{"down",			VIRTUAL_DOWN      },
	{"pgup",			VIRTUAL_PAGEUP		},	{"pgdn",			VIRTUAL_PAGEDOWN  },
	{"pageup",		VIRTUAL_PAGEUP		},	{"pagedown",	VIRTUAL_PAGEDOWN  },
	{"end",			VIRTUAL_END			},	{"home",			VIRTUAL_HOME      },
	{"ins",			VIRTUAL_INSERT    },

	{"ctrlleft",	VIRTUAL_CTRL_LEFT	}, {"ctrlright",	VIRTUAL_CTRL_RIGHT},
	{"ctrlup",		VIRTUAL_CTRL_UP   }, {"ctrldown",   VIRTUAL_CTRL_DOWN },
	{"ctrlins",    VIRTUAL_CTRL_INSERT},{"ctrldel",		VIRTUAL_CTRL_DELETE},
	{"ctrlhome",	VIRTUAL_CTRL_HOME },	{"ctrlend",		VIRTUAL_CTRL_END	},
	{"ctrlpgdn",	VIRTUAL_CTRL_PAGEDOWN},
	{"ctrlpgup",	VIRTUAL_CTRL_PAGEUP},

	{"b1",			VIRTUAL_BUTTON1	},	{"b2",			VIRTUAL_BUTTON2   },
	{"b3",			VIRTUAL_BUTTON3	},

	{"F1",			VIRTUAL_F1			},	{"F2",			VIRTUAL_F2			},
	{"F3",			VIRTUAL_F3			},	{"F4",			VIRTUAL_F4			},
	{"F5",			VIRTUAL_F5			},	{"F6",			VIRTUAL_F6			},
	{"F7",			VIRTUAL_F7			},	{"F8",			VIRTUAL_F8			},
	{"F9",			VIRTUAL_F9			},	{"F10",			VIRTUAL_F10			},
	{"F11",			VIRTUAL_F11			},	{"F12",			VIRTUAL_F12			},
	{"break",		VIRTUAL_BREAK		},	{"backtab",		VIRTUAL_BACKTAB   },
	{"scrolllock",VIRTUAL_SCRLLOCK	},	{"printscreen",VIRTUAL_PRINTSCRN	},
	{"numlock",		VIRTUAL_NUMLOCK	},	{"enter",		VIRTUAL_ENTER     },
	{"sysreq",		VIRTUAL_SYSRQ		},
	{NULL,			0}
};


/* ===========================================================================
-- Routine to insert a given key value into the branching list.  Will either
-- be put into the next level from current key, or will open up a new lower
-- level.
--
-- Usage:	static BRANCH *PutKey(KEYTABLE *keytable, BRANCH *key, int mchr)
--
-- Inputs:	keytable - Pointer to the key handling table.
--				key      - Pointer to BRANCH of last char in string.  If NULL, 
--							  this is first key of string.
--				mchr		- character at this position of string.
--
-- Returns:	key      - new pointer to BRANCH of this key.
=========================================================================== */
static BRANCH *PutKey(KEYTABLE *keytable, BRANCH *key, int mchr) {

	if ( (key == NULL) || (key->nest != NULL) ) {	/* Next level search	*/
		key = (key == NULL) ? keytable->Table : key->nest;
		while (key->key != mchr) {
			if (key->next != NULL) {				/* Nope, try next in list */
				key = key->next;
			} else {
				key = key->next = keytable->Table + keytable->NumEntries++;
				*key = EmptyBranch;					/* Make sure it's empty	*/
				key->key  = mchr;						/* Insert key				*/
				break;									/* DONE!						*/
			}
		}
	} else {
		key = key->nest = keytable->Table + keytable->NumEntries++;
		*key = EmptyBranch;							/* And clear entries		*/
		key->key  = mchr;								/* Insert key				*/
	}
	return (key);
}

/* ===========================================================================
-- Routine to expand the table if necessary.  Must handle pointer redirection
-- within the routine.  Could dispense with this if next and nest were ints.
--
-- Usage:	static int ExpandTable(KEYTABLE **entry, BRANCH **keyadr);
--
-- Inputs:	entry  - pointer to pointer of the main keytable.
--				keyadr - pointer to pointer of one BRANCH level
--
-- Returns:	Increases size of table and modifies *entry and *keyadr as
--				necessary.  Updates structure entries as necessary.
=========================================================================== */
static int ExpandTable(KEYTABLE **entry, BRANCH **keyadr) {
				
	KEYTABLE *newtable;
	BRANCH	*key;
	int		 i, oldsize, newsize;
	ptrdiff_t diff;

	oldsize = (*entry)->MaxEntries;
	newsize = (*entry)->MaxEntries = oldsize + TABLEINCSIZE;
		
	newtable = realloc(*(entry), sizeof(KEYTABLE) + (newsize-TABLEORGSIZE)*sizeof(BRANCH));
	diff = (char *) newtable - (char *) (*entry);
#ifdef DEBUG
	printf("Reallocing %p to %p  %x\n", *(entry), newtable, diff);
	fflush(stdout);
#endif

/* ... Translate all addresses in the table */
	if (diff != 0) {
		for (i=0,key=newtable->Table; i<oldsize; i++,key++) {
			if (key->nest != NULL) 
				key->nest = (BRANCH *) ( ((char *) key->nest) + diff);
			if (key->next != NULL)
				key->next = (BRANCH *) ( ((char *) key->next) + diff);
		}
	}

/* Modify return values to match */
	*keyadr = (BRANCH *) ( ((char *) *keyadr) + diff);
	*entry = newtable;
	return(0);
}

/* ===========================================================================
-- Routine to convert next "char" in string to int value.
-- Handles translations of strings such as \b, \E, \127 \x7F etc.
--
-- Usage:	static int ConvertEscapeChar(char **instr)
--
-- Inputs:	instr - pointer to current string pointer position
--
-- Returns:	Value of the character.
--          **instr reset to point to next available char
=========================================================================== */
static int ConvertEscapeChar(char **instr) {

	char *string = *instr;								/* Input string				*/
	int	i,achr;

	achr = *string++;										/* Next available char		*/

/* Translate escaped characters */
	if (achr == '\0') {									/* Already at string end	*/
		return(0);
	} else if (achr == '\\') {							/* Escaped character			*/
		achr = *string++;
		if (achr == 'x') {								/* Format of \x1b				*/
			achr = 0;
			for (i=0; i<2 && isxdigit(*string); i++,string++) {
				achr = 16*achr + tolower(*string)-'0';
				if (tolower(*string) >= 'a') achr += 10-('a'-'0');
			}
		} else if (achr=='0' && isdigit(*string)) {	/* Format of \027	*/
			achr = 0;
			for (i=0; i<3 && isdigit(*string); i++,string++)
				achr = 8*achr + *string-'0';
		} else if (achr != '0' && isdigit(achr)) {	/* Format of \127 */
			achr = achr-'0';
			for (i=0; i<3 && isdigit(*string); i++,string++)
				achr = 10*achr + *string-'0';
		} else switch (achr) {
			case '0':	achr = '\0'; break;
			case 'a':	achr = '\a'; break;
			case 'b':	achr = '\b'; break;
			case 'f':	achr = '\f'; break;
			case 'n':	achr = '\n'; break;
			case 'r':	achr = '\r'; break;
			case 't':	achr = '\t'; break;
			case 'v':	achr = '\v'; break;
			case 'E':
			case 'e':	achr = 0x1b; break;
			default:
				fprintf(stderr, "Unrecognized escape character: %c\n", *(string-1));
				achr = '\a';								/* Fake it as an alert		*/
		}
	}
	*instr = string;										/* Return modified string	*/
	return(achr);											/* And return value			*/
}

/* ===========================================================================
-- Routine to insert a keystring into table structure.
--
-- Usage:	static int InsertKeyString(void **entry, char *string, int code);
--
-- Inputs:	entry  - pointer to pointer of main table.  Returned to user.
--				string - character string representing the key
--				code   - key code to be returned on this string
--
-- Returns:	modified entry with current table.  May be modified by expansion.
--
-- Notes:	On first call, *entry should be NULL indicating that table should
--				be initialized.  Space in *entry may be free()'d later if desired.
=========================================================================== */
static int InsertKeyString(void **entry, char *str, int code) {

	KEYTABLE *keytable;
	BRANCH	*key;
	char		*aptr, *string=str;

/* Initialize entry table if not done before */
	if (*entry == NULL) {
		keytable = malloc(sizeof(KEYTABLE));
		keytable->signature  = MAGIC;				/* Mark table as valid		*/
		keytable->MaxEntries = TABLEORGSIZE;	/* Make room for init #		*/
		keytable->NumEntries = 0;					/* No entries yet in table	*/
		keytable->setup[0] = '\0';					/* No initialize string		*/
		keytable->exit[0]  = '\0';					/* No termination string	*/
		keytable->Table[0] = EmptyBranch;		/* Zero this puppy			*/
	} else {
		keytable = *entry;
	}

	if (code==DO_SETUP || code==DO_EXIT) {		/* Terminal init/reset strings */
		aptr = (code==DO_SETUP) ? keytable->setup : keytable->exit;
		while (*string != '\0') *aptr++ = ConvertEscapeChar(&string);
		*aptr = '\0';
	  	*entry = keytable;							/* Pass back value */
	  	return(0);
	}

	key = NULL;											/* Start as nothing		*/
	while (*string != '\0') {						/* Do all chars of file	*/
		key = PutKey(keytable, key, ConvertEscapeChar(&string)) ;
		if (keytable->NumEntries >= keytable->MaxEntries) 	/* Expand? */
			ExpandTable(&keytable, &key);
	}

	if (key->code != 0)								/* Is it already in use */
		TTYprintf("WARNING: String multiply defined (%s)\n", str);
	key->code = code;

	*entry = keytable;					/* Pass back (possibly modified) value */
	return(0);
}


/* ===========================================================================
-- Routine to free a loaded keyboard table.  Also resets the terminal if there
-- was an exit= entry in the table.
--
-- Usage:	int Kfree(void **entry);
--
-- Inputs:	entry		- address of a (void *) pointer that was returned by
--							  Kgetkey().  Pointer will be set to NULL on exit.
--
-- Returns:	 0  ==> Unable to find the database termkey file
--				-1  ==> Nothing in structure
=========================================================================== */
int Kfree(void **entry) {

	KEYTABLE *keytable;

	keytable = (KEYTABLE *) *entry;			/* Cast pointer			*/

	if (keytable == NULL || keytable->signature != MAGIC) return(-1);

	if (*keytable->exit != '\0') fputs(keytable->exit, stdout);

	keytable->signature = 0;					/* Invalid structure		*/
	free(keytable);								/* And free memory		*/
	*entry = NULL;									/* Mark now as unused	*/
	return(0);

}


/* ===========================================================================
-- Routine to initialize a loaded keyboard table.  Set the terminal to the
-- desired mode if there was a setup= entry in the table.
--
-- Usage:	void Kinit(void *entry);
--
-- Inputs:	entry	- valid pointer returned by Kgetkey().
--
-- Returns:	 void
=========================================================================== */
void Kinit(void *entry) {

	KEYTABLE *keytable;

	keytable = (KEYTABLE *) entry;			/* Cast pointer			*/

	if (keytable!=NULL && keytable->signature==MAGIC && *keytable->setup!='\0')
		fputs(keytable->setup, stdout);

	return;
}


/* ===========================================================================
-- Routine to lookup a terminal type in termkey database and fill in a search
--	structure to be passed to Kgetkey() routine.
--
-- Usage:	int Kgetent(char *termtype, void **entry);
--
-- Inputs:	termtype - character string for terminal type (vt320)  (or)
--							  pathname for replacement termkey file (first char /) (or)
--							  NULL or '\0' to use KEYBOARD or TERM as terminal type.
--				entry		- address of a (void *) pointer that is returned if
--							  successful with structure to be passed to Kgetkey().
--
-- Returns:	-1  ==> Unable to find the database termkey file
--				 0  ==> Specified terminal type is not in the database
--				 1  ==> Successful
=========================================================================== */
int Kgetent(char *termtype, void **entry) {

	char *keyline = NULL;				/* String from termkey file				*/
	char *keyfile = "termkey";			/* Keyboard description file to search	*/
	char Filename[PATH_MAX];			/* Resolved filename							*/
	char *envname;							/* Environment termtype						*/
	FILE *funit;
	CODES *cd;								/* Pointer to codes[] table */
	char *aptr, *bptr, *name, *value;
	char line[LONG_STR_SIZE];			/* Maximum line length in file */

	*entry = NULL;							/* In case we fail, don't want it set */

/* See if we want to modify the termkey file */
	if ( (termtype != NULL) && (*termtype != '\0') && (*termtype == '/') ) {
		keyfile = termtype;
		termtype = NULL;
	}

/* Make sure we have a valid terminal name stored in envname */
	envname = getenv("KEYBOARD");
	if (envname == NULL) envname = getenv("TERM");
	if (envname == NULL) envname = "default";

/* Use envname as termtype if I don't otherwise have one */
	if (termtype == NULL || *termtype == '\0') termtype = envname;

#ifdef DEBUG
	printf("termtype: %s   envname: %s\n", termtype, envname);
	fflush(stdout);
#endif

/* ------------------------------------------------
-- Get a line of text with the key descriptions
-- If termtype matches envname, use TERMKEY if set.
--------------------------------------------------- */
	if ((stricmp(termtype,envname)==0) && ((keyline=getenv("TERMKEY"))!=NULL))
		keyline         = strdup(keyline);

/* --------------------------------------------------
-- If don't have keyline set, open file and search
---------------------------------------------------- */
	if (keyline == NULL) {
		SysResolveDyntName(Filename, keyfile, sizeof(Filename));
		if ( (funit = fopen(Filename, "r")) == NULL) return(-1);
		while (fgets(line, sizeof(line), funit) != NULL) {
			if (*line == '\0' || *line == '#' || isspace(*line)) continue;
			if ( (aptr=strchr(line, ':')) == NULL) continue;
			*aptr = '\0';								/* Place eos at end of names	*/
			bptr = line;								/* Set bptr to start				*/
			while ( (bptr=strtok(bptr, "|")) != NULL) {
				if (stricmp(termtype, bptr) == 0) {
					keyline = malloc(2048);			/* Better be big enough	*/
					strcpy(keyline, aptr+1);		/* Rest of the text		*/
					aptr = keyline+strlen(keyline);
					while (aptr != keyline && isspace(*(aptr-1)) ) aptr--;
					while ( *(aptr-1) == '\\') {
						*(--aptr) = '\0';				/* Truncate last charater */
						if (fgets(line, sizeof(line), funit) == NULL) break;
						bptr = line;
						while (isspace(*bptr)) bptr++;
						if (*bptr == ':') bptr++;	/* This should happen!			*/
						strcpy(aptr, bptr);			/* Copy this line into string	*/
						aptr += strlen(aptr);		/* Skip to end of line			*/
						while ( (aptr != keyline) && isspace(*(aptr-1)) )
							*(--aptr) = '\0';			/* Strip trailing whitespace */
					}
					break;								/* Gets out of strtok loop */
				}
				bptr = NULL;							/* strtok gets NULL second time */
			}
			if (keyline != NULL) break;			/* Out of while fgets() loop */
		}
		fclose(funit);
		if (keyline == NULL) return(0);			/* No entries */
	}
			
#ifdef DEBUG
	printf("Got line: %s\n", keyline);
	fflush(stdout);
#endif

/* ------------------------------------------------
-- Now, have long string of form <code>=<value> where
-- <code> is one of the mnemonics above and <value> is
-- a key sequence.  Decode string and build table.
--------------------------------------------------- */
	aptr = keyline;
	while ( (name = strtok(aptr, ":")) != NULL) {
		aptr = NULL;											/* So strtok() works */
		if ( (value = strchr(name,'=')) == NULL) {	/* Valid string?		*/
			fprintf(stderr, "Invalid <name>=<value> in termkey: %s\n", name);
			continue;											/* Go to next			*/
		}
		*value++ = '\0';										/* Split name, value	*/
		for (cd=keycodes; cd->name != NULL; cd++) {
			if (stricmp(cd->name, name) == 0) break;
		}
		if (cd->name == NULL) {
			fprintf(stderr, "Unrecognized <name> in termkey: %s\n", name);
			continue;
		}
#ifdef DEBUG		
		printf("Name: %-6s  Value: %-10s  Code: %d\n", name,value,cd->code);
		fflush(stdout);
#endif
		InsertKeyString(entry, value, cd->code);
	}

	free(keyline);												/* Deallocate my space */
	return(1);
}

/* ===========================================================================
-- Routine to read stdin and return appropriate code from possible strings
--
-- Usage:	int Kgetkey(void *entry);
--
-- Inputs:	entry - structure returned by successful Kgetent()
--
-- Returns:	code for keys.  Errors are handled in various strange ways.
--
-- Notes:	Partial strings (out of time) for specific codes will return 0
--				Strings containing bad chars will return the offending char
=========================================================================== */
int Kgetkey(void *entry) {

	BRANCH *key;
	int achr;

#if defined GNU_C || defined AIX_C
	tcdrain(fileno(stdout));					/* Make sure buffers cleared	*/
	tcdrain(fileno(stderr));					/* Before looking at keyboard	*/
#endif

	achr = CONgetcRaw();							/* Get first character	*/
#ifdef DEBUG
   printf("First character:     %.2x\n",achr);
	if (achr < 0) exit(1);
#endif
	if (entry == NULL) return(achr);			/* Default behavior		*/

	key = ((KEYTABLE *)entry)->Table;		/* Table start				*/
	do {
		if (key->key == achr) {
			if (key->nest == NULL) return(key->code);
#ifdef DEBUG
   printf("Looking for more ..");
#endif
			achr = CONwaitchr(SEQTIMELIMIT);
			if (achr == -1) return(key->code);
#ifdef DEBUG
   printf("Following character: %.2x\n",achr);
#endif
			key = key->nest;
		} else {
			key = key->next;						/* Look up the next one */
		}
	} while (key != NULL);
	return(achr);
}


/* ===========================================================================
-- Routine to print out one branch of internal structure.
--
-- Usage:	int PrintBranch(BRANCH *key, int indent);
--
-- Inputs:	key    - Starting branch entry of one level
--				indent - Number of spaces to indent this level
--
-- Returns:	Outputs to terminal.  Recursively calls itself if any
--				entry includes a nested level.
=========================================================================== */
static void PrintBranch(BRANCH *key, int indent) {

	int i,achr;

	do {
		for (i=0; i<indent; i++) putc(' ',stdout);
		achr = key->key;
		if (achr == 0x7F)  printf("key: <del>");
		else if (achr == 0x1b)  printf("key: <esc>");
		else if (achr == '\a')  printf("key: \\a   ");
		else if (achr == '\b')  printf("key: \\b   ");
		else if (achr == '\f')  printf("key: \\f   ");
		else if (achr == '\n')  printf("key: \\n   ");
		else if (achr == '\r')  printf("key: \\r   ");
		else if (achr == '\t')  printf("key: \\t   ");
		else if (achr == '\v')  printf("key: \\v   ");
		else if (iscntrl(achr)) printf("key: ^%c  ", achr+'A'-1);
		else						  printf("key:  %c  ", achr);
		printf("  code: %3i  me: %p  next: %p  nest: %p\n", key->code, key, key->next, key->nest);
		if (key->nest != NULL) PrintBranch(key->nest, indent+2);
	} while ( (key = key->next) != NULL);

	return;
}

/* ===========================================================================
-- Routine to print out the internal structure.
--
-- Usage:	int Kprintdata(void *entry);
--
-- Inputs:	entry - structure returned by successful Kgetent()
--
-- Returns:	prints to terminal all internal data following trees to completion.
=========================================================================== */
void	Kprintdata(void *entry) {
	
  	KEYTABLE *keytable;
	int i;

	keytable = (KEYTABLE *) entry;

	printf("setup: ");
	for (i=0; i<80 && keytable->setup[i]!='\0'; i++) 
	  printf("<%d>",keytable->setup[i]);
	putc('\n', stdout);
	
	printf("exit:  ");
	for (i=0; i<80 && keytable->exit[i]!='\0'; i++) 
	  printf("<%d>", keytable->exit[i]);
	putc('\n', stdout);

	PrintBranch(keytable->Table, 0);
	return;
}

/* ************************************************************************* */

#ifdef TEST

int main(int argc, char *argv[]) {

	int achr, rcode;
	void *entry;
	CODES *cd;

	if (argc <= 1) {
	  printf("Printing key scan codes from keyboard\n");
	  printf("Use format <program> <term> to get test mode\n");
 	  CONInitialize();
	  do {
		  achr = CONgetcRaw();
		  printf("<%3d> <%c>", achr, achr);
	  } while (achr != 04);
	  return(EXIT_SUCCESS);
	}

	rcode = Kgetent(argv[1], &entry);
	if (rcode == 0) {
		printf("Terminal specified was not found: %s\n", argv[1]);
		return(EXIT_FAILURE);
	} else if (rcode == -1) {
		printf("Unable to find the termkey file\n");
		return(EXIT_FAILURE);
	} else {
		Kprintdata(entry);
	}
/*	Kfree(&entry); */

	CONInitialize();

	do {
		achr = Kgetkey(entry);
		for (cd=keycodes; cd->name != NULL; cd++) if (cd->code == achr) break;
		if (cd->name != NULL) {
			printf("%s\n", cd->name);
		} else {
			printf("<%i>\n", achr);
		}
	} while (achr != 4);						/* ^D terminates reading */

	return(EXIT_SUCCESS);
}

#if (defined CSET2 || defined WATCOM || defined MSC60 || defined MSC70)
int CONwaitchr(int ms) {				/* Just so it compiles */
	return(-1);
}
#endif

#endif		/* TEST */
