#include <stdint.h>

#include "gdt.h"

#define GDT_ACCESS_PRESENT    0x80
#define GDT_ACCESS_RING0      0x00
#define GDT_ACCESS_CODE_DATA  0x10
#define GDT_ACCESS_EXECUTABLE 0x08
#define GDT_ACCESS_DIRECTION  0x04
#define GDT_ACCESS_READ_WRITE 0x02

#define GDT_FLAGS_GRANULARITY_4K 0x80
#define GDT_FLAGS_32BIT          0x40

struct gdt_entry {
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_middle;
	uint8_t access;
	uint8_t granularity;
	uint8_t base_high;
} __attribute__((packed));

struct gdt_pointer {
	uint16_t limit;
	uint32_t base;
} __attribute__((packed));

static struct gdt_entry gdt[3];
static struct gdt_pointer gdt_ptr;

extern void gdt_flush(uint32_t gdt_ptr_addr);

static void gdt_set_entry(uint32_t index, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t flags) {
	gdt[index].limit_low = limit & 0xFFFF;
	gdt[index].base_low = base & 0xFFFF;
	gdt[index].base_middle = (base >> 16) & 0xFF;
	gdt[index].access = access;
	gdt[index].granularity = ((limit >> 16) & 0x0F) | (flags & 0xF0);
	gdt[index].base_high = (base >> 24) & 0xFF;
}

void gdt_install(void) {
	gdt_ptr.limit = sizeof(gdt) - 1;
	gdt_ptr.base = (uint32_t)&gdt;

	gdt_set_entry(0, 0, 0, 0, 0);
	gdt_set_entry(1, 0, 0xFFFFF,
	              GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 |
	                  GDT_ACCESS_CODE_DATA | GDT_ACCESS_EXECUTABLE |
	                  GDT_ACCESS_READ_WRITE,
	              GDT_FLAGS_GRANULARITY_4K | GDT_FLAGS_32BIT);
	gdt_set_entry(2, 0, 0xFFFFF,
	              GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 |
	                  GDT_ACCESS_CODE_DATA | GDT_ACCESS_READ_WRITE,
	              GDT_FLAGS_GRANULARITY_4K | GDT_FLAGS_32BIT);

	gdt_flush((uint32_t)&gdt_ptr);
}
