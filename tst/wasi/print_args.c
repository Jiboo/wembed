#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern char **environ;

int main(int argc, char **argv) {
	printf("argv at: %p\n", argv);
	printf("argc: %d\n", argc);
	for (int i = 0; i < argc; i++) {
		printf("arg %d: %s at %p\n", i, argv[i], argv[i]);
	}

  printf("environ at: %p\n", environ);
  for (char **env = environ; *env != 0; env++) {
    printf("%s\n", *env);
  }

	return EXIT_SUCCESS;
}
