
//#define JSMN_STATIC
// absence of this induces implementation
//#define JSMN_HEADER
#include "../jsmn.h"
#include "../systemd_macros.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

/////////////////////////////////////////////////////////////////////////////////////
#ifndef _cleanup_
#define _cleanup_(x) __attribute__((cleanup(x)))
#endif // _cleanup_(x)

static inline void file_close(FILE **fpp) {
  FILE *fp = *fpp;

  if (fp == stdin)
    return;

  if (fp == stdout)
    return;

  if (fp == stderr)
    return;

  if (fp)
    // of course much more error checking here
    fclose(fp);
}
// usage:
//   _cleanup_filep_ FILE * fpin = fopen(file_name, "r");
#define _cleanup_filep_ _cleanup_(file_close)

// usage:
// _cleanup_free_ char * data_ = malloc(0xFF, sizeof(char)) ;
static inline void freep(void *p) { free(*(void **)p); }
#define _cleanup_free_ _cleanup_(freep)

/////////////////////////////////////////////////////////////////////////////////////
// gigabyte = 1024 * 1024 * 1024 bytes
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
  fclose(fp_);
  return (dbj_string){length, buffer};
}
/////////////////////////////////////////////////////////////////////////////////////

/* Function realloc_it() is a wrapper function for standard realloc()
 * with one difference - it frees old memory pointer in case of realloc
 * failure. Thus, DO NOT use old data pointer in anyway after call to
 * realloc_it(). If your code has some kind of fallback algorithm if
 * memory can't be re-allocated - use standard realloc() instead.
 */
static inline void *realloc_it(void *ptrmem, size_t size) {

  assert(size > 1);
  void *p = realloc(ptrmem, size);
  assert(p);
  if (!p) {
    free(ptrmem);
    perror(__FILE__ " -- realloc() has failed");
    exit(EXIT_FAILURE);
  }
  return p;
}

/*
   Reading JSON from stdin and printing its content to stdout.

   NOTES:

   - this is a recursive function
   - it seems it returns no of elements printed

 */

static int recursive_dump_function(FILE *stream_out, const char *json_data,
                                   jsmntok_t *t, size_t count, int indent) {
  int i = 0, j = 0, k = 0;
  jsmntok_t *key = 0;

  if (count == 0) {
    return 0;
  }

  if (t->type == JSMN_PRIMITIVE) {
    fprintf(stream_out, "%.*s", t->end - t->start, json_data + t->start);
    return 1;
  } else if (t->type == JSMN_STRING) {
    fprintf(stream_out, "'%.*s'", t->end - t->start, json_data + t->start);
    return 1;
  } else if (t->type == JSMN_OBJECT) {
    fprintf(stream_out, "\n");
    j = 0;
    for (i = 0; i < t->size; i++) {
      for (k = 0; k < indent; k++) {
        fprintf(stream_out, "  ");
      }
      key = t + 1 + j;
      j += recursive_dump_function(stream_out, json_data, key, count - j,
                                   indent + 1);
      if (key->size > 0) {
        fprintf(stream_out, ": ");
        j += recursive_dump_function(stream_out, json_data, t + 1 + j,
                                     count - j, indent + 1);
      }
      fprintf(stream_out, "\n");
    }
    return j + 1;
  } else if (t->type == JSMN_ARRAY) {
    j = 0;
    fprintf(stream_out, "\n");
    for (i = 0; i < t->size; i++) {
      for (k = 0; k < indent - 1; k++) {
        fprintf(stream_out, "  ");
      }
      fprintf(stream_out, "   - ");
      j += recursive_dump_function(stream_out, json_data, t + 1 + j, count - j,
                                   indent + 1);
      fprintf(stream_out, "\n");
    }
    return j + 1;
  }
  return 0;
}

/////////////////////////////////////////////////////////////////////////////////////
FILE *fetch_json_stream(const char *const json_fname_) {
  FILE *stream_in = 0; // default

  if (json_fname_) {
    stream_in = fopen(json_fname_, "r");
    if (feof(stream_in)) {
      perror("Error reading json file, unexpected end of file\n");
      exit(EXIT_FAILURE);
    }
    if (ferror(stream_in)) {
      perror("Error reading json file ");
      exit(EXIT_FAILURE);
    }
  }

  return stream_in;
}
/////////////////////////////////////////////////////////////////////////////////////
int jsondump(int argc, const char **argv) {
  FILE *stream_out = stdout;
  _cleanup_filep_ FILE *stream_in = fetch_json_stream(argc > 1 ? argv[1] : 0);
  ////////////////////////////////
  dbj_string string_ = dbj_file_to_string(stream_in);
  _cleanup_free_ char *json_data = string_.data;
  const size_t json_data_len = string_.length;

  /* Prepare the parser */
  jsmn_parser parser_ = {};
  jsmn_init(&parser_);

  // probe the tokens number
  // result is just indicative as json data might be unclosed
  // when reading from a file
  int tokcount = jsmn_parse(&parser_, json_data, json_data_len, 0, 0);

  /* Allocate tokens apparently required  */
  _cleanup_free_ jsmntok_t *tok = realloc_it(0, sizeof(*tok) * tokcount);

  /* parsing rezult should be the same as tokcount
   *  but not when reading from a file
   */
  jsmn_init(&parser_);
  int jsmn_parse_rezult_ =
      jsmn_parse(&parser_, json_data, json_data_len, tok, tokcount);

  /* Invalid character inside JSON string */
  // if (jsmn_parse_rezult_ == JSMN_ERROR_INVAL) {
  //  perror("Invalid character inside JSON string");
  //  goto clean_before_exit;
  //}
  /*
  The string is not a full JSON packet, more bytes expected
  we ignore that when reading from a file
  */
  // if (jsmn_parse_rezult_ == JSMN_ERROR_PART) {
  //  free(tok);
  //  continue;
  //}

  if (jsmn_parse_rezult_ == JSMN_ERROR_NOMEM) {
    perror("This should not happen?");
    goto clean_before_exit;
  }
  recursive_dump_function(stream_out, json_data, tok, parser_.toknext, 0);

clean_before_exit:

  // free(tok);
  // free(json_data);
  /*if (stream_in != stdin)
    fclose(stream_in);*/

  return EXIT_SUCCESS;
}
