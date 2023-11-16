/*
 * mm-explicit.c - The best malloc package EVAR!
 *
 * TODO (bug): Uh..this is an implicit list???
 */

#include <stdint.h>
#include <stdio.h>

#include "memlib.h"
#include "mm.h"

/** The required alignment of heap payloads */
const size_t ALIGNMENT = 2 * sizeof(size_t);


/** The layout of each block allocated on the heap */
typedef struct {
    /** The size of the block and whether it is allocated (stored in the low bit) */
    size_t header;
    /**
     * We don't know what the size of the payload will be, so we will
     * declare it as a zero-length array.  This allow us to obtain a
     * pointer to the start of the payload.
     */
    uint8_t payload[];
} block_t;

/*
typedef struct {
    size_t header;
    free_block_t *prev;
    free_block_t *next;
    uint8_t payload[];
} free_block_t;
*/

/** The first and last blocks on the heap */
static block_t *mm_heap_first = NULL;
static block_t *mm_heap_last = NULL;
//static free_block_t *free_list_start = NULL;

/** Rounds up `size` to the nearest multiple of `n` */
static size_t round_up(size_t size, size_t n) {
    return (size + (n - 1)) / n * n;
}


/*
static void add_to_front(free_block_t *block) {
    if (free_list_start == NULL){
        free_list_start = block;
        block->prev = NULL;
        block->next = NULL;
        return;
    }
    block->prev = NULL;
    block->next = free_list_start;
    free_list_start = block;
}

static void remove_from_list(free_block_t *block) {
    if (free_list_start == NULL){
        free_list_start = block;
        block->prev = NULL;
        block->next = NULL;
        return;
    }
    block->prev = NULL;
    block->next = free_list_start;
    free_list_start = block;
}

static block_t *get_backward(block_t *block){
    look at back at footer to get size and add to that 
    block_t *next_block = (block_t *) ((uint8_t *) block - get_size(block));
    return next_block
}

static block_t *get_forward(block_t *block){
    block_t *next_block = (block_t *) ((uint8_t *) block + get_size(block));
    return next_block
}

free_block_t *mm_init(block_t *block) {
    free_block_t *node = {.header = block->header, .prev = NULL, .next = NULL, .footer = block->header}
    return node;
}
*/

/** Extracts a block's size from its header */
static size_t get_size(block_t *block) {
    return block->header & ~1;
}

static size_t *get_footer(block_t *block, size_t size){
    size_t *footer = (size_t *) ((uint8_t *) block + size - sizeof(size_t));
    return footer;
}

/** Set's a block's header with the given size and allocation state */
static void set_header(block_t *block, size_t size, bool is_allocated) {
    block->header = size | is_allocated;
    size_t *footer = get_footer(block, size);
    *(footer) = size | is_allocated;
    
}

/** Extracts a block's allocation state from its header */
static bool is_allocated(block_t *block) {
    return block->header & 1;
}

/**
 * Finds the first free block in the heap with at least the given size.
 * If no block is large enough, returns NULL.
 */
static block_t *find_fit(size_t size) {
    // Traverse the blocks in the heap using the implicit list
    for (block_t *curr = mm_heap_first; mm_heap_last != NULL && curr <= mm_heap_last;
         curr = (void *) curr + get_size(curr)) {
        // If the block is free and large enough for the allocation, return it
        if (!is_allocated(curr) && get_size(curr) >= size) {
            return curr;
        }
    }
    return NULL;
}

/** Gets the header corresponding to a given payload pointer */
static block_t *block_from_payload(void *ptr) {
    return ptr - offsetof(block_t, payload);
}

/**
 * mm_init - Initializes the allocator state
 */
bool mm_init(void) {
    // We want the first payload to start at ALIGNMENT bytes from the start of the heap
    void *padding = mem_sbrk(ALIGNMENT - sizeof(block_t));
    if (padding == (void *) -1) {
        return false;
    }

    // Initialize the heap with no blocks
    mm_heap_first = NULL;
    mm_heap_last = NULL;
    return true;
}

void block_coalescing() {
    block_t *curr = mm_heap_first;
    while (mm_heap_last != NULL && curr <= mm_heap_last) {
        block_t *next = (void *) curr + get_size(curr);
        if (!is_allocated(curr)) {
            if (curr == mm_heap_last) {
                return;
            }
            while (!is_allocated(next)) {
                if (next == mm_heap_last) {
                    set_header(curr, get_size(curr) + get_size(next), false);
                    mm_heap_last = curr;
                    return;
                }
                set_header(curr, get_size(curr) + get_size(next), false);
                next = (void *) curr + get_size(curr);
            }
        }
        curr = next;
    }
}

block_t *block_splitting(block_t *big_block, size_t size_to_use) {
    block_t *temp = big_block;
    size_t remains = get_size(big_block);
    set_header(big_block, size_to_use, true);
    block_t *shortened_block = (block_t *) ((uint8_t *) big_block + size_to_use);
    set_header(shortened_block, remains - size_to_use, false);
    if (temp == mm_heap_last) {
        mm_heap_last = shortened_block;
    }
    return big_block;
}
/**
 * mm_malloc - Allocates a block with the given size
 */
void *mm_malloc(size_t size) {
    block_coalescing();
    // The block must have enough space for a header and be 16-byte aligned
    size = round_up(sizeof(block_t) + size + sizeof(size_t), ALIGNMENT);

    // If there is a large enough free block, use it
    block_t *block = find_fit(size);
    if (block != NULL) {
        if (get_size(block) > (size + sizeof(block_t) + sizeof(size_t))) {
            block_t *hm_block = block_splitting(block, size);
            return hm_block->payload;
        }
        set_header(block, get_size(block), true);
        return block->payload;
    }

    // Otherwise, a new block needs to be allocated at the end of the heap
    block = mem_sbrk(size);
    if (block == (void *) -1) {
        return NULL;
    }

    // Update mm_heap_first and mm_heap_last since we extended the heap
    if (mm_heap_first == NULL) {
        mm_heap_first = block;
    }
    mm_heap_last = block;

    // Initialize the block with the allocated size
    set_header(block, size, true);
    return block->payload;
}

/**
 * mm_free - Releases a block to be reused for future allocations
 */
void mm_free(void *ptr) {
    // mm_free(NULL) does nothing
    if (ptr == NULL) {
        return;
    }

    // Mark the block as unallocated
    block_t *block = block_from_payload(ptr);
    set_header(block, get_size(block), false);
}

/**
 * mm_realloc - Change the size of the block by mm_mallocing a new block,
 *      copying its data, and mm_freeing the old block.
 */
void *mm_realloc(void *old_ptr, size_t size) {
    if (old_ptr == NULL) {
        return mm_malloc(size);
    }
    if (size == 0) {
        mm_free(old_ptr);
        return NULL;
    }
    if (old_ptr != NULL) {
        uint8_t *payload = mm_malloc(size);
        size_t amount = get_size(block_from_payload(old_ptr));
        if (size < get_size(block_from_payload(old_ptr))) {
            amount = size;
        }
        memcpy(payload, old_ptr, amount);
        mm_free(old_ptr);
        return payload;
    }
    return NULL;
}

/**
 * mm_calloc - Allocate the block and set it to zero.
 */
void *mm_calloc(size_t nmemb, size_t size) {
    uint8_t *payload = mm_malloc(size * nmemb);
    memset(payload, 0, size * nmemb);
    return payload;
}

/**
 * mm_checkheap - So simple, it doesn't need a checker!
 */
void mm_checkheap(void) {
    for (block_t *curr = mm_heap_first; mm_heap_last != NULL && curr <= mm_heap_last;
         curr = (void *) curr + get_size(curr)) {
            size_t *footer = get_footer(curr, get_size(curr));
            if (*(footer) != curr->header ){
                printf("\nFOOTER NOT EQUAL TO HEADER");
            }
        }
}