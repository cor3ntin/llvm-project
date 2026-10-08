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

template <universal template... Us>
struct Pack {
  static constexpr unsigned size = sizeof...(Us);
};

Pack<> none;
Pack<int> one;
Pack<int, 42, TT, C> many;

static_assert(Pack<>::size == 0);
static_assert(Pack<int, 42, TT, C>::size == 4);

template <universal template... Us>
constexpr unsigned count() { return sizeof...(Us); }
static_assert(count<int, 42, TT>() == 3);

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

namespace Redeclarations {

// A universal template parameter is only equivalent to another universal
// template parameter, so these are distinct function templates.
template <universal template U> constexpr int f() { return 0; }
template <typename T> constexpr int f() { return 1; }
template <auto V> constexpr int f() { return 2; }
template <template <typename> class TT> constexpr int f() { return 3; }

int x;
static_assert(f<x>() == 0);

template <template <universal template> class> constexpr int g() { return 0; }
template <template <typename> class> constexpr int g() { return 1; }

template <universal template U> void h();
template <universal template V> void h() {}

template <universal template> struct S1; // expected-note {{previous template declaration is here}}
template <typename T> struct S1 {};
// expected-error@-1 {{template parameter has a different kind in template redeclaration}}

template <typename T> struct S2; // expected-note {{previous template declaration is here}}
template <universal template U> struct S2 {};
// expected-error@-1 {{template parameter has a different kind in template redeclaration}}

template <universal template... Us> struct S3; // expected-note {{previous universal template parameter pack declared here}}
template <universal template U> struct S3 {};
// expected-error@-1 {{universal template parameter conflicts with previous universal template parameter pack}}

} // namespace Redeclarations

namespace KindDispatch {

template <typename> struct Box {};
template <typename> constexpr int vt = 0;
template <typename> concept Con = true;

template <universal template> constexpr int kind = 0;
template <auto V> constexpr int kind<V> = 1;
template <typename T> constexpr int kind<T> = 2;
template <template <universal template...> typename TT>
constexpr int kind<TT> = 3;
template <template <universal template...> auto VT>
constexpr int kind<VT> = 4;
template <template <universal template...> concept CT>
constexpr int kind<CT> = 5;

static_assert(kind<42> == 1);
static_assert(kind<int> == 2);
static_assert(kind<Box> == 3);
static_assert(kind<vt> == 4);
static_assert(kind<Con> == 5);

template <universal template U> constexpr int kind<U> = 6;
// expected-error@-1 {{variable template partial specialization does not specialize any template argument}}

} // namespace KindDispatch

namespace TemplateTemplateArguments {

template <typename T, typename A = int> struct WithDefault {};
template <typename...> struct Tuple {};
template <typename T, int N> struct Array {};
template <typename> struct Box {};

// The template parameters of the argument that correspond to a universal
// template parameter can be of any kind; the remaining ones need defaults.
template <template <universal template> class> struct One {};
One<Box> o1;
One<WithDefault> o2;
One<Tuple> o3;

template <template <universal template, universal template> class> struct Two {};
Two<Array> t1;
Two<WithDefault> t2;
Two<Tuple> t3;

template <universal template> struct UBox {};
template <template <typename> class> struct TypeOnly {};
TypeOnly<UBox> t4;

template <template <universal template> class Z, universal template U>
using Apply = Z<U>;
static_assert(__is_same(Apply<WithDefault, int>, WithDefault<int, int>));

// A template-head mixing universal and other template parameters.
template <class T, unsigned long BlockSize = 42> class BlockAllocator {};

template <template <class, universal template...> class> struct Mixed {};
Mixed<BlockAllocator> m1;

template <typename, typename> struct rebind_alloc_t;
template <template <class, universal template...> typename Alloc, class T,
          universal template... Args, class U>
struct rebind_alloc_t<Alloc<T, Args...>, U> {
  using type = Alloc<U, Args...>;
};
static_assert(__is_same(rebind_alloc_t<BlockAllocator<int>, long>::type,
                        BlockAllocator<long>));
static_assert(__is_same(rebind_alloc_t<BlockAllocator<int, 128>, long>::type,
                        BlockAllocator<long, 128>));

} // namespace TemplateTemplateArguments

namespace Constraints {

template <typename T> concept IsInt = __is_same(T, int); // #IsInt

template <universal template U> constexpr int f() { return 0; }
template <universal template U> requires IsInt<U> constexpr int f() {
  return 1;
}

// The satisfaction of a constraint depends on the argument of the universal
// template parameter, whichever order the specializations are checked in.
static_assert(f<int>() == 1);
static_assert(f<long>() == 0);
static_assert(f<42>() == 0);
static_assert(f<int>() == 1);

template <universal template U> requires IsInt<U> void g(); // #ConstrainedG
void test() {
  g<long>();
  // expected-error@-1 {{no matching function for call to 'g'}}
  //   expected-note@#ConstrainedG {{candidate template ignored: constraints not satisfied [with U = long]}}
  //   expected-note@#ConstrainedG {{because 'long' does not satisfy 'IsInt'}}
  //   expected-note@#IsInt {{because '__is_same(long, int)' evaluated to false}}
}

// A concept cannot have a universal template parameter.
template <universal template U> concept Any = true;
// expected-error@-1 {{a concept cannot have a universal template parameter}}

} // namespace Constraints

namespace OverloadSets {

void ovl(int);
void ovl(double);
template <typename T> void tmpl(T);
void single();

template <universal template U> struct S {}; // #OverloadSetsS

S<ovl> s1;
// expected-error@-1 {{overload set 'ovl' cannot be the argument of a universal template parameter}}
//   expected-note@#OverloadSetsS {{template parameter is declared here}}
S<tmpl> s2;
// expected-error@-1 {{function template 'tmpl' cannot be the argument of a universal template parameter}}
//   expected-note@#OverloadSetsS {{template parameter is declared here}}
S<single> s3;

} // namespace OverloadSets

namespace ConstantTypes {

// The type of a value bound to a universal template parameter is part of
// it, so a partial specialization for a constant of another type does not
// match.
template <universal template> struct K { static constexpr int v = 0; };
template <long L> struct K<L> { static constexpr int v = 1; };

static_assert(K<42>::v == 0);
static_assert(K<42L>::v == 1);

template <universal template...> struct Ks { static constexpr int v = 0; };
template <long... Ls> struct Ks<Ls...> { static constexpr int v = 1; };

static_assert(Ks<42L, 43L>::v == 1);
static_assert(Ks<42L, 43>::v == 0);

} // namespace ConstantTypes

namespace DeferredMatching {

// A constant template parameter of a partial specialization deduced from an
// id-expression is deduced as if the id-expression were its template
// argument.
int x, y;
constexpr int c = 24;
enum E { e1 = 7 };
struct Agg { int m; };
Agg agg;

template <universal template> struct K { static constexpr int v = 0; };
template <auto V> struct K<V> { static constexpr int v = 1; };
template <int &R> struct K<R> { static constexpr int v = 2; };

static_assert(K<42>::v == 1);
static_assert(K<c>::v == 1);
static_assert(K<e1>::v == 1);
static_assert(K<x>::v == 2);
static_assert(K<(x)>::v == 2);
static_assert(K<agg.m>::v == 2);
static_assert(K<int>::v == 0);

template <universal template> struct is_constant {
  static constexpr bool value = false;
};
template <decltype(auto) V> struct is_constant<V> {
  static constexpr bool value = true;
};

static_assert(is_constant<24>::value);
static_assert(is_constant<c>::value);
static_assert(is_constant<e1>::value);
static_assert(is_constant<(x)>::value);
static_assert(!is_constant<x>::value);
static_assert(!is_constant<int>::value);

template <universal template> constexpr bool is_constant_v = false;
template <decltype(auto) V> constexpr bool is_constant_v<V> = true;
static_assert(is_constant_v<c>);
static_assert(!is_constant_v<x>);

template <universal template> struct as_value;
template <decltype(auto) V> struct as_value<V> {
  static constexpr decltype(auto) value = V;
};
static_assert(as_value<c>::value == 24);
static_assert(as_value<e1>::value == e1);

template <universal template...> struct Refs {
  static constexpr bool value = false;
};
template <int &...Rs> struct Refs<Rs...> {
  static constexpr bool value = true;
};
static_assert(Refs<x, y>::value);
static_assert(Refs<x, (y)>::value);
static_assert(!Refs<x, c>::value);

// A value written in a partial specialization is not deduced, so it does not
// match an id-expression.
template <universal template, universal template> struct Pair {
  static constexpr int v = 0;
};
template <typename T> struct Pair<T, 24> { static constexpr int v = 1; };
static_assert(Pair<int, 24>::v == 1);
static_assert(Pair<int, c>::v == 0);

} // namespace DeferredMatching

namespace IdExpressions {

// Every id-expression is deferred: it matches a partial specialization for a
// constant of another type, as if it had been written as its argument, while
// a constant must already have that type.
template <universal template> struct D { static constexpr int v = 0; };
template <long L> struct D<L> { static constexpr int v = 1; };

constexpr int c = 1;
enum E { e1 = 7 };
template <class T> constexpr int vt = 3;
template <class T> concept IsInt = __is_same(T, int);
struct St { static constexpr int sc = 4; };
struct HasValue { static constexpr int value = 6; };
template <class T> struct Dep { static constexpr int v = D<T::value>::v; };

static_assert(D<c>::v == 1);
static_assert(D<e1>::v == 1);
static_assert(D<E::e1>::v == 1);
static_assert(D<vt<int>>::v == 1);
static_assert(D<St::sc>::v == 1);
static_assert(D<IsInt<int>>::v == 1);
static_assert(Dep<HasValue>::v == 1);
void f() {
  template for (constexpr int i : {1})
    static_assert(D<i>::v == 1);
}

// Anything else is a constant.
static_assert(D<(c)>::v == 0);
static_assert(D<1>::v == 0);
static_assert(D<true>::v == 0);

template <universal template U> struct S {};
static_assert(!__is_same(S<IsInt<int>>, S<true>));
static_assert(__is_same(S<IsInt<int>>, S<IsInt<int>>));

// Once substituted, the name of a constant template parameter is the constant
// it denotes, so that a specialization formed inside a template is the one
// formed outside of it.
template <int V> struct Fwd {
  using type = S<V>;
  static constexpr int v = D<V>::v;
};
static_assert(__is_same(Fwd<1>::type, S<1>));
static_assert(Fwd<1>::v == 0);

template <int N> constexpr int g(S<N>) { return N; }
static_assert(g(S<2>{}) == 2);
static_assert(g(Fwd<3>::type{}) == 3);

// That constant binds like the name of the template parameter would.
template <template <universal template> class Z, universal template U>
using Apply = Z<U>;
template <long L> struct Long { static constexpr long v = L; };
template <decltype(auto) X> struct DeclTypeOf { using type = decltype(X); };
struct Lit { int v; };
template <const Lit &R> struct LitRef { static constexpr int v = R.v; };

template <int V> struct ByValue {
  static_assert(Apply<Long, V>::v == V);
  static_assert(__is_same(typename Apply<DeclTypeOf, V>::type, int));
};
template struct ByValue<5>;

template <Lit L> struct ByClass {
  static_assert(Apply<LitRef, L>::v == L.v);
  static_assert(__is_same(typename Apply<DeclTypeOf, L>::type, Lit));
};
template struct ByClass<Lit{6}>;

} // namespace IdExpressions

namespace InconsistentDeduction {

template <universal template U> struct S {};
template <universal template U> void f(S<U>, S<U>); // #InconsistentF
void test() {
  f(S<int>{}, S<42>{});
  // expected-error@-1 {{no matching function for call to 'f'}}
  //   expected-note@#InconsistentF {{candidate template ignored: deduced conflicting arguments for parameter 'U' ('int' vs. 42)}}
}

} // namespace InconsistentDeduction
