// RUN: %clang_cc1 -std=c++26 -fcontracts -fsyntax-only -verify %s

// Checks that a named assertion-control object type must model the interface
// the compiler reads (D4324): an empty class with is_ignored, constify,
// assumable, and a call operator.

#include "Inputs/assertion_control.h"
using namespace std::contracts;

// The worked control objects are accepted.
int ok1(int x) pre<default_control>(x > 0) { return x; }
int ok2(int x) pre<review>(x > 0) { return x; }
void ok3(int x) { contract_assert<mandatory>(x > 0); }

// Not a class type.
int bad1(int x) pre<int>(x > 0) { return x; }
// expected-error@-1 {{assertion-control object type 'int' must be a class type}}

// A class with state is rejected.
struct NonEmpty {
  int state;
  static constexpr bool is_ignored(evaluation_config) { return false; }
  static constexpr bool constify = false;
  static constexpr bool assumable = false;
  violation_response operator()(const char *, std::source_location,
                                evaluation_config) const;
};
int bad2(int x) pre<NonEmpty>(x > 0) { return x; }
// expected-error@-1 {{assertion-control object type 'NonEmpty' must be an empty class}}

// A class missing the required members is rejected, one diagnostic per member.
struct Missing {};
int bad3(int x) pre<Missing>(x > 0) { return x; }
// expected-error@-1 4 {{is missing required member}}

// A dependent control type defers checking to instantiation: no error here.
template <class C> int tpl(int x) pre<C>(x > 0) { return x; }
template int tpl<review>(int); // fine
