#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Test Case 1 : Lazy Allocation and Resident Pages
void baseline_test() {
    
    printf("TEST CASE 1 : Lazy Allocation and Resident Pages\n");
    
    int pid = fork();

    if (pid == 0) {
        // Child Process: Allocate pages and check resident page count
        
        struct vmstats stats_before, stats_after;
        getvmstats(getpid(),  &stats_before);

        // Ask for 10 pages of memory
        char *mem = sbrklazy(10 * 4096);

        // Touch 5 of these pages
        for (int i = 0; i < 5; i++) {
            mem[i * 4096] = 'X'; // Access the page to trigger lazy allocation
        }

        getvmstats(getpid(), &stats_after);

        int faults = stats_after.page_faults - stats_before.page_faults;
        int resident = stats_after.resident_pages - stats_before.resident_pages;

        // We expect 5 page faults and 5 resident pages
        if (faults == 5 && resident == 5) {
            printf("CASE 1 (SUCCESS) - 5 page faults and 5 resident pages as expected.\n");
            exit(0);
        } else {
            printf("CASE 1 (FAILED) - Faults: %d, Resident Diff: %d, Evictions: %d\n", faults, resident, stats_after.pages_evicted);
            exit(1);
        }

    } else {
        // Parent Process: Wait for child to finish
        wait(0);
    }

}

// Test Case 2 : API Sanity checks and zombie processes
void api_sanity_test() {
    
    printf("TEST CASE 2 : API Sanity checks and zombie processes\n");
    
    int pid = fork();

    if (pid == 0) {
        
        struct vmstats stats; // Uninitialized stats struct

        // Case 2A: Invalid PID
        if (getvmstats(99999, &stats) == -1) {
            printf("CASE 2A (SUCCESS) - Properly handled invalid PID.\n");
        } else {
            printf("CASE 2A (FAILED) - Did not handle invalid PID correctly.\n");
            exit(1);
        }

        // Case 2B: NULL pointer
        if (getvmstats(getpid(), 0) == -1) {
            printf("CASE 2B (SUCCESS) - Properly handled NULL pointer.\n");
        } else {
            printf("CASE 2B (FAILED) - Did not handle NULL pointer correctly.\n");
            exit(1);
        }

        // Case 2C: Zombie process
        int child_pid = fork();
        if (child_pid == 0) {
            // Grandchild process: Exit immediately to become a zombie
            char *mem = sbrklazy(4096);
            mem[0] = 'Z'; // Touch the page to ensure it's allocated
            exit(0);
        } else {
            // Child process: Wait for grandchild to exit and become a zombie
            pause(10); // Wait for a short time to ensure the grandchild has exited and become a zombie

            // Now the child is a zombie, try to get stats for it
            if (getvmstats(child_pid, &stats) == 0 && stats.page_faults > 0) {
                printf("CASE 2C (SUCCESS) - Properly handled zombie process.\n");
            } else {
                printf("CASE 2C (FAILED) - Failed to read stats for zombie process.\n");
            }

            wait(0); // Clean up the zombie process
            exit(0);
        }

    } else {
        // Parent Process: Wait for child to finish
        wait(0);
    }

}

void swap_test() {

    printf("TEST CASE 3 : Memory Crisis, Eviction, and Rescue\n");
    int pid = fork();

    if (pid == 0) {
        struct vmstats stats;
        char *base = sbrklazy(0); // Remember the exact address we started at
        int pages_allocated = 0;

        printf("  [Child] Allocating memory until RAM is full... (This may take a few seconds)\n");

        // 1. Force the Crisis (Exhaust physical memory)
        while (1) {
            char *mem = sbrklazy(4096);
            if (mem == (char*)-1) {
                printf("  [Child] sbrk failed!\n");
                break;
            }
            
            // Write "MAGIC" to the page so it's physically mapped
            mem[0] = 'M'; mem[1] = 'A'; mem[2] = 'G'; mem[3] = 'I'; mem[4] = 'C';
            pages_allocated++;

            // Check if we pushed the Clock hand hard enough to evict something
            getvmstats(getpid(), &stats);
            
            // Hit the brakes! Stop exactly at 20 evictions so we don't overflow the 4MB swap disk!
            if (stats.pages_evicted >= 20) {
                break;
            }
        }

        printf("  [Child] RAM full! Pages Evicted: %d, Swapped Out: %d\n", stats.pages_evicted, stats.pages_swapped_out);

        if (stats.pages_evicted == 0) {
            printf("CASE 3A (FAILED) - Could not trigger eviction.\n");
            exit(1);
        }

        // 2. The Rescue Mission (Swap In)
        // Our Clock algorithm scans in a circle. The first pages we allocated (at the 'base' pointer) 
        // are guaranteed to be the ones it kicked out. Let's read them back!
        printf("  [Child] Reading back original pages to force Swap In...\n");
        
        for (int i = 0; i < pages_allocated; i++) {
            char *page = base + (i * 4096);
            // Read the magic data back (This triggers the PTE_SWAP vmfault!)
            if (page[0] != 'M' || page[1] != 'A' || page[2] != 'G' || page[3] != 'I' || page[4] != 'C') {
                printf("CASE 3B (FAILED) - Data corruption detected on swap in!\n");
                exit(1);
            }
        }

        // Check stats one last time
        getvmstats(getpid(), &stats);
        printf("  [Child] Final Stats -> Swapped In: %d\n", stats.pages_swapped_in);

        if (stats.pages_swapped_in > 0) {
            printf("CASE 3 (SUCCESS) - Successfully evicted, swapped out, and rescued data without corruption!\n");
            exit(0);
        } else {
            printf("CASE 3C (FAILED) - Eviction happened, but Swap In failed to trigger.\n");
            exit(1);
        }

    } else {
        wait(0);
    }

}

// Test Case 4 : Priority Showdown with SC-MLFQ and Clock Eviction
void priority_test() {
    printf("TEST CASE 4 : SC-MLFQ Priority Showdown\n");

    int pid_a = fork();
    if (pid_a == 0) {
        // CHILD A (The Hog - Target: Level 3)
        char *mem = sbrklazy(40 * 4096);
        for(int i = 0; i < 40; i++) mem[i * 4096] = 'A'; 

        while(1) {
            // Massive CPU loop to ensure it drops to Level 3 and stays there
            volatile int x = 0;
            for(volatile int i = 0; i < 50000; i++) x = x + 1;
        }
    }

    int pid_b = fork();
    if (pid_b == 0) {
        // CHILD B (The VIP - Target: Level 0)
        char *mem = sbrklazy(40 * 4096);
        for(int i = 0; i < 40; i++) mem[i * 4096] = 'B'; 

        while(1) {
            // Spam system calls and yield to ensure it stays at Level 0
            getpid();
            getpid();
            getpid();
            getpid();
        }
    }

    // Give the scheduler time to demote Child A and stabilize
    pause(20);

    int pid_c = fork();
    if (pid_c == 0) {
        // CHILD C (The Crusher)
        printf("  [Crusher] Squeezing memory to force evictions...\n");
        struct vmstats stats_a, stats_b;
        int total_allocs = 0;

        while(1) {
            char *mem = sbrklazy(4096);
            if (mem == (char*)-1) break; // RAM completely full
            mem[0] = 'C';
            total_allocs++;

            // Check the stats of A and B every 100 allocations
            if (total_allocs % 100 == 0) {
                getvmstats(pid_a,&stats_a);
                getvmstats(pid_b, &stats_b);

                // Stop the crusher once the Clock algorithm has done enough damage
                if (stats_a.pages_evicted >= 10 || stats_b.pages_evicted >= 10) {
                    break;
                }
            }
        }
        exit(0);
    }

    // Parent waits for Crusher to finish its job
    wait(0);

    // Fetch the final aftermath stats
    struct vmstats final_a, final_b;
    getvmstats(pid_a,&final_a);
    getvmstats(pid_b, &final_b);

    printf("  [Result] Child A (CPU-Bound Level 3) Evictions: %d\n", final_a.pages_evicted);
    printf("  [Result] Child B (Interactive Level 0) Evictions: %d\n", final_b.pages_evicted);

    if (final_a.pages_evicted > final_b.pages_evicted) {
        printf("CASE 4 (SUCCESS) - Clock algorithm correctly targeted the lower priority process!\n");
    } else {
        printf("CASE 4 (FAILED) - Priorities were not respected.\n");
    }

    // Cleanup the infinite loops using standard xv6 kill()
    kill(pid_a);
    kill(pid_b);
    wait(0); // Reap A
    wait(0); // Reap B
}

int main(void) {

    printf("Starting vmstats tests...\n");

    baseline_test();
    api_sanity_test();
    swap_test();
    priority_test();

    printf("vmstats tests completed.\n");
    exit(0);

}