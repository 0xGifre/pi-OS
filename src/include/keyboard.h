#ifndef _KERNEL_KEYBOARD_H
#define _KERNEL_KEYBOARD_H

#include <stdbool.h>

bool keyboard_try_read_char(char* out);
char keyboard_read_char(void);

#endif
