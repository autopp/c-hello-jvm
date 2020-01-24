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
typedef uint32_t u4_t;

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

typedef struct {
  u2_t attribute_name_index;
  u4_t attribute_length;
  char *info;
} attributes_t;

typedef struct {
  u2_t access_flags;
  u2_t name_index;
  u2_t descriptor_index;
  u2_t attributes_count;
  attributes_t *attributes;
} method_t;

void init_reader(reader_t *reader, size_t size, char *data) {
  reader->size = size;
  reader->cur = 0;
  reader->data = data;
}

void read_bytes(reader_t *reader, char *buf, size_t n) {
  if (reader->cur + n > reader->size) {
    error("cannot read from reader");
  }
  memcpy(buf, reader->data + reader->cur, n);
  reader->cur += n;
}

u1_t read_u1(reader_t *reader) {
  char ch;
  read_bytes(reader, &ch, 1);
  return (u1_t)ch;
}

u2_t read_u2(reader_t *reader) {
  char buf[2];
  read_bytes(reader, buf, 2);
  return ((u2_t)buf[0]) << 8 | (u2_t)buf[1];
}

u4_t read_u4(reader_t *reader) {
  char buf[4];
  read_bytes(reader, buf, 4);
  return ((u4_t)buf[0] << 24 | (u4_t)buf[1] << 16 | (u4_t)buf[2] << 8 | (u4_t)buf[3]);
}

void read_attribute(reader_t *reader, attributes_t *attribute) {
  attribute->attribute_name_index = read_u2(reader);
  u4_t attribute_length = read_u4(reader);
  attribute->attribute_length = attribute_length;
  char *info = malloc(attribute_length);
  if (info == NULL) {
    error("cannot malloc for attribute");
  }
  read_bytes(reader, info, attribute_length);
  attribute->info = info;
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
  char magic[4];
  read_bytes(class_reader, magic, 4);
  read_u2(class_reader); // major
  read_u2(class_reader); // minor

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
          char *bytes = malloc(length);
          if (bytes == NULL) {
            error("cannot malloc for constant utf8");
          }
          read_bytes(class_reader, bytes, (size_t)length);
          constant->utf8.length = length;
          constant->utf8.bytes = bytes;
          break;
        }
      default:
        error("undefined tag: %d: %d", i, tag);
        break;
    }
  }

  read_u2(class_reader); // accsess flags
  read_u2(class_reader); // this class
  read_u2(class_reader); // super class
  read_u2(class_reader); // interface count
  read_u2(class_reader); // field count

  u2_t method_count = read_u2(class_reader);
  method_t methods[method_count];
  for (int i = 0; i < method_count; i++) {
    method_t *method = methods + i;
    method->access_flags = read_u2(class_reader);
    method->name_index = read_u2(class_reader);
    method->descriptor_index = read_u2(class_reader);
    u2_t attributes_count = read_u2(class_reader);
    method->attributes_count = attributes_count;
    method->attributes = malloc(sizeof(attributes_t) * attributes_count);
    for (int j = 0; j < attributes_count; j++) {
      read_attribute(class_reader, method->attributes + j);
    }
  }

  // read attributes of class
  u2_t attributes_count = read_u2(class_reader);
  attributes_t attributes[attributes_count];
  for (int i = 0; i < attributes_count; i++) {
    read_attribute(class_reader, attributes + i);
  }

  // find main method
  method_t *main_method = NULL;
  for (int i = 0; i < method_count; i++) {
    method_t *method = &methods[i];
    constant_t *c = &constant_pool[method->name_index];
    if (c->common.tag != CONSTANT_UTF8) {
      error("method name is not UTF8: %x", c->common.tag);
    }
    char method_name[c->utf8.length + 1];
    memcpy(method_name, c->utf8.bytes, c->utf8.length);
    method_name[c->utf8.length] = '\0';
    if (strcmp(method_name, "main") == 0) {
      main_method = method;
      break;
    }
  }

  if (main_method == NULL) {
    error("cannot found main method");
  }

  // find code attribute of main
  attributes_t *code_attribute = NULL;
  for (int i = 0; i < main_method->attributes_count; i++) {
    attributes_t *attribute = &main_method->attributes[i];
    constant_t *c = &constant_pool[attribute->attribute_name_index];
    if (c->common.tag != CONSTANT_UTF8) {
      error("main method attribute name is not UTF8: %x", c->common.tag);
    }
    char attribute_name[c->utf8.length + 1];
    memcpy(attribute_name, c->utf8.bytes, c->utf8.length);
    attribute_name[c->utf8.length] = '\0';
    if (strcmp(attribute_name, "Code") == 0) {
      code_attribute = attribute;
      break;
    }
  }

  if (code_attribute == NULL) {
    error("cannot found code attribute of main method");
  }

  reader_t code_reader_instance;
  reader_t *code_reader = &code_reader_instance;
  init_reader(code_reader, code_attribute->attribute_length, code_attribute->info);
  read_u2(code_reader); //max stack
  read_u2(code_reader); //max stack
  u4_t code_length = read_u4(code_reader);
  char code[code_length];
  read_bytes(code_reader, code, code_length);
  u2_t exeption_table_length = read_u2(code_reader);
  if (exeption_table_length != 0) {
    error("exception is not supported yet");
  }

  u2_t code_attributes_count = read_u2(code_reader);
  attributes_t code_attributes[code_attributes_count];
  for (int i = 0; i < code_attributes_count; i++) {
    read_attribute(code_reader, &code_attributes[i]);
  }

  // cleanup attribute of code attribute
  for (int i = 0; i < code_attributes_count; i++) {
    free(code_attributes[i].info);
  }

  // cleanup attributes of class
  for (int i = 0; i < attributes_count; i++) {
    free(attributes[i].info);
  }

  // cleanup methods
  for (int i = 0; i < method_count; i++) {
    for (int j = 0; j < methods[i].attributes_count; j++) {
      free(methods[i].attributes[j].info);
    }
    free(methods[i].attributes);
  }

  // cleanup constant pool
  for (int i = 0; i < constant_pool_count; i++) {
    constant_t *c = &constant_pool[i];
    if (c->common.tag == CONSTANT_UTF8) {
      free(c->utf8.bytes);
    }
  }

  free(class_file_data);

  return 0;
}
