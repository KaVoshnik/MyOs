#include <interrupts.h>
#include <io.h>
#include <terminal.h>
#include <string.h>
#include <pit.h>
#include <keyboard.h>
#include <mouse.h>

#define IDT_ENTRY_COUNT 256
#define IDT_TYPE_INTERRUPT_GATE 0x8E
#define KERNEL_CODE_SELECTOR 0x08
#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1
#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01
#define PIC_EOI 0x20
#define IRQ_BASE 0x20

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_descriptor {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[IDT_ENTRY_COUNT];
static struct idt_descriptor idtr;

static const char *exception_messages[] = {
    "Divide-by-zero",
    "Debug",
    "Non-maskable interrupt",
    "Breakpoint",
    "Overflow",
    "Bound range exceeded",
    "Invalid opcode",
    "Device not available",
    "Double fault",
    "Coprocessor segment overrun",
    "Invalid TSS",
    "Segment not present",
    "Stack fault",
    "General protection fault",
    "Page fault",
    "Reserved",
    "x87 FPU error",
    "Alignment check",
    "Machine check",
    "SIMD floating point",
    "Virtualization",
    "Security",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

static inline void lidt(const struct idt_descriptor *desc) {
    __asm__ volatile("lidt %0" : : "m"(*desc));
}

static void idt_set_gate(uint8_t vector, void (*isr)(void)) {
    uint64_t handler = (uint64_t)isr;
    idt[vector].offset_low = handler & 0xFFFF;
    idt[vector].selector = KERNEL_CODE_SELECTOR;
    idt[vector].ist = 0;
    idt[vector].type_attr = IDT_TYPE_INTERRUPT_GATE;
    idt[vector].offset_mid = (handler >> 16) & 0xFFFF;
    idt[vector].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[vector].zero = 0;
}

static void pic_remap(void) {
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC1_DATA, IRQ_BASE);
    io_wait();
    outb(PIC2_DATA, IRQ_BASE + 8);
    io_wait();
    outb(PIC1_DATA, 4);
    io_wait();
    outb(PIC2_DATA, 2);
    io_wait();
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    outb(PIC1_DATA, 0xFC); /* unmask IRQ0 and IRQ1 */
#if ENABLE_MOUSE_DRIVER
    outb(PIC2_DATA, 0xEF); /* unmask IRQ12 (mouse) */
#else
    outb(PIC2_DATA, 0xFF); /* keep all slave IRQs masked */
#endif
}

static void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

static void print_hex64(uint64_t value) {
    static const char hex_digits[] = "0123456789ABCDEF";
    char buffer[17];
    buffer[16] = '\0';
    for (int i = 15; i >= 0; --i) {
        buffer[i] = hex_digits[value & 0xF];
        value >>= 4;
    }
    terminal_write(buffer);
}

static void exception_handler_common(uint8_t vector, uint64_t error_code) {
    const char *message = "Unknown";
    if (vector < sizeof(exception_messages) / sizeof(exception_messages[0])) {
        message = exception_messages[vector];
    }
    terminal_write_line("");
    terminal_set_color(TERMINAL_COLOR_LIGHT_RED, TERMINAL_COLOR_BLACK);
    terminal_write("[exception] ");
    terminal_write(message);
    terminal_write(" (vector 0x");
    print_hex64(vector);
    terminal_write_line(")");
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREY, TERMINAL_COLOR_BLACK);
    terminal_write("Error code: 0x");
    print_hex64(error_code);
    terminal_write_line("");
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

#define DEFINE_ISR_NOERR(n) \
    __attribute__((interrupt)) \
    static void isr##n(struct interrupt_frame *frame) { \
        (void)frame; \
        exception_handler_common(n, 0); \
    }

#define DEFINE_ISR_ERR(n) \
    __attribute__((interrupt)) \
    static void isr##n(struct interrupt_frame *frame, uint64_t error_code) { \
        (void)frame; \
        exception_handler_common(n, error_code); \
    }

DEFINE_ISR_NOERR(0)
DEFINE_ISR_NOERR(1)
DEFINE_ISR_NOERR(2)
DEFINE_ISR_NOERR(3)
DEFINE_ISR_NOERR(4)
DEFINE_ISR_NOERR(5)
DEFINE_ISR_NOERR(6)
DEFINE_ISR_NOERR(7)
DEFINE_ISR_ERR(8)
DEFINE_ISR_NOERR(9)
DEFINE_ISR_ERR(10)
DEFINE_ISR_ERR(11)
DEFINE_ISR_ERR(12)
DEFINE_ISR_ERR(13)
DEFINE_ISR_ERR(14)
DEFINE_ISR_NOERR(15)
DEFINE_ISR_NOERR(16)
DEFINE_ISR_ERR(17)
DEFINE_ISR_NOERR(18)
DEFINE_ISR_NOERR(19)
DEFINE_ISR_NOERR(20)
DEFINE_ISR_NOERR(21)
DEFINE_ISR_NOERR(22)
DEFINE_ISR_NOERR(23)
DEFINE_ISR_NOERR(24)
DEFINE_ISR_NOERR(25)
DEFINE_ISR_NOERR(26)
DEFINE_ISR_NOERR(27)
DEFINE_ISR_NOERR(28)
DEFINE_ISR_NOERR(29)
DEFINE_ISR_NOERR(30)
DEFINE_ISR_NOERR(31)

__attribute__((interrupt))
static void irq_timer(struct interrupt_frame *frame) {
    (void)frame;
    pit_handle_tick();
    pic_send_eoi(0);
}

__attribute__((interrupt))
static void irq_keyboard(struct interrupt_frame *frame) {
    (void)frame;
    uint8_t scancode = inb(0x60);
    keyboard_handle_scancode(scancode);
    pic_send_eoi(1);
}

#if ENABLE_MOUSE_DRIVER
__attribute__((interrupt))
static void irq_mouse(struct interrupt_frame *frame) {
    (void)frame;
    static uint8_t mouse_packet[4] = {0};
    static size_t mouse_packet_index = 0;
    
    uint8_t status = inb(0x64);
    if (!(status & 0x20)) {
        pic_send_eoi(12);
        return;
    }
    
    uint8_t data = inb(0x60);
    
    size_t expected_length = mouse_packet_length();
    if (expected_length < 3) {
        expected_length = 3;
    } else if (expected_length > 4) {
        expected_length = 4;
    }
    
    if (mouse_packet_index < expected_length && mouse_packet_index < sizeof(mouse_packet)) {
        mouse_packet[mouse_packet_index++] = data;
    } else {
        mouse_packet_index = 0;
        pic_send_eoi(12);
        return;
    }
    
    if (mouse_packet_index < expected_length) {
        pic_send_eoi(12);
        return;
    }
    
    uint8_t flags = mouse_packet[0];
    if (!(flags & 0x08)) {
        mouse_packet_index = 0;
        pic_send_eoi(12);
        return;
    }
    
    int8_t x_delta = (int8_t)mouse_packet[1];
    int8_t y_delta = (int8_t)mouse_packet[2];
    
    int buttons = 0;
    if (flags & 0x01) buttons |= MOUSE_EVENT_BUTTON_LEFT;
    if (flags & 0x02) buttons |= MOUSE_EVENT_BUTTON_RIGHT;
    if (flags & 0x04) buttons |= MOUSE_EVENT_BUTTON_MIDDLE;
    
    int scroll = 0;
    if (expected_length == 4) {
        int8_t scroll_delta = (int8_t)mouse_packet[3];
        if (scroll_delta > 0) {
            scroll = MOUSE_EVENT_SCROLL_UP;
        } else if (scroll_delta < 0) {
            scroll = MOUSE_EVENT_SCROLL_DOWN;
        }
    }
    
    mouse_event_t event = {
        .x = x_delta,
        .y = y_delta,
        .buttons = buttons,
        .scroll = scroll
    };
    
    mouse_buffer_push_direct(&event);
    mouse_packet_index = 0;
    
    pic_send_eoi(12);
}
#endif

void interrupts_init(void) {
    memset(idt, 0, sizeof(idt));

    idt_set_gate(0, (void *)isr0);
    idt_set_gate(1, (void *)isr1);
    idt_set_gate(2, (void *)isr2);
    idt_set_gate(3, (void *)isr3);
    idt_set_gate(4, (void *)isr4);
    idt_set_gate(5, (void *)isr5);
    idt_set_gate(6, (void *)isr6);
    idt_set_gate(7, (void *)isr7);
    idt_set_gate(8, (void *)isr8);
    idt_set_gate(9, (void *)isr9);
    idt_set_gate(10, (void *)isr10);
    idt_set_gate(11, (void *)isr11);
    idt_set_gate(12, (void *)isr12);
    idt_set_gate(13, (void *)isr13);
    idt_set_gate(14, (void *)isr14);
    idt_set_gate(15, (void *)isr15);
    idt_set_gate(16, (void *)isr16);
    idt_set_gate(17, (void *)isr17);
    idt_set_gate(18, (void *)isr18);
    idt_set_gate(19, (void *)isr19);
    idt_set_gate(20, (void *)isr20);
    idt_set_gate(21, (void *)isr21);
    idt_set_gate(22, (void *)isr22);
    idt_set_gate(23, (void *)isr23);
    idt_set_gate(24, (void *)isr24);
    idt_set_gate(25, (void *)isr25);
    idt_set_gate(26, (void *)isr26);
    idt_set_gate(27, (void *)isr27);
    idt_set_gate(28, (void *)isr28);
    idt_set_gate(29, (void *)isr29);
    idt_set_gate(30, (void *)isr30);
    idt_set_gate(31, (void *)isr31);

    idt_set_gate(IRQ_BASE + 0, (void *)irq_timer);
    idt_set_gate(IRQ_BASE + 1, (void *)irq_keyboard);
#if ENABLE_MOUSE_DRIVER
    idt_set_gate(IRQ_BASE + 12, (void *)irq_mouse);
#endif

    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint64_t)&idt[0];

    lidt(&idtr);
    pic_remap();
}

void interrupts_enable(void) {
    __asm__ volatile("sti");
}

void interrupts_disable(void) {
    __asm__ volatile("cli");
}

