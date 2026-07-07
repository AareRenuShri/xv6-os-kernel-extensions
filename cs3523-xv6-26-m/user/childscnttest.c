#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void){
  int pid = fork();
  if (pid == 0) {
    getpid();
    getpid();
    exit(0);
  }
  pause(10); // adding extra time so that the child runs first
  int count = getchildsyscount(pid);
  printf("child syscalls = %d\n", count);
  wait(0);
  exit(0);
}
