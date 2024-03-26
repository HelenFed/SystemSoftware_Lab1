#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kernel.h"

#define DEBUG_KERNEL_RESET

/* If the kernel_alloc function fails, than the failed_kernel_alloc function will be triggered.
 * The function will print an error message in stderr (standard error - one of the standard streams)
 * and terminate the program with an error code. */

static _Noreturn void
failed_kernel_alloc(void) {
#define msg "Function kernel_alloc() failed - couldn't allocate memory\n"
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
#undef msg
    exit(EXIT_FAILURE);
}


/* If the kernel_free function fails, than the failed_kernel_free function will be triggered.
 * The function will print an error message in stderr (standard error - one of the standard streams)
 * and terminate the program with an error code. */

static _Noreturn void
failed_kernel_free(void) {
#define msg "Function kernel_free() failed - couldn't free memory\n"
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
#undef msg
    exit(EXIT_FAILURE);
}


/* If the kernel_reset function fails, than the failed_kernel_reset function will be triggered.
 * The function will print an error message in stderr (standard error - one of the standard streams)
 * and terminate the program with an error code. */

static _Noreturn void
failed_kernel_reset(void) {
#define msg "Function kernel_reset() failed - couldn't reset values of memory\n"
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
#undef msg
    exit(EXIT_FAILURE);
}

//Conditional code for Unix-based systems

#if !(defined(_WIN32) || defined(_WIN64))

#include <sys/mman.h>
#include <errno.h>

/* kernel_alloc() function allocates memory for the kernel.
 * It uses mmap() system call to obrain anonymous memory that has no file origin.
 * In case of a failure, it returns NULL or calls failed_kernel_alloc() function.
 * If there is not enough memory available (ENOMEM), then the function returns NULL;
 * In case of other possible errors (like invalid arguments), then the function calls failed_kernel_alloc(). */

void *
kernel_alloc(size_t size)
{
    void *ptr;

#if defined(MAP_ANONYMOUS)
# define MMAP_FLAG_ANON MAP_ANONYMOUS
#elif defined(MAP_ANON)
# define MMAP_FLAG_ANON MAP_ANON
#else
# error "Do not know how to get anonymous memory"
#endif
    ptr = mmap(NULL, size, PROT_READ|PROT_WRITE, MMAP_FLAG_ANON|MAP_PRIVATE, -1, 0);
    if (ptr == MAP_FAILED) {
        if (errno == ENOMEM)
            return NULL;
        failed_kernel_alloc();
    }
    return ptr;
}

/* kernel_free() function releases memory previously allocated by kernel_alloc().
 * To do that it uses munmap() system call. */

void
kernel_free(void *ptr, size_t size){
    if (munmap(ptr, size) < 0)
        failed_kernel_free();
} 

/* kernel_reset() function resets the values of memory previously allocated by kernel_alloc().
 * To do that it uses madvice() system call for pre-fetching memory.
 * If the madvice() call fails, it calls the failed_kernel_reset() function. */

void
kernel_reset(void *ptr, size_t size) {
#ifdef DEBUG_KERNEL_RESET
    memset(ptr, 0x7e, size);
#endif
    if (madvise(ptr, size, MADV_DONTNEED) < 0)
        failed_kernel_reset();
}

//Conditional code for Windows
#else
#include <Windows.h>

/* kernel_alloc() function allocates memory for the kernel.
 * It uses VirtualAlloc() function for memory reservation and allocation.
 * In case of a failure, it returns NULL value indicating that allocation was unsucessful.*/

void *
kernel_alloc(size_t size) {
    void *ptr;

    ptr = VirtualAlloc(NULL, size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    if (ptr == NULL) {
        return NULL;
    }
    return ptr;
}


/* kernel_free() function releases memory previously allocated by kernel_alloc().
 * To do that it uses VirtualFree() function for memory release. */

void
kernel_free(void *ptr, size_t size) {
    (void)size;
    if (VirtualFree(ptr, 0, MEM_RELEASE) == 0)
        failed_kernel_free();
}


/* kernel_reset() function resets the values of memory previously allocated by kernel_alloc().
 * To do that it uses VirtualAlloc() function with the MEM_RESET flag for memory resetting.
 * If the VirtualAlloc()  call fails, it calls the failed_kernel_reset() function. */

void
kernel_reset(void *ptr, size_t size) {
#ifdef DEBUG_KERNEL_RESET
    memset(ptr, 0x7e, size);
#endif
    if (ViraulAlloc(ptr, size, MEM_RESET, PAGE_READWRITE) == NULL)
        failed_kernel_reset();
}

#endif /* deined(_WIN32) || defined(_WIN64) */
