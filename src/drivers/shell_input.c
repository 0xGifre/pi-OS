#include <stddef.h>

#include <keyboard.h>
#include <shell_input.h>
#include <tty.h>

size_t shell_read_line(char* out, size_t capacity) {
	size_t len = 0;
	for (;;) {
		char c = keyboard_read_char();
		if (c == '\n') {
			terminal_putchar('\n');
			out[len] = '\0';
			return len;
		}
		if (c == '\b') {
			if (len > 0) {
				len--;
				terminal_backspace();
			}
			continue;
		}
		if (c < 32 || c > 126) {
			continue;
		}
		if (len + 1 < capacity) {
			out[len++] = c;
			terminal_putchar(c);
		}
	}
}
