/* sys.c - system file and OS control routines */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#if (defined CSET2 || defined WATCOM || defined MSC60)
	#define	INCL_WINSHELLDATA
	#define	INCL_DOSSIGNALS
	#define	INCL_DOSPROCESS
	#define	INCL_DOS
	#include <os2.h>
	#ifdef MSC60
		#include <process.h>
		#include <direct.h>
	#endif
#elif defined MSC70
	#include <windows.h>
	#include "msi.h"							/* Required for serial number operations */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#define	SYS_OS_C_SOURCE
#include "mytypes.h"
#include "extends.h"
#include "defaults.h"

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
/* My share of the global vars     */
/* ------------------------------- */
EXPORT VOLATILE_SIG_ATOMIC_T SysBreakFlag  = FALSE;	/* Break pressed */
EXPORT VOLATILE_SIG_ATOMIC_T SysBreakCount = 0;			/* Count of breaks pressed */
EXPORT VOLATILE_SIG_ATOMIC_T	SysXtermCharCount;		/* Way to determine chars waiting */
EXPORT INT32 SysDebugFlag = 0;								/* No debug enabled */

/* ------------------------------- */
/* Private global vars             */
/* ------------------------------- */


/* ===========================================================================
-- Modified access() subroutine to look for access mode *and* ensure that it
-- really is a file and not a subdirectory.
=========================================================================== */
static int Access(const char *path, int amode) {

	int rc;
	struct stat buf;

/* Use standard access to try first, followed by stat to be sure */
	if ( (rc = access(path, amode)) != 0) return(rc);
	if ( (rc = stat(path, &buf)) != 0) return(rc);	/* If it fails, fail also */

	rc = S_ISREG(buf.st_mode) ? 0 : -1;
	errno = ENOENT;
	return(rc);
}


/* ===========================================================================
-- Usage: BOOL SysCheckLicense(char *product);
--
-- Inputs: product - either "RUMP" or "GENPLOT"
--
-- Output: none
--
-- Returns: Returns true if this is a valid license
=========================================================================== */
EXPORT char SysSerialNumber[32] = "OS2-mot-v.0-010";
EXPORT char SysSerialNumberEx[32] = "OS2-mot-v.0-010";

#ifdef NT

static HKEY RegistryHive = HKEY_CURRENT_USER;

typedef struct _LICENSEDATA {
	int version;					/* Version of this structure			*/
	char *serial;					/* Serial number from registry		*/
	char *product;					/* Product name (RUMP or GENPLOT)	*/
	int  errorcode;				/* Extended error information			*/
	int  bUseSerial;				/* Number of bytes in UseSerial		*/
	char UseSerial[32];			/* Returned serial number chars		*/
	char BetaCRC[5];				/* Beta test match pattern				*/
	time_t expiration;			/* Expiration data of beta test		*/
} LICENSEDATA;

BOOL SysCheckLicense(char *product) {
	char filename[PATH_MAX], serial[LONG_STR_SIZE], verified[LONG_STR_SIZE];

	int (*ValidateKey)(LICENSEDATA *data);
	LICENSEDATA data, bestdata;

	DWORD isize1,isize2, itype1,itype2;
	LONG  rc,rc1,rc2;

	int i, BestIval, ival1, ival2;
	HMODULE hmod;
	HKEY hkey;
	BOOL rcode, GotHive;

/* Lookup the DLL with the serial number verification code */
	ValidateKey = NULL;
	TTYprintf("SERIAL NUMBER VERIFICATION\n");
	TTYprintf("  Looking for serialno.dll ... ");
	rcode = SysResolveDyntName(filename, "serialno.dll", sizeof(filename));
	if (! rcode) {
		TTYprintf("does not exist\n"); 
	} else {
		TTYprintf("found %s\n    Loading ... ", filename);
		hmod = LoadLibrary(filename);
		if (hmod == NULL) {
			TTYprintf("failed\n");
		} else {
			TTYprintf("successful ... ");
			ValidateKey = (int (*)(LICENSEDATA *)) GetProcAddress(hmod, "ValidateKey");
			if (ValidateKey == NULL) {
				TTYprintf("entrypoint not found\n");
				FreeLibrary(hmod);
			} else {
				TTYprintf("entry found\n");
			}
		}
	}
	TTYprintf("\n");

	if (ValidateKey == NULL) {
		ERRprintf(
"ERROR: No valid license file (serialno.dll) was found.\n"
"\n"
"  This error may be the result of an aborted or improper installation.  If\n"
"  you think this message is in error, check for the file serialno.dll in\n"
"  the executable install directory.  By default, this is the system path\n"
"  \\Program Files\\CGS\\Genplot\\NT.\n"
"\n"
"  A test and evaluation serial number can be obtained from www.genplot.com\n"
"  or by contacting CGS directly.  Evaluation licenses expire in 30-180 days\n"
"  but may be renewed.\n");
		sleep(20);
		return(FALSE);
	}

/* -----------------------------------------------------------------------------
-- (1) Scan through several possible keys where the serial number can be stored
-- (2) Look in each key for a value "Serial" and "VerifiedSerial"
-- (3) If Serial is valid (ival=0) and VerifiedSerial does not match, rewrite
-- (4) If VerifiedSerial is valid and Serial does not match, rewrite
----------------------------------------------------------------------------- */
	BestIval = 99;													/* Default return value */

	/* Look in both HKEY_CURRENT_USER and HKEY_LOCAL_MACHINE for serial number key */
	GotHive = FALSE;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Computer Graphic Service\\Genplot/Rump\\2.0", 0, KEY_QUERY_VALUE, &hkey) == ERROR_SUCCESS) {
		isize1 = sizeof(serial);
		if (RegQueryValueEx(hkey, "Serial", NULL, &itype1, serial, &isize1) == ERROR_SUCCESS) {
			RegistryHive = HKEY_CURRENT_USER;
			GotHive = TRUE;
		} else {
			RegCloseKey(hkey);
		}
	}
	if (! GotHive) {
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Computer Graphic Service\\Genplot/Rump\\2.0", 0, KEY_QUERY_VALUE, &hkey) == ERROR_SUCCESS) {
			isize1 = sizeof(serial);
			if (RegQueryValueEx(hkey, "Serial", NULL, &itype1, serial, &isize1) == ERROR_SUCCESS) {
				RegistryHive = HKEY_LOCAL_MACHINE;
				GotHive = TRUE;
			} else {
				RegCloseKey(hkey);
			}
		}
	}
	if (! GotHive) {
		ERRprintf("ERROR: Can't find Genplot/Rump in the registry.  Nothing works\n");
		FreeLibrary(hmod);
		return(FALSE);
	}

	isize1 = sizeof(serial);
	rc1 = RegQueryValueEx(hkey, "Serial", NULL, &itype1, serial, &isize1);
	isize2 = sizeof(verified);
	rc2 = RegQueryValueEx(hkey, "VerifiedSerial", NULL, &itype2, verified, &isize2);
	RegCloseKey(hkey);

	ival1 = 99;											/* Assume invalid */
	if (rc1 == ERROR_SUCCESS && itype1 == REG_SZ) {

#ifdef OBSOLETE_CODE_THAT_I_HOPE_I_NEVER_NEED_AGAIN
/* On new installation, this may be just a pointer to the application installation ID */
		if (*serial == '{') {						/* Oh shit - new installation */
			char chUserName[256]="", chCmpName[256]="", chSrNum[256]="";
			int dwUserName=256, dwCmpName=256, dwSrNum=256;
			USERINFOSTATE rc;
			rc = MsiGetUserInfo(serial, chUserName, &dwUserName, chCmpName, &dwCmpName, chSrNum, &dwSrNum);
			if (rc != USERINFOSTATE_PRESENT) {
				char *errmsg="Unknown error";
				char szTmp[4096];																	/* Big buffer */
				if (rc == USERINFOSTATE_ABSENT)     errmsg = "USERINFOSTATE_ABSENT: Some or all of the user information is absent";
				if (rc == USERINFOSTATE_INVALIDARG) errmsg = "USERINFOSTATE_INVALIDARG: On eof the function parameters was invalid";
				if (rc == USERINFOSTATE_MOREDATA)   errmsg = "USERINFOSTATE_MOREDATA: A buffer is too small to hold the requested data";
				if (rc == USERINFOSTATE_UNKNOWN)    errmsg = "USERINFOSTATE_UNKNOWN: The product code does not identify a known product";
				sprintf(szTmp, "The attempt to look up the serial number following installation has failed.  The developers need to know"
								   "how and when this happens - really unfortunate.  The following debug information will help.  To fix the,"
								   "password manually, run regedit and set the serial number entry.  It is located in either HKEY_CURRENT_USER"
								   "or HKEY_LOCAL_MACHINE under Software\\Computer Graphic Service\\Genplot/Rump\\2.0.  The key is serial and"
								   "should be set to the correct text string.\n\n"
								   "     MisGetUserInfo error: %s\n"
									"     Product Code: %s\n"
								   "     dwUserName: %d    chUserName: %s\n"
								   "     dwCmpName: %d     chCmpName:  %s\n"
								   "     dwSrNum: %d       chSrNum:    %s\n",
						  errmsg, serial, dwUserName, chUserName, dwCmpName, chCmpName, dwSrNum, chSrNum);
				MessageBox(NULL, szTmp, "Serial Number Lookup Failure", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
			}
			strscpy(serial, chSrNum, sizeof(serial));
			if (RegOpenKeyEx(RegistryHive, "Software\\Computer Graphic Service\\Genplot/Rump\\2.0", 0, KEY_SET_VALUE, &hkey) == ERROR_SUCCESS) {
				RegSetValueEx(hkey, "Serial", 0, REG_SZ, serial, strlen(serial)+1);
				RegCloseKey(hkey);
			}
		}
#endif
		data.version = 0x0001;						/* Valid for version 1 only */
		data.serial  = serial;
		data.product = product;
		data.bUseSerial = sizeof(data.UseSerial);
		strcpy(data.BetaCRC, "kben");				/* Corresponds to BETA-02-0000 */
		ival1 = ValidateKey(&data);
		if (ival1 < BestIval) {						/* Keep most permissive */
			BestIval = ival1;
			bestdata = data;
			strscpy(SysSerialNumberEx, data.serial, sizeof(SysSerialNumberEx));
			strscpy(SysSerialNumber, data.UseSerial, sizeof(SysSerialNumber));
		}
	}

	ival2 = 99;											/* No validated serial number */
	if (rc2 == ERROR_SUCCESS && itype2 == REG_SZ) {
		data.version = 0x0001;						/* Valid for version 1 only */
		data.serial  = verified;
		data.product = product;
		data.bUseSerial = sizeof(data.UseSerial);
		strcpy(data.BetaCRC, "kben");				/* Corresponds to BETA-02-0000 */
		ival2 = ValidateKey(&data);
		if (ival2 < BestIval) {						/* Keep most permissive */
			BestIval = ival2;
			bestdata = data;
			strscpy(SysSerialNumberEx, data.serial, sizeof(SysSerialNumberEx));
			strscpy(SysSerialNumber, data.UseSerial, sizeof(SysSerialNumber));
		}
	}
	FreeLibrary(hmod);

/* Check if we are compatible */
	if (ival1 < 0 || ival2 < 0) {
		ERRprintf("ERROR: License file is no longer compatible with this code revision (FATAL)\n");
		FreeLibrary(hmod);
		return(FALSE);
	}

/* Modify the registry to keep verified serial numbers stored for updates */
	if (ival1 == 0) {
		if (ival2 != 0 || strcmp(serial, verified) != 0) {
			if ( (rc = RegOpenKeyEx(RegistryHive, "Software\\Computer Graphic Service\\Genplot/Rump\\2.0", 0, KEY_SET_VALUE, &hkey)) == ERROR_SUCCESS) {
				RegSetValueEx(hkey, "VerifiedSerial", 0, REG_SZ, serial, (int) strlen(serial)+1);
				RegCloseKey(hkey);
			}
		}
	} else if (ival2 == 0) {
		if ( (rc = RegOpenKeyEx(RegistryHive, "Software\\Computer Graphic Service\\Genplot/Rump\\2.0", 0, KEY_SET_VALUE, &hkey)) == ERROR_SUCCESS) {
			char szTmp[LONG_STR_SIZE];
			RegSetValueEx(hkey, "Serial", 0, REG_SZ, verified, (int) strlen(verified)+1);
			RegCloseKey(hkey);
			sprintf(szTmp, "Previous product serial number\n  %s\nhas been recognized and saved", verified);
			MessageBox(NULL, szTmp, "Serial Number Update", MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
		}
	}

/* Check the response */
	switch (BestIval) {
		case 0:
			rcode = TRUE;
			break;

		case 1:
			strscpy(SysSerialNumber, "Test & Evaluation", sizeof(SysSerialNumber));
			TTYprintf("\n\n"
"==========================================================================\n"
"       Just reminding you that this is not a fully licensed copy :-)      \n"
"       This license expires %s"
"==========================================================================\n",
						asctime(localtime(&bestdata.expiration)) );
			sleep(2);
			rcode = TRUE;
			break;

		case 2:
			strscpy(SysSerialNumber, "*Expired license*", sizeof(SysSerialNumber));
			ERRprintf(
"ERROR: Test and evaluation license expired on %s\n"
"\n"
"  A new copy of the test license can be obtained from www.genplot.com or\n"
"  or by contacting CGS directly.  This expired copy will run, but\n"
"  you will be forced to suffer through a 10 second timeout ...\n"
"\n"
"  Why do I have the code expire?\n"
"    (1) An annoyance to encourage you to cough up for a real license\n"
"        (which then supports my diet coke habit).\n"
"    (2) The code changes often.  Licensed users get notification of major\n"
"        problems and are encouraged to update their code.  The expiration\n"
"        of test licenses minimizes the number of potentially bad copies out\n"
"        in the research community.\n"
"    (3) Licensed users are much more likely to complain about errors and\n"
"        problems -- meaning that they get fixed all that much sooner.\n"
"\n"
"\n"
"Okay - prepare to wait ... 00",
						asctime(localtime(&bestdata.expiration)) );
			for (i=10; i>0; i--) {
				TTYprintf("\b\b%2.2d", i); 
				TTYflush();
				sleep(1);
			}
			rcode = TRUE;
			break;

		case 3:
			strscpy(SysSerialNumber, "Invalid license", sizeof(SysSerialNumber));
			ERRprintf(
"ERROR: Serial number (%s) could not be verified (error = %d).\n"
"\n"
"  It is possible that you entered the wrong serial number during install.\n"
"  To check and correct the serial number, use either REGEDIT REGEDT32 to\n"
"  examine the serial number stored under one of the following keys or wait\n"
". for the timeout below and choose \"Modify Serial Number\" under the \"HELP\"\n"
"  menu list.  The serial number must match the 14 character string printed on\n"
"  the install diskettes, or the appropriate test/evaluation serial number.\n"
"\n"
"    HKEY_LOCAL_MACHINE\\SOFTWARE\\Computer Graphics Service\\RUMP/Genplot\\2.0\n"
"\n"
"  The program will now start after a 60 second annoyance delay.\n"
"\n"
"\n"
"Okay - prepare to wait ... 000",
						 serial, bestdata.errorcode);
			for (i=60; i>0; i--) {
				TTYprintf("\b\b%2.2d", i);
				TTYflush();
				sleep(1);
			}
			rcode = TRUE;
			break;

		case 10:
			ERRprintf(
"FATAL ERROR: File serialno.dll file appears to have been tampered with.\n"
"             [This error also results if your time/date are really far off!]\n");
			rcode = FALSE;
			break;

		default:
			strscpy(SysSerialNumber, "No license", sizeof(SysSerialNumber));
			ERRprintf(
"ERROR: No serial number was found in any of the expected registry keys.\n"
"\n"
"  This should only happen if you moved the program over manually to another\n"
"  machine without running the install program -- or you had to reload the\n"
"  operating system from scratch destroying the registry.\n"
"\n"
"  Genplot and RUMP rely on settings in the registry to locate components and\n"
"  configuration data.  You must install at least once on each machine or\n"
"  after a full operating system re-install.\n"
"\n"
"  If you think this message is in error, check for a serial number in the\n"
"  registry under one of the following keys:\n"						 
"\n"
"    HKEY_LOCAL_MACHINE\\SOFTWARE\\Computer Graphics Service\\RUMP/Genplot\\2.0\n"
"\n"
"  WARNING: Editing the registry can be very powerful, but be exceedingly\n"
"           careful when changing entries that you do not fully understand!\n\n");
			rcode = FALSE;
			break;
	}

	TTYprintf("Returning from serial.dll with rc=%d, serial=%s\n", rcode, SysSerialNumber);
	TTYflush();

	if (! rcode) sleep(20);       /* Leave time to read the message */
	return(rcode);
}

/* ===========================================================================
-- Routine to reset the serial number based on user entered string.  It does
-- no checking and just puts the string into the appropriate registry points.
-- The VerifiedSerial number is blanked out.
--
-- Usage: SysModifyLicense(char *serial)
--
-- Inputs: serial - new serial number string.  Taken as is with no tests.
--
-- Output: Modifies registry keys
--
-- Return: TRUE if able to modify the string, FALSE if the serial number
--         was blank, or if there was no registry entry to change.
=========================================================================== */
BOOL SysModifyLicense(char *serial) {

	HKEY hkey;

	if (serial == NULL || *serial == '\0') return(FALSE);

	if (RegOpenKeyEx(RegistryHive, "Software\\Computer Graphic Service\\Genplot/Rump\\2.0", 0, KEY_SET_VALUE, &hkey) != ERROR_SUCCESS)
		return(FALSE);

	RegSetValueEx(hkey, "Serial", 0, REG_SZ, serial, (int) strlen(serial)+1);
	RegDeleteValue(hkey, "VerifiedSerial");
	RegCloseKey(hkey);

	strscpy(SysSerialNumberEx, serial, sizeof(SysSerialNumberEx));
	return(TRUE);
}

#else
BOOL SysCheckLicense(char *product) {
	return(TRUE);
}
BOOL SysModifyLicense(char *serial) {
	return(TRUE);
}
#endif


/* ===========================================================================
-- Routine to return the current system search path (dynamic libraries, etc.)
--
-- Usage: char *SysGetSysSearchPath()
--
-- Inputs: none
--
-- Output: none
--
-- Returns: Pointer to character string with the current search path.  If it
--          has not been scanned yet, the search path will be determined by
--          the default search information.  See also SysSetSysSearchPath().
=========================================================================== */
static char *SysSearchPath=NULL;				/* Default search path			*/

char *SysGetSysSearchPath(void) {
	
	char *aptr;

/* If already have, just return */
	if (SysSearchPath != NULL) return(SysSearchPath);

/* Check for environment variable "GenplotSearchPath_NT" (debug mainly) */
#ifdef OS2
	if ( (aptr = getenv("GenplotSearchPath_OS2")) != NULL) {
#elif defined NT
	if ( (aptr = getenv("GenplotSearchPath_NT")) != NULL) {
#else
	if ( (aptr = getenv("GenplotSearchPath_UNIX")) != NULL) {
#endif
		SysSearchPath = strdup(aptr);
		return(SysSearchPath);
	}

/* Check the profile strings (if available) */
	#ifdef OS2
		aptr = calloc(LONG_STR_SIZE,1);
		if (PrfQueryProfileString(HINI_PROFILE, "GENPLOT", "PATH", NULL, aptr, LONG_STR_SIZE)) {
			SysSearchPath = strdup(aptr);
			free(aptr);
			return(SysSearchPath);
		}
		free(aptr);
	#elif defined NT
		{
			LONG rc;
			HKEY hkey;
			DWORD isize, itype;
			BOOL GotHive;
			char data[LONG_STR_SIZE], dataex[LONG_STR_SIZE];

			/* Look in both HKEY_CURRENT_USER and HKEY_LOCAL_MACHINE for serial number key */
			GotHive = FALSE;
			if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Computer Graphic Service\\Genplot/Rump\\2.0", 0, KEY_QUERY_VALUE, &hkey) == ERROR_SUCCESS) {
				isize = sizeof(data);
				GotHive = RegQueryValueEx(hkey, "Serial", NULL, &itype, data, &isize) == ERROR_SUCCESS;
				if (! GotHive) RegCloseKey(hkey);
			}
			if (! GotHive && RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Computer Graphic Service\\Genplot/Rump\\2.0", 0, KEY_QUERY_VALUE, &hkey) == ERROR_SUCCESS) {
				isize = sizeof(data);
				GotHive = RegQueryValueEx(hkey, "Serial", NULL, &itype, data, &isize) == ERROR_SUCCESS;
				if (! GotHive) RegCloseKey(hkey);
			}
			if (GotHive) {
				isize = sizeof(data);
				rc = RegQueryValueEx(hkey, "GENPLOT_PATH", NULL, &itype, data, &isize);
				RegCloseKey(hkey);
				if (rc == ERROR_SUCCESS && itype == REG_SZ) {
					ExpandEnvironmentStrings(data, dataex, sizeof(dataex));
					SysSearchPath = strdup(dataex);
					return(SysSearchPath);
				}
			}
		}
	#endif

/* Final default is to use the configuration from load time */
	#ifdef CONFIGPATH								/* Set at compile time?			*/
		SysSearchPath=strdup(CONFIGPATH);	/* From makefile -Dxxxx			*/
	#else												/* Or use the defaults			*/
		SysSearchPath=strdup(DEFAULTDIR);	/* From <defaults.h>				*/
	#endif
	return(SysSearchPath);
}

/* ===========================================================================
-- Routine to set the system search path (dynamic libraries, etc.)
--
-- Usage: char *SysSetSysSearchPath(char *newpath)
--
-- Inputs: newpath - string representing the new search path.  If NULL,
--                   the existing path will be erased and reparsed next
--                   time needed.
--
-- Output: none
--
-- Returns: Pointer to the new search path string.
=========================================================================== */
char *SysSetSysSearchPath(char *newpath) {

	if (SysSearchPath != NULL) free(SysSearchPath);
	SysSearchPath = (newpath != NULL) ? strdup(newpath) : NULL;
	return(SysSearchPath);
}

/* ===========================================================================
-- Subroutine to resolve a given name into the correct pathname for dynamic
-- programs.  Uses the CONFIG search path or given environment variables.
--
-- Usage:  SysResolveDyntName(char *path, const char *basename, int path_len);
--
-- Input:  basename - the base part of the name (genplot.hlb)
--
-- Output: pathname - pathname where file is expected.  Search order is
--                    (1) Current directory
--                    (2) Assume in DefaultDir (see defaults.h)
============================================================================ */
BOOL SysResolveDyntName(char *path, const char *basename, int path_len) {
	
#if (defined OS2 || defined NT)
	char *separator=";";			/* Due to disks, path separater must ; */
#else
	char *separator=":";			/* For UNIX, standard is to use the : */
#endif
	char *SearchPath;				/* Default search path			*/

	char tmpname[PATH_MAX],							/* test filename					*/
		  localbase[PATH_MAX],						/* Modified basename				*/
		  *tmpconfig, *tmpptr,						/* Temporary config pointers	*/
		  *aptr;											/* And random pointer			*/
	BOOL succeed;
	
	SearchPath = SysGetSysSearchPath();

	if (Access(basename, F_OK) == 0) {					/* If file exists, use */
		strscpy(path, basename, path_len);
		return(TRUE);
	} else if ( (aptr=getenv(basename)) != NULL) {	/* If SET, use that	*/
		strscpy(path, aptr, path_len);
		return(Access(aptr,F_OK) == 0);
	} 

/* ------------------------------------------------------------------------
-- SearchPath contains a string of form <dir>;<dir>;<dir> 
-- We have to parse <dir>, make path, check existence, and continue.
-- If none exist, return with name corresponding to last <dir> in string.
--
-- For each, check <name>/local before checking actual directory
------------------------------------------------------------------------ */
	tmpptr = tmpconfig = strdup(SearchPath);	/* Local copy for trashing */

	succeed = FALSE;
	SysMakePath(localbase, "Local", basename, NULL);		/* Modified basename				*/

	while ( (aptr=strtok(tmpptr, separator)) != NULL) {
		tmpptr = NULL;									/* For next time */
		if (*aptr == '\0') continue;
		SysMakePath(tmpname, aptr, localbase, NULL);	/* try a name */
		if (Access(tmpname, F_OK) == 0) {succeed = TRUE; break;}
		SysMakePath(tmpname, aptr, basename, NULL);	/* try a name */
		if (Access(tmpname, F_OK) == 0) {succeed = TRUE; break;}
	}

	if (succeed) {
		strscpy(path, tmpname, path_len);
	} else {
#ifdef DEBUG
		fprintf(stderr, "WARNING: Filename %s not found in path %s\n", basename, SearchPath);
#endif
		strscpy(path, basename, path_len);			/* Default behavior */
	}
	free(tmpconfig);
	return(succeed);
}


/* ---------------------------------------------------------------------------
-- Routine to check if user has pressed a single ^C to terminate a process
--
-- Usage:  BOOL SysChkBreak(BOOL flag);
--
--	Inputs: flag -- if TRUE, break flag will be reset before return
--
-- Output: Has a <break> signal occured since last reset of internal flag.
--         Note that a second break signal before reset will terminate.
---------------------------------------------------------------------------- */
BOOL SysChkBreak(BOOL flag) {

	BOOL rcode;
	static int FirstTime=TRUE;

	if (FirstTime) {
		if (CONBreakNotify(&SysBreakFlag, TRUE) != 0) {
			ERRprintf("WARNING: Break flag not initialized - ^C will be strange\n");
		}
		if (CONBreakNotify(&SysBreakCount, TRUE) != 0) {
			ERRprintf("WARNING: Break count flag not initialized - ^C will be strange\n");
		}
		FirstTime = FALSE;
	}

	rcode = SysBreakFlag;
	if (flag) {
		CONBreakClear();
		SysBreakFlag = FALSE;
	}
/*	TTYprintf("SysChkBreak: rc=%d  flag=%d  SysBreakFlag: %d\n", rcode, flag, SysBreakFlag); */
	return rcode;
}


/* ---------------------------------------------------------------------------
-- Routine to push a secondary shell
--
-- Usage: LOG = SysPushShell(void)
--
-- Inputs: none
--
-- Output: RCODE - Error code if program executed
---------------------------------------------------------------------------- */
BOOL SysPushShell(void) {

#ifdef NT
	char *ComspecPtr;
	if (SysGUIMode) { 
		return(SysSystem("start"));
	} else if ((ComspecPtr=getenv("COMSPEC")) == (void *) NULL) {
		fprintf(stderr,"Unable to locate the COMMAND.COM");
		return(FALSE);
	} else {
		TTYprintf("\nSecondary Command Processor Invoked (CP$)\n"
					 "Type \"EXIT\" to return to invoking process");
		spawnl(P_WAIT, ComspecPtr, NULL);
	}
#elif defined OS2
	char *ComspecPtr;
	if ((ComspecPtr=getenv("COMSPEC")) == (void *) NULL) {
		fprintf(stderr,"Unable to locate the COMMAND.COM");
		return(FALSE);
	}
	TTYprintf("\nSecondary Command Processor Invoked (CP$)\n"
				 "On PM window versions, typing may not echo until after <CR>\n"
				 "Type \"EXIT\" to return to invoking process");
	spawnl(P_WAIT, ComspecPtr, NULL);
#else
	raise(SIGTSTP);
#endif
	return(TRUE);

}

/* ---------------------------------------------------------------------------
-- Routine to execute a command in a secondary shell
--
-- Usage:  int SysSystem(char *command);
--
-- Inputs:  command - external command to be executed by the external shell
--
-- Output:  depends
--
-- Returns: return code from command
--
-- Note: For NT and OS/2, if the command line ends with an &, it is taken as
--       request for a detached process.  The & is converted into a "start"
--       at the beginning of the command line.  The following are equivalent:
--               start eps test.mac
--               eps test.mac &
---------------------------------------------------------------------------- */
int SysSystem(char *command) {

#if (defined NT || defined OS2) 
	int rcode;
	char *cmd, *aptr;								/* Command line and pointers	*/

	CONSuspend();									/* Suspend CON processing		*/
	cmd = malloc(strlen(command)+30);		/* Leave space for extra chrs	*/
	strcat(strcpy(cmd, "start \"Microsoft Sucks\" "), command);
	aptr = cmd + strlen(cmd) - 1;
	if (*aptr == '&') {
		*aptr = '\0';
		aptr = cmd;
	} else {
		aptr = cmd+24;
	}

	rcode = System(aptr);
	CONRestart();								/* Restart CON processing */
	free(cmd);
	return(rcode);
#else
	int rcode;

	CONSuspend();								/* Suspend CON processing */
	rcode = System(command);
	CONRestart();								/* Restart CON processing */
	return(rcode);
#endif
}

/* ---------------------------------------------------------------------------
-- Usage: BOOL SysListDir(char *spec, char *givenoption);
--
-- Inputs: filename - file name to be output
---------------------------------------------------------------------------- */
#if ! (defined OS2 || defined NT)
BOOL SysListDir(char *spec, char *givenoption) {

  char cmdline[PATH_MAX];
  sprintf(cmdline, "ls %s %s", givenoption, spec);
  return(SysSystem(cmdline) == 0);

}
#endif

/* ============================================================================
--     Usage: BOOL SysFileMore(CHAR *filename);
--
--     Inputs: filename - file name to be output
============================================================================ */
static BOOL TrivialMore(char *filename) {

	FILE *funit;
	unsigned long iline=0;						/* Line number			*/
	char inbuf[LONG_STR_SIZE], *aptr;
	int achr, rows, cols, onscreen;
	int key;
	
	if ( (funit=fopen( filename, "r" )) == NULL) {
		ERRprintf("ERROR: %s could not be opened\n", filename);
		return(FALSE);
	}

/* Clear the screen, or at least a short block so know where it is */
/*	ScrClearAttrib(D_NORMAL); */
	TTYputs("\n\n\n\n\n");							/* Rather than clear screen */

	iline    = 0;
	onscreen = 0;
	rows = ScrInfo->rows;							/* Initial values */
	cols = (ScrInfo->buf_cols != -1) ? ScrInfo->buf_cols : ScrInfo->cols;
/*	TTYprintf("Reported size: %d by %d\n", rows, cols); */

	while (fgets(inbuf, sizeof(inbuf), funit) != NULL) {
		iline++;
		if (inbuf[strlen(inbuf)-1] == '\n') inbuf[strlen(inbuf)-1] = '\0';

		aptr = inbuf;
		while (((int) strlen(aptr)) > cols) {
			onscreen++;
			achr = aptr[cols-1];
			aptr[cols-1] = '\0';
			TTYputs(aptr);
			TTYputs("\\\n");
			aptr += cols-1;
			*aptr = achr;
		}
		TTYputsnl(aptr);
		onscreen++;

		if (onscreen >= rows-1) {
PromptForAction:
			CONputs("--More--");
			key = CONgetc();
			CONputs("\r         \r");
			rows = ScrInfo->rows;				/* Possibly new values */
			cols = (ScrInfo->buf_cols != -1) ? ScrInfo->buf_cols : ScrInfo->cols;
			switch (key) {
				case 'd':
				case 4:								/* ^D */
					onscreen = rows-11;
					break;
				case ' ':
				case 'z':
					onscreen = 2;
					break;
				case 0x07:							/* ^G			*/
				case 0x1A:							/* ^Z			*/
				case 0x1B:							/* Escape	*/
				case 'q':
				case 'Q':
					goto QuitFile;
				case '=':
					printf("Line Number = %lu\n", iline);
					goto PromptForAction;
				case '\r':
				case '\n':
					onscreen = rows-1;
					break;
				case 't':
					fseek(funit, 0L, SEEK_SET);
					ScrClearAttrib(D_NORMAL);
					iline = 0;
					onscreen = 0;
					break;
				default:
					RingBell();
					goto PromptForAction;
			}
		}
	}

QuitFile:
	fclose(funit);								/* close file */
	return(TRUE);
}

BOOL SysFileMore(char *filename) {

	int  rcode=TRUE;
	char *pager=NULL;
	char cmdline[PATH_MAX];

	if ( (pager=getenv("PAGER")) == NULL) pager=DefaultPager;
			
	if (pager != NULL && stricmp(pager, "internal") != 0) {
		sprintf(cmdline, "%s %s", pager, filename);
		if (SysSystem(cmdline) != 0) {
			ERRprintf("ERROR: External <more> failed (%s)\n", cmdline);
			rcode = FALSE;
		}
	} else {
		rcode = TrivialMore(filename);
	}
	return(rcode);
}


/* ---------------------------------------------------------------------------
-- Routine to sleep a specified number of nanoseconds
--
--	Usage:   unsigned int = NanoSleep(unsigned long timeval);
--               unsigned int = MilliSleep(unsigned long timeval);
--		
--	Inputs:  time  - Number of nanoseconds or milliseconds to sleep
--
--	Returns: 0 unless aborted early by alarm signal
--------------------------------------------------------------------------- */
#if defined LINUX || defined UNIX
struct timespec {
	time_t tv_sec;
	long int tv_nsec;
};
int nanosleep(const struct timespec *rqtp, struct timespec *rmtp);
#endif

unsigned int NanoSleep(unsigned long timeval) {
	int rc=0;

#if (defined CSET2 || defined WATCOM || defined MSC60)		/* OS/2 specific version */
	DosSleep(timeval/1000000L);
#elif defined MSC70
	Sleep(timeval/1000000L);
#elif defined LINUX || defined UNIX
	struct timespec request;
	request.tv_sec  = timeval / 1000000000UL;
	request.tv_nsec = timeval - 1000000000UL*request.tv_sec;
	rc = nanosleep(&request, NULL);
#else															/* Default if nothing better */
	rc = sleep( (unsigned int) (timeval/1000000000UL) );
#endif

	return(0);
}


unsigned int MilliSleep(unsigned long timeval) {
	int rc=0;

#if (defined CSET2 || defined WATCOM || defined MSC60)		/* OS/2 specific version */
	DosSleep(timeval);
#elif defined MSC70
	Sleep(timeval);
#elif defined LINUX || defined UNIX
	struct timespec request;
	request.tv_sec  = timeval / 1000;
	request.tv_nsec = (timeval - 1000*request.tv_sec)*1000000L;
	rc = nanosleep(&request, NULL);
#else															/* Default if nothing better */
	rc = sleep( (unsigned int) (timeval/1000UL) );
#endif

	return(rc);
}
