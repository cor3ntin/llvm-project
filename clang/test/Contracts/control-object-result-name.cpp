// RUN: %clang_cc1 -std=c++26 -fcontracts -fsyntax-only -verify %s

// D4324: a control object reaches the predicate through assertion_context::
// check(), which calls a synthesized function handed the addresses of what the
// predicate reads. A postcondition's result name has no address to hand over:
// CodeGen binds it to the return slot through an opaque value rather than to
// anything in the frame. Until that is wired up the combination is rejected,
// because the alternative is a checker that silently reads the wrong storage.

#include "Inputs/assertion_control.h"

using namespace std::contracts;

// expected-error@+1 {{a postcondition whose predicate names its result is not yet supported with an assertion-control object}}
int named_result(int x) post<mandatory_v>(r: r > 0) { return x; }

// A postcondition that does not name its result is fine.
int unnamed_result(const int x) post<mandatory_v>(x > 0) { return x; }

// As is a precondition, which has no result to name.
int precondition(int x) pre<mandatory_v>(x > 0) { return x; }

// And the result name is still fine without a control object.
int no_control(int x) post(r: r > 0) { return x; }
