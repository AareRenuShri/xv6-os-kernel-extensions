// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

int clock_hand = 0;

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct spinlock frametable_lock;
struct spinlock swap_lock;

struct swap_slot {
  int in_use;
};
struct swap_slot swap_space[NSWAPSPACE];

struct frame{
  int in_use;
  struct proc *p;
  int reference_bit;
  uint64 va;
};

struct frame frames[NFRAME];

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&frametable_lock, "frametable_lock");
  initlock(&swap_lock, "swap_lock");

  for(int i = 0; i < NFRAME; i++){
    frames[i].in_use = 0;
    frames[i].p = 0;
    frames[i].reference_bit = 0;
  }

  for(struct swap_slot *s = swap_space; s< &swap_space[NSWAPSPACE]; s++ ){
    s->in_use  = 0;
  }
  freerange(end, (char*)end + (NFRAME * PGSIZE));
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.

  uint index = ((uint64)pa - KERNBASE) >> 12;
  if(index < NFRAME) {
    acquire(&frametable_lock);
    frames[index].in_use = 0;
    frames[index].p = 0;
    frames[index].va = 0;
    release(&frametable_lock);
  }
  
  memset(pa, 1, PGSIZE);
  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
  }else{
    return evict_pages();
  }
  return (void*)r;
}

void
track_frame(struct proc *p, uint64 va, uint64 pa){
  if(pa < (uint64)KERNBASE || pa >= PHYSTOP) return;
  uint index = (pa - KERNBASE) >> 12;

  // PRECISE CHANGE: 
  // If the physical page is beyond the 50-frame test limit, 
  // we treat it as "untracked" memory. 
  if(index >= NFRAME) 
    return; 

  acquire(&frametable_lock);
  if(frames[index].in_use == 0 && p != 0){
    p->resident_pages++;
  }
  
  frames[index].in_use = 1;
  frames[index].p = p;
  frames[index].va = va;
  frames[index].reference_bit = 1;
  release(&frametable_lock);
}

int swap_out(uint64 pa){
  struct proc *p = myproc();
  int priority = (p != 0) ? p->cur_q_level : 3;
  int slot = -1;

  // 1. Safely find a free disk slot
  acquire(&swap_lock);
  for(int i = 0; i < NSWAPSPACE; i++){
    if(swap_space[i].in_use == 0){
      swap_space[i].in_use = 1;
      slot = i;
      break;
    }
  }
  release(&swap_lock);

  if(slot == -1) 
    return -1; 

  // 2. Perform the I/O (The driver handles its own internal locks)
  disk_swap_write(slot, (char*)pa, priority); 

  // 3. Safely update process statistics
  if(p != 0) {
    acquire(&p->lock);
    p->pages_swapped_out++;
    p->disk_writes++;
    release(&p->lock);
  }
  
  return slot;
}

void
swap_in(uint64 pa, int slot) // Standard order: address first, then slot
{
  struct proc *p = myproc();
  // Get priority for the disk scheduler
  int priority = (p != 0) ? p->cur_q_level : 3;

  // 1. Perform the disk read with 3 arguments: slot, buffer, priority
  disk_swap_read(slot, (char*)pa, priority);

  // 2. Free the disk slot
  acquire(&swap_lock);
  if(slot >= 0 && slot < NSWAPSPACE){
    swap_space[slot].in_use = 0;
  }
  release(&swap_lock);

  // 3. Update stats
  if(p != 0) {
    int already_held = holding(&p->lock);
    if(!already_held)
      acquire(&p->lock);
    p->disk_reads++;      
    p->pages_swapped_in++;
    p->resident_pages++;
    if(!already_held)
      release(&p->lock);
  }
}

void swap_free(int index){
  acquire(&swap_lock);
  swap_space[index].in_use = 0;
  release(&swap_lock);
}

void*
evict_pages(void)
{
  struct frame *f;
  pte_t *pte;
  uint64 pa;

  for(int i = 0; i < NFRAME; i++){
    acquire(&frametable_lock);
    f = &frames[clock_hand];
    clock_hand = (clock_hand + 1) % NFRAME;

    if(f->in_use && f->p != 0 && f->p->pagetable != 0){
      pte = walk(f->p->pagetable, f->va, 0);
      if(pte != 0 && (*pte & PTE_V)){
        pa = PTE2PA(*pte);

        // Clock algorithm: check reference bit
        if(frames[((uint64)pa - KERNBASE) >> 12].reference_bit == 1){
          frames[((uint64)pa - KERNBASE) >> 12].reference_bit = 0;
        } else {
          // Victim found – evict it
          *pte &= ~PTE_V;
          *pte |= PTE_SWAPPED;
          release(&frametable_lock);

          int swap_index = swap_out(pa);
          if(swap_index != -1){
            *pte = (swap_index << 10) | PTE_FLAGS(*pte) | PTE_SWAPPED;

            acquire(&frametable_lock);
            struct proc *evicted_proc = f->p;
            f->in_use = 0;
            f->p = 0;
            f->va = 0;
            release(&frametable_lock);

            if(evicted_proc != 0){
              acquire(&evicted_proc->lock);
              evicted_proc->pages_evicted++;
              evicted_proc->resident_pages--;
              release(&evicted_proc->lock);
            }

            kfree((void*)pa);
            return kalloc();
          } else {
            panic("evict_pages: swap full");
          }
        }
      }
    }
    release(&frametable_lock);
  }
  return 0;
}