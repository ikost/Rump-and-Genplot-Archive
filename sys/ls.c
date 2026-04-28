/*  UNIX replacement of "ls" and "ll" */

#if (defined OS2 || defined NT)

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#if (defined CSET2 || defined GNU_C)
	#define INCL_DOSFILEMGR
	#include <os2.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <dirent.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#ifndef TRUE
	#define TRUE  1
#endif
#ifndef FALSE
	#define FALSE 0
#endif

#define DIRCHR 0x7E					/* Special characters */
#define HIDCHR 0x06
#define SYSCHR 0x2A
#define RDCHR  0x11

#define	WILD_MAX	128				/* Max # chars in wild name pattern		*/

#if (! defined _MSC_VER || _MSC_VER < 1400)
	typedef int ptrdiff_t;
#endif

typedef struct _MYFILEBUF {				/* Almost the struct dirent */
   ptrdiff_t  d_name;						/* name (use strlen())  (POSIX)  */
   int		  d_namlen;						/* valid length d_name  (OS/2)   */
	int		  d_attrib;						/* DOS file attribute   (OS/2)   */
	mode_t	  d_mode;						/* File mode            (OS/2)   */
	UINT64	  d_size;						/* File size in bytes   (OS/2)   */
	UINT64	  d_alloc;						/* Allocated bytes      (OS/2)   */
	time_t	  d_atime,d_mtime,d_ctime; /* access,modify,create (OS/2)   */
} MYFILEBUF;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
PRIVATE  int check_pause(int *num_lines);
PRIVATE  int ll_print(void);
PRIVATE  int ls_print(int num_file, int num_dir);
PRIVATE  int lf_print(int num_file, int num_dir);
PRIVATE  int single_col_print(int num_file, int num_dir);
PRIVATE  int compare (MYFILEBUF *arg1, MYFILEBUF *arg2);
PRIVATE	int out_line(void);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
/* Patterns from keyboard input */
	PRIVATE	char SearchSpec[PATH_MAX];

/* Bit patterns for various operating modes */
	#define	FL_REVERSE_SORT		0x0001	/* Perform all sorts in reverse	*/
	#define	FL_APPEND_TYPE			0x0002	/* Append / to dirs, * to exes	*/
	#define	FL_NO_DIR_DIVE			0x0004	/* Don't dive into dir initially	*/
	#define	FL_SHOW_DIR_DOTS		0x0008	/* Show . and .. entries			*/
	#define	FL_SHOW_DOTS			0x0010	/* Show files beginning with .	*/
	#define	FL_USE_ACCESS_DATE	0x0020	/* Use access instead of modify	*/
	#define	FL_USE_SINGLE_COL		0x0040	/* Output as single column			*/
	#define	FL_PAUSE_ON_LIST		0x0080	/* Pause during output				*/
	#define	FL_RECURSIVE_LIST		0x0100	/* Do recursive listing				*/
	#define	FL_IGNORE_CASE			0x0200	/* Do case ignorant comparison	*/
	#define	FL_NO_DIRS				0x0400	/* Don't show any directories		*/

  	PRIVATE int flags;

/* Operating modes */
	PRIVATE	enum MODES {LS_MODE,LL_MODE,LF_MODE,SINGLE_COL_MODE} output_mode;

/* Sorting algorithm -- default (and minor key) is always BY_NAME */
	PRIVATE	enum SORTS {BY_NAME,BY_DATE,BY_SIZE,BY_EXT} sort_type;

/* Initial defaults to search for */

/* Structures holding total number of files and output space */
	PRIVATE	int  num_lines;					/* Number of lines output		*/
	PRIVATE	int	num_entries;				/* Number of entries				*/
	PRIVATE	MYFILEBUF *files;					/* Directory of information   */
	PRIVATE	int  NameWidth;					/* Width of largest name		*/
	PRIVATE	char output[PATH_MAX];
	PRIVATE	char *lineptr;

	PRIVATE  char *NamePtr, *Names;
	PRIVATE	unsigned int NamePtrSize;

/* ---------------------------------------------------------------------------
-- Main Procedure
---------------------------------------------------------------------------- */
BOOL SysListDir(CHAR *pattern, CHAR *option) {

	char	*aptr, opt[30], TestName[256];

	int	i;
	int	rcode;

	char	SearchDir [PATH_MAX]="\0";				/* Blank directory	*/
	char	SearchName[WILD_MAX]="*";				/* Match everything	*/
	
	DIR	*dirp;										/* Directory unit */
	struct dirent *entry;							/* And an entry	*/
	
	int	num_file,num_dir,num_alloc;			/* Number total, files, dirs */

/* Set default operating parameters */
  	flags       = FL_IGNORE_CASE;
	sort_type	= BY_NAME;
	output_mode = LS_MODE;

/* Copy search criteria to SearchSpec */
	if (pattern == NULL || *pattern == '\0') pattern = "*";

/* Determine the options and parse now */
	if (option != NULL) {
		while (*option) {									/* While more args		*/
			while (isspace(*option) || *option=='-' || *option=='/') option++;
			if (! *option) break;
			aptr = opt;
			while (*option && ! isspace(*option)) *aptr++ = *option++;
			*aptr = '\0'; aptr = opt;

			if ( LexEqual(aptr,"date",4) || LexEqual(aptr,"sortdate",5) ||
				  LexEqual(aptr,"time",4) || LexEqual(aptr,"sorttime",5) ) {
				sort_type = BY_DATE;
			} else if (LexEqual(aptr,"size",4)  || LexEqual(aptr,"sortsize",5) ) {
				sort_type = BY_SIZE;
			} else if (LexEqual(aptr,"name",4)  || LexEqual(aptr,"sortname",5) ) {
				sort_type = BY_NAME;
			} else if (LexEqual(aptr,"extension",3) || LexEqual(aptr,"sortext",5) ) {
				sort_type = BY_EXT;
			} else if (LexEqual(aptr,"reverse",3)) {
				flags |= FL_REVERSE_SORT;
			} else if (LexEqual(aptr,"nodir",3)) {
				flags |= FL_NO_DIRS;
			} else if (LexEqual(aptr,"ll",2)) {
				output_mode = LL_MODE;
			} else if (LexEqual(aptr,"lf",2)) {
				output_mode = LF_MODE;
			} else if (LexEqual(aptr,"ls",2)) {
				output_mode = LS_MODE;
			} else if (LexEqual(aptr,"pause",5)) {
				flags |= FL_PAUSE_ON_LIST;
			} else {
				while (*aptr) {
					switch (*(aptr++)) {
						case 'x':			/* Preserve case on comparisons */
							flags &= ~FL_IGNORE_CASE;								break;
						case 'l':			/* long format mode, links, owner, etc	*/
							output_mode = LL_MODE;									break;
						case 'F':			/* cause dirs to get "/", exes '*'		*/
							flags |= FL_APPEND_TYPE;								break;
						case 'd':			/* list only name even if directory		*/
							flags |= FL_NO_DIR_DIVE;								break;
						case 'a':			/* all entries (including . and ..)		*/
							flags |= FL_SHOW_DIR_DOTS | FL_SHOW_DOTS;			break;
						case 'A':			/* all entries except . and ..			*/
							flags |= FL_SHOW_DOTS;									break;
						case 'u':			/* use last access in sort or ls -l		*/
							flags |= FL_USE_ACCESS_DATE;							break;
						case '1':			/* one entry per line (as w/ redirect)	*/
							output_mode = SINGLE_COL_MODE;
							flags |= FL_USE_SINGLE_COL;							break;
						case 'C':			/* multicolumn (as in ls | lpr)			*/
							output_mode = LS_MODE;
							flags &= ~FL_USE_SINGLE_COL;							break;
						case 'r':			/* Reverse the sense of the sort order	*/
							flags |= FL_REVERSE_SORT;								break;
						case 't':			/* sort by time modified (latest first) */
							sort_type = BY_DATE;											break;
						case 'R':			/* Recursive list								*/
							flags |= FL_RECURSIVE_LIST;								break;
						case 'c':			/* sort by last modified date				*/
						case 'i':			/* not relevent to OS/2						*/
						case 'q':			/* not relevent to OS/2						*/
						case 'f':			/* not relevent to OS/2						*/
						case 'g':			/* not relevent to OS/2						*/
						case 'h':			/* not relevent to OS/2						*/
						case 's':			/* # kb of physical space for file		*/
						case 'H':			/* not relevent to OS/2						*/
						case 'L':			/* not relevent to OS/2						*/
							break;
						default:
							ERRprintf("WARNING: Unknown ls option (%s) ignored\n", aptr);
							break;
					}
				}
			}
		}
	}

/* Copy search criteria to SearchSpec - eliminating any trailing / or \
 * if it follows an alphanumeric ( ls abc/ ==> ls abc, but ls c:\ does
 * not become ls c: */
	strcpy(SearchSpec, pattern);
	if (strlen(SearchSpec) >= 2) {
		aptr = SearchSpec + strlen(SearchSpec) - 1;
		if ((*aptr == '/' || *aptr == '\\') && isalnum(*(aptr-1))) *aptr = '\0';
	}

/* ---------------------------------------------------------------------------
;   Check the wildcard pathname now.  If a directory or disk, add the
;   appropriate request to search for all files
;-------------------------------------------------------------------------- */
	i = SysFileCheck(SearchSpec, SearchDir, SearchName);
	if (i == -1) {											/* Some sort of fuck-up */
		ERRprintf("Illegal directory specified (%s)\n", SearchSpec);
		return(-1);
	} else if (i == 1) {
		strcpy(SearchName, "*");						/* Single existing directory */
	}

	SysMakePath(SearchSpec, SearchDir, SearchName, NULL);

	if (flags & FL_IGNORE_CASE && i != 1) {		/* Ignore case on all?	*/
		strlwr(SearchSpec); 
		strlwr(SearchDir); 
		strlwr(SearchName);
	}

/* ---------------------------------------------------------------------------
;  Allocate space as we need it.  Load entire directory first and then sort.
;
;  Go through the directory and count number of files and directories which
;  match the specified criteria.  Load into memory on a first pass.  The
;  files "." and ".." are always skipped in the count.
;-------------------------------------------------------------------------- */
/* No names in the list yet, nor in stack */
	Names = NamePtr = NULL;							/* None allocated */
	NamePtrSize = 0;									/* Size is zero */
	files = NULL;										/* No files allocated */
	num_alloc = 0;										/* Size is zero */
	num_entries = num_dir = 0;						/* And none recorded yet */

	if ( (dirp = opendir(SearchDir)) == NULL) {
		ERRprintf("ERROR: Unable to open directory: %s\n", SearchDir); 
		return(2);
	}

	while ( (entry = readdir(dirp)) != NULL) {

#if (defined CSET2)
		if ( *entry->d_name == '.' || (entry->d_attrib & (FILE_SYSTEM | FILE_HIDDEN)) ) {
#elif defined MSC70
		if ( *entry->d_name == '.' || (entry->d_attrib & (_A_SYSTEM | _A_HIDDEN)) ) {
#else
		if ( *entry->d_name == '.' || (entry->d_attr & (FILE_SYSTEM | FILE_HIDDEN)) ) {
#endif
			if (! (flags & FL_SHOW_DOTS)) continue;	/* Ignore UNIX hidden	*/
			if (! (flags & FL_SHOW_DIR_DOTS)) {			/* How about . and ..?	*/
				if (strcmp(entry->d_name,".")  == 0) continue;
				if (strcmp(entry->d_name,"..") == 0) continue;
			}
		}
#if (defined CSET2 || defined MSC70)
		if ( (flags & FL_NO_DIRS) && S_ISDIR(entry->d_mode) ) continue;
#endif
		
		/* See if need space to hold this name */
		if ((NamePtr-Names)+strlen(entry->d_name)+1 >= NamePtrSize) {
			ptrdiff_t ioff;
			NamePtrSize += 16384;									/* 1000 typical files */
			ioff = NamePtr - Names;
			Names = realloc(Names, NamePtrSize*sizeof(*Names));
			NamePtr = Names + ioff;
		}
		strcpy(NamePtr, entry->d_name);

		strncpy(TestName, NamePtr, sizeof(TestName));
		if (flags & FL_IGNORE_CASE) strlwr(TestName);
		if (! SysCheckMatch(TestName, SearchName)) continue;

		/* See if need space to hold this entry */
		if (num_entries >= num_alloc) {							/* Need more space */
			num_alloc += 50;
			files = realloc(files, num_alloc*sizeof(*files));
		}

#if (defined CSET2 || defined MSC70)
		files[num_entries].d_name     = NamePtr - Names;
		files[num_entries].d_namlen	= entry->d_namlen;							/* Rest of info */
		files[num_entries].d_attrib	= entry->d_attrib;
		files[num_entries].d_mode		= entry->d_mode;
		files[num_entries].d_size		= entry->d_size;
		files[num_entries].d_alloc		= entry->d_alloc;
		files[num_entries].d_atime		= entry->d_atime;
		files[num_entries].d_mtime		= entry->d_mtime;
		files[num_entries].d_ctime		= entry->d_ctime;
#else
		files[num_entries].d_name     = NamePtr - Names;
		files[num_entries].d_namlen	= strlen(entry->d_name);					/* Rest of info */
		files[num_entries].d_attrib	= entry->d_attr;
		files[num_entries].d_mode		= 0;
		files[num_entries].d_size		= entry->d_size;
		files[num_entries].d_alloc		= entry->d_size;
		files[num_entries].d_atime		= 0;
		files[num_entries].d_mtime		= 0;
		files[num_entries].d_ctime		= 0;
#endif
		
#if (defined CSET2 || defined MSC70)
		if (S_ISDIR(entry->d_mode) ) {
			num_dir++;
			if (flags & FL_APPEND_TYPE) {
				strcat(NamePtr, "/");
				files[num_entries].d_namlen++;
			}
		}
#endif
		NamePtr += strlen(NamePtr)+1;					/* Point past name now */
		num_entries++;										/* This puppy is done! */

	}
	closedir(dirp);
	num_file = num_entries-num_dir;

/* ---------------------------------------------------------------------------
;  If not matching files or directories, print appropriate message and exit
;-------------------------------------------------------------------------- */
	if (num_entries == 0) {
		ScrSetAttrib(D_BOLD);	TTYprintf("\nDirectory: %s\n\n<No entries selected>\n\n",SearchSpec);	ScrSetAttrib(D_NORMAL);
		if (Names != NULL) free(Names);
		if (files != NULL) free(files);
		return(1);
	}

/* ---------------------------------------------------------------------------
;  Sort on keys  (1) FILE/DIRECTORY  (2) User specified  (3) Filename 
;  And determine maximum width of the names
;-------------------------------------------------------------------------- */
	qsort((void *) files, num_entries, sizeof(*files), (int (*)(const void *, const void *) ) compare);
	NameWidth = 2;
	for (i=0; i<num_entries; i++) NameWidth = max(NameWidth, files[i].d_namlen);

/* ---------------------------------------------------------------------------
;  Output the list 
;-------------------------------------------------------------------------- */
	num_lines = 0;										/* No lines output */
	switch (output_mode) {
		case LL_MODE:
			rcode = ll_print();
			break;
		case LF_MODE:
			rcode = lf_print(num_file, num_dir);
			break;
		case LS_MODE:
			rcode = ls_print(num_file, num_dir);
			break;
		case SINGLE_COL_MODE:
			rcode = single_col_print(num_file, num_dir);
			break;
	}
	if (Names != NULL) free(Names);
	if (files != NULL) free(files);
	TTYputc('\n');
	return(rcode);
}


/* ---------------------------------------------------------------------------
-- Comparison routine to determine "larger" of the filenames.
---------------------------------------------------------------------------- */
int compare (MYFILEBUF *arg1, MYFILEBUF *arg2) {

	int rcode, rc_name;
	UINT64 ut1,ut2;

	char dir[PATH_MAX];				/* Directory of fname */
	char fname_1[PATH_MAX];			/* Filename 1         */
	char ext_1[PATH_MAX];			/* Extension 1		  */
	char fname_2[PATH_MAX];			/* Filename 2         */
	char ext_2[PATH_MAX];			/* Extension 2		  */
	
	if (S_ISDIR(arg2->d_mode) && ! S_ISDIR(arg1->d_mode)) return(-1);
	if (S_ISDIR(arg1->d_mode) && ! S_ISDIR(arg2->d_mode)) return( 1);

	SysSplitPath(Names+arg1->d_name, dir,fname_1,ext_1);
	SysSplitPath(Names+arg2->d_name, dir,fname_2,ext_2);

	if (output_mode == LF_MODE) {		/* Special case on LF mode search */
		rcode = stricmp(ext_1,ext_2);	/* Return search on extension     */
		if (rcode) return(rcode);
	}

	rc_name = (flags & FL_IGNORE_CASE) ? stricmp(Names+arg1->d_name,Names+arg2->d_name) : strcmp(Names+arg1->d_name,Names+arg2->d_name);
	switch (sort_type) {
		case BY_EXT:
			rcode = stricmp(ext_1,ext_2);
			break;

		case BY_NAME:
			rcode = rc_name;
			break;

		case BY_DATE:
			if (flags & FL_USE_ACCESS_DATE) {
				ut1 = (unsigned long) arg1->d_atime;
				ut2 = (unsigned long) arg2->d_atime;
			} else {
				ut1 = (unsigned long) arg1->d_mtime;
				ut2 = (unsigned long) arg2->d_mtime;
			}
			rcode = (ut1 == ut2) ? rc_name : (ut1 > ut2) ? +1 : -1 ;
			break;

		case BY_SIZE:
			ut1 = arg1->d_size;
			ut2 = arg2->d_size;
			rcode = (ut1 == ut2) ? rc_name : (ut1 > ut2) ? +1 : -1 ;
			break;

		default:
			rcode = rc_name;
	}

/* Possibly reverse the sense, and then return */
	if (flags & FL_REVERSE_SORT) rcode = -rcode;
	return rcode;
}

/* ----------------------------------------------------------------------------
-- Usage:  int ll_print(void)
--
--	Inputs: none
--
--	Output: outputs to stdout ls format listing
---------------------------------------------------------------------------- */
int ll_print(void) {

	int i,attrib;
	char *format_file, *format_dir;
	char tok10[10];
	static char *MONTHS[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	struct tm *today;

	ScrSetAttrib(D_BOLD);	TTYprintf("\nDirectory: %s\n\n",SearchSpec);	ScrSetAttrib(D_NORMAL);
	num_lines += 2;

	if (sizeof(UINT64) == 8) {
		format_file = "%s  %15I64u    %3s %2d %4d   %2d:%2.2d:%2.2d  %s\n";
		format_dir  = "%s      <directory>    %3s %2d %4d   %2d:%2.2d:%2.2d  %s\n";
	} else {
		format_file = "%s %11lu    %3s %2d %4d   %2d:%2.2d:%2.2d  %s\n";
		format_dir  = "%s <directory>    %3s %2d %4d   %2d:%2.2d:%2.2d  %s\n";
	}

	for (i=0;i<num_entries;i++) {
		strcpy(tok10,". . . . .");
		if (S_ISDIR(files[i].d_mode)) break;				/* End of files */

		attrib = files[i].d_attrib;							/* And the attribute */
#if defined MSC70
		if (attrib & _A_ARCH  )		tok10[0] = 'A';
		if (attrib & _A_SYSTEM)		tok10[2] = 'S';
		if (attrib & _A_RDONLY)		tok10[4] = 'R';
		if (attrib & _A_HIDDEN)		tok10[6] = 'H';
#else
		if (attrib & FILE_ARCHIVED) tok10[0] = 'A';
		if (attrib & FILE_SYSTEM  ) tok10[2] = 'S';
		if (attrib & FILE_READONLY) tok10[4] = 'R';
		if (attrib & FILE_HIDDEN  ) tok10[6] = 'H';
#endif

		today = localtime(&files[i].d_mtime);

		if (check_pause(&num_lines)) return(0); num_lines++;
		TTYprintf(format_file, tok10, files[i].d_size,
						 MONTHS[today->tm_mon],	today->tm_mday,	today->tm_year+1900,
						 today->tm_hour,	today->tm_min,		today->tm_sec,
					 Names+files[i].d_name);
	}

	for (i=i;i<num_entries;i++) {
		strcpy(tok10,". . . . .");
		attrib = files[i].d_attrib;
#if defined MSC70
		if (attrib & _A_ARCH  )		tok10[0] = 'A';
		if (attrib & _A_SYSTEM)		tok10[2] = 'S';
		if (attrib & _A_RDONLY)		tok10[4] = 'R';
		if (attrib & _A_HIDDEN)		tok10[6] = 'H';
#else
		if (attrib & FILE_ARCHIVED) tok10[0] = 'A';
		if (attrib & FILE_SYSTEM  ) tok10[2] = 'S';
		if (attrib & FILE_READONLY) tok10[4] = 'R';
		if (attrib & FILE_HIDDEN  ) tok10[6] = 'H';
#endif
		today = localtime(&files[i].d_mtime);

		if (check_pause(&num_lines)) return(0);	num_lines++;
		TTYprintf(format_dir, tok10,
		MONTHS[today->tm_mon],	today->tm_mday,	today->tm_year+1900,
				 today->tm_hour,	today->tm_min,		today->tm_sec,
				 Names+files[i].d_name);
	}
	return(0);
}


/* ---------------------------------------------------------------------------
--   Usage:  int single_col_print(int num_file, int num_dir)
--
--	Inputs: num_file - number of files in the directory list
--           num_dir  - number of directories in the list
--
--	Output: outputs to stdout in single column format
---------------------------------------------------------------------------- */
PRIVATE int single_col_print(int num_file, int num_dir) {

	int i;

	ScrSetAttrib(D_BOLD);	TTYprintf("\nDirectory: %s\n",SearchSpec);	ScrSetAttrib(D_NORMAL);
	num_lines += 1;

	memset((lineptr=output),' ',sizeof(output));		/* Prefill output line blank */
	output[sizeof(output)-1] = '\0';						/* And NULL terminate */

	for (i=0;i<num_entries;i++) {
		memcpy(output, Names+files[i].d_name, files[i].d_namlen);
		lineptr = output+files[i].d_namlen+1;
		if (out_line()) return 0;
	}

	return(0);
}


/* ---------------------------------------------------------------------------
--   Usage:  int ls_print(int num_file, int num_dir)
--
--	Inputs: num_file - number of files in the directory list
--           num_dir  - number of directories in the list
--
--	Output: outputs to stdout ls format listing
---------------------------------------------------------------------------- */
PRIVATE int ls_print(int num_file, int num_dir) {

	int i,attrib;
	int width;

	ScrSetAttrib(D_BOLD);	TTYprintf("\nDirectory: %s\n",SearchSpec);	ScrSetAttrib(D_NORMAL);
	num_lines += 1;

	memset((lineptr=output),' ',sizeof(output));		/* Prefill output line blank */
	output[sizeof(output)-1] = '\0';						/* And NULL terminate */

	i=0;								/* Used counter */
	if (NameWidth > 78) {
		width = 80;
	} else {
		width = 80/(80/(NameWidth+2));		/* Width of each name field */
	}
	if (num_file != 0) {
		ScrSetAttrib(D_BOLD);	TTYputs("\n Files:\n");		ScrSetAttrib(D_NORMAL);
		num_lines += 2;

		for (;i<num_entries;i++) {
			if (S_ISDIR(files[i].d_mode)) break;		/* End of files */
			attrib = files[i].d_attrib;
#if defined MSC70
			if (attrib & _A_RDONLY  )  *lineptr = RDCHR;
			if (attrib & _A_HIDDEN  )  *lineptr = RDCHR;
			if (attrib & _A_SYSTEM  )  *lineptr = RDCHR;
#else			
			if (attrib & FILE_READONLY)  *lineptr = RDCHR;
			if (attrib & FILE_HIDDEN  )  *lineptr = RDCHR;
			if (attrib & FILE_SYSTEM  )  *lineptr = RDCHR;
#endif
			lineptr++;
			memcpy(lineptr, Names+files[i].d_name, files[i].d_namlen);
			if (files[i].d_namlen > width-2) {			/* Very long name - go ahead and allow */
				lineptr += files[i].d_namlen;
			} else {
				lineptr += (width-1);						/* Skip to next spot */
			}
			if (lineptr-output+NameWidth > 80) { if (out_line()) return(0); }
		}
		if (out_line()) return(0);
	}

	if (num_dir != 0) {
		ScrSetAttrib(D_BOLD);	TTYputs("\n Directories:\n");	ScrSetAttrib(D_NORMAL);
		num_lines += 1;

		for (;i<num_entries;i++) {
			*(lineptr++) = DIRCHR;
			memcpy(lineptr, Names+files[i].d_name, files[i].d_namlen);
			if (files[i].d_namlen > width-2) {			/* Very long name - go ahead and allow */
				lineptr += files[i].d_namlen;
			} else {
				lineptr += (width-1);						/* Skip to next spot */
			}
			if (lineptr-output+NameWidth > 80) { if (out_line()) return(0); }
		}
		if (out_line()) return(0);
	}
	return(0);
}


/* ---------------------------------------------------------------------------
--   Usage:  int lf_print(int num_file, int num_dir)
--
--	Inputs: num_file - number of files in the directory list
--           num_dir  - number of directories in the list
--
--	Output: outputs to stdout ls format listing
--
---------------------------------------------------------------------------- */
PRIVATE int lf_print(int num_file, int num_dir) {

	int i;

	char dir[PATH_MAX];					/* Directory of fname */
	char fname[PATH_MAX];				/* Filename 1         */
	char ext[PATH_MAX];					/* Extension 1		  */
	char current_ext[PATH_MAX];		/* Starting extension */

	ScrSetAttrib(D_BOLD);	TTYprintf("\nDirectory: %s\n",SearchSpec);	ScrSetAttrib(D_NORMAL);
	num_lines += 1;
	memset((lineptr=output),' ',sizeof(output));		/* Prefill output line blank */
	output[sizeof(output)-1] = '\0';						/* And NULL terminate */

	i=0;								/* Used counter */
	if (num_file != 0) {
		ScrSetAttrib(D_BOLD);	TTYputs("\n Files:\n");	ScrSetAttrib(D_NORMAL);
		num_lines += 2;
		strcpy(current_ext,"zzz");		/* No starting extension */

		for (;i<num_entries;i++) {
			if (S_ISDIR(files[i].d_mode)) break;	/* End of files */
			SysSplitPath(Names+files[i].d_name, dir,fname,ext);
			if (strcmp(ext,current_ext)) {
				if (out_line()) return(0);
				strcpy(current_ext,ext);
				memcpy(output,".    files:",11);
				memcpy(lineptr,ext,strlen(ext));
				lineptr = output+16;
			}
			memcpy(lineptr,fname,strlen(fname));
			lineptr = lineptr+10;				/* Next spot! */
			if (lineptr-output > 70) {			/* Time to output? */
				if (out_line()) return(0); 
				lineptr = output+16; }			/* More are blank! */
		}
		if (out_line()) return(0);
	}

	if (num_dir != 0) {
		ScrSetAttrib(D_BOLD);	TTYputs("\n Directories:\n");	ScrSetAttrib(D_NORMAL);
		num_lines += 1;
		strcpy(current_ext,"zzz");		/* No starting extension */

		for (;i<num_entries;i++) {
			SysSplitPath(Names+files[i].d_name, dir,fname,ext);
			if (strcmp(ext,current_ext)) {
				if (out_line()) return(0);
				strcpy(current_ext,ext);
				memcpy(output,".    dirs:",10);
				memcpy(lineptr,ext,strlen(ext));
				lineptr = output+16;
			}
			memcpy(lineptr,fname,strlen(fname));
			lineptr = lineptr+10;				/* Next spot! */
			if (lineptr-output > 70) {			/* Time to output? */
				if (out_line()) return(0); 
				lineptr = output+16; }			/* More are blank! */
		}
		if (out_line()) return(0);
	}
	return(0);
}


/* ---------------------------------------------------------------------------
--  Usage: int out_line(void)
--
--	Inputs: none
--
--	Output: 1 ==> request to quit from "MORE"
--		   0 ==> output ok
---------------------------------------------------------------------------- */
PRIVATE int out_line(void) {

	if (lineptr != output) {
		while (*(--lineptr) == ' ') {
			if (lineptr == output) return(0);
		}
		strcpy(++lineptr,"\n");							/* Adds the necessary NULL */
		if (check_pause(&num_lines)) return(1);
		num_lines++;
		TTYputs(output);
		memset((lineptr=output),' ',sizeof(output));		/* Prefill output line blank */
		output[sizeof(output)-1] = '\0';						/* And NULL terminate */
	}
	return(0);
}
				
/* ---------------------------------------------------------------------------
-- Usage:  Routine to pause and wait if necessary
--
--	Inputs: 
--
--	Output: 
---------------------------------------------------------------------------- */
PRIVATE int check_pause(int *num_lines) {

#define NUM_LINES 24

	int ichr;
	int rcode=0;
	
	if ( (flags&FL_PAUSE_ON_LIST) == 0  || *num_lines < NUM_LINES) return(0);

	CONputs(" --- MORE --- ");
	ichr = CONgetc();								/* Wait for a char  */
	CONputs("\r               \r");			/* Clear the "more" */

	if (ichr == '\r' || ichr == '\n')		/* What does key imply? */
		*num_lines = NUM_LINES-1;
	else if (ichr == ' ')
		*num_lines = 1;
	else if (ichr == 'q' || ichr == 'Q')
		rcode = 1;
	else
		*num_lines = 10;								/* Reset # of lines */

	return(rcode);
}

#endif
