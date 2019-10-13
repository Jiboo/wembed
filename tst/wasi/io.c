#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

int main(int argc, char** argv) {
  FILE *f1, *f2;
  if (mkdir("test", 0755) && errno != EEXIST) {
    fprintf(stderr, "Failed to create dir: %s\n", strerror(errno));
    return 1;
  }

  f1 = fopen("test/file.bin", "wb");
  if(f1) {
    int v[] = {42, -1, 7}; // underlying storage of std::array is an array
    fwrite(v, sizeof(int), sizeof(v)/sizeof(int), f1);
    fclose(f1);
  }
  else {
    fprintf(stderr, "Failed to open file for write: %s\n", strerror(errno));
    return 1;
  }

  // read the same data and print it to the standard output
  f2 = fopen("test/file.bin", "rb");
  if(f2) {
    int v[3];
    size_t sz = fread(v, sizeof(int), sizeof(v)/sizeof(int), f2);
    fclose(f2);
    for(size_t n = 0; n < sz; ++n) {
      printf("%d\n", v[n]);
    }
  }
  else {
    fprintf(stderr, "Failed to open file for read: %s\n", strerror(errno));
    return 1;
  }

  if(remove("test/file.bin")) {
    fprintf(stderr, "Failed to delete file: %s\n", strerror(errno));
    return 1;
  }

  if(remove("test")) {
    fprintf(stderr, "Failed to delete directory: %s\n", strerror(errno));
    return 1;
  }

  f2 = fopen("/etc/bashrc", "r");
  if(f2) {
    fprintf(stderr, "Could escape root preopen\n");
    return 1;
  }

  f2 = fopen("~/.bashrc", "r");
  if(f2) {
    fprintf(stderr, "Could escape root preopen\n");
    return 1;
  }

  return EXIT_SUCCESS;
}
