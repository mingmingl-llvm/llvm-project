#include "lib.h"
#include <cstdio>
#include <cstdlib>

// https://gcc.godbolt.org/z/5vjr5Eqnr
int main(int argc, char **argv) {
  int sum = 0;
  for (int i = 0; i < 1000; i++) {
    int a = rand();
    int b = rand();
    Base *ptr = createType(i);
    sum += ptr->func1(a, b) + ptr->func2(b, a);
    delete ptr;
  }
  printf("sum is %d\n", sum);
  return 0;
}
