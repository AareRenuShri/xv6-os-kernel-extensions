#include "kernel/types.h"
#include "user/user.h"

int main() {
  int pid = getpid();

  for(int i=0;i<100000;i++){
    getpid();
  }

  struct mlfqinfo info;
  getmlfqinfo(pid,&info);

  printf("Syscall-heavy process\n");
  printf("Final level: %d\n",info.level);

  exit(0);
}