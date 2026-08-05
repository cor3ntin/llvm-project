// RUN: %clang_cc1 -std=c++26 -fcontracts -fsyntax-only -verify %s

// [expr.prim.id.unqual]/7: if an unqualified-id appears in the predicate of a
// contract assertion C and the entity is
//   - a variable declared outside of C of object type T,
//   - a variable or template parameter declared outside of C of type
//     "reference to T", or
//   - a structured binding of type T whose corresponding variable is declared
//     outside of C,
// then the type of the expression is const T.
//
// Note that the rule is keyed on being declared *outside of C*, not on storage
// duration: a namespace-scope variable is const inside a predicate just like an
// automatic one. Constification is also shallow - it does not propagate through
// a pointer dereference - and does not reach entities declared inside the
// predicate itself, including within a lambda contained in it.
//
// This is [expr.prim.id.unqual]/8 Example 2 verbatim, with the standard's own
// comments retained.

int n = 0;
struct X { bool m(); }; // expected-note {{'m' declared here}}
struct Y {
  int z = 0;
  void f(int i, int *p, int &r, X x, X *px)
      pre(++n)     // expected-error {{cannot assign to variable 'n' because it is considered 'const' inside of a contract}}
      pre(++i)     // expected-error {{cannot assign to variable 'i' because it is considered 'const' inside of a contract}}
      pre(++(*p))  // OK
      pre(++r)     // expected-error {{cannot assign to variable 'r' because it is considered 'const' inside of a contract}}
      pre(x.m())   // expected-error {{'this' argument to member function 'm' has type 'const X', but function is not marked const}}
                   // expected-note@-1 {{'this' is 'const' within contract introduced here}}
      pre(px->m()) // OK
      pre([=,&i,*this] mutable {
        ++n;       // expected-error {{cannot assign to variable 'n' because it is considered 'const' inside of a contract}}
        ++i;       // expected-error {{cannot assign to a variable captured by reference which was captured as const because it is inside a contract}}
        ++p;       // OK, refers to member of closure type
        ++r;       // OK, refers to non-reference member of closure type
        ++this->z; // OK, captured *this
        ++z;       // OK, captured *this
        int j = 17;
        [&] {
          int k = 34;
          ++i; // expected-error {{cannot assign to a variable captured by reference which was captured as const because it is inside a contract}}
          ++j; // OK
          ++k; // OK
        }();
        return true;
      }());

  template <int N, int &R, int *P>
  void g()
      pre(++N)     // expected-error {{expression is not assignable}}
      pre(++R)     // expected-error {{cannot assign to variable 'R' because it is considered 'const' inside of a contract}}
      pre(++(*P)); // OK

  int h() post(r : ++r) // expected-error {{cannot assign to variable 'r' because it is considered 'const' inside of a contract}}
      post(r : [=] mutable {
        ++r; // OK, refers to member of closure type
        return true;
      }());

  int &k() post(r : ++r); // expected-error {{cannot assign to variable 'r' because it is considered 'const' inside of a contract}}
};
