#include <shell_string.h>

int shell_starts_with(const char* text, const char* prefix) {
	while (*prefix) {
		if (*text != *prefix) {
			return 0;
		}
		text++;
		prefix++;
	}
	return 1;
}
