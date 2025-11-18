#include <system.h>
#include <io.h>

void system_halt(void) {
    while (1) {
        __asm__ volatile("cli; hlt");
    }
}

void system_poweroff(void) {
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    system_halt();
}

void system_reboot(void) {
    uint8_t temp;
    do {
        temp = inb(0x64);
    } while (temp & 0x02);
    outb(0x64, 0xFE);
    system_halt();
}


