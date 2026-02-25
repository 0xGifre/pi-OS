#include <math.h>

float sqrtf(float x) {
	if (x < 0.0f) return -1.0f;
	if (x == 0.0f) return 0.0f;

	float guess = x;
	for (int i = 0; i < 10; i++) {
		guess = 0.5f * (guess + x / guess);
	}
	return guess;
}
