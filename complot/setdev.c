/* SETDEV.C - Select a device and initialize */

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
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <unistd.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"								/* Needed for IO_ResolveChannel */
#include "tplot.h"
#include "complot.h"
#include "plotdefs.h"
#include "io_chan.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

typedef struct _DEVPAIR {
	CHAR name[9];
	PLOTDRIVER *pointer;
} DEVPAIR;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
PRIVATE	void	lstdev(FILE *unt);
PRIVATE	int	plcode(CHAR *request, PLT_DEVICEINFO *device);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */
LOGICAL	tab_ini(PLT_DEVICEINFO **tablet);
LOGICAL	tab_dsp(INTEGER key, INTEGER *parms, CHAR *cparms);
CHAR PlotDeviceDatFilename[PATH_MAX]="devices.dat";

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */


/* ===========================================================================
-- Routine to select a device, then initialize and fill in DeviceInfo structure
-- 
-- Usage:  int PlotSelectDevice(CHAR *name, INTEGER *status)
-- 
-- Inputs: name - ASCII device name (requires 2 character match)  Special names:
--                HElp    - List all known names and recycle
--                LIst    - List all known names and recycle
--                Special - Read from user values
-- 
-- Output: setdev - Logical indicating validity of device initialization
--         status - 16 bit status word on type of device
--                     BIT 0   - Cursor is present on this device
--                     BIT 1   - Cursor may be present (Set if bit 0 set)
--                     BIT 2   - CRT type terminal (TEK, SELANAR, etc)
--                     BIT 3   - Supports tablet commands on digitizer
--                     BIT 4-7 - Number of available pens on unit
--                               Set to 1 for units which don't care
--
-- Returns:   0 -> Successfully selected a new device
--          > 0 -> Requested no effective change
--          < 0 -> error during selection
============================================================================ */
int PlotSelectDevice(CHAR *name, INTEGER *status) {

	int statme;
	PLT_DEVICEINFO *Dev=NULL;

	if (! PlotAllocateDevice((void **)&Dev)) {		/* Can I get space?		*/
		return(-1);
	} else if ((statme=plcode(name, Dev)) != 0) {	/* Successful request?	*/
		free(Dev);
		return( (statme<0) ? -1 : +1);
	}
	
	if (DEVICE != NULL) {								/* New request, close old one */
		PlotCloseDevice();								/* Close it							*/
		free(DEVICE);										/* And free the memory			*/
	}
	DEVICE = Dev;											/* Now, make this one active	*/

	if (! PlotOpenDevice()) return(-1);				/* Can I open it		*/

	if (status != NULL) 
		*status = (DEVICE->NumberPens<<4) + DEVICE->StatusBits;
	return(0);	

}

/* ============================================================================
-- Mnemonic based call for selecting tablet devices
-- 
-- Usage: int PlotSelectTablet(NAME, IP)
-- 
-- Inputs: NAME - at least two ASCII character device name
-- 
-- Output: IPAR 1,2 -> pixels/inch X and Y
--         IPAR 3,4 -> maximum pixels X and Y
--         IPAR 5   -> key support (bit mapped)
-- 
-- Output: PlotTabletSet - Logical indicating validity of device initialization
--
-- Returns:   0 -> Successfully selected a new device
--          > 0 -> Requested no effective change
--          < 0 -> error during selection
=========================================================================== */
int PlotSelectTablet(char *name, int ip[]) {

	int statme;
	PLT_DEVICEINFO *Tab=NULL;

	if (! PlotAllocateDevice((void **)&Tab)) {		/* Can I get space?		*/
		return(-1);
	} else if ((statme=plcode(name, Tab)) != 0) {	/* Successful request?	*/
		free(Tab);
		return( (statme<0) ? -1 : +1);
	} else if (! (Tab->StatusBits & 0x08)) {			/* Can emulate tablet?	*/
		free(Tab);
		return(-1);
	}

	if (TABLET != NULL) {								/* New request, close old one */
		PlotCloseTablet();								/* Close it							*/
		free(TABLET);										/* And free the memory			*/
	}
	TABLET = Tab;											/* Now, make this one active	*/

	if (! PlotOpenTablet()) return(-1);				/* Can I open it		*/
	
	if (ip != NULL) {
		ip[0] = TABLET->xperinch;
		ip[1] = TABLET->yperinch;
		ip[2] = TABLET->xmax;
		ip[3] = TABLET->ymax;
		ip[4] = 0xFF;
	}
	return(0);

}

/* ===========================================================================
--  Logical function to return device parameters corresponding to specified
--  device name.  Intended for setdev(), but other routines can access as well.
-- 
--  Usage: int = PLCODE(CHAR *name, PLT_DEVICEINFO *device);
-- 
--  Inputs: name - 2 character name of the device
-- 
--  Output: Fills in the DeviceInfo structure with:
--						CHAR	  DriverName[DRIVER_NAME_SIZE];
-- 					CHAR    IO_Channel[IO_CHANNEL_SIZE];
-- 					INTEGER Class;
-- 					INTEGER SubDevice;
-- 					INTEGER Options;
-- 					INTEGER NumberPens;
-- 					INTEGER StatusBits
-- 
--  Note: ERROR message output on invalid device names
--
-- Returns:   0 -> all okay
--          < 0 -> bad failure
--          > 0 -> request not to do anything
============================================================================ */
PRIVATE int plcode(CHAR *request, PLT_DEVICEINFO *device) {

	FILE *unt=NULL;
	char *aptr, *optr;

	char file[PATH_MAX];							/* Space for filename */
	char name[VARNAME_STR_SIZE];				/* Requested name */
	char devname[VARNAME_STR_SIZE];			/* Device name in table */
	char devline[LONG_STR_SIZE];				/* Line from device table */
	int  len, mode, i;

	strscpy(name, request, sizeof(name));	/* Use local copy of request */

/* Open the information file */
	SysResolveDyntName(file, PlotDeviceDatFilename, sizeof(file));	/* Open file */
	if ( (unt=fopen(file, "r")) == NULL) {
		gen_err2("Device definition file missing or bad",file);
		return(-1);
	}

	while (TRUE) {
		if (LexEqual(name,"auto",4) || *name=='\0') {
			if ((aptr = getenv("GTERM")) != NULL) {
				strcpy(name, aptr);
#if !(defined OS2 || defined NT)
			} else if (getenv("DISPLAY") != NULL) {
				strcpy(name, "X");
#endif
			} else {
#if (defined NT || defined OS2)
				strcpy(name, "pm");					/* Default device */
#else
				strcpy(name, "list");
#endif
			}
		} 
		if ( (! LexEqual(name,"list",2)) && (*name != '?') ) break;
		lstdev(unt);
		UserInput("Enter desired device code: (no change) ",name, sizeof(name));
		if (*name == '\0') {fclose(unt); return(1);}
	}

/* -------------  String passed from the devices.dat file ---------------------
0         1         2         3         4         5         6         7
012345678901234567890123456789012345678901234567890123456789012345678901234567890
SPECIAL   SPECIAL   00 00  x x x x 15  UNKNOWN          Special request code
HPDISK    HP-GL     06 01  x x     06  XON*DISK*QUERY   HP 7550 disk output
---------------------------------------------------------------------------- */
	while (fgets(devline, sizeof(devline), unt) != NULL) {
		if (*devline == ' ' || *devline == '\n' || *devline == '\0' || (strncmp(devline,"/*",2) == 0) ) continue;
		if (sscanf(devline, "%s", devname) != 1) goto BadLine;
		if (stricmp(devname, "@END") == 0) break;
		if (LexEqual(devname, name, (int) strnblen(name))) {	/* Have a match!!! */
			if (sscanf(&devline[10], "%s %d %d",  device->DriverName, &device->SubDevice, &device->Options) != 3) goto BadLine;
			if (sscanf(&devline[35], "%d",        &device->NumberPens) != 1) goto BadLine;
/* Fixed ... LexParseLine(IO_Channel, sizeof(IO_Channel), &devline[39], NULL); */
			aptr = devline+39;									/* Start of I/O channel */
			len  = sizeof(device->IO_Channel);
			optr = device->IO_Channel;
			mode = 0;
			while (isspace(*aptr)) aptr++;
			while (len > 1) {
				if (mode == 0 && isspace(*aptr))    break;
				if (*aptr == '\n' || *aptr == '\0') break;
				if (*aptr == '\'') mode ^= 0x01;
				if (*aptr == '\"') mode ^= 0x02;
				*(optr++) = *(aptr++); len--;
			} 
			*optr = '\0';
			i = 0;
			if (devline[27] != ' ') i |= 0x01;
			if (devline[29] != ' ') i |= 0x02;
			if (devline[31] != ' ') i |= 0x04;
			if (devline[33] != ' ') i |= 0x08;
			device->StatusBits = i;
			if (stricmp(device->DriverName, "special") == 0) {
				UserInput("Primary device driver name: ",devline, sizeof(devline));
				if (sscanf(devline,"%19s", device->DriverName) != 1) goto BadLine;
				UserInput("Minor dev # and sub-mode: "  ,devline, sizeof(devline));
				device->SubDevice = 0; device->Options = 0;
				sscanf(devline,"%d %i", &device->SubDevice, &device->Options);
				UserInput("I/O channel information: "   ,devline, sizeof(devline));
				strscpy(device->IO_Channel, devline, sizeof(device->IO_Channel));
			}
			fclose(unt);
			IO_ResolveChannel(device->IO_Channel, sizeof(device->IO_Channel));
			return(0);
		}
	}

	ERRprintf("ERROR: Unrecognized device name (%s)\n", name);
	lstdev(unt);
	fclose(unt);
	return(-1);


BadLine:
	ERRprintf("ERROR: Syntax error in %s (%s)\n", PlotDeviceDatFilename, devline);
	fclose(unt);
	return(-1);
}


/* ===========================================================================
--  Routine to list out the various available devices to the screen
-- 
--  Usage:  lstdev(unt)
-- 
--  Inputs: FUNIT - Fortran unit opened with 'DEVICES.DAT' file
============================================================================ */
PRIVATE void lstdev(FILE *unt) {

	CHAR devline[LONG_STR_SIZE];
	
	rewind(unt);										/* Back to beginning */
	puts("\nCurrently configured devices:");
	while (fgets(devline, sizeof(devline), unt) != NULL) {
		if (strncmp(devline,"@END",4) == 0) break;
		if (*devline != ' ' && (strncmp(devline,"/*",2) != 0) ) fputs(devline,stdout);
	}
	fputs("\n",stdout);
	rewind(unt);
	return;
}

/* ===========================================================================
-- The IO_ResolveChannel takes a string containing a IO channel and resolves
-- potential requests for unique filenames based on given pattern.  The string
-- is then rewritten to the string and returned.
--
-- Usage:  void IO_ResolveChannel(char *name, size_t maxlen);
--
-- Inputs: name   - current I/O channel string
--         maxlen - defined length of name
--
-- Output: name   - potentially modified string
--
-- Returns: void
=========================================================================== */
void IO_ResolveChannel(char *given, size_t maxlen) {

#if (defined OS2 || defined NT)
	char dir[PATH_MAX], ext[PATH_MAX];
#endif

	static unsigned int pid=0;
	char szTmp[LONG_STR_SIZE], fname[PATH_MAX], *aptr;
	int retries=100;

	if (pid == 0) pid = ((unsigned int) getpid()) % 1000U;

	strlwr(strscpy(szTmp, given, sizeof(szTmp)));	/* Copy for lower case */

	if ( (aptr = strstr(szTmp, "*query")) != NULL) {
		LexPromptStr(fname, sizeof(fname), "Enter pathname for plotting file: ");
		strcpy(given+(aptr-szTmp)+1, fname);

	} else if ( (aptr = strstr(szTmp, "*cmdline")) != NULL) {
		if (! LexGetFileP(fname, sizeof(fname), "Filename (default): ")) *fname = '\0';
		strcpy(given+(aptr-szTmp)+1, fname);

	} else {													/* Check for format request */
		aptr = strrchr(given, '*');					/* Find last * char         */
		aptr = (aptr==NULL) ? given : aptr+1 ;		/* If not there, use string */
		if (strchr(aptr, '%') != NULL) {				/* Is there a % in string?	 */
			do {												/* Use as format string		 */
				sprintf(szTmp, aptr, pid);				/* Encode string				 */
				pid = (pid+1)%1000U;						/* Increment pid valud		 */
#if (defined OS2 || defined NT)
				SysSplitPath(szTmp, dir, fname, ext);
				fname[8] = ext[4] = '\0';				/* Make legal for FAT disks */
				SysMakePath(szTmp, dir, fname, ext);
#endif
			} while (retries-- && access(szTmp, F_OK)==0);
			strcpy(aptr, szTmp);							/* Finally, replace value	*/
		}
	}

	return;
}
