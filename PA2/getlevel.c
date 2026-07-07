#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void){
    printf("pid = %d\n", getpid());
    printf("level = %d\n", getlevel());
    exit(0);
}