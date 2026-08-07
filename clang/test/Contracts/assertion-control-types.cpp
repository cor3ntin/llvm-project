// RUN: %clang_cc1 -std=c++26 -fcontracts -fsyntax-only -verify %s
// expected-no-diagnostics

// Validates the D4324 assertion-control library surface in isolation, before
// the compiler learns to read these types off pre/post/contract_assert.

#include "Inputs/assertion_control.h"

using namespace std::contracts;

// A stand-in for what the compiler synthesizes, so the static properties can be
// exercised here without a contract in sight.
consteval assertion_static_info make_info(evaluation_semantic sem,
                                          assertion_check_side side) {
  return std::__create_assertion_static_info(sem, side, false, false);
}

// assertion_static_info reports back exactly what it was created with.
static_assert(make_info(evaluation_semantic::enforce,
                        assertion_check_side::definition)
                  .semantic() == evaluation_semantic::enforce);
static_assert(make_info(evaluation_semantic::ignore,
                        assertion_check_side::not_applicable)
                  .side() == assertion_check_side::not_applicable);
static_assert(make_info(evaluation_semantic::observe,
                        assertion_check_side::client)
                  .side() == assertion_check_side::client);

// is_virtual/overrides_virtual are groundwork; the compiler passes false for
// both today whatever the enclosing function is.
static_assert(!make_info(evaluation_semantic::enforce,
                         assertion_check_side::definition)
                   .is_virtual());
static_assert(!make_info(evaluation_semantic::enforce,
                         assertion_check_side::definition)
                   .overrides_virtual());

// A default-constructed one is inert.
static_assert(assertion_static_info{}.side() ==
              assertion_check_side::not_applicable);
static_assert(!assertion_static_info{}.is_virtual());

// The three worked control objects model the concept; an unrelated type does not.
static_assert(assertion_control<default_control>);
static_assert(assertion_control<review>);
static_assert(assertion_control<mandatory>);
static_assert(!assertion_control<int>);

// default_control: no constify, non-optimizable, ignored only at 'ignore'.
static_assert(default_control::constify(make_info(
                  evaluation_semantic::enforce,
                  assertion_check_side::definition)) == false);
static_assert(default_control::is_ignored(make_info(
    evaluation_semantic::ignore, assertion_check_side::definition)));
static_assert(!default_control::is_ignored(make_info(
    evaluation_semantic::enforce, assertion_check_side::definition)));

// review: constified, always checked.
static_assert(review::constify(make_info(
                  evaluation_semantic::enforce,
                  assertion_check_side::definition)) == true);
static_assert(!review::is_ignored(make_info(
    evaluation_semantic::ignore, assertion_check_side::definition)));

// mandatory: guaranteed-enforced and optimizable.
static_assert(mandatory::constify(make_info(
                  evaluation_semantic::enforce,
                  assertion_check_side::definition)) == false);
static_assert(!mandatory::is_ignored(make_info(
    evaluation_semantic::ignore, assertion_check_side::definition)));
