#include <rtl8139.h>
#include <pci.h>
#include <terminal.h>

/* Realtek RTL8139 vendor/device IDs */
#define RTL8139_VENDOR_ID 0x10EC
#define RTL8139_DEVICE_ID 0x8139

static rtl8139_info_t g_rtl8139;

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
}

