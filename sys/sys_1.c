/* mytypes.c - system file and OS control routines */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#if (defined CSET2 || defined WATCOM || defined MSC60)
	#define	INCL_WINSHELLDATA
	#define	INCL_DOSSIGNALS
	#define	INCL_DOS
	#include <os2.h>
	#ifdef MSC60
		#include <process.h>
		#include <direct.h>
	#endif
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"

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
/* My share of the global vars     */
/* ------------------------------- */

/* ------------------------------- */
/* Private global vars             */
/* ------------------------------- */

/* ---------------------------------------------------------------------------
; Routine to determine the largest block of memory currently free
;
; Usage:  long int SysQueryMem();
;
; Inputs: none
;
; Output: Maximum memory block size currently free
;-------------------------------------------------------------------------- */
long int SysQueryMem(void) {

#ifdef MSC60
	LONG lAvail;
	if (DosMemAvail(&lAvail) != 0) lAvail= 0;			/* How much available */
	return(lAvail);
#else
	return(1048576l);
#endif
}

/* ===========================================================================
-- Modified access() subroutine to look for access mode *and* ensure that it
-- really is a file and not a subdirectory.
=========================================================================== */
static int Access(const char *path, int amode) {

	int rc;
	struct stat buf;

/* Use standard access to try first, followed by stat to be sure */
	             rc = access(path, amode);					/* Is it there?	*/
	if (rc == 0) rc = stat(path, &buf);						/* Does it stat?	*/
	if (rc == 0) rc = S_ISREG(buf.st_mode) ? 0 : -1;	/* Is it regular?	*/
/*	printf("Access: rc=%d  file=%s\n", rc, path); */
	errno = ENOENT;
	return(rc);
}
	

/* ===========================================================================
-- Subroutine to resolve a given base filename into a full pathname searching
-- along a specified path, with a specified set of possible extensions.
-- Order is always
--    1. Environment variable of basename
--    2. Current directory with all possible extensions
--    3. Each directory in search path, all possible extensions
--       searched within each directory before proceeding to the next
--
-- Usage:  SysFindFile(char *fullpath, char *base, char *search, char *exts,
--								int amode);
--
-- Input:  base   - root of the file to find (atom3.dat for example)
--         search - string of form dir1;dir2;dir3 - separated by ';' for
--                  OS/2 and ':' for UNIX systems
--         exts   - string of form ext1;ext2;ext3 - separated as above
--         amode  - mode of access needed - R_OK, W_OK, X_OK, F_OK from access()
--
-- Output: fullpath - pathname where file is found.
--
-- Returns: Success of finding a matching file (TRUE/FALSE)
============================================================================ */
static int FindIt(char *fullpath, char *base, char *path, char *exts,
						  int amode, BOOL TryGz);

BOOL SysFindFileGz(char *fullpath, char *base, char *path, char *exts,
						  int amode) {
	return FindIt(fullpath, base, path, exts, amode, TRUE);
}
BOOL SysFindFile(char *fullpath, char *base, char *path, char *exts,
						  int amode) {
	return FindIt(fullpath, base, path, exts, amode, FALSE);
}

#if (defined OS2 || defined NT)
	#define	SEPARATOR	';'			/* Separator on directory lists			*/
	#define	DISALLOWED	":\\/"		/* Illegal chars on a simple filename	*/
#else
	#define	SEPARATOR	':'
	#define	DISALLOWED	"/;"
#endif

/* ---------------------------------------------------------------------------
-- Routine to scan for any one of a list of extensions (or none) in a
-- given directory.  Called by ScanDirTree()
--------------------------------------------------------------------------- */
static char *ScanExtList(char *base, char *dir, char *exts, int amode, BOOL TryGz) {

	static char fname[PATH_MAX];			/* test filename					*/
	char ext[PATH_MAX];						/* Filename components			*/
	char *aptr;									/* And random pointer			*/

	*ext = '\0';
	do {
		SysMakePath(fname, dir, base, ext);	/* Create a test name	*/
		if (Access(fname, amode) == 0) {		/* If file exists, use	*/
			return(fname);
		} else if (TryGz && Access(strcat(fname, ".gz"), amode) == 0) {
			return(fname);
		}
		aptr = ext;								/* Copy next extension to ext[] */
		while (*exts && *exts != SEPARATOR) *aptr++ = *exts++;
		while (*exts == SEPARATOR) exts++;
		*aptr = '\0';
	} while (*ext != '\0');

	return(NULL);
}

/* ---------------------------------------------------------------------------
-- Routine to scan for any one of a list of extensions (or none) in a
-- directory or recursively under the directory.  Called by FindIt()
--------------------------------------------------------------------------- */
static char *ScanDirTree(char *base, char *specdir, char *exts, int amode, BOOL TryGz) {

	BOOL Recurse=FALSE, FullRecurse=FALSE;
	char *aptr, dir[PATH_MAX], sdir[PATH_MAX];	/* Local copies */

	DIR *dirp;
	struct dirent *afile;
	struct stat statbuf;

	strcpy(dir, specdir);								/* Make a copy I can abuse */
	aptr = dir+strlen(dir);								/* Last character */
	if (aptr > dir && (*(aptr-1) == '!')) {		/* See a ! ==> recursively look at all directories */
		Recurse = TRUE;
		*(--aptr) = '\0';
	}
	if (aptr > dir && (*(aptr-1) == '!')) {		/* See two ! ==> recursively look all the way down the tree */
		FullRecurse = TRUE;
		*(--aptr) = '\0';
	}

/* First - look in the existing directory */
	if ( (aptr = ScanExtList(base, dir, exts, amode, TryGz)) != NULL) return(aptr);
	if (! Recurse && ! FullRecurse) return(NULL);

/* Okay - now we are recursive.  Open the directory and search */
	if ( (dirp = opendir(dir)) == NULL) return(NULL);		/* Open the directory if possible */
	while ( (afile = readdir(dirp)) != NULL) {
		if (strcmp(afile->d_name, ".") == 0 || strcmp(afile->d_name, "..") == 0) continue;
		sprintf(sdir, "%s/%s", dir, afile->d_name);
		if (stat(sdir, &statbuf) != 0 || ! S_ISDIR(statbuf.st_mode)) continue;
		if (FullRecurse) strcat(sdir, "!!");
		if ( (aptr = ScanDirTree(base, sdir, exts, amode, TryGz)) != NULL) break;
	}
	closedir(dirp);
	return(aptr);
}

/* ---------------------------------------------------------------------------
-- Generic routine to look for files along complex paths.  Implements
-- the root search in environment variables and as given, then calls
-- ScanDirTree() for each directory within a path specifier.
--------------------------------------------------------------------------- */
static int FindIt(char *fullpath, char *base, char *path, char *exts, int amode, BOOL TryGz) {

	char fname[PATH_MAX], dir[PATH_MAX], *sdir, *aptr;
	
	if (Access(base, amode) == 0) {				/* If file exists, use */
		strcpy(fullpath, base);
		return(TRUE);
	} else if (TryGz && Access(strcat(strcpy(fname, base), ".gz"), amode) == 0) {
		strcpy(fullpath, fname);
		return(TRUE);
	} else if ( (aptr=getenv(base)) != NULL) { /* If SET, use that	*/
		strcpy(fullpath, aptr);
		return(Access(aptr, amode) == 0);
	} else if ( (aptr=getenv(strupr(strcpy(fname, base)))) != NULL) {
		strcpy(fullpath, aptr);
		return(Access(aptr, amode) == 0);
	}

/* ------------------------------------------------------------------------
-- search may contain a string of form <dir>;<dir>;<dir> 
-- exts   may contain a string of form <ext>;<ext>;<ext>
-- The empty ext is also considered valid, as is the empty dir.
------------------------------------------------------------------------ */

	sdir = (path == NULL) ? "" : path;		/* Path to search					*/
	if ( (strpbrk(base, DISALLOWED)) != NULL) sdir = "";
	if (exts == NULL) exts = "";				/* Modify as necessary			*/

	*dir = '\0';									/* Start with local directory */
	do {
		if ( (aptr = ScanDirTree(base, dir, exts, amode, TryGz)) != NULL) {
			strcpy(fullpath, aptr);
			return(TRUE);
		}
		aptr = dir;									/* Copy next directory to dir[] */
		while (*sdir && *sdir != SEPARATOR) *aptr++ = *sdir++;
		if (*sdir == SEPARATOR) sdir++;
		*aptr = '\0';
	} while (*dir != '\0');
		
	strcpy(fullpath, base);					/* Default behavior */
	return(FALSE);
}


/* ===========================================================================
-- Subroutine to translate a relative filename into a fully qualified pathname.
--
-- Usage:  SysQualifyPath(char *buffer, const char *path, size_t buffer_len);
--
-- Input:  path - partially qualified pathname.  If specified as NULL,
--                returns current path only with no filename.
--
-- Output: buffer - fully qualified pathname.  Full path to the file is
--                  returned including any disk or home directory structures.
--                  Path should be point to file from any other directory.
--                  On error, returns NULL.  (real screwup!)
--
-- Note: (1) The file need not currently exist, nor need the name be valid as
--           as filename.  May be used for adding directory to wildcards.
--       (2) If unable to implement, return path unchanged.
============================================================================ */
char *SysQualifyPath(char *buffer, char *path, size_t buffer_len) {
	
#if (defined OS2 || defined NT)

	char curdir[] = ".", diskdir[] = "a:.";

/* ... Deal with chance of specifying e: only.  _fullpath normally fails */
	if (path == NULL || *path == '\0') {
		path = curdir;
	} else if (path[1] == ':' && path[2] == '\0') {
		*diskdir = *path;
		path = diskdir;
	}
	return(_fullpath(buffer, path, buffer_len));

#else		/* OS2 */

	char *aptr;
	int  ileft;
	char mybuf[PATH_MAX], *username;
	struct passwd *pw;								/* Password information	*/

/* (1) If there is any // or /~ tokens, they mark path restarts */
	while ( (aptr = strstr(path, "//")) != NULL) path = aptr+1;
	while ( (aptr = strstr(path, "/~")) != NULL) path = aptr+1;

/* (2) If resulting path starts with /, we have an absolute path */
	if (*path == '/') {										/* Root specified name */
	   if (buffer == NULL) {
		   buffer = strdup(path);
		} else {
		   if (strlen(path) >= buffer_len) return(NULL);
			if (buffer != path) strcpy(buffer, path);
		}
		return(buffer);
	}

/* (3) If starts with ~ then, need to lookup the user name and info */
	if (*path == '~') {
		username = ++path;										/* Skip over the ~	*/
		while (*path != '/' && *path != '\0') path++;	/* Scan for dir sep	*/
		if (path != username) {
			strncpy(mybuf, username, path-username);		/* Really strncpy	*/
			mybuf[path-username] = '\0';
			pw = getpwnam(mybuf);
		} else {
			pw = getpwuid(getuid());
		}
		if (pw == NULL) return(NULL);
		if (*path == '/') path++;								/* This now handled */

/* Okay, copy the parts */
		strcpy(mybuf, pw->pw_dir);								/* First part */
		aptr = mybuf + strlen(mybuf);
		if (aptr != mybuf && *(aptr-1) == '/') *(--aptr) = '\0';

	} else if (getcwd(mybuf, PATH_MAX) == NULL) {
		return(NULL);
	}

/* At this point, we have buffer filled with starting info, now walk through
   the additional path info eliminating .. and . structures */
	while (*path != '\0') {
		if (*path == '.') {					/* Handle . and .. */
			path++;
			if (*path == '.') {
				path++;
				if ( (aptr = strrchr(mybuf, '/')) == NULL) return(NULL);
				*aptr = '\0';
			}
			if (*(path++) != '/') return(NULL);
		} else {
			aptr  = mybuf + strlen(mybuf);				/* Point to the null */
			ileft = PATH_MAX-strlen(mybuf);				/* And # available	*/
			*(aptr++) = '/';	ileft--;						/* File separator		*/
			while (*path && *path != '/') {
				if (ileft-- <= 0) return(NULL);
				*(aptr++) = *(path++);
			}
			*aptr = '\0';
			if (*path == '/') path++;
		}
	}

	if (buffer == NULL) {
	   buffer = strdup(mybuf);
	} else {
	   if (strlen(mybuf) >= buffer_len) return(NULL);
		strcpy(buffer, mybuf);
	}
	return(buffer);

#endif	/* OS2 */

}


/* ---------------------------------------------------------------------------
-- Routine to attempt filename completion from partial name
--
-- Usage:   int = SysCompleteFilename(char *pattern, char *name, size_t length);
--
-- Inputs:  pattern - First letters to be matched -- can be filename or path
--          length  - Maximum number of characters in name
--
-- Output:  Fills name with partially completed filename.
--
-- Returns:  number of matching files to beginning pattern.
--          -1 ==> Matching name will not fit in name space
--           0 ==> none, pattern is returned as name
--           1 ==> unique filename returned
--          >1 ==> non-unique, "common" part of all matching names returned
--
-- Notes: If matching name is <dir>, will be returned with an appended '/'
--------------------------------------------------------------------------- */
int SysCompleteFilename(char *pattern, char *name, size_t length) {

	char *dirnull, *fname, *tmp, *tmp2, *dir, *match;
	DIR	*dirp;										/* Directory unit */
	struct dirent *entry;							/* And an entry	*/
	struct stat		statbuf;
	int imatch, ilen;

	ilen = (int) strlen(pattern);
	if (ilen==0 || ilen>=PATH_MAX || pattern[ilen-1]=='/' || pattern[ilen-1] == '\\')
		return(0);

	SysMarkPath(pattern, &fname, &dirnull);	/* Split off the directory	*/
	if (*fname == '\0') return(0);				/* No filename component	*/
	dir = strdup(pattern);
	dir[dirnull-pattern] = '\0';					/* Null terminate				*/
	ilen = (int) strlen(fname);					/* # of chars in name part	*/

	if ( (dirp = opendir(*dir ? dir : ".")) == NULL) {free(dir); return(0);}

	imatch = 0;
	while ( (entry = readdir(dirp)) != NULL) {
#if (defined OS2 || defined NT)
		if (strnicmp(entry->d_name, fname, ilen) != 0) continue;
#else
		if (strncmp (entry->d_name, fname, ilen) != 0) continue;
#endif
		if (imatch == 0) {
			match = strdup(entry->d_name);		/* Maximum length it will ever be */
		} else {
			tmp=match; tmp2=entry->d_name;
#if (defined OS2 || defined NT)
			while (tolower(*tmp) == tolower(*tmp2)) {tmp++; tmp2++;}
#else
			while (*tmp == *tmp2) {tmp++; tmp2++;}
#endif
			*tmp = '\0';
		}
		imatch++;
	}
	closedir(dirp);

	if (imatch == 0) match = strdup(fname);
	
	if (strlen(dir)+strlen(match)+2 > length) {
		imatch = -1;
	} else {
		SysMakePath(name, dir, match, NULL);
		if (imatch==1 && stat(name, &statbuf)==0 && S_ISDIR(statbuf.st_mode))
			strcat(name, "/");				/* Directory identifier */
	}

	free(dir); free(match);
	return(imatch);
}


/* ===========================================================================
-- Subroutine to change active disk
--
-- Usage:  BOOL SysSetDisk(CHAR disk)
--
-- Inputs: NAME - Disk name (CHARACTER)
--	         May be a number 1=A, 2=B or the letter A,B,C.  Only the
--	         first character is used in test, and only low bits kept.
--
-- Output: SysSetDisk - Success of disk change
=========================================================================== */
BOOL SysSetDisk(char disk) {

#ifdef CSET2
	ULONG Current, DriveMap;								/* For current query		*/

	if (isalpha(disk)) disk &= 0x1F;						/* Convert to number		*/
	if (disk == 0) return(TRUE);							/* 0 => current drive	*/
	if (disk > 26) return(FALSE);							/* Beyond Z = bad!		*/
	
	DosQueryCurrentDisk(&Current, &DriveMap);
	if ( (DriveMap & (0x01 << (disk-1)) ) == 0) return(FALSE);
	return(_chdrive(disk) == 0);
#elif defined MSC70
	if (isalpha(disk)) disk &= 0x1F;                /* Convert to number    */
	if (disk == 0) return(TRUE);                    /* 0 => current drive   */
	if (disk > 26) return(FALSE);                   /* Beyond Z = bad!      */
	return(_chdrive(disk) == 0);
#else
	return(FALSE);
#endif

}

/* ===========================================================================
-- SysSplitPath() breaks a full path name into three components.  <path> points
-- to the full path. The maximum size necessary for each buffer is assumed to
-- be PATH_MAX.
--
-- Syntax: void SysSplitPath(const char *path, char *dir, char *fname, char *ext);
--
-- Inputs: path  - full path name potentially containing any of the components.
--
-- Output: dir   - Path, if any, of subdirectory including trailing slash.
--         fname - Base file name minus last extension
--         ext   - Last extension, if any, including the leading dot (.).
--
-- Notes: 1. Any component not in <path> is set to empty string
--        2. Forward and backslashes are considered synonymous
=========================================================================== */
void SysSplitPath(const char *path, char *dirn, char *filen, char *extn) {
	
	const char *dir=path, *file, *ext, *aptr;
	ptrdiff_t ilen;
	
	while (isspace(*dir)) dir++;			/* Skip any white space	*/
	file = dir;									/* Initial guess			*/

#if (defined OS2 || defined NT)
	if (file[2] == ':') file += 2;		/* Skip over the drive spec */
#endif

/* ... Find start of file as first char past last forward or back slash */
	while ( (aptr = strpbrk(file, "\\/")) != NULL) file = aptr+1;

/* ... Find start of extension as the last . in file, or NULL */
	ext = strrchr(file, '.');
	if (ext == NULL) ext = file + strlen(file);

	if (dirn != NULL) {
		if ( (ilen = file-dir) != 0) strncpy(dirn, dir, ilen);
		dirn[ilen] = '\0';
	}
	if (filen != NULL) {
		if ( (ilen = ext-file) != 0) strncpy(filen, file, ilen);
		filen[ilen] = '\0';
	}
	if (extn != NULL) strcpy(extn, ext);
	return;
}

/* ===========================================================================
-- The SysMakePath() routine combines path, file and ext components to generate
-- full pathname.  Any component may be NULL or blank, and components need not
-- be properly specified (ie. test.o is a valid base filename and will generate
-- the same result as specifying test as base  filename and .o as extension).
-- <path> must point to a buffer large enough to contain a concatenation of all
-- elements plus separators.
--
-- Syntax:  void SysMakePath(char *path, const char *dir, const char *fname,
--                                       const char *ext);
--
-- Inputs: dir   - Subdirectory path optionally followed by a /.  If dir is
--                 NULL or empty, only fname returned.
--         fname - filename with possible extension.  If NULL or empty,
--                 nothing is inserted.
--         ext   - filename extension with optional leading dot (.).  A dot
--                 will be automatically inserted if it is not present.
--
-- Output: path  - completely formed path from the specified components.
--
-- Notes: 1. Full path should fit in PATH_MAX length though this is not checked.
--        2. Under OS2, both forward and back slashes are allowed. 
--        3. Path is locally allocated, so possible to reuse dir or fname in call
--        4. For UNC, change first two characters to \\ instead of //
=========================================================================== */
void SysMakePath(char *path_user, const char *dir, const char *fname, const char *ext) {
	
	char path[PATH_MAX], *aptr=path;

	if (dir != NULL && *dir) {
		strcpy(aptr, dir);
		aptr += strlen(dir);
#if (defined OS2 || defined NT)
		if (strchr(":/\\", *(aptr-1)) == NULL) *aptr++ = '\\';
#else
		if (*(aptr-1) != '/') *aptr++ = '/';
#endif
	}

	if (fname != NULL && *fname) {
		strcpy(aptr, fname); 
		aptr += strlen(fname);
	}

	if (ext != NULL && *ext) {
		if (*ext != '.') *(aptr++) = '.';		/* Add a dot to the path */
		strcpy(aptr, ext);
		aptr += strlen(ext);
	}

	*aptr = '\0';										/* Make sure terminated */
	strcpy(path_user, path);						/* And copy to user space */

/* For UNC, change the //server to \\server so valid */
#if (defined OS2 || defined NT)
	if (strncmp(path_user, "//", 2) == 0 && isalnum(path_user[2])) path_user[0] = path_user[1] = '\\';
#endif

	return;
}


/* ===========================================================================
-- The SysMarkPath() routine takes a specified path and returns pointers
-- identifying where the filename starts, and where one would place a null
-- such that the directory stands alone.
--
-- Syntax:  SysMarkPath(const char *path, char **fname, char **dirnull);
--
-- Inputs: path  - string containing a path (quite possibly invalid or wild)
--
-- Output: *fname   - pointer to first char in path which is part of the filename
--         *dirnull - pointer to position a NULL should be written to turn path
--                    into a true directory name
--
-- Returns: nothing
--
-- Notes: 1. Under UNIX, fname is either same or one character ahead of dirnull
--        2. Under OS/2, the ambiguity of the c:\ is properly handled.
=========================================================================== */
void SysMarkPath(const char *path, char **pfname, char **pdirnull) {

	char *fname, *dirnull;

#if (defined OS2 || defined NT)
	char *aptr;

	if ( (dirnull = strpbrk(path, "/\\:")) == NULL) {	/* No separators */
		dirnull = fname = (char *) path;
	} else {
		while ( (aptr=strpbrk(dirnull+1, "/\\:")) != NULL) dirnull = aptr;
		fname = dirnull+1;											/* File starts */
	
		if (*dirnull == ':') {										/* c:junk.bat	*/
			dirnull++;
		} else if (dirnull != path && *(dirnull-1) == ':') {	/* c:\junk.bat	*/
			dirnull++;
		} else if (dirnull == path && (*dirnull == '/' || *dirnull == '\\') ) {
			dirnull++;
		}
	}
#else
	if ( (dirnull = strrchr(path, '/')) == NULL)				/* No separators */
		dirnull = fname = (char *) path;
	else {
		fname = dirnull+1;											/* fname after sep */
		if (dirnull == path && *dirnull == '/') dirnull++;
	}
#endif

	if (pdirnull != NULL) *pdirnull = dirnull;
	if (pfname   != NULL) *pfname   = fname;
	return;
}


/* ===========================================================================
-- Routine to add an extension if one does not already exist on the file
--
-- Usage:  char *SysAddExt(char *pathname, char *newext);
--
-- Inputs: pathname - pointer to existing path
--         newext   - pointer to extension to append
--
-- Output: potentially modified pathname
--
-- Return: pointer to pathname
=========================================================================== */
char *SysAddExt(char *pathname, char *newext) {
	
	char dir[PATH_MAX];				/* Directory */
	char fname[PATH_MAX];			/* Filename	 */
	char ext[PATH_MAX];				/* Extension */

	SysSplitPath(pathname, dir, fname, ext);
	if (*ext == '\0') SysMakePath(pathname, dir, fname, newext);
	return(pathname);
}


/* ===========================================================================
-- Replace an existing extension with a different one - or add if none exists
--
-- Usage:  char *SysReplaceExt(char *pathname, char *newext);
--
-- Inputs: pathname - pointer to existing path
--         newext   - pointer to extension to append
--
-- Output: potentially modified pathname
--
-- Return: pointer to pathname
=========================================================================== */
char *SysReplaceExt(char *pathname, char *newext) {
	
	char dir[PATH_MAX];				/* Directory */
	char fname[PATH_MAX];			/* Filename	 */
	char ext[PATH_MAX];				/* Extension */

	SysSplitPath(pathname, dir, fname, ext);
	SysMakePath (pathname, dir, fname, newext);
	return(pathname);
}

/* ===========================================================================
-- Delete extension from a pathname
--
-- Usage:  char *SysDeleteExt(char *pathname);
--
-- Inputs: pathname - pointer to existing path
--
-- Output: potentially modified pathname
--
-- Return: pointer to pathname
=========================================================================== */
char *SysDeleteExt(char *pathname) {
	
	char dir[PATH_MAX];				/* Directory */
	char fname[PATH_MAX];			/* Filename	 */
	char ext[PATH_MAX];				/* Extension */

	SysSplitPath(pathname, dir, fname, ext);
	SysMakePath (pathname, dir, fname, NULL);
	return(pathname);
}

/* ===========================================================================
-- Routine to determine info on a user specified name.  Determines if it is
-- an existing file or directory, or non-existent name, or a wildcard name
-- which may or may not exist.
--
-- Usage:    SysFileCheck(char *name, char *fulldir, char *filename)
--
-- Inputs:   name  - name to be checked
--
-- Output:   fulldir - if not NULL, filled with the directory in which the file
--                     would exist
--           filename - if not NULL, filled with only the filename part of the
--                      passed name.
--
-- Returns:  -1 ==> something really invalid.
--            0 ==> existing file
--            1 ==> existing directory
--            2 ==> non-existent file
--            3 ==> wildcard filename (validity not determined)
=========================================================================== */
int SysFileCheck(char *name, char *fulldir, char *filename) {
	
	char pathname[PATH_MAX];		/* Full pathname */
	char dir[PATH_MAX];				/* Directory */
	char fname[PATH_MAX];			/* Filename	 */
	char ext[PATH_MAX];				/* Extension */
	struct stat statbuf;				/* Buffer for status of name */
	int i;
	
	strcpy(pathname, name);										/* Get a copy */
	if (SysQualifyPath(pathname, name, sizeof(pathname)) == NULL) return(-1);

	SysSplitPath(pathname, dir, fname, ext);				/* Split the name		*/
	if (fulldir != NULL) {										/* Reassamble pieces */
		SysMakePath(fulldir, dir, NULL, NULL);				/* Reassemble pieces */
		if ( (i = (int) strlen(fulldir)) >= 2) 			/* Make name clean	*/
			if (fulldir[i-2] != ':') fulldir[i-1] = '\0';
	}
	if (filename != NULL)										/* And maybe filename */
		SysMakePath(filename, NULL, fname, ext);

	if (stat(pathname, &statbuf) != -1) {					/* Name exists already */
		if (! S_ISDIR(statbuf.st_mode)) return(0);		/* If not dir, assume file */
		if (fulldir  != NULL) strcpy(fulldir,pathname);	/* Now, is dir, so set so */
		if (filename != NULL) *filename = '\0';
		return(1);													/* Tell world is dir */
	}
	if ( *fname == '!' ||
        (strpbrk(fname, "*?[]^!") != NULL) || 
		  (strpbrk(ext, "*?[]^!")   != NULL)) return(3);	/* Wild card filename */
	return(2);														/* Non-existent file */
}

/* ---------------------------------------------------------------------------
-- Routine to compare a given file string against a match pattern.  Will handle
-- really almost arbitrary strings -- not limited to files.  This is within the
-- UNIX philosophy, not the PC, so structures like *abc*def?x will be handled
-- properly.
--
-- Usage:    BOOL = SysCheckMatch(char *name, char *pattern)
--
-- Inputs:   name    - name to be checked for match
--           pattern - wild card pattern to be compared against
--
-- Output:   none
--
-- Returns:  TRUE  - name matches the pattern
--           FALSE - name fails to match the pattern
--
-- Notes:    pattern may consist of the following special characters
--              ! -- preceeding entire expression, matches any that fail set
--              * -- matches any 0 to n characters
--              ? -- matches any single character
--              [A-z]        -- matches any character in the range A-z
--              [A,B,C,a-z]  -- matches any of the specified characters
--              ^[...]       -- negates the effect of the above
--
-- The routine ChkRegular does the work for the range expressions in a regular
-- expression [...].
---------------------------------------------------------------------------- */
static int ChkRegular(char achr, char *pattern, char **endpattern) {

	char a1, a2;
	int invert=FALSE, match=FALSE;

	if (*pattern == '^') {invert = TRUE; pattern++;}	/* Are we reversed */
	if (*(pattern++) != '[') goto BadPattern;				/* Invalid pattern */

	while (TRUE) {
		if ( ! (a1 = *(pattern++)) ) goto BadPattern;	/* EOS, fail		*/
		if (a1 == ']') {
			if (endpattern != NULL) *endpattern = pattern;
			return(invert ? !match : match);					/* Return status	*/
		}
		if (a1 == '\\' && ( (a1=*(pattern++)) == '\0') ) goto BadPattern;
		if (*pattern == '-') {									/* Range spec		*/
			pattern++;												/* Skip the -		*/
			if ( (a2 = *(pattern++)) == '\0') goto BadPattern;
			if ( (achr >= a1) && (achr <= a2) ) match = TRUE;
		} else if (*pattern==',' || *pattern==']') {		/* Valid single?	*/
			if (achr == a1) match = TRUE;
		} else {
			goto BadPattern;
		}
		if (*pattern == ',') pattern++;						/* Skip separator	*/
	}

BadPattern:
/*	fprintf(stderr, "Invalid regular expression: %s -- %s\n", pattern, orgpattern); */
	return(-1);
}

/* ------------------------------------------------------------------------- */
BOOL SysCheckMatch(char *String, char *Pattern) {

	char	  *WildPosn=NULL;
	char	  *HoldString=NULL;							/* Avoid compiler warning */
	char	  *aptr;
	int		rcode;
	int		AtWild = FALSE;
	BOOL		Success = TRUE;							/* Let a match return TRUE */
	char		upandlow[3] = "aA";						/* Upper & lower case		*/
		
	if (*Pattern == '!') {
		Success = FALSE;									/* A match now returns FALSE */
		Pattern++;
	}

	while (*String) {										/* As long as possible	*/
		if (AtWild) {
			Pattern = WildPosn;							/* Where we look from	*/
			upandlow[0] = (char) tolower(*Pattern);
			upandlow[1] = (char) toupper(*Pattern);
			if (*Pattern == '[' || (*Pattern == '^' && Pattern[1] == '[') ) {
				String = HoldString;
				while (TRUE) {								/* Search all chars		*/
					if (! *String) return(!Success);
					if ( (rcode = ChkRegular(*String, Pattern, &aptr)) == -1) return(!Success);
					String++;								/* Okay, it did or did not */
					if (rcode == 1) break;				/* Yes, it did					*/
				}
				Pattern = aptr;							/* Pattern now here			*/
#ifdef FS_CASE_INSENSITIVE
			} else if ( (FSType & FS_CASE_INSENSITIVE) &&
				( (String = strpbrk(HoldString, upandlow)) != NULL) ) {
				String++; Pattern++;						/* Found a matching char */
			} else if ( (String = strchr(HoldString, *Pattern)) != NULL) {
				String++; Pattern++;						/* Found a matching char */
#else
			} else if ( (String = strpbrk(HoldString, upandlow)) != NULL) {
				String++; Pattern++;						/* Found a matching char */
#endif
			} else {
				return(! Success);						/* No match, fail now	 */
			}
			HoldString = String;							/* Save string posn		*/
			AtWild = FALSE;
		} else if ( (*Pattern == '[' || (*Pattern == '^' && Pattern[1] == '[')) &&
			( (rcode = ChkRegular(*String, Pattern, &aptr)) != 0) ) {
			if (rcode == -1) return(! Success);		/* Fail on error			*/
			Pattern = aptr;
			String++;
		} else if (*Pattern == '*') {					/* Wild character?		*/
			while (TRUE) {
				if (*Pattern=='?') {
					if (*(++String) == '\0') {
						if (*(++Pattern) == '\0') return(Success);
						return(!Success);
					}
				} else if (*Pattern != '*')
					break;
				Pattern++;									/* Go to next pattern	*/
			}
			if (*Pattern == '\0') return(Success);	/* Must be done!			*/
			WildPosn   = Pattern;						/* Save first past *		*/
			HoldString = String;
			AtWild = TRUE;
#ifdef FS_CASE_INSENSITIVE
		} else if (*Pattern == '?' || (*String == *Pattern) ||
			( (FSType & FS_CASE_INSENSITIVE) && (tolower(*String)==tolower(*Pattern)) )) {
#else
		} else if (*Pattern == '?' || (tolower(*String) == tolower(*Pattern)) ) {
#endif			
			Pattern++;
			String++;
		} else if (WildPosn != NULL) {				/* Any wild chars?		*/
			AtWild = TRUE;
		} else {
			return(!Success);
		}
	}
	while (*Pattern=='*') Pattern++;					/* Skip any trailing *	*/
	if (*Pattern == '\0') return(Success);			/* Did it end also?		*/
	return(! Success);
}

/* ---------------------------------------------------------------------------
-- The tempnam function allows the program to create a temporary file in
-- another directory. This file name will be unique.  The <ext> argument is
-- the extension (after .) in the name.  malloc() is used to allocate space
-- for the name and the program must free() the memory when complete.
--
-- Filenames are created in the following directories:
--
--  (1) directory specified by the dfltdir if it is not NULL
--  (2) directory pointed to by variable TEMP/TMP if it exists (uppercase)
--  (3) current directory
--
-- If the TMP/TEMP directory is invalid (ie. does not exist even though
--    specified, routine will use current directory as TMP instead.  It
--    will not, however, override specification of the dfltdir if given.
--
-- Usage:  char *SysTempFile(char *dir, char *ext)
--
-- Returns a unique filename in a temporary directory.
--
-- Inputs:  dir - dir to use if no user envirornment specified one
--          ext - extension for the filename
--
-- Returns:  pointer to character string with a unique filename.  String
--           is allocated by malloc() and must be freed by calling program.
--           NULL if unsuccessful
--
-- Notes:    This is a POSIX implementation (using SysMakePath).  OS/2 does not
--           return ENOTDIR from stat so must handle separately to check for
--           a valid directory
---------------------------------------------------------------------------- */
char *SysTmpFilename(char *dirptr, char *suffix) {

	char pathname[PATH_MAX];		/* Pathname created			*/
	char fname[8];						/* Filename part				*/
	struct stat buf;
	int i;

/* Check that directory is valid, if not take appropriate action */
	if (dirptr != NULL && *dirptr != '\0') {
#if (defined OS2 || defined NT)
		if (isalpha(*dirptr) && dirptr[1]==':' && dirptr[2]=='\0') goto FindName;
#endif
		if (stat(dirptr, &buf) == 0 && S_ISDIR(buf.st_mode)) goto FindName;
		errno = ENOTDIR;
		return(NULL);
	} else {
		if ( (dirptr = getenv("TMP")) == NULL) dirptr = getenv("TEMP");
		if (dirptr == NULL || *dirptr == '\0') {dirptr = NULL; goto FindName;}
#if (defined OS2 || defined NT)
		if (isalpha(*dirptr) && dirptr[1]==':' && dirptr[2]=='\0') goto FindName;
#endif
		if (stat(dirptr, &buf) == 0 && S_ISDIR(buf.st_mode)) goto FindName;
/*		ERRprintf("ERROR: TMP variable (%s) invalid, using current dir\n", dirptr); */
		dirptr = NULL;
	}
		
FindName:
	for (i=0; i<9999; i++) {
		sprintf(fname, "#t_%4.4i", i);
		SysMakePath(pathname, dirptr, fname, suffix);
		if (Access(pathname, F_OK) == 0) {				/* File exists		*/
			continue;											/* Try next name	*/
		} else if (errno == ENOENT) {						/* No entry ==> success */
			return(strdup(pathname));
		} else {
			return(NULL);
		}
	}
	return(NULL);
}


/* ===========================================================================
-- The SysTmpFile() command allocates and opens a unique file in a specified
-- directory, commonly as a temporary file.  The file is not deleted upon exit
-- of the program (unlike ANSI tmpfile() function).  Directory can be specified
-- or may allow use of the directory specified by the TMP environment variable.
--
-- Usage:  FILE *SysTmpFile(char *pathname, char *dfltdir, char *ext);
--
-- Inputs: dfltdir - default directory to create file in.  If specified as
--                   NULL, the directory specified by the TMP environment
--                   variable will be used instead.
--         ext     - default extension to use on temporary name.  File will be
--                   of the form T_nnnn.ext where nnnn consists of digits [0-9]
--
-- Output: pathname - If not NULL, resulting filename will be copied here. 
--                    Must be capable of holding upto PATH_MAX chars unless you 
--                    know better.
--
-- Returns: Handle to a unique, not previously existing, file opened using
--          fopen() with attributes "w".  If unsuccessful, return NULL with
--          errno properly set.
--
-- If attempt to open in "dfltdir" fails, or if "dfltdir" is NULL and the
-- TMP directory is invalid, temporary file will be opened in the current
-- directory.
=========================================================================== */
FILE *SysTmpFile(char *pathname, char *dfltdir, char *ext, char *mode) {
	
	FILE *ftmp;
	char *aptr;
	char *mymode="w";
	
	if ( (aptr = SysTmpFilename(dfltdir, ext)) == NULL) return(NULL);
	if (pathname != NULL) strcpy(pathname, aptr);
	if (mode != NULL) mymode = mode;
	ftmp = fopen(aptr, mymode);
	free(aptr);
	return(ftmp);
}

/* ---------------------------------------------------------------------------
-- Routine to move (either by rename or by copy) a file from an old name to 
-- a new name.  Will handle renaming across physical disks by copying if
-- unable to simply rename.  Copy is stupid - does simple open and duplicate.
-- The file timestamps, and any extended attributes under OS/2 will be lost.
--
-- Usage:  int SysMoveFile(char *old, char *new);
--
--	Inputs: old -- name of the old file
--         new -- name where it is wanted
--
-- Returns:  0 -- all successful (except possibly the unlink)
--          -1 -- memory allocation failed
--           1 -- new filename already exists
--           2 -- old name could not be opened
--           3 -- new name could not be opened
--
-- Note: This routine will attempt to delete the old file
---------------------------------------------------------------------------- */
#define	MOVE_BUFFER_SIZE	4096
int SysMoveFile(char *old, char *new) {

	FILE *fold, *fnew;
	char *buffer;
	int n;

	if (access(new, F_OK) == 0) return(1);
	if ( (buffer = malloc(MOVE_BUFFER_SIZE)) == NULL) return(-1);

	if ( (fold = fopen(old, "rb")) == NULL) {
		free(buffer);
		return(2);
	} else if ( (fnew = fopen(new, "wb")) == NULL) {
		fclose(fold); 
		free(buffer);
		return(3);
	}

	while ( (n = (int) fread(buffer, 1, MOVE_BUFFER_SIZE, fold)) > 0) 
	  	fwrite(buffer, 1, n, fnew);

	free(buffer);
	fclose(fold);
	fclose(fnew);
	remove(old);
	return(0);
}


/* ===========================================================================
-- Usage: int SysReadLongLine(LEXPSTR *inbuf, FILE *funit, int bStrip);
--
-- Inputs: inbuf         - pointer to allocated character string structure
--           inbuf->buf  - actual buffer
--           inbuf->size - dimensioned size of buffer
--         bFlags        - Binary flag indicating whitespace stripping
--						B_STRIP_LEADING_WHITESPACE			0x01
--						B_STRIP_TRAILING_WHITESPACE		0x02
--						B_SKIP_BLANK_LINES					0x04
--						B_SKIP_COMMENT_LINES					0x08
--						B_ALLOW_CONTINUES						0x10
--
-- Output: inbuf->buf  - filled with next line read from the file
--                       The size of inbuf->buf will be modified if necessary
--                       to accomodate the entire line, no matter how long.
--                       Both inbuf->buf and inbuf->size may be changed by
--                       this call.
--
-- Returns: len - number of characters in the buffer.  The terminating
--                newline will be stripped first.
--
-- NOTES: (1) inbuf->buf may be allocated or modified even if nothing is 
--            read from the file.  Blank lines are returned unless flagged.
=========================================================================== */
#define	LINEBUFSIZE	1024
int SysReadLongLine(ARBSTR *inbuf, FILE *funit, int bFlags) {

	char *aptr;
	int ilen;

	if (inbuf->buf == NULL || inbuf->size <= 0) {
		inbuf->size = LINEBUFSIZE;
		inbuf->buf  = realloc(inbuf->buf, inbuf->size);
	}

/* Read first, then continue until we get a \n or reach end of the file */
ReadAnother:
	*inbuf->buf = 0;								/* Null terminate for no-read */
	if (feof(funit)) return(-1);
ReadMore:
	while (TRUE) {
		ilen = (int) strlen(inbuf->buf);
		if (fgets(inbuf->buf+ilen, inbuf->size-ilen, funit) == NULL) break;
		if ( (aptr = strchr(inbuf->buf, '\n')) != NULL) {*aptr = '\0'; break;}
		if (feof(funit)) break;
		
		inbuf->size += LINEBUFSIZE;
		inbuf->buf   = realloc(inbuf->buf, inbuf->size);
	}

/* Strip trailing whitespace */
	if (bFlags & B_STRIP_TRAILING) {
		aptr = inbuf->buf + strlen(inbuf->buf) - 1;
		while (aptr >= inbuf->buf && isspace(*aptr)) *aptr-- = '\0';
	}

	if (bFlags & B_STRIP_LEADING) {
		aptr = inbuf->buf;
		while (isspace(*aptr)) aptr++;
		if (aptr != inbuf->buf) memmove(inbuf->buf, aptr, strlen(aptr)+1);
	}

/* Return value is string length */
	ilen = (int) strlen(inbuf->buf);

/* Skip blank lines if so directed, or if skipping comments */
	if (bFlags & (B_SKIP_BLANK | B_SKIP_COMMENTS) && ilen == 0) 
		goto ReadAnother;

/* Skip comments (identified as /_* or #) */
	if (bFlags & B_SKIP_COMMENTS) {
		aptr = inbuf->buf;
		if (*aptr == '#') {
			if (aptr[1] == '\0' || isspace(aptr[1])) goto ReadAnother;
		} else if (strncmp(aptr, "/*", 2) == 0) {
			if (aptr[2] == '\0' || isspace(aptr[2])) goto ReadAnother;
		}
	}

/* And read more if logical continuation (and enabled) */
	if (bFlags & B_ALLOW_CONTINUES && ilen >= 2) {
		aptr = inbuf->buf + ilen - 2;				/* Last two characters */
		if (aptr[1] == '\\' && isspace(aptr[0])) {
			*aptr = '\0';								/* Truncate last two */
			goto ReadMore;
		}
	}

	if (ilen == 0 && feof(funit)) ilen = -1;			/* End of file */
	return(ilen);
}


/* ---------------------------------------------------------------------------
-- Routine to print unusual error message and return as an abort
--
-- Usage:  void SysPanic(char *file, int line);
--
--	Inputs: file -- filename of offending line.
--         line -- line where offense took place
--
-- Normally defined as a macro:  #define panic SysPanic(__FILE__, __LINE__)
---------------------------------------------------------------------------- */
void SysPanic(char *file, int line) {
	
	perror("Unexpected library error");
	fprintf(stderr,"\n?Panic in line %d of file %s\n",line,file);
	fflush(stderr);
	abort();
}
