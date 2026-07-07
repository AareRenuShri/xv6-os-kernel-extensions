//C2
#include "kernel/types.h"
#include "user/user.h"
int main(void) {
  int count = getsyscount();
  printf("%d\n", count);
  exit(0);
}