#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void){
    int p1 = fork();
    if(p1 == 0){
        volatile long x = 0;
        for(long i = 0; i < 600000000; i++) x++;
        printf("cpu done level=%d\n", getlevel());
        exit(0);
    }

    int p2 = fork();
    if(p2 == 0){
        int i;
        for(i = 0; i < 50000; i++){
            getpid();
            getsyscount();
            getlevel();
        }
        printf("interactive done level=%d\n", getlevel());
        exit(0);
    }

    int p3 = fork();
    if(p3 == 0){
        int r;
        for(r = 0; r < 10; r++){
            volatile long x = 0;
            for(long i = 0; i < 20000000; i++) x++;
            int j;
            for(j = 0; j < 500; j++) getpid();
        }
        printf("mixed done level=%d\n", getlevel());
        exit(0);
    }

    int i;
    for(i = 0; i < 6; i++){
        pause(25);
        struct mlfqinfo i1, i2, i3;
        getmlfqinfo(p1, &i1);
        getmlfqinfo(p2, &i2);
        getmlfqinfo(p3, &i3);
        printf("check %d: cpu=level%d interactive=level%d mixed=level%d\n",
            i+1, i1.level, i2.level, i3.level);
    }

    wait(0);
    wait(0);
    wait(0);
    exit(0);
}