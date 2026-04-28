/* GptBitmap.c */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#if defined MSC70
	#include <windows.h>								/* Standard windows headers */
	#include <vfw.h>									/* Need Video-for-Windows headers */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"
#include "tplot.h"
#include "gptxtrn.h"
#include "gptdef.h"				/* For GPTUserRead and Write */

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
/* Locally defined global vars     */
/* ------------------------------- */

/*============================================================================
-- Function to create a BitMap structure from a surface function
--
-- Usage: void *GptCreateBitmap(SURFACE *surf);
--
-- Inputs: surf - pointer to a valid SURFACE structure
--
-- Output: none
--
-- Return: Pointer to allocated space with valid bitmap.  Data immediately
--         follows the header information.  On error, returns NULL.  The
--         pointer type is (void *) to allow routines to use without having
--         to understand the internal information
============================================================================ */
#pragma pack(2)

typedef struct _GPT_BITMAPFILEHEADER {
	UINT2	bfType;								/* Must be "BM" == 19778					*/
	UINT4	bfSize;								/* File size in bytes						*/
	UINT2	bfReserved[2];						/* Unused - must be zero					*/
	UINT4	bfOffBits;							/* Offset to where bitmap data starts	*/
} GPT_BITMAPFILEHEADER;

typedef struct _GPT_BITMAPINFOHEADER {
	UINT4 biSize;								/* Size of this header in bytes			*/
	UINT4	biWidth;								/* Width of image in pixels				*/
	UINT4 biHeight;							/* Height of image in pixels				*/
	UINT2	biPlanes;							/* # planes of target device (must=0)	*/
	UINT2	biBitCount;							/* Number of bits per pixel				*/
	UINT4	biCompression;						/* Compression type (0 => none)			*/
	UINT4	biSizeImage;						/* Bytes in image data (0 ok if uncompressed) */
	UINT4	biXPelsPerMeter;					/* Obvious, but usually zero				*/
	UINT4	biYPelsPerMeter;					/* Obvious, but usually zero				*/
	UINT4	biClrUsed;							/* # colors used. 0 => use biBitCount	*/
	UINT4	biClrImportant;					/* # colors important.  0 => all			*/
} GPT_BITMAPINFOHEADER;

typedef struct _GPT_RGBQUAD {				/* Order of bytes for a color index		*/
	BYTE rgbs[4];
} GPT_RGBQUAD;

#pragma pack()

void *GptCreateBitmap(SURFACE *surf) {

	int i,j,ineed, nrow,ncol, rgb;
	UCHAR *data;
	REAL *z;
	GPT_BITMAPINFOHEADER *bmih;

	nrow = GptSurface->nrow;
	ncol = GptSurface->ncol;

	ineed = sizeof(*bmih) + nrow*ncol*3;	/* Total space required for header and data */
	if (bmih = calloc(ineed, 1)) == NULL) {
		ERRprintf("ERROR: Unable to allocate a bitmap info header structure\n");
		return(NULL);
	}
	bmih->biSize = sizeof(*bmih);				/* Size of this header in bytes			*/
	bmih->biWidth = ncol;						/* Width of image in pixels				*/
	bmih->biHeight = nrow;						/* Height of image in pixels				*/
	bmih->biPlanes = 1;							/* # planes of target device (must=0)	*/
	bmih->biBitCount = 24;						/* Number of bits per pixel				*/
	bmih->biCompression = 0;					/* Compression type (0 => none)			*/
	bmih->biSizeImage = nrow * ncol * 3;	/* Image data Bytes (0 ok if uncompressed) */
	bmih->biXPelsPerMeter = 0;					/* Obvious, but usually zero				*/
	bmih->biYPelsPerMeter = 0;					/* Obvious, but usually zero				*/
	bmih->biClrUsed = 0;							/* # colors used. 0 => use biBitCount	*/
	bmih->biClrImportant = 0;					/* # colors important.  0 => all			*/

	z = surface->z;
	data = ((BYTE *) bmih) + sizeof(*bmih);	/* Where does data start */
	for (i=0; i<=nrow; i++) {
		for (j=0; j<=ncol; j++) {
			rgb = GVSelectContinuumColor(z[i*ncols+j], zminz, zmaxz);	/* zminz,zmaxz defined in gptdef.h */
			*data++ = B_FROM_RGB(rgb);			/* Set into correct color format */
			*data++ = G_FROM_RGB(rgb);
			*data++ = R_FROM_RGB(rgb);
		}
	}

	{
		GPT_BITMAPFILEHEADER bmfh;
		FILE *funit;

		bmfh.bfType = 19778;
		bmfh.bfSize = sizeof(bmfh)+ineed;
		bmfh.bfReserved[0] = bmfh.bfReserved[1] = 0;
		bmfh.bfOffBits = sizeof(bmfh)+sizeof(*bmih);
		funit = fopen("test.bmp", "wb");
		fwrite(bmfh, 1, sizeof(bmfh), funit);
		fwrite(bmfh, 1, ineed, funit);
		fclose(funit);
	}

	return (void *) bmih;
}
