// RUN: %clang_cc1 -std=c++26 -fcontracts -fsyntax-only -verify %s
// expected-no-diagnostics

// D4324: a postcondition may name its result while naming a control object. The
// result is not a variable in the frame - it is the function's return slot - so
// the checker is handed that slot's address like any other capture, and CodeGen
// binds the result name to it rather than to the checker's own return value.

#include "Inputs/assertion_control.h"

using namespace std::contracts;

int scalar(int x) post<mandatory_v>(r: r > 0) { return x; }

// A wider result than the parameter, so the two cannot be confused.
long widened(int x) post<mandatory_v>(r: r > 100) { return x * 2L; }

// A class result, which is returned indirectly.
struct S {
  int m;
};
S indirect(const int v) post<mandatory_v>(r: r.m == v) { return S{v}; }

// The result alongside other captures: a parameter and a member.
struct T {
  int limit;
  int clamp(const int v) const post<mandatory_v>(r: r <= limit && r <= v) {
    return v < limit ? v : limit;
  }
};

// A result name in a contract that also reads nothing else.
int constant() post<mandatory_v>(r: r == 7) { return 7; }

// Several postconditions on one function, each naming the result.
int several(const int x) post<mandatory_v>(r: r > 0) post<review_v>(r: r == x) {
  return x;
}

// A reference result is not covered here: a reference-returning function with a
// result-name postcondition crashes CodeGen even with no control object in
// sight, because EmitPostContracts builds an OpaqueValueExpr of the result's
// declared type and expressions cannot have reference type. Fixing that also
// means reading the referent out of the return slot, which holds a pointer.

// And with no control object, which is the path that was silently reading
// uninitialized memory before the return-slot store was kept.
int plain(int x) post(r: r > 0) { return x; }
