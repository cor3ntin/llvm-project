// RUN: %clang_cc1 -std=c++2c -verify %s

// The examples of P2989R3 "A Simple Approach to Universal Template
// Parameters", one per namespace, in the order of the paper.

namespace std {
using size_t = decltype(sizeof(0));
template <class T> T &&declval();
template <class T, class U> struct pair {};
template <bool B, class T, class F> struct conditional { using type = T; };
template <class T, class F> struct conditional<false, T, F> { using type = F; };
template <bool B, class T, class F>
using conditional_t = typename conditional<B, T, F>::type;
template <class T> concept regular = true;
template <class T, class U> constexpr bool is_same_v = __is_same(T, U);
template <class... Types> struct tuple {
  constexpr tuple(Types...) {}
};
template <class... Types> struct variant {};
template <class T, class... Types> T &&get(variant<Types...> &&);
template <class T> struct complex {
  constexpr complex(T re = T(), T im = T()) : re(re), im(im) {}
  T re, im;
};
} // namespace std

template <universal template...> struct list {};
template <universal template> constexpr int kind = 0;
template <typename T> constexpr int kind<T> = 1;
template <template <universal template...> class TT> constexpr int kind<TT> = 2;

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

namespace template_cpos_get {

using std::tuple;

template<std::size_t I, class... Types >
const Types...[I] & get( const tuple<Types...>& t ) noexcept;
template<class T, class... Types >
const T & get( const tuple<Types...>& t ) noexcept;

int main() {
    std::tuple t{1, 2.0};
    get<0>(t); // extract the 0th element
    get<double>(t); // extract the element that is a double
    static_assert(__is_same(decltype(get<0>(t)), const int &));
    static_assert(__is_same(decltype(get<double>(t)), const double &));
}

} // namespace template_cpos_get

namespace template_cpos_get_element {

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
static_assert(get_element<1>(t) == 2.0);
static_assert(get_element<1uz>(t) == 2.0);
constexpr int one = 1;
static_assert(get_element<one>(t) == 2.0);
static_assert(get_element<-1> == 0);

} // namespace template_cpos_get_element

namespace generic_rebind_allocator {

using std::size_t;

template <class T, size_t BlockSize = 42>
class BlockAllocator {
    public:
    using value_type = T;
    T* allocate(size_t);
    void deallocate(T*, size_t);
};

} // namespace generic_rebind_allocator

namespace generic_rebind {

using generic_rebind_allocator::BlockAllocator;

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

// Without universal template parameters, ranges::to cannot take
// llvm::SmallVector, which has a constant template parameter.
namespace ranges_to_cxx23 {

template <typename T> struct CalculateSmallVectorDefaultInlinedElements {
  static constexpr unsigned value = 4;
};
namespace llvm {
template <typename T, unsigned N = CalculateSmallVectorDefaultInlinedElements<T>::value>
class SmallVector;
} // namespace llvm

namespace ranges {
template <template <class...> class C, class... Args>
constexpr auto to(Args &&...args); // #to
} // namespace ranges

struct {} view;

auto v = view | ranges::to<llvm::SmallVector> ();
// expected-error@-1 {{no matching function for call to 'to'}}
//   expected-note@#to {{candidate template ignored: template argument for non-type template parameter must be an expression for template parameter 'C'}}

} // namespace ranges_to_cxx23

namespace ranges_to {

template <typename T> struct CalculateSmallVectorDefaultInlinedElements {
  static constexpr unsigned value = 4;
};
namespace llvm {
template <typename T, unsigned N = CalculateSmallVectorDefaultInlinedElements<T>::value>
class SmallVector {};
} // namespace llvm

namespace ranges {
template <template <universal template...> class C> struct to_adaptor {
  template <class R>
  friend constexpr C<typename R::value_type> operator|(R, to_adaptor) {
    return {};
  }
};

template<template<universal template...> class C, class... Args>
constexpr auto to(Args&&... args) { return to_adaptor<C>{}; }
} // namespace ranges

struct { using value_type = int; } view;

auto v = view | ranges::to<llvm::SmallVector> ();
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

namespace deduction_typenames {

template <universal template T>
struct S {};
template <universal template U, template <universal template> typename T>
void f(T<U> x);

using A = S<int>; // T deduces to int
void h1() {
  f(A{}); // calls f<int, S>
}

template <universal template U, template <universal template> typename T>
list<U, T> deduced(T<U> x);
static_assert(__is_same(decltype(deduced(A{})), list<int, S>));

} // namespace deduction_typenames

namespace deduction_template_names {

using deduction_typenames::S;
using deduction_typenames::f;
using deduction_typenames::deduced;

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

static_assert(__is_same(decltype(deduced(B{})), list<std::pair, S>));
static_assert(__is_same(decltype(deduced(C{})), list<std::conditional_t, S>));
static_assert(__is_same(decltype(deduced(D{})), list<std::regular, S>));
static_assert(__is_same(decltype(deduced(E{})), list<std::is_same_v, S>));

} // namespace deduction_template_names

// Well-formed: x is a deferred template argument.
namespace expressions_decltype_auto {

template <int&> struct A { };
template <universal template U> struct B : A<U> { };
int x;
B<x> b1;

} // namespace expressions_decltype_auto

namespace expressions_explicit_specialization {

static constexpr int a = 0;

template <universal template>
struct S { // #1
    static constexpr int value = 0;
};

template <>
struct S<a> { // #2
    static constexpr int value = 1;
};

void test() {
    static_assert(S<a>::value   == 1); // a is a deferred template argument
    static_assert(S<(a)>::value == 0); // (a) is a constant of type const int&
}

} // namespace expressions_explicit_specialization

namespace expressions_references_for_id_expressions {

template <auto&> struct A { };
template <auto> struct B { };
template <decltype(auto)> struct C { };

template <template<universal template> class Z, universal template U>
struct Foo : Z<U> { };

int x; // #x
Foo<A, x> b1; // Ok: instantiate A<x>
Foo<B, x> b2; // error: B<x> is ill-formed
// expected-error@-1 {{non-type template argument is not a constant expression}}
//   expected-note@-2 {{in instantiation of template class 'expressions_references_for_id_expressions::Foo<expressions_references_for_id_expressions::B, x>' requested here}}
//   expected-note@-3 {{read of non-const variable 'x' is not allowed in a constant expression}}
//   expected-note@#x {{declared here}}
Foo<C, x> b3; // error: C<x> is ill-formed
// expected-error@-1 {{non-type template argument is not a constant expression}}
//   expected-note@-2 {{in instantiation of template class 'expressions_references_for_id_expressions::Foo<expressions_references_for_id_expressions::C, x>' requested here}}
//   expected-note@-3 {{read of non-const variable 'x' is not allowed in a constant expression}}
//   expected-note@#x {{declared here}}

} // namespace expressions_references_for_id_expressions

namespace expressions_deferred {

template <int&> struct A { };
template <universal template U> struct B : A<U> { };
int x;
B<x> b1;

} // namespace expressions_deferred

namespace expressions_deferred_alias {

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

} // namespace expressions_deferred_alias

namespace injected_class_names {

template <universal template>
void f();

template <typename T>
struct S {
    void g() {
        f<S>(); // #1
        static_assert(kind<S> == 1);
    }
};
template struct S<int>;

} // namespace injected_class_names

namespace injected_class_names_qualified {

template <universal template>
void f();
template <typename T>
struct S {
    void g() {
        f<S>(); // S is S<T>
        f<injected_class_names_qualified::S>(); // S is S
        static_assert(kind<S> == 1);
        static_assert(kind<injected_class_names_qualified::S> == 2);
    }
};
template struct S<int>;

} // namespace injected_class_names_qualified

namespace injected_class_names_unnamed_class {

template <universal template>
void f();

struct {
    template <typename T>
    struct K {
         template <typename U>
         using K2 = K<U>;
         void g() {
           f<K2>(); // ok (K2 denotes a class template)
         }
         static_assert(kind<K> == 1);
         static_assert(kind<K2> == 2);
    };
} x;
template struct decltype(x)::K<int>;

} // namespace injected_class_names_unnamed_class

namespace concepts_variant_key {

template <universal template K,
          class... Types>
concept VariantKey = requires { std::get<K>(std::declval<std::variant<Types...>>()); };
// expected-error@-3 {{a concept cannot have a universal template parameter}}

} // namespace concepts_variant_key

namespace concepts_ifndr {

template <universal template> constexpr bool is_type_v = false;
template <typename T> constexpr bool is_type_v<T> = true;

template<class> concept T = true;
template<universal template U> concept C = is_type_v<U> && T<U>;
// expected-error@-1 {{a concept cannot have a universal template parameter}}
template<auto V> concept A = C<V>;   // IFNDR

} // namespace concepts_ifndr

namespace library_support_traits {

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

static_assert(is_typename_v<int> && is_typename_v<S>);
static_assert(!is_typename_v<0> && !is_typename_v<TS> && !is_typename_v<TC>);
static_assert(is_nttp_v<V> && is_nttp_v<(V)> && is_nttp_v<0>);
static_assert(!is_nttp_v<int> && !is_nttp_v<TS> && !is_nttp_v<TV>);
static_assert(!is_template_v<int> && !is_template_v<S> && !is_template_v<V> &&
              !is_template_v<(V)> && !is_template_v<0>);
static_assert(is_template_v<TV> && is_template_v<VV> && is_template_v<UV> &&
              is_template_v<TS> && is_template_v<VS> && is_template_v<US> &&
              is_template_v<TC> && is_template_v<VC>);
static_assert(is_type_template_v<TS> && is_type_template_v<VS> &&
              is_type_template_v<US>);
static_assert(!is_type_template_v<TV> && !is_type_template_v<TC>);
static_assert(is_var_template_v<TV> && is_var_template_v<VV> &&
              is_var_template_v<UV>);
static_assert(!is_var_template_v<TS> && !is_var_template_v<TC>);
static_assert(is_concept_v<TC> && is_concept_v<VC>);
static_assert(!is_concept_v<TS> && !is_concept_v<TV>);

} // namespace library_support_traits

namespace library_support_is_specialization_of {

template<template<universal template...> typename, typename>
inline constexpr bool is_specialization_of_v = false;

template<
template<universal template...> typename Primary,
universal template... Args
>
inline constexpr bool is_specialization_of_v<Primary, Primary<Args...>>    = true;

template <typename, typename = int> struct vector;
template <typename, int> struct array;
static_assert(is_specialization_of_v<array, array<int, 2>>);
static_assert(!is_specialization_of_v<array, int>);
static_assert(is_specialization_of_v<vector, vector<int>>);
static_assert(!is_specialization_of_v<array, vector<int>>);

} // namespace library_support_is_specialization_of

namespace reflection_as_type_as_value {

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

static_assert(__is_same(as_type_t<int>, int));
static_assert(as_value_v<42> == 42);

} // namespace reflection_as_type_as_value

namespace reflection_test {

using library_support_traits::is_nttp_v;
using library_support_traits::is_typename_v;
using reflection_as_type_as_value::as_type;
using reflection_as_type_as_value::as_value_v;

template <universal template U>
constexpr auto test() {
    if constexpr(is_typename_v<U>) {
        using a = as_type<U>::type; // ok
        return a{42};
    }
    else if constexpr(is_nttp_v<U>) {
        decltype(auto) v = as_value_v<U>;
        return v;
    }
}
static_assert(test<int>() == 42);
static_assert(test<24>() == 24);

} // namespace reflection_test

namespace syntax_universal_concept {

template <class> concept universal = true;

template <
        universal template, // template
        universal foo       // type constraint or NTTP
>
struct X {};
X<1, int> x;

} // namespace syntax_universal_concept

namespace syntax_universal_type {

struct universal {};

template <
        universal template, // template
        universal foo       // type constraint or NTTP
>
struct X {};
X<int, universal{}> x;

} // namespace syntax_universal_type

namespace syntax_template_auto {

template <
   auto, // variable
   template auto, // universal parameter
   // expected-error@-1 {{expected '<' after 'template'}}
   template <auto> auto, // variable template
   template <template auto> auto // variable template with a universal template parameter.
   // expected-error@-1 {{expected '<' after 'template'}}
>
// expected-error@-1 {{template template parameter must have its own template parameters}}
struct S;

} // namespace syntax_template_auto

namespace temp_param_packs {

template <class... Types>                       // Types is a template type parameter pack
class Tuple;                                    // but not a pack expansion

template <class T, int... Dims>                 // Dims is a constant template parameter pack
struct multi_array;                             // but not a pack expansion

template <class... T>
struct value_holder {
    template <T... Values> struct apply { };    // Values is a constant template parameter pack
};                                              // and a pack expansion

template <class... T, T... Values>              // error: Values expands template type parameter
struct static_array;                            // pack T within the same template parameter list
// expected-error@-2 {{template parameter pack must be the last template parameter}}

} // namespace temp_param_packs

namespace temp_param_names {

class T { public: T(int); };
int i;

template<class T, T i> void f(T t) {
    T t1 = i;         // template parameters T and i
    temp_param_names::T t2 = temp_param_names::i; // global namespace members T and i
}

} // namespace temp_param_names

namespace temp_arg_general_array {

template<class T> class Array {
    T* v;
    int sz;
    public:
    explicit Array(int);
    T& operator[](int);
    T& elem(int i) { return v[i]; }
};

Array<int> v1(20);
typedef std::complex<double> dcomplex;    // std::complex is a standard library template
Array<dcomplex> v2(30);
Array<dcomplex> v3(40);

void bar() {
    v1[3] = 7;
    v2[3] = v3.elem(4) = dcomplex(7,8);
}

} // namespace temp_arg_general_array

namespace temp_arg_general_type_id {

template<class T> void f();
template<int I> void f();

void g() {
    f<int()>();       // int() is a type-id: call the first f()
}

} // namespace temp_arg_general_type_id

namespace temp_arg_general_access {

template<class T> class X {
    static T t;
};

class Y {
    private:
    struct S { /*...*/ }; // #YS
    X<S> x;           // OK, S is accessible
    // X<Y::S> has a static member of type Y::S
    // OK, even though Y::S is private
};

X<Y::S> y;          // error: S not accessible
// expected-error@-1 {{'S' is a private member of 'temp_arg_general_access::Y'}}
//   expected-note@#YS {{declared private here}}

} // namespace temp_arg_general_access

namespace temp_arg_general_template_access {

template <template <class TT> class T> class A {
    typename T<int>::S s;
    // expected-error@-1 {{'S' is a private member of 'temp_arg_general_template_access::B<int>'}}
};

template <class U> class B {
    private:
    struct S { /*...*/ }; // #BS
};

A<B> b;             // error: A has no access to B::S
// expected-note@-1 {{in instantiation of template class 'temp_arg_general_template_access::A<temp_arg_general_template_access::B>' requested here}}
//   expected-note@#BS {{declared private here}}

} // namespace temp_arg_general_template_access

namespace temp_arg_general_empty_list {

template<class T = char> class String;
String<>* p;                    // OK, String<char>
String* q;                      // syntax error
// expected-error@-1 {{cannot form pointer to deduced class template specialization type}}
template<class ... Elements> class Tuple;
Tuple<>* t;                     // OK, Elements is empty
Tuple* u;                       // syntax error
// expected-error@-1 {{cannot form pointer to deduced class template specialization type}}

} // namespace temp_arg_general_empty_list

namespace temp_arg_general_destructor {

template<class T> struct A {
    ~A();
};
void f(A<int>* p, A<int>* q) {
    p->A<int>::~A();               // OK, destructor call
    q->A<int>::~A<int>();          // OK, destructor call
}

} // namespace temp_arg_general_destructor

namespace temp_arg_general_default_argument {

template<typename T, typename U = int> struct S { };
S<bool>* p;         // the type of p is S<bool, int>*
static_assert(__is_same(decltype(p), S<bool, int>*));

} // namespace temp_arg_general_default_argument

namespace temp_arg_type {

template <class T> class X { };
template <class T> void f(T t) { }
struct { } unnamed_obj;

void f() {
    struct A { };
    enum { e1 };
    typedef struct { } B;
    B b;
    X<A> x1;          // OK
    X<A*> x2;         // OK
    X<B> x3;          // OK
    f(e1);            // OK
    f(unnamed_obj);   // OK
    f(b);             // OK
}

template <universal template U>
void g() {
    X<U> x4; // OK
}
template void g<int>();

} // namespace temp_arg_type

namespace temp_arg_template {

template<class T> class A { /* ... */ };
template<class T, class U = T> class B { /* ... */ };
template<class ... Types> class C { /* ... */ };
template<auto n> class D { /* ... */ };
template<template<class> class P> class X { /* ... */ };
template<template<class ...> class Q> class Y { /* ... */ };
template<template<int> class R> class Z { /* ... */ };

template<template<universal template ...> class S> class U { /* ... */ };
template<template<class, universal template> class T> class V { /* ... */ };

X<A> xa;             // OK
X<B> xb;             // OK
X<C> xc;             // OK
Y<A> ya;             // OK
Y<B> yb;             // OK
Y<C> yc;             // OK
Z<D> zd;             // OK

U<A> ua;             // OK
U<D> ud;             // OK
V<B> vb;             // OK

} // namespace temp_arg_template

namespace temp_deduct_type {

template<universal template> struct K { static constexpr int v = 0; };
template<auto V> struct K<V> { static constexpr int v = 1; };
template<int& R> struct K<R> { static constexpr int v = 2; };
template<long L> void f(K<L>); // #f
constexpr int c = 24;
int x;
static_assert(K<c>::v == 1);    // V is 24
static_assert(K<x>::v == 2);    // R refers to x
static_assert(K<42>::v == 1);
void g() {
    f(K<c>());                    // error: deduction fails
    // expected-error@-1 {{no matching function for call to 'f'}}
    //   expected-note@#f {{candidate template ignored: substitution failure: deduced non-type template argument does not have the same type as the corresponding template parameter ('const int' vs 'long')}}
}

} // namespace temp_deduct_type
