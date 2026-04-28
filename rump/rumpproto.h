/* File rump.c */
int Rump(void);
int RbsShutDown(void);

/* Energy conversion routines (macros) */
#define RBSCNNLI(i,ibuf)       ((ibuf)->first + (i))
#define RBSENERGY(chanl,ibuf)  (0.001*((chanl)*(ibuf)->kevch+(ibuf)->kev0))
#define RBSCNNLE(x1,ibuf)      ((1000.0f*(x1)-(ibuf)->kev0)/(ibuf)->kevch)
#define RBSINDEX(chanl,ibuf)   (min((ibuf)->npt-1,max(0,(int)((chanl)-(ibuf)->first+0.5))))

/* File config.c */
BOOL RbsConfig(int key);
void RbsPrintCopyright(void);

/* File sim2.c */
#ifndef TYPE_SAMPLE_DEFINED			/* May be done elsewhere also	*/
	typedef struct _SAMPLE SAMPLE;
	#define	TYPE_SAMPLE_DEFINED
#endif
	
BOOL	  RbsSimMain(int key);
void    SimReset(int level);
int     SimLoadDensityTable(char *filename);
void    SimDrawSample(void);
void    SimSetIdent(void);
int     SimLocateName(char *inam, BOOL add_it);
SAMPLE *SimDuplicateSample(SAMPLE *orig);
SAMPLE *SimCreateEmptySample(SAMPLE *sample);

/* File creatr.c */
BOOL  SimCheck(void *Sample);
void *SimCreateDetails(void *Sample, int elx, int layx);
BOOL  SimWriteProfile(void *Sample, FILE *funit);
void	SimCalcInfo(ATOMS *atomp);
BOOL  GenerateStopFoilTable(int z, int m, REAL e_min, REAL e_inc, int npt);

/* File anlyz.c */
void SimAnlyz(int z, REAL mass,
					REAL efront, REAL eback, REAL hfront, REAL hback,
					REAL qqq, REAL sigf, REAL sigb);

/* File reswork.c */
void ResStatus(int level);
void ResReset(int level);
BOOL ResRead(char *filename);

/* File pert.c */
BOOL RbsPertMain(int key);
void PertReset(int key);
void PertStatus(void);
int  PertFreeAll(void);

/* ======================================================================== */

/* File APLOT.F77: */
int RbsAplot( int key, char *toke);
SPECTRUM *RbsPlot( int key, SPECTRUM *ibf);

/* File ATOMIO.F77: */
int RbsStash( char *fname);
int RbsLoad( char *fname);
int RbsLoad1( char *fname);
BOOL RbsLoadAtomicData(char *fname);

/* File stopping.h */
BOOL RbsLoadStopTable(char *fname);
BOOL RbsLoadZieglerData(char *fname);
BOOL RbsLoadKalbitzerData(char *fname);
BOOL RbsStpCreate(int zb, REAL mb, REAL emin, REAL emax, REAL cutoff);
STOPPING_TABLE *RbsStpfind(int zb, REAL mb, REAL *e_scale, REAL e_beam);
STOPPING_POWER *RbsLookupStop(STOPPING_TABLE *table, int z);

/* File RDFOLD.F77: */
int RbsRdFileOld( char *filen, SPECTRUM *mybuff);
/* int RbsRdfold( FILE *lun ); */

/* File RDWR.F77: */
SPECTRUM *RbsGetBuf(char *prompt, SPECTRUM *idflt);
void      RbsBufferScroll(SPECTRUM *ibf);
void      RbsActive(SPECTRUM *buf);
SPECTRUM *RbsRdFile(char *path);
BOOL      RbsWriteFile(SPECTRUM *ibf, BOOL IsRewrite);
char     *RbsBeamCode(SPECTRUM *ibf, char *buffer);

/* File TPLOT.F77: */
void RbsTestMark( void );
void RbsMark( int key, REAL energy, REAL height, char *string);
int  RbsCursor(REAL *energy, REAL *height, int *button);
void axtrak( void );
void RbsChgpen( int Auto);
void RbsPldata(struct _PLOT_PARMS *parms);
void RbsSwmode(VERTMODE newlin);
void disp( void );
int RbsAxdraw( SPECTRUM *ibf);
REAL RbsRange( REAL xmin, REAL xmax, SPECTRUM *ibf);
REAL RbsGetMeV( int *key, REAL def, char *cmsg, char *kmsg);

/* File ANLYTC.F77: */
void RbsSetCorrByMatchCounts(void);
REAL RbsGetRealMass( int iz,int iso);
int RbsAnlytc( int fkey, char *toke);

/* File ATOMDO.F77: */
int RbsIdentp( char *tken2, int *Z, int *isotmp, int *ch);
int RbsIdent( char *tken, ATOMS **t, int *isotmp);
ATOMS *RbsIdentRaw( char *s2);
char *atomic_symbol( int z);
REAL atomic_mass( int z);
REAL atomic_density( int z);
REAL **atomic_isotopes( int z);
ATOMS *atomic_data( int z);
void RbsScalePrint(void);
REAL RbsSetSScale (ATOMS *atomp, REAL new_scale);

/* File BMANIP.F77: */
int RbsBmanip(int key, char *token);
REAL RbsNormK(SPECTRUM *ibf);
int RbsGetBufNum(SPECTRUM *ibf);

/* File stopp.c: */
void RbsStopp( void );
void RbsGenStopp( int elno, STOPPING_TABLE *table);

/* File ZIEGLER.F77: */
int zread1(char *filen);
int zcheck(int z1, REAL m1, int z2);
void zstop(int z1,REAL m1,int z2,REAL ee,REAL *r_se,REAL *r_sn, int units);

/* File NULL.C: */
BOOL RbsNewprf(void);
void ScrGetPos(int *i, int *j, int *l);
int  RbsSite( int key, char *toke);

/* File SITE.F77: */
