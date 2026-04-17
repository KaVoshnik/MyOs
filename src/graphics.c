#include <graphics.h>
#include <memory.h>
#include <string.h>
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
 * Module state
 * ========================================================= */
static gfx_ctx_t g_ctx;

/* =========================================================
 * Init / flip
 * ========================================================= */
int graphics_init(void) {
    if (g_ctx.ready) return 0; /* already done */

    /* Require Multiboot framebuffer info (flag bit 12) */
    if (!mb_info || !(mb_info->flags & (1u << 12))) return -1;

    uint64_t fb_addr = mb_info->framebuffer_addr;
    if (!fb_addr) return -2;

    /* Only linear (type=1) or RGB (type=2) framebuffers supported */
    if (mb_info->framebuffer_type != 1 && mb_info->framebuffer_type != 2) return -3;
    if (mb_info->framebuffer_bpp != 32) return -4;

    g_ctx.hw_framebuffer = (uint32_t *)(uintptr_t)fb_addr;
    g_ctx.width          = mb_info->framebuffer_width;
    g_ctx.height         = mb_info->framebuffer_height;
    g_ctx.pitch_pixels   = mb_info->framebuffer_pitch / 4; /* bytes -> words */
    g_ctx.bpp            = mb_info->framebuffer_bpp;

    /* Allocate back-buffer */
    size_t buf_size = (size_t)g_ctx.pitch_pixels * g_ctx.height * 4;
    g_ctx.framebuffer = (uint32_t *)kmalloc(buf_size);
    if (!g_ctx.framebuffer) {
        /* Fallback: write directly to hw (no double-buffering) */
        g_ctx.framebuffer = g_ctx.hw_framebuffer;
    }

    g_ctx.ready = 1;
    gfx_clear(GFX_BLACK);
    graphics_flip();
    return 0;
}

void graphics_flip(void) {
    if (!g_ctx.ready) return;
    if (g_ctx.framebuffer == g_ctx.hw_framebuffer) return; /* direct mode */

    /* Fast row copy */
    size_t row_bytes = (size_t)g_ctx.width * 4;
    for (uint32_t y = 0; y < g_ctx.height; y++) {
        uint32_t *src = g_ctx.framebuffer  + (size_t)y * g_ctx.pitch_pixels;
        uint32_t *dst = g_ctx.hw_framebuffer + (size_t)y * g_ctx.pitch_pixels;
        memcpy(dst, src, row_bytes);
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
            if (bits & (0x80 >> col)) {
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
 * Demo / compat
 * ========================================================= */
void graphics_demo(void) {
    if (!g_ctx.ready) return;
    gfx_desktop_begin("12:00");

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
