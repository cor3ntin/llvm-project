// RUN: %clang_cc1 -std=c++26 -fcontracts -fsyntax-only -verify %s

// D4324: a control object does not have to be a namespace-scope variable of a
// concrete type. It can be a member of a class template, be reached through a
// dependent qualifier, arrive as a template parameter, or inherit the interface
// the compiler reads off it. Each of those defers the decisions the compiler
// makes about a contract - whether it is ignored, whether the predicate is
// constified - until instantiation, so this checks they are made per
// instantiation rather than once on the template pattern.

#include "Inputs/assertion_control.h"

using namespace std::contracts;

struct base_ctl {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return false; }
  void operator()(const assertion_context &ctx) const {
    if (!ctx.check())
      __builtin_trap();
  }
};

// The compiler looks through bases for the members it needs, so a control
// object may inherit the whole interface rather than restate it.
struct derived_ctl : base_ctl {};
inline constexpr derived_ctl derived_v{};
int inherited(int x) pre<derived_v>(x > 0) { return x; }

namespace nested_in_template {

template <class T> struct Outer {
  // The control object is a nested class of a class template, so both its type
  // and its value depend on T.
  struct Ctl : base_ctl {};
  static constexpr Ctl ctl{};

  // Named from a member function of the same template.
  int mem(int x) pre<ctl>(x > 0) { return x; }

  // Named from a nested class, where the enclosing specialization is still
  // dependent while the pattern is parsed.
  struct Inner {
    int f(int x) pre<Outer::ctl>(x > 0) { return x; }
  };

  // And from a nested class template, one more level of dependence.
  template <class U> struct InnerT {
    int f(int x) pre<Outer::ctl>(x > 0) { return x; }
  };
};

// Naming a specialization's member from outside, with concrete arguments.
int outside(int x) pre<Outer<int>::ctl>(x > 0) { return x; }

// A dependent qualifier: Outer<T>::ctl is not resolvable until T is known.
template <class T> int dependent_qual(int x) pre<Outer<T>::ctl>(x > 0) {
  return x;
}

void use() {
  Outer<int> o;
  o.mem(1);
  Outer<int>::Inner{}.f(1);
  Outer<int>::InnerT<char>{}.f(1);
  outside(1);
  dependent_qual<int>(1);
  dependent_qual<double>(1);
}

} // namespace nested_in_template

namespace as_template_parameter {

// The object itself arrives as a reference non-type template parameter.
template <auto &C> int by_nttp(int x) pre<C>(x > 0) { return x; }

// Only the type arrives; the object is reached through it.
template <class C> int by_type(int x) pre<C::instance>(x > 0) { return x; }
struct holder {
  static constexpr base_ctl instance{};
};

void use() {
  by_nttp<derived_v>(1);
  by_nttp<nested_in_template::Outer<long>::ctl>(1);
  by_type<holder>(1);
}

} // namespace as_template_parameter

namespace constify_per_instantiation {

// Whether the predicate is constified is the control object's call, and here
// that answer differs between specializations.
template <bool Constify> struct ctl {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return Constify; }
  void operator()(const assertion_context &ctx) const {
    if (!ctx.check())
      __builtin_trap();
  }
};

template <class T> struct Outer {
  static constexpr ctl<sizeof(T) == 1> c{};

  // Mutating a parameter is well-formed only where constify() answers false.
  // The pattern itself must not be constified: the object has not been asked
  // yet, and constifying here would reject every specialization.
  // expected-error@+1 {{cannot assign to variable 'x' because it is considered 'const' inside of a contract}}
  static int mutate(int x) pre<c>(++x > 0) { return x; }
};

void use() {
  Outer<int>::mutate(1); // sizeof(int) != 1, constify() is false: accepted

  // expected-note@+1 {{in instantiation of member function 'constify_per_instantiation::Outer<char>::mutate' requested here}}
  Outer<char>::mutate(1); // sizeof(char) == 1, constify() is true: rejected
}

// The same policy split, reached through a nested class rather than directly.
template <class T> struct Wrapper {
  struct Nested {
    static constexpr ctl<sizeof(T) == 1> c{};
    // expected-error@+1 {{cannot assign to variable 'x' because it is considered 'const' inside of a contract}}
    static int mutate(int x) pre<c>(++x > 0) { return x; }
  };
};

void use_nested() {
  Wrapper<int>::Nested::mutate(1);

  // expected-note@+1 {{in instantiation of member function 'constify_per_instantiation::Wrapper<char>::Nested::mutate' requested here}}
  Wrapper<char>::Nested::mutate(1);
}

} // namespace constify_per_instantiation
