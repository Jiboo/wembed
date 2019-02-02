#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
  int lCount = 100;

  float lAccumulator = 0;
  for (int i = 0; i < lCount; i++) {
    lAccumulator += rand();
  }

  printf("%f", lAccumulator);

  return EXIT_SUCCESS;
}
