<h1> dbj gason </h1>
> 2011 MAY dbj at dbj dot org 

- turned into C11 code
- developing in Visual Studio 2019
  - using clang-cl compiler, only
- test suite is unchanged 

## Status 
- Port is done and generally working ok. 
- Original tests are used and are all passed (see the Number parsing bellow).
- using heap allocation "pretty print" works as before, but fails under stack memory builds
- json samples are included in the repo
- Right now working on documentation :wink:

## Problems

- C API
  - `gason.h` and `gason.c` are "pushed in the front" aka the root of the repo
    - folder `sampling` contains all of the rest, including the VS solution and project
  - looking into `gason.h` the C API logic and design, will be easy to decipher.
  - by looking into `dbj_gason_sampling.c` and `pretty-print.c` one should be able to understand the C API usage rather quickly.
  - Visual Studio debugger is your friend :wink:
- Naming
  - using `dbj_gason_` prefix no complete, W.I.P.
- Memory allocation
  - Memory is not released after parsing, this amount of allocated memory grows on each parse
  - Memory is not release until application exit, not good at all 
  - If this is used as lib, users should be able to release the memory periodically or whenever they need or want.
  - `DBJ_GASON_STACK_ALLOC` is defined makes use of stack, thus not heap allocation is used
    - if stack allocation is used, all the tests are passing (as before)
    - **BUG**: the "pretty print" does not work if stack allocation is in use, have to look into this
  - `DBJ_JSON_USER_DEFINED_ALLOCATION` is inside but it is not used
    - do not use
- Number parsing
  - `DBJ_GASON_STRICT_DOUBLE_PARSING` defined, makes gason use `strtod()`
  	- that in turn fails to parse bad input, thus some tests will naturally fail on bad json input
	- otherwise internal "forgiving" algorithm is used and that has a problem
		- see: https://github.com/vivkin/gason/issues/19#issue-98596074

> Original work in this repo is &copy; 2021 by dbj@dbj.org

---

Original readme

## gason
gason is new version of [vjson](https://code.google.com/p/vjson) parser. It's still very **fast** and have very **simple interface**. Completly new api, different internal representation and using new C++ standard features explains why parser get a new name.

- [Status](#status)
- [Problems](#problems)
- [gason](#gason)
- [Features](#features)
- [Installation](#installation)
- [Usage](#usage)
	- [Parsing](#parsing)
	- [Iteration](#iteration)
- [Notes](#notes)
	- [NaN-boxing](#nan-boxing)
	- [Memory management](#memory-management)
	- [Parser internals](#parser-internals)
- [Performance](#performance)
- [License](#license)

## Features
* No dependencies
* Small codebase (~450 loc)
* Small memory footprint (16-24B per value)

gason is **not strict** parser:
* Source buffer can contain more than one value (first will be parsed; pointer to the rest returns)
* Single number, string or identifier will be succesfully parsed
* Trailing `,` before closing `]` or `}` is not an error

gason is **destructive** parser, i.e. you **source buffer** will be **modified**! Strings stored as pointers to source buffer, where closing `"` (or any other symbol, if string have escape sequences) replaced with `'\0'`. Arrays and objects are represented as single linked list (without random access).

## Installation
1. Download latest [gason.h](https://raw.github.com/vivkin/gason/master/src/gason.h) and [gason.cpp](https://raw.github.com/vivkin/gason/master/src/gason.cpp)
2. Compile with C++11 support (`-std=c++11` flag for gcc/clang)
3. ...
4. PROFIT!

## Usage
### Parsing
```cpp
#include "gason.h"
...
char *source = get_useless_facebook_response(); // or read file, whatever
// do not forget terminate source string with 0
char *endptr;
dbj_gason_value value;
JsonAllocator allocator;
int status = dbj_gason_parse(source, &endptr, &value, allocator);
if (status != JSON_OK) {
	fprintf(stderr, "%s at %zd\n", dbj_gason_error_str(status), endptr - source);
	exit(EXIT_FAILURE);
}
```
All **values** will become **invalid** when **allocator** be **destroyed**. For print verbose error message see `printError` function in [pretty-print.cpp](pretty-print.cpp).

### Iteration
```cpp
double sum_and_print(dbj_gason_value o) {
	double sum = 0;
	switch (o.getTag()) {
	case JSON_NUMBER:
		printf("%g\n", o.toNumber());
		sum += o.toNumber();
		break;
	case JSON_STRING:
		printf("\"%s\"\n", o.toString());
		break;
	case JSON_ARRAY:
		for (auto i : o) {
			sum += sum_and_print(i->value);
		}
		break;
	case JSON_OBJECT:
		for (auto i : o) {
			printf("%s = ", i->key);
			sum += sum_and_print(i->value);
		}
		break;
	case JSON_TRUE:
		fprintf(stdout, "true");
		break;
	case JSON_FALSE:
		fprintf(stdout, "false");
		break;
	case JSON_NULL:
		printf("null\n");
		break;
	}
	return sum;
}
...
double sum = sum_and_print(value);
printf("sum of all numbers: %g\n", sum);
```
Arrays and Objects use the same `dbj_gason_node` struct, but for arrays valid only `next` and `value` fields!

## Notes
### NaN-boxing
gason stores values using NaN-boxing technique. By [IEEE-754](http://en.wikipedia.org/wiki/IEEE_floating_point) standard we have 2^52-1 variants for encoding double's [NaN](http://en.wikipedia.org/wiki/NaN). So let's use this to store value type and payload:
```
 sign
 |  exponent
 |  |
[0][11111111111][yyyyxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx]
				 |   |
				 tag |
					 payload
```
48 bits payload [enough](http://en.wikipedia.org/wiki/X86-64#Virtual_address_space_details) for store any pointer on x64. Numbers use zero tag, so infinity and nan are accessible.

### Memory management
JsonAllocator allocates big blocks of memory and use pointer bumping inside theese blocks for smaller allocations. Size of block can be tuned by *JSON_ZONE_SIZE* constant (default 4 KiB).

### Parser internals
> [05.11.13, 2:52:33] ???????? ????????????: ?? ?????????? ?????? ?????????? ???????? ???? ??????????????????!

Internally in `dbj_gason_parse` function nested arrays/objects stored in array of circulary linked list of `dbj_gason_node`. Size of that array can be tuned by *JSON_STACK_SIZE* constant (default 32).

## Performance

For build parser shootout:

1. `clone-enemy-parser.sh` (need mercurial, git, curl, nodejs)
2. `cmake -DCMAKE_BUILD_TYPE=Release -DSHOOTOUT=ON`

Test files downloads from random repos on github:
* `big.json` - just big with big count escape sequences
* `monster.json` - 3d model, lot of numbers
* `data.json` - many objects

First column - parse time in microseconds, second - traverse and sum all numbers.

Intel Core i7 2.3 GHz, OSX 10.9, clang-500.2.79:
```
shootout/big.json: length 6072200
	 gason      20092us         71us 	(5520251617769.000000)
	 vjson      19723us        156us 	(5520251617769.000000)
	sajson      25226us        128us 	(5520251617769.000000)
 rapidjson      18093us         96us 	(5520251617769.000000)
shootout/data.json: length 17333
	 gason         75us          4us 	(3754.333493)
	 vjson         80us          5us 	(3754.333471)
	sajson        117us          8us 	(3754.333493)
 rapidjson         91us          7us 	(3754.333493)
shootout/monster.json: length 196473
	 gason        924us        162us 	(34474757.667613)
	 vjson       2218us        396us 	(34474757.667621)
	sajson       1898us        380us 	(34474757.667613)
 rapidjson       2210us        446us 	(34474757.667613)
```
Samsung Galaxy Note II (GT-N7100), Android 4.3, gcc 4.8.2:
```
I/ruberoid( 8944): /sdcard/Download/shootout/big.json: length 6072200
I/ruberoid( 8944):      gason      66269us        561us 	(5520251617769.000000)
I/ruberoid( 8944):      vjson      59052us       1058us 	(5520251617769.000000)
I/ruberoid( 8944):     sajson      75240us       1573us 	(5520251617769.000000)
I/ruberoid( 8944):  rapidjson      82948us        808us 	(5520251617769.000000)
I/ruberoid( 8944): /sdcard/Download/shootout/data.json: length 17333
I/ruberoid( 8944):      gason        328us         32us 	(3754.333493)
I/ruberoid( 8944):      vjson        316us         54us 	(3754.333471)
I/ruberoid( 8944):     sajson        291us         55us 	(3754.333493)
I/ruberoid( 8944):  rapidjson        386us         48us 	(3754.333493)
I/ruberoid( 8944): /sdcard/Download/shootout/monster.json: length 196473
I/ruberoid( 8944):      gason       6330us       1342us 	(34474757.667613)
I/ruberoid( 8944):      vjson       9481us       2236us 	(34474757.667621)
I/ruberoid( 8944):     sajson       6400us       1971us 	(34474757.667613)
I/ruberoid( 8944):  rapidjson       7464us       1745us 	(34474757.667613)
```

## License
Distributed under the MIT license. Copyright (c) 2013-2015, Ivan Vashchaev
