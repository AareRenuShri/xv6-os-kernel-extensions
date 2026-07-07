#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void){
    int pid = fork();

    if(pid == 0){
        printf("child start level=%d\n", getlevel());
        volatile long x = 0;
        for(long i = 0; i < 2000000000; i++) x++;
        printf("child done level=%d\n", getlevel());
        exit(0);
    }

    struct mlfqinfo info;
    int i;
    for(i = 0; i < 10; i++){
        pause(10);
        getmlfqinfo(pid, &info);
        printf("demoting: level=%d ticks=%d %d %d %d\n",
            info.level,
            info.ticks[0], info.ticks[1],
            info.ticks[2], info.ticks[3]);
        if(info.level == 3){
            printf("child at level 3, waiting for boost\n");
            break;
        }
    }

    pause(150);

    getmlfqinfo(pid, &info);
    printf("after boost: level=%d ticks=%d %d %d %d\n",
        info.level,
        info.ticks[0], info.ticks[1],
        info.ticks[2], info.ticks[3]);

    if(info.level < 3)
        printf("boost worked!\n");
    else
        printf("still at level 3 - may be demoted again\n");

    wait(0);
    exit(0);
}