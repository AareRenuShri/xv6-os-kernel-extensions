#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PAGE 4096

// Helper to use your unified syscall
void print_stats(char *label) {
    struct diskstats ds;
    if(getdiskstats(&ds) < 0) {
        printf("Error: getdiskstats failed\n");
        return;
    }
    printf("\n--- %s ---\n", label);
    printf("  Reads:        %lu\n", ds.disk_reads);
    printf("  Writes:       %lu\n", ds.disk_writes);
    printf("  Avg Latency:  %lu\n", ds.avg_latency);
}

// ── Test 1: RAID Write Overhead ─────────────────────────────────────────────
void test_raid_overhead() {
    printf("\n=== Test 1: RAID Write Overhead ===\n");
    
    int npages = 80;
    char *p = sbrk(npages * PAGE);

    // Initial write to trigger swap-outs
    for(int i = 0; i < npages; i++) p[i * PAGE] = (char)i;
    
    print_stats("Current RAID Stats");
    printf("Requirement Check:\n");
    printf("- RAID 0: ~320 writes (80 pgs * 4 blocks)\n");
    printf("- RAID 5: ~640 writes (Data + Parity blocks)\n");
}

// ── Test 2: SSTF Seek Optimization ──────────────────────────────────────────
void test_seek_optimization() {
    printf("\n=== Test 2: Seek Optimization ===\n");
    
    int n = 200;
    char *p = sbrk(n * PAGE);

    // 1. Test FCFS
    setdisksched(0); 
    // Access in a "bouncing" pattern to maximize seek distance
    for(int i = 0; i < n/2; i++) {
        p[i * PAGE] = 'a';
        p[(n - 1 - i) * PAGE] = 'a'; 
    }
    print_stats("FCFS Performance");

    // 2. Test SSTF
    setdisksched(1);
    for(int i = 0; i < n/2; i++) {
        p[i * PAGE] = 'b';
        p[(n - 1 - i) * PAGE] = 'b';
    }
    print_stats("SSTF Performance");

    printf("\nAnalysis: SSTF 'avg_latency' should be lower than FCFS.\n");
}

// ── Test 3: Priority Tie-Breaking ───────────────────────────────────────────
void test_priority_scheduling() {
    printf("\n=== Test 3: Priority Tie-Breaking ===\n");
    setdisksched(1); // Ensure SSTF is on

    int pid = fork();
    if(pid == 0) {
        // Child: Let it drop in MLFQ priority by spinning
        for(int i = 0; i < 500000; i++); 
        char *p = sbrk(50 * PAGE);
        for(int i = 0; i < 50; i++){
            printf("Child writing in page : %d\n",i);
            p[i*PAGE] = 1;
        } 
        exit(0);
    } else {
        // Parent: High Priority
        char *p = sbrk(50 * PAGE);
        for(int i = 0; i < 50; i++){
            printf("Parent writing in page : %d\n",i);

            p[i*PAGE] = 2;
        } 
        wait(0);
        printf("Priority test finished. Check kernel logs for 'SSTF_PICK' order.\n");
    }
}

int main(void) {
    printf("PA4 Performance Benchmark\n");
    printf("Target: RAID 0/5 and SSTF Scheduler\n");

    test_raid_overhead();
    test_seek_optimization();
    test_priority_scheduling();

    printf("\nBenchmark Complete.\n");
    exit(0);
}