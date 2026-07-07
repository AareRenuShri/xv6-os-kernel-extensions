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
    printf("---- Test 3: Eviction correctness + data integrity ----\n");
    int pid = getpid();

    char *mem = sbrklazy(NUMPAGES * PGSIZE);
    if(mem == SBRK_ERROR) {
        printf("sbrklazy failed\n");
        exit(1);
    }
    printf("Writing to %d pages...\n", NUMPAGES);
    for(int i = 0; i < NUMPAGES; i++) {
        mem[i * PGSIZE] = (char)(i * 3); 
    }
    printf("After write pass:\n");
    print_stats(pid);

    printf("Re-reading all pages (verifying swap-in correctness)...\n");
    int mismatches = 0;
    for(int i = 0; i < NUMPAGES; i++) {
        if(mem[i * PGSIZE] != (char)(i * 3)) {
            printf("MISMATCH at page %d: got %d expected %d\n",
                   i, (int)(unsigned char)mem[i * PGSIZE], (int)(unsigned char)(i * 3));
            mismatches++;
        }
    }
    printf("After re-read pass:\n");
    print_stats(pid);

    if(mismatches == 0)
        printf("PASS: all %d pages read back correctly\n", NUMPAGES);
    else
        printf("FAIL: %d mismatches found\n", mismatches);

    exit(0);
}