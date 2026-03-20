// mm.h
#ifndef MM_H
#define MM_H

#include "types.h"

#define PAGE_SIZE 0x1000 // 4KB

// Флаги для map_page
#define PTE_PRESENT     0x1
#define PTE_WRITABLE    0x2
#define PTE_USER        0x4
#define PTE_WRITETHROUGH 0x8
#define PTE_CACHE_DISABLED 0x10
#define PTE_ACCESSED    0x20
#define PTE_DIRTY       0x40
#define PTE_LARGE_PAGE  0x80
#define PTE_GLOBAL      0x100
#define PTE_NO_EXECUTE  (1ULL << 63)

typedef struct {
    uint64_t present : 1;
    uint64_t writable : 1;
    uint64_t user : 1;
    uint64_t writethrough : 1;
    uint64_t cache_disabled : 1;
    uint6 1;
    uint64_t dirty : 1;
    uint64_t large_page : 1; // или PAT для PT
    uint64_t global : 1;
    uint64_t available : 3;
    uint64_t frame_number_high : 32; // bits 32-62 (for 4-level paging)
    uint64_t nx : 1; // No Execute (bit 63)
} __attribute__((packed)) page_table_entry_t;

paddr_t get_free_page();
void free_page(paddr_t page);
paddr_t get_physical_address(uint64_t *page_dir, vaddr_t vaddr);
void map_page(uint64_t *page_dir, vaddr_t vaddr, paddr_t paddr, int flags);
void unmap_page(uint64_t *page_dir, vaddr_t vaddr);
struct page_table_entry *get_pte(uint64_t *page_dir, vaddr_t vaddr);
void mm_init();

#endif

