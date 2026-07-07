#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  printf("syscalls = %d\n", getsyscount());
  printf("syscalls = %d\n", getsyscount());
  exit(0);
}
