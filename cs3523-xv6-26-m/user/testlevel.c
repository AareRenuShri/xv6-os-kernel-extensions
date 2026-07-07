#include "kernel/types.h"
#include "user/user.h"

int main()
{
  int level = getlevel();
  printf("Current MLFQ level: %d\n", level);
  exit(0);
}