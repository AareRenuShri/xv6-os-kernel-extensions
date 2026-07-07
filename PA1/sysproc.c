#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
extern struct proc proc[NPROC]; //B2
extern struct spinlock wait_lock;


uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    if(addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

//A1
uint64
sys_hello(void)
{
  printf("Hello from the kernel!\n");
  return 0;
}

//A2
uint64
sys_getpid2(void)
{
  return myproc()->pid;
}

//B1
uint64
sys_getppid(void)
{
  struct proc *process = myproc();
  if(process->parent)
    return process->parent->pid;
  else
    return -1;
}

//B2
uint64
sys_getnumchild(void)
{
  struct proc *p = myproc();
  struct proc *child;
  int count = 0;

  for(child = proc; child < &proc[NPROC]; child++) {
    acquire(&child->lock);
    if(child->parent == p && child->state != ZOMBIE) {
      count++;
    }
    release(&child->lock);
  }
  return count;
}

// C2
uint64
sys_getsyscount(void)
{
  return myproc()->syscall_count;
}

// // C3
// uint64
// sys_getchildsyscount(void)
// {
//   int pid;
//   struct proc *p = myproc();
//   struct proc *child;
//   argint(0, &pid);

//   for(child = proc; child < &proc[NPROC]; child++) {
//     acquire(&child->lock);
//     if(child->parent == p && child->pid == pid) {
//       uint64 count = child->syscall_count;
//       release(&child->lock);
//       return count;
//     }
//     release(&child->lock);
//   }
//   return -1;
// }

// C3
uint64
sys_getchildsyscount(void)
{
  int pid;
  struct proc *p = myproc();
  struct proc *child;

  argint(0, &pid);

  acquire(&wait_lock);   // protects parent-child relationships

  for (child = proc; child < &proc[NPROC]; child++) {
    if (child->pid == pid && child->parent == p) {
      uint64 count = child->syscall_count;
      release(&wait_lock);
      return count;
    }
  }

  release(&wait_lock);
  return -1;
}
