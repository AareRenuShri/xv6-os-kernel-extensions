//B1
#include "kernel/types.h"
#include "user/user.h"
int main(void) {
  int ppid = getppid();
  printf("%d\n", ppid);
  exit(0);    
}