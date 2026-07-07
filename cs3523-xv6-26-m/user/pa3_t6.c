#include "kernel/types.h"
#include "user/user.h"

void child_work(int id) {
    int pages = 10000;
    char *mem = sbrk(pages * 4096);
    if(mem == (char*)-1) {
        printf("Child %d sbrk failed!\n", id);
        exit(1);
    }

    // Write a unique character for this specific child
    for(int i = 0; i < pages; i++) {
        mem[i * 4096] = 'A' + id;
    }
    
    // Read it back to ensure another process didn't overwrite our swapped pages
    int errors = 0;
    for(int i = 0; i < pages; i++) {
        if(mem[i * 4096] != 'A' + id) errors++;
    }
    
    struct vmstats st;
    getvmstats(getpid(), &st);
    printf("Child %d -> Evictions: %d, Faults: %d, Corruptions: %d\n", 
           id, st.pages_evicted, st.page_faults, errors);
    exit(0);
}

int main() {
    printf("Starting Concurrent Paging Stress Test...\n");
    
    for(int i = 1; i <= 3; i++) {
        if(fork() == 0) {
            child_work(i);
        }
    }
    
    // Parent waits for all 3 children to finish surviving the memory pressure
    for(int i = 0; i < 3; i++) {
        wait(0);
    }
    
    printf("Concurrent test complete. OS survived!\n");
    exit(0);
}