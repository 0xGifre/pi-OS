#include <math.h>

float tanf(float x) {
	float c = cosf(x);
	if (c == 0.0f) return 1e9f;
	return sinf(x) / c;
}
