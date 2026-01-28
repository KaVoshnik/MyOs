#ifndef _MYOS_RTL8139_H
#define _MYOS_RTL8139_H

#include <stdint.h>

typedef struct {
    uint8_t present;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t io_base;
    uint8_t irq_line;
} rtl8139_info_t;

void rtl8139_init(void);
const rtl8139_info_t *rtl8139_get_info(void);

#endif /* _MYOS_RTL8139_H */

