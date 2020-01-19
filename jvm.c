#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define BUF_SIZE 1024
#define error(...) do {\
  fprintf(stderr, __VA_ARGS__);\
  fputs("\n", stderr);\
  exit(1);\
} while (1)

typedef struct {
  size_t size;
  size_t cur;
  char* data;
} reader_t;

void init_reader(reader_t *reader, size_t size, char *data) {
  reader->size = size;
  reader->cur = 0;
  reader->data = data;
}

void read_bytes(reader_t *reader, unsigned char *buf, size_t n) {
  if (reader->cur + n >= reader->size) {
    error("cannot read from reader");
  }
  memcpy(buf, reader->data + reader->cur, n);
  reader->cur += n;
}

uint16_t read_u2(reader_t *reader) {
  unsigned char buf[2];
  read_bytes(reader, buf, 2);
  return ((uint16_t)buf[0]) << 8 | (uint16_t)buf[1];
}

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
  reader_t class_reader_instance;
  reader_t *class_reader = &class_reader_instance;
  init_reader(class_reader, class_file_size, class_file_data);

  // read magic
  unsigned char magic[4];
  read_bytes(class_reader, magic, 4);
  uint16_t major = read_u2(class_reader);
  uint16_t minor = read_u2(class_reader);

  return 0;
}
