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

typedef uint8_t u1_t;
typedef uint16_t u2_t;

typedef struct {
  size_t size;
  size_t cur;
  char* data;
} reader_t;

typedef enum {
  CONSTANT_UNDEFINED = 0x00,
  CONSTANT_CLASS = 0x07,
  CONSTANT_FIELDREF = 0x09,
  CONSTANT_METHODREF = 0x0A,
  CONSTANT_NAME_AND_TYPE = 0x0C,
  CONSTANT_STRING = 0x08,
  CONSTANT_UTF8 = 0x01
} constant_tag_t;

typedef union {
  struct {
    constant_tag_t tag;
  } common;
  struct {
    constant_tag_t tag;
    u2_t name_index;
  } class;
  struct {
    constant_tag_t tag;
    u2_t class_index;
    u2_t name_and_type_index;
  } fieldref;
  struct {
    constant_tag_t tag;
    u2_t class_index;
    u2_t name_and_type_index;
  } methodref;
  struct {
    constant_tag_t tag;
    u2_t name_index;
    u2_t descriptor_index;
  } name_and_type;
  struct {
    constant_tag_t tag;
    u2_t string_index;
  } string;
  struct {
    constant_tag_t tag;
    u2_t length;
    char *bytes;
  } utf8;
} constant_t;

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

u1_t read_u1(reader_t *reader) {
  unsigned char ch;
  read_bytes(reader, &ch, 1);
  return ch;
}

u2_t read_u2(reader_t *reader) {
  unsigned char buf[2];
  read_bytes(reader, buf, 2);
  return ((u2_t)buf[0]) << 8 | (u2_t)buf[1];
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

  // read magic and major/minor
  unsigned char magic[4];
  read_bytes(class_reader, magic, 4);
  read_u2(class_reader);
  read_u2(class_reader);

  // read constant pool
  u2_t constant_pool_count = read_u2(class_reader);
  constant_t constant_pool[constant_pool_count];
  constant_pool[0].common.tag = CONSTANT_UNDEFINED;
  for (int i = 1; i < constant_pool_count; i++) {
    constant_tag_t tag = (constant_tag_t)read_u1(class_reader);
    constant_t *constant = constant_pool + i;
    constant->common.tag = tag;
    switch (tag) {
      case CONSTANT_CLASS:
        constant->class.name_index = read_u2(class_reader);
        break;
      case CONSTANT_FIELDREF:
        constant->fieldref.class_index = read_u2(class_reader);
        constant->fieldref.name_and_type_index = read_u2(class_reader);
        break;
      case CONSTANT_METHODREF:
        constant->methodref.class_index = read_u2(class_reader);
        constant->methodref.name_and_type_index = read_u2(class_reader);
        break;
      case CONSTANT_NAME_AND_TYPE:
        constant->name_and_type.name_index = read_u2(class_reader);
        constant->name_and_type.descriptor_index = read_u2(class_reader);
        break;
      case CONSTANT_STRING:
        constant->string.string_index = read_u2(class_reader);
        break;
      case CONSTANT_UTF8:
        {
          u2_t length = read_u2(class_reader);
          unsigned char *bytes = malloc(length);
          if (bytes == NULL) {
            error("cannot malloc for constant utf8");
          }
          read_bytes(class_reader, bytes, (size_t)length);
          constant->utf8.length = length;
          constant->utf8.bytes = (char *)bytes;
          break;
        }
      default:
        error("undefined tag: %d: %d", i, tag);
        break;
    }
  }

  return 0;
}
