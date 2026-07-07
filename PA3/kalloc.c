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

//PA3
struct frame_entry frame_table[MAX_FRAMES];
static struct spinlock frame_lock;
static int clock_hand = 0;
static int user_frames = 0;

char swap_space[MAX_SWAP][PGSIZE];
static int swap_used[MAX_SWAP];
static struct spinlock swap_lock;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&frame_lock, "frame_lock");
  initlock(&swap_lock, "swap_lock");

  for(int i = 0; i < MAX_FRAMES; i++) {
    frame_table[i].in_use = 0;
    frame_table[i].p = 0;
    frame_table[i].virtual_addr = 0;
    frame_table[i].physical_addr = 0;
    frame_table[i].ref_bit = 0;
  }

  for(int i = 0; i < MAX_SWAP; i++) {
    swap_used[i] = 0;
  }
  freerange(end, (void*)PHYSTOP);
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

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

//PA3
int frame_alloc (struct proc *p, uint64 va, uint64 pa){
  acquire(&frame_lock);
  if(user_frames < MAX_FRAMES) {
    for(int i = 0; i < MAX_FRAMES; i++) {
      if(frame_table[i].in_use == 0) {
        frame_table[i].in_use = 1;
        frame_table[i].p = p;
        frame_table[i].virtual_addr = va;
        frame_table[i].physical_addr = pa;
        frame_table[i].ref_bit = 1;
        user_frames++;
        release(&frame_lock);
        return 0;
      }
    }
  }
  release(&frame_lock);
  return 1; 
}

void frame_free(uint64 pa){
  acquire(&frame_lock);
  for(int i = 0; i < MAX_FRAMES; i++){
    if(frame_table[i].in_use && frame_table[i].physical_addr == pa){
      frame_table[i].in_use = 0;
      frame_table[i].p = 0;
      frame_table[i].virtual_addr = 0;
      frame_table[i].physical_addr = 0;
      frame_table[i].ref_bit = 0;
      user_frames--;
      break;
    }
  }
  release(&frame_lock);
}

int frames_full() {
  acquire(&frame_lock);
  int full = (user_frames >= MAX_FRAMES);
  release(&frame_lock);
  return full;
}

void frame_set_ref(uint64 pa, int ref) {
  acquire(&frame_lock);
  for(int i = 0; i < MAX_FRAMES; i++){
    if(frame_table[i].in_use && frame_table[i].physical_addr == pa){
      frame_table[i].ref_bit = ref;
      break;
    }
  }
  release(&frame_lock);
}

int clck_victim(){
  acquire(&frame_lock);
  if(user_frames == 0) {
    release(&frame_lock);
    return -1;
  }
  for(int pass = 0; pass < 2; pass++){ 
    for(int i = 0; i < MAX_FRAMES; i++){
      int j = (clock_hand + i) % MAX_FRAMES;
      if(!frame_table[j].in_use) continue;

      if(frame_table[j].ref_bit == 1){
        frame_table[j].ref_bit = 0;
      } else {
        int best = j;
        int best_level = (frame_table[j].p) ? frame_table[j].p->level : 0;

        for(int k = 0; k < MAX_FRAMES; k++){
           if(!frame_table[k].in_use || frame_table[k].ref_bit != 0) continue;
           int lvl = (frame_table[k].p) ? frame_table[k].p->level : 0;
           if(lvl > best_level){ 
             best_level = lvl;
             best = k;
           }
        }
        clock_hand = (best + 1) % MAX_FRAMES;
        release(&frame_lock);
        return best;
      }
    }
  }
  release(&frame_lock);
  return -1; 
}

int swap_alloc(){
  acquire(&swap_lock);
  for(int slot = 0; slot < MAX_SWAP; slot++){
    if(!swap_used[slot]){
      swap_used[slot] = 1;
      release(&swap_lock);
      return slot;
    }
  }
  release(&swap_lock);
  return -1; 
}

void swap_free(int slot){
  if(slot < 0 || slot >= MAX_SWAP)
    return;
  acquire(&swap_lock);
  swap_used[slot] = 0;
  release(&swap_lock);
}
 
int swap_out(char *pa){
  int slot = swap_alloc();
  if(slot < 0)
    return -1;
  memmove(swap_space[slot], pa, PGSIZE);
  return slot;
}
 
void swap_in(int slot, char *pa){
  if(slot < 0 || slot >= MAX_SWAP)
    panic("swap_in: bad slot");
  memmove(pa, swap_space[slot], PGSIZE);
  swap_free(slot);
}
 
struct proc* get_frame_owner(int idx) { return frame_table[idx].p; }
uint64       get_frame_va(int idx)    { return frame_table[idx].virtual_addr; }
uint64       get_frame_pa(int idx)    { return frame_table[idx].physical_addr; }