// RUN: %clang_cc1 -triple %itanium_abi_triple -std=c++2c -fsyntax-only -verify -DITANIUM %s
// RUN: %clang_cc1 -triple %ms_abi_triple -std=c++2c -fsyntax-only -verify -DMICROSOFT %s

// Alias packs whose pattern is a pack produced by a builtin. Such a pack only
// learns its size once the pattern has been substituted, so the alias pack is
// expanded from the substituted pattern.

// expected-no-diagnostics

template <typename...> struct TypeList;
template <typename T> struct W;

namespace Dedup {

template <typename... Ts>
struct S {
  using ...dedup = __builtin_dedup_pack<Ts...>;
  using list = TypeList<dedup...>;
  static constexpr int count = sizeof...(dedup);
};

static_assert(__is_same(S<int, double, int, float, double>::list,
                        TypeList<int, double, float>));
static_assert(S<int, double, int, float, double>::count == 3);

static_assert(__is_same(S<>::list, TypeList<>));
static_assert(S<>::count == 0);

static_assert(__is_same(S<int, int, int>::list, TypeList<int>));
static_assert(S<int, int, int>::count == 1);

// The elements are plain aliases, so they can be indexed.
template <typename... Ts>
struct Indexed {
  using ...dedup = __builtin_dedup_pack<Ts...>;
  using first = dedup...[0];
  using second = dedup...[1];
};
static_assert(__is_same(Indexed<int, int, double>::first, int));
static_assert(__is_same(Indexed<int, int, double>::second, double));

// The builtin can also consume an alias pack.
template <typename... Ts>
struct Wrapped {
  using ...w = W<Ts>;
  using uniq = TypeList<__builtin_dedup_pack<w...>...>;
};
static_assert(__is_same(Wrapped<int, int, double>::uniq,
                        TypeList<W<int>, W<double>>));

// An alias pack whose pattern dedups another alias pack.
template <typename... Ts>
struct Chained {
  using ...w = W<Ts>;
  using ...uniq = __builtin_dedup_pack<w...>;
  using list = TypeList<uniq...>;
};
static_assert(__is_same(Chained<int, double, int>::list,
                        TypeList<W<int>, W<double>>));

} // namespace Dedup

namespace Sort {

template <typename... Ts>
struct S {
  using ...sorted = __builtin_sort_pack<Ts...>;
  using list = TypeList<sorted...>;
  static constexpr int count = sizeof...(sorted);
};

// The order __builtin_sort_pack produces depends on the mangling, and so on
// the ABI.
#ifdef ITANIUM
static_assert(__is_same(S<double, int, char>::list, TypeList<char, double, int>));
#else
static_assert(__is_same(S<double, int, char>::list, TypeList<char, int, double>));
#endif
static_assert(S<double, int, char>::count == 3);

// Sorting is idempotent, whatever the order is.
template <typename... Ts>
struct Idempotent {
  using ...once = __builtin_sort_pack<Ts...>;
  using ...twice = __builtin_sort_pack<once...>;
  static_assert(__is_same(TypeList<once...>, TypeList<twice...>));
};
template struct Idempotent<double, int, char, int>;

// Sorting the result of deduplicating.
template <typename... Ts>
struct UniqueSorted {
  using ...uniq = __builtin_sort_pack<__builtin_dedup_pack<Ts...>...>;
  using list = TypeList<uniq...>;
  static constexpr int count = sizeof...(uniq);
};
#ifdef ITANIUM
static_assert(__is_same(UniqueSorted<double, int, char, int, double>::list,
                        TypeList<char, double, int>));
#else
static_assert(__is_same(UniqueSorted<double, int, char, int, double>::list,
                        TypeList<char, int, double>));
#endif
static_assert(UniqueSorted<double, int, char, int, double>::count == 3);

} // namespace Sort

namespace StillDependent {

// The pack size is not known while the enclosing template is still dependent,
// so the alias pack stays a pack until the outer template is instantiated.
template <typename... Outer>
struct Outer_ {
  template <typename... Inner>
  struct Inner_ {
    using ...dedup = __builtin_dedup_pack<Outer..., Inner...>;
    using list = TypeList<dedup...>;
  };
};

static_assert(__is_same(Outer_<int, double>::Inner_<double, char>::list,
                        TypeList<int, double, char>));
static_assert(__is_same(Outer_<>::Inner_<int, int>::list, TypeList<int>));

} // namespace StillDependent
