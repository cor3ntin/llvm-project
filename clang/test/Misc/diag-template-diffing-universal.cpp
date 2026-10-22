// RUN: not %clang_cc1 -fsyntax-only -std=c++2c %s 2>&1 | FileCheck %s --check-prefix=CHECK-ELIDE-NOTREE
// RUN: not %clang_cc1 -fsyntax-only -std=c++2c -fno-elide-type %s 2>&1 | FileCheck %s --check-prefix=CHECK-NOELIDE-NOTREE
// RUN: not %clang_cc1 -fsyntax-only -std=c++2c -fdiagnostics-show-template-tree %s 2>&1 | FileCheck %s --check-prefix=CHECK-ELIDE-TREE

// The arguments of a universal template parameter can be of different kinds.

template <universal template U> struct S {};
template <universal template... Us> struct P {};
template <auto &> struct R {};
template <template <universal template> class Z, universal template U>
struct Foo {};
template <typename> struct Box {};
template <typename> struct Box2 {};
struct Lit { int v; };
int x;

Foo<R, x> f1;
Foo<R, (x)> f1;
// CHECK-ELIDE-NOTREE: error: redefinition of 'f1' with a different type: 'Foo<[...], (x)>' vs 'Foo<[...], x>'
// CHECK-NOELIDE-NOTREE: error: redefinition of 'f1' with a different type: 'Foo<template R, (x)>' vs 'Foo<template R, x>'
// CHECK-ELIDE-TREE: error: redefinition of 'f1' with a different type
// CHECK-ELIDE-TREE-NEXT: {{^}}  Foo<
// CHECK-ELIDE-TREE-NEXT: {{^}}    [...],
// CHECK-ELIDE-TREE-NEXT: {{^}}    [(x) != x]>

void take1(S<int>);
void call1() { take1(S<42>{}); }
// CHECK-ELIDE-NOTREE: no known conversion from 'S<42>' to 'S<int>' for 1st argument
// CHECK-NOELIDE-NOTREE: no known conversion from 'S<42>' to 'S<int>' for 1st argument
// CHECK-ELIDE-TREE: no known conversion from argument type to parameter type for 1st argument
// CHECK-ELIDE-TREE-NEXT: {{^}}  S<
// CHECK-ELIDE-TREE-NEXT: {{^}}    [42 != int]>

void take2(S<1>);
void call2() { take2(S<1L>{}); }
// CHECK-ELIDE-NOTREE: no known conversion from 'S<1L>' to 'S<1>' for 1st argument
// CHECK-ELIDE-TREE: no known conversion from argument type to parameter type for 1st argument
// CHECK-ELIDE-TREE-NEXT: {{^}}  S<
// CHECK-ELIDE-TREE-NEXT: {{^}}    [1L != 1]>

void take3(S<Box>);
void call3() { take3(S<Box2>{}); }
// CHECK-ELIDE-NOTREE: no known conversion from 'S<template Box2>' to 'S<template Box>' for 1st argument
// CHECK-ELIDE-TREE: no known conversion from argument type to parameter type for 1st argument
// CHECK-ELIDE-TREE-NEXT: {{^}}  S<
// CHECK-ELIDE-TREE-NEXT: {{^}}    [template Box2 != template Box]>

void take4(S<Lit{1}>);
void call4() { take4(S<Lit{2}>{}); }
// CHECK-ELIDE-NOTREE: no known conversion from 'S<Lit{2}>' to 'S<Lit{1}>' for 1st argument
// CHECK-ELIDE-TREE: no known conversion from argument type to parameter type for 1st argument
// CHECK-ELIDE-TREE-NEXT: {{^}}  S<
// CHECK-ELIDE-TREE-NEXT: {{^}}    [Lit{2} != Lit{1}]>

void take5(P<int, 1, x>);
void call5() { take5(P<int, 2>{}); }
// CHECK-ELIDE-NOTREE: no known conversion from 'P<[...], 2, (no argument)>' to 'P<[...], 1, x>' for 1st argument
// CHECK-ELIDE-TREE: no known conversion from argument type to parameter type for 1st argument
// CHECK-ELIDE-TREE-NEXT: {{^}}  P<
// CHECK-ELIDE-TREE-NEXT: {{^}}    [...],
// CHECK-ELIDE-TREE-NEXT: {{^}}    [2 != 1],
// CHECK-ELIDE-TREE-NEXT: {{^}}    [(no argument) != x]>

void take6(S<Box<int>>);
void call6() { take6(S<Box<long>>{}); }
// CHECK-ELIDE-NOTREE: no known conversion from 'S<Box<long>>' to 'S<Box<int>>' for 1st argument
// CHECK-ELIDE-TREE: no known conversion from argument type to parameter type for 1st argument
// CHECK-ELIDE-TREE-NEXT: {{^}}  S<
// CHECK-ELIDE-TREE-NEXT: {{^}}    Box<
// CHECK-ELIDE-TREE-NEXT: {{^}}      [long != int]>>
