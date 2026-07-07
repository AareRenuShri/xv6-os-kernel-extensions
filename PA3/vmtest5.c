#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096
#define NUMPAGES 55

void print_stats(const char *label, int pid) {
    struct vmstats info;
    if(getvmstats(pid, &info) < 0) {
        printf("getvmstats failed for %s\n", label);
        return;
    }
    printf("%s (pid=%d):\n", label, pid);
    printf("  page_faults:       %d\n", info.page_faults);
    printf("  pages_evicted:     %d\n", info.pages_evicted);
    printf("  pages_swapped_out: %d\n", info.pages_swapped_out);
    printf("  resident_pages:    %d\n", info.resident_pages);
}

int main() {
    printf("---- Test 5: Scheduler-aware eviction ----\n");
    printf("Lower MLFQ level process should lose pages first\n\n");

    int child = fork();
    if(child == 0) {
        printf("Child (pid=%d): burning CPU to get demoted...\n", getpid());
        volatile long x = 0;
        for(long i = 0; i < 100000000L; i++) x++;

        int level = getlevel();
        printf("Child demoted to MLFQ level %d\n", level);

        char *mem = sbrklazy(NUMPAGES * PGSIZE);
        for(int i = 0; i < NUMPAGES; i++)
            mem[i * PGSIZE] = (char)i;

        print_stats("Child (lower priority)", getpid());
        exit(0);
    } else {
        wait(0);
        char *mem = sbrklazy(NUMPAGES * PGSIZE);
        for(int i = 0; i < NUMPAGES; i++)
            mem[i * PGSIZE] = (char)i;

        int plevel = getlevel();
        printf("Parent MLFQ level: %d\n", plevel);
        print_stats("Parent (higher priority)", getpid());

        printf("\nExpected: child had more evictions than parent\n");
    }
    exit(0);
}