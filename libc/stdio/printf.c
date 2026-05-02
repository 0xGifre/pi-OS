#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static bool print(const char* data, size_t length) {
	const unsigned char* bytes = (const unsigned char*) data;
	for (size_t i = 0; i < length; i++)
		if (putchar(bytes[i]) == EOF)
			return false;
	return true;
}

static int print_unsigned(unsigned int value, unsigned int base) {
	static const char digits[] = "0123456789abcdef";
	char buffer[32];
	int length = 0;

	if (value == 0) {
		return print("0", 1) ? 1 : -1;
	}

	while (value != 0 && length < (int)sizeof(buffer)) {
		buffer[length++] = digits[value % base];
		value /= base;
	}

	for (int i = 0; i < length / 2; i++) {
		char tmp = buffer[i];
		buffer[i] = buffer[length - 1 - i];
		buffer[length - 1 - i] = tmp;
	}

	return print(buffer, length) ? length : -1;
}

int printf(const char* restrict format, ...) {
	va_list parameters;
	va_start(parameters, format);

	int written = 0;

	while (*format != '\0') {
		size_t maxrem = INT_MAX - written;

		if (format[0] != '%' || format[1] == '%') {
			if (format[0] == '%')
				format++;
			size_t amount = 1;
			while (format[amount] && format[amount] != '%')
				amount++;
			if (maxrem < amount) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(format, amount))
				return -1;
			format += amount;
			written += amount;
			continue;
		}

		const char* format_begun_at = format++;

		if (*format == 'c') {
			format++;
			char c = (char) va_arg(parameters, int /* char promotes to int */);
			if (!maxrem) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(&c, sizeof(c)))
				return -1;
			written++;
		} else if (*format == 's') {
			format++;
			const char* str = va_arg(parameters, const char*);
			size_t len = strlen(str);
			if (maxrem < len) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(str, len))
				return -1;
			written += len;
		} else if (*format == 'u') {
			format++;
			int amount = print_unsigned(va_arg(parameters, unsigned int), 10);
			if (amount < 0 || maxrem < (size_t)amount)
				return -1;
			written += amount;
		} else if (*format == 'x') {
			format++;
			int amount = print_unsigned(va_arg(parameters, unsigned int), 16);
			if (amount < 0 || maxrem < (size_t)amount)
				return -1;
			written += amount;
		} else if (*format == 'd') {
			format++;
			int value = va_arg(parameters, int);
			unsigned int magnitude = value < 0 ?
			                         (unsigned int)(-(value + 1)) + 1u :
			                         (unsigned int)value;

			if (value < 0) {
				if (!maxrem || !print("-", 1))
					return -1;
				written++;
				maxrem--;
			}

			int amount = print_unsigned(magnitude, 10);
			if (amount < 0 || maxrem < (size_t)amount)
				return -1;
			written += amount;
		} else {
			format = format_begun_at;
			size_t len = strlen(format);
			if (maxrem < len) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(format, len))
				return -1;
			written += len;
			format += len;
		}
	}

	va_end(parameters);
	return written;
}
