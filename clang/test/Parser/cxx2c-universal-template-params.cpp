// RUN: %clang_cc1 -std=c++23 -verify=cxx23 %s
// RUN: %clang_cc1 -std=c++2c -verify=cxx2c %s

// cxx2c-no-diagnostics

template <universal template U> // cxx23-error {{universal template parameters are a C++2c extension}}
struct S {};

template <universal template> // cxx23-error {{universal template parameters are a C++2c extension}}
void f();

template <universal template... Us> // cxx23-error {{universal template parameters are a C++2c extension}}
struct Pack {};

// 'universal template' introduces a universal template parameter even when
// 'universal' names a concept or a type.
namespace concept_name {
template <class> concept universal = true;
template <universal template U, universal T> // cxx23-error {{universal template parameters are a C++2c extension}}
struct X {};
}

namespace type_name {
struct universal {};
template <universal template, universal V> // cxx23-error {{universal template parameters are a C++2c extension}}
struct X {};
}
