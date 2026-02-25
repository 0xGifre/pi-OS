#include <string.h>

void i32_to_string(int value, char* out) {
	if (value < 0) {
		*out++ = '-';
		unsigned int mag = (unsigned int)(-(value + 1)) + 1u;
		u32_to_string(mag, out);
		return;
	}
	u32_to_string((unsigned int)value, out);
}
