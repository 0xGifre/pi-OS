#include <string.h>

void u32_to_string(unsigned int value, char* out) {
	char rev[16];
	int n = 0;

	if (value == 0u) {
		out[0] = '0';
		out[1] = '\0';
		return;
	}

	while (value > 0u && n < (int)sizeof(rev)) {
		rev[n++] = (char)('0' + (value % 10u));
		value /= 10u;
	}

	for (int i = 0; i < n; i++) {
		out[i] = rev[n - 1 - i];
	}
	out[n] = '\0';
}
