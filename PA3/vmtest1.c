#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096
#define NUMPAGES 40

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
    printf("---- Test 1: Large allocation + sequential page faults ----\n");
    int pid = getpid();

    printf("Before allocation:\n");
    print_stats(pid);

    char *mem = sbrklazy(NUMPAGES * PGSIZE);
    if(mem == SBRK_ERROR) {
        printf("sbrklazy failed\n");
        exit(1);
    }
    printf("Allocated %d pages lazily (no faults yet)\n", NUMPAGES);
    print_stats(pid);

    printf("Accessing pages sequentially...\n");
    for(int i = 0; i < NUMPAGES; i++) {
        mem[i * PGSIZE] = (char)i;
    }
    printf("After sequential access of %d pages:\n", NUMPAGES);
    print_stats(pid);

    printf("Verifying data...\n");
    int ok = 1;
    for(int i = 0; i < NUMPAGES; i++) {
        if(mem[i * PGSIZE] != (char)i) {
            printf("MISMATCH at page %d\n", i);
            ok = 0;
        }
    }
    if(ok) printf("All data verified correctly\n");
    exit(0);
}