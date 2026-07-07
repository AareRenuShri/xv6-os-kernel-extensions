#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096

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
    printf("---- Test 6: Edge cases ----\n");

    printf("\n-- Case 1: invalid pid --\n");
    struct vmstats info;
    int ret = getvmstats(9999, &info);
    printf("getvmstats(9999) = %d (expected -1): %s\n",
           ret, ret == -1 ? "PASS" : "FAIL");

    // case 2: stats before any allocation
    printf("\n-- Case 2: stats before any allocation --\n");
    print_stats(getpid());

    // case 3: allocate but don't access (no faults expected)
    printf("\n-- Case 3: lazy alloc without access --\n");
    sbrklazy(10 * PGSIZE);
    print_stats(getpid());
    printf("(page_faults should be same as before)\n");

    // case 4: single page fault
    printf("\n-- Case 4: single page fault --\n");
    char *mem = sbrklazy(PGSIZE);
    mem[0] = 42;
    print_stats(getpid());
    printf("(page_faults should have incremented by 1)\n");

    // case 5: child stats accessible by parent
    printf("\n-- Case 5: parent reads child stats --\n");
    int pid = fork();
    if(pid == 0) {
        char *m = sbrklazy(5 * PGSIZE);
        for(int i = 0; i < 5; i++) m[i * PGSIZE] = i;
        exit(0);
    } else {
        wait(0);
        ret = getvmstats(pid, &info);
        printf("getvmstats on exited child = %d\n", ret);
    }
    printf("\n=== Edge case tests done ===\n");
    exit(0);
}