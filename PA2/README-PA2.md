# PA2: System-Call-Aware Multi-Level Feedback Queue Scheduler in xv6

## Problem Statement

For this assignment I swapped out xv6's default round-robin scheduler for a 4-level Multi-Level Feedback Queue (MLFQ) scheduler.

* **Levels & quanta** – Four priority levels, each with its own fixed time quantum (2/4/8/16 ticks). Use up your full slice, and you get demoted a level.
* **The syscall-aware twist** – This is where PA1 comes back in. Using the syscall counter I built earlier, the scheduler checks if a process made at least as many syscalls as ticks it consumed. If so, it's treated as interactive and skips demotion — so I/O-heavy processes stay snappy while CPU-bound ones gradually sink to lower priority.
* **No starvation** – Every 128 ticks, a global boost resets every process back to level 0, so nothing gets stuck at the bottom forever.

New syscalls `getlevel()` and `getmlfqinfo()` let user programs peek at these scheduling stats directly.

---
## What I Built

- Rewrote `scheduler()` to run a proper 4-level MLFQ instead of round-robin.
- Added tick counting, demotion logic, and the global boost (every 128 ticks) in `trap.c`.
- Implemented `getlevel()` and `getmlfqinfo()` as new system calls to expose scheduling stats to user space.
- Wrote five test programs to validate each behavior (CPU-bound demotion, interactive protection, starvation prevention, boost, and mixed workloads).

---
## Logic

- Processes start at level 0 (highest priority). Every time a process uses its full time slice, it gets pushed down to the next level. Processes at lower levels get bigger time slices but run less often.
- The syscall-aware rule checks, at the end of each slice, how many syscalls the process made vs. how many ticks it consumed. If syscalls ≥ ticks, the process is considered interactive and isn't demoted — this keeps interactive processes responsive.
- To avoid starvation, every 128 ticks all processes get moved back to level 0. Even a process stuck at level 3 eventually gets a shot at high priority again.
- Tick counting happens in `trap.c` on every timer interrupt. The demotion decision happens in `scheduler()` after the process yields back, since that's where the process lock is held safely.

---
## Modified Files :

kernel/proc.h    - added MLFQ fields to struct proc, added struct mlfqinfo
kernel/proc.c    - initialized MLFQ fields in allocproc(), rewrote scheduler()
kernel/trap.c    - tick counting, demotion, global boost every 128 ticks
kernel/sysproc.c - implemented sys_getlevel() and sys_getmlfqinfo()
kernel/syscall.h - added SYS_getlevel (28) and SYS_getmlfqinfo (29)
kernel/syscall.c - registered new syscalls and extern declarations
user/usys.pl     - added entry for getlevel and getmlfqinfo
user/user.h      - added struct mlfqinfo and syscall prototypes
Makefile         - added new test programs to UPROGS

## Created Files :

user/getlevel.c       - tests getlevel() syscall
user/getmlfqinfo.c    - tests getmlfqinfo() syscall
user/test_cpubound.c  - cpu bound process should sink to level 3
user/test_interact.c  - syscall heavy process should stay at level 0
user/test_starve.c    - low priority process still gets cpu time
user/test_boost.c     - global boost resets level after 128 ticks
user/test_mixed.c     - cpu, interactive and mixed processes together

---
## Experimental Results

**CPU-bound (`test_cpubound`)** — Process with no syscalls demoted from level 0 to level 3.
child moved to level 1
child moved to level 2
child moved to level 3
parent check: child level = 3, ticks = 4 8 16 1

**Interactive (`test_interact`)** — Process making 40,000+ syscalls stayed at level 0 throughout.
  child end level = 0
  child level = 0, syscalls = 40270

**No starvation (`test_starve`)** — CPU-bound child at level 3 kept getting scheduled alongside an interactive
  child at level 0.
  cpu: level=3 sched=15   interactive: level=0 sched=26
cpu child still getting cpu time - no starvation

**Boost (`test_boost`)** — CPU-bound child sank to level 3; boost fired after 150 ticks.
  child at level 3, waiting for boost
  after boost: level=2, ticks=5 8 15 40
boost worked!

**Mixed (`test_mixed`)**
  cpu=level3   interactive=level0   mixed=level0
CPU-bound demoted, interactive protected, mixed workload stayed high.