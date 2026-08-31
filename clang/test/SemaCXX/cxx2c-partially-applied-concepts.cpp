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
