#include <stddef.h>
#include <string.h>

char* next_token(char** cursor) {
	char* s = *cursor;
	while (*s == ' ') {
		s++;
	}
	if (*s == '\0') {
		*cursor = s;
		return NULL;
	}

	char* start = s;
	while (*s != '\0' && *s != ' ') {
		s++;
	}
	if (*s == ' ') {
		*s = '\0';
		s++;
	}
	*cursor = s;
	return start;
}
