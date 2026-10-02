// RUN: %clang_cc1 -std=c++23 -fsyntax-only -verify -fexperimental-auto-nsdmi %s

namespace NoInitializer {
// The type is deduced from the default member initializer, so there must be
// one.
struct S {
  auto x; // expected-error {{declaration of variable 'x' with deduced type 'auto' requires an initializer}}
};
} // namespace NoInitializer

namespace OwnClass {
// The member's type cannot be the class being defined; it has no layout yet.
struct S { // expected-note {{definition of 'OwnClass::S' is not complete until the closing '}'}}
  auto x = S{}; // expected-error {{invalid use of incomplete type 'S'}}
};

struct T { // expected-note {{definition of 'OwnClass::T' is not complete until the closing '}'}}
  auto x = sizeof(T); // expected-error {{invalid application of 'sizeof' to an incomplete type 'T'}}
};

// Reached through a pointer, the deduced type is still the enclosing class.
struct U {
  auto x = *(U *)nullptr; // expected-error {{data member 'x' cannot have deduced type 'U', which is the class whose definition it appears in}}
};
} // namespace OwnClass

namespace DeductionFailure {
struct S {
  auto x = {1, 2}; // expected-error {{cannot deduce type of initializer list because std::initializer_list was not found}}
};
} // namespace DeductionFailure

namespace StillAnError {
// A placeholder type remains invalid where it has nothing to be deduced from.
struct S {
  static auto s; // expected-error {{declaration of variable 's' with deduced type 'auto' requires an initializer}}
};

struct Bitfield {
  auto b : 3 = 1; // expected-error {{bit-field 'b' has non-integral type 'auto'}}
};
} // namespace StillAnError

namespace Friend {
struct S {
  friend auto; // expected-error {{'auto' not allowed in friend declaration}}
};
} // namespace Friend
