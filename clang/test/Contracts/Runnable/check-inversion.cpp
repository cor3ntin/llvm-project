// RUN: %clang -std=c++26 -fcontracts -I%S/../Inputs %s -o %t.exe
// RUN: %t.exe | FileCheck %s

// D4324 assertion_context::check(): the control object, not the compiler,
// decides whether the predicate is evaluated. These are the behaviours that
// were impossible under the old design, where the compiler evaluated the
// predicate first and only called the object on failure.

#include "assertion_control.h"
#include <cstdio>

using namespace std::contracts;

int evaluations = 0;
bool tick(bool r) {
  ++evaluations;
  return r;
}

// Never evaluates: the predicate's side effects must not happen at all.
struct never {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return false; }
  static constexpr bool assumable = false;
  void operator()(const assertion_context &) const {}
};
inline constexpr never never_v{};

// Evaluates three times, to show the object is in charge of how often.
struct thrice {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return false; }
  static constexpr bool assumable = false;
  void operator()(const assertion_context &ctx) const {
    for (int i = 0; i < 3; ++i)
      (void)ctx.check();
  }
};
inline constexpr thrice thrice_v{};

// Reports what the context says about the assertion it was handed.
struct describe {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return false; }
  static constexpr bool assumable = false;
  void operator()(const assertion_context &ctx) const {
    std::printf("kind=%d semantic=%d comment=%s result=%d\n", (int)ctx.kind(),
                (int)ctx.semantic(), ctx.comment(), (int)ctx.check());
  }
};
inline constexpr describe describe_v{};

void unevaluated(int x) pre<never_v>(tick(x > 0)) {}
void repeated(int x) pre<thrice_v>(tick(x > 0)) {}
int described(int x) pre<describe_v>(x > 0) { return x; }
int described_post(const int x) post<describe_v>(x > 0) { return x; }
void described_assert(int x) { contract_assert<describe_v>(x > 0); }

struct S {
  int m;
  int mem(int x) pre<describe_v>(m > x) { return x; }
};

int main() {
  unevaluated(1);
  // CHECK: never evaluated: 0
  std::printf("never evaluated: %d\n", evaluations);

  evaluations = 0;
  repeated(1);
  // CHECK: evaluated repeatedly: 3
  std::printf("evaluated repeatedly: %d\n", evaluations);

  // kind 1 is pre, 2 is post, 3 is assert; semantic 1 is enforce.
  // CHECK: kind=1 semantic=1 comment=x > 0 result=1
  described(5);
  // CHECK: kind=2 semantic=1 comment=x > 0 result=1
  described_post(5);
  // CHECK: kind=3 semantic=1 comment=x > 0 result=0
  described_assert(-5);

  // The predicate reads a member, so the checker has to be handed `this`.
  // CHECK: kind=1 semantic=1 comment=m > x result=1
  S s{10};
  s.mem(3);

  // CHECK: done
  std::printf("done\n");
}
