#include <pci.h>
#include <io.h>
#include <terminal.h>

#define PCI_CFG_ADDR 0xCF8
#define PCI_CFG_DATA 0xCFC

static void term_print_hex8(uint8_t v) {
    static const char hex[] = "0123456789ABCDEF";
    char b[3];
    b[0] = hex[(v >> 4) & 0xF];
    b[1] = hex[v & 0xF];
    b[2] = '\0';
    terminal_write(b);
}

static void term_print_hex16(uint16_t v) {
    term_print_hex8((uint8_t)((v >> 8) & 0xFF));
    term_print_hex8((uint8_t)(v & 0xFF));
}

uint32_t pci_cfg_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    /* offset должен быть выровнен по 4 */
    uint32_t addr =
        (1u << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)device << 11) |
        ((uint32_t)function << 8) |
        ((uint32_t)offset & 0xFCu);
    outl(PCI_CFG_ADDR, addr);
    return inl(PCI_CFG_DATA);
}

void pci_cfg_write32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    uint32_t addr =
        (1u << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)device << 11) |
        ((uint32_t)function << 8) |
        ((uint32_t)offset & 0xFCu);
    outl(PCI_CFG_ADDR, addr);
    outl(PCI_CFG_DATA, value);
}

static uint16_t pci_cfg_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t v = pci_cfg_read32(bus, device, function, offset);
    uint8_t shift = (uint8_t)((offset & 2u) * 8u);
    return (uint16_t)((v >> shift) & 0xFFFFu);
}

static uint8_t pci_cfg_read8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t v = pci_cfg_read32(bus, device, function, offset);
    uint8_t shift = (uint8_t)((offset & 3u) * 8u);
    return (uint8_t)((v >> shift) & 0xFFu);
}

static int pci_read_device_info(uint8_t bus, uint8_t dev, uint8_t fn, pci_device_info_t *out) {
    uint16_t vendor = pci_cfg_read16(bus, dev, fn, 0x00);
    if (vendor == 0xFFFFu) {
        return 0;
    }

    if (out) {
        out->bus = bus;
        out->device = dev;
        out->function = fn;
        out->vendor_id = vendor;
        out->device_id = pci_cfg_read16(bus, dev, fn, 0x02);
        out->revision_id = pci_cfg_read8(bus, dev, fn, 0x08);
        out->prog_if = pci_cfg_read8(bus, dev, fn, 0x09);
        out->subclass = pci_cfg_read8(bus, dev, fn, 0x0A);
        out->class_code = pci_cfg_read8(bus, dev, fn, 0x0B);
    }
    return 1;
}

void pci_scan_and_print(void) {
    terminal_write_line("[pci] Scanning PCI bus...");

    int found = 0;

    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint8_t dev = 0; dev < 32; ++dev) {
            /* function 0: проверим, есть ли устройство */
            uint16_t vendor0 = pci_cfg_read16((uint8_t)bus, dev, 0, 0x00);
            if (vendor0 == 0xFFFFu) {
                continue;
            }

            /* header type чтобы понять multi-function */
            uint8_t header_type = pci_cfg_read8((uint8_t)bus, dev, 0, 0x0E);
            uint8_t fn_max = (header_type & 0x80u) ? 8 : 1;

            for (uint8_t fn = 0; fn < fn_max; ++fn) {
                pci_device_info_t info;
                if (!pci_read_device_info((uint8_t)bus, dev, fn, &info)) {
                    continue;
                }

                found++;
                terminal_write("  ");
                terminal_write("bus ");
                term_print_hex8(info.bus);
                terminal_write(" dev ");
                term_print_hex8(info.device);
                terminal_write(" fn ");
                term_print_hex8(info.function);
                terminal_write("  vid:did ");
                term_print_hex16(info.vendor_id);
                terminal_write(":");
                term_print_hex16(info.device_id);
                terminal_write("  class ");
                term_print_hex8(info.class_code);
                terminal_write("/");
                term_print_hex8(info.subclass);
                terminal_write("/");
                term_print_hex8(info.prog_if);
                terminal_write("  rev ");
                term_print_hex8(info.revision_id);
                terminal_write_line("");
            }
        }
    }

    if (!found) {
        terminal_write_line("[pci] No PCI devices found.");
    } else {
        terminal_write("[pci] Found devices: ");
        /* простая печать found */
        char buf[16];
        int n = found;
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
        terminal_write_line("");
    }
}

int pci_find_device(uint16_t vendor_id, uint16_t device_id, pci_device_info_t *out) {
    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint8_t dev = 0; dev < 32; ++dev) {
            uint16_t vendor0 = pci_cfg_read16((uint8_t)bus, dev, 0, 0x00);
            if (vendor0 == 0xFFFFu) {
                continue;
            }

            uint8_t header_type = pci_cfg_read8((uint8_t)bus, dev, 0, 0x0E);
            uint8_t fn_max = (header_type & 0x80u) ? 8 : 1;

            for (uint8_t fn = 0; fn < fn_max; ++fn) {
                pci_device_info_t info;
                if (!pci_read_device_info((uint8_t)bus, dev, fn, &info)) {
                    continue;
                }
                if (info.vendor_id == vendor_id && info.device_id == device_id) {
                    if (out) {
                        *out = info;
                    }
                    return 1;
                }
            }
        }
    }
    return 0;
}

