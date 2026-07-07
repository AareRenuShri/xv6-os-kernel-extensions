#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    printf("\n=== Phase 1: VM Stats Infrastructure Test ===\n\n");

    struct vmstats stats;
    int pid = getpid();

    printf("Testing getvmstats for PID %d...\n", pid);

    // Test 1: Valid PID
    if (getvmstats(pid, &stats) < 0) {
        printf("[FAIL] getvmstats system call returned an error!\n");
        exit(1);
    }

    printf("[PASS] getvmstats executed successfully.\n\n");

    printf("--- Current VM Stats for PID %d ---\n", pid);
    printf("Page Faults:       %d\n", stats.page_faults);
    printf("Pages Evicted:     %d\n", stats.pages_evicted);
    printf("Pages Swapped In:  %d\n", stats.pages_swapped_in);
    printf("Pages Swapped Out: %d\n", stats.pages_swapped_out);
    printf("Resident Pages:    %d\n", stats.resident_pages);
    printf("-----------------------------------\n\n");

    // Basic sanity check: since we just booted this process and haven't
    // implemented the actual tracking logic yet, these should all be exactly 0.
    if (stats.page_faults == 0 && stats.pages_evicted == 0 && stats.resident_pages == 0) {
        printf("[PASS] Stats are properly initialized to 0.\n");
    } else {
        printf("[WARN] Stats contain garbage data. Did you forget to initialize them to 0 in allocproc()?\n");
    }

    // Test 2: Invalid PID
    if (getvmstats(99999, &stats) < 0) {
        printf("[PASS] Invalid PID correctly rejected (-1 returned).\n");
    } else {
        printf("[FAIL] Invalid PID was accepted!\n");
    }

    printf("\n=== Phase 1 Test Complete ===\n");
    exit(0);
}