# dbj at dbj dot org json lib

Original at: https://github.com/zserge/jsmn.git
# JSMN


## Usage


Download `jsmn.h`, include it, done.

```c
#include "jsmn.h"

. . .

jsmn_parser p;
/* We expect no more than 128 JSON tokens */
jsmntok_t t[128]; 

jsmn_init(&p);
r = jsmn_parse(&p, s, strlen(s), t, 128);

. . .

```

Since jsmn is a single-header, header-only library, for more complex use cases you might need to define additional macros. 

`#define JSMN_STATIC` hides all jsmn API symbols by making them static. Also, if you want to include `jsmn.h` from multiple C files, to avoid duplication of symbols you may define  `JSMN_HEADER` macro.

```c
/* In every file that uses jsmn API */
#define JSMN_HEADER
#include "jsmn.h"

/* In one *.c file for jsmn implementation: */
#include "jsmn.h"
```

### API

Token type ID's are described by `jsmntype_t`:
```c
typedef enum {
	JSMN_UNDEFINED = 0,
	JSMN_OBJECT = 1,
	JSMN_ARRAY = 2,
	JSMN_STRING = 3,
	JSMN_PRIMITIVE = 4
} jsmntype_t;
```
**Note:** Unlike JSON data types, primitive tokens are not divided into
numbers, booleans and null, because one can easily tell the type using the
first character:

* `'t'`, `'f'` - boolean 
* `'n'` - null
* `'-'`, `'0'..'9'` - number

**Token** is an object of `jsmntok_t` type:
```c
typedef struct {
	jsmntype_t type; // Token type ID
	int start;       // Token start position
	int end;         // Token end position
	int size;        // Number of child (nested) tokens
} jsmntok_t;
```
> **Note:** string tokens point to the first character after
the opening quote and the last symbol before final quote. This was made 
to simplify string extraction from JSON data.

## Parsing

All job is done by `jsmn_parser` object. You can initialize a new parser using:
```c
jsmn_parser parser;
jsmntok_t tokens[10];

jsmn_init(&parser);

const char * json_string = obtain_json_as_string();

// js - pointer to JSON string
// tokens - an array of tokens available
// 10 - number of tokens available
int tokens_used = 
jsmn_parse(&parser, json_string, strlen(json_string), tokens, 10);
```
This will create a parser, that tries to parse up to 10 JSON tokens from the `json_string` string.

A non-negative return value of `jsmn_parse` is the number of tokens actually used by the parser.

Passing NULL instead of the tokens array would not store parsing results, 
but instead the function will return the number of tokens needed to parse the given string. 
This can be useful if you don't know yet how many tokens to allocate.

```c
int tokens_required = 
jsmn_parse(&parser, json_string, strlen(json_string), NULL, 0);
```

### Parsing Logic

If something goes wrong, you will get an error. 

* `JSMN_ERROR_INVAL` - bad token, JSON string is corrupted
* `JSMN_ERROR_NOMEM` - not enough tokens, JSON string is too large
* `JSMN_ERROR_PART` - JSON string is too short, expecting more JSON data

> If you get `JSMN_ERROR_NOMEM`, you can re-allocate more tokens and call `jsmn_parse` again.  

If you read json data from the stream, you can periodically call `jsmn_parse` and check if return value is `JSMN_ERROR_PART`.

You will get that error until you reach the end of JSON data.

[1]: http://www.json.org/
[2]: http://zserge.com/jsmn.html
