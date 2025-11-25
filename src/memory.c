// src/memory.c - Memory management implementation
#include "../include/memory.h"
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef unsigned int size_t;
typedef int int32_t;
// Physical memory bitmap
static uint32_t page_bitmap[TOTAL_PAGES / 32];
static uint32_t used_pages = 0;
static uint32_t free_pages = TOTAL_PAGES;

// Heap allocator
typedef struct heap_block {
    uint32_t size;
    int is_free;
    struct heap_block* next;
} heap_block_t;

static heap_block_t* heap_start = 0;

// ============================================
// PHYSICAL MEMORY MANAGER
// ============================================

void pmm_init() {
    for (uint32_t i = 0; i < TOTAL_PAGES / 32; i++) {
        page_bitmap[i] = 0;
    }
    used_pages = 0;
    free_pages = TOTAL_PAGES;
}

static inline int pmm_test_page(uint32_t page) {
    uint32_t index = page / 32;
    uint32_t bit = page % 32;
    return page_bitmap[index] & (1 << bit);
}

static inline void pmm_set_page(uint32_t page) {
    uint32_t index = page / 32;
    uint32_t bit = page % 32;
    page_bitmap[index] |= (1 << bit);
}

static inline void pmm_clear_page(uint32_t page) {
    uint32_t index = page / 32;
    uint32_t bit = page % 32;
    page_bitmap[index] &= ~(1 << bit);
}

static uint32_t pmm_find_free_page() {
    for (uint32_t i = 0; i < TOTAL_PAGES; i++) {
        if (!pmm_test_page(i)) {
            return i;
        }
    }
    return (uint32_t)-1;
}

void* pmm_alloc_page() {
    uint32_t page = pmm_find_free_page();

    if (page == (uint32_t)-1) {
        return 0;
    }

    pmm_set_page(page);
    used_pages++;
    free_pages--;

    uint32_t addr = KERNEL_END + (page * PAGE_SIZE);

    // Zero out the page
    uint32_t* ptr = (uint32_t*)addr;
    for (int i = 0; i < PAGE_SIZE / 4; i++) {
        ptr[i] = 0;
    }

    return (void*)addr;
}

void pmm_free_page(void* addr) {
    uint32_t page_addr = (uint32_t)addr;

    if (page_addr < KERNEL_END || page_addr >= MEMORY_END) {
        return;
    }

    uint32_t page = (page_addr - KERNEL_END) / PAGE_SIZE;

    if (!pmm_test_page(page)) {
        return;
    }

    pmm_clear_page(page);
    used_pages--;
    free_pages++;
}

void pmm_get_stats(uint32_t* total, uint32_t* used, uint32_t* free) {
    *total = TOTAL_PAGES;
    *used = used_pages;
    *free = free_pages;
}

// ============================================
// HEAP ALLOCATOR (kmalloc/kfree)
// ============================================

void heap_init() {
    heap_start = (heap_block_t*)pmm_alloc_page();
    heap_start->size = PAGE_SIZE - sizeof(heap_block_t);
    heap_start->is_free = 1;
    heap_start->next = 0;
}

static heap_block_t* heap_find_block(size_t size) {
    heap_block_t* current = heap_start;
    heap_block_t* best = 0;

    while (current) {
        if (current->is_free && current->size >= size) {
            if (!best || current->size < best->size) {
                best = current;
            }
        }
        current = current->next;
    }

    return best;
}

void* kmalloc(size_t size) {
    if (size == 0) return 0;

    size = (size + 3) & ~3;

    heap_block_t* block = heap_find_block(size);

    if (!block) {
        void* new_page = pmm_alloc_page();
        if (!new_page) return 0;

        block = (heap_block_t*)new_page;
        block->size = PAGE_SIZE - sizeof(heap_block_t);
        block->is_free = 1;
        block->next = 0;

        heap_block_t* current = heap_start;
        while (current->next) {
            current = current->next;
        }
        current->next = block;
    }

    if (block->size >= size + sizeof(heap_block_t) + 64) {
        heap_block_t* new_block = (heap_block_t*)((uint8_t*)block + sizeof(heap_block_t) + size);
        new_block->size = block->size - size - sizeof(heap_block_t);
        new_block->is_free = 1;
        new_block->next = block->next;

        block->size = size;
        block->next = new_block;
    }

    block->is_free = 0;

    return (void*)((uint8_t*)block + sizeof(heap_block_t));
}

void kfree(void* ptr) {
    if (!ptr) return;

    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));
    block->is_free = 1;

    if (block->next && block->next->is_free) {
        block->size += sizeof(heap_block_t) + block->next->size;
        block->next = block->next->next;
    }

    heap_block_t* current = heap_start;
    while (current && current->next != block) {
        current = current->next;
    }

    if (current && current->is_free) {
        current->size += sizeof(heap_block_t) + block->size;
        current->next = block->next;
    }
}
