#include <stddef.h>

#include <string.h>

bool parse_float(const char* s, float* out) {
	if (s == NULL || *s == '\0') {
		return false;
	}

	float sign = 1.0f;
	if (*s == '-') {
		sign = -1.0f;
		s++;
	} else if (*s == '+') {
		s++;
	}

	float value = 0.0f;
	bool saw_digit = false;
	while (*s >= '0' && *s <= '9') {
		saw_digit = true;
		value = value * 10.0f + (float)(*s - '0');
		s++;
	}

	if (*s == '.') {
		s++;
		float place = 0.1f;
		while (*s >= '0' && *s <= '9') {
			saw_digit = true;
			value += (float)(*s - '0') * place;
			place *= 0.1f;
			s++;
		}
	}

	if (!saw_digit || *s != '\0') {
		return false;
	}

	*out = sign * value;
	return true;
}
