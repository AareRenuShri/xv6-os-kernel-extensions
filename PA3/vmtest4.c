#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096
#define NUMPAGES 60

void print_stats(int pid) {
    struct vmstats info;
    if(getvmstats(pid, &info) < 0) {
        printf("getvmstats failed\n");
        return;
    }
    printf("  page_faults:       %d\n", info.page_faults);
    printf("  pages_evicted:     %d\n", info.pages_evicted);
    printf("  pages_swapped_in:  %d\n", info.pages_swapped_in);
    printf("  pages_swapped_out: %d\n", info.pages_swapped_out);
    printf("  resident_pages:    %d\n", info.resident_pages);
}

int main() {
    printf("---- Test 4: Reusing evicted pages (swap-in) ----\n");
    int pid = getpid();

    char *mem = sbrklazy(NUMPAGES * PGSIZE);
    if(mem == SBRK_ERROR) {
        printf("sbrklazy failed\n");
        exit(1);
    }
    printf("Pass 1: forward access (0 -> 59)...\n");

    for(int i = 0; i < NUMPAGES; i++)
        mem[i * PGSIZE] = (char)i;
    printf("After pass 1:\n");
    print_stats(pid);

    struct vmstats before, after;
    getvmstats(pid, &before);

    printf("Pass 2: backward access (59 -> 0)...\n");
    for(int i = NUMPAGES - 1; i >= 0; i--)
        mem[i * PGSIZE] = (char)(i + 1); 

    printf("After pass 2:\n");
    print_stats(pid);

    printf("Pass 3: verifying new values...\n");
    int mismatches = 0;
    for(int i = 0; i < NUMPAGES; i++) {
        if(mem[i * PGSIZE] != (char)(i + 1)) {
            printf("MISMATCH at page %d\n", i);
            mismatches++;
        }
    }

    getvmstats(pid, &after);
    int new_swaps = after.pages_swapped_in - before.pages_swapped_in;
    printf("Swap-ins during pass 2: %d\n", new_swaps);

    if(mismatches == 0 && new_swaps > 0)
        printf("PASS: swap-in works correctly\n");
    else if(mismatches > 0)
        printf("FAIL: %d data mismatches\n", mismatches);
    else
        printf("WARN: no swap-ins observed (pages may not have been evicted)\n");

    exit(0);
}