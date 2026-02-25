#include <stdio.h>

#include "../../libc/include/math.h"
#include "../../libc/include/random.h"
#include "../../libc/include/string.h"
#include "../include/tty.h"
#include "../include/vga.h"

static void print_calc_help(void) {
	printf("List of available operations:\n");
	printf("ADD          Addition.\n");
	printf("SUB          Subtraction.\n");
	printf("MUL          Multiplication.\n");
	printf("DIV          Division.\n");
	printf("SIN          Sinus.\n");
	printf("COS          Cosinus.\n");
	printf("TAN          Tangent.\n");
	printf("SQRT         Square root.\n");
	printf("HELP         Show this help.\n");
	printf("QUIT         Exit the calculator.\n");
}

static bool run_calc_command(char* line) {
	char* cursor = line;
	char* cmd = next_token(&cursor);
	if (cmd == NULL) {
		return true;
	}

	if (strings_equal(cmd, "help")) {
		print_calc_help();
		return true;
	}

	if (strings_equal(cmd, "quit")) {
		return false;
	}

	float a = 0.0f;
	float b = 0.0f;
	float result = 0.0f;
	char result_str[32];

	if (strings_equal(cmd, "sin") || strings_equal(cmd, "cos") ||
	    strings_equal(cmd, "tan") || strings_equal(cmd, "sqrt")) {
		char* a_tok = next_token(&cursor);
		if (!parse_float(a_tok, &a)) {
			printf("Usage: %s <x>\n", cmd);
			return true;
		}
		if (strings_equal(cmd, "sin")) result = sinf(a);
		else if (strings_equal(cmd, "cos")) result = cosf(a);
		else if (strings_equal(cmd, "tan")) result = tanf(a);
		else {
			if (a < 0.0f) {
				printf("Error: sqrt negative\n");
				return true;
			}
			result = sqrtf(a);
		}
		float_to_string(result, result_str, 4);
		printf("Result: %s\n", result_str);
		return true;
	}

	if (strings_equal(cmd, "add") || strings_equal(cmd, "sub") ||
	    strings_equal(cmd, "mul") || strings_equal(cmd, "div")) {
		char* a_tok = next_token(&cursor);
		char* b_tok = next_token(&cursor);
		if (!parse_float(a_tok, &a) || !parse_float(b_tok, &b)) {
			printf("Usage: %s <a> <b>\n", cmd);
			return true;
		}

		if (strings_equal(cmd, "add")) result = a + b;
		else if (strings_equal(cmd, "sub")) result = a - b;
		else if (strings_equal(cmd, "mul")) result = a * b;
		else {
			if (b == 0.0f) {
				printf("Error: divide by zero\n");
				return true;
			}
			result = a / b;
		}
		float_to_string(result, result_str, 4);
		printf("Result: %s\n", result_str);
		return true;
	}

	uint8_t prev_color = terminal_getcolor();
	terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
	printf("Unknown calc command: %s\n", cmd);
	terminal_setcolor(prev_color);
	return true;
}

static void start_calc_mode(void) {
	char line[128];

	printf("Calculator mode. Type 'help' for operations.\n");
	for (;;) {
		printf("calc> ");
		read_line(line, sizeof(line));
		if (!run_calc_command(line)) {
			break;
		}
	}
}

static void execute_command(char* line) {
	if (line[0] == '\0') {
		return;
	}

	if (strings_equal(line, "help")) {
		printf("For more information on a specific command, type HELP command-name\n");
		printf("CALC                    Opens a simple calculator.\n");
		printf("CLEAR                   Calls the screen clearing function.\n");
		printf("ECHO <text>             Prints the given text.\n");
		printf("RAND                    Prints a random number.\n");
		printf("RAND -r <min> <max>     Prints a random number in the given range.\n");
		return;
	}
	if (strings_equal(line, "calc")) {
		start_calc_mode();
		return;
	}
	if (strings_equal(line, "clear")) {
		terminal_clear();
		return;
	}
	if (strings_equal(line, "rand")) {
		char number[16];
		u32_to_string(random(), number);
		printf("Random: %s\n", number);
		return;
	}
	if (starts_with(line, "rand ")) {
		char* cursor = line;
		char* cmd = next_token(&cursor);
		char* mode = next_token(&cursor);
		char* min_tok = next_token(&cursor);
		char* max_tok = next_token(&cursor);
		char* extra = next_token(&cursor);
		(void)cmd;

		if (mode == NULL || !strings_equal(mode, "-r") ||
		    min_tok == NULL || max_tok == NULL || extra != NULL) {
			printf("Usage: rand -r <min> <max>\n");
			return;
		}

		int min = 0;
		int max = 0;
		if (!parse_i32(min_tok, &min) || !parse_i32(max_tok, &max)) {
			printf("Usage: rand -r <min> <max>\n");
			return;
		}
		if (max < min) {
			printf("Error: max < min\n");
			return;
		}

		unsigned int span = (unsigned int)(max - min) + 1u;
		int ranged = min + (int)(random() % span);
		char number[16];
		i32_to_string(ranged, number);
		printf("Random: %s\n", number);
		return;
	}
	if (starts_with(line, "echo ")) {
		printf("%s\n", line + 5);
		return;
	}

	uint8_t prev_color = terminal_getcolor();
	terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
	printf("Unknown command: %s\n", line);
	terminal_setcolor(prev_color);
}

void kernel_main(void) {
	char line[128];

	terminal_initialize();
	printf("Sonarix OS shell\n");
	
	printf("Type 'help' for commands.\n\n");

	for (;;) {
		printf("> ");
		read_line(line, sizeof(line));
		execute_command(line);
	}
}
