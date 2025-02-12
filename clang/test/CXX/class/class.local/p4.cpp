// RUN: %clang_cc1 -std=c++98 -fsyntax-only -verify %s 
// RUN: %clang_cc1 -std=c++11 -fsyntax-only -verify %s 
// RUN: %clang_cc1 -std=c++14 -fsyntax-only -verify %s 
// RUN: %clang_cc1 -std=c++17 -fsyntax-only -verify %s 
// RUN: %clang_cc1 -std=c++20 -fsyntax-only -verify %s 
// RUN: %clang_cc1 -std=c++23 -fsyntax-only -verify %s 

void f() {
  struct X {
    static int a; // expected-error {{static data member 'a' not allowed in local struct 'X'}}
    int b;

    static void f() { }
  };
}
