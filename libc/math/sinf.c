#include <math.h>

#define PI 3.1415926535f

float sinf(float x) {
	float res = 0.0f;
	float term = x;
	int n = 1;
	int sign = 1;

	while (x > PI) x -= 2.0f * PI;
	while (x < -PI) x += 2.0f * PI;

	for (int i = 0; i < 10; i++) {
		res += sign * term;
		n += 2;
		term *= x * x / ((n - 1) * n);
		sign *= -1;
	}

	return res;
}
