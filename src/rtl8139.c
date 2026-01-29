#include <rtl8139.h>
#include <pci.h>
#include <terminal.h>
#include <memory.h>
#include <io.h>
#include <string.h>

/* Realtek RTL8139 vendor/device IDs */
#define RTL8139_VENDOR_ID 0x10EC
#define RTL8139_DEVICE_ID 0x8139

static rtl8139_info_t g_rtl8139;

/* RTL8139 I/O register offsets */
#define RTL_IDR0     0x00 /* MAC 0..5 */
#define RTL_TSD0     0x10
#define RTL_TSAD0    0x20
#define RTL_RBSTART  0x30
#define RTL_CAPR     0x38
#define RTL_CBR      0x3A
#define RTL_IMR      0x3C
#define RTL_ISR      0x3E
#define RTL_CR       0x37
#define RTL_TCR      0x40
#define RTL_RCR      0x44
#define RTL_CONFIG1  0x52

/* CR bits */
#define RTL_CR_RX_ENABLE 0x08
#define RTL_CR_TX_ENABLE 0x04
#define RTL_CR_RESET     0x10

/* RX buffer: 8K + 16 + 1500 (datasheet common) */
#define RTL_RX_BUF_SIZE (8192u + 16u + 1500u)

static uint8_t *rtl_rx_buf = NULL;
static uint32_t rtl_rx_offset = 0;

static uint8_t *rtl_tx_buf = NULL;
static size_t rtl_tx_buf_size = 0;

static rtl8139_rx_handler_t rtl_rx_handler = 0;
static void *rtl_rx_user = 0;

void rtl8139_set_rx_handler(rtl8139_rx_handler_t handler, void *user) {
    rtl_rx_handler = handler;
    rtl_rx_user = user;
}

static inline uint16_t rtl_io(void) {
    return g_rtl8139.io_base;
}

static void term_hex8(uint8_t v) {
    static const char hex[] = "0123456789ABCDEF";
    char b[3];
    b[0] = hex[(v >> 4) & 0xF];
    b[1] = hex[v & 0xF];
    b[2] = '\0';
    terminal_write(b);
}

static void term_hex16(uint16_t v) {
    term_hex8((uint8_t)((v >> 8) & 0xFF));
    term_hex8((uint8_t)(v & 0xFF));
}

static void rtl_dump_frame_brief(const uint8_t *frame, size_t len) {
    if (len < 14) {
        terminal_write_line("  [rx] runt frame");
        return;
    }
    uint16_t ethertype = (uint16_t)((frame[12] << 8) | frame[13]);
    terminal_write("  [rx] len=");
    /* decimal print */
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

    terminal_write(" eth=0x");
    term_hex16(ethertype);
    terminal_write(" src=");
    for (int k = 6; k < 12; ++k) {
        term_hex8(frame[k]);
        if (k != 11) terminal_write(":");
    }
    terminal_write(" dst=");
    for (int k = 0; k < 6; ++k) {
        term_hex8(frame[k]);
        if (k != 5) terminal_write(":");
    }
    terminal_write_line("");

    /* Dump first 32 bytes */
    size_t dump = (len < 32) ? len : 32;
    terminal_write("       ");
    for (size_t j = 0; j < dump; ++j) {
        term_hex8(frame[j]);
        terminal_write(j + 1 == dump ? "" : " ");
    }
    terminal_write_line("");
}

const rtl8139_info_t *rtl8139_get_info(void) {
    return g_rtl8139.present ? &g_rtl8139 : 0;
}

void rtl8139_init(void) {
    pci_device_info_t pci;
    if (!pci_find_device(RTL8139_VENDOR_ID, RTL8139_DEVICE_ID, &pci)) {
        terminal_write_line("[net] RTL8139 not found on PCI bus.");
        g_rtl8139.present = 0;
        return;
    }

    /* BAR0 содержит I/O base для RTL8139 */
    uint32_t bar0 = pci_cfg_read32(pci.bus, pci.device, pci.function, 0x10);
    uint16_t io_base = (uint16_t)(bar0 & ~0x3u); /* убрать флаги */

    uint8_t irq_line = (uint8_t)(pci_cfg_read32(pci.bus, pci.device, pci.function, 0x3C) & 0xFFu);

    g_rtl8139.present = 1;
    g_rtl8139.bus = pci.bus;
    g_rtl8139.device = pci.device;
    g_rtl8139.function = pci.function;
    g_rtl8139.io_base = io_base;
    g_rtl8139.irq_line = irq_line;

    /* Enable bus mastering */
    uint32_t cmd = pci_cfg_read32(pci.bus, pci.device, pci.function, 0x04);
    cmd |= (1u << 2); /* Bus Master Enable */
    pci_cfg_write32(pci.bus, pci.device, pci.function, 0x04, cmd);

    /* Read MAC */
    for (int i = 0; i < 6; ++i) {
        g_rtl8139.mac[i] = inb((uint16_t)(io_base + RTL_IDR0 + i));
    }

    terminal_write_line("[net] RTL8139 detected:");
    terminal_write("       PCI ");
    char buf[4];
    buf[0] = '0' + (g_rtl8139.bus / 10);
    buf[1] = '0' + (g_rtl8139.bus % 10);
    buf[2] = '\0';
    terminal_write("bus=");
    terminal_write(buf);
    terminal_write(" dev=");
    buf[0] = '0' + (g_rtl8139.device / 10);
    buf[1] = '0' + (g_rtl8139.device % 10);
    buf[2] = '\0';
    terminal_write(buf);
    terminal_write(" fn=");
    buf[0] = '0' + g_rtl8139.function;
    buf[1] = '\0';
    terminal_write(buf);
    terminal_write_line("");

    terminal_write("       IO base=0x");
    /* simple hex print for io_base */
    static const char hex[] = "0123456789ABCDEF";
    char hbuf[5];
    hbuf[0] = hex[(io_base >> 12) & 0xF];
    hbuf[1] = hex[(io_base >> 8) & 0xF];
    hbuf[2] = hex[(io_base >> 4) & 0xF];
    hbuf[3] = hex[io_base & 0xF];
    hbuf[4] = '\0';
    terminal_write(hbuf);
    terminal_write(" irq=");
    buf[0] = '0' + (irq_line / 10);
    buf[1] = '0' + (irq_line % 10);
    buf[2] = '\0';
    terminal_write(buf);
    terminal_write_line("");

    terminal_write("       MAC=");
    for (int i = 0; i < 6; ++i) {
        term_hex8(g_rtl8139.mac[i]);
        if (i != 5) terminal_write(":");
    }
    terminal_write_line("");

    /* Basic hardware init (polling RX/TX) */
    /* Power on (CONFIG1 bit0 = 0 enables power?) Common trick: write 0x00 */
    outb((uint16_t)(rtl_io() + RTL_CONFIG1), 0x00);

    /* Reset */
    outb((uint16_t)(rtl_io() + RTL_CR), RTL_CR_RESET);
    for (uint32_t i = 0; i < 1000000u; ++i) {
        if ((inb((uint16_t)(rtl_io() + RTL_CR)) & RTL_CR_RESET) == 0) {
            break;
        }
    }

    /* Allocate RX/TX buffers (identity-mapped physical assumed) */
    if (!rtl_rx_buf) {
        rtl_rx_buf = (uint8_t *)kmalloc_aligned(RTL_RX_BUF_SIZE, 16);
        if (rtl_rx_buf) {
            memset(rtl_rx_buf, 0, RTL_RX_BUF_SIZE);
        }
    }
    if (!rtl_tx_buf) {
        rtl_tx_buf_size = 2048;
        rtl_tx_buf = (uint8_t *)kmalloc_aligned(rtl_tx_buf_size, 16);
        if (rtl_tx_buf) {
            memset(rtl_tx_buf, 0, rtl_tx_buf_size);
        }
    }

    if (!rtl_rx_buf || !rtl_tx_buf) {
        terminal_write_line("[net] RTL8139: buffer allocation failed.");
        g_rtl8139.present = 0;
        return;
    }

    rtl_rx_offset = 0;

    /* Set RX buffer start */
    outl((uint16_t)(rtl_io() + RTL_RBSTART), (uint32_t)(uintptr_t)rtl_rx_buf);

    /* Disable interrupts for now (polling) */
    outw((uint16_t)(rtl_io() + RTL_IMR), 0x0000);
    outw((uint16_t)(rtl_io() + RTL_ISR), 0xFFFF);

    /* Accept broadcast + physical match, enable wrap */
    uint32_t rcr = (1u << 1) | (1u << 3) | (1u << 7);
    outl((uint16_t)(rtl_io() + RTL_RCR), rcr);

    /* Enable RX/TX */
    outb((uint16_t)(rtl_io() + RTL_CR), (uint8_t)(RTL_CR_RX_ENABLE | RTL_CR_TX_ENABLE));

    /* Point TX addr0 to our buffer */
    outl((uint16_t)(rtl_io() + RTL_TSAD0), (uint32_t)(uintptr_t)rtl_tx_buf);

    terminal_write_line("[net] RTL8139 initialized (polling RX/TX).");
}

int rtl8139_send_frame(const void *data, size_t len) {
    if (!g_rtl8139.present || !rtl_tx_buf || len == 0 || len > 1518) {
        return -1;
    }
    if (len > rtl_tx_buf_size) {
        return -2;
    }
    memcpy(rtl_tx_buf, data, len);

    /* Clear status bits by reading */
    (void)inl((uint16_t)(rtl_io() + RTL_TSD0));

    /* Write length to TSD0 kicks transmission */
    outl((uint16_t)(rtl_io() + RTL_TSD0), (uint32_t)len);

    /* Poll completion with bounded wait */
    for (uint32_t i = 0; i < 1000000u; ++i) {
        uint32_t st = inl((uint16_t)(rtl_io() + RTL_TSD0));
        /* TOK (bit 15) indicates transmit OK */
        if (st & (1u << 15)) {
            return 0;
        }
    }
    return -3;
}

int rtl8139_poll_rx(int max_frames) {
    if (!g_rtl8139.present || !rtl_rx_buf) {
        return 0;
    }
    int processed = 0;

    while (processed < max_frames) {
        uint16_t cbr = inw((uint16_t)(rtl_io() + RTL_CBR));
        /* If current buffer write pointer equals our offset, nothing new */
        if (cbr == (uint16_t)rtl_rx_offset) {
            break;
        }

        /* Packet header at rx_offset: status (2), length(2) */
        uint32_t off = rtl_rx_offset;
        if (off + 4 >= RTL_RX_BUF_SIZE) {
            off = 0;
            rtl_rx_offset = 0;
        }

        uint16_t status = *(uint16_t *)(rtl_rx_buf + off);
        uint16_t length = *(uint16_t *)(rtl_rx_buf + off + 2);
        off += 4;

        if (length == 0 || length > 0x2000) {
            terminal_write_line("  [rx] invalid length, resetting offset");
            rtl_rx_offset = 0;
            outw((uint16_t)(rtl_io() + RTL_CAPR), 0);
            break;
        }

        /* status bit0 = ROK */
        if ((status & 0x0001u) == 0) {
            terminal_write_line("  [rx] frame error");
        } else {
            /* length includes CRC; drop CRC (4 bytes) if present */
            size_t frame_len = (length >= 4) ? (size_t)(length - 4) : (size_t)length;
            if (off + frame_len <= RTL_RX_BUF_SIZE) {
                const uint8_t *frame = rtl_rx_buf + off;
                if (rtl_rx_handler) {
                    rtl_rx_handler(frame, frame_len, rtl_rx_user);
                } else {
                    rtl_dump_frame_brief(frame, frame_len);
                }
            } else {
                terminal_write_line("  [rx] wrapped frame (not handled yet)");
            }
        }

        /* Advance offset (status+len header + length), align 4 */
        rtl_rx_offset = (uint32_t)((rtl_rx_offset + 4u + (uint32_t)length + 3u) & ~3u);
        if (rtl_rx_offset >= RTL_RX_BUF_SIZE) {
            rtl_rx_offset %= RTL_RX_BUF_SIZE;
        }

        /* CAPR should be set to (offset - 16) */
        uint16_t capr = (uint16_t)((rtl_rx_offset - 16u) & 0xFFFFu);
        outw((uint16_t)(rtl_io() + RTL_CAPR), capr);

        processed++;
    }

    return processed;
}

