// RUN: %clang_cc1 -std=c++2c -fsyntax-only -verify %s

// The body of an expansion statement is a template region, so an alias pack
// can be declared there even outside of any template. A pack to expand over
// is available without template parameters because the pack-producing builtins
// produce one from concrete types.

#define TEMPLATE_REGION template for (constexpr auto _ : {0})

template <typename...> struct TypeList;
template <typename T> struct W {};

// Outside a template region there is no pack to be had in the first place.
void outside_region() {
  using ...bad = __builtin_dedup_pack<int, double, int>; // expected-error {{'__builtin_dedup_pack' cannot be used outside of template}}
}

void dedup() {
  TEMPLATE_REGION {
    using ...a = __builtin_dedup_pack<int, double, int, float, double>;
    static_assert(sizeof...(a) == 3);
    static_assert(__is_same(TypeList<a...>, TypeList<int, double, float>));

    // The expansions are plain aliases, so they can be indexed.
    static_assert(__is_same(a...[0], int));
    static_assert(__is_same(a...[2], float));
  }
}

void sort() {
  TEMPLATE_REGION {
    using ...s = __builtin_sort_pack<double, int, char>;
    static_assert(sizeof...(s) == 3);

    // The order depends on the ABI, but sorting is idempotent.
    using ...t = __builtin_sort_pack<s...>;
    static_assert(__is_same(TypeList<s...>, TypeList<t...>));
  }
}

void chained() {
  TEMPLATE_REGION {
    using ...a = __builtin_dedup_pack<int, char, int>;
    using ...w = W<a>;
    static_assert(__is_same(TypeList<w...>, TypeList<W<int>, W<char>>));

    using ...uniq = __builtin_dedup_pack<W<int>, W<int>, W<char>>;
    static_assert(__is_same(TypeList<uniq...>, TypeList<W<int>, W<char>>));
  }
}

void used_in_a_declaration() {
  TEMPLATE_REGION {
    using ...a = __builtin_dedup_pack<int, int, char>;
    TypeList<a...> *p = nullptr;
    (void)p;
  }
}

// Each expansion statement is its own template region, and the alias pack is
// reinstantiated in each of them.
void nested() {
  TEMPLATE_REGION {
    TEMPLATE_REGION {
      using ...a = __builtin_dedup_pack<int, int, char>;
      static_assert(sizeof...(a) == 2);
      static_assert(__is_same(TypeList<a...>, TypeList<int, char>));
      TEMPLATE_REGION {
        using ...b = __builtin_dedup_pack<a..., double>;
        static_assert(sizeof...(b) == 3);
      }
    }
  }
}

// The pattern still needs to name a pack.
void no_pack() {
  TEMPLATE_REGION {
    using ...bad = int; // expected-error {{'int' does not contain an unexpanded pack}}
  }
}

// An expansion statement inside a template works the same way, with the
// template parameter packs also available.
template <typename... Ts>
void in_a_function_template() {
  TEMPLATE_REGION {
    using ...a = __builtin_dedup_pack<Ts...>;
    static_assert(sizeof...(a) == 2);
    static_assert(__is_same(TypeList<a...>, TypeList<int, char>));

    using ...w = W<Ts>;
    static_assert(sizeof...(w) == 3);
  }
}
void call_it() { in_a_function_template<int, char, int>(); }

template <typename... Ts>
struct InAMemberFunction {
  void m() {
    TEMPLATE_REGION {
      using ...a = __builtin_dedup_pack<Ts...>;
      static_assert(sizeof...(a) == 1);
    }
  }
};
void call_member() { InAMemberFunction<int, int>().m(); }
