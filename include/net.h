#ifndef _MYOS_NET_H
#define _MYOS_NET_H

#include <stdint.h>

void net_init(void);
void net_poll(void);

/* Минимальный ping (ICMP echo) по IPv4. Возвращает 0 при успехе. rtt_ms_out может быть NULL. */
int net_ping(uint32_t dst_ip_be, uint32_t timeout_ms, uint32_t *rtt_ms_out);

/* Утилита: парсинг IPv4 из строки "a.b.c.d". Возвращает 1 при успехе. */
int net_parse_ipv4(const char *s, uint32_t *out_ip_be);

/* Утилита: преобразование host byte order -> network byte order (big-endian). */
static inline uint32_t htonl(uint32_t v) {
    return ((v & 0x000000FFu) << 24) |
           ((v & 0x0000FF00u) << 8)  |
           ((v & 0x00FF0000u) >> 8)  |
           ((v & 0xFF000000u) >> 24);
}

#endif /* _MYOS_NET_H */

