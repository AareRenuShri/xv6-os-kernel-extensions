#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void){
    int pid = fork();

    if(pid == 0){
        printf("child start level = %d\n", getlevel());
        int i;
        for(i = 0; i < 20000; i++){
            getpid();
            getsyscount();
        }
        printf("child end level = %d\n", getlevel());
        exit(0);
    }

    int i;
    for(i = 0; i < 4; i++){
        pause(20);
        struct mlfqinfo info;
        getmlfqinfo(pid, &info);
        printf("child level=%d syscalls=%d\n", info.level, info.total_syscalls);
    }

    wait(0);
    exit(0);
}