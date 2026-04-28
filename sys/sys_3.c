/* Replacement/extensions of POSIX routines - see preload.h */

/* ---------------------------------------------------------------------------
-- Revision history:
--
-- 10/14/97 - Forced loading of "mytypes.h" in all cases - needed for AIX
--------------------------------------------------------------------------- */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#if (defined UNIX || defined LINUX)
	#include <pwd.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#define NO_PATH_EXTENSIONS
#include "mytypes.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)
#ifndef TRUE
	#define	TRUE	1
#endif
#ifndef FALSE
	#define	FALSE	0
#endif

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static char *qualify_name(char mybuf[PATH_MAX], const char *partial);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of the global vars     */
/* ------------------------------- */

/* ------------------------------- */
/* Private global vars             */
/* ------------------------------- */


/* ===========================================================================
-- Routine to reduce passed name to qualified name
--
-- Usage: char *qualify_name(char mybuf[PATH_MAX], const char *path);
--
-- Inputs: mybuf   - space for potentially revised name (PATH_MAX long)
--                   NOTE: It is possible that mybuf is not used!
--         partial - partial name
--
-- Output: Guarenteed to point to a string with the full pathname.
--         However, this is not necessarily mybuf -- if path has a
--         pure name, it will be used.
--
-- Returns: pointer to mybuf Pointer to minimum valid name.  This may be 
--          a pointer to a subsegment of path (which never changes) or 
--          to mybuf if necessary to construct the name
--
--          If error, simply returns the original path assuming error will
--          occur.
=========================================================================== */
static char *qualify_name(char mybuf[PATH_MAX], const char *partial) {
	
	char *aptr, *path, *username, *rc;
	char *userdir=NULL, *tmpname=NULL;
	int i;
	int NeedCopy = FALSE;							/* Must we copy at end?			*/
#if (defined UNIX || defined LINUX)
		struct passwd *pw;							/* Password information			*/
#endif

	if (*partial=='\"' || *partial=='\'') {	/* Starts with a quote			*/
		i = (int) strlen(partial);
		if (i>1 && partial[i-1]==*partial) {	/* Quoted string					*/
			tmpname = strdup(partial+1);			/* Copy from past the quote	*/
			tmpname[i-2] = '\0';						/* And drop last quote			*/
			partial = tmpname;						/* New partial filename			*/
			NeedCopy = TRUE;
		}
	}
		
/* (1) If there is any // or /~ tokens, they mark path restarts */
	path = (char *) partial;
	while ( (aptr = strstr(path, "//")) != NULL) path = aptr+1;
	while ( (aptr = strstr(path, "/~")) != NULL) path = aptr+1;
	if (*path != '~') {
		rc = path;

/* (3) If starts with ~ then, need to lookup the user name and info */
	} else {
		username = ++path;										/* Skip over the ~	*/
		while (*path != '/' && *path != '\0') path++;	/* Scan for dir sep	*/
#if (defined UNIX || defined LINUX)
		if (path != username) {
			strncpy(mybuf, username, path-username);		/* Really strncpy	*/
			mybuf[path-username] = '\0';
			pw = getpwnam(mybuf);
		} else {
			pw = getpwuid(getuid());
		}
		userdir = (pw != NULL) ? pw->pw_dir : NULL ;
#else
		userdir = getenv("HOMEPATH");
#endif

		if (userdir == NULL) {
			rc = (char *) partial;							/* Maybe some else understands */

		} else {													/* Okay, copy the parts */
			*mybuf = '\0';
#if ! (defined UNIX || defined LINUX)
			if ( (aptr = getenv("HOMEDRIVE")) != NULL) strcat(mybuf, aptr);
#endif
			strcat(mybuf, userdir);
			aptr = mybuf + strlen(mybuf);
			if (aptr != mybuf && (*(aptr-1)=='/' || *(aptr-1)=='\\')) *(--aptr) = '\0';
			strcpy(aptr, path);
			rc = mybuf;
			NeedCopy = FALSE;									/* No longer necessary */
		}
	}

	if (NeedCopy) rc = strcpy(mybuf, rc);				/* Copy to user space */
	if (tmpname != NULL) free(tmpname);
	return(rc);
}

/* Now, redo the routines like access to allow ~mot, but carefully undef */

int E_ACCESS(const char *path, int amode) {
	char tmp[PATH_MAX];
	return( access(qualify_name(tmp, path), amode));
}

int E_CHDIR(const char *path) {
	char tmp[PATH_MAX];
	return( chdir(qualify_name(tmp, path)));
}

int E_CHMOD(const char *path, mode_t mode) {
	char tmp[PATH_MAX];
	return( chmod(qualify_name(tmp, path), mode));
}

FILE *E_FOPEN(const char *path, const char *mode) {
	char tmp[PATH_MAX];
	return( fopen(qualify_name(tmp, path), mode));
}

FILE *E_FREOPEN(const char *path, const char *mode, FILE *stream) {
	char tmp[PATH_MAX];
	return( freopen(qualify_name(tmp, path), mode, stream));
}

DIR *E_OPENDIR(const char *dirname) {
	char tmp[PATH_MAX];
	return( opendir(qualify_name(tmp, dirname)));
}

#if (defined UNIX || defined LINUX)
long E_PATHCONF(const char *path, int name) {
	char tmp[PATH_MAX];
	return( pathconf(qualify_name(tmp, path), name));
}
#endif

int E_REMOVE(const char *path) {
	char tmp[PATH_MAX];
	return( remove(qualify_name(tmp, path)));
}

int E_RENAME(const char *old, const char *new) {
	char tmp1[PATH_MAX], tmp2[PATH_MAX];
	return( rename(qualify_name(tmp1, old), qualify_name(tmp2, new)));
}

int E_RMDIR(const char *path) {
	char tmp[PATH_MAX];
	return( rmdir(qualify_name(tmp, path)));
}

int E_STAT(const char *path, struct stat *buf) {
	char tmp[PATH_MAX];
	return( stat(qualify_name(tmp, path), buf));
}

int E_UNLINK(const char *path) {
	char tmp[PATH_MAX];
	return( unlink(qualify_name(tmp, path)));
}


#if 0														/* FOR TESTING ON OS/2 ONLY */

struct passwd *getpwnam(const char *name) {

	static char login[80]="tommy", dir[80]="/fsys/tommy";
	static struct passwd pd={login, 0, 0, dir, NULL};

	strcpy(pd.pw_name, name);
	sprintf(pd.pw_dir, "/fsys/%s", name);
	return(&pd);
}

struct passwd *getpwuid(uid_t uid) {

	static char login[80]="tommy", dir[80]="/fsys/tommy";
	static struct passwd pd={login, 0, 0, dir, NULL};

	pd.pw_uid = uid;
	return(&pd);
}

uid_t	getuid(void) {
	return(0);
}

long pathconf(const char *path, int name) {
	return(1024);
}

#endif	/* #if 0 */
