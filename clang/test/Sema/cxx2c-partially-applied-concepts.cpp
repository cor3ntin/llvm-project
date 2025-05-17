// RUN:  %clang_cc1 -std=c++2b -verify %s

template <typename C, typename>
concept Concept = true; // expected-note 2{{template is declared here}}

template <template <typename> concept C>
struct S {
};

template <typename>
struct NotAConcept {};

S<concept Concept<int>> s;
S<concept Concept<int, int>> s; // expected-error {{too many template arguments for concept 'Concept'}}
S<concept Concept<>> s; // expected-error {{too few template arguments for concept 'Concept'}}



S<concept Concept<1>> s2;
S<concept NotAConcept<int>> s3; // expected-error {{name of template argument does not refer to concept, or concept template parameter}} \
                                // expected-error {{expected expression}}
namespace BasicOverloading {

template <template <typename...T> concept C>
void f();

template <bool b>
void f();

template <typename...>
concept C = true;

void test() {
    f<C<int>>();
    f<concept C<int>>();
}

}
