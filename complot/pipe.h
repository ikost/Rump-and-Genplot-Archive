/*
 * pipe.h:  definitions for the pipe driver and children
 */

#ifndef O_BINARY						/* We use O_BINARY, define if necessary */
	#define O_BINARY	0
#endif

#define	SLEEPTIME			1				/* Granularity somewhat LARGER -- but */
#define	MAXSLEEPS			4				/* We wait maximum of 4 seconds		  */
#define	FIFO_MODE			S_IRWXU		/* named pipe: rwx for user at least  */

#if (defined CSET2 || defined MSC60)	/* Superceeds POSIX name format */
	#define	INBOUND_NAME_FORM		"\\pipe\\pd_%5.5i.%3.3s"
	#define	OUTBOUND_NAME_FORM	"\\pipe\\pd_%5.5i.%3.3s"
#elif (defined MSC70)
	#define	INBOUND_NAME_FORM		"\\\\.\\pipe\\pd_%5.5i.%3.3s"
	#define	OUTBOUND_NAME_FORM	"\\\\.\\pipe\\pd_%5.5i.%3.3s"
#else
	#define	INBOUND_NAME_FORM		"/tmp/t_dev%5.5i.%3.3s"
	#define	OUTBOUND_NAME_FORM	"/tmp/t_dsp%5.5i.%3.3s"
#endif
