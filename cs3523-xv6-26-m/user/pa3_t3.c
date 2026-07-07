#include "kernel/types.h"
#include "user/user.h"

int main() {
    struct vmstats st;
    // Aim for slightly over 128MB. 
    // If xv6 has 32768 pages, let's do 33000.
    int total_pages = 30000; 
    char *m = sbrk(total_pages * 4096);

    printf("Starting... touching %d pages\n", total_pages);
    for(int i = 0; i < total_pages; i++) {
        m[i * 4096] = (char)(i % 256);
        if(i == 32700) printf("Approaching memory limit...\n");
    }

    getvmstats(getpid(), &st);
    printf("\n--- FINAL REPORT ---\n");
    printf("Faults   : %d\n", st.page_faults);
    printf("Resident : %d\n", st.resident_pages);
    printf("Evicted  : %d\n", st.pages_evicted);
    
    exit(0);
}