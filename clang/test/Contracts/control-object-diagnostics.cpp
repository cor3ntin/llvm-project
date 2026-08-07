// RUN: %clang_cc1 -std=c++26 -fcontracts -fsyntax-only -verify %s

// D4324: what the compiler requires of a control object, and what it says when an
// object does not provide it. The calls the compiler makes are synthesized, so a
// mismatch must be reported against the object rather than as an
// overload-resolution failure in code the user never wrote.

#include "Inputs/assertion_control.h"

using namespace std::contracts;

// The minimum: is_ignored and operator(). constify is optional.
struct minimal {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  void operator()(const assertion_context &ctx) const {
    if (!ctx.check())
      __builtin_trap();
  }
};
inline constexpr minimal minimal_v{};
int ok(int x) pre<minimal_v>(x > 0) { return x; }

// Without constify, predicates are not constified, so mutation is allowed.
int mutates(int x) pre<minimal_v>(++x > 0) { return x; }

// Opting in to constification rejects the same predicate.
struct constifying {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return true; }
  void operator()(const assertion_context &ctx) const {
    if (!ctx.check())
      __builtin_trap();
  }
};
inline constexpr constifying constifying_v{};
// expected-error@+1 {{cannot assign to variable 'x' because it is considered 'const' inside of a contract}}
int constified(int x) pre<constifying_v>(++x > 0) { return x; }

// An object providing nothing at all.
struct nothing {};
inline constexpr nothing nothing_v{};
// expected-error@+2 {{assertion-control object type 'const nothing' is missing required member 'is_ignored'}}
// expected-error@+1 {{assertion-control object type 'const nothing' is missing required member 'operator()'}}
int no_members(int x) pre<nothing_v>(x > 0) { return x; }

// is_ignored present but not callable with an assertion_static_info.
struct bad_is_ignored {
  static consteval bool is_ignored() { return false; }
  void operator()(const assertion_context &ctx) const { (void)ctx.check(); }
};
inline constexpr bad_is_ignored bad_is_ignored_v{};
// expected-error@+1 {{member 'is_ignored' of assertion-control object type 'const bad_is_ignored' must be callable as 'is_ignored(std::contracts::assertion_static_info)' and return a constant of type bool}}
int wrong_is_ignored(int x) pre<bad_is_ignored_v>(x > 0) { return x; }

// constify present but not callable that way. Being optional does not mean a
// wrong one is quietly ignored.
struct bad_constify {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify() { return true; }
  void operator()(const assertion_context &ctx) const { (void)ctx.check(); }
};
inline constexpr bad_constify bad_constify_v{};
// expected-error@+1 {{member 'constify' of assertion-control object type 'const bad_constify' must be callable as 'constify(std::contracts::assertion_static_info)' and return a constant of type bool}}
int wrong_constify(int x) pre<bad_constify_v>(x > 0) { return x; }

// These must be consteval. constexpr is not enough: a constexpr function can
// also be called at run time, so it does not on its face fix the policy at
// compile time.
bool runtime_flag(); // expected-note {{declared here}}
struct plain_constexpr {
  // expected-note@+1 {{declared here}}
  static constexpr bool is_ignored(assertion_static_info) { return false; }
  void operator()(const assertion_context &ctx) const { (void)ctx.check(); }
};
inline constexpr plain_constexpr plain_constexpr_v{};
// expected-error@+1 {{member 'is_ignored' of assertion-control object type 'const plain_constexpr' must be declared consteval; it decides how the contract is compiled, so it cannot be a function that might only be known at run time}}
int constexpr_rejected(int x) pre<plain_constexpr_v>(x > 0) { return x; }

// The optional constify is held to the same rule when it is present.
struct constexpr_constify {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  // expected-note@+1 {{declared here}}
  static constexpr bool constify(assertion_static_info) { return true; }
  void operator()(const assertion_context &ctx) const { (void)ctx.check(); }
};
inline constexpr constexpr_constify constexpr_constify_v{};
// expected-error@+1 {{member 'constify' of assertion-control object type 'const constexpr_constify' must be declared consteval}}
int constexpr_constify_rejected(int x) pre<constexpr_constify_v>(x > 0) {
  return x;
}

// Not constexpr at all is likewise rejected, and for the same reason.
struct not_constexpr {
  // expected-note@+1 {{declared here}}
  static bool is_ignored(assertion_static_info) { return runtime_flag(); }
  void operator()(const assertion_context &ctx) const { (void)ctx.check(); }
};
inline constexpr not_constexpr not_constexpr_v{};
// expected-error@+1 {{member 'is_ignored' of assertion-control object type 'const not_constexpr' must be declared consteval}}
int no_constexpr(int x) pre<not_constexpr_v>(x > 0) { return x; }

// Requiring consteval means a member that cannot produce a constant is reported
// by the immediate-invocation machinery, pointing at what actually went wrong
// inside it rather than at the contract.
struct consteval_not_constant {
  static consteval bool is_ignored(assertion_static_info) {
    // expected-note@+1 {{non-constexpr function 'runtime_flag' cannot be used in a constant expression}}
    return runtime_flag();
  }
  void operator()(const assertion_context &ctx) const { (void)ctx.check(); }
};
inline constexpr consteval_not_constant consteval_not_constant_v{};
// expected-error@+2 {{call to consteval function 'consteval_not_constant::is_ignored' is not a constant expression}}
// expected-note@+1 {{in call to}}
int not_constant(int x) pre<consteval_not_constant_v>(x > 0) { return x; }

// A non-static member cannot be called without an object, so it does not count
// as providing the policy - and saying so must not leak the consteval
// address-of error that naming it would otherwise produce.
struct non_static {
  consteval bool is_ignored(assertion_static_info) const { return false; }
  void operator()(const assertion_context &ctx) const { (void)ctx.check(); }
};
inline constexpr non_static non_static_v{};
// expected-error@+1 {{member 'is_ignored' of assertion-control object type 'const non_static' must be callable as 'is_ignored(std::contracts::assertion_static_info)' and return a constant of type bool}}
int instance_member(int x) pre<non_static_v>(x > 0) { return x; }

// A data member of the right type is not callable either.
struct data_member {
  static constexpr bool is_ignored = false;
  void operator()(const assertion_context &ctx) const { (void)ctx.check(); }
};
inline constexpr data_member data_member_v{};
// expected-error@+1 {{member 'is_ignored' of assertion-control object type 'const data_member' must be callable as 'is_ignored(std::contracts::assertion_static_info)' and return a constant of type bool}}
int not_a_function(int x) pre<data_member_v>(x > 0) { return x; }

// is_ignored of the right arity but the wrong parameter type.
struct wrong_param {
  static consteval bool is_ignored(int) { return false; }
  void operator()(const assertion_context &ctx) const { (void)ctx.check(); }
};
inline constexpr wrong_param wrong_param_v{};
// expected-error@+1 {{member 'is_ignored' of assertion-control object type 'const wrong_param' must be callable as 'is_ignored(std::contracts::assertion_static_info)' and return a constant of type bool}}
int bad_param(int x) pre<wrong_param_v>(x > 0) { return x; }

// is_ignored taking one argument too many.
struct extra_param {
  static consteval bool is_ignored(assertion_static_info, int) { return false; }
  void operator()(const assertion_context &ctx) const { (void)ctx.check(); }
};
inline constexpr extra_param extra_param_v{};
// expected-error@+1 {{member 'is_ignored' of assertion-control object type 'const extra_param' must be callable as 'is_ignored(std::contracts::assertion_static_info)' and return a constant of type bool}}
int too_many(int x) pre<extra_param_v>(x > 0) { return x; }

// The same two mistakes in the optional constify.
struct constify_wrong_param {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(int) { return true; }
  void operator()(const assertion_context &ctx) const { (void)ctx.check(); }
};
inline constexpr constify_wrong_param constify_wrong_param_v{};
// expected-error@+1 {{member 'constify' of assertion-control object type 'const constify_wrong_param' must be callable as 'constify(std::contracts::assertion_static_info)' and return a constant of type bool}}
int bad_constify_param(int x) pre<constify_wrong_param_v>(x > 0) { return x; }

struct constify_extra_param {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info, int) { return true; }
  void operator()(const assertion_context &ctx) const { (void)ctx.check(); }
};
inline constexpr constify_extra_param constify_extra_param_v{};
// expected-error@+1 {{member 'constify' of assertion-control object type 'const constify_extra_param' must be callable as 'constify(std::contracts::assertion_static_info)' and return a constant of type bool}}
int constify_too_many(int x) pre<constify_extra_param_v>(x > 0) { return x; }

// operator() that does not accept an assertion_context.
struct bad_call {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  void operator()(int) const {}
};
inline constexpr bad_call bad_call_v{};
// expected-error@+2 {{no matching function for call to object of type 'const bad_call'}}
// expected-note@-4 {{candidate function not viable}}
int wrong_call(int x) pre<bad_call_v>(x > 0) { return x; }

// operator() taking nothing at all.
struct call_no_args {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  void operator()() const {}
};
inline constexpr call_no_args call_no_args_v{};
// expected-error@+2 {{no matching function for call to object of type 'const call_no_args'}}
// expected-note@-4 {{candidate function not viable: requires 0 arguments, but 1 was provided}}
int call_zero(int x) pre<call_no_args_v>(x > 0) { return x; }

// operator() wanting an extra argument the compiler does not supply.
struct call_two_args {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  void operator()(const assertion_context &, int) const {}
};
inline constexpr call_two_args call_two_args_v{};
// expected-error@+2 {{no matching function for call to object of type 'const call_two_args'}}
// expected-note@-4 {{candidate function not viable: requires 2 arguments, but 1 was provided}}
int call_two(int x) pre<call_two_args_v>(x > 0) { return x; }

// A control object has to be a class.
inline constexpr int not_a_class = 0;
// expected-error@+1 {{assertion-control object must have class type, not 'const int'}}
int non_class(int x) pre<not_a_class>(x > 0) { return x; }
