#include <assert.h>

#include "block.h"
#include "config.h"
#include "kernel.h"

/* Function block_split() splits a memory block into two blocks. The original block is marked as busy.
 * If there is not enough space in the original block to accommodate a new block of the specified size,
 * then a new block is created from the remaining space.
 * The function takes pointer to the block that needs to be split and size of memory to allocate from the block
 * as parameters.
 * If the function is successful then it returns pointer to the new block created from remaining space.
 * If it fails beacuse there is not enough space to create a new block from the original, then it returns NULL
 */

Block* block_split(Block *block, size_t size) {
    Block* block_r;	// Pointer to new block created from the remaining space
    size_t size_rest;	// Size of the remaining space after allocation

    // Mark the original block as busy
    block_set_flag_busy(block);

    // Calculate the size of the remaining space after allocation
    size_rest = block_get_size_curr(block) - size;

    // Check if there's enough space in the original block to create a new block
    if (size_rest >= BLOCK_STRUCT_SIZE + BLOCK_SIZE_MIN) {
        size_rest -= BLOCK_STRUCT_SIZE;

	// Set the size of the original block to accommodate the allocated memory
        block_set_size_curr(block, size);

	// Create new block from the remaining space
        block_r = block_next(block);
        block_init(block_r);
        block_set_size_curr(block_r, size_rest);
        block_set_size_prev(block_r, size);

	// Update flags and size of adjacent blocks
        block_set_offset(block_r, block_get_offset(block) + size + BLOCK_STRUCT_SIZE);
        if (block_get_flag_last(block)) {
            block_clr_flag_last(block);
            block_set_flag_last(block_r);
        } else {
            block_set_size_prev(block_next(block_r), size_rest);
        }
        return block_r;	// Return pointer to the new block
    }
    return NULL;	// Return NULL if there's not enough space to create a new block
}


/* Function block_merge() merges two adjacent blocks into a single block.
 * It takes pointer to the first block and pointer to the adjacent block as parameters. */
void
block_merge(Block *block, Block *block_r) {
    assert(block_get_flag_busy(block_r) == false);	// Make sure that the second block is not busy
    assert(block_next(block) == block_r);		// Make sure that the second block is adjacent

    size_t size;

    // Calculate the size of merged block
    size = block_get_size_curr(block) + block_get_size_curr(block_r) + BLOCK_STRUCT_SIZE;
    
    // Update size and flags for the first block
    block_set_size_curr(block, size);

    if (block_get_flag_last(block_r)) {
        block_set_flag_last(block);
    }
    else {
        block_set_size_prev(block_next(block_r), size);
    }

}

#include <stdio.h>


/* Function block_dontneed resets memory regions that are no longer needed to a specific pattern.
 * It's used for optimizing usage and reclaiming unused memory pages.
 * It takes pointer to the block whose memory regions are about to be reset as parameters. */
void block_dontneed(Block* block) {
    size_t size_curr;
    size_t offset, offset1, offset2;

    // Get current size of the block
    size_curr = block_get_size_curr(block);

    // Check if the size of the block is less than a page size
    if (size_curr - sizeof(tree_node_type) < ALLOCATOR_PAGE_SIZE) {
        return;	// Return uf the block size is less than a page size
    }

    // Calculate offsets for resetting memory regions
    offset = block_get_offset(block);

    offset1 = offset + BLOCK_STRUCT_SIZE + sizeof(tree_node_type);
    offset1 = (offset1 + ALLOCATOR_PAGE_SIZE - 1) & ~((size_t)ALLOCATOR_PAGE_SIZE - 1);

    offset2 = offset + size_curr + BLOCK_STRUCT_SIZE;
    offset2 &= ~((size_t)ALLOCATOR_PAGE_SIZE - 1);


    // Check if the block spans across multiple memory pages
    if (offset1 == offset2) {
        return;	// Returb is the block spans actoss only one memory page
    }

    // Assert that the difference between two offsets is a multiple of the page size
    assert(((offset2 - offset1) & ((size_t)ALLOCATOR_PAGE_SIZE - 1)) == 0);

    // Reset memory regions to a specific pattern
    kernel_reset((char*)block + (offset1 - offset), offset2 - offset1);
}
