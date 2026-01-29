#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include <stddef.h>

/* VBE Information Block */
struct vbe_info_block {
    char signature[4];          /* "VESA" */
    uint16_t version;           /* VBE version */
    uint32_t oem_string_ptr;    /* Far pointer to OEM string */
    uint32_t capabilities;     /* Capabilities */
    uint32_t video_mode_ptr;    /* Far pointer to video mode list */
    uint16_t total_memory;      /* Total memory in 64KB blocks */
    uint16_t oem_software_rev;
    uint32_t oem_vendor_name_ptr;
    uint32_t oem_product_name_ptr;
    uint32_t oem_product_rev_ptr;
    uint8_t reserved[222];
    uint8_t oem_data[256];
} __attribute__((packed));

/* VBE Mode Info Block */
struct vbe_mode_info {
    uint16_t mode_attributes;
    uint8_t win_a_attributes;
    uint8_t win_b_attributes;
    uint16_t win_granularity;
    uint16_t win_size;
    uint16_t win_a_segment;
    uint16_t win_b_segment;
    uint32_t win_func_ptr;
    uint16_t bytes_per_scanline;
    uint16_t x_resolution;
    uint16_t y_resolution;
    uint8_t x_char_size;
    uint8_t y_char_size;
    uint8_t number_of_planes;
    uint8_t bits_per_pixel;
    uint8_t number_of_banks;
    uint8_t memory_model;
    uint8_t bank_size;
    uint8_t number_of_image_pages;
    uint8_t reserved1;
    uint8_t red_mask_size;
    uint8_t red_field_position;
    uint8_t green_mask_size;
    uint8_t green_field_position;
    uint8_t blue_mask_size;
    uint8_t blue_field_position;
    uint8_t reserved_mask_size;
    uint8_t reserved_field_position;
    uint8_t direct_color_mode_info;
    uint32_t framebuffer;
    uint32_t off_screen_memory_offset;
    uint16_t off_screen_memory_size;
    uint8_t reserved2[206];
} __attribute__((packed));

/* Graphics context */
typedef struct {
    uint32_t *framebuffer;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;  /* bytes per scanline */
    uint8_t bpp;     /* bits per pixel */
    uint32_t current_color;
} graphics_context_t;

/* Color definitions (RGB format) */
#define RGB(r, g, b) ((uint32_t)((r << 16) | (g << 8) | b))
#define COLOR_BLACK      RGB(0, 0, 0)
#define COLOR_WHITE      RGB(255, 255, 255)
#define COLOR_RED        RGB(255, 0, 0)
#define COLOR_GREEN      RGB(0, 255, 0)
#define COLOR_BLUE       RGB(0, 0, 255)
#define COLOR_CYAN       RGB(0, 255, 255)
#define COLOR_MAGENTA    RGB(255, 0, 255)
#define COLOR_YELLOW     RGB(255, 255, 0)
#define COLOR_GREY       RGB(128, 128, 128)
#define COLOR_DARK_GREY  RGB(64, 64, 64)
#define COLOR_LIGHT_GREY RGB(192, 192, 192)

/* Function prototypes */
int graphics_init(uint16_t width, uint16_t height, uint8_t bpp);
void graphics_cleanup(void);
graphics_context_t *graphics_get_context(void);

void graphics_set_color(uint32_t color);
uint32_t graphics_get_color(void);
void graphics_clear(uint32_t color);
void graphics_pixel(uint32_t x, uint32_t y, uint32_t color);
void graphics_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void graphics_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void graphics_line(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color);
void graphics_circle(uint32_t x, uint32_t y, uint32_t radius, uint32_t color);
void graphics_fill_circle(uint32_t x, uint32_t y, uint32_t radius, uint32_t color);

void graphics_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg_color, uint32_t bg_color);
void graphics_draw_string(uint32_t x, uint32_t y, const char *str, uint32_t fg_color, uint32_t bg_color);

/* Display functions */
int graphics_set_video_mode(uint16_t width, uint16_t height, uint8_t bpp);
void graphics_flush(void);  /* Copy framebuffer to video memory */
void graphics_show(void);   /* Switch to graphics mode and show framebuffer */
int graphics_is_mode_active(void);  /* Check if graphics mode is active */

/* Test/demo functions */
void graphics_demo(void);

#endif /* GRAPHICS_H */

