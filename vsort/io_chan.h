/* ... Modes of I/O handling ... */
#define	IO_FILE		0x01					/* File I/O						*/
#define	IO_PIPE		0x02					/* Pipe IO to new process	*/
#define	IO_RS232		0x03					/* Out communication ports */
#define	IO_SPOOL		0x04					/* Sort sort of spooler		*/
#define	IO_GPIB		0x05					/* GPIB output					*/
#define	IO_LPR		0x06					/* Via system LPR command	*/

/* ... Control Options */
#define	IOC_CRLF			0x01				/* Send CRLF on each buffer output	*/
#define	IOC_NOCRLF		0x00				/* Don't send CRLF with buffer		*/
#define	IOC_TEXT			0x00				/* Output is text	format				*/
#define	IOC_BINARY		0x10				/* Make sure opened as binary mode	*/
#define	IOC_BUFFERED	0x20				/* Handle as buffered output			*/
#define	IOC_NOBUFFER	0x00				/* Do not buffer unless necessary	*/

/* ... Flow control values */
#define	IOF_NOFLOW	0x00					/* No flow control (default)			*/
#define	IOF_DTR		0x01					/* Use Data Terminal Ready				*/
#define	IOF_ENQ		0x02					/* Use Enquire Acknowledge				*/
#define	IOF_XON		0x03					/* Use Xon/Xoff							*/

/* ... Abilities Flag ... */
#define	IOA_WRITE				0x01		/* Device is writable					*/
#define	IOA_READ					0x02		/* Device is readable					*/
#define	IOA_SEEK					0x04		/* Device is seekable					*/
#define	IOA_DEVICE				0x08		/* Device is a device (! file)		*/
#define	IOA_TERMINAL			0x10		/* Device is controlling terminal	*/
#define	IOA_FCLOSE				0x20		/* Device uses fclose() to close		*/
#define	IOA_PCLOSE				0x40		/* Device uses pclose() to close		*/
#define	IOA_IGNORE_SETVBUF	0x1000	/* Don't ever do a setvbuf() call	*/

typedef struct _IOBLK {
	int	type;						/* Type of I/O channel (RS232, FILEIO)		*/
	FILE  *funit;					/* Either a FILE* descriptor					*/
	int	Abilities;				/* Capabilities of the driver					*/
	int	FlowControl;			/* Flow control bits								*/
	int	Options;					/* Options											*/
	int	BufSize;					/* Buffer sizes (estimate)						*/
	char  Program[20];			/* Program initiating this request			*/
	char	Channel[10];			/* I/O channel name as requested				*/
	char	Parms[DFLT_STR_SIZE]; /* I/O parameters (filename etc)			*/
} IO_BLOCK;

/* ---------------------------------------------------------------------------
-- At this moment in time, these are mostly calls to the equivalent ANSI
-- standard.  However, if I get in the habit of using IO_fputs, I can easily
-- extend it in the future if I am unable to implement all devices as standard
-- streams.
--------------------------------------------------------------------------- */
IO_BLOCK *IO_OpenChannel (char *program, char *iochan, int options, int flow);
void      IO_CloseChannel(IO_BLOCK *io);
void		 IO_ResolveChannel(char *name, size_t maxlen);
void		 IO_SetBuffer(IO_BLOCK *io, int bufsize);
int		 IO_fprintf(IO_BLOCK *io, const char *format, ...);
int		 IO_GetInput(IO_BLOCK *io, char *buf, int nchars, int term, char *prompt);

#define	 IO_fputc(c,io)							fputc(c, io->funit)
#define	 IO_fputs(str, io)						fputs(str, io->funit)
#define	 IO_fgetc(io)								fgetc(io->funit)
#define	 IO_fwrite(buf, size, count, io)		fwrite(buf, size, count, io->funit)
#define	 IO_fflush(io)								fflush(io->funit)
#define	 IO_fseek(io, offset, origin)			fseek(io->funit, offset, origin)
#define	 IO_ftell(io)								ftell(io->funit)
#define	 IO_fgetpos(io, pos)						fgetpos(io->funit, pos)
#define	 IO_fsetpos(io, pos)						fsetpos(io->funit, pos)
#define	 IO_ferror(io)								ferror(io->funit)
