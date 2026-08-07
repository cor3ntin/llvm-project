// RUN: %clang -std=c++26 -fcontracts -I%S/../Inputs %s -o %t.exe
// RUN: %t.exe | FileCheck %s

// D4324: a postcondition's result name has to reach the predicate with the value
// the function actually returned. Getting the address right is not enough: the
// store into the return slot is normally elided when the only thing that reads
// it is the return itself, which left the predicate reading uninitialized
// memory. These cases would pass by luck if that were still happening, so they
// check the answer rather than just that something ran.

#include "assertion_control.h"
#include <cstdio>

using namespace std::contracts;

struct report {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  void operator()(const assertion_context &ctx) const {
    std::printf("%s -> %s\n", ctx.comment(), ctx.check() ? "true" : "false");
  }
};
inline constexpr report report_v{};

int scalar(int x) post<report_v>(r: r == 42) { return x; }
long widened(int x) post<report_v>(r: r == 120) { return x * 2L; }

struct S {
  int m;
};
S indirect(const int v) post<report_v>(r: r.m == v) { return S{v}; }

struct T {
  int limit;
  int clamp(const int v) const post<report_v>(r: r <= limit && r <= v) {
    return v < limit ? v : limit;
  }
};

int main() {
  // The value returned is what the predicate sees, both ways round.
  // CHECK: r: r == 42 -> true
  scalar(42);
  // CHECK: r: r == 42 -> false
  scalar(7);

  // A result wider than the parameter: 60 * 2 == 120.
  // CHECK: r: r == 120 -> true
  widened(60);

  // An indirectly returned class, read alongside a parameter.
  // CHECK: r: r.m == v -> true
  indirect(5);

  // The result together with a parameter and a member.
  // CHECK: r: r <= limit && r <= v -> true
  T{10}.clamp(3);
  // CHECK: r: r <= limit && r <= v -> true
  T{10}.clamp(30);

  // CHECK: done
  std::printf("done\n");
}
