#include "kernel/types.h"
#include "user/user.h"

int main() {
    struct vmstats st;
    int total_pages = 30000; 
    
    printf("1. Allocating %d pages using sbrk()...\n", total_pages);
    char *mem = sbrk(total_pages * 4096);
    if(mem == (char*)-1) {
        printf("sbrk failed\n");
        exit(1);
    }

    printf("2. Writing to pages (Forcing page replacement & evictions)...\n");
    for(int i = 0; i < total_pages; i++) {
        mem[i * 4096] = (char)(i % 256); 
    }

    printf("3. Reading data back (Triggering faults & reusing evicted pages)...\n");
    int errors = 0;
    for(int i = 0; i < total_pages; i++) {
        if(mem[i * 4096] != (char)(i % 256)) {
            errors++;
        }
    }

    if(errors == 0) {
        printf("-> SUCCESS: All reused swapped pages maintained data integrity!\n");
    } else {
        printf("-> FAILURE: %d pages corrupted.\n", errors);
    }

    getvmstats(getpid(), &st);
    printf("\n--- VM STATS FOR PAGING TEST ---\n");
    printf("Page Faults     : %d\n", st.page_faults);
    printf("Pages Evicted   : %d\n", st.pages_evicted);
    printf("Pages Swapped In: %d\n", st.pages_swapped_in);
    printf("Resident Pages  : %d\n", st.resident_pages);
    
    exit(0);
}