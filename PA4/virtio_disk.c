//
// driver for qemu's virtio disk device.
// uses qemu's mmio interface to virtio.
//
// qemu ... -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "virtio.h"
#include "proc.h"

// the address of virtio mmio register r.
#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))

static struct disk {
  // a set (not a ring) of DMA descriptors, with which the
  // driver tells the device where to read and write individual
  // disk operations. there are NUM descriptors.
  // most commands consist of a "chain" (a linked list) of a couple of
  // these descriptors.
  struct virtq_desc *desc;

  // a ring in which the driver writes descriptor numbers
  // that the driver would like the device to process.  it only
  // includes the head descriptor of each chain. the ring has
  // NUM elements.
  struct virtq_avail *avail;

  // a ring in which the device writes descriptor numbers that
  // the device has finished processing (just the head of each chain).
  // there are NUM used ring entries.
  struct virtq_used *used;

  // our own book-keeping.
  char free[NUM];  // is a descriptor free?
  uint16 used_idx; // we've looked this far in used[2..NUM].

  // track info about in-flight operations,
  // for use when completion interrupt arrives.
  // indexed by first descriptor index of chain.
  struct {
    struct buf *b;
    char status;
  } info[NUM];

  // disk command headers.
  // one-for-one with descriptors, for convenience.
  struct virtio_blk_req ops[NUM];
  
  struct spinlock vdisk_lock;
  
} disk;

// PA4: Disk scheduling

#define SCHED_FCFS 0
#define SCHED_SSTF 1

// One entry in the pending request queue
struct disk_req {
  int valid;      // 1 = this slot is occupied
  int blockno;    // which real disk block
  int write;      // 1 = write, 0 = read
  int priority;   // MLFQ level of requesting process (0=highest, 3=lowest)
  char *data;     // pointer to BSIZE-byte buffer
};

static int            disk_policy   = SCHED_FCFS;
static int            disk_head_pos = 0;          // simulated head position
static struct disk_req req_queue[NDISKREQS];
static struct spinlock sched_lock;

// PA4: Disk statistics
static long stat_reads       = 0;
static long stat_writes      = 0;
static long stat_latency_sum = 0;
static long stat_req_count   = 0;

// PA4: RAID state
static int raid_mode = 0;   // 0=RAID0, 1=RAID1, 2=RAID5

// PA4: per-slot RAID mode record.
// When a page is swapped OUT we record which RAID mode was active.
// When swapped IN we use that same mode to read — even if raid_mode changed.
// NSWAPSPACE = 4096 (from memlayout.h).
static int slot_raid_mode[4096];  // indexed by swap slot



// PA4: scratch buffer for RAID5 parity (protected by sched_lock)
static char parity_buf[BSIZE];

void
virtio_disk_init(void)
{
  uint32 status = 0;

  initlock(&disk.vdisk_lock, "virtio_disk");

  // PA4: init scheduling lock and queue
  initlock(&sched_lock, "disksched");
  for(int i = 0; i < NDISKREQS; i++)
    req_queue[i].valid = 0;

  if(*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
     *R(VIRTIO_MMIO_VERSION) != 2 ||
     *R(VIRTIO_MMIO_DEVICE_ID) != 2 ||
     *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551){
    panic("could not find virtio disk");
  }
  
  // reset device
  *R(VIRTIO_MMIO_STATUS) = status;

  // set ACKNOWLEDGE status bit
  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  *R(VIRTIO_MMIO_STATUS) = status;

  // set DRIVER status bit
  status |= VIRTIO_CONFIG_S_DRIVER;
  *R(VIRTIO_MMIO_STATUS) = status;

  // negotiate features
  uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
  features &= ~(1 << VIRTIO_BLK_F_RO);
  features &= ~(1 << VIRTIO_BLK_F_SCSI);
  features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
  features &= ~(1 << VIRTIO_BLK_F_MQ);
  features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
  features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
  features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
  *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

  // tell device that feature negotiation is complete.
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  // re-read status to ensure FEATURES_OK is set.
  status = *R(VIRTIO_MMIO_STATUS);
  if(!(status & VIRTIO_CONFIG_S_FEATURES_OK))
    panic("virtio disk FEATURES_OK unset");

  // initialize queue 0.
  *R(VIRTIO_MMIO_QUEUE_SEL) = 0;

  // ensure queue 0 is not in use.
  if(*R(VIRTIO_MMIO_QUEUE_READY))
    panic("virtio disk should not be ready");

  // check maximum queue size.
  uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if(max == 0)
    panic("virtio disk has no queue 0");
  if(max < NUM)
    panic("virtio disk max queue too short");

  // allocate and zero queue memory.
  disk.desc = kalloc();
  disk.avail = kalloc();
  disk.used = kalloc();
  if(!disk.desc || !disk.avail || !disk.used)
    panic("virtio disk kalloc");
  memset(disk.desc, 0, PGSIZE);
  memset(disk.avail, 0, PGSIZE);
  memset(disk.used, 0, PGSIZE);

  // set queue size.
  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

  // write physical addresses.
  *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64)disk.desc;
  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64)disk.desc >> 32;
  *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = (uint64)disk.avail;
  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64)disk.avail >> 32;
  *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = (uint64)disk.used;
  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64)disk.used >> 32;

  // queue is ready.
  *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;

  // all NUM descriptors start out unused.
  for(int i = 0; i < NUM; i++)
    disk.free[i] = 1;

  // tell device we're completely ready.
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  // plic.c and trap.c arrange for interrupts from VIRTIO0_IRQ.
}

// find a free descriptor, mark it non-free, return its index.
static int
alloc_desc()
{
  for(int i = 0; i < NUM; i++){
    if(disk.free[i]){
      disk.free[i] = 0;
      return i;
    }
  }
  return -1;
}

// mark a descriptor as free.
static void
free_desc(int i)
{
  if(i >= NUM)
    panic("free_desc 1");
  if(disk.free[i])
    panic("free_desc 2");
  disk.desc[i].addr = 0;
  disk.desc[i].len = 0;
  disk.desc[i].flags = 0;
  disk.desc[i].next = 0;
  disk.free[i] = 1;
  wakeup(&disk.free[0]);
}

// free a chain of descriptors.
static void
free_chain(int i)
{
  while(1){
    int flag = disk.desc[i].flags;
    int nxt = disk.desc[i].next;
    free_desc(i);
    if(flag & VRING_DESC_F_NEXT)
      i = nxt;
    else
      break;
  }
}

// allocate three descriptors (they need not be contiguous).
// disk transfers always use three descriptors.
static int
alloc3_desc(int *idx)
{
  for(int i = 0; i < 3; i++){
    idx[i] = alloc_desc();
    if(idx[i] < 0){
      for(int j = 0; j < i; j++)
        free_desc(idx[j]);
      return -1;
    }
  }
  return 0;
}

void
virtio_disk_rw(struct buf *b, int write)
{
  uint64 sector = b->blockno * (BSIZE / 512);

  acquire(&disk.vdisk_lock);

  // the spec's Section 5.2 says that legacy block operations use
  // three descriptors: one for type/reserved/sector, one for the
  // data, one for a 1-byte status result.

  // allocate the three descriptors.
  int idx[3];
  while(1){
    if(alloc3_desc(idx) == 0) {
      break;
    }
    sleep(&disk.free[0], &disk.vdisk_lock);
  }

  // format the three descriptors.
  // qemu's virtio-blk.c reads them.

  struct virtio_blk_req *buf0 = &disk.ops[idx[0]];

  if(write)
    buf0->type = VIRTIO_BLK_T_OUT; // write the disk
  else
    buf0->type = VIRTIO_BLK_T_IN; // read the disk
  buf0->reserved = 0;
  buf0->sector = sector;

  disk.desc[idx[0]].addr = (uint64) buf0;
  disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);
  disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
  disk.desc[idx[0]].next = idx[1];

  disk.desc[idx[1]].addr = (uint64) b->data;
  disk.desc[idx[1]].len = BSIZE;
  if(write)
    disk.desc[idx[1]].flags = 0; // device reads b->data
  else
    disk.desc[idx[1]].flags = VRING_DESC_F_WRITE; // device writes b->data
  disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
  disk.desc[idx[1]].next = idx[2];

  disk.info[idx[0]].status = 0xff; // device writes 0 on success
  disk.desc[idx[2]].addr = (uint64) &disk.info[idx[0]].status;
  disk.desc[idx[2]].len = 1;
  disk.desc[idx[2]].flags = VRING_DESC_F_WRITE; // device writes the status
  disk.desc[idx[2]].next = 0;

  // record struct buf for virtio_disk_intr().
  b->disk = 1;
  disk.info[idx[0]].b = b;

  // tell the device the first index in our chain of descriptors.
  disk.avail->ring[disk.avail->idx % NUM] = idx[0];

  __sync_synchronize();

  // tell the device another avail ring entry is available.
  disk.avail->idx += 1; // not % NUM ...

  __sync_synchronize();

  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0; // value is queue number

  // Wait for virtio_disk_intr() to say request has finished.
  while(b->disk == 1) {
    sleep(b, &disk.vdisk_lock);
  }

  disk.info[idx[0]].b = 0;
  free_chain(idx[0]);

  release(&disk.vdisk_lock);
}

void
virtio_disk_intr()
{
  acquire(&disk.vdisk_lock);

  // the device won't raise another interrupt until we tell it
  // we've seen this interrupt, which the following line does.
  // this may race with the device writing new entries to
  // the "used" ring, in which case we may process the new
  // completion entries in this interrupt, and have nothing to do
  // in the next interrupt, which is harmless.
  *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;

  __sync_synchronize();

  // the device increments disk.used->idx when it
  // adds an entry to the used ring.

  while(disk.used_idx != disk.used->idx){
    __sync_synchronize();
    int id = disk.used->ring[disk.used_idx % NUM].id;

    if(disk.info[id].status != 0)
      panic("virtio_disk_intr status");

    struct buf *b = disk.info[id].b;
    b->disk = 0;   // disk is done with buf
    wakeup(b);

    disk.used_idx += 1;
  }

  release(&disk.vdisk_lock);
}

// =============================================================================
// PA4: Disk scheduling, RAID, disk-backed swap
// =============================================================================

// setdisksched: called from sys_setdisksched in sysproc.c
// policy 0 = FCFS, policy 1 = SSTF
int
setdisksched(int policy)
{
  if(policy != SCHED_FCFS && policy != SCHED_SSTF)
    return -1;
  acquire(&sched_lock);
  disk_policy = policy;
  release(&sched_lock);
  return 0;
}

// set_raid_mode: called from sys_setraidmode in sysproc.c
// mode 0 = RAID0, 1 = RAID1, 2 = RAID5
void
set_raid_mode(int mode)
{
  acquire(&sched_lock);
  raid_mode = mode;
  release(&sched_lock);
}

// get_disk_stats: fills caller's pointers with global disk statistics
void
get_disk_stats(long *reads, long *writes, long *avg_lat)
{
  acquire(&sched_lock);
  *reads   = stat_reads;
  *writes  = stat_writes;
  *avg_lat = (stat_req_count > 0) ? (stat_latency_sum / stat_req_count) : 0;
  release(&sched_lock);
}

static void
update_lat_stats(int target_block)
{
  acquire(&sched_lock);
  // Latency = |target - current| + C [cite: 231, 232, 237]
  int dist = target_block - disk_head_pos;
  if(dist < 0) dist = -dist;
  int lat = dist + ROTATIONAL_DELAY; 
  
  stat_latency_sum += lat;
  stat_req_count++;
  disk_head_pos = target_block; 
  release(&sched_lock);

  struct proc *p = myproc();
  if(p){
    acquire(&p->lock);
    // Mandatory per-process stat update for PA4_t2 [cite: 273]
    int total_ops = p->disk_reads + p->disk_writes;
    if(total_ops <= 1)
      p->avg_disk_latency = lat;
    else
      p->avg_disk_latency = ((p->avg_disk_latency * (total_ops - 1)) + lat) / total_ops;
    release(&p->lock);
  }
}
// ─── internal: do a single BSIZE-block read from real disk block `blockno` ───
// Must NOT hold sched_lock when calling this (bread/bwrite may sleep).
static void
do_disk_read(int blockno, char *dst)
{
  struct buf *b = bread(ROOTDEV, blockno);
  memmove(dst, b->data, BSIZE);
  brelse(b);

  // update stats and head position
  update_lat_stats(blockno);
}

// ─── internal: do a single BSIZE-block write to real disk block `blockno` ────
static void
do_disk_write(int blockno, char *src)
{
  struct buf *b = bread(ROOTDEV, blockno);
  memmove(b->data, src, BSIZE);
  bwrite(b);
  brelse(b);

  update_lat_stats(blockno);
}

static void
enqueue_disk_req(int blockno, int write, char *data, int priority)
{
  acquire(&sched_lock);
  for(int i = 0; i < NDISKREQS; i++){
    if(!req_queue[i].valid){
      req_queue[i].valid    = 1;
      req_queue[i].blockno  = blockno;
      req_queue[i].write    = write;
      req_queue[i].priority = priority;
      req_queue[i].data     = data;
      break;
    }
  }
  release(&sched_lock);
}

// Process all pending requests according to the scheduling policy
static void
run_disk_queue(void)
{
  while(1){
    acquire(&sched_lock);

    // Find the best request (or none)
    int best_idx = -1;
    int best_score = 0x7fffffff;

    if(disk_policy == SCHED_FCFS){
      // FCFS: first inserted = lowest index
      for(int i = 0; i < NDISKREQS; i++){
        if(req_queue[i].valid){
          best_idx = i;
          break;
        }
      }
    } else { // SSTF
      for(int i = 0; i < NDISKREQS; i++){
        if(!req_queue[i].valid) continue;
        int dist = req_queue[i].blockno - disk_head_pos;
        if(dist < 0) dist = -dist;
        // Tie-break with priority (lower priority value = more urgent)
        int score = dist * 4 + req_queue[i].priority;
        if(score < best_score){
          best_score = score;
          best_idx = i;
        }
      }
    }

    if(best_idx == -1){
      release(&sched_lock);
      break;          // queue empty
    }

    // Extract the chosen request
    int blk   = req_queue[best_idx].blockno;
    int wr    = req_queue[best_idx].write;
    char *buf = req_queue[best_idx].data;
    req_queue[best_idx].valid = 0;

    // Compute latency stats and update head BEFORE I/O
    int dist = blk - disk_head_pos;
    if(dist < 0) dist = -dist;
    int lat = dist + ROTATIONAL_DELAY;
    stat_latency_sum += lat;
    stat_req_count++;
    disk_head_pos = blk;             // seek

    // Per‑process latency
    struct proc *p = myproc();
    if(p){
      acquire(&p->lock);
      int total_ops = p->disk_reads + p->disk_writes;
      if(total_ops <= 1)
        p->avg_disk_latency = lat;
      else
        p->avg_disk_latency = ((p->avg_disk_latency * (total_ops - 1)) + lat) / total_ops;
      release(&p->lock);
    }

    release(&sched_lock);

    // Execute the actual I/O (may sleep)
    if(wr) do_disk_write(blk, buf);
    else   do_disk_read(blk, buf);

    // Loop back to handle the rest of the queue
  }
}

#define BLOCKS_PER_PAGE  (PGSIZE / BSIZE)                        // 8 blocks per page
#define BLOCKS_PER_DISK  (NSWAPBLOCKS_PER_MODE / NDISKS)        // 8192 blocks per sim disk

// Convert (mode, disk_id, phys_offset) → real disk block number
// mode 0=RAID0, 1=RAID1, 2=RAID5 — each gets its own disk region


static int
raid_real_block(int mode, int disk_id, int phys_offset)
{
  int region_base = SWAP_START + mode * NSWAPBLOCKS_PER_MODE;
  return region_base + disk_id * BLOCKS_PER_DISK + phys_offset;
}

// ─── RAID 0 (striping) ────────────────────────────────────────────────────────
// logical block b → disk = b % NDISKS, offset = b / NDISKS

static void
raid0_write_page(int slot, char *page, int priority)
{
  int base = slot * BLOCKS_PER_PAGE;
  for(int b = 0; b < BLOCKS_PER_PAGE; b++){
    int logical  = base + b;
    int disk_id  = logical % NDISKS;
    int phys     = logical / NDISKS;
    enqueue_disk_req(raid_real_block(0, disk_id, phys), 1, page + b * BSIZE, priority);
  }
}

static void
raid0_read_page(int slot, char *page, int priority)
{
  int base = slot * BLOCKS_PER_PAGE;
  for(int b = 0; b < BLOCKS_PER_PAGE; b++){
    int logical  = base + b;
    int disk_id  = logical % NDISKS;
    int phys     = logical / NDISKS;
    enqueue_disk_req(raid_real_block(0, disk_id, phys), 0, page + b * BSIZE, priority);
  }
}

// ─── RAID 1 (mirroring) ───────────────────────────────────────────────────────
// Each block is written to disk_id and (disk_id+1)%NDISKS.
// Reads come from the primary disk.

static void
raid1_write_page(int slot, char *page, int priority)
{
  int base = slot * BLOCKS_PER_PAGE;
  for(int b = 0; b < BLOCKS_PER_PAGE; b++){
    int logical  = base + b;
    int disk_id  = logical % NDISKS;
    int mirror   = (disk_id + 1) % NDISKS;
    int phys     = logical / NDISKS;
    enqueue_disk_req(raid_real_block(1, disk_id, phys), 1, page + b * BSIZE, priority);
    enqueue_disk_req(raid_real_block(1, mirror,  phys), 1, page + b * BSIZE, priority);
  }
}

static void
raid1_read_page(int slot, char *page, int priority)
{
  // read from primary disk only
  int base = slot * BLOCKS_PER_PAGE;
  for(int b = 0; b < BLOCKS_PER_PAGE; b++){
    int logical  = base + b;
    int disk_id  = logical % NDISKS;
    int phys     = logical / NDISKS;
    enqueue_disk_req(raid_real_block(1, disk_id, phys), 0, page + b * BSIZE, priority);
  }
}

// ─── RAID 5 (striping with distributed parity) ───────────────────────────────
// We group blocks into stripe rows of NDISKS blocks (3 data + 1 parity).
// Parity disk for row r = r % NDISKS.
// Parity = XOR of all data blocks in the row.
//
// For a page of BLOCKS_PER_PAGE=8 data blocks we generate ceil(8/3)=3 stripe rows.
// Row 0: data blocks 0,1,2 → parity on disk (0%4)=0
// Row 1: data blocks 3,4,5 → parity on disk (1%4)=1
// Row 2: data blocks 6,7   → parity on disk (2%4)=2  (partial row, padded with 0)

#define DATA_PER_ROW  (NDISKS - 1)   // 3 data blocks per stripe row

static void
raid5_write_page(int slot, char *page, int priority)
{
  int base = slot * BLOCKS_PER_PAGE; 

  for(int row_start = 0; row_start < BLOCKS_PER_PAGE; row_start += DATA_PER_ROW){
    int row       = (base + row_start) / DATA_PER_ROW;
    int par_disk  = row % NDISKS;        
    int phys      = row / NDISKS;        

    // 1. Use the global parity_buf here!
    // We protect it with sched_lock because it's a shared global buffer.
    acquire(&sched_lock);
    memset(parity_buf, 0, BSIZE);

    int data_block_in_row = 0;
    for(int i = 0; i < DATA_PER_ROW; i++){
      int block_idx = row_start + i;
      if(block_idx < BLOCKS_PER_PAGE){
        char *blk_data = page + (block_idx * BSIZE);
        
        // XOR current data block into the global parity_buf
        for(int j = 0; j < BSIZE; j++)
          parity_buf[j] ^= blk_data[j];

        int disk_id = data_block_in_row;
        if(disk_id >= par_disk) disk_id++;

        // Release lock before I/O call as it might sleep
        release(&sched_lock);
        enqueue_disk_req(raid_real_block(2, disk_id, phys), 1, blk_data, priority);
        acquire(&sched_lock);
        
        data_block_in_row++;
      }
    }

    // 3. Write the parity block using the global parity_buf
    // We must pass a pointer that remains valid during the I/O operation.
    release(&sched_lock);
    enqueue_disk_req(raid_real_block(2, par_disk, phys), 1, parity_buf, priority);
  }
}

static void
raid5_read_page(int slot, char *page, int priority)
{
  int base = slot * BLOCKS_PER_PAGE;

  for(int row_start = 0; row_start < BLOCKS_PER_PAGE; row_start += DATA_PER_ROW){
    int row      = (base + row_start) / DATA_PER_ROW;
    int par_disk = row % NDISKS;
    int phys     = row / NDISKS;

    int data_block_in_row = 0;
    for(int i = 0; i < DATA_PER_ROW; i++){
      int block_idx = row_start + i;
      if(block_idx < BLOCKS_PER_PAGE){
        // Determine which disk holds this data block (skipping par_disk)
        int disk_id = data_block_in_row;
        if(disk_id >= par_disk) disk_id++;

        enqueue_disk_req(raid_real_block(2, disk_id, phys), 0, 
                         page + (block_idx * BSIZE), priority);
        data_block_in_row++;
      }
    }
  }
}

void
disk_swap_write(int slot, char *page, int priority)
{
  acquire(&sched_lock);
  int mode = raid_mode;
  release(&sched_lock);

  if(slot >= 0 && slot < 4096)
    slot_raid_mode[slot] = mode;

  switch(mode){
  case 0: raid0_write_page(slot, page, priority); break;
  case 1: raid1_write_page(slot, page, priority); break;
  case 2: raid5_write_page(slot, page, priority); break;
  default: raid0_write_page(slot, page, priority); break;
  }
  run_disk_queue();          //  drain all blocks to real disk
}

void
disk_swap_read(int slot, char *page, int priority)
{
  int mode = 0;
  if(slot >= 0 && slot < 4096)
    mode = slot_raid_mode[slot];

  switch(mode){
  case 0: raid0_read_page(slot, page, priority); break;
  case 1: raid1_read_page(slot, page, priority); break;
  case 2: raid5_read_page(slot, page, priority); break;
  default: raid0_read_page(slot, page, priority); break;
  }
  run_disk_queue();          //  drain all blocks to real disk
}