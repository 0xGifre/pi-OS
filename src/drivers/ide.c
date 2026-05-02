#include <stddef.h>
#include <stdint.h>

#include <ide.h>
#include <io.h>

#define ATA_DATA       0x1F0
#define ATA_ERROR      0x1F1
#define ATA_SECCOUNT0  0x1F2
#define ATA_LBA0       0x1F3
#define ATA_LBA1       0x1F4
#define ATA_LBA2       0x1F5
#define ATA_HDDEVSEL   0x1F6
#define ATA_COMMAND    0x1F7
#define ATA_STATUS     0x1F7
#define ATA_CONTROL    0x3F6

#define ATA_SR_ERR     0x01
#define ATA_SR_DRQ     0x08
#define ATA_SR_DF      0x20
#define ATA_SR_DRDY    0x40
#define ATA_SR_BSY     0x80

#define ATA_CMD_READ_PIO   0x20
#define ATA_CMD_WRITE_PIO  0x30
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_IDENTIFY   0xEC

static const char* last_error = "not initialized";

const char* ide_last_error(void) {
	return last_error;
}

static uint8_t ide_status(void) {
	return inb(ATA_STATUS);
}

static bool ide_wait_not_busy(void) {
	for (uint32_t i = 0; i < 1000000; i++) {
		if ((ide_status() & ATA_SR_BSY) == 0) {
			return true;
		}
	}
	last_error = "timeout waiting for BSY clear";
	return false;
}

static bool ide_wait_drq(void) {
	for (uint32_t i = 0; i < 1000000; i++) {
		uint8_t status = ide_status();
		if ((status & ATA_SR_ERR) != 0) {
			(void)inb(ATA_ERROR);
			last_error = "drive reported ERR";
			return false;
		}
		if ((status & ATA_SR_DF) != 0) {
			last_error = "drive fault";
			return false;
		}
		if ((status & ATA_SR_BSY) == 0 && (status & ATA_SR_DRQ) != 0) {
			return true;
		}
	}
	last_error = "timeout waiting for DRQ";
	return false;
}

static void ide_select_lba28(uint32_t lba) {
	outb(ATA_HDDEVSEL, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
	io_wait();
}

bool ide_identify(void) {
	ide_select_lba28(0);
	outb(ATA_SECCOUNT0, 0);
	outb(ATA_LBA0, 0);
	outb(ATA_LBA1, 0);
	outb(ATA_LBA2, 0);
	outb(ATA_COMMAND, ATA_CMD_IDENTIFY);
	io_wait();

	if (ide_status() == 0) {
		last_error = "no primary master ATA device";
		return false;
	}
	if (!ide_wait_drq()) {
		return false;
	}

	for (size_t i = 0; i < 256; i++) {
		(void)inw(ATA_DATA);
	}

	last_error = "ok";
	return true;
}

bool ide_read_sector(uint32_t lba, void* buffer) {
	uint16_t* words = (uint16_t*)buffer;

	if (!ide_wait_not_busy()) {
		return false;
	}

	ide_select_lba28(lba);
	outb(ATA_SECCOUNT0, 1);
	outb(ATA_LBA0, (uint8_t)(lba & 0xFF));
	outb(ATA_LBA1, (uint8_t)((lba >> 8) & 0xFF));
	outb(ATA_LBA2, (uint8_t)((lba >> 16) & 0xFF));
	outb(ATA_COMMAND, ATA_CMD_READ_PIO);

	if (!ide_wait_drq()) {
		return false;
	}

	for (size_t i = 0; i < 256; i++) {
		words[i] = inw(ATA_DATA);
	}

	last_error = "ok";
	return true;
}

bool ide_write_sector(uint32_t lba, const void* buffer) {
	const uint16_t* words = (const uint16_t*)buffer;

	if (!ide_wait_not_busy()) {
		return false;
	}

	ide_select_lba28(lba);
	outb(ATA_SECCOUNT0, 1);
	outb(ATA_LBA0, (uint8_t)(lba & 0xFF));
	outb(ATA_LBA1, (uint8_t)((lba >> 8) & 0xFF));
	outb(ATA_LBA2, (uint8_t)((lba >> 16) & 0xFF));
	outb(ATA_COMMAND, ATA_CMD_WRITE_PIO);

	if (!ide_wait_drq()) {
		return false;
	}

	for (size_t i = 0; i < 256; i++) {
		outw(ATA_DATA, words[i]);
	}

	if (!ide_wait_not_busy()) {
		return false;
	}
	outb(ATA_COMMAND, ATA_CMD_CACHE_FLUSH);
	if (!ide_wait_not_busy()) {
		return false;
	}

	last_error = "ok";
	return true;
}
