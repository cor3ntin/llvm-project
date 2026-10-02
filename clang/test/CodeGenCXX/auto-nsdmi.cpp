// RUN: %clang_cc1 -std=c++23 -triple x86_64-linux-gnu -fexperimental-auto-nsdmi \
// RUN:   -emit-llvm -o - %s | FileCheck %s

struct S {
  auto a = 42;
  auto b = 3.5;
};

// CHECK-LABEL: define {{.*}}@_Z4makev
// CHECK: store i32 42
// CHECK: store double 3.500000e+00
S make() { return S{}; }

template <typename T>
struct W {
  auto v = T(7);
};

// CHECK-LABEL: define {{.*}}@_Z8make_devv
// CHECK: store i64 7
long make_dev() {
  W<long> w;
  return w.v;
}
