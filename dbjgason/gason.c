#include "gason.h"

const char* dbj_gason_error_str(int err) {
	switch (err) {
#define XX(no, str) \
	case JSON_##no: \
		return str;
		JSON_ERRNO_MAP(XX)
#undef XX
	default:
		return "unknown";
	}
}


#if defined(_WIN32)

#if defined(_M_AMD64)
#include <intrin.h>
#endif

// this is RtlSecureZeroMemory  from winnt.h

__forceinline void* dbj_gason_zero_memory(
	void* ptr,
	size_t cnt
)
{
	volatile char* vptr = (volatile char*)ptr;
#if defined(_M_AMD64)
	__stosb((unsigned char*)((unsigned long long)vptr), 0, cnt);
#else
	while (cnt) {
#if !defined(_M_CEE) && (defined(_M_ARM) || defined(_M_ARM64))
		__iso_volatile_store8(vptr, 0);
#else
		* vptr = 0;
#endif
		vptr++;
		cnt--;
	}

#endif // _M_AMD64

	return ptr;
}
#else // ! _WIN32

#ifndef __STDC_WANT_LIB_EXT1__
#define __STDC_WANT_LIB_EXT1__ 1
#endif // __STDC_WANT_LIB_EXT1__
#include <string.h>

__forceinline void* dbj_gason_zero_memory(
	void* ptr,
	size_t cnt
) {
	return memset_s(ptr, 0, cnt);
}
#endif // ! _WIN32

/*
* this is actually nested json nodes depth allowed
*/
#define JSON_STACK_SIZE 32

#ifdef DBJ_GASON_STACK_ALLOC

enum { dbj_gason_one_gb = 1 << 30, dbj_gason_stack_size = dbj_gason_one_gb };

static void* dbj_gason_private_stack_space(void) {
	static unsigned char private_stack_space_[dbj_gason_stack_size] = { 0 };
	return private_stack_space_;
}

static struct {
	void* head_;
	void* level_;
	size_t used_;
} dbj_gason_stack = {/* all zeroes */ };



void dbj_gason_allocator_deallocate() {
	if (dbj_gason_stack.used_ > 0) {
#ifndef NDEBUG
		dbj_gason_zero_memory(dbj_gason_stack.head_, dbj_gason_stack_size);
#endif
		dbj_gason_stack.head_ = dbj_gason_private_stack_space();
		dbj_gason_stack.level_ = dbj_gason_stack.head_;
		dbj_gason_stack.used_ = 0;
	}
}

#ifdef CLANG_OR_GNUC
__attribute__((constructor))
#endif
static void dbj_gason_stack_inialize(void)
{
	assert(dbj_gason_stack.used_ == 0);
	dbj_gason_stack.head_ = dbj_gason_private_stack_space();
	dbj_gason_stack.level_ = dbj_gason_stack.head_;
	dbj_gason_stack.used_ = 0;
}

void* dbj_gason_allocator_allocate(size_t size)
{
#ifndef CLANG_OR_GNUC
	if (dbj_gason_stack.used_ == 0) dbj_gason_stack_inialize();
#endif 
	// not brilliant: hardcoded alignment
	size = (size + 7) & ~7;

	if ((dbj_gason_stack.used_ + size) <= dbj_gason_stack_size) {
		unsigned char* p = dbj_gason_stack.level_ + size;
		dbj_gason_stack.used_ += size;
		return p;
	}
	/// can not ask for more
	return NULL;
}

#else // ! DBJ_GASON_STACK_ALLOC

/*
* semi clever and probably futile allocator attempt
*
* should be computed (once) or should be
* externaly definable
* for all desktop and laptop maqchines these days (2021)
* 0xFFFF is very small memory block
* but for IOT devices it is definitely not
* but then IOT devices will not allo heap allocations
*/
#define JSON_ZONE_SIZE 0xFFFF

typedef struct Zone Zone;

static struct Zone {
	Zone* next;
	size_t used;
} *head = 0;

void* dbj_gason_allocator_allocate(size_t size)
{
	// not brilliant: hardcoded alignment
	size = (size + 7) & ~7;

	if (head && head->used + size <= JSON_ZONE_SIZE) {
		char* p = (char*)head + head->used;
		head->used += size;
		return p;
	}

	size_t allocSize = sizeof(Zone) + size;

	Zone* zone = (Zone*)calloc(1, allocSize <= JSON_ZONE_SIZE ? JSON_ZONE_SIZE : allocSize);

	if (zone == NULL)
		return NULL;
	zone->used = allocSize;
	if (allocSize <= JSON_ZONE_SIZE || head == NULL) {
		zone->next = head;
		head = zone;
	}
	else {
		zone->next = head->next;
		head->next = zone;
	}
	return (char*)zone + sizeof(Zone);
}


// destructor call
void dbj_gason_allocator_deallocate() {
	while (head) {
		Zone* next = head->next;
		free(head);
		head = next;
	}
}

#endif // ! DBJ_GASON_STACK_ALLOC

static inline bool isspace(char c) {
	return c == ' ' || (c >= '\t' && c <= '\r');
}

static inline bool isdelim(char c) {
	return c == ',' || c == ':' || c == ']' || c == '}' || isspace(c) || !c;
}

static inline bool isdigit(char c) {
	return c >= '0' && c <= '9';
}

static inline bool isxdigit(char c) {
	return (c >= '0' && c <= '9') || ((c & ~' ') >= 'A' && (c & ~' ') <= 'F');
}

static inline int char2int(char c) {
	if (c <= '9')
		return c - '0';
	return (c & ~' ') - 'A' + 10;
}

static double string2double(char* s, char** endptr) {

#ifdef DBJ_GASON_STRICT_DOUBLE_PARSING

	double result = strtod(s, endptr);
	return result;
#else // ! DBJ_GASON_STRICT_DOUBLE_PARSING
	// problem: https://github.com/vivkin/gason/issues/19#issue-98596074
	double result = 0; //  strtod(s, NULL);
	char ch = *s;
	if (ch == '-')
		++s;

	while (isdigit(*s))
		result = (result * 10) + (*s++ - '0');

	if (*s == '.') {
		++s;

		double fraction = 1;
		while (isdigit(*s)) {
			fraction *= 0.1;
			result += (*s++ - '0') * fraction;
		}
	}

	if (*s == 'e' || *s == 'E') {
		++s;

		double base = 10;
		if (*s == '+')
			++s;
		else if (*s == '-') {
			++s;
			base = 0.1;
		}

		unsigned int exponent = 0;
		while (isdigit(*s))
			exponent = (exponent * 10) + (*s++ - '0');

		double power = 1;
		for (; exponent; exponent >>= 1, base *= base)
			if (exponent & 1)
				power *= base;

		result *= power;
	}
	*endptr = s;
	return ch == '-' ? -result : result;
#endif // // ! DBJ_GASON_STRICT_DOUBLE_PARSING
}

static inline dbj_gason_node* insertAfter(dbj_gason_node* tail, dbj_gason_node* node) {
	if (!tail)
		return node->next = node;
	node->next = tail->next;
	tail->next = node;
	return node;
}

static inline dbj_gason_value listToValue(enum dbj_gason_tag tag, dbj_gason_node* tail) {
	if (tail) {
		dbj_gason_node* head = tail->next;
		tail->next = NULL;
		return dbj_gason_value_make((dbj_gason_value_make_args) { .tag = tag, .payload = head });
	}
	return dbj_gason_value_make((dbj_gason_value_make_args) { .tag = tag, .payload = NULL });
}

int dbj_gason_parse(char* s, char** endptr, dbj_gason_value* value)
{
	dbj_gason_node* tails[JSON_STACK_SIZE] = { 0 };
	enum dbj_gason_tag tags[JSON_STACK_SIZE] = { };
	char* keys[JSON_STACK_SIZE] = { 0 };
	dbj_gason_value o = {};
	int pos = -1;
	bool separator = true;
	dbj_gason_node* node = 0;
	*endptr = s;

	while (*s) {
		while (isspace(*s)) {
			++s;
			if (!*s) break;
		}
		*endptr = s++;
		switch (**endptr) {
		case '-':
			if (!isdigit(*s) && *s != '.') {
				*endptr = s;
				return JSON_BAD_NUMBER;
			}
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			o = dbj_gason_value_make((dbj_gason_value_make_args) { .dbl_val = string2double(*endptr, &s), .tag = JSON_NUMBER });
			if (!isdelim(*s)) {
				*endptr = s;
				return JSON_BAD_NUMBER;
			}
			break;
		case '"':
			o = dbj_gason_value_make((dbj_gason_value_make_args) { .tag = JSON_STRING, .payload = s });
			for (char* it = s; *s; ++it, ++s) {
				int c = *it = *s;
				if (c == '\\') {
					c = *++s;
					switch (c) {
					case '\\':
					case '"':
					case '/':
						*it = c;
						break;
					case 'b':
						*it = '\b';
						break;
					case 'f':
						*it = '\f';
						break;
					case 'n':
						*it = '\n';
						break;
					case 'r':
						*it = '\r';
						break;
					case 't':
						*it = '\t';
						break;
					case 'u':
						c = 0;
						for (int i = 0; i < 4; ++i) {
							if (isxdigit(*++s)) {
								c = c * 16 + char2int(*s);
							}
							else {
								*endptr = s;
								return JSON_BAD_STRING;
							}
						}
						if (c < 0x80) {
							*it = c;
						}
						else if (c < 0x800) {
							*it++ = 0xC0 | (c >> 6);
							*it = 0x80 | (c & 0x3F);
						}
						else {
							*it++ = 0xE0 | (c >> 12);
							*it++ = 0x80 | ((c >> 6) & 0x3F);
							*it = 0x80 | (c & 0x3F);
						}
						break;
					default:
						*endptr = s;
						return JSON_BAD_STRING;
					}
				}
				else if ((unsigned int)c < ' ' || c == '\x7F') {
					*endptr = s;
					return JSON_BAD_STRING;
				}
				else if (c == '"') {
					*it = 0;
					++s;
					break;
				}
			}
			if (!isdelim(*s)) {
				*endptr = s;
				return JSON_BAD_STRING;
			}
			break;
		case 't':
			if (!(s[0] == 'r' && s[1] == 'u' && s[2] == 'e' && isdelim(s[3])))
				return JSON_BAD_IDENTIFIER;
			o = dbj_gason_value_make((dbj_gason_value_make_args) { .tag = JSON_TRUE });
			s += 3;
			break;
		case 'f':
			if (!(s[0] == 'a' && s[1] == 'l' && s[2] == 's' && s[3] == 'e' && isdelim(s[4])))
				return JSON_BAD_IDENTIFIER;
			o = dbj_gason_value_make((dbj_gason_value_make_args) { .tag = JSON_FALSE });
			s += 4;
			break;
		case 'n':
			if (!(s[0] == 'u' && s[1] == 'l' && s[2] == 'l' && isdelim(s[3])))
				return JSON_BAD_IDENTIFIER;
			o = dbj_gason_value_make((dbj_gason_value_make_args) { .tag = JSON_NULL });
			s += 3;
			break;
		case ']':
			if (pos == -1)
				return JSON_STACK_UNDERFLOW;
			if (tags[pos] != JSON_ARRAY)
				return JSON_MISMATCH_BRACKET;
			o = listToValue(JSON_ARRAY, tails[pos--]);
			break;
		case '}':
			if (pos == -1)
				return JSON_STACK_UNDERFLOW;
			if (tags[pos] != JSON_OBJECT)
				return JSON_MISMATCH_BRACKET;
			if (keys[pos] != NULL)
				return JSON_UNEXPECTED_CHARACTER;
			o = listToValue(JSON_OBJECT, tails[pos--]);
			break;
		case '[':
			if (++pos == JSON_STACK_SIZE)
				return JSON_STACK_OVERFLOW;
			tails[pos] = NULL;
			tags[pos] = JSON_ARRAY;
			keys[pos] = NULL;
			separator = true;
			continue;
		case '{':
			if (++pos == JSON_STACK_SIZE)
				return JSON_STACK_OVERFLOW;
			tails[pos] = NULL;
			tags[pos] = JSON_OBJECT;
			keys[pos] = NULL;
			separator = true;
			continue;
		case ':':
			if (separator || keys[pos] == NULL)
				return JSON_UNEXPECTED_CHARACTER;
			separator = true;
			continue;
		case ',':
			if (separator || keys[pos] != NULL)
				return JSON_UNEXPECTED_CHARACTER;
			separator = true;
			continue;
		case '\0':
			continue;
		default:
			return JSON_UNEXPECTED_CHARACTER;
		}

		separator = false;

		if (pos == -1) {
			*endptr = s;
			*value = o;
			return JSON_OK;
		}

		if (tags[pos] == JSON_OBJECT) {
			if (!keys[pos]) {
				if (dbj_gason_value_get_tag(o) != JSON_STRING)
					return JSON_UNQUOTED_KEY;
				keys[pos] = dbj_gason_value_to_string(o);
				continue;
			}
			if ((node = (dbj_gason_node*)dbj_gason_allocator_allocate(sizeof(dbj_gason_node))) == NULL)
				return JSON_ALLOCATION_FAILURE;
			tails[pos] = insertAfter(tails[pos], node);
			tails[pos]->key = keys[pos];
			keys[pos] = NULL;
		}
		else {
			if ((node = (dbj_gason_node*)dbj_gason_allocator_allocate(sizeof(dbj_gason_node))) == NULL)
				return JSON_ALLOCATION_FAILURE;
			tails[pos] = insertAfter(tails[pos], node);
		}
		tails[pos]->value = o;
	} // while
	return JSON_BREAKING_BAD;
}
