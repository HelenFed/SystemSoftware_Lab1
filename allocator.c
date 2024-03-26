#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#include "allocator.h"
#include "block.h"
#include "config.h"
#include "allocator_impl.h"
#include "kernel.h"

#define ARENA_SIZE (ALLOCATOR_ARENA_PAGES * ALLOCATOR_PAGE_SIZE)
#define BLOCK_SIZE_MAX (ARENA_SIZE - BLOCK_STRUCT_SIZE)

static tree_type blocks_tree = TREE_INITIALIZER;


/* Function arena_alloc() allocates memory from the kernel for the arena.
 * If the requested size > max block size, it directly allocates the requested size.
 * Otherwise, it allocates the entire arena size.
 * It takes size of the memory to allocate as paremeter and returns pointer to the allocated memory block.
 */
static Block* arena_alloc(size_t size) {
    Block *block;

    if (size > BLOCK_SIZE_MAX) {
        block = kernel_alloc(size);
        if (block != NULL) {
            arena_init(block, size - BLOCK_STRUCT_SIZE);
        }
    } else {
        block = kernel_alloc(ARENA_SIZE);
        if (block != NULL) {
            arena_init(block, ARENA_SIZE - BLOCK_STRUCT_SIZE);
        }
    }
    return block;
}

// Function that adds a block to the binary search tree
static void tree_add_block(Block* block) {
    assert(block_get_flag_busy(block) == false);
    tree_add(&blocks_tree, block_to_node(block), block_get_size_curr(block));
}

// Function that removes a block from the binary search tree
static void tree_remove_block(Block* block) {
    assert(block_get_flag_busy(block) == false);
    tree_remove(&blocks_tree, block_to_node(block));

}

/* Function mem_alloc() allocates memory of the specified size.
 * If the requested size exceeds the maximum block size, it allocates memory directly from the kernel.
 * In other case, it searched for a suitable block in the binary tree.
 * If no suitable block is found, it allocates memory from the arena.
 * It takes size of memory to allocate as a parameter.
 * If the allocation is successful, the function returns pointer to the allocated memory block.
 * If the allocation failed, the function returns NULL. */
void* mem_alloc(size_t size) {
    Block *block, *block_r;
    tree_node_type *node;

    if (size > BLOCK_SIZE_MAX) {
        if (size > SIZE_MAX - (ALIGN - 1)) {
            return NULL;	// Overflow, return NULL
        }
	// Calculate the size needed for the arena and allocate memory from the kernel
        size_t arena_size = (ROUND_BYTES(size) / ALLOCATOR_PAGE_SIZE) * ALLOCATOR_PAGE_SIZE + BLOCK_STRUCT_SIZE;
        block = arena_alloc(arena_size);
        return block_to_payload(block);	// Return payload of the allocated block
    }

    // If the size is less than minimum block size, resize the requested size for it to be at least minimum
    if (size < BLOCK_SIZE_MIN) {
        size = BLOCK_SIZE_MIN;
    }

    // Align the requested size to meet memory alignment requirenments
    size_t aligned_size = ROUND_BYTES(size);

    // Search for the fir block in the binary search tree
    node = tree_find_best(&blocks_tree, aligned_size);

    // If not suitable block found, allocate memory from arena
    if (node == NULL) {
        block = arena_alloc(aligned_size);

	// If arena allocation fails, return NULL
        if (block == NULL) {
            return NULL;
        }

    } else {
	// If the suitable block to allocate memory to has been found, then remove it from the tree
        tree_remove(&blocks_tree, node);

	// Convert tree node to a block
        block = node_to_block(node);
    }

    // Perform block splitting if necessary and add remaining block to the tree
    block_r = block_split(block, aligned_size);
    if (block_r != NULL) {
        tree_add_block(block_r);
    }
    return block_to_payload(block);	// Return payload of the allocated block
}

// Function that shows information about a node in the binary search tree.
static void show_node(const tree_node_type *node, const bool linked) {
    Block* block = node_to_block(node);
    printf("[%20p] %10zu %10zu %s %s %s %s\n", (void*)block,
    block_get_size_curr(block),
    block_get_size_prev(block),
    block_get_flag_busy(block) ? "busy" : "free",
    block_get_flag_first(block) ? "first" : "",
    block_get_flag_last(block) ? "last" : "",
    linked ? "linked" : "");
}

// Function that displays current state of the memory blocks
void mem_show(const char *msg) {
    printf("%s:\n", msg);
    if (tree_is_empty(&blocks_tree)) {
        printf("Tree is empty\n");
    } else {
        tree_walk(&blocks_tree, show_node);
    }
}

/* Function mem_free() frees the memory block pointed to by ptr.
 * If the ptr is NULL, the function returns without doing anything/
 * Otherwise, it marks the block as unoccupied and if possible, merges adjacent free blocks.
 * If the size of the block > max block size, it directly releases the memory in kernel.
 * Otherwise, it add the block back to the tree and does memory trimming if needed.*/
void mem_free(void *ptr) {
    Block *block, *block_r, *block_l;

    // If ptr is NULL, return without doing anything
    if (ptr == NULL) {
        return;
    }

    // Convert payload pointer to block pointer
    block = payload_to_block(ptr);

    // Clear 'busy' flag
    block_clr_flag_busy(block);

    // If the size of the block > max block size, it directly releases the memory in kernel.
    if (block_get_size_curr(block) > BLOCK_SIZE_MAX) {
        kernel_free(block, block_get_size_curr(block) + BLOCK_STRUCT_SIZE);
    } else {
	// Otherwise, perform block merging and add the block to the tree
        if (!block_get_flag_last(block)) {
            block_r = block_next(block);
	    // If the block is not the last block in the arena, attempt to merge with the next block
            if (!block_get_flag_busy(block_r)) {
                tree_remove_block(block_r);
                block_merge(block, block_r);
            }
        }
	// If the block is not the first block  in the arena, attempt to merge with the previous block
        if (!block_get_flag_first(block)) {
            block_l = block_prev(block);
            if (!block_get_flag_busy(block_l)) {
                tree_remove_block(block_l);
                block_merge(block_l, block);
		
		// Update block pointer to the merged block
                block = block_l;
            }
        }

	// If the block is both the first and last block in the arena, free the entire arena
        if (block_get_flag_first(block) && block_get_flag_last(block)) {
            kernel_free(block, ARENA_SIZE);
        } else {
	    // Otherwise, trim meory and add the block back to the tree
            block_dontneed(block);
            tree_add_block(block);
        }
    }
}


/* Function mem_realloc() resizes tje memory block pointed to bu ptr1 to the specified size.
 * If the ptr1 is NULL, then the function call mem_alloc() function.
 * If requested size = current size, but the current size is > max block size, then allocate a new block
 * of the requested size and copy contents of the old block to the new one before freeing the old block.
 * Otherwise, just return the pt1 as is.
 * If requested size > current size, the function will try to expand the block in place.
 * If there is enough space in adjacent block, then it will merge tham and split the newly merged block.
 * If there is not enough space even in adjacent block, it allocates a new block and copies contents,
 * before freeing the old block
 */
void* mem_realloc(void* ptr1, size_t size) {
    void *ptr2;
    Block* block1, *block_r, *block_n;
    size_t size_curr;

    // Make the requested size at least possble minimum
    if (size < BLOCK_SIZE_MIN) {
        size = BLOCK_SIZE_MIN;
    }

    size = ROUND_BYTES(size);

    // If ptr1 is NULL, allocate a new memory block of the given size
    if (ptr1 == NULL) {
        return mem_alloc(size);
    }

    block1 = payload_to_block(ptr1);
    size_curr = block_get_size_curr(block1);

    // If the current block size exceeds the maximum block size
    if (size_curr > BLOCK_SIZE_MAX) {
	// If the requested size is the same as the current size, return ptr1
        if (size == size_curr) {
            return ptr1;
        }
	// Allocate a new block and move the contents
        goto move_large_block;
    }

    // If the requested size is the same as the current size, return ptr1
    if (size == size_curr) {
        return ptr1;
    }

    // If the requested size is smaller than current size, then decrease the size of the block
    if (size < size_curr) {
        if (!block_get_flag_last(block1)) {
	    // Split the block to the requested size
            block_r = block_split(block1, size);
            if (block_r != NULL) {
                block_n = block_next(block_r);
                if (!block_get_flag_busy(block_n)) {
		    // Remove next block from tree
                    tree_remove_block(block_n);

		    // Merge blocks
                    block_merge(block_r, block_n);
                }

		// Add ne block to the tree
                tree_add_block(block_r);
                return block_to_payload(block1);    // Return payload pointer of the original block
            }
        }
    }

    // If the requested size is bigger than the current size, then increase the block size
    if (size > size_curr) {
        if (!block_get_flag_last(block1)) {
            block_r = block_next(block1);
            if (!block_get_flag_busy(block_r)) {
                size_t total_size = size_curr + block_get_size_curr(block_r) + BLOCK_STRUCT_SIZE;
                if (total_size >= size) {
                    tree_remove_block(block_r);
                    block_merge(block1, block_r);
                    block_n = block_split(block1, size);
                    if (block_n != NULL) {
                        tree_add_block(block_n);
                    }
                    return block_to_payload(block1);
                }
            }
        }
    }

move_large_block:
    ptr2 = mem_alloc(size);	// Allocate a new block of requested size
    if (ptr2 != NULL) {
	// Copy contents of the old block to the new block and free the old block
        memcpy(ptr2, ptr1, size_curr < size ? size_curr : size);
        mem_free(ptr1);
    }
    return ptr2;    // Return the pointer to the new memory block
}
