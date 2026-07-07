#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void) {
    int pid = fork();
    if(pid == 0){
        while(1);
    }
    printf("No. of children: %d\n",getnumchild());
    wait(0);
    exit(0);
}