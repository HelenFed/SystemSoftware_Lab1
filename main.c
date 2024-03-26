#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "allocator.h"
#include "tester.h"
#include "block.h"
#include "tree.h"
#include "avl/avl_impl.h"


int main(void)
{
    void *ptr1, *ptr2, *ptr3, *ptr4, *ptr5;

    ptr1 = mem_alloc(100000);
    mem_show("First allocated block constitutes an arena that is bigger than the max block size");
    printf("Allocated memory of arena: %zu\n", payload_to_block(ptr1)->size_curr);
    printf("Allocated memory of arena (no flags): %zu\n\n", block_get_size_curr(payload_to_block(ptr1)));

    // A minimum size for allocating a block is 64 bytes, so if we try anything below that, it will alloc 64
    ptr2 = mem_alloc(5);
    printf("Allocated memory for ptr2 : %zu\n", payload_to_block(ptr2)->size_curr);

  
    ptr3 = mem_alloc(543);
    printf("Allocated memory for ptr3: %zu\n", payload_to_block(ptr3)->size_curr);

    ptr4 = mem_alloc(4096);
    printf("Allocated memory for ptr4: %zu\n", payload_to_block(ptr4)->size_curr);

    mem_show("Result of allocations");

    ptr5 = mem_alloc(543);
    printf("\n\nAllocated memory for ptr5: %zu\n\n", payload_to_block(ptr5)->size_curr);

    mem_show("Result of another allocation");

    ptr1 = mem_realloc(ptr1, 80000);
    mem_show("\n\nReallocate ptr1 from 100000 -> 80000");
    
    mem_free(ptr5);
    mem_show("\nFree ptr5");
    printf("\nWhat happaned to ptr5: %zu\n", payload_to_block(ptr5)->size_curr);


    mem_realloc(ptr4, 2543);
    mem_show("\nReallocate ptr4 -> 2543");
    printf("\nNew allocated memory for ptr4: %zu\n", payload_to_block(ptr4)->size_curr);

    //srand(time(NULL));
    //tester(true);
}
