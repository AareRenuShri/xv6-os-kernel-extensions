# PA3: Scheduler-Aware Page Replacement in xv6
markdown# PA3: Scheduler-Aware Page Replacement in xv6

## Problem Statement

For this assignment I added page replacement to xv6, which previously just killed a process outright if physical memory ran out — not exactly graceful. The base repo had lazy allocation, but no way to recover from running out of physical frames. My job was to make the OS evict pages to a swap area instead of crashing, and keep the process running.

---
## What I Built

**Frame Table** — a global table that tracks every physical frame being used. Stores stuff like which process owns it, the virtual address, and a reference bit for the clock algorithm.

**Clock Algorithm** — the standard circular scan for picking which page to evict. Simpler than LRU and good enough for this.

**Swap Space** — an in-memory array with 128 slots (`MAX_SWAP = 128`) where evicted pages go.

**Scheduler-Aware Eviction** — when picking a victim page, the algorithm checks the process's priority level from the SC-MLFQ scheduler (PA2). Lower priority processes get evicted first so high-priority stuff stays safe.

**Memory Stats** — added `page_faults`, `pages_evicted`, `pages_swapped_in`, `pages_swapped_out`, and `resident_pages` per process.

---
## Design Decisions

**PTE Encoding for Swap** — Needed a way to tell apart "page never allocated" vs. "page was swapped out." Used the software-reserved bits in the RISC-V PTE — when a page is evicted, `PTE_V` is cleared and the swap slot index is stored in the upper bits.

**Eager Eviction** — At the start of every `vmfault`, checks if the frame table is full and evicts before doing anything else. Guarantees a free frame is always available, whether for a swap-in or a fresh lazy allocation. Avoids a lot of messy edge cases.

**TLB Flush** — This one caused headaches for a while — changing `PTE_V` without calling `sfence_vma()` causes random "Unexpected Scause" errors because the TLB caches the old mapping. Added explicit flushes after every PTE change involving the valid bit.

---
## Tests

**Test 1 — Large Allocation & Sequential Page Faults**: Allocated 40 pages with `MAX_FRAMES = 50`, so no eviction should happen. Confirms lazy allocation handles a big burst of page faults cleanly. `pages_evicted` stayed at 0 throughout.

**Test 2 — Force Page Replacement**: Set `MAX_FRAMES = 50` and allocated 60 pages. Got exactly 10 evictions and 10 swaps, with `resident_pages` capped at 50 — worked as expected.

**Test 3 & 4 — Data Integrity**: Wrote data to 60 pages then read it all back. Got 120 page faults total (60 initial allocs + 60 swap-ins). Every page had correct data, no corruption.

**Test 5 — Scheduler-Aware Behavior**: Ran a high-priority parent alongside a CPU-bound child that gets demoted. Under memory pressure, the child was evicted far more than the parent — exactly the expected behavior.

**Test 6 — Edge Cases & Validation**: Verified bad inputs don't crash the kernel and that stats are tracked correctly for child processes after fork (null pointers, out-of-bounds access, forked child getting its own separate stats).

---
## Files Changed

- `kernel/proc.h` — added memory stats to `struct proc`, defined `struct vmstats`, `struct frame_entry`, `MAX_FRAMES` (50), `MAX_SWAP` (128)
- `kernel/proc.c` — zero-initialized new stats in `allocproc()`
- `kernel/kalloc.c` — implemented the frame table, clock algorithm, swap space, `swap_out`/`swap_in`, plus getter functions for other kernel files
- `kernel/vm.c` — updated `vmfault()` with 3-stage logic (swap-in → evict if full → lazy alloc), updated `uvmunmap()` to free swap slots, updated `walkaddr()` so `copyin`/`copyout` trigger faults on swapped pages
- `kernel/trap.c` — added handlers for page fault causes 12 (instruction), 13 (load), 15 (store)
- `kernel/syscall.c` / `syscall.h` — registered `getvmstats` as syscall #30
- `kernel/sysproc.c` — implemented `sys_getvmstats()`
- `kernel/defs.h` — added prototypes for new frame/swap functions
- `user/user.h` — added `getvmstats()` prototype and `struct vmstats`
- `user/usys.pl` — added the `getvmstats` entry
- `Makefile` — added `_vmtest1` through `_vmtest6` to `UPROGS`
- `user/vmtest1.c` – `vmtest6.c` — full test suite covering lazy allocation, forced eviction, data integrity, scheduler-aware eviction, and edge cases

---
## Experimental Analysis

- **Test 1**: 40 pages under the 50-frame limit → 40 page faults, zero evictions. Lazy allocation works, replacement policy never kicked in.
- **Tests 2 & 3**: Pushed to 60 pages (10 over the limit). Evictions started at page 51, `resident_pages` stayed capped at 50. Clock algorithm correctly picked victims and freed frames for new requests.
- **Tests 3 & 4**: Wrote and read back data on all 60 pages — every page returned correct data, no mismatches. Confirms the swap-in path and PTE encoding work correctly.
- **Test 5**: High-priority parent vs. CPU-bound child under memory pressure — the child got evicted far more, confirming the clock algorithm avoids touching high-priority frames.