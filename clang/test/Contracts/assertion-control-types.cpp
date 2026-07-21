// RUN: %clang_cc1 -std=c++26 -fcontracts -fsyntax-only -verify %s
// expected-no-diagnostics

// Validates the D4324 assertion-control library surface in isolation, before
// the compiler learns to read these types off pre/post/contract_assert.

#include "Inputs/assertion_control.h"

using namespace std::contracts;

// The three worked control objects model the concept; an unrelated type does not.
static_assert(assertion_control<default_control>);
static_assert(assertion_control<review>);
static_assert(assertion_control<mandatory>);
static_assert(!assertion_control<int>);

// default_control: consistent (no constify), non-optimizable, ignored only at 'ignore'.
static_assert(default_control::constify == false);
static_assert(default_control::assumable == false);
static_assert(default_control::is_ignored(evaluation_config::ignore));
static_assert(!default_control::is_ignored(evaluation_config::enforce));

// review: constified, always checked.
static_assert(review::constify == true);
static_assert(review::assumable == false);
static_assert(!review::is_ignored(evaluation_config::ignore));

// mandatory: guaranteed-enforced and optimizable.
static_assert(mandatory::assumable == true);
static_assert(mandatory::constify == false);
static_assert(!mandatory::is_ignored(evaluation_config::ignore));
