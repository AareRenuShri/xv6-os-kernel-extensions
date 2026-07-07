#define NPROC        64  // maximum number of processes
#define NCPU          8  // maximum number of CPUs
#define NOFILE       16  // open files per process
#define NFILE       100  // open files per system
#define NINODE       50  // maximum number of active i-nodes
#define NDEV         10  // maximum major device number
#define ROOTDEV       1  // device number of file system root disk
#define MAXARG       32  // max exec arguments
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes
#define LOGBLOCKS    (MAXOPBLOCKS*3)  // max data blocks in on-disk log
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache
// #define FSSIZE       110000  // size of file system in blocks
#define MAXPATH      128   // maximum file path name
#define USERSTACK    1     // user stack pages

// PA4: disk image size
// We need: filesystem + 3 RAID regions for swap
// Each RAID region: 512 slots * 8 blocks/slot = 4096 blocks
// Total swap: 3 * 4096 = 12288 blocks
// Total image: 2000 (fs) + 12288 (swap) = 14288 blocks
// We set FSSIZE to 15000 so mkfs creates a big enough fs.img
#define FSSIZE       120000   // controls fs.img size — must fit fs + all swap regions

// PA4 swap/RAID constants
// NSWAPSPACE = 4096 (from memlayout.h) but we only actively use ~512 slots
// since MAX_USER_FRAMES=50 means at most 50 pages are swapped at once.
// 512 slots * 8 blocks = 4096 blocks per RAID mode region — plenty.
#define NDISKS                4       // simulated RAID disks
#define ROTATIONAL_DELAY      10      // latency = |seek| + C

