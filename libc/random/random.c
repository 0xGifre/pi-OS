#include <random.h>

static unsigned int seed = 123456u;

unsigned int random(void) {
	seed = seed * 1103515245u + 12345u;
	return (seed / 65536u) % 32768u;
}






