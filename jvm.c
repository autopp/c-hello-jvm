#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef FILE *reader_t;

#define BUF_SIZE 1024
#define error(...) do {\
  fprintf(stderr, __VA_ARGS__);\
  fputs("\n", stderr);\
  exit(1);\
} while (1)

int main(int argc, char **argv) {
  // load class file
  char class_file_name[strlen(argv[1]) + 7];
  sprintf(class_file_name, "%s.class", argv[1]);
  FILE *fp = fopen(class_file_name, "rb");
  if (fp == NULL) {
    error("cannot open %s", class_file_name);
  }

  char *class_file_data = malloc(BUF_SIZE);
  if (class_file_data == NULL) {
    error("cannot allocate for class_file_data");
  }
  size_t class_file_size = 0;
  while (1) {
    size_t n = fread(class_file_data, 1, BUF_SIZE, fp);
    class_file_size += n;

    if (n < BUF_SIZE) {
      break;
    }
    char *new_class_file_data = realloc(class_file_data, class_file_size + BUF_SIZE);
    if (new_class_file_data == NULL) {
      free(class_file_data);
      error("cannot reallocate for class_file_data");
    }
    class_file_data = new_class_file_data;
  }

  fclose(fp);

  return 0;
}
