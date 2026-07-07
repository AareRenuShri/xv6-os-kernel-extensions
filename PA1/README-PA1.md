# PA1: Extending xv6 with Custom System Calls

## Problem Statement

For this assignment I extended xv6 with six new system calls to get a feel for how user programs talk to the kernel and how to safely poke around the process table.

* **Part A** – Started simple with `hello()` and `getpid2()`, just to get the syscall wiring working end to end.
* **Part B** – Added `getppid()` and `getnumchild()`, which meant safely walking parent-child relationships in the process table without racing other processes.
* **Part C** – The bigger part: added kernel-side syscall accounting. Every process now tracks how many syscalls it's made, exposed through `getsyscount()` and `getchildsyscount()`.

That syscall counter from Part C ended up being important later — I reused it in PA2 to figure out which processes were "interactive" for the scheduler.

---
## What I Built

**Part A — Warm-up calls**
- `hello()`: Prints "Hello from the kernel!"
- `getpid2()`: Returns PID of the calling process.

**Part B — Process relationships**
- `getppid()`: Returns parent PID, or -1 if none exists.
- `getnumchild()`: Returns the number of currently alive child processes.

**Part C — Syscall accounting**
- `getsyscount()`: Returns the syscall count of the calling process.
- `getchildsyscount(int pid)`: Returns the syscall count of a given child PID.

---
## Design decisions and Assumptions:

- **Per-Process System Call Counter** — Added to `struct proc`, initialized to zero in `allocproc()`, and incremented centrally in the syscall dispatcher (`kernel/syscall.c`) so every syscall gets counted, no matter which one.
- **Child syscall lookup** — Uses `wait_lock` to safely traverse the process table and protect parent-child relationships from race conditions. Zombie children are still allowed since their syscall count stays valid until reclaimed.
- **Locking discipline** — Each process entry is locked before accessing its fields and unlocked immediately after.

---

## Files Modified

- **Kernel-space**: `kernel/syscall.c`, `kernel/syscall.h`, `kernel/sysproc.c`, `kernel/proc.c`, `kernel/proc.h`, `kernel/defs.h`
- **User-space**: `user/user.h`, `user/usys.pl`

## Files Created (User-space test programs)

- `hello.c`, `getpid2.c`, `getppid.c`, `getnumchild.c`, `getsyscount.c`, `getchildsyscount.c`