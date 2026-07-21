#ifndef CONTRACTS_INPUTS_ASSERTION_CONTROL_H
#define CONTRACTS_INPUTS_ASSERTION_CONTROL_H

// Self-contained model of the D4324 assertion-control library surface for
// tests. It avoids the standard library (built on compiler builtins) so it
// works under the -nostdsysteminc cc1 invocations the Contracts suite uses.

namespace std {

// Minimal source_location. The member names/layout are fixed by the compiler.
class source_location {
  struct __impl {
    const char *_M_file_name;
    const char *_M_function_name;
    unsigned _M_line;
    unsigned _M_column;
  };
  const __impl *__ptr_ = nullptr;
  using __bsl_ty = decltype(__builtin_source_location());

public:
  static consteval source_location
  current(__bsl_ty __ptr = __builtin_source_location()) noexcept {
    source_location __sl;
    __sl.__ptr_ = static_cast<const __impl *>(__ptr);
    return __sl;
  }
  constexpr source_location() noexcept = default;
};

template <class T> inline constexpr bool is_empty_v = __is_empty(T);
template <class A, class B> concept same_as = __is_same(A, B);
template <class From, class To>
concept convertible_to = __is_convertible(From, To);

} // namespace std

namespace std::contracts {

enum class evaluation_config : unsigned {
  ignore = 0,
  observe = 1,
  enforce = 2,
  quick_enforce = 3,
  // [4 .. 0xFFFF] reserved to the standard; [0x1'0000 ..] to vendors and users
};

enum class violation_response { proceed, terminate };

template <class T>
concept assertion_control =
    std::is_empty_v<T> &&
    requires(T c, const char *comment, std::source_location loc,
             evaluation_config cfg) {
      { T::is_ignored(cfg) } -> std::same_as<bool>;
      { T::constify } -> std::convertible_to<bool>;
      { T::assumable } -> std::convertible_to<bool>;
      { c(comment, loc, cfg) } -> std::same_as<violation_response>;
    };

struct default_control {
  static constexpr bool is_ignored(evaluation_config cfg) {
    return cfg == evaluation_config::ignore;
  }
  static constexpr bool constify = false;
  static constexpr bool assumable = false;
  violation_response operator()(const char *, std::source_location,
                                evaluation_config) const {
    return violation_response::terminate;
  }
};
inline constexpr default_control default_v{};

// Log-and-continue at the library level, always checked, constified.
struct review {
  static constexpr bool is_ignored(evaluation_config) { return false; }
  static constexpr bool constify = true;
  static constexpr bool assumable = false;
  violation_response operator()(const char *, std::source_location,
                                evaluation_config) const {
    return violation_response::proceed;
  }
};

// Guaranteed-enforced and optimizable.
struct mandatory {
  static constexpr bool is_ignored(evaluation_config) { return false; }
  static constexpr bool constify = false;
  static constexpr bool assumable = true;
  violation_response operator()(const char *, std::source_location,
                                evaluation_config) const {
    return violation_response::terminate;
  }
};

} // namespace std::contracts

#endif // CONTRACTS_INPUTS_ASSERTION_CONTROL_H
