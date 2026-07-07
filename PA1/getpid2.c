//A2
#include "kernel/types.h"
#include "user/user.h"
int main(void) {
  int pid = getpid();
  printf("%d\n", pid);
  exit(0);    
}