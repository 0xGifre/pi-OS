#include <stdio.h>

#include <shell_input.h>
#include <shell_string.h>
#include <tty.h>

static void execute_command(const char* line) {
	if (line[0] == '\0') {
		return;
	}

	if (shell_strings_equal(line, "help")) {
		printf("Commands: help, clear, echo <text>\n");
		return;
	}
	if (shell_strings_equal(line, "clear")) {
		terminal_clear();
		return;
	}
	if (shell_starts_with(line, "echo ")) {
		printf("%s\n", line + 5);
		return;
	}

	printf("Unknown command: %s\n", line);
}

void kernel_main(void) {
	char line[128];

	terminal_initialize();
	printf("Sonarix OS shell\n");
	printf("Type 'help' for commands.\n\n");

	for (;;) {
		printf("> ");
		shell_read_line(line, sizeof(line));
		execute_command(line);
	}
}
