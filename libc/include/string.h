#ifndef _STRING_H
#define _STRING_H 1

#include <sys/cdefs.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int memcmp(const void*, const void*, size_t);
void* memcpy(void* __restrict, const void* __restrict, size_t);
void* memmove(void*, const void*, size_t);
void* memset(void*, int, size_t);
size_t strlen(const char*);

int strings_equal(const char* a, const char* b);
int starts_with(const char* text, const char* prefix);
char* next_token(char** cursor);
bool parse_i32(const char* s, int* out);
bool parse_float(const char* s, float* out);
void u32_to_string(unsigned int value, char* out);
void i32_to_string(int value, char* out);
void float_to_string(float value, char* out, int decimals);
size_t read_line(char* out, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
