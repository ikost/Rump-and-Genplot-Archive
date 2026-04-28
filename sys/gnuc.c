/* **************************************************************** */
/* Homewritten posix functions for gnu C compiler                   */
/* Mike J. Uttormark / Nov. 1, 1991                                 */
/* gcc compiler                                                     */
/* **************************************************************** */

#ifdef GNU_EXTENSIONS

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>							/* where atexit should be */
#include <signal.h>							/* where raise should be */
#include <strings.h>							/* Where memmove/strerr should be */
#include <errno.h>

int	atexit(void (*func)(void));
void *memmove(void *dest, void *src, size_t length);
int	raise(int signum);
char *strerror(int e_num);
void *REALLOC(void *memblock, size_t size);

/* **************************************************************** */
/* function to register functions for execution at exit time.       */
/* **************************************************************** */

int atexit(void (*func)(void))
{
	return(on_exit(func, (char *) 0));
}

/* **************************************************************** */
/* function to move bytes to another place.  should work properly   */
/* for moves on top of oneself.                                     */
/* **************************************************************** */
void *memmove(void *dest, void *src, size_t length) {

	register unsigned char  *d, *s;

	d = (unsigned char *) dest;
	s = (unsigned char *) src;

	if (d < s)
		for(;length > 0;length--,d++,s++) *d = *s;
	else{
		d += length-1;
		s += length-1;
		for(;length > 0;length--,d--,s--) *d = *s;
	}

	return(dest);
}

/* **************************************************************** */
/* function to raise a given signal.                                */
/* **************************************************************** */

int raise(int signum) {
   return(kill(getpid(),signum));
}


/* **************************************************************** */
/* function to return string pertinant to current error.            */
/* **************************************************************** */

extern int sys_nerr;
extern char *sys_errlist[];

char *strerror(int e_num) {
	if ((e_num > 0) && (e_num < sys_nerr))
		return(sys_errlist[e_num]);
	else
		return((char *) 0);
}

/* ===========================================================================
-- function to correct the problem with the realloc() function in the gcc
-- compiler.
--
-- The problem is kludged by #defining realloc away from the function form to
-- my form:
--    #define  realloc  REALLOC
--    void *REALLOC(void *memblock, size_t size)
-- Paying the price now by having to write the function itself.
=========================================================================== */
#undef realloc

void *REALLOC(void *memblock, size_t size) {

	if (memblock == NULL) return( (void *) malloc(size));
	return( (void *) realloc(memblock, size));
}

#endif /* GNU_EXTENSIONS */
