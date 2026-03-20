// keyboard.c
#include "keyboard.h"
#include "lib/printf.h"
#include "sched.h"
#include "types.h"

// PS/2 Keyboard scan codes (without E0 prefix) for US layout
static const char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*',
    0, ' '
};

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void keyboard_init() {
    // Nothing needed for basic PS/2 keyboard initialization
}

void keyboard_handler() {
    // Read scancode from PS/2 controller port 0x60
    uint8_t scancode = inb(0x60);
    
    // Check if it's a key press (bit 7 not set)
    if (!(scancode & 0x80)) {
        // Convert scancode to ASCII character
        if (scancode < sizeof(scancode_to_ascii)) {
            char c = scancode_to_ascii[scancode];
            if (c != 0) {
                // Print character to screen
                char str[2] = {c, 0};
                printf("%s", str);
            }
        }
    }
}

