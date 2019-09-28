#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
  int lCount = 100;

  for (int i = 0; i < lCount; i++) {
    printf("%f\n", (float)i);
  }

  return EXIT_SUCCESS;
}
