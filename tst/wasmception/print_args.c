#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char** argv) {
	printf("argv at: %p\n", argv);
	printf("argc: %d\n", argc);
	for (int i = 0; i < argc; i++) {
		printf("arg %d: %s at %p\n", i, argv[i], argv[i]);
	}

	return EXIT_SUCCESS;
}
