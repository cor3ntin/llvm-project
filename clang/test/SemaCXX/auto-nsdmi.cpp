// RUN: %clang_cc1 -std=c++23 -fsyntax-only -verify -fexperimental-auto-nsdmi %s

// A non-static data member may be declared with a placeholder type, deduced
// from its default member initializer. Unlike an ordinary default member
// initializer, this one is parsed at the closing brace of its own class,
// because the class cannot be laid out until the member's type is known.

// expected-no-diagnostics

namespace Basic {
struct S {
  auto i = 42;
  auto d = 3.5;
  auto c = 'x';
};
static_assert(__is_same(decltype(S::i), int));
static_assert(__is_same(decltype(S::d), double));
static_assert(__is_same(decltype(S::c), char));
} // namespace Basic

namespace PointerToSelf {
// Deducing a pointer to the class being defined is fine; only its own layout
// would be needed.
struct S {
  auto p = (S *)nullptr;
};
static_assert(__is_same(decltype(S::p), S *));
} // namespace PointerToSelf

namespace Qualified {
struct S {
  static int g;
  auto &r = g;
  const auto k = 7;
  int n = 1;
  decltype(auto) d = n;
};
int S::g;
static_assert(__is_same(decltype(S::r), int &));
static_assert(__is_same(decltype(S::k), const int));
static_assert(__is_same(decltype(S::d), int));
} // namespace Qualified

namespace EarlierMember {
// The initializer can name a member declared before it.
struct S {
  int a = 1;
  auto b = a;
};
static_assert(__is_same(decltype(S::b), int));
} // namespace EarlierMember

namespace Nested {
// A nested class is laid out at its own closing brace, so its deduced members
// are available to the enclosing class.
struct O {
  struct I {
    auto a = 1;
  };
  I i;
  auto size = sizeof(I);
};
static_assert(__is_same(decltype(O::I::a), int));
static_assert(__is_same(decltype(O::size), decltype(sizeof(0))));

struct A {
  struct B {
    struct C {
      auto c = 1;
    };
    C cc;
    auto b = 2.0;
  };
  B bb;
  auto a = 'z';
};
static_assert(__is_same(decltype(A::a), char));
static_assert(__is_same(decltype(A::B::b), double));
static_assert(__is_same(decltype(A::B::C::c), int));
} // namespace Nested

namespace Lambdas {
struct S {
  auto f = [](int n) { return n + 1; };
  int a = 2;
  auto g = [this] { return a; };
};
static_assert(sizeof(S{}.f(1)) == sizeof(int));
} // namespace Lambdas

namespace Templates {
template <typename T>
struct S {
  auto x = T();
};
static_assert(__is_same(decltype(S<int>::x), int));
static_assert(__is_same(decltype(S<double>::x), double));
static_assert(sizeof(S<double>) == sizeof(double));

// The initializer may depend on an earlier member of the same instantiation.
template <typename T>
struct Chained {
  T t = T();
  auto u = t;
};
static_assert(__is_same(decltype(Chained<char>::u), char));

template <typename T>
struct NestedTemplate {
  struct I {
    auto a = T();
  };
  I i;
};
static_assert(__is_same(decltype(NestedTemplate<long>::I::a), long));
} // namespace Templates
