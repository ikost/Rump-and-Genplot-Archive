/*  ------------------------------------------------------------------------ */
/*  --------                                              ------------------ */
/*  -------- COPYRIGHT 1989 (c) Computer Graphics Service ------------------ */
/*  --------                                              ------------------ */
/*  --------    The source code to RUMP may be freely     ------------------ */
/*  --------  modified as long as this copyright notice   ------------------ */
/*  --------          is included and unchanged.          ------------------ */
/*  ------------------------------------------------------------------------ */

/* xrumphlp.c - test routine to verify xrumphlp.h include file */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdio.h>
#include <limits.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#define	XRUMPHLP_C_SOURCE
#include "rump.h"

#include "xrumphlp.h"

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
/* My share of global externals    */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

