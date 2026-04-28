#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int main(int argc, char *argv[]) {

	time_t ltime;
	char lctime[80];

	if (argc <= 1) {
		printf("Usage: revtime <varname> > revision.h\n");
		return(0);
	}

	time(&ltime);
	strcpy(lctime, ctime(&ltime));
	if (lctime[strlen(lctime)-1] == '\n') lctime[strlen(lctime)-1] = '\0';

	printf("EXPORT const char %s[] = \"%s\";\n", argv[1], lctime);
	return(0);
}
