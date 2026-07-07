#include "kernel/types.h"
#include "user/user.h"

int main() {
    printf("Starting Time-Synced MLFQ Priority Test...\n");

    // Lowered to 2,000. 
    printf("[Parent] Allocating initial 2,000 pages...\n");
    int parent_initial = 2000;
    char *p_mem = sbrk(parent_initial * 4096);
    for(int i = 0; i < parent_initial; i++) p_mem[i * 4096] = 'P';

    int pid = fork();

    if(pid == 0) {
        // CHILD PROCESS
        // Child ALREADY has 2,000 pages copied from the parent!
        printf("[Child] Allocating 10,000 pages...\n");
        int child_pages = 10000;
        char *c_mem = sbrk(child_pages * 4096);
        for(int i = 0; i < child_pages; i++) c_mem[i * 4096] = 'C';

        printf("[Child] Burning CPU for a few seconds to drop MLFQ priority...\n");
        for(int j = 0; j < 15; j++) {
            for(volatile int i = 0; i < 100000000; i++);
        }

        struct vmstats st;
        getvmstats(getpid(), &st);
        printf("--> [Child] (Low Priority) Pages Evicted: %d\n", st.pages_evicted);
        exit(0);
        
    } else {
        // PARENT PROCESS
        pause(30); 

        printf("[Parent] Waking up at High Priority. Allocating 16,000 overflow pages...\n");
        int parent_overflow = 16000;
        char *p_mem_2 = sbrk(parent_overflow * 4096);
        for(int i = 0; i < parent_overflow; i++) p_mem_2[i * 4096] = 'O';

        struct vmstats st;
        getvmstats(getpid(), &st);
        printf("--> [Parent] (High Priority) Pages Evicted: %d\n", st.pages_evicted);

        wait(0); 
        exit(0);
    }
}