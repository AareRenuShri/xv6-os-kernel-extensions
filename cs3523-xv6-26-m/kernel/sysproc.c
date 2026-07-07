#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
#include "vmstats.h"


extern struct spinlock wait_lock; 
extern struct proc proc[NPROC];


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


uint64
sys_hello(void){
  printf("Hello from the kernel!\n");
  return 0;
}

uint64
sys_getpid2(void)
{
  return myproc()->pid;
}






uint64
sys_getppid(void)
{

  struct proc *p = myproc();
  int pid;

  acquire(&wait_lock);

  if(p->parent == 0){
    pid= -1;
  }

  else{
    pid = myproc()->parent->pid;
  }

  release(&wait_lock);
  
  return pid;
}


uint64
sys_getnumchild(void){

  int total = 0;

  struct proc *pp;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(pp=proc;pp<&proc[NPROC];pp++){

    

    acquire(&pp->lock);
    if(pp->parent == p){
      
      if(pp->state!= ZOMBIE && pp->state!= UNUSED){
        total++;
      }
      
    }
    release(&pp->lock);
    
  }

  release(&wait_lock);

  return total;
}



uint64
sys_getsyscount(void)
{
  
  return myproc()->total_syscalls;
}


uint64 
sys_getchildsyscount(void){

  int pid;
  struct proc *pp;
  int count;


  argint(0, &pid);
    
    
  acquire(&wait_lock);

  for(pp=proc;pp<&proc[NPROC];pp++){
    acquire(&pp->lock);



    if(pp->pid == pid){

      

      if(pp->parent == myproc()){
        count = pp->total_syscalls;
        release(&pp->lock);
        release(&wait_lock);
        return count;
      }

      
      
    }

    release(&pp->lock);
  }

  release(&wait_lock);

  return -1;
}


uint64
sys_getlevel(void)
{
  return myproc()->cur_q_level;
}

uint64
sys_getmlfqinfo(void)
{
  int pid;
  uint64 info_addr;
  
  // Retrieve the arguments: (int pid, struct mlfqinfo *info)
  argint(0, &pid);
  argaddr(1, &info_addr);

  struct proc *p;
  struct mlfqinfo info;
  int found = 0;

  // Search for the process
  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid && p->state != UNUSED){
      info.level = p->cur_q_level;
      for(int i = 0; i < 4; i++){
        info.ticks[i] = p->ticks_per_level[i];
      }
      info.times_scheduled = p->num_times_sched;
      info.total_syscalls = p->total_syscalls;
      
      found = 1;
      release(&p->lock);
      break;
    }
    release(&p->lock);
  }

  if(found == 0){
    return -1; // PID not found
  }

  
  if(copyout(myproc()->pagetable, info_addr, (char *)&info, sizeof(info)) < 0)
    return -1;

  return 0;
}




uint64
sys_getvmstats(void){
  int pid;
  uint64 info_addr;

  argint(0, &pid);
  argaddr(1, &info_addr);

  struct proc *p;
  struct vmstats info;

  int found  = 0;

  for(p = proc; p< &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid && p->state != UNUSED){

      info.page_faults = p->page_faults;
      info.pages_evicted = p->pages_evicted;
      info.pages_swapped_in = p->pages_swapped_in;
      info.pages_swapped_out = p->pages_swapped_out;
      info.resident_pages  = p->resident_pages;

      // PA4
      info.disk_reads        = p->disk_reads;
      info.disk_writes       = p->disk_writes;
      info.avg_disk_latency  = p->avg_disk_latency;

      found  = 1;
      release(&p->lock);
      break;
    }
    release(&p->lock);

    
  }

  if(found==0){
      return -1;
  }

  if(copyout(myproc()->pagetable, info_addr, (char *)&info, sizeof(info)) < 0)
    return -1;

  return 0;

}

// PA4: set RAID mode (0=RAID0, 1=RAID1, 2=RAID5)
uint64
sys_setraidmode(void)
{
  int mode;
  argint(0, &mode);
  if(mode < 0 || mode > 2)
    return -1;
  set_raid_mode(mode);
  return 0;
}

uint64
sys_setdisksched(void)
{
  int policy;
  argint(0, &policy);
  return setdisksched(policy);
}

uint64
sys_setfaileddisk(void)
{
  int disk;
  argint(0, &disk);
  return set_failed_disk(disk);
}