// C3 
#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  int pid = fork();

  if (pid < 0) {
    printf("fork failed\n");
    exit(1);
  }

  if (pid == 0) { // child
    getpid();     
    getpid();     
    uptime();     
    exit(0);
  } else { //parent
    wait(0);

    int count = getchildsyscount(pid);
    printf("Child (pid %d) syscall count: %d\n", pid, count);
  }

  exit(0);
}
