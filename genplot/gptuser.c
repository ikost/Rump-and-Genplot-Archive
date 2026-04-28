/* subroutines from main genplot */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#if (defined CSET2 || defined MSC60)	/* Necessary for USER dynamic link */
	#define INCL_DOS
	#include <os2.h>
#elif defined MSC70
	#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <time.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"
#include "gptxtrn.h"
#include "gptdef.h"

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

/* ------------------------------- */
/* My share of global externals    */
/* ------------------------------- */

#if defined CSET2

/* ===========================================================================
-- Routine to free a loaded User DLL module
=========================================================================== */
int GptFreeUserDLL(LOGICAL CallExit) {

	HMODULE hmod;
	int (*Exit)(void)=NULL;						/* Just used locally */

	if (*GptUserModule != '\0' && DosQueryModuleHandle(GptUserModule, &hmod) == 0) {
		if (CallExit && (DosQueryProcAddr(hmod,0,"UserExit", (PFN *) &Exit)) == 0)
			(*Exit)();
		_freemod(hmod);
	}

	GptUserCmd   = NULL;							/* Clear functions now		*/
	GptUserFnc   = GptDefaultUserFnc;		/* Reset all to defaults	*/
	GptUserRead  = GptDefaultUserRead;
	GptUserWrite = GptDefaultUserWrite;
	*GptUserModule = '\0';						/* And mark as unloaded		*/
	return(0);
}

/* ===========================================================================
-- Routine to load and initialize a user DLL module
=========================================================================== */
int GptLoadUserDLL(char *module) {

	char szTmp[PATH_MAX], name[PATH_MAX];
	HMODULE hmod;
	APIRET rc;
	int (*Init)(void)=NULL;

	strscpy(szTmp, module, sizeof(szTmp));

	if (! SysResolveDyntName(name, szTmp, sizeof(name))) {	/* modifies name */
		strcpy(name, szTmp);
		SysAddExt(name, ".usr");
		if (! SysResolveDyntName(name, name, sizeof(name))) strcpy(name, szTmp);
	}

	GptFreeUserDLL(TRUE);						/* Free existing module */

	if ( (rc=DosLoadModule(szTmp, sizeof(szTmp), name, &hmod)) != 0) {
		ERRprintf("Dynamic module not found (rc=%i, fail=%s)\n", (int) rc, szTmp);
		return(NOMORE);
	}

	DosQueryModuleName(hmod, sizeof(GptUserModule), GptUserModule);

	TTYprintf("Searching %s:", GptUserModule);
	if (DosQueryProcAddr(hmod,0,"UserCmd",  (PFN *) &GptUserCmd  ) == 0) TTYputs(" cmd()");
	if (DosQueryProcAddr(hmod,0,"UserFnc",  (PFN *) &GptUserFnc  ) == 0) TTYputs(" fnc()");
	if (DosQueryProcAddr(hmod,0,"UserRead", (PFN *) &GptUserRead ) == 0) TTYputs(" read()");
	if (DosQueryProcAddr(hmod,0,"UserWrite",(PFN *) &GptUserWrite) == 0) TTYputs(" write()");
	TTYputc('\n');

	if (DosQueryProcAddr(hmod,0,"UserInit",  (PFN *) &Init) == 0) {
		if ((*Init)() != 0) {
			ERRprintf("ERROR: Module %s failed to initialize\n", GptUserModule);
			GptFreeUserDLL(FALSE);							/* Dump it now */
			return(NOMORE);
		}
	}

	return(OKAY);
}

/* ===========================================================================
-- Routine to load and initialize a user MDL module
=========================================================================== */
int GptLoadUserMDL(char *module) {

	char szTmp[PATH_MAX], name[PATH_MAX];
	HMODULE hmod;
	APIRET rc;
	int (*Init)(void)=NULL;

	strscpy(szTmp, module, sizeof(szTmp));

	if (! SysResolveDyntName(name, szTmp, sizeof(name))) {	/* modifies name */
		strcpy(name, szTmp);
		SysAddExt(name, ".mdl");
		if (! SysResolveDyntName(name, name, sizeof(name))) strcpy(name, szTmp);
	}

	if ( (rc=DosLoadModule(szTmp, sizeof(szTmp), name, &hmod)) != 0) {
		ERRprintf("Dynamic module not found (rc=%i, fail=%s)\n", (int) rc, szTmp);
		return(NOMORE);
	}

	if ( (rc=DosQueryProcAddr(hmod,0,"Init", (PFN *) &Init)) != 0) {
		ERRprintf("ERROR: Huh? No Init() procedure in %s (rc=%d)\n", szTmp, rc);
		return(NOMORE);
	}

	TTYprintf("Executing Init() from %s\n", szTmp);
	(*Init)();
	return(OKAY);
}

#elif defined MSC70

/* ===========================================================================
-- Routine to free a loaded User DLL module
=========================================================================== */
int GptFreeUserDLL(LOGICAL CallExit) {

	HMODULE hmod;
	int (*Exit)(void);							/* Just used locally */

	if (*GptUserModule != '\0' && (hmod=GetModuleHandle(GptUserModule)) != NULL) {
		if (CallExit && ( (Exit = (void *) GetProcAddress(hmod, "UserExit")) != NULL) )
			(*Exit)();
		FreeLibrary(hmod);
	}

	GptUserCmd   = NULL;							/* Clear functions now		*/
	GptUserFnc   = GptDefaultUserFnc;		/* Reset all to defaults	*/
	GptUserRead  = GptDefaultUserRead;
	GptUserWrite = GptDefaultUserWrite;
	*GptUserModule = '\0';						/* And mark as unloaded		*/
	return(0);
}

/* ===========================================================================
-- Routine to load and initialize a user DLL module
=========================================================================== */
int GptLoadUserDLL(char *module) {

	char szTmp[PATH_MAX], name[PATH_MAX];
	HMODULE hmod;
	int (*Init)(void);

	strscpy(szTmp, module, sizeof(szTmp));

	if (! SysResolveDyntName(name, szTmp, sizeof(name))) {	/* modifies name */
		strcpy(name, szTmp);
		SysAddExt(name, ".usr");
		if (! SysResolveDyntName(name, name, sizeof(name))) strcpy(name, szTmp);
	}

	GptFreeUserDLL(TRUE);						/* Free existing module */

	if ( (hmod=LoadLibrary(name)) == NULL) {
		ERRprintf("Dynamic module not found (rc=%i, fail=%s)\n", GetLastError(), name);
		return(NOMORE);
	}

	GetModuleFileName(hmod, GptUserModule, sizeof(GptUserModule));

	TTYprintf("Searching %s:", GptUserModule);
	if ( (GptUserCmd   = (void *) GetProcAddress(hmod, "UserCmd"  )) != NULL) TTYputs(" cmd()");
	if ( (GptUserFnc   = (void *) GetProcAddress(hmod, "UserFnc"  )) != NULL) TTYputs(" fnc()");
	if ( (GptUserRead  = (void *) GetProcAddress(hmod, "UserRead" )) != NULL) TTYputs(" read()");
	if ( (GptUserWrite = (void *) GetProcAddress(hmod, "UserWrite")) != NULL) TTYputs(" write()");
	TTYputc('\n');

	if ( (Init = (void *) GetProcAddress(hmod, "UserInit")) != NULL) {
		if ((*Init)() != 0) {
			ERRprintf("ERROR: Module %s failed to initialize\n", GptUserModule);
			GptFreeUserDLL(FALSE);							/* Dump it now */
			return(NOMORE);
		}
	}

	return(OKAY);
}

/* ===========================================================================
-- Routine to load and initialize a user MDL module
=========================================================================== */
int GptLoadUserMDL(char *module) {

	char szTmp[PATH_MAX], name[PATH_MAX];
	HMODULE hmod;
	int (*Init)(void);

	strscpy(szTmp, module, sizeof(szTmp));

	if (! SysResolveDyntName(name, szTmp, sizeof(name))) {	/* modifies name */
		strcpy(name, szTmp);
		SysAddExt(name, ".mdl");
		if (! SysResolveDyntName(name, name, sizeof(name))) strcpy(name, szTmp);
	}

	if ( (hmod=LoadLibrary(name)) == NULL) {
		ERRprintf("Dynamic module not found (rc=%i, fail=%s)\n", GetLastError(), name);
		return(NOMORE);
	}

	if ( (Init = (void *) GetProcAddress(hmod, "Init")) == NULL) {
		ERRprintf("ERROR: Huh? No Init() procedure in %s (rc=%d)\n", szTmp, GetLastError());
		return(NOMORE);
	}

	TTYprintf("Executing Init() from %s\n", szTmp);
	(*Init)();
	return(OKAY);
}

#elif (defined MSC60 && defined _DLL)

/* ===========================================================================
-- Routine to free a loaded User DLL module
=========================================================================== */
int GptFreeUserDLL(LOGICAL CallExit) {

	HMODULE hmod;
	APIRET rc;
	int (*Exit)(void);					/* Just used locally */

	if (*GptUserModule != '\0' && DosGetModHandle(GptUserModule, &hmod) == 0) {
		if (CallExit && (DosGetProc(hmod, "_UserExit", (PPFN) &Exit)) == 0)
			(*Exit)();
		DosFreeModule(hmod);
	}

	GptUserCmd   = NULL;							/* Clear functions now		*/
	GptUserFnc   = GptDefaultUserFnc;		/* Reset all to defaults	*/
	GptUserRead  = GptDefaultUserRead;
	GptUserWrite = GptDefaultUserWrite;
	*GptUserModule = '\0';						/* And mark as unloaded		*/
	return(0);
}

/* ===========================================================================
-- Routine to load and initialize a user DLL module
=========================================================================== */
int GptLoadUserDLL(char *name) {

	HMODULE hmod;
	USHORT rc;
	int (*Init)(void);
	char errbuf[PATH_MAX];

	GptFreeUserDLL(TRUE);						/* Free existing module */

	strcpy(GptUserModule, name);

	if ( (rc=DosLoadModule(errbuf, sizeof(errbuf), GptUserModule, &hmod)) != 0) {
		ERRprintf("Dynamic module not found (rc=%i, fail=%s)\n", (int) rc, errbuf);
		*GptUserModule = '\0';
		return(NOMORE);
	}

	DosGetModName(hmod, sizeof(GptUserModule), GptUserModule);

	TTYprintf("Searching %s:", GptUserModule);
	if (DosGetProcAddr(hmod, "_UserCmd",   (PPFN) &GptUserCmd  ) == 0) TTYputs(" cmd()");
	if (DosGetProcAddr(hmod, "_UserFnc",   (PPFN) &GptUserFnc  ) == 0) TTYputs(" fnc()");
	if (DosGetProcAddr(hmod, "_UserRead",  (PPFN) &GptUserRead ) == 0) TTYputs(" read()");
	if (DosGetProcAddr(hmod, "_UserWrite", (PPFN) &GptUserWrite) == 0) TTYputs(" write()");
	TTYputc('\n');

	if (DosGetProcAddr(hmod, "_UserInit",  (PPFN) &Init) == 0) {
		if ((*Init)() != 0) {
			gen_err2("Module failed to initialize", GptUserModule);
			GptFreeUserDLL(FALSE);							/* Dump it now */
			return(NOMORE);
		}
	}

	return(OKAY);
}

/* ===========================================================================
-- Routine to load and initialize a user MDL module
=========================================================================== */
int GptLoadUserMDL(char *module) {
	TTYputs("ERROR: This version can't load user modules\n");
	return(UNIMPLEMENTED);
}

#else

/* ===========================================================================
-- Routines to load/free a loaded User DLL module
=========================================================================== */
int GptFreeUserDLL(LOGICAL CallExit) {

	GptUserCmd   = NULL;							/* Clear functions now		*/
	GptUserFnc   = GptDefaultUserFnc;		/* Reset all to defaults	*/
	GptUserRead  = GptDefaultUserRead;
	GptUserWrite = GptDefaultUserWrite;
	*GptUserModule = '\0';						/* And mark as unloaded		*/
	return(0);
}

/* ===========================================================================
-- Routine to load and initialize a user DLL module
=========================================================================== */
int GptLoadUserDLL(char *name) {

	TTYputs("WARNING: The USER -LOAD is disabled.  Default module only available\n");
	return(NOMORE);

}

/* ===========================================================================
-- Routine to load and initialize a user MDL module
=========================================================================== */
int GptLoadUserMDL(char *module) {
	TTYputs("ERROR: This version can't load user modules\n");
	return(UNIMPLEMENTED);
}

#endif
