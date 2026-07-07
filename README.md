# xv6 OS Kernel Extensions

Four kernel projects built on top of xv6-riscv for CS3523 (Operating Systems II). Each one builds on the previous — starting with basic syscalls and ending with a scheduler-aware, RAID-backed virtual memory system.

---
## Project Timeline

1. **Custom System Calls** — new syscalls for process info + per-process syscall accounting ([`PA1/`])
2. **SC-MLFQ Scheduler** — 4-level feedback queue scheduler with syscall-aware demotion and anti-starvation boost ([`PA2/`])
3. **Page Replacement** — frame table, Clock eviction algorithm, in-memory swap ([`PA3/`])
4. **Disk & RAID Swap** — disk-backed swap, FCFS/SSTF scheduling, RAID 0/1/5 ([`PA4/`])

[`cs3523-xv6-26-m/`] has everything merged into one working kernel.

---
## Why This Project

Every assignment pushed into a different part of the OS:

- PA1 is about how a syscall gets from user space into the kernel, and how to safely share the process table.
- PA2 builds a scheduler that's fair and doesn't starve low-priority processes — using the syscall counter from PA1 to tell interactive processes apart from CPU-bound ones.
- PA3 handles what happens when physical memory runs out, instead of just killing the process like base xv6 does.
- PA4 connects virtual memory to actual disk storage, with RAID spreading and protecting data across simulated disks.

They're not fully independent either — a process's syscall behavior (PA1) affects its scheduling priority (PA2), which affects which pages get evicted under memory pressure (PA3), which affects how those evictions get scheduled across disks (PA4).

---
## Tech Stack

- OS: xv6-riscv
- Language: C
- Architecture: RISC-V
- Emulator: QEMU

---
## Building & Running

Each PA folder only has the files that were added/changed for that assignment, so it won't build on its own. To actually run the OS, use `cs3523-xv6-26-m/`, which has the full integrated kernel:

```bash
cd cs3523-xv6-26-m
make qemu
```

This boots xv6 in QEMU. From the shell you can run the test programs mentioned in each PA's README — things like `PA4_t1`, `test_cpubound`, `getmlfqinfo`.

To exit QEMU: `Ctrl-a` then `x`.

---
## Repository Structure
xv6-os-kernel-extensions/
├── PA1/                  # hello, getpid2, getppid, getnumchild, getsyscount, getchildsyscount
├── PA2/                  # 4-level SC-MLFQ scheduler
├── PA3/                  # Clock algorithm + swap + scheduler-aware eviction
├── PA4/                  # Disk-backed swap + FCFS/SSTF + RAID 0/1/5
└── cs3523-xv6-26-m/      # Full integrated kernel — build & run this

---
## Key Concepts Demonstrated

- Kernel-to-userspace system call design
- Process table synchronization and locking
- Multi-level feedback queue scheduling
- Virtual memory, paging, page faults, TLB management
- Page replacement (Clock algorithm)
- Disk I/O scheduling (FCFS, SSTF)
- RAID (striping, mirroring, parity)