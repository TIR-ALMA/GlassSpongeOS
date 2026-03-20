#ifndef SCHED_H
#define SCHED_H

#include "types.h"

#define MAX_PROCESSES 64
#define MAX_OPEN_FILES 64
#define PAGE_SIZE 0x1000

enum {
    PROC_FREE = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_ZOMBIE
};

struct registers {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    uint64_t rip, rsp, rflags;
    uint64_t cr3;
    uint64_t error_code;
    uint64_t fault_addr;
};

struct file_descriptor {
    int in_use;
    int type;
    void *data;
    int flags;
    size_t offset;
    size_t size;
};

struct process {
    int state;
    pid_t id;
    char name[32];
    uint64_t *page_dir;
    struct registers *regs;
    vaddr_t heap_start;
    vaddr_t heap_brk;
    vaddr_t heap_end;
    int timeslice;
    int ticks_remaining;
    pid_t parent_id;
    mode_t umask;
    uint64_t blocked_signals;
    void (*signal_handlers[64])(int);
    void *signal_stack;
    size_t signal_stack_size;
    int signal_stack_active;
    struct file_descriptor *open_files[MAX_OPEN_FILES];
    int exit_code;
    struct process *next;
    struct process *prev;
};

extern struct process processes[MAX_PROCESSES];
extern struct process *current_process;
extern struct process *process_queue;

struct process *create_process();
void switch_to_process(struct registers *regs);
struct process *copy_process(struct process *parent);
void sched_init();
void schedule();

#endif

