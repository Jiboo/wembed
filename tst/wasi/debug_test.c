#include <stdio.h>
#include <stdlib.h>

struct teststruct {
  int a;
  float b;
};

struct testbitfield {
  int a : 1;
  int b : 2;
  int c : 4;
  int d : 8;
  int e : 16;
};

enum testenum {
  ENUM_A,
  ENUM_B,
  ENUM_C = 0x20,
  ENUM_D = -1,
};

void init_teststruct(struct teststruct *pInput) {
  if (pInput->a == 0) {
    pInput->a = 0;
    pInput->b = 1;
  }
}

void init_testbitfield(struct testbitfield *pInput) {
  pInput->a = 0;
  pInput->b = 1;
  pInput->c = 2;
  pInput->d = 4;
  pInput->e = 8;
}

char* unknown() {
  return "ENUM_UNKNOWN";
}

char* get_testenum(enum testenum pInput) {
  switch (pInput) {
    case ENUM_A: return "ENUM_A";
    case ENUM_B: return "ENUM_B";
    case ENUM_C: return "ENUM_C";
    case ENUM_D: return "ENUM_D";
    default: return unknown();
  }
}

void* add_ptr(void (*pPtr)(), unsigned pOffset) {
  return pPtr + pOffset;
}

int main(int argc, char** argv) {
  printf("argv at %p\n", argv);

  struct teststruct a;
  struct testbitfield b;

  init_teststruct(&a);
  init_testbitfield(&b);

  struct testbitfield c[512];
  for (int i = 0; i < 512; i++) {
    init_testbitfield(&c[i]);
  }

  return 0;
}
