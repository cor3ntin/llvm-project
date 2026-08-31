// RUN: %clang_cc1 -std=c++2c -verify %s
// expected-no-diagnostics

// A universal template parameter accepts an argument of any kind.
template <universal template U>
struct S {};

template <universal template U>
void f();

struct X {};
constexpr int V = 42;
template <typename> struct TT {};
template <typename> concept C = true;

S<int> s_type;
S<X> s_class;
S<V> s_value;
S<TT> s_template;
S<C> s_concept;

// The parameter may be unnamed.
template <universal template>
struct Unnamed {};

Unnamed<int> u;

namespace UsedAsArgument {

// A universal template parameter can be forwarded as an argument; what it
// actually names is only determined once it has been substituted.
template <universal template U>
struct Fwd {
  using type = TT<U>;
};

using A = Fwd<int>::type;

} // namespace UsedAsArgument

namespace ContextualKeyword {

// 'universal' only introduces a template parameter when followed by
// 'template', so it remains usable as an ordinary identifier.
int universal = 3;
struct universal_t {
  int universal;
};
template <typename universal>
struct Shadow {};

} // namespace ContextualKeyword

namespace Packs {

// FIXME: A universal template parameter pack does not yet accept more than
// one argument.
template <universal template... Us>
struct Pack {};

Pack<int> one;

} // namespace Packs
