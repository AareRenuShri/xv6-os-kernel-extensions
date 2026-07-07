#include "kernel/types.h"
#include "user/user.h"

int main() {
    printf("--- PA4_t4: Priority Eviction Accuracy ---\n");
    int pid = fork();
    if(pid < 0) { printf("fork failed\n"); exit(1); }

    if(pid == 0) {
        // Child: CPU‑bound (will be demoted to high MLFQ level = low priority)
        sbrk(500 * 4096);                         // 500 pages
        char *base = (char*) sbrk(0) - 500 * 4096;
        printf("CPU‑bound child (pid %d) touching all 500 pages...\n", getpid());
        for(int i = 0; i < 500; i++)
            base[i * 4096] = (char) i;           // force page fault + allocation

        // Busy loop to get demoted in scheduler
        for(volatile int j = 0; j < 100000000; j++);
        exit(0);
    } else {
        // Parent: I/O‑bound (stays at high priority = low MLFQ level)
        sbrk(40 * 4096);
        char *base = (char*) sbrk(0) - 40 * 4096;
        printf("IO‑bound parent (pid %d) touching its 40 pages...\n", getpid());
        for(int i = 0; i < 40; i++)
            base[i * 4096] = (char) i;

        pause(30);  // sleep, allowing child to get demoted and pages to be swapped out

        struct vmstats cs, ps;
        getvmstats(pid, &cs);
        getvmstats(getpid(), &ps);
        wait(0);

        printf("\n--- Eviction Results ---\n");
        printf("CPU‑bound child evictions: %d\n", cs.pages_evicted);
        printf("IO‑bound parent evictions: %d\n", ps.pages_evicted);
        if(cs.pages_evicted > ps.pages_evicted)
            printf("PASS: priority eviction works.\n");
        else if(cs.pages_evicted == 0 && ps.pages_evicted == 0)
            printf("WARN: no evictions, increase allocation.\n");
        else
            printf("FAIL: low‑priority process wasn’t evicted more.\n");
    }
    exit(0);
}