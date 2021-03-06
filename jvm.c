#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

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

typedef void (*method_func_t)(constant_t *args[], int args_count);

typedef struct {
  const char *name;
  method_func_t body;
} method_entry_t;

typedef struct object object_t;

typedef struct {
  const char *name;
  object_t *value;
} field_entry_t;
struct object {
  field_entry_t *fields;
  method_entry_t *methods;
};

typedef enum {
  OP_GETSTATIC = 0xB2,
  OP_LDC = 0x12,
  OP_INVOKEVIRTUAL = 0xB6,
  OP_RETURN = 0xB1
} opcode_t;

void init_reader(reader_t *reader, size_t size, char *data) {
  reader->size = size;
  reader->cur = 0;
  reader->data = data;
}

char *skip_bytes(reader_t *reader, size_t n) {
  if (reader->cur + n > reader->size) {
    error("cannot read from reader");
  }
  char *cur_ptr = reader->data + reader->cur;
  reader->cur += n;

  return cur_ptr;
}

void read_bytes(reader_t *reader, char *buf, size_t n) {
  if (reader->cur + n > reader->size) {
    error("cannot read from reader");
  }
  memcpy(buf, reader->data + reader->cur, n);
  reader->cur += n;
}

int is_eod(reader_t *reader) {
  return reader->cur >= reader->size;
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

#define copy_utf8(name, c)                           \
  char name[(c)->utf8.length + 1];                   \
  do                                                 \
  {                                                  \
    assert_constant((c), CONSTANT_UTF8);             \
    memcpy(name, (c)->utf8.bytes, (c)->utf8.length); \
    name[(c)->utf8.length] = '\0';                   \
  } while (0)

#define assert_constant(c, t)\
  do {\
    if ((c)->common.tag != (t)) {\
      error("expected %s, but got 0x%02x", #t, (c)->common.tag);\
    }\
  } while (0)

void read_attribute(reader_t *reader, attributes_t *attribute) {
  attribute->attribute_name_index = read_u2(reader);
  u4_t attribute_length = read_u4(reader);
  attribute->attribute_length = attribute_length;
  attribute->info = skip_bytes(reader, attribute_length);
}

void println(constant_t *args[], int args_count) {
  if (args_count != 1) {
    error("expected 1, but got %d", args_count);
  }
  copy_utf8(message, args[0]);
  printf("%s\n", message);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    error("usage: %s classfile", argv[0]);
  }
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
    size_t n = fread(class_file_data + class_file_size, 1, BUF_SIZE, fp);
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
  skip_bytes(class_reader, 4); // magic
  skip_bytes(class_reader, 2); // major
  skip_bytes(class_reader, 2); // minor

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
          constant->utf8.length = length;
          constant->utf8.bytes = skip_bytes(class_reader, (size_t)length);
          break;
        }
      default:
        error("undefined tag: %d: %d", i, tag);
        break;
    }
  }

  skip_bytes(class_reader, 2); // accsess flags
  skip_bytes(class_reader, 2); // this class
  skip_bytes(class_reader, 2); // super class
  skip_bytes(class_reader, 2); // interface count
  skip_bytes(class_reader, 2); // field count

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

  if (!is_eod(class_reader)) {
    error("class file contents are remained yet");
  }

  // find main method
  method_t *main_method = NULL;
  for (int i = 0; i < method_count; i++) {
    method_t *method = &methods[i];
    constant_t *c = &constant_pool[method->name_index];
    copy_utf8(method_name, c);
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
    copy_utf8(attribute_name, c);
    if (strcmp(attribute_name, "Code") == 0) {
      code_attribute = attribute;
      break;
    }
  }

  if (code_attribute == NULL) {
    error("cannot found code attribute of main method");
  }

  reader_t code_attr_reader_instance;
  reader_t *code_attr_reader = &code_attr_reader_instance;
  init_reader(code_attr_reader, code_attribute->attribute_length, code_attribute->info);
  u2_t max_stack = read_u2(code_attr_reader);
  skip_bytes(code_attr_reader, 2); // max locals
  u4_t code_length = read_u4(code_attr_reader);
  char code[code_length];
  read_bytes(code_attr_reader, code, code_length);
  u2_t exeption_table_length = read_u2(code_attr_reader);
  if (exeption_table_length != 0) {
    error("exception is not supported yet");
  }

  u2_t code_attributes_count = read_u2(code_attr_reader);
  attributes_t code_attributes[code_attributes_count];
  for (int i = 0; i < code_attributes_count; i++) {
    read_attribute(code_attr_reader, &code_attributes[i]);
  }

  // initialize runtime
  field_entry_t empty_fields[] = {
    { NULL }
  };
  method_entry_t empty_methods[] = {
    { NULL }
  };
  method_entry_t out_methods[] = {
    { "println", println },
    { NULL }
  };
  object_t out_object = {
    empty_fields,
    out_methods
  };
  field_entry_t system_fields[] = {
    { "out", &out_object },
    { NULL }
  };
  object_t system_class = {
    system_fields,
    empty_methods
  };

  struct {
    const char *path;
    object_t *class;
  } classes[] = {
    { "java/lang/System", &system_class },
    { NULL }
  };

  // eval bytecodes;
  reader_t code_reader_instance;
  reader_t *code_reader = &code_reader_instance;
  init_reader(code_reader, code_length, code);

  constant_t *operand_stack[max_stack];
  size_t sp = 0;
  bool finished = false;
  while (!finished) {
    u1_t opcode = read_u1(code_reader);
    switch (opcode) {
      case OP_GETSTATIC: { // getstatic
        u2_t operand = read_u2(code_reader);
        operand_stack[sp++] = &constant_pool[operand];
        break;
      }
      case OP_LDC: { // ldc
        u1_t operand = read_u1(code_reader);
        constant_t *str = &constant_pool[operand];
        assert_constant(str, CONSTANT_STRING);
        operand_stack[sp++] = &constant_pool[str->string.string_index];
        break;
      }
      case OP_INVOKEVIRTUAL: { // invokevirtual
        constant_t *cp_info = &constant_pool[read_u2(code_reader)];
        assert_constant(cp_info, CONSTANT_METHODREF);
        constant_t *name_and_type = &constant_pool[cp_info->methodref.name_and_type_index];
        assert_constant(name_and_type, CONSTANT_NAME_AND_TYPE);

        copy_utf8(method_name, &constant_pool[name_and_type->name_and_type.name_index]);
        copy_utf8(descriptor, &constant_pool[name_and_type->name_and_type.descriptor_index]);

        // count args
        int args_count = 0;
        const char *p = descriptor;
        while ((p = strchr(p, ';')) != NULL) {
          p++;
          args_count++;
        }
        constant_t *args[args_count];
        for (int i = 0; i < args_count; i++) {
          args[i] = operand_stack[--sp];
        }

        constant_t *context = operand_stack[--sp];
        copy_utf8(base_class, &constant_pool[constant_pool[context->fieldref.class_index].class.name_index]);

        copy_utf8(base_class_target, &constant_pool[constant_pool[context->fieldref.name_and_type_index].name_and_type.name_index]);

        // search class
        object_t *class = NULL;
        for (int i = 0; classes[i].path != NULL; i++) {
          if (strcmp(classes[i].path, base_class) == 0) {
            class = classes[i].class;
            break;
          }
        }
        if (class == NULL) {
          error("cannot found base class: %s", base_class);
        }

        // search referenced object
        object_t *receiver = NULL;
        for (int i = 0; class->fields[i].name != NULL; i++) {
          if (strcmp(class->fields[i].name, base_class_target) == 0) {
            receiver = class->fields[i].value;
            break;
          }
        }
        if (receiver == NULL) {
          error("cannot found field: %s", base_class_target);
        }

        // search method
        method_func_t func = NULL;
        for (int i = 0; receiver->methods[i].name != NULL; i++) {
          if (strcmp(receiver->methods[i].name, method_name) == 0) {
            func = receiver->methods[i].body;
            break;
          }
        }
        if (func == NULL) {
          error("cannot found method: %s", method_name);
        }

        // invoke method
        func(args, args_count);

        break;
      }
      case OP_RETURN: // return
        finished = true;
        break;
      default:
        error("unknown opecode: 0x%x", opcode);
    }
  }

  // cleanup methods
  for (int i = 0; i < method_count; i++) {
    free(methods[i].attributes);
  }

  free(class_file_data);

  return 0;
}
