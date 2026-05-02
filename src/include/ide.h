#ifndef KERNEL_IDE_H
#define KERNEL_IDE_H

#include <stdbool.h>
#include <stdint.h>

bool ide_identify(void);
bool ide_read_sector(uint32_t lba, void* buffer);
bool ide_write_sector(uint32_t lba, const void* buffer);
const char* ide_last_error(void);

#endif
