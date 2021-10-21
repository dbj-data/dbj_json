///
/// currenlty this is "just" a separate compilation unit
/// one could just add it to the one's project and use it
///
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32) && !defined(NDEBUG)
#include <execinfo.h>
#include <signal.h>
#endif
#include "../gason.h"

///
/// gigabyte = 1024 * 1024 * 1024 bytes
///
enum { dbj_file_max_len = 1024 * 1024 * 1024 };

typedef struct {
  size_t length;
  char *data;
} dbj_string;

dbj_string dbj_file_to_string(FILE *fp_) {
  assert(fp_);
  fseek(fp_, 0, SEEK_END);
  long length = ftell(fp_);
  assert(length < dbj_file_max_len);
  fseek(fp_, 0, SEEK_SET);
  char *buffer = calloc(length + 1, sizeof(char));
  assert(buffer);
  fread(buffer, 1, length, fp_);
  buffer[length] = '\0';
  return (dbj_string){length, buffer};
}
///
///
///

#undef DBJ_GASON_PRINT
#define DBJ_GASON_PRINT(...) fprintf(stdout, __VA_ARGS__)
#undef DBJ_GASON_ERR_PRINT
#define DBJ_GASON_ERR_PRINT(...) fprintf(stderr, __VA_ARGS__)

enum { SHIFT_WIDTH = 2 };

static void dumpString(const char *s) {
  fputc('"', stdout);
  while (*s) {
    int c = *s++;
    switch (c) {
    case '\b':
      DBJ_GASON_PRINT("\\b");
      break;
    case '\f':
      DBJ_GASON_PRINT("\\f");
      break;
    case '\n':
      DBJ_GASON_PRINT("\\n");
      break;
    case '\r':
      DBJ_GASON_PRINT("\\r");
      break;
    case '\t':
      DBJ_GASON_PRINT("\\t");
      break;
    case '\\':
      DBJ_GASON_PRINT("\\\\");
      break;
    case '"':
      DBJ_GASON_PRINT("\\\"");
      break;
    default:
      DBJ_GASON_PRINT("%c", c);
    }
  }
  DBJ_GASON_PRINT("%s\"", s);
}

#define STARTING_INDENT 0

static void dump_value_recursive_fun(dbj_gason_value o, int indent /* = 0 */) {

  switch (dbj_gason_value_get_tag(o)) {
  case JSON_NUMBER:
    DBJ_GASON_PRINT("%f", dbj_gason_value_to_number(o));
    break;
  case JSON_STRING:
    dumpString(dbj_gason_value_to_string(o));
    break;
  case JSON_ARRAY:
    // It is not necessary to use o.toNode() to check if an array or object
    // is empty before iterating over its members, we do it here to allow
    // nicer pretty printing.
    if (!dbj_gason_value_to_node(o)) {
      DBJ_GASON_PRINT("[]");
      break;
    }
    DBJ_GASON_PRINT("[\n");
    json_node_for_each(o) {
      DBJ_GASON_PRINT("%*s", indent + SHIFT_WIDTH, "");
      dump_value_recursive_fun(node_->value, indent + SHIFT_WIDTH);
      DBJ_GASON_PRINT(node_->next ? ",\n" : "\n");
    }
    DBJ_GASON_PRINT("%*s]", indent, "");
    break;
  case JSON_OBJECT:
    if (!dbj_gason_value_to_node(o)) {
      DBJ_GASON_PRINT("{}");
      break;
    }
    DBJ_GASON_PRINT("{\n");
    json_node_for_each(o) {
      DBJ_GASON_PRINT("%*s", indent + SHIFT_WIDTH, "");
      dumpString(node_->key);
      DBJ_GASON_PRINT(": ");
      dump_value_recursive_fun(node_->value, indent + SHIFT_WIDTH);
      DBJ_GASON_PRINT(node_->next ? ",\n" : "\n");
    }
    DBJ_GASON_PRINT("%*s}", indent, "");
    break;
  case JSON_TRUE:
    DBJ_GASON_PRINT("true");
    break;
  case JSON_FALSE:
    DBJ_GASON_PRINT("false");
    break;
  case JSON_NULL:
    DBJ_GASON_PRINT("null");
    break;
  }
}

static void printError(const char *filename, int status, char *endptr,
                       char *source, size_t size) {
  char *s = endptr;
  while (s != source && *s != '\n')
    --s;
  if (s != endptr && s != source)
    ++s;

  int lineno = 0;
  for (char *it = s; it != source; --it) {
    if (*it == '\n') {
      ++lineno;
    }
  }

  int column = (int)(endptr - s);

  DBJ_GASON_ERR_PRINT("%s:%d:%d: %s\n", filename, lineno + 1, column + 1,
                      dbj_gason_error_str(status));

  while (s != source + size && *s != '\n') {
    int c = *s++;
    switch (c) {
    case '\b':
      DBJ_GASON_ERR_PRINT("\\b");
      column += 1;
      break;
    case '\f':
      DBJ_GASON_ERR_PRINT("\\f");
      column += 1;
      break;
    case '\n':
      DBJ_GASON_ERR_PRINT("\\n");
      column += 1;
      break;
    case '\r':
      DBJ_GASON_ERR_PRINT("\\r");
      column += 1;
      break;
    case '\t':
      DBJ_GASON_ERR_PRINT("%*s", SHIFT_WIDTH, "");
      column += SHIFT_WIDTH - 1;
      break;
    case '\0':
      DBJ_GASON_ERR_PRINT("\"");
      break;
    default:
      fputc(c, stderr);
    }
  }

  DBJ_GASON_ERR_PRINT("\n%*s\n", column + 1, "^");
}

int pretty_print(int argc, char *argv[static 2]) {
  char *source_ = 0;
  const char *TEST_FILE = argv[1];
  FILE *fp = 0;
  if (argc > 1) {
    fp = fopen(TEST_FILE, "r");
  } else {
    perror(__FILE__ " need 2 arguments.");
    goto clear_before_exit;
  }

  if (!fp) {
    perror(TEST_FILE);
    goto clear_before_exit;
  }

  // also zero limited
  dbj_string string_ = dbj_file_to_string(fp);
  fclose(fp);
  fp = NULL;
  source_ = string_.data;
  size_t sourceSize = string_.length;

  char *endptr = 0;
  dbj_gason_value value = {};
  // dbj_gason_allocator allocator;
  int status = dbj_gason_parse(source_, &endptr, &value);
  if (status != JSON_OK) {
    printError((argc > 1 && strcmp(argv[1], "-")) ? argv[1] : "-stdin-", status,
               endptr, source_, sourceSize);
    goto clear_before_exit;
  }

  DBJ_GASON_PRINT("\nfile dump: %s\n\n", TEST_FILE);
  dump_value_recursive_fun(value, STARTING_INDENT);
  DBJ_GASON_PRINT("\n");

clear_before_exit:
  if (fp)
    fclose(fp);
  free(source_);
  return 0;
}
