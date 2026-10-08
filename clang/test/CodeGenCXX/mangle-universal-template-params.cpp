// RUN: %clang_cc1 -std=c++2c -triple x86_64-linux-gnu -emit-llvm -o - %s | FileCheck %s

// FIXME: The mangling of universal template parameters and arguments is not
// yet specified by the Itanium C++ ABI.

template <universal template U> struct S {};
template <universal template... Us> struct P {};
template <typename T> void use(T) {}

int x;
struct Agg { int m; } agg;
template <const int &> struct ConstRef {};

template <typename T> struct Extract;
template <template <universal template> class TT, universal template U>
struct Extract<TT<U>> { using type = S<U>; };

template <universal template U> int f() { return 0; }
template <typename T> int f() { return 1; }

template <typename T> concept IsInt = __is_same(T, int);

void test() {
  // CHECK-DAG: define {{.*}} @_Z3useI1SIiEEvT_(
  use(S<int>{});

  // The type of a value is part of the argument.
  // CHECK-DAG: define {{.*}} @_Z3useI1SILi42EEEvT_(
  use(S<42>{});
  // CHECK-DAG: define {{.*}} @_Z3useI1SILl42EEEvT_(
  use(S<42L>{});

  // An id-expression names an entity, and is distinct from a reference to it.
  // CHECK-DAG: define {{.*}} @_Z3useI1SIXL_Z1xEEEEvT_(
  use(S<x>{});
  // CHECK-DAG: define {{.*}} @_Z3useI1SIL_Z1xEEEvT_(
  use(S<(x)>{});
  // CHECK-DAG: define {{.*}} @_Z3useI1SIXdtL_Z3aggE1mEEEvT_(
  use(S<agg.m>{});

  // References of different types to the same object are distinct.
  // CHECK-DAG: define {{.*}} @_Z3useI1SIXcvRKiL_Z1xEEEEvT_(
  use(Extract<ConstRef<x>>::type{});

  // CHECK-DAG: define {{.*}} @_Z3useI1PIJiXL_Z1xEEL_Z1xELi42EEEEvT_(
  use(P<int, x, (x), 42>{});

  // A concept-id is an id-expression, distinct from its value.
  // CHECK-DAG: define {{.*}} @_Z3useI1SILb1EEEvT_(
  use(S<true>{});
  // CHECK-DAG: define {{.*}} @_Z3useI1SIX5IsIntIiEEEEvT_(
  use(S<IsInt<int>>{});

  // A function template overloaded on the kind of its template parameter.
  // CHECK-DAG: define {{.*}} @_Z1fITuXL_Z1xEEEiv(
  f<x>();
  // CHECK-DAG: define {{.*}} @_Z1fITuLi42EEiv(
  f<42>();
}
