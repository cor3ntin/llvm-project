// RUN: %clang_cc1 -std=c++26 -fcontracts -fsyntax-only -verify %s

// D4324: a contract that names a control object is driven by that object during
// constant evaluation too, not just at run time. The object's operator() runs,
// and assertion_context::check() evaluates the predicate - through
// __builtin_contract_check(), since the type-erased function pointer check()
// calls at run time has no body an evaluator could walk.
//
// Each rejection below produces the same three notes: one at the operation that
// is not a constant expression, one at the dispatch call, and one at the
// static_assert.

#include "Inputs/assertion_control.h"

using namespace std::contracts;

// Shrugs at a failed predicate.
struct lenient {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return false; }
  constexpr void operator()(const assertion_context &) const {}
};
inline constexpr lenient lenient_v{};

// A failed predicate is fatal.
struct strict {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return false; }
  constexpr void operator()(const assertion_context &ctx) const {
    if (!ctx.check())
      __builtin_trap(); // expected-note 3 {{subexpression not valid in a constant expression}}
  }
};
inline constexpr strict strict_v{};

// Never looks at the predicate at all.
struct never {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return false; }
  constexpr void operator()(const assertion_context &) const {}
};
inline constexpr never never_v{};

// Reported to the compiler as ignored, so never dispatched.
struct ignored {
  static consteval bool is_ignored(assertion_static_info) { return true; }
  static consteval bool constify(assertion_static_info) { return false; }
  constexpr void operator()(const assertion_context &) const {}
};
inline constexpr ignored ignored_v{};

namespace who_decides {

constexpr int lenient_fn(int x) pre<lenient_v>(x > 0) { return x; }
constexpr int strict_fn(int x) pre<strict_v>(x > 0) { return x; } // expected-note {{in call to}}

// Predicate holds: both objects agree.
static_assert(lenient_fn(1) == 1);
static_assert(strict_fn(1) == 1);

// Predicate fails, and the object decides what that means. The lenient one lets
// the constant expression stand.
static_assert(lenient_fn(-1) == -1);

// The strict one does not.
// expected-error@+2 {{static assertion expression is not an integral constant expression}}
// expected-note@+1 {{in call to}}
static_assert(strict_fn(-1) == -1);

// A contract its object reports as ignored is not evaluated, so a false
// predicate is harmless.
constexpr int ignored_fn(int x) pre<ignored_v>(x > 0) { return x; }
static_assert(ignored_fn(-1) == -1);

// Nor can an object that never calls check() reject anything.
constexpr int never_fn(int x) pre<never_v>(x > 0) { return x; }
static_assert(never_fn(-1) == -1);

} // namespace who_decides

namespace what_the_predicate_reads {

// The predicate must be evaluated in the frame of the function the contract is
// attached to, not the control object's. When that is wrong these fail with
// 'parameter with unknown value' instead of the contract's own answer.
constexpr int two_params(int a, int b) pre<strict_v>(a > b) { return a - b; } // expected-note {{in call to}}
static_assert(two_params(5, 3) == 2);
// expected-error@+2 {{static assertion expression is not an integral constant expression}}
// expected-note@+1 {{in call to}}
static_assert(two_params(3, 5) == -2);

struct S {
  int m;
  // Reads a member, so the predicate needs `this` from the right frame.
  constexpr int mem(int x) const pre<strict_v>(m > x) { return x; } // expected-note {{in call to}}
};
static_assert(S{10}.mem(3) == 3);
// expected-error@+2 {{static assertion expression is not an integral constant expression}}
// expected-note@+1 {{in call to}}
static_assert(S{1}.mem(3) == 3);

// A local read by a contract_assert.
constexpr int local() {
  int n = 7;
  contract_assert<strict_v>(n == 7);
  return n;
}
static_assert(local() == 7);

} // namespace what_the_predicate_reads

namespace repeated_evaluation {

// check() may be called more than once, and a side-effect-free predicate must
// give the same answer each time.
struct twice {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return false; }
  constexpr void operator()(const assertion_context &ctx) const {
    const bool first = ctx.check();
    const bool second = ctx.check();
    if (first != second)
      __builtin_trap();
    if (!first)
      __builtin_trap(); // expected-note {{subexpression not valid in a constant expression}}
  }
};
inline constexpr twice twice_v{};

constexpr int f(int x) pre<twice_v>(x > 0) { return x; } // expected-note {{in call to}}
static_assert(f(4) == 4);
// expected-error@+2 {{static assertion expression is not an integral constant expression}}
// expected-note@+1 {{in call to}}
static_assert(f(-4) == -4);

} // namespace repeated_evaluation

namespace context_at_compile_time {

// The static properties the compiler describes are readable during constant
// evaluation, so an object can branch on them.
struct describing {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return false; }
  constexpr void operator()(const assertion_context &ctx) const {
    if (ctx.kind() != assertion_kind::pre)
      __builtin_trap(); // expected-note {{subexpression not valid in a constant expression}}
    if (ctx.semantic() != evaluation_semantic::enforce)
      __builtin_trap();
    if (ctx.static_info().side() != assertion_check_side::definition)
      __builtin_trap();
    if (!ctx.check())
      __builtin_trap();
  }
};
inline constexpr describing describing_v{};

// kind() is pre, semantic() is enforce, side() is definition: all as described.
constexpr int as_pre(int x) pre<describing_v>(x > 0) { return x; }
static_assert(as_pre(1) == 1);

// The same object on a contract_assert sees kind() == assert, so it trips the
// first check above - which is what proves kind() is really being read.
constexpr int as_assert(int x) {
  contract_assert<describing_v>(x > 0); // expected-note {{in call to}}
  return x;
}
// expected-error@+2 {{static assertion expression is not an integral constant expression}}
// expected-note@+1 {{in call to}}
static_assert(as_assert(1) == 1);

} // namespace context_at_compile_time
