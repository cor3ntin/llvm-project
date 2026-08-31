// RUN: %clang_cc1 -std=c++23 -verify=cxx23 %s
// RUN: %clang_cc1 -std=c++2c -verify=cxx2c %s

// cxx2c-no-diagnostics

template <universal template U> // cxx23-error {{universal template parameters are a C++2c extension}}
struct S {};

template <universal template> // cxx23-error {{universal template parameters are a C++2c extension}}
void f();

template <universal template... Us> // cxx23-error {{universal template parameters are a C++2c extension}}
struct Pack {};
