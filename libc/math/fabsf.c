#include <math.h>

float fabsf(float x) {
	union {
		float f;
		unsigned int u;
	} val;

	val.f = x;
	val.u &= 0x7FFFFFFFu;
	return val.f;
}
