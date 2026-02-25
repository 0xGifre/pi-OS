#include <string.h>

void float_to_string(float value, char* out, int decimals) {
	if (value < 0.0f) {
		*out++ = '-';
		value = -value;
	}

	int int_part = (int)value;
	float frac = value - (float)int_part;

	char rev[16];
	int ri = 0;
	if (int_part == 0) {
		rev[ri++] = '0';
	} else {
		while (int_part > 0 && ri < (int)sizeof(rev)) {
			rev[ri++] = (char)('0' + (int_part % 10));
			int_part /= 10;
		}
	}
	while (ri > 0) {
		*out++ = rev[--ri];
	}

	*out++ = '.';
	for (int i = 0; i < decimals; i++) {
		frac *= 10.0f;
		int d = (int)frac;
		if (d < 0) d = 0;
		if (d > 9) d = 9;
		*out++ = (char)('0' + d);
		frac -= (float)d;
	}
	*out = '\0';
}
