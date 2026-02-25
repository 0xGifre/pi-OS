#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <io.h>
#include <keyboard.h>

static const char scancode_map[128] = {
	0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
	'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0
};

static const char scancode_map_shift[128] = {
	0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0, 'A', 'S',
	'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
	'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0
};

static bool left_shift;
static bool right_shift;

static bool keyboard_data_ready(void) {
	return (inb(0x64) & 0x01) != 0;
}

bool keyboard_try_read_char(char* out) {
	if (!keyboard_data_ready()) {
		return false;
	}

	uint8_t scancode = inb(0x60);
	bool released = (scancode & 0x80u) != 0;
	scancode &= 0x7Fu;

	if (scancode == 0x2A) {
		left_shift = !released;
		return false;
	}
	if (scancode == 0x36) {
		right_shift = !released;
		return false;
	}
	if (released) {
		return false;
	}

	bool shift = left_shift || right_shift;
	char c = shift ? scancode_map_shift[scancode] : scancode_map[scancode];
	if (c == 0) {
		return false;
	}

	*out = c;
	return true;
}

char keyboard_read_char(void) {
	char c = 0;
	while (!keyboard_try_read_char(&c)) {
	}
	return c;
}
