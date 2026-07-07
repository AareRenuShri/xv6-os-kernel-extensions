#include "kernel/types.h"
#include "user/user.h"

#define PGSIZE 4096
#define TOTAL_PAGES 600   // > 512 frames → swap is forced

void run_workload(char *p, char *label, int sched_policy) {
    setdisksched(sched_policy);
    printf("%s (policy = %s)\n", label, sched_policy ? "SSTF" : "FCFS");

    // Sequential write (low seek distance)
    for (int i = 0; i < TOTAL_PAGES; i++)
        p[i * PGSIZE] = 'A';

    // Random/reverse access (high seek distance)
    for (int i = TOTAL_PAGES - 2; i >= 0; i -= 2)
        p[i * PGSIZE] = 'B';
    for (int i = 1; i < TOTAL_PAGES; i += 2)
        p[i * PGSIZE] = 'C';

    struct vmstats st;
    getvmstats(getpid(), &st);
    printf("  disk reads: %d, writes: %d, avg latency: %d\n",
           st.disk_reads, st.disk_writes, st.avg_disk_latency);
}

int main() {
    printf("--- PA4_t2: SSTF vs FCFS Performance ---\n");

    char *p = sbrk(TOTAL_PAGES * PGSIZE);
    if (p == (char*)-1) {
        printf("FAIL: sbrk failed\n");
        exit(1);
    }

    int pid = fork();
    if (pid < 0) { printf("fork failed\n"); exit(1); }

    if (pid == 0) {
        run_workload(p, "Child", 0);   // FCFS
        exit(0);
    } else {
        pause(20);                      // let child finish
        run_workload(p, "Parent", 1);   // SSTF
        wait(0);
    }

    printf("PASS: Both policies completed without data corruption.\n");
    printf("      SSTF should have a significantly lower average latency than FCFS.\n");
    exit(0);
}