#include <ea.c>				/* Get all the code */

int main(int argc, char *argv[]) {

	if (argc != 4) {
		fprintf(stderr, "Usage: tag <file> <attribute> <value>\n");
		return(EXIT_FAILURE);
	}

	EA_SetAscii(argv[1], argv[2], argv[3]);
	return(EXIT_SUCCESS);
}

