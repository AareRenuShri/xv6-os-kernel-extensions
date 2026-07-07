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
    printf("---- Test 2: Force page replacement (60 pages, MAX_FRAMES=50) ----\n");
    int pid = getpid();

    char *mem = sbrklazy(NUMPAGES * PGSIZE);
    if(mem == SBRK_ERROR) {
        printf("sbrklazy failed\n");
        exit(1);
    }
    printf("Before access:\n");
    print_stats(pid);

    printf("Accessing %d pages...\n", NUMPAGES);
    for(int i = 0; i < NUMPAGES; i++) {
        mem[i * PGSIZE] = (char)i;
        if(i == 49) {
            printf("At page 50 (MAX_FRAMES reached):\n");
            print_stats(pid);
        }
    }
    printf("After accessing all %d pages:\n", NUMPAGES);
    print_stats(pid);

    struct vmstats info;
    getvmstats(pid, &info);
    if(info.pages_evicted > 0)
        printf("PASS: eviction occurred (%d pages evicted)\n", info.pages_evicted);
    else
        printf("FAIL: no evictions occurred\n");

    exit(0);
}