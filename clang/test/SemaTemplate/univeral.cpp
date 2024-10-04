// RUN: %clang_cc1 -fsyntax-only -verify %s -std=c++2c

namespace universal {

constexpr int V = 0;
struct S;

template <typename T>
int TV;
template <auto T>
int VV;
template <universal template>
int UV;
template <typename T>
struct TS;
template <auto T>
struct VS;
template <universal template>
struct US;

template <typename T>
concept TC = true;
template <auto T>
concept VC = true;
template <universal template>
concept UC = true;

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
template <typename U>
inline constexpr bool is_typename_v<U> = true;
template <decltype(auto) U>
inline constexpr bool is_nttp_v<U> = true;
template <template<universal template> typename U>
constexpr bool is_template_v<U> = true;
template <template<universal template> auto U>
constexpr bool is_template_v<U> = true;
template <template<universal template> concept U>
constexpr bool is_template_v<U> = true;
template <template<universal template> typename U>
inline constexpr bool is_type_template_v<U> = true;
template <template<universal template> auto U>
inline constexpr bool is_var_template_v<U> = true;
template <template<universal template> concept U>
inline constexpr bool is_concept_v<U> = true;

static_assert(is_typename_v<int>);
static_assert(is_typename_v<S>);
static_assert(is_nttp_v<V>);
static_assert(is_nttp_v<(V)>);
static_assert(is_nttp_v<0>);

static_assert(!is_template_v<int>);
static_assert(!is_template_v<S>);
static_assert(!is_template_v<V>);
static_assert(!is_template_v<(V)>);
static_assert(!is_template_v<0>);


static_assert(is_template_v<TV>);
static_assert(is_template_v<VV>);
static_assert(is_template_v<UV>);
static_assert(is_template_v<TS>);
static_assert(is_template_v<VS>);
static_assert(is_template_v<US>);
static_assert(is_template_v<TC>);
static_assert(is_template_v<VC>);
static_assert(is_template_v<UC>);


static_assert(is_var_template_v<TV>);
static_assert(is_var_template_v<VV>);
static_assert(is_var_template_v<UV>);
static_assert(is_type_template_v<TS>);
static_assert(is_type_template_v<VS>);
static_assert(is_type_template_v<US>);
static_assert(is_concept_v<TC>);
static_assert(is_concept_v<VC>);
static_assert(is_concept_v<UC>);


static_assert(!is_var_template_v<int>);
static_assert(!is_var_template_v<S>);
static_assert(!is_var_template_v<V>);
static_assert(!is_var_template_v<(V)>);
static_assert(!is_var_template_v<0>);
static_assert(!is_var_template_v<TC>);
static_assert(!is_var_template_v<VC>);
static_assert(!is_var_template_v<UC>);
static_assert(!is_var_template_v<TS>);
static_assert(!is_var_template_v<VS>);
static_assert(!is_var_template_v<US>);

static_assert(!is_concept_v<int>);
static_assert(!is_concept_v<S>);
static_assert(!is_concept_v<V>);
static_assert(!is_concept_v<(V)>);
static_assert(!is_concept_v<0>);
static_assert(!is_concept_v<TV>);
static_assert(!is_concept_v<VV>);
static_assert(!is_concept_v<UV>);
static_assert(!is_concept_v<TS>);
static_assert(!is_concept_v<VS>);
static_assert(!is_concept_v<US>);

}

namespace _typename {

constexpr int V = 0;
struct S;

template <typename T>
int TV;
template <auto T>
int VV;
template <universal template>
int UV;
template <typename T>
struct TS;
template <auto T>
struct VS;
template <universal template>
struct US;

template <typename T>
concept TC = true;
template <auto T>
concept VC = true;
template <universal template>
concept UC = true;

template <universal template U>
inline constexpr bool is_template_v = false;
template <universal template U>
inline constexpr bool is_type_template_v = false;
template <universal template U>
inline constexpr bool is_var_template_v = false;
template <universal template U>
inline constexpr bool is_concept_v = false;
template <template<typename> typename U>
constexpr bool is_template_v<U> = true;
template <template<typename> auto U>
constexpr bool is_template_v<U> = true;
template <template<typename> concept U>
constexpr bool is_template_v<U> = true;
template <template<typename> typename U>
inline constexpr bool is_type_template_v<U> = true;
template <template<typename> auto U>
inline constexpr bool is_var_template_v<U> = true;
template <template<typename> concept U>
inline constexpr bool is_concept_v<U> = true;

static_assert(is_template_v<TV>);
static_assert(is_template_v<VV>); // expected-error {{static assertion failed due to requirement}}
static_assert(is_template_v<UV>); // expected-error {{static assertion failed due to requirement}}
static_assert(is_template_v<TS>);
static_assert(is_template_v<VS>); // expected-error {{static assertion failed due to requirement}}
static_assert(is_template_v<US>); // expected-error {{static assertion failed due to requirement}}
static_assert(is_template_v<TC>);
static_assert(is_template_v<VC>); // expected-error {{static assertion failed due to requirement}}
static_assert(is_template_v<UC>); // expected-error {{static assertion failed due to requirement}}


static_assert(is_var_template_v<TV>);
static_assert(is_var_template_v<VV>); // expected-error {{static assertion failed due to requirement}}
static_assert(is_var_template_v<UV>); // expected-error {{static assertion failed due to requirement}}
static_assert(is_type_template_v<TS>);
static_assert(is_type_template_v<VS>); // expected-error {{static assertion failed due to requirement}}
static_assert(is_type_template_v<US>); // expected-error {{static assertion failed due to requirement}}
static_assert(is_concept_v<TC>);
static_assert(is_concept_v<VC>); // expected-error {{static assertion failed due to requirement}}
static_assert(is_concept_v<UC>); // expected-error {{static assertion failed due to requirement}}
}
