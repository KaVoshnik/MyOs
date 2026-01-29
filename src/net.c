#include <net.h>
#include <rtl8139.h>
#include <terminal.h>
#include <pit.h>
#include <string.h>

/* Simple htons/ntohs helpers (we store IPs as big-endian on the wire). */
static uint16_t bswap16(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }
static uint16_t htons(uint16_t v) { return bswap16(v); }
static uint16_t ntohs(uint16_t v) { return bswap16(v); }

static uint16_t checksum16(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t sum = 0;
    for (size_t i = 0; i + 1 < len; i += 2) {
        sum += (uint16_t)((p[i] << 8) | p[i + 1]);
    }
    if (len & 1) {
        sum += (uint16_t)(p[len - 1] << 8);
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

/* Ethernet */
#define ETH_TYPE_ARP  0x0806
#define ETH_TYPE_IPV4 0x0800

typedef struct __attribute__((packed)) {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t ethertype;
} eth_hdr_t;

/* ARP */
typedef struct __attribute__((packed)) {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;
    uint8_t sha[6];
    uint32_t spa;
    uint8_t tha[6];
    uint32_t tpa;
} arp_pkt_t;

/* IPv4 + ICMP */
typedef struct __attribute__((packed)) {
    uint8_t ver_ihl;
    uint8_t tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t ttl;
    uint8_t proto;
    uint16_t hdr_csum;
    uint32_t src;
    uint32_t dst;
} ipv4_hdr_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t code;
    uint16_t csum;
    uint16_t ident;
    uint16_t seq;
} icmp_echo_t;

/* Very small ARP cache: only one entry for now (gateway/target) */
static struct {
    uint8_t valid;
    uint32_t ip_be;
    uint8_t mac[6];
} arp_cache;

static uint8_t g_mac[6];
static uint32_t g_ip_be;      /* 10.0.2.15 */
static uint32_t g_gw_ip_be;   /* 10.0.2.2  */

static volatile int g_ping_got_reply = 0;
static uint16_t g_ping_ident = 0x1234;
static uint16_t g_ping_seq = 1;

static void net_send_eth(const uint8_t dst_mac[6], uint16_t ethertype, const void *payload, size_t len) {
    uint8_t frame[1518];
    if (len + sizeof(eth_hdr_t) > sizeof(frame)) {
        return;
    }
    eth_hdr_t *eth = (eth_hdr_t *)frame;
    memcpy(eth->dst, dst_mac, 6);
    memcpy(eth->src, g_mac, 6);
    eth->ethertype = htons(ethertype);
    memcpy(frame + sizeof(eth_hdr_t), payload, len);
    (void)rtl8139_send_frame(frame, sizeof(eth_hdr_t) + len);
}

static void arp_send_request(uint32_t target_ip_be) {
    arp_pkt_t arp;
    memset(&arp, 0, sizeof(arp));
    arp.htype = htons(1);
    arp.ptype = htons(0x0800);
    arp.hlen = 6;
    arp.plen = 4;
    arp.oper = htons(1);
    memcpy(arp.sha, g_mac, 6);
    arp.spa = g_ip_be;
    /* tha zeros */
    arp.tpa = target_ip_be;

    static const uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    net_send_eth(bcast, ETH_TYPE_ARP, &arp, sizeof(arp));
}

static void arp_handle(const arp_pkt_t *arp) {
    if (ntohs(arp->htype) != 1 || ntohs(arp->ptype) != 0x0800 || arp->hlen != 6 || arp->plen != 4) {
        return;
    }
    uint16_t op = ntohs(arp->oper);

    /* Cache sender */
    arp_cache.valid = 1;
    arp_cache.ip_be = arp->spa;
    memcpy(arp_cache.mac, arp->sha, 6);

    /* If it's a request for our IP, reply */
    if (op == 1 && arp->tpa == g_ip_be) {
        arp_pkt_t reply;
        memset(&reply, 0, sizeof(reply));
        reply.htype = htons(1);
        reply.ptype = htons(0x0800);
        reply.hlen = 6;
        reply.plen = 4;
        reply.oper = htons(2);
        memcpy(reply.sha, g_mac, 6);
        reply.spa = g_ip_be;
        memcpy(reply.tha, arp->sha, 6);
        reply.tpa = arp->spa;
        net_send_eth(arp->sha, ETH_TYPE_ARP, &reply, sizeof(reply));
    }
}

static int arp_resolve(uint32_t ip_be, uint8_t out_mac[6], uint32_t timeout_ms) {
    if (arp_cache.valid && arp_cache.ip_be == ip_be) {
        memcpy(out_mac, arp_cache.mac, 6);
        return 1;
    }

    /* Ask */
    arp_send_request(ip_be);

    uint64_t start = pit_seconds();
    uint32_t timeout_s = (timeout_ms + 999) / 1000;
    if (timeout_s == 0) timeout_s = 1;

    while ((uint32_t)(pit_seconds() - start) < timeout_s) {
        net_poll();
        if (arp_cache.valid && arp_cache.ip_be == ip_be) {
            memcpy(out_mac, arp_cache.mac, 6);
            return 1;
        }
    }
    return 0;
}

static void icmp_handle(const ipv4_hdr_t *ip, const uint8_t *payload, size_t payload_len) {
    if (payload_len < sizeof(icmp_echo_t)) return;
    const icmp_echo_t *icmp = (const icmp_echo_t *)payload;

    /* Echo reply */
    if (icmp->type == 0 && icmp->code == 0) {
        if (icmp->ident == htons(g_ping_ident)) {
            g_ping_got_reply = 1;
        }
        return;
    }

    /* Echo request -> reply */
    if (icmp->type == 8 && icmp->code == 0) {
        /* Build reply: same payload, type=0 */
        uint8_t out[1500];
        if (payload_len > sizeof(out)) return;
        memcpy(out, payload, payload_len);
        ((icmp_echo_t *)out)->type = 0;
        ((icmp_echo_t *)out)->csum = 0;
        ((icmp_echo_t *)out)->csum = checksum16(out, payload_len);

        /* send IPv4 back to sender */
        uint8_t dst_mac[6];
        if (!arp_resolve(ip->src, dst_mac, 1000)) return;

        ipv4_hdr_t iph;
        iph.ver_ihl = 0x45;
        iph.tos = 0;
        iph.total_len = htons((uint16_t)(sizeof(ipv4_hdr_t) + payload_len));
        iph.id = htons(0x4242);
        iph.frag_off = htons(0);
        iph.ttl = 64;
        iph.proto = 1;
        iph.hdr_csum = 0;
        iph.src = g_ip_be;
        iph.dst = ip->src;
        iph.hdr_csum = checksum16(&iph, sizeof(iph));

        uint8_t pkt[1500];
        memcpy(pkt, &iph, sizeof(iph));
        memcpy(pkt + sizeof(iph), out, payload_len);

        net_send_eth(dst_mac, ETH_TYPE_IPV4, pkt, sizeof(iph) + payload_len);
    }
}

static void ipv4_handle(const ipv4_hdr_t *ip, const uint8_t *payload, size_t payload_len) {
    uint8_t ihl = (uint8_t)(ip->ver_ihl & 0x0F);
    if (ihl < 5) return;
    if ((ip->ver_ihl >> 4) != 4) return;
    if (ip->dst != g_ip_be) return;

    if (ip->proto == 1) {
        icmp_handle(ip, payload, payload_len);
    }
}

static void net_rx_handler(const uint8_t *frame, size_t len, void *user) {
    (void)user;
    if (len < sizeof(eth_hdr_t)) return;
    const eth_hdr_t *eth = (const eth_hdr_t *)frame;
    uint16_t et = ntohs(eth->ethertype);
    const uint8_t *payload = frame + sizeof(eth_hdr_t);
    size_t plen = len - sizeof(eth_hdr_t);

    if (et == ETH_TYPE_ARP) {
        if (plen >= sizeof(arp_pkt_t)) {
            arp_handle((const arp_pkt_t *)payload);
        }
        return;
    }

    if (et == ETH_TYPE_IPV4) {
        if (plen < sizeof(ipv4_hdr_t)) return;
        const ipv4_hdr_t *ip = (const ipv4_hdr_t *)payload;
        uint16_t total = ntohs(ip->total_len);
        size_t hdr_bytes = (size_t)((ip->ver_ihl & 0x0F) * 4u);
        if (total < hdr_bytes || total > plen) return;
        const uint8_t *l4 = payload + hdr_bytes;
        size_t l4len = (size_t)total - hdr_bytes;
        ipv4_handle(ip, l4, l4len);
    }
}

void net_init(void) {
    const rtl8139_info_t *info = rtl8139_get_info();
    if (!info || !info->present) {
        terminal_write_line("[net] net_init: no NIC");
        return;
    }
    memcpy(g_mac, info->mac, 6);

    /* QEMU usernet defaults */
    g_ip_be = htonl(0x0A00020Fu);   /* 10.0.2.15 */
    g_gw_ip_be = htonl(0x0A000202u);/* 10.0.2.2  */

    arp_cache.valid = 0;
    g_ping_got_reply = 0;

    rtl8139_set_rx_handler(net_rx_handler, 0);
    terminal_write_line("[net] net stack ready (ARP/IPv4/ICMP).");
}

void net_poll(void) {
    (void)rtl8139_poll_rx(8);
}

int net_ping(uint32_t dst_ip_be, uint32_t timeout_ms, uint32_t *rtt_ms_out) {
    if (rtt_ms_out) *rtt_ms_out = 0;
    
    uint8_t dst_mac[6];
    if (!arp_resolve(dst_ip_be, dst_mac, timeout_ms)) {
        return -1;
    }

    /* Build ICMP echo */
    uint8_t payload[32];
    memset(payload, 0, sizeof(payload));
    const char *msg = "MYOS-PING";
    memcpy(payload, msg, strlen(msg));

    uint8_t icmp_buf[sizeof(icmp_echo_t) + sizeof(payload)];
    icmp_echo_t *icmp = (icmp_echo_t *)icmp_buf;
    icmp->type = 8;
    icmp->code = 0;
    icmp->csum = 0;
    icmp->ident = htons(g_ping_ident);
    icmp->seq = htons(g_ping_seq++);
    memcpy(icmp_buf + sizeof(icmp_echo_t), payload, sizeof(payload));
    icmp->csum = checksum16(icmp_buf, sizeof(icmp_buf));

    ipv4_hdr_t ip;
    ip.ver_ihl = 0x45;
    ip.tos = 0;
    ip.total_len = htons((uint16_t)(sizeof(ipv4_hdr_t) + sizeof(icmp_buf)));
    ip.id = htons(0x1111);
    ip.frag_off = htons(0);
    ip.ttl = 64;
    ip.proto = 1;
    ip.hdr_csum = 0;
    ip.src = g_ip_be;
    ip.dst = dst_ip_be;
    ip.hdr_csum = checksum16(&ip, sizeof(ip));

    uint8_t pkt[1500];
    memcpy(pkt, &ip, sizeof(ip));
    memcpy(pkt + sizeof(ip), icmp_buf, sizeof(icmp_buf));

    g_ping_got_reply = 0;

    /* Use pit_seconds for timing (100 Hz = 10ms resolution) */
    uint64_t start_ticks = pit_seconds() * 100; /* Convert to 100Hz ticks */
    net_send_eth(dst_mac, ETH_TYPE_IPV4, pkt, sizeof(ip) + sizeof(icmp_buf));

    uint32_t timeout_ticks = (timeout_ms * 100) / 1000;
    if (timeout_ticks == 0) timeout_ticks = 100;
    
    while ((uint64_t)((pit_seconds() * 100) - start_ticks) < timeout_ticks) {
        net_poll();
        if (g_ping_got_reply) {
            uint64_t elapsed_ticks = (pit_seconds() * 100) - start_ticks;
            if (rtt_ms_out) {
                *rtt_ms_out = (uint32_t)((elapsed_ticks * 1000) / 100);
            }
            return 0;
        }
    }
    return -2;
}

int net_parse_ipv4(const char *s, uint32_t *out_ip_be) {
    if (!s || !out_ip_be) return 0;
    uint32_t parts[4] = {0,0,0,0};
    int part = 0;
    uint32_t acc = 0;
    int have = 0;
    while (*s) {
        char c = *s++;
        if (c >= '0' && c <= '9') {
            acc = acc * 10 + (uint32_t)(c - '0');
            if (acc > 255) return 0;
            have = 1;
            continue;
        }
        if (c == '.') {
            if (!have || part >= 3) return 0;
            parts[part++] = acc;
            acc = 0;
            have = 0;
            continue;
        }
        return 0;
    }
    if (!have || part != 3) return 0;
    parts[3] = acc;
    uint32_t ip = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
    *out_ip_be = htonl(ip);
    return 1;
}

