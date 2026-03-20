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

// --- Новые элементы для буфера ---
#define KEYBOARD_BUFFER_SIZE 256
static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile int keyboard_buffer_head = 0;
static volatile int keyboard_buffer_tail = 0;
static volatile int keyboard_buffer_count = 0;
// --- Конец новых элементов ---

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
    uint8_t scancode = inb(0x60 Check if it's a key press (bit 7 not set)
    if (!(scancode & 0x80)) {
        // Convert scancode to ASCII character
        if (scancode < sizeof(scancode_to_ascii)) {
            char c = scancode_to_ascii[scancode];
            if (c != 0) {
                // --- Изменение: добавляем символ в буфер вместо печати ---
                if (keyboard_buffer_count < KEYBOARD_BUFFER_SIZE) {
                    keyboard_buffer[keyboard_buffer_head] = c;
                    keyboard_buffer_head = (keyboard_buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
                    keyboard_buffer_count++;
                }
                // char str[2] = {c, 0};
                // printf("%s", str); // <-- Закомментировано
                // --- Конец изменения ---
            }
        }
    }
}

// --- Новые функции для доступа к буферу ---
char keyboard_get_char() {
    if (keyboard_buffer_count > 0) {
        char c = keyboard_buffer[keyboard_buffer_tail];
        keyboard_buffer_tail = (keyboard_buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
        keyboard_buffer_count--;
        return c;
    }
    return 0; // Буфер пуст
}

int keyboard_chars_available() {
    return keyboard_buffer_count > 0;
}
// --- Конец новых функций ---

