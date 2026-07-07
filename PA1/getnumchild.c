//B2
#include "kernel/types.h"
#include "user/user.h"
int main(void) {
  int gnc = getnumchild();
  printf("%d\n", gnc);
  exit(0);    
}