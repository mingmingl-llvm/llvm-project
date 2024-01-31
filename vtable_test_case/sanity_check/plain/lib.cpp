#include "lib.h"

#include <cstdio>
#include <cstdlib>
#include <memory>

int Derived1::func1(int a, int b) { return a + b; }
int Derived1::func2(int a, int b) { return a * b; }

int Derived2::func1(int a, int b) { return a - b; }

int Derived2::func2(int a, int b) {return a * (a - b); }

namespace {

} // namespace

__attribute__((noinline)) Base *createType(int a) {
  Base *base = nullptr;
  if (a % 4 == 0)
    base = new Derived1();
  else
    base = new Derived2();
  return base;
}
