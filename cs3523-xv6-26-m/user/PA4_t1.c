#include "kernel/types.h"
#include "user/user.h"

#define PGSIZE 4096
#define NPAGES 150

int main() {
    printf("--- PA4_t1: Swap Correctness (%d Pages) ---\n", NPAGES);

    char *p = sbrk(NPAGES * PGSIZE);
    if(p == (char*)-1) {
        printf("FAIL: sbrk failed\n");
        exit(1);
    }

    // Write unique patterns
    for(int i = 0; i < NPAGES; i++)
        p[i * PGSIZE] = i % 256;

    printf("Written %d pages with unique patterns.\n", NPAGES);

    // Read back and verify
    int errors = 0;
    for(int i = 0; i < NPAGES; i++) {
        if(p[i * PGSIZE] != (char)(i % 256)) {
            printf("Mismatch at page %d: expected %d, got %d\n",
                   i, i % 256, p[i * PGSIZE]);
            errors++;
            if(errors > 5) break;
        }
    }

    if(errors == 0) {
        printf("Verified all %d pages – no errors.\n", NPAGES);
        printf("PASS: Disk-backed swap correctly wrote and read back all pages.\n");
    } else {
        printf("FAIL: %d pages corrupted after swap.\n", errors);
    }

    exit(0);
}