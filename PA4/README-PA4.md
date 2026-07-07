# PA4: Disk Scheduling and RAID-backed Swap in xv6

## Problem Statement

This assignment replaced PA3's in-memory swap array with a real disk-backed swap system. I implemented two disk scheduling policies — FCFS and SSTF — selectable at runtime via `setdisksched()`, along with a simulated latency model based on seek distance. On top of that, I simulated a 4-disk RAID array supporting RAID 0 (striping), RAID 1 (mirroring), and RAID 5 (striping with XOR parity and reconstruction on read), switchable via `setraidmode()`. Disk scheduling also considers MLFQ priority from PA2 as a tie-breaker. Added disk I/O statistics (`disk_reads`, `disk_writes`, `avg_disk_latency`) to the existing per-process stats.

---
## What I Built

**Disk-backed Swap**
- Replaced the in-memory swap array with a disk-backed mechanism.
- Each evicted page is written to a swap slot (8 disk blocks) on the real disk image.
- Swap-in reads the page back from disk when a page fault occurs on a swapped PTE.

**Disk Scheduling Policies**
- Implemented **FCFS** (policy 0) and **SSTF** (policy 1).
- A pending request queue (`NDISKREQS = 128`) holds disk I/O requests.
- `run_disk_queue()` selects the next request per the active policy.
- `setdisksched(policy)` switches policies at runtime.

**RAID-backed Storage**
- Simulated **4 identical disks** with separate logical regions per RAID mode.
- **RAID 0 (striping)**: logical block `b` → disk `b % 4`, block `b / 4`.
- **RAID 1 (mirroring)**: each block written to two disks; reads from the primary disk.
- **RAID 5 (striping with distributed parity)**: data blocks XORed to compute parity, distributed via `parity_disk = row % N`; parity updates handled correctly on writes; missing blocks reconstructed on read.
- `setraidmode(mode)` switches RAID mode (0/1/2) at runtime. Each swap slot records the RAID mode it was written under, so correctness holds even after mode changes.

**Disk Latency Model**
- `latency = |current_block − requested_block| + ROTATIONAL_DELAY` (delay = 10).
- Head position updates after every disk I/O; per-process average latency is tracked.

**Priority-aware Scheduling**
- The MLFQ level (`cur_q_level`) of the requesting process is attached to disk requests.
- SSTF uses priority as a tie-breaker (lower priority number = more urgent).
- The clock eviction algorithm also uses process priority when picking victim pages.

**Kernel Statistics (Integration with PA1)**
- Extended `vmstats` with `disk_reads`, `disk_writes`, `avg_disk_latency` — tracked per-process and globally, exposed via `getvmstats()`.

**User-space Test Programs**
- `PA4_t1`: Verifies swap correctness for 150 pages.
- `PA4_t2`: Compares FCFS vs. SSTF latency under heavy swapping (600 pages).
- `PA4_t3`: Validates RAID-5 data integrity with unique page patterns.
- `PA4_t4`: Demonstrates priority eviction — CPU-bound child evicted far more than I/O-bound parent.

---
## Design Decisions and Assumptions

- **Disk image size**: Increased to `FSSIZE = 120000` blocks to fit three RAID regions (32768 blocks each) plus the filesystem.
- **Swap region layout**: Each RAID mode gets its own contiguous region starting at `SWAP_START = 20000`, well beyond the filesystem area.
- **Swap slot mapping**: One swap slot = one page = 8 disk blocks (`PGSIZE/BSIZE`).
- **RAID 5 parity buffer**: A global `parity_buf[BSIZE]` protected by `sched_lock`, avoiding stack allocation issues during I/O.
- **Queue flushing**: Disk I/O requests are enqueued, then the whole queue is drained at the end of `disk_swap_write/read` to preserve correctness.
- **Priority in eviction**: The clock algorithm uses the process's `cur_q_level` (from MLFQ) to prefer evicting pages from low-priority (CPU-bound) processes.
- **Concurrency**: Locks (`sched_lock`, `frametable_lock`, `swap_lock`) protect shared structures consistently. Disk I/O runs without holding these locks to avoid deadlocks.

---
## Files Modified

- `kernel/kalloc.c` — fixed deadlock in `evict_pages()`; limited physical memory to `NFRAME * PGSIZE` so all pages are tracked for eviction
- `kernel/virtio_disk.c` — implemented the disk request queue, `enqueue_disk_req()`, `run_disk_queue()` (FCFS/SSTF); added RAID-0/1/5 read/write functions; integrated latency model and priority handling; added `disk_swap_write()`/`disk_swap_read()`
- `kernel/memlayout.h` — defined `NFRAME` (512), `SWAP_START` (20000), `NSWAPBLOCKS_PER_MODE`, `NDISKREQS` (128), `NDISKS` (4)
- `kernel/param.h` — removed duplicate definitions; set `FSSIZE = 120000`
- `kernel/sysproc.c` — added `sys_setraidmode()`
- `kernel/syscall.h` / `syscall.c` — assigned `SYS_setraidmode` and mapped to `sys_setraidmode`
- `user/usys.pl` — added `entry("setraidmode");`
- `user/user.h` — added `int setraidmode(int);`
- `user/PA4_t1.c` – `PA4_t4.c` — the four test programs described above

*(Files like `vm.c`, `trap.c`, `proc.c`, `defs.h` already had the required swap-aware logic from PA3 and needed no further changes.)*

---
## Experimental Results

All tests were run on the modified xv6 with 3 CPUs and 128 MB RAM on the QEMU virt machine.

**PA4_t1 – Swap Correctness**: Wrote unique values to 150 pages (forcing swapping past the 512-frame limit). All values were read back correctly, confirming disk-backed swap works.

**PA4_t2 – SSTF vs. FCFS**: Under heavy swapping (600 pages), SSTF reduced average disk latency by ~50% compared to FCFS — confirming the scheduler reorders requests to minimize seek distance.

**PA4_t3 – RAID-5 Data Integrity**: With RAID-5 mode set, 160 pages were written, evicted, and reread with no data corruption — confirming correct parity calculation and distribution.

**PA4_t4 – Priority Eviction**: The CPU-bound child (demoted to low priority) suffered 239 page evictions, while the I/O-bound parent (high priority) had none — validating the MLFQ + eviction + disk scheduling integration.

---
## Conclusion

This implementation extends xv6 with disk-backed swap, configurable disk scheduling, RAID-backed storage, and priority-aware eviction. The results confirm correctness and show the effectiveness of SSTF scheduling and priority-based eviction.