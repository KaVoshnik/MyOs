#ifndef _MYOS_RTL8139_H
#define _MYOS_RTL8139_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t present;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t io_base;
    uint8_t irq_line;
    uint8_t mac[6];
} rtl8139_info_t;

typedef struct {
    uint8_t cr;
    uint16_t capr;
    uint16_t cbr;
    uint16_t isr;
    uint16_t imr;
    uint32_t rcr;
} rtl8139_regs_t;

void rtl8139_init(void);
const rtl8139_info_t *rtl8139_get_info(void);

typedef void (*rtl8139_rx_handler_t)(const uint8_t *frame, size_t len, void *user);
void rtl8139_set_rx_handler(rtl8139_rx_handler_t handler, void *user);

/* Отправить Ethernet кадр (len <= 1518). Возвращает 0 при успехе. */
int rtl8139_send_frame(const void *data, size_t len);

/* Принять/обработать входящие кадры (polling), печатает краткий дамп. */
int rtl8139_poll_rx(int max_frames);

/* Прочитать ключевые регистры RTL8139 (для отладки). Возвращает 1 при успехе. */
int rtl8139_get_regs(rtl8139_regs_t *out);

#endif /* _MYOS_RTL8139_H */

