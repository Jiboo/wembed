#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
  int lCount = 100;
	int *lIArray = malloc(sizeof(int) * lCount);

  for (int i = 0; i < lCount; i++) {
    lIArray[i] = rand();
  }

  int lAccumulator = 0;
  for (int i = 0; i < lCount; i++) {
    lAccumulator += lIArray[i] + rand();
  }

  printf("%i", lAccumulator);

  return EXIT_SUCCESS;
}
