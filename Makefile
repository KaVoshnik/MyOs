BUILD_DIR := build
ISO_DIR := $(BUILD_DIR)/iso
KERNEL := $(BUILD_DIR)/kernel.elf
ISO_IMAGE := $(BUILD_DIR)/MyOs.iso

CC := x86_64-elf-gcc
LD := x86_64-elf-ld
NASM := nasm

CFLAGS := -m64 -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -mgeneral-regs-only -Wall -Wextra -Werror -nostdlib -nostdinc -fno-builtin -I include
LDFLAGS := -nostdlib -z max-page-size=0x1000

SRC := src/kernel.c src/terminal.c src/string.c src/interrupts.c src/pit.c src/keyboard.c src/memory.c src/shell.c
OBJ := $(SRC:%.c=$(BUILD_DIR)/%.o) $(BUILD_DIR)/boot.o

.PHONY: all clean run iso

all: $(ISO_IMAGE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/boot.o: src/boot.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): linker.ld $(OBJ)
	$(LD) $(LDFLAGS) -T $< -o $@ $(OBJ)

$(ISO_DIR): $(KERNEL)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.elf
	cp grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg

$(ISO_IMAGE): $(ISO_DIR)
	grub-mkrescue -o $@ $(ISO_DIR)

run: $(ISO_IMAGE)
	qemu-system-x86_64 -cdrom $(ISO_IMAGE)

clean:
	rm -rf $(BUILD_DIR)

