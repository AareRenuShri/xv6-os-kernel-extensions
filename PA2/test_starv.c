#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void){
    int cpu_pid = fork();
    if(cpu_pid == 0){
        volatile long x = 0;
        for(long i = 0; i < 800000000; i++) x++;
        printf("cpu child done level=%d\n", getlevel());
        exit(0);
    }

    int int_pid = fork();
    if(int_pid == 0){
        int i;
        for(i = 0; i < 30000; i++){
            getpid();
            getsyscount();
        }
        printf("interactive child done level=%d\n", getlevel());
        exit(0);
    }

    int i;
    int prev = 0;
    for(i = 0; i < 6; i++){
        pause(30);
        struct mlfqinfo c1, c2;
        getmlfqinfo(cpu_pid, &c1);
        getmlfqinfo(int_pid, &c2);
        printf("cpu: level=%d sched=%d  interactive: level=%d sched=%d\n",
            c1.level, c1.times_scheduled,
            c2.level, c2.times_scheduled);
        if(c1.times_scheduled > prev){
            printf("cpu child still getting cpu time - no starvation\n");
            prev = c1.times_scheduled;
        }
    }

    wait(0);
    wait(0);
    exit(0);
}