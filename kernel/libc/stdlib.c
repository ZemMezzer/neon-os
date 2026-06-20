#include <stddef.h>
#include <stdint.h>

#include "stdlib.h"

#define HEAP_ALIGNMENT 16U
#define HEAP_BLOCK_MAGIC 0x4E484550U
#define HEAP_MIN_SPLIT_SIZE HEAP_ALIGNMENT

typedef struct HeapBlock {
    size_t size;

    struct HeapBlock* next;
    struct HeapBlock* prev;

    uint32_t magic;
    uint32_t is_free;
} HeapBlock;

#define HEAP_HEADER_SIZE \
    ((sizeof(HeapBlock) + (HEAP_ALIGNMENT - 1U)) & \
    ~(HEAP_ALIGNMENT - 1U))

extern uint8_t __heap_start[];
extern uint8_t __heap_end[];

static HeapBlock* heap_first = 0;
static volatile int heap_ready = 0;

static volatile HeapBlock* heap_vblock(HeapBlock* block) {
    return (volatile HeapBlock*)(void*)block;
}

static uintptr_t heap_align_up_address(uintptr_t value) {
    return (value + (HEAP_ALIGNMENT - 1U)) & ~(HEAP_ALIGNMENT - 1U);
}

static uintptr_t heap_align_down_address(uintptr_t value) {
    return value & ~(HEAP_ALIGNMENT - 1U);
}

static size_t heap_align_size(size_t value) {
    if (value > (size_t)-1 - (HEAP_ALIGNMENT - 1U)) {
        return 0;
    }

    return (value + (HEAP_ALIGNMENT - 1U)) & ~(HEAP_ALIGNMENT - 1U);
}

static uint8_t* heap_block_payload(HeapBlock* block) {
    return (uint8_t*)(void*)block + HEAP_HEADER_SIZE;
}

static uintptr_t heap_block_end(HeapBlock* block) {
    volatile HeapBlock* vblock = heap_vblock(block);

    return (uintptr_t)heap_block_payload(block) + vblock->size;
}

static int heap_init(void) {
    uintptr_t start;
    uintptr_t end;
    volatile HeapBlock* first;

    if (heap_ready) {
        return heap_first != 0;
    }

    start = heap_align_up_address((uintptr_t)__heap_start);
    end = heap_align_down_address((uintptr_t)__heap_end);

    if (end <= start) {
        return 0;
    }

    if (end - start <= HEAP_HEADER_SIZE + HEAP_MIN_SPLIT_SIZE) {
        return 0;
    }

    first = (volatile HeapBlock*)(void*)start;

    first->size = (size_t)(end - start - HEAP_HEADER_SIZE);
    first->next = 0;
    first->prev = 0;
    first->magic = HEAP_BLOCK_MAGIC;
    first->is_free = 1;

    heap_first = (HeapBlock*)(void*)start;
    heap_ready = 1;

    return 1;
}

static void heap_split_block(HeapBlock* block, size_t wanted_size) {
    volatile HeapBlock* vblock;
    volatile HeapBlock* vnew_block;
    HeapBlock* new_block;
    HeapBlock* old_next;
    uintptr_t new_block_address;
    size_t block_size;
    size_t remaining;

    if (!block) {
        return;
    }

    vblock = heap_vblock(block);
    block_size = vblock->size;

    if (block_size < wanted_size) {
        return;
    }

    remaining = block_size - wanted_size;

    if (remaining < HEAP_HEADER_SIZE + HEAP_MIN_SPLIT_SIZE) {
        return;
    }

    old_next = vblock->next;

    new_block_address =
        (uintptr_t)heap_block_payload(block) + wanted_size;

    new_block = (HeapBlock*)(void*)new_block_address;
    vnew_block = heap_vblock(new_block);

    vnew_block->size = remaining - HEAP_HEADER_SIZE;
    vnew_block->next = old_next;
    vnew_block->prev = block;
    vnew_block->magic = HEAP_BLOCK_MAGIC;
    vnew_block->is_free = 1;

    if (old_next) {
        heap_vblock(old_next)->prev = new_block;
    }

    vblock->size = wanted_size;
    vblock->next = new_block;
}

static void heap_merge_with_next(HeapBlock* block) {
    volatile HeapBlock* vblock;
    volatile HeapBlock* vnext;
    HeapBlock* next;
    HeapBlock* next_next;

    if (!block) {
        return;
    }

    vblock = heap_vblock(block);
    next = vblock->next;

    if (!next) {
        return;
    }

    vnext = heap_vblock(next);

    if (vnext->magic != HEAP_BLOCK_MAGIC) {
        return;
    }

    if (!vnext->is_free) {
        return;
    }

    if (heap_block_end(block) != (uintptr_t)next) {
        return;
    }

    next_next = vnext->next;

    vblock->size += HEAP_HEADER_SIZE + vnext->size;
    vblock->next = next_next;

    if (next_next) {
        heap_vblock(next_next)->prev = block;
    }
}

static HeapBlock* heap_find_free_block(size_t wanted_size) {
    HeapBlock* block;
    volatile HeapBlock* vblock;
    size_t guard = 0;

    block = heap_first;

    while (block) {
        if (guard++ > 65536) {
            return 0;
        }

        vblock = heap_vblock(block);

        if (vblock->magic != HEAP_BLOCK_MAGIC) {
            return 0;
        }

        if (vblock->is_free && vblock->size >= wanted_size) {
            return block;
        }

        block = vblock->next;
    }

    return 0;
}

static HeapBlock* heap_find_block_by_payload(void* ptr) {
    HeapBlock* block;
    volatile HeapBlock* vblock;
    uintptr_t target;
    uintptr_t start;
    uintptr_t end;
    size_t guard = 0;

    if (!ptr || !heap_init()) {
        return 0;
    }

    target = (uintptr_t)ptr;
    start = (uintptr_t)__heap_start;
    end = (uintptr_t)__heap_end;

    if (target < start + HEAP_HEADER_SIZE || target >= end) {
        return 0;
    }

    block = heap_first;

    while (block) {
        if (guard++ > 65536) {
            return 0;
        }

        vblock = heap_vblock(block);

        if (vblock->magic != HEAP_BLOCK_MAGIC) {
            return 0;
        }

        if ((uintptr_t)heap_block_payload(block) == target) {
            return block;
        }

        block = vblock->next;
    }

    return 0;
}

void* malloc(size_t size)
    __attribute__((noinline, used, optimize("O0")));

void* malloc(size_t size) {
    HeapBlock* block;
    volatile HeapBlock* vblock;
    size_t wanted_size;

    if (!heap_init()) {
        return 0;
    }

    if (size == 0) {
        size = 1;
    }

    wanted_size = heap_align_size(size);

    if (wanted_size == 0) {
        return 0;
    }

    block = heap_find_free_block(wanted_size);

    if (!block) {
        return 0;
    }

    heap_split_block(block, wanted_size);

    vblock = heap_vblock(block);
    vblock->is_free = 0;

    return heap_block_payload(block);
}

void free(void* ptr)
    __attribute__((noinline, used, optimize("O0")));

void free(void* ptr) {
    HeapBlock* block;
    HeapBlock* previous;
    volatile HeapBlock* vblock;

    if (!ptr) {
        return;
    }

    block = heap_find_block_by_payload(ptr);

    if (!block) {
        return;
    }

    vblock = heap_vblock(block);

    if (vblock->is_free) {
        return;
    }

    vblock->is_free = 1;

    heap_merge_with_next(block);

    previous = vblock->prev;

    if (previous && heap_vblock(previous)->is_free) {
        heap_merge_with_next(previous);
    }
}

void* realloc(void* ptr, size_t size)
    __attribute__((noinline, used, optimize("O0")));

void* realloc(void* ptr, size_t size) {
    HeapBlock* block;
    HeapBlock* next;
    volatile HeapBlock* vblock;
    volatile HeapBlock* vnext;
    void* new_ptr;
    size_t wanted_size;
    size_t copy_size;
    uint8_t* source;
    uint8_t* destination;

    if (!ptr) {
        return malloc(size);
    }

    if (size == 0) {
        free(ptr);
        return 0;
    }

    block = heap_find_block_by_payload(ptr);

    if (!block) {
        return 0;
    }

    vblock = heap_vblock(block);

    if (vblock->is_free) {
        return 0;
    }

    wanted_size = heap_align_size(size);

    if (wanted_size == 0) {
        return 0;
    }

    if (vblock->size >= wanted_size) {
        heap_split_block(block, wanted_size);
        return ptr;
    }

    next = vblock->next;

    if (next) {
        vnext = heap_vblock(next);

        if (
            vnext->is_free &&
            vblock->size + HEAP_HEADER_SIZE + vnext->size >= wanted_size
        ) {
            heap_merge_with_next(block);
            heap_split_block(block, wanted_size);

            return ptr;
        }
    }

    new_ptr = malloc(size);

    if (!new_ptr) {
        return 0;
    }

    copy_size = vblock->size;

    if (copy_size > size) {
        copy_size = size;
    }

    source = (uint8_t*)ptr;
    destination = (uint8_t*)new_ptr;

    for (size_t i = 0; i < copy_size; i++) {
        destination[i] = source[i];
    }

    free(ptr);

    return new_ptr;
}

void* calloc(size_t count, size_t size)
    __attribute__((noinline, used, optimize("O0")));

void* calloc(size_t count, size_t size) {
    void* ptr;
    size_t total_size;
    uint8_t* bytes;

    if (count != 0 && size > (size_t)-1 / count) {
        return 0;
    }

    total_size = count * size;

    ptr = malloc(total_size);

    if (!ptr) {
        return 0;
    }

    bytes = (uint8_t*)ptr;

    for (size_t i = 0; i < total_size; i++) {
        bytes[i] = 0;
    }

    return ptr;
}

size_t heap_total_bytes(void) {
    uintptr_t start;
    uintptr_t end;

    if (!heap_init()) {
        return 0;
    }

    start = heap_align_up_address((uintptr_t)__heap_start);
    end = heap_align_down_address((uintptr_t)__heap_end);

    if (end <= start + HEAP_HEADER_SIZE) {
        return 0;
    }

    return (size_t)(end - start - HEAP_HEADER_SIZE);
}

size_t heap_used_bytes(void) {
    HeapBlock* block;
    volatile HeapBlock* vblock;
    size_t total = 0;
    size_t guard = 0;

    if (!heap_init()) {
        return 0;
    }

    block = heap_first;

    while (block) {
        if (guard++ > 65536) {
            return total;
        }

        vblock = heap_vblock(block);

        if (vblock->magic != HEAP_BLOCK_MAGIC) {
            return total;
        }

        if (!vblock->is_free) {
            total += vblock->size;
        }

        block = vblock->next;
    }

    return total;
}

size_t heap_free_bytes(void) {
    HeapBlock* block;
    volatile HeapBlock* vblock;
    size_t total = 0;
    size_t guard = 0;

    if (!heap_init()) {
        return 0;
    }

    block = heap_first;

    while (block) {
        if (guard++ > 65536) {
            return total;
        }

        vblock = heap_vblock(block);

        if (vblock->magic != HEAP_BLOCK_MAGIC) {
            return total;
        }

        if (vblock->is_free) {
            total += vblock->size;
        }

        block = vblock->next;
    }

    return total;
}

void abort(void) {
    while (1) {
        asm volatile("wfe");
    }
}