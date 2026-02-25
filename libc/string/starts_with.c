#include <string.h>

int starts_with(const char* text, const char* prefix) {
	while (*prefix) {
		if (*text != *prefix) {
			return 0;
		}
		text++;
		prefix++;
	}
	return 1;
}
