#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "../../libc/include/math.h"
#include "../../libc/include/random.h"
#include "../../libc/include/string.h"
#include "../include/ide.h"
#include "../include/multiboot.h"
#include "../include/tty.h"
#include "../include/vga.h"

static const struct multiboot_info* boot_info;
static const struct multiboot_module* initrd_module;

#define RAM_FILE_MAX_FILES 16
#define RAM_FILE_NAME_MAX 32
#define RAM_FILE_DATA_MAX 512

#define INITRD_MAGIC 0x44524950u
#define INITRD_VERSION 1u
#define INITRD_NAME_MAX 56u

#define DISK_FS_MAGIC 0x53464950u
#define DISK_FS_VERSION 1u
#define DISK_FS_LBA 2048u
#define DISK_FS_SECTORS 32u
#define DISK_SECTOR_SIZE 512u
#define DISK_FS_IMAGE_SIZE (DISK_FS_SECTORS * DISK_SECTOR_SIZE)

struct initrd_header {
	uint32_t magic;
	uint32_t version;
	uint32_t file_count;
	uint32_t entries_offset;
	uint32_t data_offset;
} __attribute__((packed));

struct initrd_entry {
	char name[INITRD_NAME_MAX];
	uint32_t offset;
	uint32_t size;
} __attribute__((packed));

struct ram_file {
	bool used;
	char name[RAM_FILE_NAME_MAX];
	char data[RAM_FILE_DATA_MAX];
	size_t size;
};

struct disk_fs_header {
	uint32_t magic;
	uint32_t version;
	uint32_t file_count;
	uint32_t entries_offset;
	uint32_t data_offset;
} __attribute__((packed));

struct disk_fs_entry {
	char name[RAM_FILE_NAME_MAX];
	uint32_t offset;
	uint32_t size;
} __attribute__((packed));

static struct ram_file ram_files[RAM_FILE_MAX_FILES];
static uint8_t disk_fs_image[DISK_FS_IMAGE_SIZE];
static uint8_t disk_sector_buffer[DISK_SECTOR_SIZE];

static void copy_string_limited(char* dst, size_t dst_size, const char* src) {
	size_t i = 0;

	if (dst_size == 0) {
		return;
	}

	while (src[i] != '\0' && i + 1 < dst_size) {
		dst[i] = src[i];
		i++;
	}
	dst[i] = '\0';
}

static struct ram_file* ram_file_find(const char* name) {
	for (size_t i = 0; i < RAM_FILE_MAX_FILES; i++) {
		if (ram_files[i].used && strings_equal(ram_files[i].name, name)) {
			return &ram_files[i];
		}
	}
	return NULL;
}

static struct ram_file* ram_file_alloc(void) {
	for (size_t i = 0; i < RAM_FILE_MAX_FILES; i++) {
		if (!ram_files[i].used) {
			return &ram_files[i];
		}
	}
	return NULL;
}

static bool ram_file_valid_name(const char* name) {
	if (name == NULL || name[0] == '\0') {
		return false;
	}
	return strlen(name) < RAM_FILE_NAME_MAX;
}

static bool ram_file_create(const char* name) {
	struct ram_file* file = NULL;

	if (!ram_file_valid_name(name) || ram_file_find(name) != NULL) {
		return false;
	}

	file = ram_file_alloc();
	if (file == NULL) {
		return false;
	}

	file->used = true;
	copy_string_limited(file->name, sizeof(file->name), name);
	file->data[0] = '\0';
	file->size = 0;
	return true;
}

static bool ram_file_write_bytes(const char* name, const char* data, size_t size) {
	struct ram_file* file = ram_file_find(name);

	if (!ram_file_valid_name(name)) {
		return false;
	}

	if (file == NULL) {
		if (!ram_file_create(name)) {
			return false;
		}
		file = ram_file_find(name);
	}

	if (size >= RAM_FILE_DATA_MAX) {
		size = RAM_FILE_DATA_MAX - 1;
	}

	memcpy(file->data, data, size);
	file->data[size] = '\0';
	file->size = size;
	return true;
}

static bool ram_file_write(const char* name, const char* data) {
	if (data == NULL) {
		data = "";
	}
	return ram_file_write_bytes(name, data, strlen(data));
}

static bool ram_file_remove(const char* name) {
	struct ram_file* file = ram_file_find(name);
	if (file == NULL) {
		return false;
	}

	memset(file, 0, sizeof(*file));
	return true;
}

static void ram_file_list(void) {
	bool any = false;

	for (size_t i = 0; i < RAM_FILE_MAX_FILES; i++) {
		if (ram_files[i].used) {
			printf("%s  %u bytes\n", ram_files[i].name, (unsigned int)ram_files[i].size);
			any = true;
		}
	}

	if (!any) {
		printf("No files.\n");
	}
}

static void ram_file_clear_all(void) {
	memset(ram_files, 0, sizeof(ram_files));
}

static void ram_file_cat(const char* name) {
	struct ram_file* file = ram_file_find(name);
	if (file == NULL) {
		printf("File not found: %s\n", name);
		return;
	}

	terminal_write(file->data, file->size);
	printf("\n");
}

static bool disk_read_sectors(uint32_t lba, uint8_t* buffer, uint32_t count) {
	for (uint32_t i = 0; i < count; i++) {
		if (!ide_read_sector(lba + i, buffer + i * DISK_SECTOR_SIZE)) {
			return false;
		}
	}
	return true;
}

static bool disk_write_sectors(uint32_t lba, const uint8_t* buffer, uint32_t count) {
	for (uint32_t i = 0; i < count; i++) {
		if (!ide_write_sector(lba + i, buffer + i * DISK_SECTOR_SIZE)) {
			return false;
		}
	}
	return true;
}

static bool save_ramfs_to_disk(void) {
	struct disk_fs_header* header = (struct disk_fs_header*)disk_fs_image;
	struct disk_fs_entry* entries = NULL;
	uint32_t file_count = 0;
	uint32_t data_cursor = 0;

	memset(disk_fs_image, 0, sizeof(disk_fs_image));

	header->magic = DISK_FS_MAGIC;
	header->version = DISK_FS_VERSION;
	header->entries_offset = sizeof(*header);
	header->data_offset = sizeof(*header) + RAM_FILE_MAX_FILES * sizeof(struct disk_fs_entry);
	entries = (struct disk_fs_entry*)(disk_fs_image + header->entries_offset);

	for (size_t i = 0; i < RAM_FILE_MAX_FILES; i++) {
		if (!ram_files[i].used) {
			continue;
		}
		if (header->data_offset + data_cursor + ram_files[i].size > sizeof(disk_fs_image)) {
			return false;
		}

		copy_string_limited(entries[file_count].name, sizeof(entries[file_count].name),
		                    ram_files[i].name);
		entries[file_count].offset = data_cursor;
		entries[file_count].size = (uint32_t)ram_files[i].size;
		memcpy(disk_fs_image + header->data_offset + data_cursor,
		       ram_files[i].data, ram_files[i].size);

		data_cursor += (uint32_t)ram_files[i].size;
		file_count++;
	}

	header->file_count = file_count;
	return disk_write_sectors(DISK_FS_LBA, disk_fs_image, DISK_FS_SECTORS);
}

static bool load_ramfs_from_disk(void) {
	const struct disk_fs_header* header = NULL;
	const struct disk_fs_entry* entries = NULL;

	if (!disk_read_sectors(DISK_FS_LBA, disk_fs_image, DISK_FS_SECTORS)) {
		return false;
	}

	header = (const struct disk_fs_header*)disk_fs_image;
	if (header->magic != DISK_FS_MAGIC ||
	    header->version != DISK_FS_VERSION ||
	    header->file_count > RAM_FILE_MAX_FILES ||
	    header->entries_offset >= sizeof(disk_fs_image) ||
	    header->data_offset >= sizeof(disk_fs_image)) {
		return false;
	}
	if (header->entries_offset + header->file_count * sizeof(*entries) > sizeof(disk_fs_image)) {
		return false;
	}

	entries = (const struct disk_fs_entry*)(disk_fs_image + header->entries_offset);
	ram_file_clear_all();

	for (uint32_t i = 0; i < header->file_count; i++) {
		uint32_t data_start = header->data_offset + entries[i].offset;
		uint32_t data_end = data_start + entries[i].size;

		if (data_start > sizeof(disk_fs_image) ||
		    data_end > sizeof(disk_fs_image) ||
		    data_end < data_start) {
			continue;
		}

		ram_file_write_bytes(entries[i].name,
		                     (const char*)disk_fs_image + data_start,
		                     entries[i].size);
	}

	return true;
}

static void load_initrd_archive(const struct multiboot_module* module) {
	const struct initrd_header* header = NULL;
	const struct initrd_entry* entries = NULL;
	const char* module_base = NULL;
	uint32_t module_size = 0;

	if (module == NULL || module->mod_end <= module->mod_start) {
		return;
	}

	module_base = (const char*)module->mod_start;
	module_size = module->mod_end - module->mod_start;
	if (module_size < sizeof(*header)) {
		return;
	}

	header = (const struct initrd_header*)module_base;
	if (header->magic != INITRD_MAGIC || header->version != INITRD_VERSION) {
		return;
	}

	if (header->entries_offset > module_size ||
	    header->data_offset > module_size ||
	    header->file_count > RAM_FILE_MAX_FILES) {
		return;
	}

	if (header->entries_offset + header->file_count * sizeof(*entries) > module_size) {
		return;
	}

	entries = (const struct initrd_entry*)(module_base + header->entries_offset);
	for (uint32_t i = 0; i < header->file_count; i++) {
		uint32_t data_start = header->data_offset + entries[i].offset;
		uint32_t data_end = data_start + entries[i].size;

		if (data_start > module_size || data_end > module_size || data_end < data_start) {
			continue;
		}

		ram_file_write_bytes(entries[i].name, module_base + data_start, entries[i].size);
	}
}

static void initialize_initrd(uint32_t multiboot_magic, uint32_t multiboot_info_addr) {
	if (multiboot_magic != MULTIBOOT_BOOTLOADER_MAGIC || multiboot_info_addr == 0) {
		return;
	}

	boot_info = (const struct multiboot_info*)multiboot_info_addr;
	if ((boot_info->flags & MULTIBOOT_INFO_MODS) == 0 || boot_info->mods_count == 0) {
		return;
	}

	initrd_module = (const struct multiboot_module*)boot_info->mods_addr;
	load_initrd_archive(initrd_module);
}

static void print_initrd_info(void) {
	if (initrd_module == NULL) {
		printf("No initrd module was loaded by GRUB.\n");
		return;
	}

	uint32_t start = initrd_module->mod_start;
	uint32_t end = initrd_module->mod_end;
	uint32_t size = end - start;
	const struct initrd_header* header = (const struct initrd_header*)start;
	const char* cmdline = initrd_module->string != 0 ?
	                      (const char*)initrd_module->string : "";

	printf("initrd module: %s\n", cmdline);
	printf("Start: 0x%x End: 0x%x Size: %u bytes\n", start, end, size);

	if (size < sizeof(*header) ||
	    header->magic != INITRD_MAGIC ||
	    header->version != INITRD_VERSION) {
		printf("Invalid initrd archive.\n");
		return;
	}

	printf("Archive files: %u\n", header->file_count);
}

static void print_info_color_blocks(void) {
	enum vga_color palette[] = {
		VGA_COLOR_BLACK, VGA_COLOR_RED, VGA_COLOR_GREEN, VGA_COLOR_BROWN,
		VGA_COLOR_BLUE, VGA_COLOR_MAGENTA, VGA_COLOR_CYAN, VGA_COLOR_LIGHT_GREY,
		VGA_COLOR_DARK_GREY, VGA_COLOR_LIGHT_RED, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_LIGHT_BROWN,
		VGA_COLOR_LIGHT_BLUE, VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_LIGHT_CYAN, VGA_COLOR_WHITE
	};

	for (size_t i = 0; i < sizeof(palette) / sizeof(palette[0]); i++) {
		terminal_setcolor(vga_entry_color(palette[i], VGA_COLOR_BLACK));
		terminal_putchar((char)219);
		terminal_putchar((char)219);
	}
	terminal_putchar('\n');
}

static void print_info_ascii_art(void) {
	uint8_t prev_color = terminal_getcolor();
	uint8_t art_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
	uint8_t info_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

	printf("\n");
	terminal_setcolor(art_color);
	printf("   3141592653589793238462643");
	terminal_setcolor(info_color);
	printf("\n");

	terminal_setcolor(art_color);
	printf("  38   327    950                         ");
	terminal_setcolor(info_color);
	printf("OS: pi-OS\n");
	

	terminal_setcolor(art_color);
	printf(" 2     884    197                         ");
	terminal_setcolor(info_color);
	printf("Version: 0.1-dev\n");
	

	terminal_setcolor(art_color);
	printf("       169     399                        ");
	terminal_setcolor(info_color);
	printf("Kernel: 32-bit freestanding\n");
	

	terminal_setcolor(art_color);
	printf("      375      105                        ");
	terminal_setcolor(info_color);
	printf("Shell: builtin CLI + calc mode\n");
	

	terminal_setcolor(art_color);
	printf("      820      9749                       ");
	terminal_setcolor(info_color);
	printf("Author: 0xGifre\n");
	

	terminal_setcolor(art_color);
	printf("     4459       2307      81  ");
	terminal_setcolor(info_color);
	printf("\n");


	terminal_setcolor(art_color);
	printf("    64062        862089986              ");
	terminal_setcolor(info_color);
	terminal_setcolor(info_color);
	print_info_color_blocks();
	terminal_setcolor(prev_color);
	

	terminal_setcolor(art_color);
	printf("    2803          482534");
	terminal_setcolor(info_color);
	printf("\n");

}

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
		printf("DISK                    Detects the primary IDE disk.\n");
		printf("DISK READ <lba>         Reads and prints one raw disk sector.\n");
		printf("DISK WRITE <lba> <txt>  Writes text to one raw disk sector.\n");
		printf("ECHO <text>             Prints the given text.\n");
		printf("GDT                     Shows the active kernel segment layout.\n");
		printf("INFO                    Shows system info and ASCII art.\n");
		printf("INITRD                  Shows the GRUB-loaded initrd archive.\n");
		printf("LS                      Lists RAM FS files.\n");
		printf("LOADFS                  Loads RAM FS snapshot from disk.\n");
		printf("RAMFS                   Alias for INITRD.\n");
		printf("RAND                    Prints a random number.\n");
		printf("RAND -r <min> <max>     Prints a random number in the given range.\n");
		printf("TOUCH <name>            Creates an empty RAM FS file.\n");
		printf("WRITE <name> <text>     Writes text to a RAM FS file.\n");
		printf("CAT <name>              Prints a RAM FS file.\n");
		printf("RM <name>               Deletes a RAM FS file.\n");
		printf("SAVEFS                  Saves RAM FS snapshot to disk.\n");
		return;
	}
	if (strings_equal(line, "gdt")) {
		printf("GDT loaded by kernel entry code.\n");
		printf("0x00 null descriptor\n");
		printf("0x08 ring 0 code: base 0, limit 4 GiB, read/execute\n");
		printf("0x10 ring 0 data: base 0, limit 4 GiB, read/write\n");
		return;
	}
	if (strings_equal(line, "initrd") || strings_equal(line, "ramfs")) {
		print_initrd_info();
		return;
	}
	if (strings_equal(line, "ls")) {
		ram_file_list();
		return;
	}
	if (strings_equal(line, "disk")) {
		if (ide_identify()) {
			printf("IDE primary master: detected.\n");
		} else {
			printf("IDE error: %s\n", ide_last_error());
		}
		return;
	}
	if (starts_with(line, "disk read ")) {
		char* cursor = line;
		(void)next_token(&cursor);
		(void)next_token(&cursor);
		char* lba_tok = next_token(&cursor);
		char* extra = next_token(&cursor);
		int lba = 0;

		if (lba_tok == NULL || extra != NULL || !parse_i32(lba_tok, &lba) || lba < 0) {
			printf("Usage: disk read <lba>\n");
			return;
		}
		if (!ide_read_sector((uint32_t)lba, disk_sector_buffer)) {
			printf("IDE error: %s\n", ide_last_error());
			return;
		}

		printf("Sector %d:\n", lba);
		terminal_write((const char*)disk_sector_buffer, 128);
		printf("\n");
		return;
	}
	if (starts_with(line, "disk write ")) {
		char* cursor = line;
		(void)next_token(&cursor);
		(void)next_token(&cursor);
		char* lba_tok = next_token(&cursor);
		char* text = cursor;
		int lba = 0;
		size_t text_len = 0;

		while (*text == ' ') {
			text++;
		}

		if (lba_tok == NULL || !parse_i32(lba_tok, &lba) || lba < 0) {
			printf("Usage: disk write <lba> <text>\n");
			return;
		}

		memset(disk_sector_buffer, 0, sizeof(disk_sector_buffer));
		text_len = strlen(text);
		if (text_len > sizeof(disk_sector_buffer)) {
			text_len = sizeof(disk_sector_buffer);
		}
		memcpy(disk_sector_buffer, text, text_len);

		if (!ide_write_sector((uint32_t)lba, disk_sector_buffer)) {
			printf("IDE error: %s\n", ide_last_error());
			return;
		}

		printf("Wrote sector %d.\n", lba);
		return;
	}
	if (strings_equal(line, "savefs")) {
		if (!save_ramfs_to_disk()) {
			printf("Could not save FS: %s\n", ide_last_error());
			return;
		}
		printf("RAM FS saved to disk at LBA %u (%u sectors).\n",
		       DISK_FS_LBA, DISK_FS_SECTORS);
		return;
	}
	if (strings_equal(line, "loadfs")) {
		if (!load_ramfs_from_disk()) {
			printf("Could not load FS: %s\n", ide_last_error());
			return;
		}
		printf("RAM FS loaded from disk.\n");
		return;
	}
	if (starts_with(line, "touch ")) {
		char* cursor = line;
		(void)next_token(&cursor);
		char* name = next_token(&cursor);
		char* extra = next_token(&cursor);

		if (name == NULL || extra != NULL) {
			printf("Usage: touch <name>\n");
			return;
		}
		if (!ram_file_create(name)) {
			printf("Could not create file: %s\n", name);
			return;
		}
		printf("Created: %s\n", name);
		return;
	}
	if (starts_with(line, "write ")) {
		char* cursor = line;
		(void)next_token(&cursor);
		char* name = next_token(&cursor);
		char* text = cursor;

		while (*text == ' ') {
			text++;
		}

		if (name == NULL || !ram_file_valid_name(name)) {
			printf("Usage: write <name> <text>\n");
			return;
		}
		if (!ram_file_write(name, text)) {
			printf("Could not write file: %s\n", name);
			return;
		}
		printf("Wrote: %s\n", name);
		return;
	}
	if (starts_with(line, "cat ")) {
		char* cursor = line;
		(void)next_token(&cursor);
		char* name = next_token(&cursor);
		char* extra = next_token(&cursor);

		if (name == NULL || extra != NULL) {
			printf("Usage: cat <name>\n");
			return;
		}
		ram_file_cat(name);
		return;
	}
	if (starts_with(line, "rm ")) {
		char* cursor = line;
		(void)next_token(&cursor);
		char* name = next_token(&cursor);
		char* extra = next_token(&cursor);

		if (name == NULL || extra != NULL) {
			printf("Usage: rm <name>\n");
			return;
		}
		if (!ram_file_remove(name)) {
			printf("File not found: %s\n", name);
			return;
		}
		printf("Removed: %s\n", name);
		return;
	}
	if (strings_equal(line, "info") || strings_equal(line, "INFO")) {
		print_info_ascii_art();
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

void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_info_addr) {
	char line[128];

	initialize_initrd(multiboot_magic, multiboot_info_addr);
	terminal_initialize();
	print_info_ascii_art();
	
	printf("Type 'help' for commands.\n");

	for (;;) {
		printf("> ");
		read_line(line, sizeof(line));
		execute_command(line);
	}
}
