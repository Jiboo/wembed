#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
  int lCount = 100;

  int lAccumulator = 0;
  for (int i = 0; i < lCount; i++) {
    lAccumulator += rand();
  }

  printf("%i\n", lAccumulator);

  return EXIT_SUCCESS;
}
