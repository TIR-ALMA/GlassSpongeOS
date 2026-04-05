// drivers/timer.c
#include "timer.h"
#include "lib/printf.h"
#include "../sched.h"
#include "../lib/stdarg.h"
#include"
#include "../net/network.h"  // Добавить для network_poll()

// Счетчик тиков с момента запуска
volatile unsigned long timer_ticks = 0;

void timer_init() {
    // Настройка PIT (Programmable Interval Timer)
    // Частота системного таймера 100 Гц (каждые 10 мс)
    unsigned int divisor = 1193180 / 100; // 1193180 Hz - частота PIT
    
    // Отправить команду настройки делителя
    __asm__ volatile("outb %0, %1" : : "a"((char)(divisor & 0xFF)), "Nd"(0x40)); // LSB
    __asm__ volatile("outb %0, %1" : : "a"((char)((divisor >> 8) & 0xFF)), "Nd"(0x40)); // MSB
}

void timer_handler() {
    timer_ticks++;
    
    // Обновить количество тиков у текущего процесса
    if(current_process) {
        current_process->ticks_remaining--;
        if(current_process->ticks_remaining <= 0) {
            current_process->state = PROC_READY;
            schedule();
        }
    }
    
    // Периодическое обучение нейросети
    if(timer_ticks % 100 == 0) { // Каждую секунду
        // Пример обучения: PID=1, время=0, частота=100, активен=1
        kernel_liquid_predict(1, timer_ticks, 100, 1);
    }
    
    // Добавить вызов сетевого poll
    network_poll();
    
    // Отправить EOI (End of Interrupt) контроллеру прерываний
    __asm__ volatile("outb $0x20, $0xA0"); // Slave PIC
    __asm__ volatile("outb $0x20, $0x20"); // Master PIC
}

