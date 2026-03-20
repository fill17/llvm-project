// RUN: %VarRenamer %s -- | %FileCheck %s

int var1 = 0;

int foo(int a, int b) {
  static int var2 = 0;
  int var3 = 123;
  ++var2;
  return a + b + var1 + var2 + var3;
}

// CHECK: int global_var1 = 0;
// CHECK: int foo(int param_a, int param_b) {
// CHECK:   static int static_var2 = 0;
// CHECK:   int local_var3 = 123;
// CHECK:   ++static_var2;
// CHECK:   return param_a + param_b + global_var1 + static_var2 + local_var3;
