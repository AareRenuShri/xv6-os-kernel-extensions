#include "kernel/types.h"
#include "user/user.h"

int main() {
  int pid = getpid();

  for(int i = 0; i < 200; i++){
    
    // CPU work
    for(long j = 0; j < 5000000; j++);

    // syscall work
    for(int k = 0; k < 1000; k++)
      getpid();
  }

  struct mlfqinfo info;
  getmlfqinfo(pid, &info);

  printf("Mixed workload process\n");
  printf("Final level: %d\n", info.level);

  exit(0);
}