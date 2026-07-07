#include "kernel/types.h"
#include "user/user.h"

int main() {
  int pid = getpid();

  for(long i = 0; i < 1000000000; i++); // busy loop

  struct mlfqinfo info;
  getmlfqinfo(pid, &info);

  printf("CPU-bound process\n");
  printf("Final level: %d\n", info.level);

  exit(0);
}