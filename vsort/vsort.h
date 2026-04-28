#ifdef SHORT
	typedef unsigned short BITMAPQ;
	#define	BITS_PER_MAP		16				/* Bits in each BITMAPQ	*/
	#define	SWAB(in,out,size)	swab(in,out,size)
#else
	typedef unsigned char BITMAPQ;
	#define	BITS_PER_MAP		8				/* Bits in each BITMAPQ	*/
	#define	SWAB(in,out,size)	
#endif

#define	TRANSFERINFO_VERSION	0x0201	/* Version 2.01 */
typedef struct _TransferInfo {
	int		Version;							/* Transfer info version	*/
	int		Class;							/* Device class				*/
	int		SubDevice,						/* Sub-device specified		*/
				IO_Flag;							/* I/O flag options			*/
	char		IO_Chan[DFLT_STR_SIZE];		/* I/O channel information */
	int		xperinch,yperinch;			/* Resolutions					*/
	int		xmax,ymax;						/* Maximum X,Y values		*/
	int		numpens;							/* Number of pens used		*/
} TRANSFERINFO;

#define	FL_SINGLEDOTS		0x01					/* Single dots (really!)		*/
#define	FL_NOFORMFEED		0x02					/* Suppress FF on output		*/
#define	FL_NORESET			0x04					/* Suppress RESET on output	*/
#define	FL_NOLINEWIDTH		0x08					/* Suppress wide lines			*/
#define	FL_NOCOMPRESS		0x10					/* Suppress compression			*/
#define	FL_NOBLANKS			0x20					/* Suppress blank lines			*/

#define	SingleDots	(TransferInfo.IO_Flag & FL_SINGLEDOTS)		/* Single dots (really!)		*/
#define	NoFormFeed	(TransferInfo.IO_Flag & FL_NOFORMFEED)		/* Suppress FF on output?		*/
#define	NoReset		(TransferInfo.IO_Flag & FL_NORESET)			/* Suppress RESET on output?	*/
#define	NoLineWidth	(TransferInfo.IO_Flag & FL_NOLINEWIDTH)	/* Suppress wide lines			*/
#define	NoCompress	(TransferInfo.IO_Flag & FL_NOCOMPRESS)		/* Suppress compression			*/
#define	NoBlanks		(TransferInfo.IO_Flag & FL_NOBLANKS)		/* Suppress blank lines			*/

/* -------------------------------- */
extern	TRANSFERINFO TransferInfo;
extern	LOGICAL		 ReverseBits;
extern	BITMAPQ		*BitMap;
extern	unsigned int BitMapSize;
extern	int			 MapWidth;
extern	int			 ScansPerBand;
extern	int			 NumPlanes;
extern	int			 ColorMax;
extern	int			 ColorMap[];
