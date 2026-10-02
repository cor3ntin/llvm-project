// RUN: %clang_cc1 -fsyntax-only -verify -std=c++2c %s

template <typename T>
struct Wrapper;

template <typename... T>
struct Eat {
    static_assert(__is_same(T...[0], Wrapper<Wrapper<int>>));
    static_assert(__is_same(T...[1], Wrapper<Wrapper<double>>));
};

template <typename... T>
struct Test {
    using ...a =  Wrapper<T>;
    Eat<Wrapper<a>...> t;
};

Test<int, double> t;

template <typename... T>
struct S {
    using ...types = T;
};
template <typename... Ts>
struct e1 {
   using a = S<Ts...>::...types; // expected-error {{declaration type contains unexpanded parameter pack 'types'}}
   using b = S<Ts...>::...types::a; // expected-error {{declaration type contains unexpanded parameter pack 'types'}}
   using ...c = S<Ts...>::types; // expected-error {{'S<Ts...>::types' does not contain an unexpanded pack}}
   using ...d = S<Ts...>::types::a; // expected-error {{'S<Ts...>::types::a' does not contain an unexpanded pack}}
   S<Ts...>::...types e; // expected-error {{data member type contains unexpanded parameter pack 'types'}}
   S<Ts...>::...types::a f; // expected-error {{data member type contains unexpanded parameter pack 'types'}}
};

template <typename... Ts>
struct ok {
    using ...a = S<Ts...>::...types;
    using ...b = S<Ts...>::...types::a;
};

namespace DependentNNS {

template <typename...Ts>
struct S {
    using ... A = Ts;
};

template <typename T>
struct Inner {
    using type = T;
};

template <typename...Ts>
struct AAAA {
};

template <typename... T>
void f() {
    AAAA<typename S<T...>::...A::type...> t;
}

void test() {
    f<Inner<int>, Inner<long>, Inner<bool>, Inner<float>>();
}

}

namespace Chained {

template <typename...> struct List {};
template <typename T> struct W {};

// An alias pack whose pattern refers to another alias pack.
template <typename... T> struct Chain {
    using ...a = W<T>;
    using ...b = W<a>;
    List<b...> x;
};
Chain<int, char> c;

template <typename... T> struct Count {
    using ...a = W<T>;
    static constexpr int n = sizeof...(a);
};
static_assert(Count<int, char, long>::n == 3);

template <typename... T> struct Index {
    using ...a = W<T>;
    using first = a...[0];
};
static_assert(__is_same(Index<int, char>::first, W<int>));

// Alias packs are also allowed at block scope.
template <typename... T> void fn() {
    using ...a = W<T>;
    List<a...> x;
    (void)x;
}
void g() { fn<int, char>(); }

}
