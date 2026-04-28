#ifndef __defaults
   #define __defaults

#ifdef OS2
	#define DefaultEditor	NULL			/* No defaults due to PM problems */
	#define DefaultPager		NULL
	#define DEFAULTDIR		"c:\\usr\\genplot\\os2\\"
#elif defined NT
	#define DefaultEditor	"notepad"	/* No defaults due to PM problems */
	#define DefaultPager		NULL
	#define DEFAULTDIR		"c:\\usr\\genplot\\nt\\"
#else
	#define DefaultEditor	"emacs"
	#define DefaultPager		"more"
	#define DEFAULTDIR		"/usr/local/lib"
#endif

#endif /* __defaults */
