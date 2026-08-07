// RUN: %clang_cc1 -std=c++26 -fcontracts -fsyntax-only -verify %s

// Checks that a named assertion-control object must model the interface the
// compiler reads (D4324): a class exposing is_ignored, constify, assumable, and
// a call operator. Unlike the earlier type-based form, the class need not be
// empty - naming an object is what makes per-assertion state possible.

#include "Inputs/assertion_control.h"
using namespace std::contracts;

// The worked control objects are accepted.
int ok1(int x) pre<default_v>(x > 0) { return x; }
int ok2(int x) pre<review_v>(x > 0) { return x; }
void ok3(int x) { contract_assert<mandatory_v>(x > 0); }

// Not of class type.
int bad1(int x) pre<0>(x > 0) { return x; }
// expected-error@-1 {{assertion-control object must have class type, not 'int'}}

// A class with state is fine now: the control object is a value, so it may
// carry per-assertion data.
struct WithState {
  const char *label;
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return false; }
  void operator()(const assertion_context &ctx) const {
    (void)ctx.check();
  }
};
inline constexpr WithState labeled_v{"a label"};
int ok4(int x) pre<labeled_v>(x > 0) { return x; }

// An incomplete class type is rejected.
struct Incomplete; // expected-note {{forward declaration of 'Incomplete'}}
extern const Incomplete incomplete_v;
int bad2(int x) pre<incomplete_v>(x > 0) { return x; }
// expected-error@-1 {{assertion-control object has incomplete type 'const Incomplete'}}

// A class missing the required members is rejected, one diagnostic per member.
// Only is_ignored and operator() are required; constify is optional and
// defaults to false. See control-object-diagnostics.cpp.
struct Missing {};
inline constexpr Missing missing_v{};
int bad3(int x) pre<missing_v>(x > 0) { return x; }
// expected-error@-1 {{assertion-control object type 'const Missing' is missing required member 'is_ignored'}}
// expected-error@-2 {{assertion-control object type 'const Missing' is missing required member 'operator()'}}

// A dependent control object defers checking to instantiation: no error here.
template <auto C> int tpl(int x) pre<C>(x > 0) { return x; }
template int tpl<review_v>(int); // fine
