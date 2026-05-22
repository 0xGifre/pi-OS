# Declare constants for the multiboot header.
.set ALIGN,    1<<0             # align loaded modules on page boundaries
.set MEMINFO,  1<<1             # provide memory map
.set FLAGS,    ALIGN | MEMINFO  # this is the Multiboot 'flag' field
.set MAGIC,    0x1BADB002       # 'magic number' lets bootloader find the header
.set CHECKSUM, -(MAGIC + FLAGS) # checksum of above, to prove we are multiboot

# Declare a header as in the Multiboot Standard.
.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

# Reserve a stack for the initial thread.
.section .bss
.align 16
stack_bottom:
.skip 16384 # 16 KiB
stack_top:

# The kernel entry point.
.section .text
.global _start
.type _start, @function
_start:
	movl $stack_top, %esp

	# Preserve GRUB's Multiboot registers before calling C code.
	# eax = magic, ebx = multiboot_info.
	pushl %ebx
	pushl %eax

	# GRUB enters here in 32-bit protected mode. Install our own GDT so the
	# kernel does not depend on the bootloader's descriptor table.
	call gdt_install

	# Floating-point code in the kernel uses the x87 FPU. Make sure the FPU is
	# enabled and initialized before C code can execute float operations.
	movl %cr0, %eax
	andl $0xFFFFFFF3, %eax # clear EM and TS
	orl $0x22, %eax       # set MP and NE
	movl %eax, %cr0
	fninit

	# Transfer control to the main kernel.
	call kernel_main

	# Hang if kernel_main unexpectedly returns.
	cli
1:	hlt
	jmp 1b
.size _start, . - _start
