#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
extern struct proc proc[NPROC]; //B2

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

// C3
uint64
sys_getchildsyscount(void)
{
  int pid;
  struct proc *p = myproc();
  struct proc *child;
  argint(0, &pid);

  for(child = proc; child < &proc[NPROC]; child++) {
    acquire(&child->lock);
    if(child->parent == p && child->pid == pid) {
      uint64 count = child->syscall_count;
      release(&child->lock);
      return count;
    }
    release(&child->lock);
  }
  return -1;
}

uint64
sys_getlevel(void){
  return myproc()->level;
}

uint64
sys_getmlfqinfo(void){
  int pid;
  uint64 addr;
  
  argint(0, &pid);
  argaddr(1, &addr);

  struct mlfqinfo info;
  struct proc *p;
  int found = 0;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid && p->state != UNUSED){
      info.level           = p->level;
      info.ticks[0]        = p->ticks[0];
      info.ticks[1]        = p->ticks[1];
      info.ticks[2]        = p->ticks[2];
      info.ticks[3]        = p->ticks[3];
      info.times_scheduled = p->times_scheduled;
      info.total_syscalls  = p->syscall_count;
      found = 1;
    }
    release(&p->lock);
    if(found) break;
  }

  if(!found) return -1;

  if(copyout(myproc()->pagetable, addr, (char*)&info, sizeof(info)) < 0)
    return -1;

  return 0;
}

// PA3
uint64 sys_getvmstats(void){
  int pid;
  uint64 addr;
  struct proc *p;
  argint(0, &pid);
  argaddr(1, &addr);

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->pid == pid) {
      struct vmstats info;
      info.page_faults     = p->page_faults;
      info.pages_evicted   = p->pages_evicted;
      info.pages_swapped_in  = p->pages_swapped_in;
      info.pages_swapped_out = p->pages_swapped_out;
      info.resident_pages  = p->resident_pages;
      release(&p->lock);
      if(copyout(myproc()->pagetable, addr,(char*)&info, sizeof(info)) < 0){
        return -1;
      }
      return 0;
    }
    release(&p->lock);
  }
  return -1;  
}