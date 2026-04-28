/* ---------------------------------------------------------------------------
-- MAX_ENTRIES  - maximum # of entries in a single IFD directory
-- TIFF_COOLKIE - code used to mark valid structures (Ian's birthday)
--------------------------------------------------------------------------- */
#define	MAX_ENTRIES	100	
#define	TIFF_COOKIE	0x021795		/* Magic cookie to mark valid TIFF *		*/

/* --------------------------------------------------------------------------
-- Inidividual directory entries in TIFF (IFD entry).  Defined by standard
-------------------------------------------------------------------------- */
typedef struct _TIFF_ENTRY {
   USHORT tag;							/* Entry tag - purpose of entry				*/
	USHORT type;						/* Type (short/long/ascii/byte/rational)	*/
	ULONG  count;						/* Number of data items for this entry		*/
	union {								/* Union of various types						*/
		ULONG offset;					/* An offset in file								*/
		ULONG lval;						/* A long value									*/
		USHORT sval[2];				/* Two short values								*/
	} value;
} TIFF_ENTRY;
	
/* -----------------------------------------------------------------------
-- FILE * type handle for TIFF.  Includes space to store temp directory
----------------------------------------------------------------------- */
typedef struct _TIFF {
	ULONG	magic;									/* Magic cookie validator			*/
	enum {LOCAL_FILE, PASSED_FILE} type;	/* We opened, or passed open		*/
	FILE *funit;									/* Opened file unit					*/
	ULONG  ifd_pointer;							/* Pointer to last IFD pointer	*/
	USHORT ifd_count;								/* Number of IFD entries so far	*/
	TIFF_ENTRY entries[MAX_ENTRIES];			/* Temporary space for dir entry	*/
} TIFF;

/* -------------
-- Prototypes
--------------*/
TIFF *TiffOpenFile(char *name, FILE *funit);
int TiffWriteIFD(TIFF *tf);
int TiffCloseFile(TIFF *tf);

int TiffPutLong(TIFF *tf, USHORT tag, ULONG value);
int TiffPutShortArray(TIFF *tf, USHORT tag, USHORT count, USHORT *buf);
int TiffPutLongArray(TIFF *tf, USHORT tag, USHORT count, ULONG *buf);
int TiffPutShort(TIFF *tf, USHORT tag, USHORT value);
int TiffPut2Short(TIFF *tf, USHORT tag, USHORT val1, USHORT val2);
int TiffPutString(TIFF *tf, USHORT tag, char *string);
int TiffPutRational(TIFF *tf, USHORT tag, ULONG numerator, ULONG denominator);

#define	TF_NOPREALIGN		0x01		/* Bit def'n for flag in TiffWriteData */
#define	TF_NOPOSTALIGN		0x02		/* Bit def'n for flag in TiffWriteData */
ULONG TiffWriteData(void *buf, size_t size, size_t num, TIFF *tf, int flags);

/* -------------
-- TIFF tags
------------- */
#define	BITSPERSAMPLE					0x102
#define	COLORMAP							0x140
#define	COLORRESPONSECURVES			0x12D
#define	COMPRESSION						0x103
#define	GRAYRESPONSECURVE				0x123
#define	GRAYRESPONSEUNIT				0x122
#define	IMAGELENGTH						0x101
#define	IMAGEWIDTH						0x100
#define	NEWSUBFILETYPE					0xFE
#define	PHOTOMETRICINTERPRETATION	0x106
#define	PLANARCONFIGURATION			0x11C
#define	PREDICTOR						0x13D
#define	RESOLUTIONUNIT					0x128
#define	ROWSPERSTRIP					0x116
#define	SAMPLESPERPIXEL				0x115
#define	STRIPBYTECOUNTS				0x117
#define	STRIPOFFSETS					0x111
#define	XRESOLUTION						0x11A
#define	YRESOLUTION						0x11B
#define	ARTIST							0x13B
#define	DATETIME							0x132
#define	HOSTCOMPUTER					0x13C
#define	IMAGEDESCRIPTION				0x10E
#define	MAKE								0x10F
#define	MODEL								0x110
#define	SOFTWARE							0x131
#define	GROUP3OPTIONS					0x124
#define	GROUP4OPTIONS					0x125
#define	DOCUMENTNAME					0x10D
#define	PAGENAME							0x11D
#define	PAGENUMBER						0x129
#define	XPOSITION						0x11E
#define	YPOSITION						0x11F
#define	CELLLENGTH						0x109		/* Not recommended */
#define	CELLWIDTH						0x108		/* Not recommended */
#define	FILLORDER						0x10A		/* Not recommended */
#define	FREEBYTECOUNTS					0x121		/* Not recommended */
#define	FREEOFFSETS						0x120		/* Not recommended */
#define	MAXSAMPLEVALUE					0x119		/* Not recommended */
#define	MINSAMPLEVALUE					0x118		/* Not recommended */
#define	SUBFILETYPE						0xFF		/* Not recommended */
#define	ORIENTATION						0x112		/* Not recommended */
#define	THRESHOLDING					0x107		/* Not recommended */
#define	WHITEPOINT						0x13E
#define	PRIMARYCHROMATICITIES		0x13F

/* ----------------------------------------------------
-- Number formats from the TIFF standard definition
---------------------------------------------------- */
#define	T_BYTE							1			/* Single byte format	*/
#define	T_ASCII							2			/* Encoded ASCII string */
#define	T_SHORT							3			/* Two byte integers		*/
#define	T_LONG							4			/* Four byte integers	*/
#define	T_RATIONAL						5			/* Ratio of two LONGS	*/
