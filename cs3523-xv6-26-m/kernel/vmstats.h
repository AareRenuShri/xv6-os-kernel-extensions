
#ifndef VMSTATS_H
#define VMSTATS_H

struct vmstats {
    int page_faults;
    int pages_evicted;
    int pages_swapped_in;
    int pages_swapped_out;
    int resident_pages;
    // PA4
    int disk_reads;
    int disk_writes;
    int avg_disk_latency;
};

#endif