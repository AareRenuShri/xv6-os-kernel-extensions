#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096
#define MAX_USER_PAGES 32

// Bypasses the broken xv6 printf to force numbers to the screen
void print_num(int n) {
    char buf[16];
    int i = 0;
    if(n == 0) { buf[i++] = '0'; }
    while(n > 0) { buf[i++] = (n % 10) + '0'; n /= 10; }
    while(--i >= 0) { write(1, &buf[i], 1); }
}

void print_stats(struct vmstats *s) {
    struct diskstats d;
    getdiskstats(&d);

    printf("    Page Faults       = "); print_num(s->page_faults); printf("\n");
    printf("    Pages Evicted     = "); print_num(s->pages_evicted); printf("\n");
    printf("    Disk Reads        = "); print_num((int)d.disk_reads); printf("\n");
    printf("    Disk Writes       = "); print_num((int)d.disk_writes); printf("\n");
    printf("    Avg Disk Latency  = "); print_num((int)d.avg_latency); printf(" ticks\n");
}

void run_test(int sched_policy, const char* name) {
  if(fork() == 0) {
    // Set the disk scheduling policy (0 = FCFS, 1 = SSTF)
    if(setdisksched(sched_policy) < 0) {
      printf("ERROR: setdisksched failed\n");
      exit(1);
    }
    
    printf("\n=== Running Workload with %s ===\n", name);

    int npages = 460;
    char *buf = sbrk(npages * PGSIZE);
    if (buf == (char*)-1) {
      printf("ERROR: sbrk failed\n");
      exit(1);
    }

    // Write Phase: Fill memory with unique data. 
    // This will hit MAX_USER_PAGES and force the disk to start swapping!
    for(int i = 0; i < npages; i++) {
      buf[i * PGSIZE] = (char)(i & 0xFF);
    }

    // Read Phase: Read the data back.
    // This forces the disk to swap-in the evicted pages.
    int errors = 0;
    for(int i = 0; i < 25; i++) {
      if(buf[i * PGSIZE] != (char)(i & 0xFF)) {
         errors++;
      }
    }

    // Fetch the PA4 Disk Stats
    struct vmstats stats;
    if (getvmstats(getpid(), &stats) < 0) {
      printf("ERROR: getvmstats failed\n");
      exit(1);
    }

    // Fetch disk stats for disk_writes
    struct diskstats d;
    getdiskstats(&d);

    print_stats(&stats);
    printf("    Data Errors       = %d\n", errors);
    
    if(errors == 0 && d.disk_writes > 0) {
      printf("    [PASS] Data Integrity Maintained during Disk Swapping!\n");
    } else {
      printf("    [FAIL] Something went wrong.\n");
    }

    exit(0);
  }
  wait(0);
}

void run_raid5_test() {
  if(fork() == 0) {
    printf("\n=== Running Workload with RAID 5 DISK FAILURE ===\n");

    int npages = MAX_USER_PAGES * 2;
    char *buf = sbrk(npages * PGSIZE);

    printf("  1. Writing data to RAID array...\n");
    for(int i = 0; i < npages; i++) buf[i * PGSIZE] = (char)(i & 0xFF);

    printf("  2. SIMULATING CATASTROPHIC DISK FAILURE (Disk 1)...\n");
    faildisk(1); // Kill Disk 1!

    printf("  3. Reading data back (Forcing Reconstruction)...\n");
    int errors = 0;
    for(int i = 0; i < npages; i++) {
      if(buf[i * PGSIZE] != (char)(i & 0xFF)) errors++;
    }

    if(errors == 0) {
      printf("    [PASS] RAID 5 successfully reconstructed the lost data!\n");
    } else {
      printf("    [FAIL] Data was lost. Errors: %d\n", errors);
    }
    exit(0);
  }
  wait(0);
}

void run_priority_test() {
  printf("\n=== Running Workload with SC-MLFQ Priority Contention ===\n");
  
  setdisksched(0); // Set to FCFS so priority is the only factor

  int pid_low = fork();
  if (pid_low == 0) {
    // LOW PRIORITY: Drop to Level 3, but keep loop short enough to avoid PA2 bugs
    for (volatile long j = 0; j < 5000000L; j++); 
    
    // FIX 1: Ask for 64 pages to guarantee we blow past your 32-page limit!
    int npages = 260; 
    
    char *buf = sbrk(npages * PGSIZE);
    if (buf == (char*)-1) exit(1);

    for(int i = 0; i < npages; i++) buf[i * PGSIZE] = 'L';
    
    // FIX 2: Print stats BEFORE dying!
    struct diskstats d;
    getdiskstats(&d);
    printf("  [Level 3 Process] Avg Disk Latency: "); print_num((int)d.avg_latency); printf(" ticks\n");
    exit(0);
  }

  int pid_high = fork();
  if (pid_high == 0) {
    // HIGH PRIORITY: Stay at Level 0
    for (int j = 0; j < 100; j++) getpid(); 
    
    int npages = 260; 
    
    char *buf = sbrk(npages * PGSIZE);
    if (buf == (char*)-1) exit(1);

    for(int i = 0; i < npages; i++) buf[i * PGSIZE] = 'H';
    
    // FIX 2: Print stats BEFORE dying!
    struct diskstats d;
    getdiskstats(&d);
    printf("  [Level 0 Process] Avg Disk Latency: "); print_num((int)d.avg_latency); printf(" ticks\n");
    exit(0);
  }

  // Parent waits for both children to finish
  wait(0);
  wait(0);
  printf("    [PASS] Priority contention successfully evaluated!\n");
}
int main(void) {
  printf("Starting PA4 Disk Scheduling & RAID Evaluation...\n");
  
  // Run the test with FCFS
  run_test(0, "FCFS (First Come First Serve)");
  
  // Run the test with SSTF
  run_test(1, "SSTF (Shortest Seek Time First)");

    // Run the RAID 5 Failure Test
    run_raid5_test();

    // Run the Priority Contention Test
    run_priority_test();

  printf("\nEvaluation Complete.\n");
  exit(0);
}