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

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"

/* -------------------------------- */
/* My share of the global variables */
/* -------------------------------- */
EXPORT const char GenplotRevisionLevel[] = "2.11 (11/11/11)" ;
EXPORT const char GenplotLinkDate[]      = __DATE__ "  "  __TIME__  ;
EXPORT const REAL GptVersionNumber       = 2.11f;

#include "revision.h"
