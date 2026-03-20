#include "sched.h"
#include "mm.h"
#include "lib/string.h"

struct process processes[MAX_PROCESSES];
struct process *current_process = NULL;
struct process *process_queue = NULL;

struct process *create_process() {
    for(int i = 0; i < MAX_PROCESSES; i++) {
        if(processes[i].state == PROC_FREE) {
            processes[i].state = PROC_READY;
            processes[i].id = i;
            processes[i].timeslice = 10;
            processes[i].ticks_remaining = 10;
            processes[i].page_dir = get_free_page();
            processes[i].heap_start = 0x200000;
            processes[i].heap_brk = 0x200000;
            processes[i].heap_end = 0x400000;
            processes[i].parent_id = 0;
            processes[i].umask = 022;
            processes[i].blocked_signals = 0;
            for(int j = 0; j < 64; j++) {
                processes[i].signal_handlers[j] = 0;
            }
            processes[i].signal_stack = NULL;
            processes[i].signal_stack_size = 0;
            processes[i].signal_stack_active = 0;
            
            // Инициализировать регистры
            processes[i].regs = (struct registers *)get_free_page();
            if(processes[i].regs) {
                memset(processes[i].regs, 0, sizeof(struct registers));
                processes[i].regs->rip = 0x400000; // Точка входа процесса
                processes[i].regs->rsp = 0x500000; // Стек процесса
                processes[i].regs->rflags = 0x202; // Interrupt flag enabled
            }
            return &processes[i];
        }
    }
    return NULL;
}

void switch_to_process(struct registers *regs) {
    // Сохранить контекст в стеке
    __asm__ volatile (
        "mov %%rsp, %0\n\t"
        "mov %1, %%rsp\n\t"
        "push %%rax\n\t"
        "push %%rbx\n\t"
        "push %%rcx\n\t"
        "push %%rdx\n\t"
        "push %%rsi\n\t"
        "push %%rdi\n\t"
        "push %%rbp\n\t"
        "push %%r8\n\t"
        "push %%r9\n\t"
        "push %%r10\n\t"
        "push %%r11\n\t"
        "push %%r12\n\t"
        "push %%r13\n\t"
        "push %%r14\n\t"
        "push %%r15\n\t"
        :
        : "m"(current_process->regs->rsp), "m"(regs->rsp)
        : "memory"
    );
}

struct process *copy_process(struct process *parent) {
    struct process *child = create_process();
    if(!child) return NULL;
    
    // Копировать данные процесса
    child->state = PROC_READY;
    child->parent_id = parent->id;
    child->page_dir = parent->page_dir; // Для простоты разделяем страницы
    child->heap_start = parent->heap_start;
    child->heap_brk = parent->heap_brk;
    child->heap_end = parent->heap_end;
    child->umask = parent->umask;
    child->blocked_signals = parent->blocked_signals;
    
    // Копировать открытые файлы
    for(int i = 0; i < MAX_OPEN_FILES; i++) {
        child->open_files[i] = parent->open_files[i];
    }
    
    // Копировать таблицу сигналов
    for(int i = 0; i < 64; i++) {
        child->signal_handlers[i] = parent->signal_handlers[i];
    }
    
    return child;
}

void sched_init() {
    // Инициализация планировщика
}
