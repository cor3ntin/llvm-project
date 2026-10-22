// RUN: %clang_cc1 -std=c++2c -verify %s

// The examples of P2989R3 "A Simple Approach to Universal Template
// Parameters", in the order of the paper.

namespace std {
using size_t = decltype(sizeof(0));
template <class T, class U> struct pair {};
template <bool B, class T, class F> struct conditional { using type = T; };
template <class T, class F> struct conditional<false, T, F> { using type = F; };
template <bool B, class T, class F>
using conditional_t = typename conditional<B, T, F>::type;
template <class T> concept regular = true;
template <class T, class U> constexpr bool is_same_v = __is_same(T, U);
} // namespace std

namespace difference_with_p1985 {

template <universal template> void SomeOtherTemplate();

template <universal template U> // #U
void f() {
  SomeOtherTemplate<U>(); // ok.
  U foo; // error: U is not used as a template argument.
  // expected-error@-1 {{'U' does not refer to a value}}
  //   expected-note@#U {{declared here}}
  U = 0; // error: U is not used as a template argument.
  // expected-error@-1 {{'U' does not refer to a value}}
  //   expected-note@#U {{declared here}}
}

} // namespace difference_with_p1985

namespace template_cpos {

template <class... Ts> struct tuple;
template <> struct tuple<> {};
template <class T, class... Ts> struct tuple<T, Ts...> {
  T first;
  tuple<Ts...> rest;
};
template <std::size_t I, class... Ts>
constexpr auto get(const tuple<Ts...> &t) {
  if constexpr (I == 0)
    return t.first;
  else
    return get<I - 1>(t.rest);
}
template <class T, class U, class... Ts>
constexpr auto get(const tuple<U, Ts...> &t) {
  if constexpr (__is_same(T, U))
    return t.first;
  else
    return get<T>(t.rest);
}

namespace __get_element_t {
template <typename T> struct __cpo {
  template <class Tuple> constexpr auto operator()(const Tuple &t) const {
    return get<T>(t);
  }
};
} // namespace __get_element_t

namespace __get_element_v {
template <std::size_t I> struct __cpo {
  template <class Tuple> constexpr auto operator()(const Tuple &t) const {
    return get<I>(t);
  }
};
} // namespace __get_element_v

template <universal template>
inline constexpr int get_element = /*delete*/0;

template <typename T>
inline constexpr __get_element_t::__cpo<T> get_element<T>;

template <std::size_t I>
inline constexpr __get_element_v::__cpo<I> get_element<I>;

constexpr tuple<int, double> t{1, {2.0, {}}};
static_assert(get_element<double>(t) == 2.0);
static_assert(get_element<1uz>(t) == 2.0);
constexpr int one = 1;
static_assert(get_element<one>(t) == 2.0);

// The literal 1 is a constant of type int, which does not match the partial
// specialization for std::size_t.
static_assert(get_element<1> == 0);

template <std::size_t I> struct cpo {};
template <universal template> inline constexpr int get_element2 = 0;
template <typename T> inline constexpr cpo<0> get_element2<T>;
template <auto I> inline constexpr cpo<I> get_element2<I>;
static_assert(__is_same(decltype(get_element2<1>), const cpo<1>));

} // namespace template_cpos

namespace generic_rebind {

using std::size_t;

template <class T, size_t BlockSize = 42>
class BlockAllocator {
    public:
    using value_type = T;
    T* allocate(size_t);
    void deallocate(T*, size_t);
};

template <typename, typename>
struct rebind_alloc_t;

template <template <class, universal template...> typename Alloc, class T, universal template... Args, class U>
struct rebind_alloc_t<Alloc<T, Args...>, U> {
    using type = Alloc<U, Args...>;
};
template <class Alloc>
struct allocator_traits {
    template<class T>
    using rebind_alloc = rebind_alloc_t<Alloc, T>::type;
};
using rebound = allocator_traits<BlockAllocator<int>>::rebind_alloc<long>; // now ok
using rebound2 = allocator_traits<BlockAllocator<int, 128>>::rebind_alloc<long>; // ok too

static_assert(__is_same(rebound, BlockAllocator<long, 42>));
static_assert(__is_same(rebound2, BlockAllocator<long, 128>));

} // namespace generic_rebind

namespace ranges_to {

template <typename T> struct CalculateSmallVectorDefaultInlinedElements {
  static constexpr unsigned value = 4;
};
namespace llvm {
template <typename T, unsigned N = CalculateSmallVectorDefaultInlinedElements<T>::value>
class SmallVector {};
} // namespace llvm

template <class T> struct view { using value_type = T; };

template <template <universal template...> class C> struct to_adaptor {
  template <class R>
  friend constexpr C<typename R::value_type> operator|(R, to_adaptor) {
    return {};
  }
};

template<template<universal template...> class C, class... Args>
constexpr auto to(Args&&... args) { return to_adaptor<C>{}; }

auto v = view<int>{} | to<llvm::SmallVector> ();
static_assert(__is_same(decltype(v), llvm::SmallVector<int, 4>));

} // namespace ranges_to

namespace partial_ordering {

template <universal template>
constexpr bool is_variable = false; // #1
template <auto a>
constexpr bool is_variable<a> = true; // #2

static_assert(is_variable<42>);
static_assert(!is_variable<int>);

} // namespace partial_ordering

namespace deduction {

template <universal template T>
struct S {};
template <universal template U, template <universal template> typename T>
constexpr int f(T<U> x) { return 0; }

using A = S<int>; // T deduces to int
void h1() {
  f(A{}); // calls f<int, S>
}

using B = S<std::pair>; // T is compared against template <typename T, typename U> struct pair;
using C = S<std::conditional_t>; // T is a type-alias std::conditional_t
using D = S<std::regular>; // T is a concept-name std::regular
using E = S<std::is_same_v>; // T is a variable-template std::is_same_v

void h2() {
  f(B{}); // calls f<std::pair, S>
  f(C{}); // calls f<std::conditional_t, S>
  f(D{}); // calls f<std::regular, S>
  f(E{}); // calls f<std::is_same_v, S>
}

template <universal template U, template <universal template> typename T>
constexpr bool deduced(T<U>) { return __is_same(T<U>, S<U>); }
static_assert(deduced(A{}) && deduced(B{}) && deduced(C{}) && deduced(D{}) &&
              deduced(E{}));

} // namespace deduction

namespace expressions {

// Deduction of the type of an id-expression is deferred.
namespace deferred {
template <int&> struct A { };
template <universal template U> struct B : A<U> { };
int x;
B<x> b1;
} // namespace deferred

namespace alias {
template <decltype(auto)> struct A { };
template <auto&> struct B { };
template <template <universal template> class Z, universal template U>
using Alias = Z<U>;

int i; // #i

Alias<A, i> a1; // ill-formed (non-constant expression used as template argument)
// expected-error@-1 {{non-type template argument is not a constant expression}}
//   expected-note@-2 {{in instantiation of template type alias 'Alias' requested here}}
//   expected-note@-3 {{read of non-const variable 'i' is not allowed in a constant expression}}
//   expected-note@#i {{declared here}}
Alias<A, (i)> a2; // ok A<int&>
Alias<B, i> a3; // ok B<int&>
static_assert(__is_same(decltype(a2), A<(i)>));
static_assert(__is_same(decltype(a3), B<i>));
} // namespace alias

// The rejected decltype(auto) model, with an explicit specialization: S<a>
// and S<(a)> are different specializations under the proposed model too.
namespace explicit_specialization {
static constexpr int a = 0;

template <universal template>
struct S { // #1
    static constexpr int value = 0;
};

template <>
struct S<a> { // #2
    static constexpr int value = 1;
};

static_assert(S<a>::value == 1);
static_assert(S<(a)>::value == 0);
} // namespace explicit_specialization

// The rejected model deducing references for id-expressions.
namespace forwarding {
template <auto&> struct A { };
template <auto> struct B { };
template <decltype(auto)> struct C { };

template <template<universal template> class Z, universal template U>
struct Foo : Z<U> { };

int x; // #fx
constexpr int cx = 0;
Foo<A, x> b1;
Foo<B, cx> b2;
Foo<C, cx> b3;
Foo<B, x> b4;
// expected-error@-1 {{non-type template argument is not a constant expression}}
//   expected-note@-2 {{in instantiation of template class 'expressions::forwarding::Foo<expressions::forwarding::B, x>' requested here}}
//   expected-note@-3 {{read of non-const variable 'x' is not allowed in a constant expression}}
//   expected-note@#fx {{declared here}}
} // namespace forwarding

} // namespace expressions

namespace injected_class_names {

template <universal template> constexpr int kind = 0;
template <typename T> constexpr int kind<T> = 1;
template <template <universal template...> class TT> constexpr int kind<TT> = 2;

template <universal template U>
constexpr int f() { return kind<U>; }

template <typename T>
struct S {
    static_assert(f<S>() == 1); // S is S<T>
    static_assert(f<injected_class_names::S>() == 2); // S is S
};
template struct S<int>;

template <universal template U> struct W {};
template <typename T>
struct S2 {
  using t1 = W<S2>;
  using t2 = W<injected_class_names::S2>;
};
static_assert(__is_same(S2<int>::t1, W<S2<int>>));
static_assert(__is_same(S2<int>::t2, W<S2>));

// A class template nested in an unnamed class cannot be named with a
// qualified name, but an alias template can stand in for it.
struct {
    template <typename T>
    struct K {
         template <typename U>
         using K2 = K<U>;
         static_assert(f<K>() == 1);
         static_assert(f<K2>() == 2);
    };
} x;
template struct decltype(x)::K<int>;

} // namespace injected_class_names

namespace concepts {

// Universal template parameters are not proposed in concept definitions.
template <universal template K, class... Types>
concept VariantKey = true;
// expected-error@-2 {{a concept cannot have a universal template parameter}}

} // namespace concepts

namespace library_support {

template <universal template T>
inline constexpr bool is_typename_v = false;
template <universal template U>
inline constexpr bool is_nttp_v = false;
template <universal template U>
inline constexpr bool is_template_v = false;
template <universal template U>
inline constexpr bool is_type_template_v = false;
template <universal template U>
inline constexpr bool is_var_template_v = false;
template <universal template U>
inline constexpr bool is_concept_v = false;

template <typename T>
inline constexpr bool is_typename_v<T> = true;

template <decltype(auto) V>
inline constexpr bool is_nttp_v<V> = true;

template <template<universal template...> typename U>
constexpr bool is_template_v<U> = true;

template <template<universal template...> auto U>
constexpr bool is_template_v<U> = true;

template <template<universal template...> concept U>
constexpr bool is_template_v<U> = true;

template <template<universal template...> typename U>
inline constexpr bool is_type_template_v<U> = true;

template <template<universal template...> auto U>
inline constexpr bool is_var_template_v<U> = true;

template <template<universal template...> concept U>
inline constexpr bool is_concept_v<U> = true;

constexpr int V = 0;
struct S;
template <typename T> int TV;
template <auto T> int VV;
template <universal template> int UV;
template <typename T> struct TS;
template <auto T> struct VS;
template <universal template> struct US;
template <typename T> concept TC = true;
template <auto T> concept VC = true;

static_assert(is_typename_v<int>);
static_assert(is_typename_v<S>);
static_assert(is_nttp_v<V>);
static_assert(is_nttp_v<(V)>);
static_assert(is_nttp_v<0>);

static_assert(!is_template_v<int>);
static_assert(!is_template_v<S>);
static_assert(!is_template_v<V>);
static_assert(!is_template_v<(V)>);
static_assert(!is_template_v<0>);

static_assert(is_template_v<TV>);
static_assert(is_template_v<VV>);
static_assert(is_template_v<UV>);
static_assert(is_template_v<TS>);
static_assert(is_template_v<VS>);
static_assert(is_template_v<US>);
static_assert(is_template_v<TC>);
static_assert(is_template_v<VC>);

static_assert(is_var_template_v<TV>);
static_assert(is_var_template_v<VV>);
static_assert(is_var_template_v<UV>);
static_assert(is_type_template_v<TS>);
static_assert(is_type_template_v<VS>);
static_assert(is_type_template_v<US>);
static_assert(is_concept_v<TC>);
static_assert(is_concept_v<VC>);

static_assert(!is_typename_v<0> && !is_typename_v<TS> && !is_typename_v<TC>);
static_assert(!is_nttp_v<int> && !is_nttp_v<TS> && !is_nttp_v<TV>);
static_assert(!is_type_template_v<TV> && !is_type_template_v<TC>);
static_assert(!is_var_template_v<TS> && !is_var_template_v<TC>);
static_assert(!is_concept_v<TS> && !is_concept_v<TV>);

template<template<universal template...> typename, typename>
inline constexpr bool is_specialization_of_v = false;

template<
template<universal template...> typename Primary,
universal template... Args
>
inline constexpr bool is_specialization_of_v<Primary, Primary<Args...>>  = true;

template <typename, typename = int>
struct vector;
template <typename, int>
struct array;
static_assert(is_specialization_of_v<array, array<int, 2>>);
static_assert(!is_specialization_of_v<array, int>);
static_assert(is_specialization_of_v<vector, vector<int>>);
static_assert(!is_specialization_of_v<array, vector<int>>);

} // namespace library_support

namespace reflection {

template <universal template T>
inline constexpr bool is_typename_v = false;
template <universal template U>
inline constexpr bool is_nttp_v = false;
template <typename T>
inline constexpr bool is_typename_v<T> = true;
template <decltype(auto) V>
inline constexpr bool is_nttp_v<V> = true;

template<universal template>
struct as_type;
template<typename T>
struct as_type<T> { using type = T; };
template <universal template T>
using as_type_t = as_type<T>::type;

template<universal template U>
constexpr auto as_value_v = /*delete*/ 0;
template<decltype(auto) V>
constexpr decltype(auto) as_value_v<V> = V;

template <universal template U>
constexpr auto test() {
    if constexpr(is_typename_v<U>) {
        using a = as_type_t<U>;
        return a{42};
    }
    else if constexpr(is_nttp_v<U>) {
        decltype(auto) v = as_value_v<U>;
        return v;
    }
}
static_assert(test<int>() == 42);
static_assert(test<24>() == 24);
constexpr long c = 7;
static_assert(test<c>() == 7L);

} // namespace reflection

namespace syntax {

namespace concept_name {
template <class> concept universal = true;
template <
        universal template, // template
        universal foo       // type constraint or NTTP
>
struct X {};
X<1, int> x;
} // namespace concept_name

namespace type_name {
struct universal {};
template <
        universal template, // template
        universal foo       // type constraint or NTTP
>
struct X {};
X<int, universal{}> x;
} // namespace type_name

} // namespace syntax
