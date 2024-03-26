#include <stdbool.h>

#include "allocator_impl.h"
#include "tree.h"

#define BLOCK_OCCUPIED (size_t)0x1
#define BLOCK_LAST (size_t)0x2

/* Structure that represent a memory block used by the memory allocator
 */
typedef struct {
    size_t size_curr;	// Size of the block
    size_t size_prev;	// Size of the previous block
    size_t offset;	// Offset of the block from the start of the arena
    //bool flag_busy;
    //bool flag_first;
    //bool flag_last;
} Block;

#define BLOCK_STRUCT_SIZE ROUND_BYTES(sizeof(Block))
#define BLOCK_SIZE_MIN ROUND_BYTES(sizeof(tree_node_type))

// Function that splits memory block into two blocks
Block *block_split(Block *, size_t);

// Function that merges two adjacent memory blocks
void block_merge(Block *, Block *);

// Function that unmaps pages that are not required within a memory block
void block_dontneed(Block *);


// Function that converts a block pointerrrr to a payload pointer
static inline void *
block_to_payload(const Block *block)
{
    return (char *)block + BLOCK_STRUCT_SIZE;
}

// Function that converts a payload pointer a block pointer
static inline Block *
payload_to_block(const void *ptr)
{
    return (Block *)((char *)ptr - BLOCK_STRUCT_SIZE);
}

// Function that converts a block pointer to a tree node pointer
static inline tree_node_type *
block_to_node(const Block *block)
{
    return block_to_payload(block);
}

// Function that converts a tree node pointer to a block pointer
static inline Block *
node_to_block(const tree_node_type *node)
{
    return payload_to_block(node);
}

// Function that sets the current size of the block
static inline void
block_set_size_curr(Block *block, size_t size)
{
    size_t flags = block->size_curr & (BLOCK_OCCUPIED | BLOCK_LAST);
    block->size_curr = size | flags;
}

// Function that returns current size of the block
static inline size_t
block_get_size_curr(const Block *block)
{
    return block->size_curr & ~(BLOCK_OCCUPIED | BLOCK_LAST);
}

// Function that sets the previous size of the block
static inline void
block_set_size_prev(Block *block, size_t size)
{
    block->size_prev = size;
}

// Function that returns the previous size of the block
static inline size_t
block_get_size_prev(const Block *block)
{
    return block->size_prev;
}

// Function that sets flag 'busy' for the block
static inline void
block_set_flag_busy(Block *block)
{
    block->size_curr |= BLOCK_OCCUPIED;
}

// Function that checks if the block is busy
static inline bool
block_get_flag_busy(const Block *block)
{
    return (block->size_curr & BLOCK_OCCUPIED) != 0;
}

// Function that clears the 'busy' flag for the block
static inline void
block_clr_flag_busy(Block *block)
{
    block->size_curr &= ~(BLOCK_OCCUPIED);
}

// Function that checks if the block is the first one in arena
static inline bool
block_get_flag_first(const Block *block)
{
    return (block->size_prev == 0) ? true : false;
}


// Function that sets flag 'last' on the block
static inline void
block_set_flag_last(Block *block)
{
    block->size_curr |= BLOCK_LAST;
}

// Function that checks if the block is the last one in arena
static inline bool
block_get_flag_last(const Block *block)
{
    return (block->size_curr & BLOCK_LAST) != 0;
}

// Function that clears the 'last' flag for the block
static inline void
block_clr_flag_last(Block *block)
{
    block->size_curr &= ~(BLOCK_LAST);
}

// Function that sets the offset of the block
static inline void block_set_offset(Block* block, size_t offset) {
    block->offset = offset;
}

// Function that returns the offset of tje block
static inline size_t block_get_offset(Block* block) {
    return block->offset;
}

// Function that returns a pointer to the next block in the arena
static inline Block *
block_next(const Block *block)
{
    return (Block *)
        ((char *)block + BLOCK_STRUCT_SIZE + block_get_size_curr(block));
}

// Function that returns a pointer to the previous blovk in arena
static inline Block *
block_prev(const Block *block)
{
    return (Block *)
        ((char *)block - BLOCK_STRUCT_SIZE - block_get_size_prev(block));
}

// Function that initializes the block by clearing the flags
static inline void
arena_init(Block *block, size_t size)
{
    block->size_curr = size;
    block->size_prev = 0;
    block->offset = 0;
    block_set_flag_last(block);
}

static inline void
block_init(Block *block)
{
    block_clr_flag_busy(block);
    block_clr_flag_last(block);
}
