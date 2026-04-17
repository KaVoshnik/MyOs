#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include <stddef.h>

/* =========================================================
 * Graphics subsystem for MyOs
 * ---------------------------------------------------------
 * Architecture:
 *   - Direct-write to Multiboot linear framebuffer (32 bpp)
 *   - Back-buffer (heap) + graphics_flip() for tear-free GUI
 *   - 8x16 font for readable text at any resolution
 *   - Primitive drawing layer (pixels, rects, lines, circles)
 *   - Text layer with clipping
 *   - GUI widget layer (windows, buttons, taskbar) — for desktop
 * ========================================================= */

/* ----- Framebuffer info (filled by graphics_init) ---------- */
typedef struct {
    uint32_t *framebuffer;   /* back-buffer (heap)              */
    uint32_t *hw_framebuffer;/* hardware video memory (direct)  */
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch_pixels;  /* pitch in 32-bit words           */
    uint8_t   bpp;
    int       ready;         /* 1 after successful init         */
} gfx_ctx_t;

/* ----- Color helpers --------------------------------------- */
/* All colors are 0x00RRGGBB in the back-buffer.
 * The hardware framebuffer may use a different channel order;
 * graphics_pixel_hw() handles the conversion automatically.   */
#define GFX_RGB(r,g,b)  ((uint32_t)(((uint32_t)(r)<<16)|((uint32_t)(g)<<8)|(uint32_t)(b)))
#define GFX_RGBA(r,g,b,a) ((uint32_t)(((uint32_t)(a)<<24)|((uint32_t)(r)<<16)|((uint32_t)(g)<<8)|(uint32_t)(b)))

/* Named palette */
#define GFX_BLACK        GFX_RGB(  0,  0,  0)
#define GFX_WHITE        GFX_RGB(255,255,255)
#define GFX_RED          GFX_RGB(220, 50, 50)
#define GFX_GREEN        GFX_RGB( 50,200, 50)
#define GFX_BLUE         GFX_RGB( 50,100,220)
#define GFX_CYAN         GFX_RGB(  0,210,210)
#define GFX_MAGENTA      GFX_RGB(200, 50,200)
#define GFX_YELLOW       GFX_RGB(240,200,  0)
#define GFX_ORANGE       GFX_RGB(240,130,  0)
#define GFX_DARK_GREY    GFX_RGB( 40, 40, 40)
#define GFX_GREY         GFX_RGB(100,100,100)
#define GFX_MID_GREY     GFX_RGB(140,140,140)
#define GFX_LIGHT_GREY   GFX_RGB(200,200,200)
#define GFX_TRANSPARENT  0xFFFFFFFFu   /* sentinel: skip pixel  */

/* Desktop color scheme */
#define GFX_DESKTOP_BG        GFX_RGB( 30, 50, 80)   /* dark blue   */
#define GFX_TASKBAR_BG        GFX_RGB( 20, 30, 50)
#define GFX_TASKBAR_FG        GFX_WHITE
#define GFX_WIN_TITLE_BG      GFX_RGB( 50, 90,160)
#define GFX_WIN_TITLE_FG      GFX_WHITE
#define GFX_WIN_TITLE_INACT   GFX_RGB( 80, 80,100)
#define GFX_WIN_BODY_BG       GFX_RGB(230,230,235)
#define GFX_WIN_BORDER        GFX_RGB( 50, 90,160)
#define GFX_BTN_BG            GFX_RGB(210,210,215)
#define GFX_BTN_HOVER         GFX_RGB(180,200,240)
#define GFX_BTN_PRESS         GFX_RGB(100,140,210)
#define GFX_BTN_BORDER        GFX_RGB(120,120,140)
#define GFX_BTN_FG            GFX_BLACK
#define GFX_TEXT_FG           GFX_BLACK
#define GFX_TEXT_BG           GFX_TRANSPARENT

/* Font dimensions */
#define GFX_FONT_W  8
#define GFX_FONT_H  16

/* =========================================================
 * Core init / flip
 * ========================================================= */
int  graphics_init(void);      /* call after memory_init(); uses mb_info */
void graphics_flip(void);      /* blit back-buffer -> hw framebuffer      */
void graphics_shutdown(void);
int  graphics_ready(void);
gfx_ctx_t *graphics_ctx(void);

uint32_t graphics_width(void);
uint32_t graphics_height(void);

/* =========================================================
 * Primitive drawing  (all write to back-buffer)
 * ========================================================= */
void gfx_clear(uint32_t color);
void gfx_pixel(uint32_t x, uint32_t y, uint32_t color);
void gfx_hline(uint32_t x, uint32_t y, uint32_t w, uint32_t color);
void gfx_vline(uint32_t x, uint32_t y, uint32_t h, uint32_t color);
void gfx_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
void gfx_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void gfx_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void gfx_fill_rect_blend(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                          uint32_t color, uint8_t alpha); /* simple alpha */
void gfx_circle(uint32_t cx, uint32_t cy, uint32_t r, uint32_t color);
void gfx_fill_circle(uint32_t cx, uint32_t cy, uint32_t r, uint32_t color);
void gfx_rounded_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                       uint32_t r, uint32_t color);
void gfx_fill_rounded_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                            uint32_t r, uint32_t fill, uint32_t border);

/* =========================================================
 * Text drawing
 * ========================================================= */
/* Draw a single character; bg=GFX_TRANSPARENT skips background */
void gfx_char(uint32_t x, uint32_t y, char c,
               uint32_t fg, uint32_t bg);
/* Draw a string; returns x position after last character */
uint32_t gfx_string(uint32_t x, uint32_t y, const char *s,
                     uint32_t fg, uint32_t bg);
/* Draw string clipped to a rectangle */
void gfx_string_clipped(uint32_t x, uint32_t y, const char *s,
                         uint32_t fg, uint32_t bg,
                         uint32_t clip_x, uint32_t clip_y,
                         uint32_t clip_w, uint32_t clip_h);
/* Centered string inside a rect */
void gfx_string_centered(uint32_t rx, uint32_t ry, uint32_t rw, uint32_t rh,
                          const char *s, uint32_t fg, uint32_t bg);
/* Measure string width in pixels */
uint32_t gfx_string_width(const char *s);

/* =========================================================
 * GUI widget layer
 * ========================================================= */

/* ---- Taskbar -------------------------------------------- */
#define GFX_TASKBAR_H  28

typedef struct {
    char     title[64];
    uint32_t time_str[8]; /* reserved for clock */
} gfx_taskbar_t;

void gfx_taskbar_draw(const char *title, const char *clock_str);

/* ---- Button --------------------------------------------- */
typedef struct {
    uint32_t x, y, w, h;
    char     label[32];
    int      pressed;   /* 1 = currently held */
    int      hovered;   /* 1 = mouse over     */
    int      enabled;
} gfx_button_t;

void gfx_button_init(gfx_button_t *btn, uint32_t x, uint32_t y,
                     uint32_t w, uint32_t h, const char *label);
void gfx_button_draw(const gfx_button_t *btn);
int  gfx_button_hit(const gfx_button_t *btn, uint32_t mx, uint32_t my);

/* ---- Window --------------------------------------------- */
#define GFX_WIN_TITLE_H   22
#define GFX_WIN_MAX       8
#define GFX_WIN_FLAG_NONE      0
#define GFX_WIN_FLAG_NORESIZE  (1<<0)
#define GFX_WIN_FLAG_MODAL     (1<<1)

typedef struct {
    uint32_t x, y, w, h;  /* outer frame including title bar  */
    char     title[64];
    int      visible;
    int      focused;
    int      dragging;
    int32_t  drag_ox, drag_oy;  /* offset from mouse to window origin */
    uint32_t flags;
    /* Close button bounds (absolute) */
    uint32_t close_x, close_y, close_w, close_h;
} gfx_window_t;

void gfx_window_init(gfx_window_t *win, uint32_t x, uint32_t y,
                     uint32_t w, uint32_t h, const char *title, uint32_t flags);
void gfx_window_draw(const gfx_window_t *win);

/* Inner client area origin and size */
uint32_t gfx_window_client_x(const gfx_window_t *win);
uint32_t gfx_window_client_y(const gfx_window_t *win);
uint32_t gfx_window_client_w(const gfx_window_t *win);
uint32_t gfx_window_client_h(const gfx_window_t *win);

/* Mouse event processing — returns 1 if event was consumed */
int gfx_window_mouse_down(gfx_window_t *win, uint32_t mx, uint32_t my);
int gfx_window_mouse_up(gfx_window_t *win, uint32_t mx, uint32_t my);
int gfx_window_mouse_move(gfx_window_t *win, uint32_t mx, uint32_t my);

/* Hit-test: is (mx,my) in title bar (drag zone)? */
int gfx_window_hit_titlebar(const gfx_window_t *win, uint32_t mx, uint32_t my);
/* Hit-test: is (mx,my) on close button? */
int gfx_window_hit_close(const gfx_window_t *win, uint32_t mx, uint32_t my);

/* ---- Mouse cursor --------------------------------------- */
void gfx_cursor_draw(uint32_t mx, uint32_t my);

/* ---- Desktop shell helper ------------------------------- */
/* Draw a full desktop frame: background + taskbar.
 * Call once per frame before drawing windows.               */
void gfx_desktop_begin(const char *clock_str);

/* Compatibility shims for existing shell_cmd_gui code */
#define RGB(r,g,b)       GFX_RGB(r,g,b)
#define COLOR_BLACK      GFX_BLACK
#define COLOR_WHITE      GFX_WHITE
#define COLOR_RED        GFX_RED
#define COLOR_GREEN      GFX_GREEN
#define COLOR_BLUE       GFX_BLUE
#define COLOR_CYAN       GFX_CYAN
#define COLOR_MAGENTA    GFX_MAGENTA
#define COLOR_YELLOW     GFX_YELLOW
#define COLOR_GREY       GFX_GREY
#define COLOR_DARK_GREY  GFX_DARK_GREY
#define COLOR_LIGHT_GREY GFX_LIGHT_GREY

/* Old API shims — implemented as thin wrappers */
static inline int  graphics_init_compat(uint16_t w, uint16_t h, uint8_t b)
                    { (void)w;(void)h;(void)b; return graphics_init(); }
static inline void graphics_cleanup(void)    { graphics_shutdown(); }
static inline int  graphics_is_mode_active(void) { return graphics_ready(); }
static inline void graphics_show(void)       { graphics_flip(); }
static inline void graphics_flush(void)      { graphics_flip(); }
static inline void graphics_clear(uint32_t c){ gfx_clear(c); }
static inline void graphics_pixel(uint32_t x,uint32_t y,uint32_t c){gfx_pixel(x,y,c);}
static inline void graphics_fill_rect(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint32_t c){gfx_fill_rect(x,y,w,h,c);}
static inline void graphics_rect(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint32_t c){gfx_rect(x,y,w,h,c);}
static inline void graphics_line(uint32_t x0,uint32_t y0,uint32_t x1,uint32_t y1,uint32_t c){gfx_line((int32_t)x0,(int32_t)y0,(int32_t)x1,(int32_t)y1,c);}
static inline void graphics_circle(uint32_t x,uint32_t y,uint32_t r,uint32_t c){gfx_circle(x,y,r,c);}
static inline void graphics_fill_circle(uint32_t x,uint32_t y,uint32_t r,uint32_t c){gfx_fill_circle(x,y,r,c);}
static inline void graphics_draw_char(uint32_t x,uint32_t y,char c,uint32_t fg,uint32_t bg){gfx_char(x,y,c,fg,bg);}
static inline void graphics_draw_string(uint32_t x,uint32_t y,const char*s,uint32_t fg,uint32_t bg){gfx_string(x,y,s,fg,bg);}
typedef gfx_ctx_t graphics_context_t;
static inline graphics_context_t *graphics_get_context(void){return graphics_ctx();}
static inline int graphics_set_video_mode(uint16_t w,uint16_t h,uint8_t b){(void)w;(void)h;(void)b;return graphics_ready()?0:-1;}

void graphics_demo(void);

#endif /* GRAPHICS_H */
