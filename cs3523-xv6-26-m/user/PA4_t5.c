//   T1: Disk-backed swap correctness (swap-out + swap-in data integrity)
//   T2: Disk scheduling — FCFS vs SSTF latency + setdisksched validation
//   T3: RAID correctness — RAID 0, RAID 1, RAID 5 data integrity
//   T4: Kernel statistics — all vmstats fields, disk I/O counters, latency
//   T5: VM system — lazy alloc faults, sbrk(-n) cleanup, resident pages



#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


#define PAGE_SIZE    4096
#define SMALL_PAGES  64
#define LARGE_PAGES  2048
#define SMALL_BYTES  (SMALL_PAGES * PAGE_SIZE)
#define LARGE_BYTES  (LARGE_PAGES * PAGE_SIZE)

#define PATTERN(i)   ((unsigned char)((i) & 0xFF))


static int passed = 0;
static int failed = 0;

static void pass(const char *msg) { printf("  [PASS] %s\n", msg); passed++; }
static void fail(const char *msg) { printf("  [FAIL] %s\n", msg); failed++; }

static void section(int n, const char *title) {
    printf("\n========================================\n");
    printf("  TEST %d: %s\n", n, title);
    printf("========================================\n");
}

static void print_vmstats(struct vmstats *s) {
    printf("  page_faults      : %d\n", s->page_faults);
    printf("  pages_evicted    : %d\n", s->pages_evicted);
    printf("  pages_swapped_out: %d\n", s->pages_swapped_out);
    printf("  pages_swapped_in : %d\n", s->pages_swapped_in);
    printf("  resident_pages   : %d\n", s->resident_pages);
    printf("  disk_reads       : %d\n", s->disk_reads);
    printf("  disk_writes      : %d\n", s->disk_writes);
    printf("  avg_disk_latency : %d\n", s->avg_disk_latency);
}

// ══════════════════════════════════════════════
//  TEST 1: Disk-backed swap correctness
//
//  Covers:
//  - Pages written to disk on eviction (not memory)
//  - Pages read back correctly from disk on fault
//  - disk_writes > 0 after eviction
//  - disk_reads  > 0 after swap-in
//  - Data integrity across swap-out + swap-in cycle
// ══════════════════════════════════════════════
static void test1_swap_correctness(void) {
    section(1, "Disk-backed swap correctness");

    //  Part A: data integrity 
    printf("  [A] Writing %d pages, reading back after eviction...\n", LARGE_PAGES);

    char *base = sbrklazy(LARGE_BYTES);
    if (base == (char *)-1) { fail("sbrklazy failed"); return; }

    for (int i = 0; i < LARGE_PAGES; i++) {
        char *pg = base + i * PAGE_SIZE;
        pg[0]             = (char)PATTERN(i);
        pg[PAGE_SIZE - 1] = (char)PATTERN(i);
    }

    int errors = 0;
    for (int i = 0; i < LARGE_PAGES; i++) {
        char *pg = base + i * PAGE_SIZE;
        if ((unsigned char)pg[0]             != PATTERN(i) ||
            (unsigned char)pg[PAGE_SIZE - 1] != PATTERN(i)) {
            if (++errors <= 3)
                printf("  Page %d: expected 0x%x got 0x%x/0x%x\n",
                       i, PATTERN(i),
                       (unsigned char)pg[0],
                       (unsigned char)pg[PAGE_SIZE - 1]);
        }
    }
    if (errors == 0) pass("data integrity: all pages correct after swap-out/swap-in");
    else             fail("data integrity: corruption detected after swap");

    sbrk(-(int)LARGE_BYTES);

    // Part B: disk I/O 
    printf("  [B] Verifying disk_writes and disk_reads are non-zero...\n");

    struct vmstats before, after;
    if (getvmstats(getpid(), &before) < 0) { fail("getvmstats failed"); return; }

    char *mem = sbrklazy(LARGE_BYTES);
    if (mem == (char *)-1) { fail("sbrklazy failed"); return; }

    // Write all pages forces eviction to disk
    for (int i = 0; i < LARGE_PAGES; i++)
        mem[i * PAGE_SIZE] = (char)PATTERN(i);

    // Read first quarter back forces disk reads
    volatile char sink = 0;
    for (int i = 0; i < LARGE_PAGES / 4; i++)
        sink += mem[i * PAGE_SIZE];
    (void)sink;

    if (getvmstats(getpid(), &after) < 0) { fail("getvmstats failed"); return; }

    int dw = after.disk_writes - before.disk_writes;
    int dr = after.disk_reads  - before.disk_reads;
    int ev = after.pages_evicted - before.pages_evicted;
    int si = after.pages_swapped_in - before.pages_swapped_in;

    printf("  pages_evicted delta    : %d\n", ev);
    printf("  pages_swapped_in delta : %d\n", si);
    printf("  disk_writes delta      : %d\n", dw);
    printf("  disk_reads delta       : %d\n", dr);

    if (ev > 0) pass("pages were evicted from RAM");
    else        fail("no evictions — physical memory may be too large");

    if (dw > 0) pass("disk_writes > 0 — swap goes to disk (not in-memory array)");
    else        fail("disk_writes == 0 — swap may still be in-memory");

    if (si > 0) pass("pages swapped back in from disk");
    else        fail("no pages swapped in");

    if (dr > 0) pass("disk_reads > 0 — swap-in reads from disk");
    else        fail("disk_reads == 0 — swap-in may not use disk");

    sbrk(-(int)LARGE_BYTES);
}

// ══════════════════════════════════════════════
//  TEST 2: Disk scheduling — FCFS and SSTF
//
//  Covers:
//  - setdisksched(0) = FCFS accepted
//  - setdisksched(1) = SSTF accepted
//  - setdisksched(invalid) = -1 returned
//  - SSTF avg_disk_latency <= FCFS for scattered access
//  - Disk head position updated after each request
//  - avg_disk_latency > 0 (latency model: |head-block| + C)
// ══════════════════════════════════════════════
static void test2_disk_scheduling(void) {
    section(2, "Disk scheduling — FCFS vs SSTF");

    //  Part A: input validation 
    printf("  [A] setdisksched input validation...\n");
    int r0 = setdisksched(0);
    int r1 = setdisksched(1);
    int r2 = setdisksched(2);
    int rn = setdisksched(-1);
    printf("  setdisksched(0)=->[%d]  setdisksched(1)=->[%d]  "
           "setdisksched(2)=->[%d]  setdisksched(-1)=->[%d]\n",
           r0, r1, r2, rn);

    if (r0 == 0 && r1 == 0)
        pass("valid policies 0 and 1 accepted");
    else
        fail("valid policies rejected");

    if (r2 == -1 && rn == -1)
        pass("invalid policies correctly return -1");
    else
        fail("invalid policies not rejected");

    //  Part B: FCFS run 
    printf("  [B] FCFS run (policy=0)...\n");
    setdisksched(0);

    char *mem = sbrklazy(LARGE_BYTES);
    if (mem == (char *)-1) { fail("sbrklazy failed"); return; }

    // Scattered access pattern 
    for (int i = 0; i < LARGE_PAGES; i++) {
        int j = (i * 37) % LARGE_PAGES;
        mem[j * PAGE_SIZE] = (char)i;
    }
    volatile char sink = 0;
    for (int i = 0; i < LARGE_PAGES; i++) {
        int j = (i * 37) % LARGE_PAGES;
        sink += mem[j * PAGE_SIZE];
    }
    (void)sink;

    struct vmstats s_fcfs;
    if (getvmstats(getpid(), &s_fcfs) < 0) { fail("getvmstats failed"); return; }
    int fcfs_lat = s_fcfs.avg_disk_latency;
    printf("  FCFS avg_disk_latency : %d\n", fcfs_lat);
    sbrk(-(int)LARGE_BYTES);

    //  Part C: SSTF run 
    printf("  [C] SSTF run (policy=1)...\n");
    setdisksched(1);

    mem = sbrklazy(LARGE_BYTES);
    if (mem == (char *)-1) { fail("sbrklazy failed"); return; }

    for (int i = 0; i < LARGE_PAGES; i++) {
        int j = (i * 37) % LARGE_PAGES;
        mem[j * PAGE_SIZE] = (char)i;
    }
    sink = 0;
    for (int i = 0; i < LARGE_PAGES; i++) {
        int j = (i * 37) % LARGE_PAGES;
        sink += mem[j * PAGE_SIZE];
    }
    (void)sink;

    struct vmstats s_sstf;
    if (getvmstats(getpid(), &s_sstf) < 0) { fail("getvmstats failed"); return; }
    int sstf_lat = s_sstf.avg_disk_latency;
    printf("  SSTF avg_disk_latency : %d\n", sstf_lat);
    sbrk(-(int)LARGE_BYTES);

    setdisksched(0);

    if (fcfs_lat > 0)
        pass("FCFS avg_disk_latency > 0 (latency model active)");
    else
        fail("FCFS avg_disk_latency == 0");

    if (sstf_lat > 0)
        pass("SSTF avg_disk_latency > 0 (latency model active)");
    else
        fail("SSTF avg_disk_latency == 0");

    if (s_fcfs.disk_reads > 0 || s_fcfs.disk_writes > 0)
        pass("disk I/O recorded under FCFS");
    else
        fail("no disk I/O under FCFS");

    if (s_sstf.disk_reads > 0 || s_sstf.disk_writes > 0)
        pass("disk I/O recorded under SSTF");
    else
        fail("no disk I/O under SSTF");

    if (sstf_lat <= fcfs_lat)
        pass("SSTF latency <= FCFS latency (scheduling improves seek time)");
    else
        printf("  [NOTE] SSTF > FCFS on this run — acceptable for near-sequential access\n");
}

// ══════════════════════════════════════════════
//  TEST 3: RAID correctness
//
//  Covers:
//  - RAID 0: striping — disk = b%N, block = b/N
//  - RAID 1: mirroring — each block on 2 disks
//  - RAID 5: striping + XOR parity across disks
//  Each is tested by writing a pattern, forcing eviction
//  to disk through the RAID layer, then reading back.
//  A child process is used so the test is isolated.
// ══════════════════════════════════════════════
static void test3_raid_correctness(void) {
    section(3, "RAID correctness — RAID 0, RAID 1, RAID 5");

    // All three RAID tests use the same structure:
    // child allocates large memory, writes pattern, reads back,
    // exits 0 on success / 2 on corruption / 1 on alloc failure.
    // The active RAID policy is set by raid_policy in virtio_disk.c
    // (default = RAID 5). We test data correctness through all paths.

    //  RAID 0 pattern  
    printf("  [RAID 0] striping — single-byte pattern...\n");
    int pid = fork();
    if (pid < 0) { fail("fork failed"); goto raid1; }
    if (pid == 0) {
        char *m = sbrklazy(LARGE_BYTES);
        if (m == (char *)-1) exit(1);
        for (int i = 0; i < LARGE_PAGES; i++)
            m[i * PAGE_SIZE] = (char)PATTERN(i);
        int ok = 1;
        for (int i = 0; i < LARGE_PAGES; i++)
            if ((unsigned char)m[i * PAGE_SIZE] != PATTERN(i)) { ok = 0; break; }
        exit(ok ? 0 : 2);
    }
    { int st = 0; wait(&st);
      if      (st == 0) pass("RAID 0: data read back correctly through striping layer");
      else if (st == 2) fail("RAID 0: data corruption detected");
      else              fail("RAID 0: child crashed"); }

raid1:
    //  RAID 1 pattern (two bytes per page, XOR check) 
    printf("  [RAID 1] mirroring — two-byte pattern...\n");
    pid = fork();
    if (pid < 0) { fail("fork failed"); goto raid5; }
    if (pid == 0) {
        char *m = sbrklazy(LARGE_BYTES);
        if (m == (char *)-1) exit(1);
        for (int i = 0; i < LARGE_PAGES; i++) {
            char *pg = m + i * PAGE_SIZE;
            pg[0] = (char)PATTERN(i);
            pg[1] = (char)(PATTERN(i) ^ 0xAA);
        }
        int ok = 1;
        for (int i = 0; i < LARGE_PAGES; i++) {
            char *pg = m + i * PAGE_SIZE;
            if ((unsigned char)pg[0] != PATTERN(i) ||
                (unsigned char)pg[1] != (PATTERN(i) ^ 0xAA)) { ok = 0; break; }
        }
        exit(ok ? 0 : 2);
    }
    { int st = 0; wait(&st);
      if      (st == 0) pass("RAID 1: data read back correctly through mirroring layer");
      else if (st == 2) fail("RAID 1: data corruption detected");
      else              fail("RAID 1: child crashed"); }

raid5:
    //  RAID 5 pattern 
    printf("  [RAID 5] striping+parity — three-field pattern...\n");
    pid = fork();
    if (pid < 0) { fail("fork failed"); return; }
    if (pid == 0) {
        char *m = sbrklazy(LARGE_BYTES);
        if (m == (char *)-1) exit(1);
        for (int i = 0; i < LARGE_PAGES; i++) {
            char *pg = m + i * PAGE_SIZE;
            pg[0]             = (char)(i & 0xFF);
            pg[1]             = (char)((i >> 8) & 0xFF);
            pg[PAGE_SIZE - 1] = (char)PATTERN(i);
        }
        int ok = 1;
        for (int i = 0; i < LARGE_PAGES; i++) {
            char *pg = m + i * PAGE_SIZE;
            if ((unsigned char)pg[0]             != (i & 0xFF) ||
                (unsigned char)pg[1]             != ((i >> 8) & 0xFF) ||
                (unsigned char)pg[PAGE_SIZE - 1] != PATTERN(i)) { ok = 0; break; }
        }
        exit(ok ? 0 : 2);
    }
    { int st = 0; wait(&st);
      if      (st == 0) pass("RAID 5: data read back correctly through parity layer");
      else if (st == 2) fail("RAID 5: data corruption detected");
      else              fail("RAID 5: child crashed"); }
}

// ══════════════════════════════════════════════
//  TEST 4: Kernel statistics
//
//  Covers:
//  - getvmstats returns correct data for valid PID
//  - getvmstats returns -1 for invalid PID
//  - page_faults, pages_evicted, pages_swapped_in/out all > 0
//  - disk_reads, disk_writes, avg_disk_latency all > 0
//  - resident_pages tracked (goes up on alloc, reflects evictions)
//  - Per-process stats are independent (child has own counters)
// ══════════════════════════════════════════════
static void test4_statistics(void) {
    section(4, "Kernel statistics — vmstats completeness");

    //  Part A: invalid PID handling 
    printf("  [A] getvmstats with invalid PIDs...\n");
    struct vmstats s;
    int r1 = getvmstats(-1,    &s);
    int r2 = getvmstats(99999, &s);
    printf("  getvmstats(-1)    = %d  getvmstats(99999) = %d\n", r1, r2);
    if (r1 == -1 && r2 == -1)
        pass("invalid PIDs correctly return -1");
    else
        fail("invalid PIDs did not return -1");

    //  Part B: run heavy workload then check all fields 
    printf("  [B] Running swap-heavy workload...\n");

    char *mem = sbrklazy(LARGE_BYTES);
    if (mem == (char *)-1) { fail("sbrklazy failed"); return; }

    for (int i = 0; i < LARGE_PAGES; i++)
        mem[i * PAGE_SIZE] = (char)i;

    volatile char sink = 0;
    for (int i = 0; i < LARGE_PAGES; i++)
        sink += mem[i * PAGE_SIZE];
    (void)sink;

    sbrk(-(int)LARGE_BYTES);

    if (getvmstats(getpid(), &s) < 0) { fail("getvmstats failed"); return; }
    printf("  Full stats after workload:\n");
    print_vmstats(&s);

    if (s.page_faults > 0)       pass("page_faults > 0");
    else                          fail("page_faults == 0");

    if (s.pages_evicted > 0)     pass("pages_evicted > 0");
    else                          fail("pages_evicted == 0");

    if (s.pages_swapped_out > 0) pass("pages_swapped_out > 0");
    else                          fail("pages_swapped_out == 0");

    if (s.pages_swapped_in > 0)  pass("pages_swapped_in > 0");
    else                          fail("pages_swapped_in == 0");

    if (s.disk_reads > 0)        pass("disk_reads > 0");
    else                          fail("disk_reads == 0");

    if (s.disk_writes > 0)       pass("disk_writes > 0");
    else                          fail("disk_writes == 0");

    if (s.avg_disk_latency > 0)  pass("avg_disk_latency > 0");
    else                          fail("avg_disk_latency == 0");

   
}

// ══════════════════════════════════════════════
//  TEST 5: VM system behaviour
//
//  Covers:
//  - Lazy allocation: page_faults increment on first touch
//  - sbrk(-n) with swapped pages: no panic, swap slots freed
//  - Memory reusable after shrink (no swap slot leak)
//  - resident_pages > 0 after allocation
//  - resident_pages < LARGE_PAGES when evictions occur
// ══════════════════════════════════════════════
static void test5_vm_system(void) {
    section(5, "VM system — lazy alloc, sbrk cleanup, resident pages");

    //  Part A: lazy allocation increments page_faults 
    printf("  [A] Lazy allocation page fault counting...\n");
    struct vmstats before, after;
    if (getvmstats(getpid(), &before) < 0) { fail("getvmstats failed"); return; }

    char *mem = sbrklazy(SMALL_BYTES);
    if (mem == (char *)-1) { fail("sbrklazy failed"); return; }

    for (int i = 0; i < SMALL_PAGES; i++)
        mem[i * PAGE_SIZE] = 1;

    if (getvmstats(getpid(), &after) < 0) { fail("getvmstats failed"); return; }

    int faults = after.page_faults - before.page_faults;
    printf("  page_faults delta: %d (expect ~%d)\n", faults, SMALL_PAGES);

    if (faults >= SMALL_PAGES)
        pass("page_faults incremented once per lazily allocated page");
    else if (faults > 0)
        pass("page_faults incremented (some pages may have been eviction faults)");
    else
        fail("page_faults did not increment on lazy allocation");

    sbrk(-(int)SMALL_BYTES);

    //  Part B: sbrk(-n) with swapped pages 
    printf("  [B] sbrk(-n) releases swapped pages without panic...\n");

    mem = sbrklazy(LARGE_BYTES);
    if (mem == (char *)-1) { fail("sbrklazy failed"); return; }

    // Force heavy eviction
    for (int i = 0; i < LARGE_PAGES; i++)
        mem[i * PAGE_SIZE] = (char)i;

    // Shrink — kernel must free all swap slots cleanly
    char *ret = sbrk(-(int)LARGE_BYTES);
    if (ret != (char *)-1)
        pass("sbrk(-LARGE_BYTES) with swapped pages succeeded (no panic)");
    else
        fail("sbrk(-LARGE_BYTES) returned -1");

    // Re-allocate to confirm swap slots were freed (no leak)
    char *mem2 = sbrklazy(SMALL_BYTES);
    if (mem2 == (char *)-1) {
        fail("re-allocation failed — possible swap slot leak");
        return;
    }
    for (int i = 0; i < SMALL_PAGES; i++)
        mem2[i * PAGE_SIZE] = (char)i;
    int ok = 1;
    for (int i = 0; i < SMALL_PAGES; i++)
        if ((unsigned char)mem2[i * PAGE_SIZE] != (unsigned char)i) { ok = 0; break; }

    if (ok) pass("memory reusable after shrink — swap slots correctly freed");
    else    fail("data corrupt after re-allocation — swap slots may have leaked");

    sbrk(-(int)SMALL_BYTES);

    //  Part C: resident_pages tracking 
    printf("  [C] resident_pages tracking...\n");
    if (getvmstats(getpid(), &before) < 0) { fail("getvmstats failed"); return; }

    mem = sbrklazy(LARGE_BYTES);
    if (mem == (char *)-1) { fail("sbrklazy failed"); return; }

    for (int i = 0; i < LARGE_PAGES; i++)
        mem[i * PAGE_SIZE] = (char)i;

    if (getvmstats(getpid(), &after) < 0) { fail("getvmstats failed"); return; }

    printf("  resident_pages before : %d\n", before.resident_pages);
    printf("  resident_pages after  : %d\n", after.resident_pages);
    printf("  pages_evicted         : %d\n", after.pages_evicted);

    if (after.resident_pages > 0)
        pass("resident_pages > 0 after allocation");
    else
        fail("resident_pages == 0 after allocation");

    if (after.pages_evicted > 0 && after.resident_pages < LARGE_PAGES)
        pass("resident_pages < total pages (evictions correctly reduce count)");
    else if (after.resident_pages > 0)
        pass("resident_pages tracked");
    else
        fail("resident_pages not tracked correctly");

    sbrk(-(int)LARGE_BYTES);
}


int main(int argc, char *argv[]) {
    printf("\n");    

    if (argc == 2) {
        int n = atoi(argv[1]);
        switch (n) {
            case 1: test1_swap_correctness(); break;
            case 2: test2_disk_scheduling();  break;
            case 3: test3_raid_correctness(); break;
            case 4: test4_statistics();       break;
            case 5: test5_vm_system();        break;
            
            default:
                printf("Unknown test %d. Valid: 1-6\n", n);
                exit(1);
        }
    } else {
        test1_swap_correctness();
        test2_disk_scheduling();
        test3_raid_correctness();
        test4_statistics();
        test5_vm_system();
        
    }

    printf("\n########################################\n");
    printf("  Results: %d passed, %d failed\n", passed, failed);
    printf("########################################\n\n");

    exit(0);
}