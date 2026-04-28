/* support.c */

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
#include <unistd.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "xtrn.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#ifndef min
	#define	min(a,b)		(((a) < (b)) ? (a) : (b))
#endif

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
/* Locally defined global vars     */
/* ------------------------------- */

/* ----------------------------------------------------------------------------
-- Compress the data by run length encoding
--
-- Usage: CALL RLL_CODE(inbuf,nbytes_in,outbuf,nbytes_out)
--
-- Inputs: inbuf      - buffer containing uncompressed data
-- 	     nbytes_in  - number of bytes in inbuf
--
-- Output: outbuf     - output buffer containing compressed data
--	        nbytes_out - number of bytes in outbuf
--
-- Note: (1) In worse case, nbytes_out = 2*nbytes_in.
--           Input string replaced by <repeat><char><repeat><char>
--       (2) nbytes_out will have last null's suppressed
---------------------------------------------------------------------------- */
size_t rll_encode(unsigned char *src, size_t nbytes, unsigned char *dest) {
	
	int	iout=0;							/* Number of pairs output */
	int	matchcnt=0;						/* Presetup the loop */
	unsigned char matchchr;
	
/* ... dump trailing blank chars */
	while (nbytes > 0 && (src[nbytes-1]==0)) nbytes--;
	if (nbytes == 0) return(0);

/* ... start the run-length encoding ... */
	matchchr = *src;									/* First matching char */
	while (--nbytes != 0) {
		src++;											/* Next character in loop */
		if (*src != matchchr || matchcnt == 255) {
			*(dest++) = (char) (matchcnt);		/* Output <REPEAT><CHAR> */
			*(dest++) = matchchr;					/* And the char			 */
			iout++;										/* Keep track of numbers */
			matchchr = *src;
			matchcnt = 0;
		} else {
			matchcnt++;
		}
	}
	if (matchchr != 0x00) {
		*(dest++) = (char) (matchcnt);		/* Output <REPEAT><CHAR> */
		*(dest++) = matchchr;
		iout++;
	}
	return(2*iout);
}


/* ----------------------------------------------------------------------------
-- Compress the data using format 2 (TIFF)
--
-- Usage: size_t tiff_encode(unsigned char *src, size_t nbytes,
--                           unsigned char *dest, int flags);
--
-- Inputs: src    - buffer containing uncompressed data
--         nbytes - number of bytes in src
--         flags  - 0 ==> suppress trailing null characters in src string
--                  1 ==> retain and compress trailing nulls as well
--
-- Output: dest   - output buffer filled with compressed data
--
-- Returns: Number of bytes in the packed string
--
-- Note: (1) In worse case, output count = nbytes+(nbytes/128)
--           Input string replaced by <repeat><char><repeat><char>
--
-- It appears that TIFF is always better than the RLL method above.  Only
-- time it will not be true if the 255 repeat length for RLL is often
-- achieved, not very common at least for our graphics.
---------------------------------------------------------------------------- */
size_t tiff_encode(unsigned char *src, size_t nbytes, unsigned char *dest, int flags) {
	
	int icnt;
	unsigned char matchchr;
	unsigned char *dest_org=dest, *aptr;
	
/* ... dump trailing blank chars if requested */
	if (flags == 0) {
		while (nbytes && src[nbytes-1]==0) nbytes--;
	}
	
/* ... start the TIFF-length encoding ... */
	while (nbytes) {									/* Anything left to scan	*/
		if (nbytes <= 3) {							/* Short, just scan out		*/
			*dest++ = (int) nbytes-1;
			while (nbytes) {*dest++ = *src++; nbytes--;}
		} else if (*src == *(src+1)) {			/* Repeated char				*/
			matchchr = *src;							/* Copy the match char		*/
			icnt = 0;									/* None in pattern match	*/
			while (nbytes && icnt < 128 && *src == matchchr) {
				icnt++; nbytes--; src++;
			}
			*dest++ = -(icnt-1);						/* Pattern length			*/
			*dest++ = matchchr;						/* And the pattern		*/
		} else {
			icnt = 0;
			aptr = src;									/* Need to keep original */
			while (nbytes > 1 && icnt < 127) {
				if (*aptr == *(aptr+1) && *aptr == *(aptr+2)) break;
				icnt++; nbytes--; aptr++;
			}
			if (nbytes <= 2 && icnt+nbytes < 128) {	 /* 1 or 2, tack on */
				icnt += (int) nbytes;
				nbytes = 0;
			}
			*dest++ = icnt-1;
			while (icnt--) *dest++ = *src++;
		}
	}
	return(dest-dest_org);
}

/* ----------------------------------------------------------------------------
-- Compress the data using format 3 (DELTA)
--
-- Usage: CALL DELTA_ENCODE(inbuf,nbytes_in,outbuf,nbytes_out,lastrow)
--
-- Inputs: inbuf      - buffer containing uncompressed data
-- 	     nbytes_in  - number of bytes in inbuf
--
-- Output: outbuf     - output buffer containing compressed data
--	        nbytes_out - number of bytes in outbuf
--
-- Can be very efficient, but requires knowledge of previous line.
---------------------------------------------------------------------------- */
size_t delta_encode(unsigned char *src, size_t nbytes, unsigned char *dest, unsigned char *lastrow) {
	
	int icnt, numsame;
	unsigned char *dest_org=dest;
	
/* ... start the delta-length encoding ... */
	numsame = 0;
	while (nbytes) {
		if (*src == *lastrow) {
			nbytes--; src++; lastrow++;
			numsame++;
		} else {
			for (icnt=1; icnt<8; icnt++) {
				if (src[icnt] == lastrow[icnt]) break;
			}
			*dest++ = ((icnt-1) << 5) | min(31, numsame);
			if (numsame >= 31) {
				numsame -= 31;						/* Number we send out */
				do {
					*dest++ = min(255, numsame);
					numsame -= 255;
				} while (numsame >= 0);
			}
			while (icnt--) {
				*dest++ = *src++;
				lastrow++;	nbytes--;
			}
			numsame = 0;
		}
	}

	return(dest-dest_org);
}
