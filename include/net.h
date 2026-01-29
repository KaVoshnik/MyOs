#ifndef _MYOS_NET_H
#define _MYOS_NET_H

#include <stdint.h>

void net_init(void);
void net_poll(void);

/* Endianness helpers */
static inline uint16_t net_bswap16(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }
static inline uint32_t net_bswap32(uint32_t v) {
    return ((v & 0x000000FFu) << 24) |
           ((v & 0x0000FF00u) << 8)  |
           ((v & 0x00FF0000u) >> 8)  |
           ((v & 0xFF000000u) >> 24);
}
static inline uint16_t htons(uint16_t v) { return net_bswap16(v); }
static inline uint16_t ntohs(uint16_t v) { return net_bswap16(v); }
static inline uint32_t htonl(uint32_t v) { return net_bswap32(v); }
static inline uint32_t ntohl(uint32_t v) { return net_bswap32(v); }

/* Минимальный ping (ICMP echo) по IPv4.
 * dst_ip_host — IP в host byte order (например 0x0A000202 для 10.0.2.2).
 * Возвращает 0 при успехе. rtt_ms_out может быть NULL.
 */
int net_ping(uint32_t dst_ip_host, uint32_t timeout_ms, uint32_t *rtt_ms_out);

/* Утилита: парсинг IPv4 из строки "a.b.c.d". Возвращает 1 при успехе.
 * out_ip_host — IP в host byte order.
 */
int net_parse_ipv4(const char *s, uint32_t *out_ip_host);

#endif /* _MYOS_NET_H */

