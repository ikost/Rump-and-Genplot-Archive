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
#include <math.h>
#include <limits.h>
#include <unistd.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "rump.h"
#include "xsect.h"

#ifndef FALSE
	#define FALSE 0
#endif

BOOL RbsNewprf(void) {
	ERRprintf("OOPS: Didn't think anyone used this routine anymore - sorry not implemented\n");
	return(FALSE);
}

/* --- RbsSite has no additional commands for the time being --- */
int RbsSite(int key, char *token) {
	return(FALSE);
}

void ScrGetPos(int *i, int *j, int *l) {*i=*j=*l=0; return;}
