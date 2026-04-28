int EA_Set(char *Filename, char *szAttrib, void *EA, int cbEA);
int EA_Query(char *Filename, char *szAttrib, void **EA, int *cbEA);
int EA_SetAscii(char *Filename, char *szAttrib, char *szValue);
int EA_QueryAscii(char *Filename, char *szAttrib, char *szValue, int maxlen);
int EA_SetAsciiList(char *Filename, char *szAttrib, char *list[]);
int EA_QueryAsciiList(char *Filename, char *szAttrib, char **list[]);
int EA_Print(void *EA, int cbEA);
