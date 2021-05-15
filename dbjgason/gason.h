#pragma once

#undef CLANG_OR_GNUC

#if defined (__clang__)  || defined(__GNUC__)
#define CLANG_OR_GNUC
#else
#undef CLANG_OR_GNUC
#error Currently only clang or gcc are supported
#endif
#define __STDC_WANT_LIB_EXT1__ 1
#include <stdbool.h>
#include <inttypes.h> // includes stdint.h
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

	/// 
	/// when defined causes use of strtod()
	/// that in turn fails to parse bad input
	/// thus some tests will fail on bad json input
	/// otherwise internal forgiving algorithm is used 
	/// and that has a problem
	/// https://github.com/vivkin/gason/issues/19#issue-98596074
	/// 
#define DBJ_GASON_STRICT_DOUBLE_PARSING

	///
	/// if defined all of the memory is one 
	/// array from the stack
	/// 
#define DBJ_GASON_STACK_ALLOC


	typedef enum dbj_gason_tag {
		JSON_NUMBER = 0,
		JSON_STRING,
		JSON_ARRAY,
		JSON_OBJECT,
		JSON_TRUE,
		JSON_FALSE,
		JSON_NULL = 0xF
	} dbj_gason_tag;

#define JSON_VALUE_PAYLOAD_MASK 0x00007FFFFFFFFFFFULL
#define JSON_VALUE_NAN_MASK 0x7FF8000000000000ULL
#define JSON_VALUE_TAG_MASK 0xF
#define JSON_VALUE_TAG_SHIFT 47

	typedef union {
		uint64_t ival;
		double fval;
	} dbj_gason_value;

	typedef struct dbj_gason_node  dbj_gason_node;
	struct dbj_gason_node {
		dbj_gason_value value;
		dbj_gason_node* next;
		char* key;
	};


	typedef struct {
		double dbl_val;
		enum dbj_gason_tag tag /*= JSON_NULL*/;
		void* payload /*= nullptr)*/;
	} dbj_gason_value_make_args;

	inline dbj_gason_value dbj_gason_value_make(dbj_gason_value_make_args args)
	{
		if (args.tag == JSON_NUMBER)
			return (dbj_gason_value) { .fval = args.dbl_val };

		assert((uintptr_t)args.payload <= JSON_VALUE_PAYLOAD_MASK);

		return (dbj_gason_value) {
			.ival = JSON_VALUE_NAN_MASK | ((uint64_t)args.tag << JSON_VALUE_TAG_SHIFT) | (uintptr_t)args.payload
		};

	}

	inline  bool dbj_gason_value_is_double(dbj_gason_value jval) {
		return (int64_t)jval.ival <= (int64_t)JSON_VALUE_NAN_MASK;
	}
	inline dbj_gason_tag dbj_gason_value_get_tag(dbj_gason_value jval) {
		return dbj_gason_value_is_double(jval)
			? JSON_NUMBER
			: (dbj_gason_tag)((jval.ival >> JSON_VALUE_TAG_SHIFT) & JSON_VALUE_TAG_MASK);
	}
	inline uint64_t dbj_gason_value_get_payload(dbj_gason_value jval) {
		assert(!dbj_gason_value_is_double(jval));
		return jval.ival & JSON_VALUE_PAYLOAD_MASK;
	}
	inline double dbj_gason_value_to_number(dbj_gason_value jval) {
		assert(dbj_gason_value_get_tag(jval) == JSON_NUMBER);
		return jval.fval;
	}
	inline char* dbj_gason_value_to_string(dbj_gason_value jval) {
		assert(dbj_gason_value_get_tag(jval) == JSON_STRING);
		return (char*)dbj_gason_value_get_payload(jval);
	}
	inline  dbj_gason_node* dbj_gason_value_to_node(dbj_gason_value jval) {
		assert(dbj_gason_value_get_tag(jval) == JSON_ARRAY || dbj_gason_value_get_tag(jval) == JSON_OBJECT);
		return (dbj_gason_node*)dbj_gason_value_get_payload(jval);
	}

	// alias but unused by design
	// because that makes code more readable
	typedef dbj_gason_node* JsonIterator;

	inline dbj_gason_node* dbj_gason_iterator_advance(dbj_gason_node* p) { return p = p->next; }

	inline bool dbj_gason_iterator_neq(const dbj_gason_node* left, const dbj_gason_node* right) {
		return left != right;
	}

	inline dbj_gason_node* dbj_gason_iterator_begin(dbj_gason_value o) {
		return (dbj_gason_node*) { dbj_gason_value_to_node(o) };
	}
	inline dbj_gason_node* dbj_gason_iterator_end() {
		return (dbj_gason_node*)0;
	}

#define json_node_for_each(/*dbj_gason_value*/ val ) \
	for ( dbj_gason_node* node_ = dbj_gason_iterator_begin(val); node_ != dbj_gason_iterator_end(); node_ = dbj_gason_iterator_advance(node_) )

#define JSON_ERRNO_MAP(XX)                           \
	XX(OK, "ok")                                     \
	XX(BAD_NUMBER, "bad number")                     \
	XX(BAD_STRING, "bad string")                     \
	XX(BAD_IDENTIFIER, "bad identifier")             \
	XX(STACK_OVERFLOW, "stack overflow")             \
	XX(STACK_UNDERFLOW, "stack underflow")           \
	XX(MISMATCH_BRACKET, "mismatch bracket")         \
	XX(UNEXPECTED_CHARACTER, "unexpected character") \
	XX(UNQUOTED_KEY, "unquoted key")                 \
	XX(BREAKING_BAD, "breaking bad")                 \
	XX(ALLOCATION_FAILURE, "allocation failure")

	enum JsonErrno {
#define XX(no, str) JSON_##no,
		JSON_ERRNO_MAP(XX)
#undef XX
	};

	const char* dbj_gason_error_str(int err);

	// by deafult dbj json uses a little chunked heap based allocator
	// it allocates large chunk and allocated from it untill next
	// large chunk is needed.
	// 
	// that is supposed to be faster.
	// 
	// dbj gason de allocation  happens only once and usualy before exit
	// that is a design forced onto users
	// that is very easy to implement, see the 
	// allocate deallocate in here for inspiration

#ifdef DBJ_JSON_USER_DEFINED_ALLOCATION
// if defined must provide 
// DBJ_JSON_ALLOC( SIZE_ )
// DBJ_JSON_FREE_BEFORE_EXIT( PTR_ )
#else 

	void* dbj_json_allocator_allocate(size_t);

#ifdef CLANG_OR_GNUC
	__attribute__((destructor)) void dbj_json_allocator_deallocate();
#endif

#endif // ! DBJ_JSON_USER_DEFINED_ALLOCATION


	int dbj_gason_parse(char* str, char** endptr, dbj_gason_value* value);


#ifdef __cplusplus
} // extern "C"
#endif


	///
	/// currently unused 
	/// 
#if 0
#ifdef CLANG_OR_GNUC

#undef _cleanup_
#define _cleanup_(x) __attribute__((cleanup(x)))

inline void dbj_cleanup_file_close(FILE** fpp) {
	FILE* fp = *fpp;

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
#undef  _cleanup_filep_ 
#define _cleanup_filep_ _cleanup_(dbj_cleanup_file_close)

inline void dbj_cleanup_freep(void* p) { free(*(void**)p); }
// usage:
// _cleanup_free_ char * data_ = malloc(0xFF, sizeof(char)) ;
#undef  _cleanup_free_
#define _cleanup_free_ _cleanup_(dbj_cleanup_freep)

#endif // CLANG_OR_GNUC
#endif // 0