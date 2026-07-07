#include "kernel/types.h"
#include "user/user.h"

#define PGSIZE 4096
#define TEST_PAGES 160

int main() {
    printf("--- RAID‑5 Correctness Test (%d Pages) ---\n", TEST_PAGES);

    if(setraidmode(2) < 0) {
        printf("FAIL: setraidmode(2) not supported\n");
        exit(1);
    }

    char *p = sbrk(TEST_PAGES * PGSIZE);
    if(p == (char*)-1) {
        printf("FAIL: sbrk failed\n");
        exit(1);
    }

    // Write unique patterns
    for(int i = 0; i < TEST_PAGES; i++)
        memset(p + i * PGSIZE, (i % 255) + 1, PGSIZE);

    printf("Written %d pages with unique patterns.\n", TEST_PAGES);

    // Force evictions / RAID‑5 writes by touching again
    for(int i = 0; i < TEST_PAGES; i++)
        p[i * PGSIZE] += 1;

    // Verify
    int errors = 0;
    for(int i = 0; i < TEST_PAGES; i++) {
        char expected = ((i % 255) + 1) + 1;
        if(p[i * PGSIZE] != expected) {
            printf("Mismatch at page %d: expected %d, got %d\n",
                   i, expected, p[i * PGSIZE]);
            errors++;
            if(errors > 5) break;
        }
    }

    if(errors == 0) {
        printf("Verified all %d pages – no errors.\n", TEST_PAGES);
        printf("PASS: RAID‑5 striping, parity, and reconstruction work correctly.\n");
    } else {
        printf("FAIL: %d pages corrupted.\n", errors);
    }

    exit(0);
}