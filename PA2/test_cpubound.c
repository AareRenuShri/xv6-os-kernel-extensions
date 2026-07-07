#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void){
    int pid = fork();

    if(pid == 0){
        printf("child start level = %d\n", getlevel());
        volatile long x = 0;
        int last = getlevel();
        while(1){
            for(long i = 0; i < 50000000; i++) x++;
            int cur = getlevel();
            if(cur != last){
                printf("child moved to level %d\n", cur);
                last = cur;
            }
            if(cur == 3){
                printf("child at lowest level, done\n");
                break;
            }
        }
        exit(0);
    }

    int i;
    for(i = 0; i < 5; i++){
        pause(30);
        struct mlfqinfo info;
        getmlfqinfo(pid, &info);
        printf("parent check: child level=%d ticks=%d %d %d %d\n",
            info.level,
            info.ticks[0], info.ticks[1],
            info.ticks[2], info.ticks[3]);
    }

    wait(0);
    exit(0);
}