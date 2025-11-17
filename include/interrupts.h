#ifndef _MYOS_INTERRUPTS_H
#define _MYOS_INTERRUPTS_H

#include <stdint.h>

struct interrupt_frame {
    uint64_t rip;
    uint16_t cs;
    uint16_t _pad0;
    uint32_t _pad1;
    uint64_t rflags;
    uint64_t rsp;
    uint16_t ss;
    uint16_t _pad2;
    uint32_t _pad3;
};

void interrupts_init(void);
void interrupts_enable(void);
void interrupts_disable(void);

#endif /* _MYOS_INTERRUPTS_H */

