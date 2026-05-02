#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITRD_MAGIC 0x44524950u
#define INITRD_VERSION 1u
#define INITRD_NAME_MAX 56u

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

struct input_file {
	const char* path;
	const char* name;
	uint8_t* data;
	uint32_t size;
};

static const char* basename_of(const char* path) {
	const char* slash = strrchr(path, '/');
	return slash == NULL ? path : slash + 1;
}

static void die(const char* message, const char* detail) {
	if (detail != NULL) {
		fprintf(stderr, "mkinitrd: %s: %s\n", message, detail);
	} else {
		fprintf(stderr, "mkinitrd: %s\n", message);
	}
	exit(1);
}

static void* checked_calloc(size_t count, size_t size) {
	void* ptr = calloc(count, size);
	if (ptr == NULL) {
		die("out of memory", NULL);
	}
	return ptr;
}

static void read_file(struct input_file* file) {
	FILE* fp = fopen(file->path, "rb");
	long length = 0;

	if (fp == NULL) {
		die("could not open input file", file->path);
	}
	if (fseek(fp, 0, SEEK_END) != 0) {
		die("could not seek input file", file->path);
	}

	length = ftell(fp);
	if (length < 0) {
		die("could not measure input file", file->path);
	}
	if ((unsigned long)length > UINT32_MAX) {
		die("input file is too large", file->path);
	}
	if (fseek(fp, 0, SEEK_SET) != 0) {
		die("could not rewind input file", file->path);
	}

	file->size = (uint32_t)length;
	file->data = checked_calloc(file->size == 0 ? 1 : file->size, 1);

	if (file->size != 0 && fread(file->data, 1, file->size, fp) != file->size) {
		die("could not read input file", file->path);
	}

	fclose(fp);
}

static void write_exact(FILE* fp, const void* data, size_t size, const char* path) {
	if (size != 0 && fwrite(data, 1, size, fp) != size) {
		die("could not write output file", path);
	}
}

int main(int argc, char** argv) {
	const char* output_path = NULL;
	struct input_file* files = NULL;
	struct initrd_entry* entries = NULL;
	struct initrd_header header;
	FILE* output = NULL;
	uint32_t file_count = 0;
	uint32_t data_cursor = 0;

	if (argc < 3) {
		die("usage: mkinitrd <output> <file> [file...]", NULL);
	}

	output_path = argv[1];
	file_count = (uint32_t)(argc - 2);
	files = checked_calloc(file_count, sizeof(files[0]));
	entries = checked_calloc(file_count, sizeof(entries[0]));

	for (uint32_t i = 0; i < file_count; i++) {
		files[i].path = argv[i + 2];
		files[i].name = basename_of(files[i].path);

		if (files[i].name[0] == '\0' || strlen(files[i].name) >= INITRD_NAME_MAX) {
			die("bad initrd file name", files[i].path);
		}

		read_file(&files[i]);
	}

	header.magic = INITRD_MAGIC;
	header.version = INITRD_VERSION;
	header.file_count = file_count;
	header.entries_offset = sizeof(header);
	header.data_offset = sizeof(header) + file_count * sizeof(entries[0]);

	for (uint32_t i = 0; i < file_count; i++) {
		strncpy(entries[i].name, files[i].name, sizeof(entries[i].name) - 1);
		entries[i].offset = data_cursor;
		entries[i].size = files[i].size;
		data_cursor += files[i].size;
	}

	output = fopen(output_path, "wb");
	if (output == NULL) {
		die("could not open output file", output_path);
	}

	write_exact(output, &header, sizeof(header), output_path);
	write_exact(output, entries, file_count * sizeof(entries[0]), output_path);
	for (uint32_t i = 0; i < file_count; i++) {
		write_exact(output, files[i].data, files[i].size, output_path);
	}

	if (fclose(output) != 0) {
		die("could not close output file", output_path);
	}

	for (uint32_t i = 0; i < file_count; i++) {
		free(files[i].data);
	}
	free(entries);
	free(files);
	return 0;
}
