// RUN: %clang_cc1 -std=c++2c -verify %s

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

namespace DeferredDeduction {

int x; // #x
static constexpr int c = 1;
int arr[2];
void fn();
namespace N { int y; }
struct St { static int s; };
struct Agg { int m; int a[2]; };
Agg agg; // #agg

// An unparenthesized id-expression or class member access denotes the entity
// it names, so the type of the constant template parameter it is bound to is
// deduced there rather than here.
template <int &> struct Ref { static constexpr int k = 1; };
template <universal template U> struct Through : Ref<U> {};

static_assert(Through<x>::k == 1);
static_assert(Through<N::y>::k == 1);
static_assert(Through<St::s>::k == 1);
static_assert(Through<agg.m>::k == 1);
static_assert(Through<agg.a[1]>::k == 1);
static_assert(Through<arr[0]>::k == 1);
static_assert(Through<(x)>::k == 1);
static_assert(Through<*&x>::k == 1);

template <auto &> struct FnRef { static constexpr int k = 1; };
template <universal template U> struct ThroughFn : FnRef<U> {};
static_assert(ThroughFn<fn>::k == 1);

template <decltype(auto)> struct DeclTypeAuto {};
template <auto &V> struct AutoRef { using type = decltype(V); };
template <auto V> struct ByValue { static constexpr int k = V; };

template <template <universal template> class Z, universal template U>
using Alias = Z<U>; // #Alias

// Bound to a parameter of reference type, the entity is what binds.
static_assert(__is_same(Alias<AutoRef, x>::type, int &));
static_assert(__is_same(Alias<AutoRef, agg.m>::type, int &));

// Bound to 'decltype(auto)', the argument behaves as if it had been written
// there, so an entity that is not a constant expression is ill-formed, while a
// parenthesized one denotes a reference.
Alias<DeclTypeAuto, (x)> a1;
Alias<DeclTypeAuto, x> a2;
// expected-error@-1 {{non-type template argument is not a constant expression}}
//   expected-note@-2 {{read of non-const variable 'x' is not allowed in a constant expression}}
//   expected-note@-3 {{in instantiation of template type alias 'Alias' requested here}}
//   expected-note@#x {{declared here}}
Alias<DeclTypeAuto, agg.m> a3;
// expected-error@-1 {{non-type template argument is not a constant expression}}
//   expected-note@-2 {{read of non-constexpr variable 'agg' is not allowed in a constant expression}}
//   expected-note@-3 {{in instantiation of template type alias 'Alias' requested here}}
//   expected-note@#agg {{declared here}}

// Bound by value, a constant is required.
static_assert(Alias<ByValue, c>::k == 1);

// Any other expression is a value, whose type is deduced as if the parameter
// were declared 'decltype(auto)'; it must be a constant template argument.
template <universal template U> struct S {};
S<c + 1> v1;
S<&x> v2;
S<42> v3;
S<x + 1> v4;
// expected-error@-1 {{non-type template argument is not a constant expression}}
//   expected-note@-2 {{read of non-const variable 'x' is not allowed in a constant expression}}
//   expected-note@#x {{declared here}}
S<agg.m + 1> v5;
// expected-error@-1 {{non-type template argument is not a constant expression}}
//   expected-note@-2 {{read of non-constexpr variable 'agg' is not allowed in a constant expression}}
//   expected-note@#agg {{declared here}}

// Arguments naming different entities are different, and parentheses matter.
static_assert(!__is_same(S<x>, S<(x)>));
static_assert(!__is_same(S<x>, S<N::y>));
static_assert(!__is_same(S<agg.m>, S<agg.a[0]>));
static_assert(__is_same(S<x>, S<x>));
static_assert(__is_same(S<agg.m>, S<agg.m>));

// The entity survives deduction: it can be recovered from a specialization and
// bound to a parameter of reference type.
template <universal template U> struct W {};
template <typename T> struct Rebind;
template <universal template U> struct Rebind<W<U>> { using type = Ref<U>; };
static_assert(Rebind<W<x>>::type::k == 1);
static_assert(Rebind<W<agg.m>>::type::k == 1);

} // namespace DeferredDeduction
