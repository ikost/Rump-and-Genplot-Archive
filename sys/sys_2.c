/* ANSI C / POSIX extensions */

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
#include <limits.h>
#include <fcntl.h>
#include <ctype.h>
#include <pwd.h>
#if (defined LINUX && ! defined __USE_BSD)	/* Need prototype for ftruncate()		*/
	#define	__USE_BSD								/* Must be placed just before unistd.h */
#endif													/* And because of ENDIAN, must be last	*/
#include <unistd.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"

/* ---------------------------------------------------------------------------
-- The nint function returns the nearest integer to a given double real value.
-- Properly handles negative and positive values.  Can also be implemented as
-- a macro rather than a function.
--
-- Usage:   int nint(double val);
--
--	Inputs:  val  - real number to be tested
--
--	Returns: Nearest integer.  [0.5,1.5)   -->  1.0  (and obvious extension)
--                            (-1.5,-0.5] --> -1.0  (and obvious extension)
--
-- Macro definition:  
--    #define nint(x)  ( ((x) > 0) ? (int)((x)+0.5f) : (int)((-(x))+0.5f) )
--
-- NOTE: (1) The alternate version is still available because of the BUG in 
--           the CSET compiler with -G4 -O+ optimization enabled.  Hopefully 
--           they will eventually fix this problem.
--       (2) nint is in SunOS libm.a in the same module as other math routines
--           used by genplot.  That makes the version of nint here a
--           'multiple definition'.  The libm.a version seems to produce
--           identical results. -rcc
---------------------------------------------------------------------------- */
#ifndef GNU_EXTENSIONS
#if (defined CSET2 && defined PROBLEM_486)
int nint(double val) {
	volatile double val2;
	if (val < 0) return(-(int) (-val+0.5));
	val2 = val+0.5; 
	return((int) val2);
}
#else
int nint(double val) {
	return((val >= 0) ? (int)(val+0.5) : -((int)(-val+0.5)));
}
#endif
#endif


/* ---------------------------------------------------------------------------
-- The fprivate() function marks the specified stream (and underlying) file to
-- be unique to the current process and not to be inherited across forks,
-- spawn, exec or popen events.
--
-- Syntax: int fprivate(FILE *stream);
--
-- Inputs: stream - valid stream pointer
--
-- Returns: 0 - if successful, otherwise -1 and errno set appropriately
--------------------------------------------------------------------------- */
int fprivate(FILE *stream) {
	int flags, fd;

	fd = fileno(stream);
	if ( (flags = fcntl(fd, F_GETFD)) == -1) return(-1);	/* Get current flags */
	flags |= FD_CLOEXEC;												/* Set close on exec	*/
	if ( fcntl(fd, F_SETFD, flags) == -1) return(-1);
	return(0);
}

/* ---------------------------------------------------------------------------
-- The ftrunc() function truncates the specified stream at a desired position.
-- This action on a file opened for writing causes the stream to immediately
-- become the specified length. Stream may be read from or written to after
-- this operation
--
-- Usage:  int = ftrunc(FILE *stream, long posn);
--
-- Inputs: stream - valid stream descriptor
--         posn   - length in bytes (via FSEEK) where truncate desired.  If
--                  posn is -1, will be truncated at current position.
--
-- Returns: 0 if successful
--          -1 if not, with errno set appropriately
--------------------------------------------------------------------------- */
int ftrunc(FILE *stream, long posn) {

	if (fflush(stream) != 0) return(-1);
	if (posn==-1L && (posn=ftell(stream))==-1L) return(-1);
	return(ftruncate(fileno(stream), posn));		/* And truncate the sucker */
}


/* ===========================================================================
-- Routine to compare two strings for lexical match over a specified minimum
-- length.  Leading blanks and the case of the characters are ignored.
-- However, all given characters must match corresponding characters in other
-- NULL terminated string.  Trailing blanks on the token are significant.
--
-- Usage: int LexEqual(char *tok1, char *tok2, int minlen)
--
-- Inputs: tok1 - One string to compare
--         tok2 - Comparison string
--         minlen - Minimum number of characters which must match
--                  If negative, use absolute value or size of token 2,
--                  whichever is smaller.
--
-- Output: 2 ==> Strings are identical within case
--         1 ==> Strings are identical over at least minlen chars
--         0 ==> Either strings are different, or fewer than minlen
--               chars in one of the strings
=========================================================================== */
INTEGER LexEqual(const char *tok1, const char *tok2, INTEGER minlen) {
	
	size_t len_1,len_2;
	
	while (isspace(*tok1)) tok1++;					/* Compare from non-white */
	while (isspace(*tok2)) tok2++;					/* Compare from non-white */

	len_1 = strnblen(tok1);
	len_2 = strnblen(tok2);

	if (minlen < 0) minlen = max(1, min((int) len_2, -minlen));

	if (len_1 < (size_t) minlen || len_2 < (size_t) minlen) {
		return(0);
	} else if (memicmp(tok1, tok2, (len_1<len_2) ? len_1 : len_2) != 0) {
		return(0);
	} else if (len_1 != len_2) {				/* Match all but length */
		return(1);
	} else {											/* Complete match!! */
		return(2);
	}
}

/* ===========================================================================
-- The strnblen function determines the active (non-blank trailing) length of
-- a string and returns that value.  It is essentially a strlen followed by
-- backing up over all space characters.
--
-- Usage:  size_t = strnblen(const char *string);
--
-- Inputs: string - string whose length is to be tested
--
-- Return: number of characters in string exclusive of trailing blanks
=========================================================================== */
size_t strnblen(const char *string) {
	
	const char *eptr=NULL,*aptr=string;		/* Pointer looking forward */

	while (*aptr != '\0') {
		if (! isspace(*aptr)) eptr = aptr;
		++aptr;
	}
	return( eptr == NULL ? 0 : eptr-string+1);
}

/* ===========================================================================
-- The strscpy (string safe copy) function is what strncpy should have been
-- a safe way to copy a string of indeterminate length to a known length area 
-- leaving it a string.  Problem with strncpy is that you are not guarenteed 
-- that the string will be null terminated.  If the input string is longer, 
-- then the full output string will be filled with valid characters.
--    strncpy(str, "123456789", 8);
-- leaves str with the character '1','2','3','4','5','6','7','8' -- ie. no
-- trailing '\0'. strscpy() fixes this so the equivalent call gives
--    strscpy(str, "123456789", 8);
-- leaves str with the character '1','2','3','4','5','6','7','\0'.
-- 
-- Usage:  char * = strscpy(char *s1, const char *s2, size_t len);
--
-- Inputs: s2  - incoming string
--         len - maximum length of string s1
--
-- Output: s1  - result string (copy of s2)
--
-- Return: s1
=========================================================================== */
char *strscpy(char *s1, const char *s2, size_t len) {
	if (len>1) strncpy(s1, s2, len-1);
	if (len>0) s1[len-1] = '\0';
	return(s1);
}

#if ! (defined OS2 || defined NT)

/* ===========================================================================
--   The strlwr function converts any uppercase letters in the given
--   null-terminated <string> to lowercase. The strupr function converts
--   any lowercase letters to uppercase. Other characters are not
--   affected.
--
-- Usage:  char *strlwr(char *string);
--
-- Inputs: string - string to be converted to lower case
--
-- Return: pointer to converted string
=========================================================================== */
char *strlwr(char *str) {
	register char *aptr=str;
	do {
		*aptr = (char) tolower(*aptr);
	} while (*(++aptr) != '\0');
	return(str);
}

char *strupr(char *str) {
	register char *aptr=str;
	do {
		*aptr = (char) toupper(*aptr);
	} while (*(++aptr) != '\0');
	return(str);
}


/* ===========================================================================
-- The strdup function allocates storage space (with a call to malloc) for a
-- copy of <string>, and returns a pointer to the storage space containing the
-- copied string. The function returns NULL if storage cannot be allocated.
--
-- Usage:  char *strdup( char *string );
--
-- Inputs: string - string to be duplicated
--
-- Returns: pointer if successful, or NULL if not
=========================================================================== */
char *strdup(const char *string) {
	char *ptr;
	if ( (ptr = malloc(strlen(string)+1)) != NULL) strcpy(ptr, string);
	return(ptr);
}


/* ===========================================================================
--   The stricmp function is a case-insensitive version of ANSI standard
--   strcmp.  The stricmp function compares <string1> and <string2> and returns
--   a value indicating the relationship:
--     < 0       <string1> is less than    <string2>
--     = 0       <string1> is identical to <string2>
--     > 0       <string1> is greater than <string2>
--
-- Usage:   int stricmp(char *string1, char *string2);
--
-- Inputs:  string1 - first  string to be compared
--          string2 - second string to be compared
--
-- Returns: a negative value if <string1> is less than <string2>,
--          0 if <string1> is equal to <string2>, or a positive value
--          if <string1> is greater than <string2>.
=========================================================================== */
int stricmp(const char *str1, const char *str2) {
	
	register int a,b;
	do {
		if ( (a = tolower(*str1++)) != (b = tolower(*str2++)) ) {
			if (a < b) 
				return(-1);
			else
				return(+1);
		}
	} while ( a != '\0');
	return(0);
}


/* ===========================================================================
--   The strnicmp function is a case-insensitive version of ANSI standard
--   strncmp.  The strnicmp function compares, at most, the first <count>
--   characters of <string1> and <string2> and returns:
--     < 0       <string1> is less than    <string2>
--     = 0       <string1> is identical to <string2>
--     > 0       <string1> is greater than <string2>
--
-- Usage:   int strnicmp(char *string1, char *string2, size_t count);
--
-- Inputs:  string1 - first  string to be compared
--          string2 - second string to be compared
--          count   - maximum number of characters to compare
--
-- Returns: a negative value if <string1> is less than <string2>,
--          0 if <string1> is equal to <string2>, or a positive value
--          if <string1> is greater than <string2>.
=========================================================================== */
int strnicmp(const char *str1, const char *str2, size_t count) {
	
	register int a,b;
	while (count--) {
		if ( (a = tolower(*str1++)) != (b = tolower(*str2++)) ) {
			if (a < b) 
				return(-1);
			else
				return(+1);
		}
		if ( a == '\0' ) break;
	}
	return(0);
}

/* ===========================================================================
--   The memicmp function is a case-insensitive version of ANSI standard
--   memcmp.  The memicmp function compares <buf1> and <buf2> and returns
--   a value indicating the relationship:
--     < 0       <buf1> is less than    <buf2>
--     = 0       <buf1> is identical to <buf2>
--     > 0       <buf1> is greater than <buf2>
--
-- Usage:   int memicmp(void *buf1, void *buf2, unsigned count);
--
-- Inputs:  buf1 - first  buffer area to be compared
--          buf2 - second buffer area to be compared
--
-- Returns: a negative value if <buf1> is less than <buf2>, 0 if
--          <buf1> is equal to <buf2>, or a positive value if <buf1>
--          is greater than <buf2>.
=========================================================================== */
int memicmp(const void *buf1, const void *buf2, size_t count) {

	register int a,b;
	register const char  *s, *t;

	for (s=buf1,t=buf2;count>0;count--,s++,t++) {
		a = tolower(*s);
		b = tolower(*t);
		if (a == b) continue;
		return(a < b ? -1 : 1);
		}
	return(0);
}

/* _fullpath is implemented in the C library -- */

/* ---------------------------------------------------------------------------
-- The _fullpath() routine converts a partial path stored in <file> to a
-- fully qualified path stored in <buffer>.  If length of the fully qualified
-- path would exceed length, NULL is returned.  Otherwise, address of <buffer>
-- is returned.  If <buffer> is NULL, _fullpath() insteads allocates a buffer 
-- of size PATH_MAX.  Calling routine is responsible for free() of the space 
-- when finished.  Path validity is not checked -- _fullpath only constructs
-- what would be the full pathname corresponding to the specified path.
--
-- Syntax:  char *_fullpath(char *buffer, const char *path, size_t maxlen);
--
-- Inputs:  path   - partially qualified file or pathname
--          maxlen - amount of space in buffer
--
-- Output: buffer - If !NULL and no errors, filled with fully qualified path.
--                  If NULL, _fullpath() returns pointer to allocated buffer.
--
-- Returns: pointer to buffer containing the fully qualified pathname.
--          NULL indicates an error condition.
--------------------------------------------------------------------------- */
char *_fullpath(char *buffer, const char *path, size_t maxlen) {

	char *aptr;
	int  ileft;
	
	if (buffer == NULL) {
		if ( (buffer = malloc(PATH_MAX)) == NULL) return(NULL);
		maxlen = PATH_MAX;
	}

/* (1) If there is any // or /~ tokens, they marks path restarts */
	while ( (aptr = strstr(path, "//")) != NULL) path = aptr+1;
	while ( (aptr = strstr(path, "/~")) != NULL) path = aptr+1;

/* (2) If resulting path starts with /, we have an absolute path */
	if (*path == '/') {										/* Root specified name */
		if (strlen(path) >= maxlen) return(NULL);
		strcpy(buffer, path);
		return(buffer);
	}

/* (3) Look up the preceeding directory information */
	if (*path == '~') {
		char LoginName[32];								/* Limit to 32 characters */
		struct passwd *userinfo;

		if (*(++path) == '/') {							/* Append this user's ID */
			strcpy(LoginName, getlogin());
			path++;
		} else if ( (aptr = strchr(path,'/')) != NULL) {
			*LoginName = '\0'; 
			strncat(LoginName, path, aptr-path);
			path = aptr+1;
		} else {
			strcpy(LoginName, path);
			path += strlen(path);
		}
		if ( (userinfo = getpwnam(LoginName)) == NULL) return(NULL);
		if (strlen(userinfo->pw_name) > maxlen) return(NULL);
		strcpy(buffer, userinfo->pw_name);

	} else if (getcwd(buffer, maxlen) == NULL) {
		return(NULL);
	}
	if (*path == '\0') return(buffer);

/* At this point, we have buffer filled with starting info, now walk through
   the additional path info eliminating .. and . structures */

	while (*path != '\0') {
		if (*path == '.') {					/* Handle . and .. */
			path++;
			if (*path == '.') {
				path++;
				if ( (aptr = strrchr(buffer, '/')) == NULL) return(NULL);
				*aptr = '\0';
			}
			if (*(path++) != '/') return(NULL);
		} else {
			aptr  = buffer + strlen(buffer);				/* Point to the null */
			ileft = maxlen-strlen(buffer);				/* And # available */
			*(aptr++) = '/';	ileft--;						/* File separator */
			while (*path && *path != '/') {
				if (ileft-- <= 0) return(NULL);
				*(aptr++) = *(path++);
			}
			*aptr = '\0';
			if (*path == '/') path++;
		}
	}
	return(buffer);
}

#elif (defined GNU_C && defined OS2)			/* Needs redef of _fullpath */

/* _fullpath is implemented in the C library as int rather than char * -- */

/* ---------------------------------------------------------------------------
-- The _fullpath() routine converts a partial path stored in <file> to a
-- fully qualified path stored in <buffer>.  If length of the fully qualified
-- path would exceed length, NULL is returned.  Otherwise, address of <buffer>
-- is returned.  If <buffer> is NULL, _fullpath() insteads allocates a buffer 
-- of size PATH_MAX.  Calling routine is responsible for free() of the space 
-- when finished.  Path validity is not checked -- _fullpath only constructs
-- what would be the full pathname corresponding to the specified path.
--
-- Syntax:  char *_fullpath(char *buffer, const char *path, size_t maxlen);
--
-- Inputs:  path   - partially qualified file or pathname
--          maxlen - amount of space in buffer
--
-- Output: buffer - If !NULL and no errors, filled with fully qualified path.
--                  If NULL, _fullpath() returns pointer to allocated buffer.
--
-- Returns: pointer to buffer containing the fully qualified pathname.
--          NULL indicates an error condition.
--------------------------------------------------------------------------- */
int _abspath (char *, __const__ char *, int);		/* Taken from stdlib.h */

char *_fullpath(char *buffer, const char *path, size_t maxlen) {

	char *localbuf;

	localbuf = (buffer != NULL) ? buffer : malloc(maxlen=PATH_MAX) ;
	if (_abspath(buffer, path, maxlen) != 0) {
		if (buffer == NULL) free(localbuf);
		localbuf = NULL;
	}
	return(localbuf);
}

#endif		/* (defined OS2 || defined NT) */

