// RUN: %clang_cc1 -std=c++26 -fcontracts -fsyntax-only -verify %s
// expected-no-diagnostics

// D4324 makes constification a per-assertion policy chosen by the control
// object's `constify` member rather than a build-wide rule. This exercises the
// plumbing (shouldConstifyContractPredicate + the per-contract Constify flag)
// across control objects with constify true and false.
//
// Note: the positive effect (constify == true rejecting a mutation) cannot be
// checked here because entity constification is not yet functional on this base
// branch (see the pre-existing constification.cpp failure). D4324's default is
// no constification, which this design already delivers for default_control.

#include "Inputs/assertion_control.h"
using namespace std::contracts;

int p_default(int x) pre(x > 0) { return x; }
int p_review(int x) pre<review_v>(x > 0) { return x; }       // constify == true
int p_mandatory(int x) pre<mandatory_v>(x > 0) { return x; } // constify == false

void a_default(int x) { contract_assert(x > 0); }
void a_review(int x) { contract_assert<review_v>(x > 0); }
void a_mandatory(int x) { contract_assert<mandatory_v>(x > 0); }

// A temporary control object picks up its own type's policy just like a named
// one does.
int p_temp(int x) pre<review{}>(x > 0) { return x; }

// Also exercises the instantiation path (TransformContractStmt computes the
// constify policy from the substituted control object).
template <auto C> int tpl(int x) pre<C>(x > 0) { return x; }
template int tpl<review_v>(int);
template int tpl<mandatory_v>(int);
