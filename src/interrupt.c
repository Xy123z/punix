// src/interrupt.c - Interrupt handling and keyboard driver
#include "../include/interrupt.h"

// Scancode Definitions for Control Key and target letters
#define LCTRL_SCANCODE 0x1D
#define S_SCANCODE     0x1F  // Scancode for the 'S' key
#define X_SCANCODE     0x2D  // Scancode for the 'X' key

// ASCII Control Codes (Used by the editor in text.c)
#define CTRL_S 0x13 // ASCII 19
#define CTRL_X 0x18 // ASCII 24

// IDT
struct idt_entry idt[256];
struct idt_ptr idtp;

// Keyboard buffer (circular)
static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile int kbd_read_pos = 0;
static volatile int kbd_write_pos = 0;

// NEW: Global state flag to track if the control key is pressed
static volatile int ctrl_pressed = 0;

// Port I/O
static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

// Keyboard scancode to ASCII mapping
static const char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

// Keyboard interrupt handler (IRQ1)
void keyboard_handler() {
    uint8_t scancode = inb(0x60);

    // --- Control Key State Tracking ---
    if (scancode == LCTRL_SCANCODE) {
        ctrl_pressed = 1;
        goto end_handler;
    }
    // Check for key release (Scancode | 0x80)
    else if (scancode == (LCTRL_SCANCODE | 0x80)) {
        ctrl_pressed = 0;
        goto end_handler;
    }
    // ----------------------------------

    // Only handle key presses (not releases)
    if (!(scancode & 0x80)) {
        if (scancode < sizeof(scancode_to_ascii)) {
            char c = scancode_to_ascii[scancode];

            // --- Control Character Mapping ---
            if (ctrl_pressed) {
                if (scancode == S_SCANCODE) {
                    c = CTRL_S;
                } else if (scancode == X_SCANCODE) {
                    c = CTRL_X;
                } else {
                    // Ignore other control key combinations to prevent unhandled control chars
                    goto end_handler;
                }
            }
            // ---------------------------------

            if (c != 0) {
                // Add to circular buffer
                keyboard_buffer[kbd_write_pos] = c;
                kbd_write_pos = (kbd_write_pos + 1) % KEYBOARD_BUFFER_SIZE;
            }
        }
    }

end_handler:
    // Send End of Interrupt (EOI) to PIC
    outb(0x20, 0x20);
}

// Assembly wrapper for keyboard interrupt
extern void keyboard_interrupt_handler();
__asm__(
    ".global keyboard_interrupt_handler\n"
    "keyboard_interrupt_handler:\n"
    "   pusha\n"
    "   call keyboard_handler\n"
    "   popa\n"
    "   iret\n"
);

// Check if keyboard buffer has data
int keyboard_has_data() {
    return kbd_read_pos != kbd_write_pos;
}

// Read character from keyboard buffer (with HLT for power saving)
char keyboard_read() {
    while (!keyboard_has_data()) {
        __asm__ volatile("sti; hlt; cli");
    }

    char c = keyboard_buffer[kbd_read_pos];
    kbd_read_pos = (kbd_read_pos + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

// Read line function (Uses the new C-style name 'keyboard_read_line')
void keyboard_read_line(char* buffer, int max_len) {
    int i = 0;

    while (i < max_len - 1) {
        char c = keyboard_read();

        if (c == '\n') {
            buffer[i] = '\0';
            // Note: putchar needs to be called from caller
            break;
        } else if (c == '\b') {
            if (i > 0) {
                i--;
                // Backspace handled by caller
            }
        } else if ((c >= '0' && c <= '9') || c == '-' ||
                   (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   c == ' ' || c == '.') {
            buffer[i++] = c;
        }
        // IMPORTANT: Control characters like CTRL_S and CTRL_X are NOT handled here,
        // as this function is for reading command lines only.
    }

    buffer[i] = '\0';
}

// Set IDT entry
static void idt_set_gate(int num, uint32_t handler, uint16_t selector, uint8_t flags) {
    idt[num].offset_low = handler & 0xFFFF;
    idt[num].selector = selector;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
    idt[num].offset_high = (handler >> 16) & 0xFFFF;
}

// Initialize IDT
void idt_init() {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (uint32_t)&idt;

    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    // Set keyboard interrupt (IRQ1 = interrupt 33)
    idt_set_gate(33, (uint32_t)keyboard_interrupt_handler, 0x08, 0x8E);

    __asm__ volatile("lidt %0" : : "m"(idtp));
}

// Initialize PIC
void pic_init() {
    outb(0x20, 0x11);
    outb(0x21, 0x20);
    outb(0x21, 0x04);
    outb(0x21, 0x01);

    outb(0xA0, 0x11);
    outb(0xA1, 0x28);
    outb(0xA1, 0x02);
    outb(0xA1, 0x01);

    // Unmask IRQ1 (keyboard) only
    outb(0x21, 0xFD);
    outb(0xA1, 0xFF);
}

void keyboard_init() {
    kbd_read_pos = 0;
    kbd_write_pos = 0;
}
