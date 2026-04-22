#include <net.h>
#include <rtl8139.h>
#include <terminal.h>
#include <pit.h>
#include <string.h>

/* Endianness helpers are in net.h */
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
    uint32_t ip_host;
    uint8_t mac[6];
} arp_cache;

static uint8_t g_mac[6];
static uint32_t g_ip_host;      /* 10.0.2.15 */
static uint32_t g_gw_ip_host;   /* 10.0.2.2  */
static uint32_t g_netmask_host; /* 255.255.255.0 */

static volatile int g_ping_got_reply = 0;
static uint16_t g_ping_ident = 0x1234;
static uint16_t g_ping_seq = 1;

/* Debug helper: dump basic info about an Ethernet frame we transmit */
static void net_dump_tx_frame(const uint8_t *frame, size_t len) {
    if (!frame || len < sizeof(eth_hdr_t)) {
        terminal_write_line("[net][tx] frame too short");
        return;
    }
    const eth_hdr_t *eth = (const eth_hdr_t *)frame;
    uint16_t et = ntohs(eth->ethertype);

    terminal_write("[net][tx] len=");
    /* simple decimal print */
    char buf[16];
    size_t n = len;
    int i = 0;
    if (n == 0) {
        buf[i++] = '0';
    } else {
        char tmp[16];
        int t = 0;
        while (n > 0 && t < (int)sizeof(tmp)) {
            tmp[t++] = (char)('0' + (n % 10));
            n /= 10;
        }
        while (t > 0 && i < (int)sizeof(buf) - 1) {
            buf[i++] = tmp[--t];
        }
    }
    buf[i] = '\0';
    terminal_write(buf);

    static const char hex[] = "0123456789ABCDEF";
    char h16[5];

    terminal_write(" eth=0x");
    h16[0] = hex[(et >> 12) & 0xF];
    h16[1] = hex[(et >> 8) & 0xF];
    h16[2] = hex[(et >> 4) & 0xF];
    h16[3] = hex[et & 0xF];
    h16[4] = '\0';
    terminal_write(h16);

    terminal_write(" dst=");
    for (int k = 0; k < 6; ++k) {
        char b[3];
        b[0] = hex[(eth->dst[k] >> 4) & 0xF];
        b[1] = hex[eth->dst[k] & 0xF];
        b[2] = '\0';
        terminal_write(b);
        if (k != 5) terminal_write(":");
    }
    terminal_write(" src=");
    for (int k = 0; k < 6; ++k) {
        char b[3];
        b[0] = hex[(eth->src[k] >> 4) & 0xF];
        b[1] = hex[eth->src[k] & 0xF];
        b[2] = '\0';
        terminal_write(b);
        if (k != 5) terminal_write(":");
    }
    terminal_write_line("");

    /* Dump первые ~40 байт */
    size_t dump = len < 40 ? len : 40;
    terminal_write("          ");
    for (size_t j = 0; j < dump; ++j) {
        char b[3];
        b[0] = hex[(frame[j] >> 4) & 0xF];
        b[1] = hex[frame[j] & 0xF];
        b[2] = '\0';
        terminal_write(b);
        terminal_write(j + 1 == dump ? "" : " ");
    }
    terminal_write_line("");
}

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
    net_dump_tx_frame(frame, sizeof(eth_hdr_t) + len);
    int rc = rtl8139_send_frame(frame, sizeof(eth_hdr_t) + len);
    if (rc != 0) {
        terminal_write("[net][tx] rtl8139_send_frame failed rc=");
        char buf[4];
        buf[0] = (char)('0' + ((-rc / 10) % 10));
        buf[1] = (char)('0' + ((-rc) % 10));
        buf[2] = '\0';
        if (rc > -10 && rc < 0) {
            /* shift for single-digit */
            buf[0] = '-';
            buf[1] = (char)('0' + (-rc));
            buf[2] = '\0';
        }
        terminal_write(buf);
        terminal_write_line("");
    }
}

static void arp_send_request(uint32_t target_ip_host) {
    arp_pkt_t arp;
    memset(&arp, 0, sizeof(arp));
    arp.htype = htons(1);
    arp.ptype = htons(0x0800);
    arp.hlen = 6;
    arp.plen = 4;
    arp.oper = htons(1);
    memcpy(arp.sha, g_mac, 6);
    arp.spa = htonl(g_ip_host);
    /* tha zeros */
    arp.tpa = htonl(target_ip_host);

    static const uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    net_send_eth(bcast, ETH_TYPE_ARP, &arp, sizeof(arp));
    terminal_write("[net][arp] who-has ");
    terminal_write("0x");
    /* quick hex */
    static const char hex[] = "0123456789ABCDEF";
    char h[9];
    uint32_t v = target_ip_host;
    for (int i = 7; i >= 0; --i) {
        h[i] = hex[v & 0xF];
        v >>= 4;
    }
    h[8] = '\0';
    terminal_write(h);
    terminal_write_line(" (sent)");
}

static void arp_handle(const arp_pkt_t *arp) {
    if (ntohs(arp->htype) != 1 || ntohs(arp->ptype) != 0x0800 || arp->hlen != 6 || arp->plen != 4) {
        return;
    }
    uint16_t op = ntohs(arp->oper);

    uint32_t spa_host = ntohl(arp->spa);
    uint32_t tpa_host = ntohl(arp->tpa);

    /* Cache sender */
    arp_cache.valid = 1;
    arp_cache.ip_host = spa_host;
    memcpy(arp_cache.mac, arp->sha, 6);

    terminal_write("[net][arp] op=");
    terminal_write(op == 1 ? "req" : (op == 2 ? "rep" : "unk"));
    terminal_write(" spa=0x");
    static const char hex[] = "0123456789ABCDEF";
    char h[9];
    uint32_t v = spa_host;
    for (int i = 7; i >= 0; --i) {
        h[i] = hex[v & 0xF];
        v >>= 4;
    }
    h[8] = '\0';
    terminal_write(h);
    terminal_write(" sha=");
    for (int i = 0; i < 6; ++i) {
        char b[3];
        b[0] = hex[(arp->sha[i] >> 4) & 0xF];
        b[1] = hex[arp->sha[i] & 0xF];
        b[2] = '\0';
        terminal_write(b);
        if (i != 5) terminal_write(":");
    }
    terminal_write_line("");

    /* If it's a request for our IP, reply */
    if (op == 1 && tpa_host == g_ip_host) {
        arp_pkt_t reply;
        memset(&reply, 0, sizeof(reply));
        reply.htype = htons(1);
        reply.ptype = htons(0x0800);
        reply.hlen = 6;
        reply.plen = 4;
        reply.oper = htons(2);
        memcpy(reply.sha, g_mac, 6);
        reply.spa = htonl(g_ip_host);
        memcpy(reply.tha, arp->sha, 6);
        reply.tpa = htonl(spa_host);
        net_send_eth(arp->sha, ETH_TYPE_ARP, &reply, sizeof(reply));
    }
}

static int arp_resolve(uint32_t ip_host, uint8_t out_mac[6], uint32_t timeout_ms) {
    /* Resolve self without ARP */
    if (ip_host == g_ip_host) {
        memcpy(out_mac, g_mac, 6);
        return 1;
    }

    if (arp_cache.valid && arp_cache.ip_host == ip_host) {
        memcpy(out_mac, arp_cache.mac, 6);
        return 1;
    }

    /* Ask */
    arp_send_request(ip_host);

    uint64_t start = pit_seconds();
    uint32_t timeout_s = (timeout_ms + 999) / 1000;
    if (timeout_s == 0) timeout_s = 1;

    while ((uint32_t)(pit_seconds() - start) < timeout_s) {
        net_poll();
        if (arp_cache.valid && arp_cache.ip_host == ip_host) {
            memcpy(out_mac, arp_cache.mac, 6);
            return 1;
        }
    }
    terminal_write("[net][arp] resolve failed req=0x");
    static const char hex[] = "0123456789ABCDEF";
    char h[9];
    uint32_t v = ip_host;
    for (int i = 7; i >= 0; --i) {
        h[i] = hex[v & 0xF];
        v >>= 4;
    }
    h[8] = '\0';
    terminal_write(h);
    if (arp_cache.valid) {
        terminal_write(" cache=0x");
        v = arp_cache.ip_host;
        for (int i = 7; i >= 0; --i) {
            h[i] = hex[v & 0xF];
            v >>= 4;
        }
        h[8] = '\0';
        terminal_write(h);
    } else {
        terminal_write(" cache=<empty>");
    }
    terminal_write_line("");
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
        if (!arp_resolve(ntohl(ip->src), dst_mac, 1000)) return;

        ipv4_hdr_t iph;
        iph.ver_ihl = 0x45;
        iph.tos = 0;
        iph.total_len = htons((uint16_t)(sizeof(ipv4_hdr_t) + payload_len));
        iph.id = htons(0x4242);
        iph.frag_off = htons(0);
        iph.ttl = 64;
        iph.proto = 1;
        iph.hdr_csum = 0;
        iph.src = htonl(g_ip_host);
        iph.dst = ip->src; /* already network order in received packet */
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
    if (ntohl(ip->dst) != g_ip_host) return;

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

    /* Store IPs in proper host byte order: first octet in the most-significant
     * byte of the uint32_t, matching what htonl()/ntohl() expect on x86.
     *   10.0.2.15  -> 0x0A02000F
     *   10.0.2.2   -> 0x0A020002
     *   255.255.255.0 -> 0xFFFFFF00
     * Previously the values were stored little-endian (first octet in LSB),
     * which made htonl() produce the wrong network-byte-order value and caused
     * ntohl(ip->dst) != g_ip_host, so every incoming IPv4 packet was dropped. */
    g_ip_host      = (10u << 24) | (0u << 16) | (2u << 8) | 15u;  /* 10.0.2.15  */
    g_gw_ip_host   = (10u << 24) | (0u << 16) | (2u << 8) |  2u;  /* 10.0.2.2   */
    g_netmask_host = (255u << 24) | (255u << 16) | (255u << 8) | 0u; /* 255.255.255.0 */

    arp_cache.valid = 0;
    g_ping_got_reply = 0;

    rtl8139_set_rx_handler(net_rx_handler, 0);
    terminal_write_line("[net] net stack ready (ARP/IPv4/ICMP).");
}

void net_poll(void) {
    (void)rtl8139_poll_rx(8);
}

int net_ping(uint32_t dst_ip_host, uint32_t timeout_ms, uint32_t *rtt_ms_out) {
    if (rtt_ms_out) *rtt_ms_out = 0;

    /* Loopback: ping own IP without NIC/ARP */
    if (dst_ip_host == g_ip_host) {
        g_ping_got_reply = 1;
        if (rtt_ms_out) *rtt_ms_out = 0;
        return 0;
    }

    /* Route: if destination is not on-link, send via default gateway */
    uint32_t next_hop_ip =
        ((dst_ip_host & g_netmask_host) == (g_ip_host & g_netmask_host))
            ? dst_ip_host
            : g_gw_ip_host;
    
    uint8_t dst_mac[6];
    if (!arp_resolve(next_hop_ip, dst_mac, timeout_ms)) {
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
    ip.src = htonl(g_ip_host);
    ip.dst = htonl(dst_ip_host);
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

int net_parse_ipv4(const char *s, uint32_t *out_ip_host) {
    if (!s || !out_ip_host) return 0;
    uint32_t parts[4] = {0, 0, 0, 0};
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

    /* Build in proper host byte order: first octet (parts[0]) in the
     * most-significant byte so that htonl(ip_host) produces the correct
     * network-byte-order value.  E.g. "10.0.2.2" -> 0x0A020002. */
    *out_ip_host = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
    return 1;
}

