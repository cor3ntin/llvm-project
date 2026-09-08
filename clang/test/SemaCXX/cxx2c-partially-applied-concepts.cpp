// RUN: %clang_cc1 -std=c++2c -verify %s

template <typename C, typename>
concept Concept = true; // expected-note 2{{template is declared here}}

template <template <typename> concept C>
struct S {};

template <typename>
struct NotAConcept {};

// Binding all but one of the concept's parameters leaves a concept of arity
// one, which is what S expects.
S<concept Concept<int>> s1;

S<concept Concept<int, int>> s2; // expected-error {{too many template arguments for concept 'Concept'}}
S<concept Concept<>> s3; // expected-error {{too few template arguments for concept 'Concept'}}

// FIXME: The bound arguments are not yet checked against the parameters of the
// concept they are applied to, so this is wrongly accepted.
S<concept Concept<1>> s4;

S<concept NotAConcept<int>> s5; // expected-error {{name of template argument does not refer to concept, or concept template parameter}}

namespace BasicOverloading {

template <template <typename... T> concept C>
void f();

template <bool b>
void f();

template <typename...>
concept C = true;

void test() {
  // A concept-id and a partially applied concept both select the overload
  // taking a concept template parameter.
  f<C<int>>();
  f<concept C<int>>();
}

} // namespace BasicOverloading

namespace Qualified {

namespace N {
template <typename, typename>
concept Concept = true;
}

S<concept N::Concept<int>> s;

} // namespace Qualified

namespace PackExpansion {

// A partially applied concept may be a pack expansion, expanding the packs
// named by its bound arguments. Note this is a deliberate extension: P2841
// does not allow it.
template <typename, typename>
concept Pair = true;

template <template <typename> concept... Cs>
struct Pack {};

template <typename... Ts>
struct Expand {
  using type = Pack<concept Pair<Ts>...>;
  // An expansion may be mixed with ordinary arguments.
  using mixed = Pack<concept Pair<int>, concept Pair<Ts>...>;
};

using E = Expand<int, char>::type;
using M = Expand<int, char>::mixed;
using Empty = Expand<>::type;

// The expansion must name at least one unexpanded pack.
using Bad = Pack<concept Pair<int>...>; // expected-error {{pack expansion does not contain any unexpanded parameter packs}}

// Each element of the expansion binds its own element of the pack, so this
// checks Same<int, int> && Same<char, int>.
template <typename A, typename B>
concept Same = __is_same(A, B);

template <template <typename> concept... Cs>
struct AllSameAsInt {
  static constexpr bool value = (Cs<int> && ...);
};

template <typename... Ts>
struct Check {
  using type = AllSameAsInt<concept Same<Ts>...>;
};

static_assert(Check<int, int>::type::value);
static_assert(!Check<int, char>::type::value);
static_assert(!Check<char, char>::type::value);
static_assert(Check<>::type::value); // empty fold over '&&'

} // namespace PackExpansion

namespace Dependent {

template <template <typename> concept C, typename T>
struct Use {
  static constexpr bool value = C<T>;
};

template <typename, typename>
concept Pair = true;

static_assert(Use<concept Pair<int>, int>::value);

// The bound arguments precede those written where the concept is used, so
// these check Same<int, int> and Same<int, char>.
template <typename A, typename B>
concept Same = __is_same(A, B);

static_assert(Use<concept Same<int>, int>::value);
static_assert(!Use<concept Same<int>, char>::value);
static_assert(Use<concept Same<char>, char>::value);

} // namespace Dependent
