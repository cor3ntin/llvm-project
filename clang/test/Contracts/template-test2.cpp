// RUN: %clang_cc1 -std=c++26 -fcontracts -fsyntax-only -verify %s

// Contracts inside templates, including inside lambdas in local classes, and
// the constification rules those go through on instantiation.
//
// [expr.prim.id.unqual]/7 makes an id-expression naming a variable declared
// outside the contract assertion an lvalue of const type, so incrementing a
// parameter or a local from within a predicate is ill-formed. That holds in a
// template just as it does outside one; the diagnostic is issued when the
// contract is instantiated.

namespace BasicTest {
template <class T>
T f(T x) {
  T local = x;
  contract_assert(++x);     // expected-error {{cannot assign to variable 'x' because it is considered 'const' inside of a contract}}
  contract_assert(++local); // expected-error {{cannot assign to variable 'local' because it is considered 'const' inside of a contract}}
  return x;
}

void basic() {
  f(1); // expected-note {{in instantiation of function template specialization 'BasicTest::f<int>' requested here}}
}
} // namespace BasicTest

namespace LambdaTest {
template <class T>
void f(T x) {
  T local = x;
  contract_assert(++x);     // expected-error {{cannot assign to variable 'x' because it is considered 'const' inside of a contract}}
  contract_assert(++local); // expected-error {{cannot assign to variable 'local' because it is considered 'const' inside of a contract}}

  // A contract assertion in the body of a lambda, in a local class, in a
  // template. Whether the predicate may modify the named entity depends on how
  // the lambda captured it.
  struct X { // expected-note {{in instantiation of member function 'LambdaTest::f(int)::X::by_reference' requested here}}
             // expected-note@-1 {{in instantiation of member function 'LambdaTest::f(int)::X::contract_only' requested here}}
    // Captured by copy, so [expr.prim.id.unqual]/4 applies before /7: the
    // id-expression has the type of the closure member, which is non-const
    // because the lambda is mutable.
    auto explicit_copy(T z) {
      return [z]() mutable { contract_assert(++z); };
    }

    // Same, reached through an implicit capture. The capture is well formed
    // because z is also referenced outside the contract assertion.
    auto implicit_copy(T z) {
      return [=]() mutable {
        (void)z;
        contract_assert(++z);
      };
    }

    // Captured by reference, so the id-expression still denotes z itself and
    // /7 constifies it.
    auto by_reference(T z) {
      return [&z]() mutable { // expected-note {{while substituting into a lambda expression here}}
        contract_assert(++z); // expected-error {{cannot assign to a variable captured by reference which was captured as const because it is inside a contract}}
      };
    }

    // [expr.prim.lambda.capture]/10: every potential reference to z is inside
    // an assertion-statement in the body of the lambda, so z cannot be
    // implicitly captured here.
    auto contract_only(T z) {
      return [=]() mutable { // expected-note {{while substituting into a lambda expression here}}
        contract_assert(++z); // expected-error {{cannot assign to a variable captured by reference which was captured as const because it is inside a contract}}
      };
    }
  };

  X y;
  y.explicit_copy(x);
  y.implicit_copy(x);
  y.by_reference(x);
  y.contract_only(x);
}

void instant() {
  f(42); // expected-note 3 {{in instantiation of function template specialization 'LambdaTest::f<int>' requested here}}
}
} // namespace LambdaTest
