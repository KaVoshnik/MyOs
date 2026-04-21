#include <graphics.h>
#include <memory.h>
#include <string.h>
#include <io.h>
#include <mouse.h>
#include <keyboard.h>
#include <pit.h>
#include <terminal.h>
#include <stdint.h>
#include <stddef.h>

/* =========================================================
 * Multiboot framebuffer (provided by kernel.c)
 * ========================================================= */
struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower, mem_upper, boot_device, cmdline;
    uint32_t mods_count, mods_addr, syms[4];
    uint32_t mmap_length, mmap_addr;
    uint32_t drives_length, drives_addr;
    uint32_t config_table, boot_loader_name, apm_table;
    uint32_t vbe_control_info, vbe_mode_info;
    uint16_t vbe_mode, vbe_interface_seg, vbe_interface_off, vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint8_t  color_info[6];
} __attribute__((packed));

extern struct multiboot_info *mb_info;

/* =========================================================
 * 8×16 VGA-style font  (printable ASCII 32–127)
 * Each glyph is 16 bytes, one byte per row, MSB = leftmost pixel.
 * ========================================================= */
static const uint8_t font8x16[96][16] = {
    /* 0x20 space */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    /* 0x21 !     */ {0,0,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0,0x18,0x18,0,0,0,0},
    /* 0x22 "     */ {0,0,0x6C,0x6C,0x6C,0,0,0,0,0,0,0,0,0,0,0},
    /* 0x23 #     */ {0,0,0x36,0x36,0x7F,0x36,0x36,0x36,0x7F,0x36,0x36,0x36,0,0,0,0},
    /* 0x24 $     */ {0,0x18,0x18,0x3E,0x63,0x03,0x1E,0x30,0x60,0x63,0x3E,0x18,0x18,0,0,0},
    /* 0x25 %     */ {0,0,0,0x23,0x33,0x18,0x0C,0x06,0x33,0x31,0,0,0,0,0,0},
    /* 0x26 &     */ {0,0,0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x33,0x6E,0,0,0,0,0,0},
    /* 0x27 '     */ {0,0,0x18,0x18,0x18,0,0,0,0,0,0,0,0,0,0,0},
    /* 0x28 (     */ {0,0,0x0C,0x06,0x03,0x03,0x03,0x03,0x03,0x06,0x0C,0,0,0,0,0},
    /* 0x29 )     */ {0,0,0x30,0x60,0xC0,0xC0,0xC0,0xC0,0xC0,0x60,0x30,0,0,0,0,0},
    /* 0x2A *     */ {0,0,0,0,0x63,0x36,0x1C,0x7F,0x1C,0x36,0x63,0,0,0,0,0},
    /* 0x2B +     */ {0,0,0,0,0x18,0x18,0x18,0xFF,0x18,0x18,0x18,0,0,0,0,0},
    /* 0x2C ,     */ {0,0,0,0,0,0,0,0,0,0x18,0x18,0x18,0x0C,0,0,0},
    /* 0x2D -     */ {0,0,0,0,0,0,0,0xFF,0,0,0,0,0,0,0,0},
    /* 0x2E .     */ {0,0,0,0,0,0,0,0,0,0,0x18,0x18,0,0,0,0},
    /* 0x2F /     */ {0,0,0x60,0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0,0,0,0,0,0},
    /* 0x30 0     */ {0,0,0x3E,0x63,0x63,0x73,0x6B,0x67,0x63,0x63,0x3E,0,0,0,0,0},
    /* 0x31 1     */ {0,0,0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3F,0,0,0,0,0},
    /* 0x32 2     */ {0,0,0x1E,0x33,0x30,0x30,0x18,0x0C,0x06,0x33,0x3F,0,0,0,0,0},
    /* 0x33 3     */ {0,0,0x1E,0x33,0x30,0x30,0x1C,0x30,0x30,0x33,0x1E,0,0,0,0,0},
    /* 0x34 4     */ {0,0,0x38,0x3C,0x36,0x33,0x33,0x7F,0x30,0x30,0x78,0,0,0,0,0},
    /* 0x35 5     */ {0,0,0x3F,0x03,0x03,0x03,0x1F,0x30,0x30,0x33,0x1E,0,0,0,0,0},
    /* 0x36 6     */ {0,0,0x1C,0x06,0x03,0x03,0x1F,0x33,0x33,0x33,0x1E,0,0,0,0,0},
    /* 0x37 7     */ {0,0,0x7F,0x63,0x30,0x18,0x0C,0x0C,0x0C,0x0C,0x0C,0,0,0,0,0},
    /* 0x38 8     */ {0,0,0x1E,0x33,0x33,0x33,0x1E,0x33,0x33,0x33,0x1E,0,0,0,0,0},
    /* 0x39 9     */ {0,0,0x1E,0x33,0x33,0x33,0x3E,0x30,0x30,0x18,0x0E,0,0,0,0,0},
    /* 0x3A :     */ {0,0,0,0,0x18,0x18,0,0,0,0x18,0x18,0,0,0,0,0},
    /* 0x3B ;     */ {0,0,0,0,0x18,0x18,0,0,0,0x18,0x18,0x0C,0,0,0,0},
    /* 0x3C <     */ {0,0,0,0x30,0x18,0x0C,0x06,0x06,0x0C,0x18,0x30,0,0,0,0,0},
    /* 0x3D =     */ {0,0,0,0,0,0x7E,0,0,0x7E,0,0,0,0,0,0,0},
    /* 0x3E >     */ {0,0,0,0x06,0x0C,0x18,0x30,0x30,0x18,0x0C,0x06,0,0,0,0,0},
    /* 0x3F ?     */ {0,0,0x1E,0x33,0x30,0x18,0x0C,0x0C,0,0x0C,0x0C,0,0,0,0,0},
    /* 0x40 @     */ {0,0,0x3E,0x63,0x63,0x6F,0x6B,0x6B,0x6F,0x03,0x3E,0,0,0,0,0},
    /* 0x41 A     */ {0,0,0x0C,0x1E,0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0,0,0,0,0},
    /* 0x42 B     */ {0,0,0x1F,0x33,0x33,0x33,0x1F,0x33,0x33,0x33,0x1F,0,0,0,0,0},
    /* 0x43 C     */ {0,0,0x1E,0x33,0x03,0x03,0x03,0x03,0x03,0x33,0x1E,0,0,0,0,0},
    /* 0x44 D     */ {0,0,0x0F,0x1B,0x33,0x33,0x33,0x33,0x33,0x1B,0x0F,0,0,0,0,0},
    /* 0x45 E     */ {0,0,0x3F,0x03,0x03,0x03,0x1F,0x03,0x03,0x03,0x3F,0,0,0,0,0},
    /* 0x46 F     */ {0,0,0x3F,0x03,0x03,0x03,0x1F,0x03,0x03,0x03,0x03,0,0,0,0,0},
    /* 0x47 G     */ {0,0,0x1E,0x33,0x03,0x03,0x3B,0x33,0x33,0x33,0x1E,0,0,0,0,0},
    /* 0x48 H     */ {0,0,0x33,0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x33,0,0,0,0,0},
    /* 0x49 I     */ {0,0,0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0,0,0,0,0},
    /* 0x4A J     */ {0,0,0x78,0x30,0x30,0x30,0x30,0x30,0x33,0x33,0x1E,0,0,0,0,0},
    /* 0x4B K     */ {0,0,0x33,0x1B,0x0F,0x07,0x07,0x0F,0x1B,0x33,0x63,0,0,0,0,0},
    /* 0x4C L     */ {0,0,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x3F,0,0,0,0,0},
    /* 0x4D M     */ {0,0,0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x63,0x63,0,0,0,0,0},
    /* 0x4E N     */ {0,0,0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x63,0x63,0,0,0,0,0},
    /* 0x4F O     */ {0,0,0x1E,0x33,0x33,0x33,0x33,0x33,0x33,0x33,0x1E,0,0,0,0,0},
    /* 0x50 P     */ {0,0,0x1F,0x33,0x33,0x33,0x1F,0x03,0x03,0x03,0x03,0,0,0,0,0},
    /* 0x51 Q     */ {0,0,0x1E,0x33,0x33,0x33,0x33,0x33,0x3B,0x1E,0x38,0,0,0,0,0},
    /* 0x52 R     */ {0,0,0x1F,0x33,0x33,0x33,0x1F,0x0F,0x1B,0x33,0x63,0,0,0,0,0},
    /* 0x53 S     */ {0,0,0x1E,0x33,0x03,0x03,0x1E,0x30,0x30,0x33,0x1E,0,0,0,0,0},
    /* 0x54 T     */ {0,0,0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0,0,0,0,0},
    /* 0x55 U     */ {0,0,0x33,0x33,0x33,0x33,0x33,0x33,0x33,0x33,0x1E,0,0,0,0,0},
    /* 0x56 V     */ {0,0,0x33,0x33,0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0,0,0,0,0},
    /* 0x57 W     */ {0,0,0x63,0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x41,0,0,0,0,0},
    /* 0x58 X     */ {0,0,0x63,0x63,0x36,0x1C,0x1C,0x1C,0x36,0x63,0x63,0,0,0,0,0},
    /* 0x59 Y     */ {0,0,0x33,0x33,0x33,0x33,0x1E,0x0C,0x0C,0x0C,0x1E,0,0,0,0,0},
    /* 0x5A Z     */ {0,0,0x7F,0x63,0x30,0x18,0x0C,0x06,0x03,0x63,0x7F,0,0,0,0,0},
    /* 0x5B [     */ {0,0,0x1E,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x1E,0,0,0,0,0},
    /* 0x5C \     */ {0,0,0x01,0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0,0,0,0,0,0},
    /* 0x5D ]     */ {0,0,0x1E,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x1E,0,0,0,0,0},
    /* 0x5E ^     */ {0,0,0x08,0x1C,0x36,0x63,0,0,0,0,0,0,0,0,0,0},
    /* 0x5F _     */ {0,0,0,0,0,0,0,0,0,0,0,0xFF,0,0,0,0},
    /* 0x60 `     */ {0,0,0x0C,0x0C,0x18,0,0,0,0,0,0,0,0,0,0,0},
    /* 0x61 a     */ {0,0,0,0,0,0x1E,0x30,0x3E,0x33,0x33,0x6E,0,0,0,0,0},
    /* 0x62 b     */ {0,0,0x03,0x03,0x03,0x1F,0x33,0x33,0x33,0x33,0x1F,0,0,0,0,0},
    /* 0x63 c     */ {0,0,0,0,0,0x1E,0x33,0x03,0x03,0x33,0x1E,0,0,0,0,0},
    /* 0x64 d     */ {0,0,0x30,0x30,0x30,0x3E,0x33,0x33,0x33,0x33,0x6E,0,0,0,0,0},
    /* 0x65 e     */ {0,0,0,0,0,0x1E,0x33,0x3F,0x03,0x33,0x1E,0,0,0,0,0},
    /* 0x66 f     */ {0,0,0x1C,0x36,0x06,0x06,0x1F,0x06,0x06,0x06,0x0F,0,0,0,0,0},
    /* 0x67 g     */ {0,0,0,0,0,0x6E,0x33,0x33,0x33,0x3E,0x30,0x33,0x1E,0,0,0},
    /* 0x68 h     */ {0,0,0x03,0x03,0x03,0x1B,0x37,0x33,0x33,0x33,0x73,0,0,0,0,0},
    /* 0x69 i     */ {0,0,0x0C,0,0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0,0,0,0,0},
    /* 0x6A j     */ {0,0,0x30,0,0x38,0x30,0x30,0x30,0x30,0x30,0x33,0x1E,0,0,0,0},
    /* 0x6B k     */ {0,0,0x03,0x03,0x33,0x1B,0x0F,0x0F,0x1B,0x33,0x63,0,0,0,0,0},
    /* 0x6C l     */ {0,0,0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0,0,0,0,0},
    /* 0x6D m     */ {0,0,0,0,0,0x63,0x77,0x7F,0x6B,0x63,0x63,0,0,0,0,0},
    /* 0x6E n     */ {0,0,0,0,0,0x1F,0x33,0x33,0x33,0x33,0x33,0,0,0,0,0},
    /* 0x6F o     */ {0,0,0,0,0,0x1E,0x33,0x33,0x33,0x33,0x1E,0,0,0,0,0},
    /* 0x70 p     */ {0,0,0,0,0,0x1F,0x33,0x33,0x33,0x1F,0x03,0x03,0x03,0,0,0},
    /* 0x71 q     */ {0,0,0,0,0,0x6E,0x33,0x33,0x33,0x3E,0x30,0x30,0x30,0,0,0},
    /* 0x72 r     */ {0,0,0,0,0,0x37,0x1B,0x03,0x03,0x03,0x07,0,0,0,0,0},
    /* 0x73 s     */ {0,0,0,0,0,0x1E,0x33,0x06,0x18,0x33,0x1E,0,0,0,0,0},
    /* 0x74 t     */ {0,0,0,0x06,0x06,0x1F,0x06,0x06,0x06,0x36,0x1C,0,0,0,0,0},
    /* 0x75 u     */ {0,0,0,0,0,0x33,0x33,0x33,0x33,0x33,0x6E,0,0,0,0,0},
    /* 0x76 v     */ {0,0,0,0,0,0x33,0x33,0x33,0x33,0x1E,0x0C,0,0,0,0,0},
    /* 0x77 w     */ {0,0,0,0,0,0x63,0x63,0x6B,0x7F,0x77,0x63,0,0,0,0,0},
    /* 0x78 x     */ {0,0,0,0,0,0x63,0x36,0x1C,0x1C,0x36,0x63,0,0,0,0,0},
    /* 0x79 y     */ {0,0,0,0,0,0x33,0x33,0x33,0x33,0x3E,0x30,0x18,0x0F,0,0,0},
    /* 0x7A z     */ {0,0,0,0,0,0x3F,0x18,0x0C,0x06,0x03,0x3F,0,0,0,0,0},
    /* 0x7B {     */ {0,0,0x38,0x0C,0x0C,0x0C,0x07,0x0C,0x0C,0x0C,0x38,0,0,0,0,0},
    /* 0x7C |     */ {0,0,0x18,0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x18,0,0,0,0,0},
    /* 0x7D }     */ {0,0,0x07,0x0C,0x0C,0x0C,0x38,0x0C,0x0C,0x0C,0x07,0,0,0,0,0},
    /* 0x7E ~     */ {0,0,0x6E,0x3B,0,0,0,0,0,0,0,0,0,0,0,0},
    /* 0x7F DEL   */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};

/* =========================================================
 * Bochs/QEMU VBE registers (I/O ports 0x01CE / 0x01CF)
 * Работают на QEMU с -vga std, -vga qxl, bochs-display
 * ========================================================= */
#define VBE_DISPI_IOPORT_INDEX  0x01CE
#define VBE_DISPI_IOPORT_DATA   0x01CF

#define VBE_DISPI_INDEX_ID          0
#define VBE_DISPI_INDEX_XRES        1
#define VBE_DISPI_INDEX_YRES        2
#define VBE_DISPI_INDEX_BPP         3
#define VBE_DISPI_INDEX_ENABLE      4
#define VBE_DISPI_INDEX_BANK        5
#define VBE_DISPI_INDEX_VIRT_WIDTH  6
#define VBE_DISPI_INDEX_VIRT_HEIGHT 7
#define VBE_DISPI_INDEX_X_OFFSET    8
#define VBE_DISPI_INDEX_Y_OFFSET    9

#define VBE_DISPI_DISABLED      0x00
#define VBE_DISPI_ENABLED       0x01
#define VBE_DISPI_LFB_ENABLED   0x40
#define VBE_DISPI_NOCLEARMEM    0x80

/* Linear framebuffer base — fallback if PCI scan finds nothing.
 * On QEMU -vga std the BGA/Bochs VBE LFB is typically at 0xE0000000,
 * but we always try to read the real address from PCI BAR0 first.  */
#define VBE_LFB_FALLBACK  0xE0000000u

static uint16_t vbe_read(uint16_t index) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}
static void vbe_write(uint16_t index, uint16_t data) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, data);
}

/* Read a PCI config dword without pulling in pci.h/pci.c */
static uint32_t _pci_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off) {
    uint32_t addr = (1u<<31)|((uint32_t)bus<<16)|((uint32_t)dev<<11)
                    |((uint32_t)fn<<8)|(off & 0xFCu);
    outl(0xCF8, addr);
    return inl(0xCFC);
}

/* Scan PCI bus 0 for a display-class device and return its BAR0 (MMIO base).
 * Returns 0 if nothing found.  Class 0x03 = Display controller.            */
static uint32_t _pci_find_vga_bar0(void) {
    for (uint8_t dev = 0; dev < 32; dev++) {
        uint32_t id = _pci_read32(0, dev, 0, 0x00);
        if ((id & 0xFFFF) == 0xFFFF) continue;          /* no device */
        uint8_t cls = (uint8_t)(_pci_read32(0, dev, 0, 0x08) >> 24);
        if (cls != 0x03) continue;                       /* not display */
        uint32_t bar0 = _pci_read32(0, dev, 0, 0x10);
        /* BAR0 bit0=0 → MMIO, bits 1-3 = type/prefetch flags */
        if (bar0 & 1u) continue;                         /* I/O BAR, skip */
        uint32_t base = bar0 & 0xFFFFFFF0u;
        if (base == 0) continue;
        return base;
    }
    return 0;
}

/* Try to set a VBE linear framebuffer mode via Bochs/BGA I/O ports.
 * Fills *lfb_addr_out with the real LFB physical address.
 * Returns 1 on success (VBE ID 0xB0C0-0xB0C5 detected), 0 otherwise. */
static int vbe_set_mode(uint16_t w, uint16_t h, uint16_t bpp,
                        uint32_t *lfb_addr_out) {
    uint16_t id = vbe_read(VBE_DISPI_INDEX_ID);
    if (id < 0xB0C0 || id > 0xB0C5) return 0;

    vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    vbe_write(VBE_DISPI_INDEX_XRES,   w);
    vbe_write(VBE_DISPI_INDEX_YRES,   h);
    vbe_write(VBE_DISPI_INDEX_BPP,    bpp);
    vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

    if (vbe_read(VBE_DISPI_INDEX_XRES) != w) return 0;
    if (vbe_read(VBE_DISPI_INDEX_YRES) != h) return 0;

    /* Find the real LFB address from PCI BAR0 */
    uint32_t bar = _pci_find_vga_bar0();
    *lfb_addr_out = bar ? bar : VBE_LFB_FALLBACK;
    return 1;
}

/* =========================================================
 * Module state
 * ========================================================= */
static gfx_ctx_t g_ctx;
static uint32_t g_hw_pitch_bytes     = 0;
static uint32_t g_hw_bytes_per_pixel = 4;
/* Channel shifts for packing into hw pixel (determined from Multiboot color_info) */
static uint8_t g_r_pos = 16; /* bit position of red   in hw pixel */
static uint8_t g_g_pos =  8; /* bit position of green in hw pixel */
static uint8_t g_b_pos =  0; /* bit position of blue  in hw pixel */

/* =========================================================
 * Init / flip
 * ========================================================= */

/* Pack 0x00RRGGBB into hardware pixel format using channel positions */
static inline uint32_t _pack_color(uint8_t r, uint8_t g2, uint8_t b) {
    return ((uint32_t)r  << g_r_pos)
         | ((uint32_t)g2 << g_g_pos)
         | ((uint32_t)b  << g_b_pos);
}

static void _hw_put(uint32_t x, uint32_t y, uint32_t color) {
    uint8_t *base = (uint8_t *)g_ctx.hw_framebuffer
                    + (size_t)y * g_hw_pitch_bytes
                    + (size_t)x * g_hw_bytes_per_pixel;
    uint8_t r  = (uint8_t)((color >> 16) & 0xFF);
    uint8_t g2 = (uint8_t)((color >>  8) & 0xFF);
    uint8_t b  = (uint8_t)( color        & 0xFF);

    switch (g_hw_bytes_per_pixel) {
        case 4: {
            uint32_t packed = _pack_color(r, g2, b);
            base[0]=(uint8_t)(packed      );
            base[1]=(uint8_t)(packed >>  8);
            base[2]=(uint8_t)(packed >> 16);
            base[3]=0;
            break;
        }
        case 3: {
            uint32_t packed = _pack_color(r, g2, b);
            base[0]=(uint8_t)(packed      );
            base[1]=(uint8_t)(packed >>  8);
            base[2]=(uint8_t)(packed >> 16);
            break;
        }
        case 2: {
            uint16_t v = (uint16_t)(((r&0xF8u)<<8)|((g2&0xFCu)<<3)|((b&0xF8u)>>3));
            base[0]=(uint8_t)(v&0xFF); base[1]=(uint8_t)(v>>8);
            break;
        }
        default: base[0]=b; break;
    }
}

/* Internal: finish init after hw params are known */
static int _graphics_finish_init(uint32_t w, uint32_t h,
                                  uint8_t bpp, uint32_t pitch_bytes,
                                  uint32_t *hw_fb) {
    g_ctx.hw_framebuffer = hw_fb;
    g_ctx.width          = w;
    g_ctx.height         = h;
    g_ctx.bpp            = bpp;
    g_hw_pitch_bytes     = pitch_bytes;
    g_hw_bytes_per_pixel = (bpp + 7u) / 8u;

    /* Read channel positions from Multiboot color_info if available.
     * color_info layout (Multiboot spec):
     *   [0] red_field_position   [1] red_mask_size
     *   [2] green_field_position [3] green_mask_size
     *   [4] blue_field_position  [5] blue_mask_size
     * For standard BGR framebuffer: R=16,G=8,B=0
     * For RGB (some cards):         R=0, G=8, B=16          */
    if (mb_info && (mb_info->flags & (1u << 12)) &&
        mb_info->framebuffer_type == 1) {
        g_r_pos = mb_info->color_info[0];
        g_g_pos = mb_info->color_info[2];
        g_b_pos = mb_info->color_info[4];
    } else {
        /* VBE/Bochs default: BGR layout (red at bit 16) */
        g_r_pos = 16; g_g_pos = 8; g_b_pos = 0;
    }

    /* Back-buffer always 32-bit, no padding */
    g_ctx.pitch_pixels = w;
    size_t buf = (size_t)w * h * 4;
    g_ctx.framebuffer = (uint32_t *)kmalloc(buf);
    if (!g_ctx.framebuffer) {
        g_ctx.framebuffer  = hw_fb;
        g_ctx.pitch_pixels = pitch_bytes / 4;
    }
    g_ctx.ready = 1;
    gfx_clear(GFX_BLACK);
    graphics_flip();
    return 0;
}

int graphics_init(void) {
    if (g_ctx.ready) return 0;

    /* ---- Path 1: Multiboot gave us a real linear framebuffer ---- */
    if (mb_info && (mb_info->flags & (1u << 12))) {
        uint64_t fb_addr  = mb_info->framebuffer_addr;
        uint32_t fb_w     = mb_info->framebuffer_width;
        uint32_t fb_h     = mb_info->framebuffer_height;
        uint8_t  fb_bpp   = mb_info->framebuffer_bpp;
        uint32_t fb_pitch = mb_info->framebuffer_pitch;
        uint8_t  fb_type  = mb_info->framebuffer_type;

        /* Reject: text mode (type=2) or tiny "resolution" (80x25) */
        int is_text = (fb_type == 2) ||
                      (fb_w <= 320 && fb_h <= 240) ||
                      (fb_bpp < 15);

        if (!is_text && fb_addr != 0 &&
            (fb_type == 1 || fb_type == 0) &&
            (fb_bpp == 15 || fb_bpp == 16 ||
             fb_bpp == 24 || fb_bpp == 32)) {

            return _graphics_finish_init(fb_w, fb_h, fb_bpp, fb_pitch,
                                         (uint32_t *)(uintptr_t)fb_addr);
        }
        /* Fall through to VBE self-setup */
    }

    /* ---- Path 2: Set mode ourselves via Bochs VBE ports ----------
     * Works on: QEMU -vga std, -vga qxl, -device bochs-display,
     *           VirtualBox, Bochs emulator.                         */
    {
        struct { uint16_t w, h; } modes[] = {
            {800,600}, {1024,768}, {640,480}
        };
        for (size_t i = 0; i < sizeof(modes)/sizeof(modes[0]); i++) {
            uint32_t lfb_addr = 0;
            if (vbe_set_mode(modes[i].w, modes[i].h, 32, &lfb_addr)) {
                uint32_t pitch = (uint32_t)modes[i].w * 4;
                return _graphics_finish_init(modes[i].w, modes[i].h,
                                             32, pitch,
                                             (uint32_t *)(uintptr_t)lfb_addr);
            }
        }
    }

    /* ---- Nothing worked ----------------------------------------- */
    return -1;
}

void graphics_flip(void) {
    if (!g_ctx.ready) return;
    if (g_ctx.framebuffer == g_ctx.hw_framebuffer) return;

    /* Always convert pixel-by-pixel so _hw_put handles byte ordering.
     * For 32bpp this is still fast — one store per pixel, no branches
     * in the common case (compiler inlines the switch).               */
    for (uint32_t y = 0; y < g_ctx.height; y++) {
        uint32_t *src = g_ctx.framebuffer + (size_t)y * g_ctx.pitch_pixels;
        for (uint32_t x = 0; x < g_ctx.width; x++)
            _hw_put(x, y, src[x]);
    }
}

void graphics_shutdown(void) {
    if (!g_ctx.ready) return;
    if (g_ctx.framebuffer && g_ctx.framebuffer != g_ctx.hw_framebuffer)
        kfree(g_ctx.framebuffer);
    g_ctx.framebuffer = NULL;
    g_ctx.hw_framebuffer = NULL;
    g_ctx.ready = 0;
}

int graphics_ready(void) { return g_ctx.ready; }
gfx_ctx_t *graphics_ctx(void) { return &g_ctx; }
uint32_t graphics_width(void)  { return g_ctx.width;  }
uint32_t graphics_height(void) { return g_ctx.height; }

/* =========================================================
 * Internal helpers
 * ========================================================= */
static inline void _put(uint32_t x, uint32_t y, uint32_t c) {
    g_ctx.framebuffer[y * g_ctx.pitch_pixels + x] = c;
}

/* =========================================================
 * Primitives
 * ========================================================= */
void gfx_clear(uint32_t color) {
    if (!g_ctx.ready) return;
    for (uint32_t y = 0; y < g_ctx.height; y++) {
        uint32_t *row = g_ctx.framebuffer + y * g_ctx.pitch_pixels;
        for (uint32_t x = 0; x < g_ctx.width; x++) row[x] = color;
    }
}

void gfx_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!g_ctx.ready || x >= g_ctx.width || y >= g_ctx.height) return;
    _put(x, y, color);
}

void gfx_hline(uint32_t x, uint32_t y, uint32_t w, uint32_t color) {
    if (!g_ctx.ready || y >= g_ctx.height) return;
    if (x >= g_ctx.width) return;
    if (x + w > g_ctx.width) w = g_ctx.width - x;
    uint32_t *row = g_ctx.framebuffer + y * g_ctx.pitch_pixels + x;
    for (uint32_t i = 0; i < w; i++) row[i] = color;
}

void gfx_vline(uint32_t x, uint32_t y, uint32_t h, uint32_t color) {
    if (!g_ctx.ready || x >= g_ctx.width) return;
    if (y + h > g_ctx.height) h = g_ctx.height - y;
    for (uint32_t i = 0; i < h; i++)
        _put(x, y + i, color);
}

void gfx_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    if (!g_ctx.ready) return;
    int32_t dx = (x1>x0)?(x1-x0):(x0-x1);
    int32_t dy = (y1>y0)?(y1-y0):(y0-y1);
    int32_t sx = x0<x1?1:-1, sy = y0<y1?1:-1;
    int32_t err = dx-dy;
    while (1) {
        if (x0>=0 && y0>=0 && (uint32_t)x0<g_ctx.width && (uint32_t)y0<g_ctx.height)
            _put((uint32_t)x0,(uint32_t)y0,color);
        if (x0==x1 && y0==y1) break;
        int32_t e2=2*err;
        if (e2>-dy){err-=dy;x0+=sx;}
        if (e2< dx){err+=dx;y0+=sy;}
    }
}

void gfx_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!g_ctx.ready) return;
    gfx_hline(x,   y,     w, color);
    gfx_hline(x,   y+h-1, w, color);
    gfx_vline(x,   y,     h, color);
    gfx_vline(x+w-1,y,    h, color);
}

void gfx_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!g_ctx.ready) return;
    if (x >= g_ctx.width || y >= g_ctx.height) return;
    if (x + w > g_ctx.width)  w = g_ctx.width  - x;
    if (y + h > g_ctx.height) h = g_ctx.height - y;
    for (uint32_t row = 0; row < h; row++)
        gfx_hline(x, y+row, w, color);
}

void gfx_fill_rect_blend(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                          uint32_t color, uint8_t alpha) {
    if (!g_ctx.ready) return;
    if (alpha == 255) { gfx_fill_rect(x,y,w,h,color); return; }
    if (x >= g_ctx.width || y >= g_ctx.height) return;
    if (x+w > g_ctx.width)  w = g_ctx.width  - x;
    if (y+h > g_ctx.height) h = g_ctx.height - y;
    uint8_t sr=(color>>16)&0xFF, sg=(color>>8)&0xFF, sb=color&0xFF;
    uint32_t a = alpha, ia = 255-alpha;
    for (uint32_t py = y; py < y+h; py++) {
        for (uint32_t px = x; px < x+w; px++) {
            uint32_t dst = g_ctx.framebuffer[py*g_ctx.pitch_pixels+px];
            uint8_t dr=(dst>>16)&0xFF,dg=(dst>>8)&0xFF,db=dst&0xFF;
            uint8_t r=(uint8_t)((sr*a+dr*ia)/255);
            uint8_t g2=(uint8_t)((sg*a+dg*ia)/255);
            uint8_t b=(uint8_t)((sb*a+db*ia)/255);
            _put(px,py,GFX_RGB(r,g2,b));
        }
    }
}

void gfx_circle(uint32_t cx, uint32_t cy, uint32_t r, uint32_t color) {
    if (!g_ctx.ready) return;
    int32_t f=1-(int32_t)r, dx=1, dy=-2*(int32_t)r, px=0, py=(int32_t)r;
    /* 4 cardinal points */
    gfx_pixel(cx,   cy+r, color); gfx_pixel(cx,   cy-r, color);
    gfx_pixel(cx+r, cy,   color); gfx_pixel(cx-r, cy,   color);
    while (px<py) {
        if (f>=0){py--;dy+=2;f+=dy;}
        px++;dx+=2;f+=dx;
        gfx_pixel(cx+px,cy+py,color); gfx_pixel(cx-px,cy+py,color);
        gfx_pixel(cx+px,cy-py,color); gfx_pixel(cx-px,cy-py,color);
        gfx_pixel(cx+py,cy+px,color); gfx_pixel(cx-py,cy+px,color);
        gfx_pixel(cx+py,cy-px,color); gfx_pixel(cx-py,cy-px,color);
    }
}

void gfx_fill_circle(uint32_t cx, uint32_t cy, uint32_t r, uint32_t color) {
    if (!g_ctx.ready) return;
    int32_t f=1-(int32_t)r, dx=1, dy=-2*(int32_t)r, px=0, py=(int32_t)r;
    gfx_hline(cx-r, cy, 2*r+1, color);
    while (px<py) {
        if (f>=0){py--;dy+=2;f+=dy;}
        px++;dx+=2;f+=dx;
        gfx_hline(cx-px, cy+py, 2*px+1, color);
        gfx_hline(cx-px, cy-py, 2*px+1, color);
        gfx_hline(cx-py, cy+px, 2*py+1, color);
        gfx_hline(cx-py, cy-px, 2*py+1, color);
    }
}

/* Rounded rectangle helpers */
void gfx_rounded_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                       uint32_t rad, uint32_t color) {
    if (!g_ctx.ready) return;
    if (rad*2 > w) rad = w/2;
    if (rad*2 > h) rad = h/2;
    /* straight edges */
    gfx_hline(x+rad, y,     w-2*rad, color);
    gfx_hline(x+rad, y+h-1, w-2*rad, color);
    gfx_vline(x,     y+rad, h-2*rad, color);
    gfx_vline(x+w-1, y+rad, h-2*rad, color);
    /* corners via midpoint circle, only 1 quadrant each */
    int32_t f=1-(int32_t)rad, dx=1, dy=-2*(int32_t)rad, px=0, py=(int32_t)rad;
    uint32_t cx1=x+rad, cy1=y+rad, cx2=x+w-1-rad, cy2=y+h-1-rad;
    while (px<py) {
        if (f>=0){py--;dy+=2;f+=dy;}
        px++;dx+=2;f+=dx;
        gfx_pixel(cx2+(uint32_t)px, cy1-(uint32_t)py, color);
        gfx_pixel(cx2+(uint32_t)py, cy1-(uint32_t)px, color);
        gfx_pixel(cx1-(uint32_t)px, cy1-(uint32_t)py, color);
        gfx_pixel(cx1-(uint32_t)py, cy1-(uint32_t)px, color);
        gfx_pixel(cx2+(uint32_t)px, cy2+(uint32_t)py, color);
        gfx_pixel(cx2+(uint32_t)py, cy2+(uint32_t)px, color);
        gfx_pixel(cx1-(uint32_t)px, cy2+(uint32_t)py, color);
        gfx_pixel(cx1-(uint32_t)py, cy2+(uint32_t)px, color);
    }
}

void gfx_fill_rounded_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                            uint32_t rad, uint32_t fill, uint32_t border) {
    if (!g_ctx.ready) return;
    if (rad*2 > w) rad = w/2;
    if (rad*2 > h) rad = h/2;
    /* fill middle bands */
    gfx_fill_rect(x, y+rad, w, h-2*rad, fill);
    /* fill top/bottom bands between corner arcs */
    int32_t f=1-(int32_t)rad, dx=1, dy=-2*(int32_t)rad, px=0, py=(int32_t)rad;
    uint32_t cx1=x+rad, cx2=x+w-1-rad, cy1=y+rad, cy2=y+h-1-rad;
    while (px<py) {
        if (f>=0){py--;dy+=2;f+=dy;}
        px++;dx+=2;f+=dx;
        /* top band */
        gfx_hline(cx1-(uint32_t)px, cy1-(uint32_t)py,
                  (uint32_t)(cx2-cx1)+2*(uint32_t)px+1, fill);
        gfx_hline(cx1-(uint32_t)py, cy1-(uint32_t)px,
                  (uint32_t)(cx2-cx1)+2*(uint32_t)py+1, fill);
        /* bottom band */
        gfx_hline(cx1-(uint32_t)px, cy2+(uint32_t)py,
                  (uint32_t)(cx2-cx1)+2*(uint32_t)px+1, fill);
        gfx_hline(cx1-(uint32_t)py, cy2+(uint32_t)px,
                  (uint32_t)(cx2-cx1)+2*(uint32_t)py+1, fill);
    }
    gfx_rounded_rect(x,y,w,h,rad,border);
}

/* =========================================================
 * Text
 * ========================================================= */
void gfx_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg) {
    if (!g_ctx.ready) return;
    unsigned char uc = (unsigned char)c;
    if (uc < 32 || uc > 127) uc = '?';
    const uint8_t *glyph = font8x16[uc - 32];
    for (uint32_t row = 0; row < GFX_FONT_H; row++) {
        uint32_t py = y + row;
        if (py >= g_ctx.height) break;
        uint8_t bits = glyph[row];
        for (uint32_t col = 0; col < GFX_FONT_W; col++) {
            uint32_t px = x + col;
            if (px >= g_ctx.width) break;
            /* Font glyphs are LSB-first: bit 0 = leftmost pixel */
            if (bits & (1u << col)) {
                _put(px, py, fg);
            } else if (bg != GFX_TRANSPARENT) {
                _put(px, py, bg);
            }
        }
    }
}

uint32_t gfx_string(uint32_t x, uint32_t y, const char *s,
                     uint32_t fg, uint32_t bg) {
    if (!g_ctx.ready || !s) return x;
    uint32_t cx = x;
    while (*s) {
        if (*s == '\n') { cx = x; y += GFX_FONT_H; }
        else { gfx_char(cx, y, *s, fg, bg); cx += GFX_FONT_W; }
        s++;
    }
    return cx;
}

void gfx_string_clipped(uint32_t x, uint32_t y, const char *s,
                         uint32_t fg, uint32_t bg,
                         uint32_t cx, uint32_t cy, uint32_t cw, uint32_t ch) {
    if (!g_ctx.ready || !s) return;
    uint32_t px = x;
    while (*s) {
        if (px + GFX_FONT_W > cx + cw) break;
        if (px >= cx && y >= cy && y + GFX_FONT_H <= cy + ch)
            gfx_char(px, y, *s, fg, bg);
        px += GFX_FONT_W;
        s++;
    }
}

void gfx_string_centered(uint32_t rx, uint32_t ry, uint32_t rw, uint32_t rh,
                          const char *s, uint32_t fg, uint32_t bg) {
    if (!s) return;
    uint32_t tw = gfx_string_width(s);
    uint32_t tx = rx + (rw > tw ? (rw-tw)/2 : 0);
    uint32_t ty = ry + (rh > GFX_FONT_H ? (rh-GFX_FONT_H)/2 : 0);
    gfx_string(tx, ty, s, fg, bg);
}

uint32_t gfx_string_width(const char *s) {
    if (!s) return 0;
    uint32_t n = 0;
    while (*s++) n++;
    return n * GFX_FONT_W;
}

/* =========================================================
 * Taskbar
 * ========================================================= */
void gfx_taskbar_draw(const char *title, const char *clock_str) {
    if (!g_ctx.ready) return;
    uint32_t y = g_ctx.height - GFX_TASKBAR_H;
    /* Background with subtle top border */
    gfx_fill_rect(0, y, g_ctx.width, GFX_TASKBAR_H, GFX_TASKBAR_BG);
    gfx_hline(0, y, g_ctx.width, GFX_RGB(60,80,120));
    /* Title / start label */
    if (title && title[0]) {
        gfx_fill_rounded_rect(4, y+3, 80, GFX_TASKBAR_H-6, 4,
                              GFX_WIN_TITLE_BG, GFX_WIN_TITLE_BG);
        gfx_string_centered(4, y+3, 80, GFX_TASKBAR_H-6,
                            title, GFX_WHITE, GFX_TRANSPARENT);
    }
    /* Clock on the right */
    if (clock_str && clock_str[0]) {
        uint32_t cw = gfx_string_width(clock_str) + 8;
        uint32_t cx2 = g_ctx.width - cw - 4;
        gfx_string_centered(cx2, y, cw, GFX_TASKBAR_H,
                            clock_str, GFX_LIGHT_GREY, GFX_TRANSPARENT);
    }
}

/* =========================================================
 * Button
 * ========================================================= */
void gfx_button_init(gfx_button_t *btn, uint32_t x, uint32_t y,
                     uint32_t w, uint32_t h, const char *label) {
    if (!btn) return;
    btn->x=x; btn->y=y; btn->w=w; btn->h=h;
    btn->pressed=0; btn->hovered=0; btn->enabled=1;
    btn->label[0]='\0';
    if (label) {
        size_t i=0;
        while (label[i] && i < sizeof(btn->label)-1)
            {btn->label[i]=label[i];i++;}
        btn->label[i]='\0';
    }
}

void gfx_button_draw(const gfx_button_t *btn) {
    if (!btn || !g_ctx.ready) return;
    uint32_t bg = btn->pressed  ? GFX_BTN_PRESS :
                  btn->hovered  ? GFX_BTN_HOVER  : GFX_BTN_BG;
    uint32_t fg = btn->enabled  ? GFX_BTN_FG : GFX_GREY;
    if (!btn->enabled) bg = GFX_LIGHT_GREY;
    gfx_fill_rounded_rect(btn->x, btn->y, btn->w, btn->h, 4, bg, GFX_BTN_BORDER);
    /* Highlight top edge (3D effect) */
    if (!btn->pressed)
        gfx_hline(btn->x+2, btn->y+1, btn->w-4, GFX_WHITE);
    gfx_string_centered(btn->x, btn->y, btn->w, btn->h, btn->label, fg, GFX_TRANSPARENT);
}

int gfx_button_hit(const gfx_button_t *btn, uint32_t mx, uint32_t my) {
    if (!btn) return 0;
    return mx>=btn->x && mx<btn->x+btn->w &&
           my>=btn->y && my<btn->y+btn->h;
}

/* =========================================================
 * Window
 * ========================================================= */
void gfx_window_init(gfx_window_t *win, uint32_t x, uint32_t y,
                     uint32_t w, uint32_t h, const char *title, uint32_t flags) {
    if (!win) return;
    win->x=x; win->y=y; win->w=w; win->h=h;
    win->visible=1; win->focused=0;
    win->dragging=0; win->drag_ox=0; win->drag_oy=0;
    win->flags=flags;
    win->close_w=14; win->close_h=14;
    win->title[0]='\0';
    if (title) {
        size_t i=0;
        while(title[i] && i<sizeof(win->title)-1){win->title[i]=title[i];i++;}
        win->title[i]='\0';
    }
}

void gfx_window_draw(const gfx_window_t *win) {
    if (!win || !win->visible || !g_ctx.ready) return;

    uint32_t tbg = win->focused ? GFX_WIN_TITLE_BG : GFX_WIN_TITLE_INACT;

    /* Shadow (2px offset, dark semi-transparent) */
    gfx_fill_rect_blend(win->x+4, win->y+4, win->w, win->h, GFX_BLACK, 60);

    /* Window body */
    gfx_fill_rect(win->x, win->y+GFX_WIN_TITLE_H, win->w,
                  win->h-GFX_WIN_TITLE_H, GFX_WIN_BODY_BG);

    /* Title bar */
    gfx_fill_rounded_rect(win->x, win->y, win->w, GFX_WIN_TITLE_H, 4,
                           tbg, GFX_WIN_BORDER);
    /* Flatten bottom of title bar corners */
    gfx_fill_rect(win->x, win->y+GFX_WIN_TITLE_H-4, win->w, 4, tbg);

    /* Border */
    gfx_rect(win->x, win->y, win->w, win->h, GFX_WIN_BORDER);

    /* Title text */
    gfx_string_clipped(win->x+8, win->y+(GFX_WIN_TITLE_H-GFX_FONT_H)/2,
                       win->title, GFX_WIN_TITLE_FG, GFX_TRANSPARENT,
                       win->x+8, win->y, win->w-30, GFX_WIN_TITLE_H);

    /* Close button (X) — top right */
    uint32_t cx2 = win->x + win->w - win->close_w - 4;
    uint32_t cy2 = win->y + (GFX_WIN_TITLE_H - win->close_h)/2;
    /* Store for hit-test */
    ((gfx_window_t*)win)->close_x = cx2;
    ((gfx_window_t*)win)->close_y = cy2;
    gfx_fill_rounded_rect(cx2, cy2, win->close_w, win->close_h, 3,
                           GFX_RGB(200,60,60), GFX_RGB(180,40,40));
    /* X mark */
    gfx_line((int32_t)cx2+3,        (int32_t)cy2+3,
              (int32_t)cx2+(int32_t)win->close_w-4,
              (int32_t)cy2+(int32_t)win->close_h-4, GFX_WHITE);
    gfx_line((int32_t)cx2+(int32_t)win->close_w-4, (int32_t)cy2+3,
              (int32_t)cx2+3,
              (int32_t)cy2+(int32_t)win->close_h-4, GFX_WHITE);
}

uint32_t gfx_window_client_x(const gfx_window_t *w){return w->x+1;}
uint32_t gfx_window_client_y(const gfx_window_t *w){return w->y+GFX_WIN_TITLE_H;}
uint32_t gfx_window_client_w(const gfx_window_t *w){return w->w-2;}
uint32_t gfx_window_client_h(const gfx_window_t *w){return w->h-GFX_WIN_TITLE_H-1;}

int gfx_window_hit_titlebar(const gfx_window_t *w, uint32_t mx, uint32_t my) {
    return mx>=w->x && mx<w->x+w->w &&
           my>=w->y && my<w->y+GFX_WIN_TITLE_H &&
           !gfx_window_hit_close(w, mx, my);
}
int gfx_window_hit_close(const gfx_window_t *w, uint32_t mx, uint32_t my) {
    return mx>=w->close_x && mx<w->close_x+w->close_w &&
           my>=w->close_y && my<w->close_y+w->close_h;
}

int gfx_window_mouse_down(gfx_window_t *w, uint32_t mx, uint32_t my) {
    if (!w || !w->visible) return 0;
    if (gfx_window_hit_titlebar(w,mx,my)) {
        w->dragging=1;
        w->drag_ox=(int32_t)mx-(int32_t)w->x;
        w->drag_oy=(int32_t)my-(int32_t)w->y;
        return 1;
    }
    if (mx>=w->x && mx<w->x+w->w && my>=w->y && my<w->y+w->h) return 1;
    return 0;
}
int gfx_window_mouse_up(gfx_window_t *w, uint32_t mx, uint32_t my) {
    (void)mx;(void)my;
    if (!w) return 0;
    w->dragging=0;
    return 0;
}
int gfx_window_mouse_move(gfx_window_t *w, uint32_t mx, uint32_t my) {
    if (!w || !w->dragging) return 0;
    int32_t nx=(int32_t)mx-w->drag_ox;
    int32_t ny=(int32_t)my-w->drag_oy;
    if (nx<0) nx=0;
    if (ny<0) ny=0;
    if ((uint32_t)nx+w->w > g_ctx.width)  nx=(int32_t)(g_ctx.width-w->w);
    if ((uint32_t)ny+w->h > g_ctx.height) ny=(int32_t)(g_ctx.height-w->h);
    w->x=(uint32_t)nx; w->y=(uint32_t)ny;
    return 1;
}

/* =========================================================
 * Mouse cursor (arrow shape)
 * ========================================================= */
static const uint16_t cursor_shape[16] = {
    0x8000,0xC000,0xE000,0xF000,
    0xF800,0xFC00,0xFE00,0xFF00,
    0xFF80,0xFFC0,0xF800,0xD800,
    0x8C00,0x0C00,0x0600,0x0600,
};
static const uint16_t cursor_mask[16] = {
    0x8000,0xC000,0xE000,0xF000,
    0xF800,0xFC00,0xFE00,0xFF00,
    0xFF80,0xFFC0,0xFC00,0xFC00,
    0xCE00,0x0E00,0x0600,0x0600,
};

void gfx_cursor_draw(uint32_t mx, uint32_t my) {
    if (!g_ctx.ready) return;
    for (uint32_t row = 0; row < 16; row++) {
        uint32_t py = my + row;
        if (py >= g_ctx.height) break;
        for (uint32_t col = 0; col < 11; col++) {
            uint32_t px = mx + col;
            if (px >= g_ctx.width) break;
            uint16_t bit = (uint16_t)(0x8000u >> col);
            if (cursor_shape[row] & bit)
                _put(px, py, GFX_WHITE);
            else if (cursor_mask[row] & bit)
                _put(px, py, GFX_BLACK);
        }
    }
}

/* =========================================================
 * Desktop helpers
 * ========================================================= */
void gfx_desktop_begin(const char *clock_str) {
    if (!g_ctx.ready) return;
    /* Background gradient: top dark, bottom slightly lighter */
    uint32_t top = GFX_RGB(20,35,60), bot = GFX_RGB(40,65,110);
    uint32_t taskbar_y = g_ctx.height - GFX_TASKBAR_H;
    for (uint32_t y = 0; y < taskbar_y; y++) {
        uint32_t r = ((top>>16&0xFF)*(taskbar_y-y) + (bot>>16&0xFF)*y)/taskbar_y;
        uint32_t g2= ((top>>8 &0xFF)*(taskbar_y-y) + (bot>>8 &0xFF)*y)/taskbar_y;
        uint32_t b = ((top    &0xFF)*(taskbar_y-y) + (bot    &0xFF)*y)/taskbar_y;
        gfx_hline(0, y, g_ctx.width, GFX_RGB(r,g2,b));
    }
    gfx_taskbar_draw("MyOs", clock_str);
}

/* =========================================================
 * GUI event loop — полноценный интерактивный рабочий стол
 *
 * Вызывается из shell командой "gui".
 * Выход: нажать Esc или закрыть все окна.
 *
 * Зависимости (включены через graphics.h → kernel):
 *   mouse_state_t get_mouse_state(void)
 *   int keyboard_try_read_char_extended(uint16_t *out)
 *   pit_seconds()
 * ========================================================= */

/* Максимум окон в сессии */
#define GUI_MAX_WINDOWS  8

typedef struct {
    gfx_window_t win;
    gfx_button_t buttons[4];
    int          btn_count;
    /* callback index (что делает окно) */
    int          kind;   /* 0=about, 1=color, 2=text */
    /* для текстового окна */
    char         text[128];
    size_t       text_len;
} gui_app_t;

/* Нарисовать содержимое одного окна */
static void gui_draw_app(gui_app_t *app) {
    if (!app->win.visible) return;
    gfx_window_draw(&app->win);

    uint32_t cx = gfx_window_client_x(&app->win);
    uint32_t cy = gfx_window_client_y(&app->win);
    uint32_t cw = gfx_window_client_w(&app->win);
    uint32_t ch = gfx_window_client_h(&app->win);

    /* Заливка клиентской области фоном */
    gfx_fill_rect(cx, cy, cw, ch, GFX_WIN_BODY_BG);

    switch (app->kind) {

    case 0: { /* About */
        gfx_string(cx+12, cy+12, "MyOs v1.2", GFX_RGB(30,70,160), GFX_TRANSPARENT);
        gfx_hline(cx+8, cy+30, cw-16, GFX_MID_GREY);
        gfx_string(cx+12, cy+40, "Graphics Engine v2", GFX_BLACK, GFX_TRANSPARENT);
        gfx_string(cx+12, cy+58, "800x600 @ 32bpp", GFX_GREY, GFX_TRANSPARENT);
        gfx_string(cx+12, cy+76, "Bochs VBE + PCI LFB", GFX_GREY, GFX_TRANSPARENT);
        gfx_fill_rounded_rect(cx+12, cy+100, 60, 60, 8,
                              GFX_RGB(30,50,80), GFX_RGB(20,40,70));
        gfx_string_centered(cx+12, cy+100, 60, 60, "My\nOs",
                            GFX_WHITE, GFX_TRANSPARENT);
        break;
    }

    case 1: { /* Color picker */
        gfx_string(cx+12, cy+12, "Color palette:", GFX_BLACK, GFX_TRANSPARENT);
        uint32_t colors[] = {
            GFX_RED, GFX_GREEN, GFX_BLUE, GFX_YELLOW,
            GFX_CYAN, GFX_MAGENTA, GFX_ORANGE, GFX_WHITE,
            GFX_GREY, GFX_DARK_GREY, GFX_RGB(128,0,0), GFX_RGB(0,128,0),
        };
        for (int i = 0; i < 12; i++) {
            uint32_t px = (uint32_t)(cx + 12 + (i % 4) * 44);
            uint32_t py = (uint32_t)(cy + 34 + (i / 4) * 44);
            gfx_fill_rounded_rect(px, py, 36, 36, 5, colors[i], GFX_DARK_GREY);
        }
        break;
    }

    case 2: { /* Notepad */
        gfx_fill_rect(cx+8, cy+8, cw-16, ch-50, GFX_WHITE);
        gfx_rect(cx+8, cy+8, cw-16, ch-50, GFX_MID_GREY);
        /* Линии как в блокноте */
        for (uint32_t l = 1; l < (ch-50)/GFX_FONT_H; l++)
            gfx_hline(cx+9, cy+8+l*GFX_FONT_H, cw-18,
                      GFX_RGB(220,230,245));
        /* Текст */
        gfx_string_clipped(cx+10, cy+10, app->text,
                           GFX_BLACK, GFX_TRANSPARENT,
                           cx+9, cy+9, cw-18, ch-52);
        /* Курсор */
        uint32_t cur_x = cx+10 + (uint32_t)(app->text_len % ((cw-20)/GFX_FONT_W)) * GFX_FONT_W;
        uint32_t cur_y = cy+10 + (uint32_t)(app->text_len / ((cw-20)/GFX_FONT_W)) * GFX_FONT_H;
        gfx_vline(cur_x, cur_y, GFX_FONT_H, GFX_BLACK);
        break;
    }

    } /* switch */

    /* Кнопки */
    for (int i = 0; i < app->btn_count; i++)
        gfx_button_draw(&app->buttons[i]);
}

/* Форматирование времени из pit_seconds() */
static void gui_format_time(char *buf, size_t bufsz, uint64_t secs) {
    uint64_t h = (secs / 3600) % 24;
    uint64_t m = (secs / 60) % 60;
    uint64_t s = secs % 60;
    /* простой snprintf без libc */
    if (bufsz < 9) return;
    buf[0] = (char)('0' + h/10); buf[1] = (char)('0' + h%10);
    buf[2] = ':';
    buf[3] = (char)('0' + m/10); buf[4] = (char)('0' + m%10);
    buf[5] = ':';
    buf[6] = (char)('0' + s/10); buf[7] = (char)('0' + s%10);
    buf[8] = '\0';
}

/* Добавить символ в текст нотпада */
static void gui_notepad_putc(gui_app_t *app, char c) {
    if (app->text_len + 1 >= sizeof(app->text) - 1) return;
    app->text[app->text_len++] = c;
    app->text[app->text_len]   = '\0';
}
static void gui_notepad_backspace(gui_app_t *app) {
    if (app->text_len > 0) app->text[--app->text_len] = '\0';
}

/* Открыть новое окно */
static void gui_open_window(gui_app_t *apps, int *count, int kind,
                             const char *title,
                             uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (*count >= GUI_MAX_WINDOWS) return;
    gui_app_t *a = &apps[*count];
    memset(a, 0, sizeof(*a));
    gfx_window_init(&a->win, x, y, w, h, title, 0);
    a->win.focused = 1;
    a->kind = kind;

    /* Кнопка Close внизу */
    uint32_t cx = gfx_window_client_x(&a->win);
    uint32_t cy = gfx_window_client_y(&a->win);
    uint32_t ch = gfx_window_client_h(&a->win);
    gfx_button_init(&a->buttons[0], cx+8, cy+ch-32, 80, 26, "Close");
    a->btn_count = 1;

    (*count)++;
}

/* Снять фокус со всех, дать одному */
static void gui_focus(gui_app_t *apps, int count, int idx) {
    for (int i = 0; i < count; i++)
        apps[i].win.focused = (i == idx);
}

/* Найти окно под курсором (верхнее = последнее в массиве) */
static int gui_hit_window(gui_app_t *apps, int count, uint32_t mx, uint32_t my) {
    for (int i = count - 1; i >= 0; i--) {
        gfx_window_t *w = &apps[i].win;
        if (!w->visible) continue;
        if (mx >= w->x && mx < w->x+w->w &&
            my >= w->y && my < w->y+w->h)
            return i;
    }
    return -1;
}

/* Переместить окно в конец массива (поверх остальных) */
static void gui_bring_to_front(gui_app_t *apps, int *count, int idx) {
    if (idx < 0 || idx >= *count - 1) return;
    gui_app_t tmp = apps[idx];
    for (int i = idx; i < *count - 1; i++) apps[i] = apps[i+1];
    apps[*count - 1] = tmp;
}

/* Нарисовать кнопки taskbar (стартовые апплеты) */
static void gui_draw_taskbar_apps(uint32_t scr_h) {
    uint32_t y = scr_h - GFX_TASKBAR_H + 3;
    struct { const char *label; uint32_t x; } tbtn[] = {
        {"About",    90},
        {"Palette", 175},
        {"Notepad", 265},
    };
    for (int i = 0; i < 3; i++) {
        gfx_button_t b;
        gfx_button_init(&b, tbtn[i].x, y, 80, GFX_TASKBAR_H-6, tbtn[i].label);
        gfx_button_draw(&b);
    }
}

/* Проверить клик по кнопкам таскбара */
static int gui_taskbar_click(uint32_t mx, uint32_t my, uint32_t scr_h) {
    uint32_t y = scr_h - GFX_TASKBAR_H + 3;
    struct { uint32_t x; } tbtn[] = {{90},{175},{265}};
    for (int i = 0; i < 3; i++) {
        if (mx >= tbtn[i].x && mx < tbtn[i].x+80 &&
            my >= y && my < y + GFX_TASKBAR_H-6)
            return i;
    }
    return -1;
}

void gui_run(void) {
    if (!g_ctx.ready) return;

    /* ВАЖНО: apps[] должен быть static — иначе ~4 КБ на стеке
     * при стеке потока 4096 байт → переполнение → page fault.    */
    static gui_app_t apps[GUI_MAX_WINDOWS];
    static int app_count;
    static int prev_lbtn;
    static int drag_win;
    static int focus_win;
    static int active_notepad;
    static uint32_t mx, my;
    static char clock_buf[16];

    /* Инициализация при каждом входе */
    memset(apps, 0, sizeof(apps));
    app_count     = 0;
    prev_lbtn     = 0;
    drag_win      = -1;
    focus_win     = 0;
    active_notepad = -1;
    mx = g_ctx.width  / 2;
    my = g_ctx.height / 2;

    /* Открыть About по умолчанию */
    gui_open_window(apps, &app_count, 0, "About MyOs",
                    80, 60, 340, 210);

    while (1) {
        /* --- Мышь --- */
        mouse_state_t ms = get_mouse_state();

        /* col/row — позиция в символьных клетках (0..79, 0..24).
         * Масштабируем в пиксели пропорционально разрешению.    */
        {
            size_t cols = 80, rows = 25;
            terminal_get_size(&cols, &rows);
            uint32_t new_mx = (uint32_t)ms.col * g_ctx.width  / (uint32_t)cols;
            uint32_t new_my = (uint32_t)ms.row * g_ctx.height / (uint32_t)rows;
            if (new_mx >= g_ctx.width)  new_mx = g_ctx.width  - 1;
            if (new_my >= g_ctx.height) new_my = g_ctx.height - 1;
            mx = new_mx;
            my = new_my;
        }

        int lbtn      = (ms.buttons & 0x01) ? 1 : 0;
        int lbtn_down = lbtn  && !prev_lbtn;
        int lbtn_up   = !lbtn && prev_lbtn;

        /* --- Клавиатура --- */
        uint16_t key = 0;
        if (keyboard_try_read_char_extended(&key)) {
            /* Esc: ASCII 0x1B или специальный код > 0xFF */
            if (key == 27 || key == 0x1B) break;

            if (active_notepad >= 0 && active_notepad < app_count) {
                gui_app_t *np = &apps[active_notepad];
                if (np->win.visible && np->kind == 2) {
                    if (key == '\b' || key == 127)
                        gui_notepad_backspace(np);
                    else if (key == '\n' || key == '\r')
                        gui_notepad_putc(np, '\n');
                    else if (key >= 32 && key < 256)
                        gui_notepad_putc(np, (char)key);
                }
            }
        }

        /* --- Клики --- */
        if (lbtn_down) {
            int tbtn = gui_taskbar_click(mx, my, g_ctx.height);
            if (tbtn >= 0) {
                const char *titles[] = {"About MyOs","Color Palette","Notepad"};
                uint32_t ws[] = {340, 300, 400};
                uint32_t hs[] = {210, 220, 280};
                uint32_t stagger = (uint32_t)app_count * 22;
                if (app_count < GUI_MAX_WINDOWS)
                    gui_open_window(apps, &app_count, tbtn, titles[tbtn],
                                    60+stagger, 50+stagger, ws[tbtn], hs[tbtn]);
                focus_win = app_count - 1;
                gui_focus(apps, app_count, focus_win);
                if (tbtn == 2) active_notepad = focus_win;
            } else {
                int hit = gui_hit_window(apps, app_count, mx, my);
                if (hit >= 0) {
                    if (gfx_window_hit_close(&apps[hit].win, mx, my)) {
                        if (active_notepad == hit) active_notepad = -1;
                        apps[hit].win.visible = 0;
                    } else {
                        gui_bring_to_front(apps, &app_count, hit);
                        focus_win = app_count - 1;
                        gui_focus(apps, app_count, focus_win);
                        if (apps[focus_win].kind == 2)
                            active_notepad = focus_win;

                        if (gfx_window_hit_titlebar(&apps[focus_win].win, mx, my)) {
                            drag_win = focus_win;
                            gfx_window_mouse_down(&apps[drag_win].win, mx, my);
                        }

                        gui_app_t *a = &apps[focus_win];
                        for (int bi = 0; bi < a->btn_count; bi++) {
                            if (gfx_button_hit(&a->buttons[bi], mx, my)) {
                                a->buttons[bi].pressed = 1;
                                if (bi == 0) {
                                    a->win.visible = 0;
                                    if (active_notepad == focus_win)
                                        active_notepad = -1;
                                }
                            }
                        }
                    }
                }
            }
        }

        if (lbtn_up) {
            if (drag_win >= 0) {
                gfx_window_mouse_up(&apps[drag_win].win, mx, my);
                drag_win = -1;
            }
            for (int i = 0; i < app_count; i++)
                for (int bi = 0; bi < apps[i].btn_count; bi++)
                    apps[i].buttons[bi].pressed = 0;
        }

        if (lbtn && drag_win >= 0)
            gfx_window_mouse_move(&apps[drag_win].win, mx, my);

        /* Hover */
        for (int i = 0; i < app_count; i++)
            for (int bi = 0; bi < apps[i].btn_count; bi++)
                apps[i].buttons[bi].hovered =
                    gfx_button_hit(&apps[i].buttons[bi], mx, my);

        prev_lbtn = lbtn;

        /* --- Рендер --- */
        gui_format_time(clock_buf, sizeof(clock_buf), pit_seconds());
        gfx_desktop_begin(clock_buf);
        gui_draw_taskbar_apps(g_ctx.height);

        for (int i = 0; i < app_count; i++)
            gui_draw_app(&apps[i]);

        gfx_cursor_draw(mx, my);
        graphics_flip();

        __asm__ volatile("pause");
    }
}

void graphics_demo(void) {
    if (!g_ctx.ready) return;

    /* Demo window */
    gfx_window_t win;
    gfx_window_init(&win, 80, 60, 400, 260, "Graphics Demo", 0);
    win.focused = 1;
    gfx_window_draw(&win);

    uint32_t cx2 = gfx_window_client_x(&win);
    uint32_t cy2 = gfx_window_client_y(&win);
    uint32_t cw  = gfx_window_client_w(&win);

    gfx_string(cx2+8, cy2+8,  "Primitives:", GFX_BLACK, GFX_TRANSPARENT);
    gfx_fill_circle(cx2+30,  cy2+55,  20, GFX_RED);
    gfx_fill_circle(cx2+90,  cy2+55,  20, GFX_GREEN);
    gfx_fill_circle(cx2+150, cy2+55,  20, GFX_BLUE);
    gfx_fill_rounded_rect(cx2+10, cy2+90, cw-20, 30, 6, GFX_RGB(80,140,220), GFX_RGB(50,100,180));
    gfx_string_centered(cx2+10, cy2+90, cw-20, 30, "Rounded rect", GFX_WHITE, GFX_TRANSPARENT);

    gfx_button_t btn;
    gfx_button_init(&btn, cx2+10, cy2+140, 110, 26, "OK");
    gfx_button_draw(&btn);
    gfx_button_init(&btn, cx2+130, cy2+140, 110, 26, "Cancel");
    gfx_button_draw(&btn);

    gfx_string(cx2+8, cy2+185, "MyOs Graphics Engine v2", GFX_GREY, GFX_TRANSPARENT);

    gfx_cursor_draw(200, 150);
    graphics_flip();
}
