#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096 
// We allocate 100 pages. Assuming your NFRAME is around 50, 
// this forces the OS to run out of memory and start swapping.
#define PAGES_TO_ALLOCATE 100 

int main() {
    printf("\n--- Starting Phase 5 Swap Stress Test ---\n");

    char *memory[PAGES_TO_ALLOCATE];

    printf("1. Allocating %d pages to force RAM exhaustion...\n", PAGES_TO_ALLOCATE);
    for(int i = 0; i < PAGES_TO_ALLOCATE; i++) {
        // sbrk(PGSIZE) asks the OS to grow the process memory by 1 page
        memory[i] = sbrk(PGSIZE); 
        
        if(memory[i] == (char*)-1) {
            printf("CRITICAL ERROR: sbrk failed at page %d. Your kalloc/evict is broken.\n", i);
            exit(1);
        }

        // We write a specific byte pattern to the page. 
        // This triggers the hardware PTE_A bit and dirties the page.
        memory[i][0] = (char)(i % 256); 
    }
    printf("-> Write Phase Complete! The Fake Disk should be full of data.\n");

    printf("2. Reading back all pages to force Page Faults (Phase 5)...\n");
    int failed = 0;
    for(int i = 0; i < PAGES_TO_ALLOCATE; i++) {
        
        // Reading this memory will trigger a cause 13 Page Fault.
        // usertrap() should catch it, read the fake disk, and resume.
        char val = memory[i][0]; 
        
        if(val != (char)(i % 256)) {
            printf("ERROR: Data corruption at page %d. Expected %d, got %d\n", i, (i%256), val);
            failed = 1;
        }
    }

    if(failed) {
        printf("FAILED: Data did not survive the round-trip to the fake disk.\n");
    } else {
        printf("SUCCESS: All %d pages survived the round-trip!\n", PAGES_TO_ALLOCATE);
        printf("WARNING: Prepare for a Kernel Panic from uvmunmap in 3... 2... 1...\n");
    }

    // This exit call triggers Phase 6 (which doesn't exist yet)
    exit(0);
}