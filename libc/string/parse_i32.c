#include <stddef.h>

#include <string.h>

bool parse_i32(const char* s, int* out) {
	if (s == NULL || *s == '\0') {
		return false;
	}

	int sign = 1;
	if (*s == '-') {
		sign = -1;
		s++;
	} else if (*s == '+') {
		s++;
	}
	if (*s == '\0') {
		return false;
	}

	int value = 0;
	while (*s >= '0' && *s <= '9') {
		value = value * 10 + (*s - '0');
		s++;
	}
	if (*s != '\0') {
		return false;
	}

	*out = sign * value;
	return true;
}
