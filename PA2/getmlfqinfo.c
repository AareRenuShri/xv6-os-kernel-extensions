#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void){
    struct mlfqinfo info;
    int pid = getpid();

    if(getmlfqinfo(pid, &info) < 0){
        printf("getmlfqinfo failed\n");
        exit(1);
    }

    printf("pid = %d\n", pid);
    printf("level = %d\n", info.level);
    printf("ticks = %d %d %d %d\n", info.ticks[0], info.ticks[1], info.ticks[2], info.ticks[3]);
    printf("times scheduled = %d\n", info.times_scheduled);
    printf("total syscalls = %d\n", info.total_syscalls);
    exit(0);
}