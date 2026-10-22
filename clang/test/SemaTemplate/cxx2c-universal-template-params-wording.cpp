// RUN: %clang_cc1 -std=c++2c -verify %s

// Coverage of the wording of P2989R3 (universal template parameters), with one
// namespace per subclause.

template <universal template U> struct S {};
template <universal template... Us> struct P {};
template <typename T> struct Box {}; // #Box
template <int> struct IntV {}; // #IntV
template <long> struct LongV {};
template <int &> struct IntRef {}; // #IntRef
template <decltype(auto) V> struct DeclTypeOf { using type = decltype(V); };
template <template <typename> class> struct TT {};
template <typename T> constexpr int vt = 0;
template <typename T> concept Con = true;

int x; // #x
constexpr int c = 1;
struct Agg { int m; };
Agg agg;
constexpr Agg *pagg = &agg;
namespace N { int y; }

// The kind of a template argument, as seen through partial specializations.
template <universal template> constexpr int kind = 0;
template <typename T> constexpr int kind<T> = 1;
template <template <universal template...> typename TT> constexpr int kind<TT> = 2;
template <template <universal template...> auto VT> constexpr int kind<VT> = 3;
template <template <universal template...> concept CT> constexpr int kind<CT> = 4;
template <decltype(auto) V> constexpr int kind<V> = 5;

namespace temp_param {

// universal-template-parameter:
//   universal template ...opt identifieropt
template <universal template> struct A1 {};
template <universal template U> struct A2 {};
template <universal template...> struct A3 {};
template <universal template... Us> struct A4 {};
A1<int> a1;
A2<1> a2;
A3<> a3;
A4<int, 1, Box> a4;

// The grammar has no default argument for a universal template parameter.
template <universal template U = int> struct Def {};
// expected-error@-1 {{expected ',' or '>' in template-parameter-list}}

// The identifier of a universal-template-parameter is not looked up; it is a
// universal-argument-name in the scope of the template declaration.
template <universal template x> struct Shadow { using type = S<x>; };
static_assert(__is_same(Shadow<int>::type, S<int>));
static_assert(__is_same(Shadow<::x>::type, S<::x>));

// A universal template parameter can be declared in any template-head.
template <universal template U> void f();
template <universal template U> constexpr int v = 0;
template <universal template U> using Alias = S<U>;
struct M {
  template <universal template U> static constexpr int g() { return kind<U>; }
};
static_assert(M::g<int>() == 1 && M::g<1>() == 5);
auto l = []<universal template U>() { return S<U>{}; };
static_assert(__is_same(decltype(l.template operator()<Box>()), S<Box>));
template <template <universal template...> class> struct H {};
H<S> h;

} // namespace temp_param

namespace temp_names {

// A universal-argument-name is a template-argument, which can be followed by
// an ellipsis.
template <universal template U, universal template... Us> struct A {
  using type = P<U, Us...>;
};
static_assert(__is_same(A<int, 1, Box>::type, P<int, 1, Box>));

} // namespace temp_names

namespace temp_arg_general {

// If lookup finds an injected-class-name, it denotes the class template for
// a type template template parameter, and a type-name otherwise, including
// for a universal template parameter.
template <class T> struct A {
  S<A> s1;
  S<temp_arg_general::A> s2;
  TT<A> t;
};
static_assert(__is_same(decltype(A<int>::s1), S<A<int>>));
static_assert(__is_same(decltype(A<int>::s2), S<A>));
static_assert(__is_same(decltype(A<int>::t), TT<A>));

// If lookup finds a universal template parameter, the template-argument
// denotes that universal template parameter.
template <universal template U> struct B { static constexpr int k = kind<U>; };
static_assert(B<int>::k == 1 && B<Box>::k == 2 && B<vt>::k == 3 &&
              B<Con>::k == 4 && B<1>::k == 5);

// An ambiguity between a type-id and an expression is resolved to a type-id.
static_assert(kind<int()> == 1);
static_assert(!__is_same(S<int()>, S<0>));

} // namespace temp_arg_general

namespace temp_arg_type {

// A template-argument for a type template parameter shall be a type-id or a
// universal-argument-name.
template <class T> class X { }; // #X
template <universal template U>
void g() {
    X<U> x4; // OK
}
template void g<int>();

template <universal template U> struct ToType { using type = X<U>; }; // #ToType
static_assert(__is_same(ToType<int>::type, X<int>));
ToType<1>::type e1;
// expected-error@#ToType {{template argument for template type parameter must be a type}}
//   expected-note@-2 {{in instantiation of template class 'temp_arg_type::ToType<1>' requested here}}
//   expected-note@#X {{template parameter is declared here}}
ToType<Box>::type e2;
// expected-error@#ToType {{use of class template 'Box' requires template arguments}}
//   expected-note@-2 {{in instantiation of template class 'temp_arg_type::ToType<Box>' requested here}}
//   expected-note@#Box {{template is declared here}}

} // namespace temp_arg_type

namespace temp_arg_nontype {

// A universal-argument-name bound to a constant template parameter: the
// invented declaration 'T x = E;' uses the argument of the universal
// template parameter as E.
template <universal template U> struct ToInt { using type = IntV<U>; }; // #ToInt
template <universal template U> struct ToLong { using type = LongV<U>; };
template <universal template U> struct ToRef { using type = IntRef<U>; }; // #ToRef
static_assert(__is_same(ToInt<1>::type, IntV<1>));
static_assert(__is_same(ToInt<c>::type, IntV<1>));
static_assert(__is_same(ToLong<1>::type, LongV<1>));
static_assert(__is_same(ToRef<x>::type, IntRef<x>));
static_assert(__is_same(ToRef<(x)>::type, IntRef<x>));
static_assert(__is_same(ToRef<agg.m>::type, IntRef<agg.m>));

ToInt<x>::type e1;
// expected-error@-1 {{non-type template argument is not a constant expression}}
//   expected-note@-2 {{in instantiation of template class 'temp_arg_nontype::ToInt<x>' requested here}}
//   expected-note@-3 {{read of non-const variable 'x' is not allowed in a constant expression}}
//   expected-note@#x {{declared here}}
ToRef<c>::type e2;
// expected-error@-1 {{value of type 'const int' is not implicitly convertible to 'int &'}}
//   expected-note@-2 {{in instantiation of template class 'temp_arg_nontype::ToRef<c>' requested here}}
//   expected-note@#IntRef {{template parameter is declared here}}
ToInt<int>::type e3;
// expected-error@#ToInt {{template argument for non-type template parameter must be an expression}}
//   expected-note@-2 {{in instantiation of template class 'temp_arg_nontype::ToInt<int>' requested here}}
//   expected-note@#IntV {{template parameter is declared here}}

} // namespace temp_arg_nontype

namespace temp_arg_template {

// A template-argument for a template template parameter shall be the name of
// a template, or a universal-argument-name.
template <universal template W> struct ToTT { using type = TT<W>; }; // #ToTT
static_assert(__is_same(ToTT<Box>::type, TT<Box>));
ToTT<int>::type e1;
// expected-error@#ToTT {{template argument for template template parameter must be a class template or type alias template}}
//   expected-note@-2 {{in instantiation of template class 'temp_arg_template::ToTT<int>' requested here}}

// A universal template parameter declared in the template-head of P matches
// a template parameter of any kind; a universal template parameter pack
// matches zero or more template parameters of any kind.
template<class T> class A { /* ... */ }; // #A
template<class T, class U = T> class B { /* ... */ };
template<class ... Types> class C { /* ... */ };
template<auto n> class D { /* ... */ }; // #D
template<template<class> class P> class X { /* ... */ };
template<template<class ...> class Q> class Y { /* ... */ };
template<template<int> class R> class Z { /* ... */ };

template<template<universal template ...> class S> class U { /* ... */ };
template<template<class, universal template> class T> class V { /* ... */ }; // #V

X<A> xa;            // OK
X<B> xb;            // OK
X<C> xc;            // OK
Y<A> ya;            // OK
Y<B> yb;            // OK
Y<C> yc;            // OK
Z<D> zd;            // OK

U<A> ua;            // OK
U<D> ud;            // OK
V<B> vb;            // OK
V<D> vd;            // error: D does not match T
// expected-error@#V {{template argument for non-type template parameter must be an expression}}
//   expected-note@-2 {{template template argument has different template parameters than its corresponding template template parameter}}
//   expected-note@#V {{previous template template parameter is here}}
//   expected-note@#D {{template parameter is declared here}}

template <class, int, template <class> class, class...> struct Mixed {};
U<Mixed> umixed;
V<C> vc;

template <template <universal template, universal template> class> struct Two {}; // #Two
Two<B> t1;
Two<C> t2;
Two<A> t3;
// expected-error@-1 {{too many template arguments for class template 'A'}}
//   expected-note@-2 {{template template argument has different template parameters than its corresponding template template parameter}}
//   expected-note@#Two {{previous template template parameter is here}}
//   expected-note@#A {{template is declared here}}

} // namespace temp_arg_template

namespace temp_arg_universal {

// - A universal-argument-name: the argument is the universal template
//   parameter it names.
template <universal template U> constexpr int fwd = kind<U>;
static_assert(fwd<int> == 1 && fwd<Box> == 2 && fwd<vt> == 3 &&
              fwd<Con> == 4 && fwd<1> == 5);

// - A type-id is a type template argument.
static_assert(kind<int> == 1 && kind<int[2]> == 1 && kind<Box<int>> == 1 &&
              kind<decltype(x)> == 1);

// - A template other than a function template is a template template
//   argument; that includes template template parameters.
static_assert(kind<Box> == 2 && kind<vt> == 3 && kind<Con> == 4);
template <template <class> class TTP> constexpr int ttp = kind<TTP>;
static_assert(ttp<Box> == 2);

void ovl(int);
void ovl(long);
template <class T> void ftmpl(T);
template <universal template U> struct Q {}; // #Q
Q<ovl> e1;
// expected-error@-1 {{overload set 'ovl' cannot be the argument of a universal template parameter}}
//   expected-note@#Q {{template parameter is declared here}}
Q<ftmpl> e2;
// expected-error@-1 {{function template 'ftmpl' cannot be the argument of a universal template parameter}}
//   expected-note@#Q {{template parameter is declared here}}

// - An unparenthesized id-expression or class member access is a deferred
//   template argument; it is bound where the universal template parameter is
//   eventually used.
template <template <universal template> class Z, universal template U>
using Apply = Z<U>; // #Apply
static_assert(__is_same(Apply<DeclTypeOf, c>::type, int));
static_assert(__is_same(Apply<IntRef, x>, IntRef<x>));
static_assert(__is_same(Apply<IntRef, N::y>, IntRef<N::y>));
static_assert(__is_same(Apply<IntRef, agg.m>, IntRef<agg.m>));
static_assert(__is_same(Apply<IntRef, pagg->m>, IntRef<agg.m>));
auto &[z] = agg;
static_assert(__is_same(Apply<IntRef, z>, IntRef<agg.m>));
static_assert(!__is_same(S<z>, S<agg.m>));
Apply<DeclTypeOf, x> e3;
// expected-error@-1 {{non-type template argument is not a constant expression}}
//   expected-note@-2 {{in instantiation of template type alias 'Apply' requested here}}
//   expected-note@-3 {{read of non-const variable 'x' is not allowed in a constant expression}}
//   expected-note@#x {{declared here}}

// - Otherwise, a constant template argument, as if for a constant template
//   parameter declared 'decltype(auto)'.
static_assert(__is_same(Apply<DeclTypeOf, (x)>::type, int &));
static_assert(__is_same(Apply<DeclTypeOf, (c)>::type, const int &));
static_assert(__is_same(Apply<DeclTypeOf, 1L>::type, long));
static_assert(__is_same(Apply<DeclTypeOf, c + 1>::type, int));
static_assert(kind<(x)> == 5 && kind<&x> == 5 && kind<nullptr> == 5 &&
              kind<1.0> == 5 && kind<Agg{}> == 5);
Q<{1}> e4;
// expected-error@-1 {{cannot deduce 'decltype(auto)' from initializer list}}
Q<"abc"> e5;
// expected-error@-1 {{reference to string literal is not allowed in a template argument}}
Q<x + 1> e6;
// expected-error@-1 {{non-type template argument is not a constant expression}}
//   expected-note@-2 {{read of non-const variable 'x' is not allowed in a constant expression}}
//   expected-note@#x {{declared here}}

// When substituted, a universal-argument-name is interpreted as the argument
// it designates: a universal-argument-name, a type-id, a template name, the
// deferred id-expression, or a constant template parameter of the type of the
// value.
template <universal template U> struct Outer {
  template <universal template V> struct Inner { using type = P<U, V>; };
  using type = Inner<U>::type;
};
static_assert(__is_same(Outer<int>::Inner<x>::type, P<int, x>));
static_assert(__is_same(Outer<x>::type, P<x, x>));
static_assert(__is_same(Apply<Box, int>, Box<int>));
static_assert(__is_same(Apply<TT, Box>, TT<Box>));
static_assert(__is_same(Apply<S, x>, S<x>));
static_assert(__is_same(Apply<S, (x)>, S<(x)>));
static_assert(__is_same(Apply<S, 1L>, S<1L>));
template <universal template U> using Twice = Apply<DeclTypeOf, U>;
static_assert(__is_same(Twice<1L>::type, long));
static_assert(__is_same(Twice<(x)>::type, int &));

} // namespace temp_arg_universal

namespace temp_type {

// Deferred template arguments are the same if they are equivalent.
static_assert(__is_same(S<x>, S<x>));
static_assert(__is_same(S<x>, S<::x>));
static_assert(__is_same(S<N::y>, S<::N::y>));
static_assert(__is_same(S<agg.m>, S<agg.m>));
static_assert(!__is_same(S<x>, S<N::y>));
static_assert(!__is_same(S<agg.m>, S<pagg->m>));

// A deferred template argument is not the same as a constant, even one with
// the same value, and constants of different types are different.
static_assert(!__is_same(S<x>, S<(x)>));
static_assert(!__is_same(S<c>, S<1>));
static_assert(!__is_same(S<1>, S<1L>));
static_assert(!__is_same(S<1>, S<true>));

// Template arguments of different kinds are never the same.
static_assert(!__is_same(S<int>, S<1>));
static_assert(!__is_same(S<int>, S<Box>));
static_assert(!__is_same(S<Box>, S<Box<int>>));
static_assert(!__is_same(S<vt>, S<vt<int>>));
static_assert(!__is_same(S<Con>, S<Con<int>>));

// The name of a constant template parameter designates its value.
template <int N> using SN = S<N>;
static_assert(__is_same(SN<1>, S<1>));

// Universal template arguments are the same if they refer to the same
// template parameter, so these declare a single function template.
template <universal template U> constexpr int r1(S<U>);
template <universal template V> constexpr int r1(S<V>) { return 1; }
static_assert(r1(S<int>{}) == 1);
template <universal template... Us> constexpr int r2(P<Us...>);
template <universal template... Vs> constexpr int r2(P<Vs...>) { return 2; }
static_assert(r2(P<int, 1>{}) == 2);
template <universal template U, universal template V>
constexpr int r3(S<U>, S<V>) { return 3; }
template <universal template U, universal template V>
constexpr int r3(S<V>, S<U>) { return 4; }
static_assert(r3<int, long>(S<int>{}, S<long>{}) == 3);
static_assert(r3<int, long>(S<long>{}, S<int>{}) == 4);

template <universal template U> struct X {
  constexpr int f(S<U>);
  template <universal template V> constexpr int g(S<U>, S<V>);
};
template <universal template U> constexpr int X<U>::f(S<U>) { return 5; }
template <universal template U>
template <universal template V>
constexpr int X<U>::g(S<U>, S<V>) { return 6; }
static_assert(X<int>{}.f(S<int>{}) == 5);
static_assert(X<int>{}.g(S<int>{}, S<1>{}) == 6);

} // namespace temp_type

namespace temp_variadic {

// The elements of a universal template parameter pack designate the
// corresponding universal template arguments, of any kind.
template <class T, int N> struct Arr {}; // #Arr
template <universal template... Us> struct Expand { using type = Arr<Us...>; }; // #Expand
static_assert(__is_same(Expand<int, 3>::type, Arr<int, 3>));
static_assert(__is_same(Expand<int, c>::type, Arr<int, 1>));
Expand<3, int>::type e1;
// expected-error@#Expand {{template argument for template type parameter must be a type}}
//   expected-note@-2 {{in instantiation of template class 'temp_variadic::Expand<3, int>' requested here}}
//   expected-note@#Arr {{template parameter is declared here}}

template <universal template... Us> struct Forward { using type = P<Us...>; };
static_assert(__is_same(Forward<int, x, (x), 1, Box>::type,
                        P<int, x, (x), 1, Box>));
template <universal template... Us> struct Wrap { using type = P<S<Us>...>; };
static_assert(__is_same(Wrap<int, 1, Box>::type, P<S<int>, S<1>, S<Box>>));
template <universal template... Us> struct Mixed { using type = P<int, Us..., 2>; };
static_assert(__is_same(Mixed<x, Box>::type, P<int, x, Box, 2>));
template <universal template... Us> constexpr int count = sizeof...(Us);
static_assert(count<> == 0 && count<int, 1, Box, x> == 4);

template <universal template U> struct XP {
  template <universal template... Vs>
  constexpr int g(S<U>, S<Vs>...) { return sizeof...(Vs); }
};
static_assert(XP<int>{}.g(S<int>{}, S<1>{}, S<long>{}) == 2);

} // namespace temp_variadic

namespace temp_func_order {

// A unique universal template argument is synthesized for a universal
// template parameter; only a universal template parameter can be deduced
// from it.
template<universal template U> constexpr int f(S<U>) { return 1; }    // #1
template<class T> constexpr int f(S<T>) { return 2; }                 // #2
static_assert(f(S<int>()) == 2);                                      // calls #2
static_assert(f(S<0>()) == 1);                                        // calls #1

template <universal template U> constexpr int o(S<U>) { return 1; }
template <class T> constexpr int o(S<T>) { return 2; }
template <auto V> constexpr int o(S<V>) { return 3; }
template <template <class> class TT> constexpr int o(S<TT>) { return 4; }
static_assert(o(S<int>{}) == 2);
static_assert(o(S<0>{}) == 3);
static_assert(o(S<Box>{}) == 4);
static_assert(o(S<x>{}) == 1);
static_assert(o(S<c>{}) == 1);

// The same applies to partial specializations.
template <typename T> struct K { static constexpr int v = 0; };
template <universal template U> struct K<Box<S<U>>> { static constexpr int v = 1; };
template <class T> struct K<Box<S<T>>> { static constexpr int v = 2; };
static_assert(K<Box<S<int>>>::v == 2);
static_assert(K<Box<S<1>>>::v == 1);

} // namespace temp_func_order

namespace temp_concept {

// A concept shall not have a universal template parameter.
template <universal template U> concept C1 = true;
// expected-error@-1 {{a concept cannot have a universal template parameter}}
template <class T, universal template... Us> concept C2 = true;
// expected-error@-1 {{a concept cannot have a universal template parameter}}

// A concept template parameter can still have universal template parameters.
template <template <universal template...> concept CT> struct CC {};
CC<Con> cc;

} // namespace temp_concept

namespace temp_dep_temp {

// A deferred template argument is dependent if it is type-dependent or
// value-dependent...
struct HasValue { static constexpr int value = 6; };
template <class T> struct D1 { using type = S<T::value>; };
static_assert(__is_same(D1<HasValue>::type, S<HasValue::value>));
template <class T> struct D2 {
  static T m;
  using type = S<m>;
};
static_assert(__is_same(D2<int>::type, S<D2<int>::m>));

// ... or if it names a member of the current instantiation.
template <class T> struct D3 {
  static int m;
  using type = S<m>;
};
static_assert(__is_same(D3<int>::type, S<D3<int>::m>));
static_assert(!__is_same(D3<int>::type, D3<long>::type));

// A universal-argument-name is dependent, so its uses are only checked when
// it is substituted.
template <universal template U> struct D4 { using type = Box<U>; }; // #D4
template <universal template U> void d5() { IntV<U> i; (void)i; }
static_assert(__is_same(D4<int>::type, Box<int>));
D4<1>::type e1;
// expected-error@#D4 {{template argument for template type parameter must be a type}}
//   expected-note@-2 {{in instantiation of template class 'temp_dep_temp::D4<1>' requested here}}
//   expected-note@#Box {{template parameter is declared here}}

} // namespace temp_dep_temp

namespace temp_deduct_partial {

// For a pair of corresponding template parameters that are not used in the
// types used for partial ordering, a universal template parameter is not at
// least as specialized as a template parameter of another kind.
template <universal template U> constexpr int f() { return 1; }  // #1
template <class T> constexpr int f() { return 2; }               // #2
template <auto V> constexpr int f() { return 3; }                // #3
template <template <class> class TT> constexpr int f() { return 4; }
static_assert(f<int>() == 2);                                    // calls #2
static_assert(f<0>() == 3);                                      // calls #3
static_assert(f<Box>() == 4);
static_assert(f<x>() == 1);

template <universal template... Us> constexpr int g() { return 1; }
template <class T> constexpr int g() { return 2; }
static_assert(g<int>() == 2);
static_assert(g<int, int>() == 1);

// This also determines which template an explicit specialization or an
// address refers to.
template <> constexpr int f<long>() { return 5; }
static_assert(f<long>() == 5);
constexpr int (*pf)() = &f<char>;
static_assert(pf() == 2);

// Pairs that conflict leave the templates unordered.
template <class T, universal template U> constexpr int h() { return 1; } // #h1
template <universal template U, class T> constexpr int h() { return 2; } // #h2
int amb = h<int, int>();
// expected-error@-1 {{call to 'h' is ambiguous}}
//   expected-note@#h1 {{candidate function [with T = int, U = int]}}
//   expected-note@#h2 {{candidate function [with U = int, T = int]}}

// Template parameters used in the function parameter types are ordered by
// deduction only.
template <universal template U> constexpr int k(int) { return 1; }
template <class T> constexpr int k(T) { return 2; }
static_assert(k<int>(0) == 1);

} // namespace temp_deduct_partial

namespace temp_deduct_type {

// A universal template parameter is deduced to the corresponding template
// argument, whatever its kind.
template <template <universal template> class TT, universal template U>
constexpr int f(TT<U>) { return kind<U>; }
static_assert(f(S<int>{}) == 1 && f(S<Box>{}) == 2 && f(S<vt>{}) == 3 &&
              f(S<Con>{}) == 4 && f(S<1>{}) == 5);
template <typename T> struct Rebind;
template <universal template U> struct Rebind<S<U>> { using type = Box<S<U>>; };
static_assert(__is_same(Rebind<S<x>>::type, Box<S<x>>));
static_assert(__is_same(Rebind<S<(x)>>::type, Box<S<(x)>>));

// Arguments of different kinds deduced for the same parameter conflict.
template <universal template U> void g(S<U>, S<U>); // #g
void test_g() {
  g(S<int>{}, S<42>{});
  // expected-error@-1 {{no matching function for call to 'g'}}
  //   expected-note@#g {{candidate template ignored: deduced conflicting arguments for parameter 'U' ('int' vs. 42)}}
}

// A deferred template argument in which a subexpression references a
// template parameter is a non-deduced context.
struct Q { static int m; };
template <class T> constexpr int k1(T, S<T::m>) { return 1; }
static_assert(k1(Q{}, S<Q::m>{}) == 1);
template <class T> int k2(S<T::m>); // #k2
int e1 = k2(S<Q::m>{});
// expected-error@-1 {{no matching function for call to 'k2'}}
//   expected-note@#k2 {{candidate template ignored: couldn't infer template argument 'T'}}

// A constant template parameter deduced from a constant bound to a universal
// template parameter must have the type of that constant.
template <long L> int l1(S<L>); // #l1
template <auto V> constexpr int l2(S<V>) { return 1; }
static_assert(l2(S<42>{}) == 1);
int e2 = l1(S<42>{});
// expected-error@-1 {{no matching function for call to 'l1'}}
//   expected-note@#l1 {{candidate template ignored: substitution failure: deduced non-type template argument does not have the same type as the corresponding template parameter ('int' vs 'long')}}
int ok = l1(S<42L>{});

// When matching a partial specialization, a constant template parameter is
// deduced from a deferred template argument as if it were its argument.
template<universal template> struct K { static constexpr int v = 0; };
template<auto V> struct K<V> { static constexpr int v = 1; };
template<int& R> struct K<R> { static constexpr int v = 2; };
template<long L> void k(K<L>); // #k
constexpr int c2 = 24;
static_assert(K<c2>::v == 1);    // V is 24
static_assert(K<x>::v == 2);    // R refers to x
static_assert(K<42>::v == 1);
void test_k() {
  k(K<c2>());                    // error: deduction fails
  // expected-error@-1 {{no matching function for call to 'k'}}
  //   expected-note@#k {{candidate template ignored: substitution failure: deduced non-type template argument does not have the same type as the corresponding template parameter ('const int' vs 'long')}}
}

} // namespace temp_deduct_type
