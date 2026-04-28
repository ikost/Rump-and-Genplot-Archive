/* Additional Routines needed in exchange with XRUMP */
IMPORT	SPECTRUM **RbsBuffers;
IMPORT	SPECTRUM *RbsTempBuf;
IMPORT	SPECTRUM *RbsActiveBuf;
IMPORT	int		  RbsNumBuf;

extern	int       RbsFindBuffer(int key, char *path);
extern	char     *RbsBeamCode(SPECTRUM *buf, char *buffer);
extern	int       RbsIdentp( char *tken2, int *Z, int *isotmp, int *ch);
extern	REAL      RbsGetRealMass( int iz,int iso);
extern	SPECTRUM *RbsPlot( int key, SPECTRUM *ibf);
extern	BOOL      RbsWriteFile(SPECTRUM *ibf, BOOL IsRewrite);
extern	SPECTRUM *RbsRdFile(char *path);
extern	void      RbsBufferScroll(SPECTRUM *ibf);

/* Minor hack to make bufptr(-1) work right */
#define	ALTBUF		RbsBuffers[0]
#define	MAINBUF		RbsBuffers[1]
#define	TMPBUF		RbsTempBuf

/* serial.c relevent variables */
extern const char  RumpRevisionLevel[];
extern const char  RumpLinkDate[];
extern const REAL  RumpVersionNumber;
extern const char *RumpDefaultConfigPath;
