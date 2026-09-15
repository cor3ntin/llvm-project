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

namespace Deduction {

// A universal template parameter can be deduced by a partial specialization,
// and matches an argument of any kind.
template <universal template A, universal template B>
constexpr bool same_v = false;
template <universal template A>
constexpr bool same_v<A, A> = true;

template <typename> struct Vec {};
template <typename> struct Lst {};
template <typename> concept C1 = true;
template <typename> concept C2 = true;

static_assert( same_v<int, int>);
static_assert(!same_v<int, char>);
static_assert( same_v<42, 42>);
static_assert(!same_v<42, 43>);
static_assert( same_v<Vec, Vec>);
static_assert(!same_v<Vec, Lst>);
static_assert( same_v<C1, C1>);
static_assert(!same_v<C1, C2>);

// Deducing the same parameter to arguments of different kinds is a mismatch.
static_assert(!same_v<int, 42>);
static_assert(!same_v<int, Vec>);
static_assert(!same_v<42, Vec>);

} // namespace Deduction

namespace SpecializationOf {

// The motivating example: one trait covering templates whose parameters are
// of any kind, including mixtures that 'template <typename...> class' cannot
// express.
template <typename T, template <universal template...> class Templ>
struct is_specialization_of { static constexpr bool value = false; };

template <template <universal template...> class Templ, universal template... Args>
struct is_specialization_of<Templ<Args...>, Templ> {
  static constexpr bool value = true;
};

template <typename...> struct Tuple {};
template <typename T, int N> struct Array {};
template <typename> struct Box {};

static_assert(is_specialization_of<Tuple<int, char>, Tuple>::value);
static_assert(is_specialization_of<Tuple<>, Tuple>::value);
static_assert(is_specialization_of<Array<int, 5>, Array>::value);
static_assert(is_specialization_of<Box<int>, Box>::value);

static_assert(!is_specialization_of<Tuple<int>, Array>::value);
static_assert(!is_specialization_of<Array<int, 5>, Tuple>::value);
static_assert(!is_specialization_of<Box<int>, Tuple>::value);
static_assert(!is_specialization_of<int, Tuple>::value);
static_assert(!is_specialization_of<int, Array>::value);

// A template template parameter whose parameters are universal accepts a
// template with parameters of any kind.
template <template <universal template...> class Templ>
struct Holder {};

Holder<Tuple> h1;
Holder<Array> h2;
Holder<Box> h3;

} // namespace SpecializationOf

namespace Packs {

// FIXME: A universal template parameter pack does not yet accept more than
// one argument.
template <universal template... Us>
struct Pack {};

Pack<int> one;

} // namespace Packs
