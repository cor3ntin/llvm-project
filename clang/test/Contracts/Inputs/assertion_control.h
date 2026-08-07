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

// Mirrors <contracts>. The values match clang's ContractEvaluationSemantic.
enum class evaluation_semantic : unsigned char {
  __unknown = 0,
  ignore = 0,
  enforce = 1,
  observe = 2,
  quick_enforce = 3,
};

enum class assertion_check_side : unsigned char {
  not_applicable = 0,
  definition = 1,
  client = 2,
};

class assertion_static_info;

} // namespace std::contracts

namespace std {

consteval contracts::assertion_static_info
__create_assertion_static_info(contracts::evaluation_semantic __semantic,
                               contracts::assertion_check_side __side,
                               bool __is_virtual,
                               bool __overrides_virtual) noexcept;

} // namespace std

namespace std::contracts {

class assertion_static_info {
  evaluation_semantic __semantic_ = evaluation_semantic::__unknown;
  assertion_check_side __side_ = assertion_check_side::not_applicable;
  bool __is_virtual_ = false;
  bool __overrides_virtual_ = false;

  friend consteval assertion_static_info std::__create_assertion_static_info(
      evaluation_semantic, assertion_check_side, bool, bool) noexcept;

public:
  constexpr assertion_static_info() noexcept = default;

  constexpr evaluation_semantic semantic() const noexcept { return __semantic_; }
  constexpr assertion_check_side side() const noexcept { return __side_; }
  constexpr bool is_virtual() const noexcept { return __is_virtual_; }
  constexpr bool overrides_virtual() const noexcept { return __overrides_virtual_; }
};

} // namespace std::contracts

namespace std {

consteval contracts::assertion_static_info
__create_assertion_static_info(contracts::evaluation_semantic __semantic,
                               contracts::assertion_check_side __side,
                               bool __is_virtual,
                               bool __overrides_virtual) noexcept {
  contracts::assertion_static_info __info;
  __info.__semantic_ = __semantic;
  __info.__side_ = __side;
  __info.__is_virtual_ = __is_virtual;
  __info.__overrides_virtual_ = __overrides_virtual;
  return __info;
}

} // namespace std

namespace std::contracts {

enum class assertion_kind : unsigned char {
  __unknown = 0,
  pre = 1,
  post = 2,
  assert = 3,
};

class assertion_context;

} // namespace std::contracts

namespace std {

// Mirrors <contracts>. The compiler synthesizes a call to this to describe one
// evaluation of a contract to its control object. __check evaluates the
// predicate and __args carries the addresses of what that predicate reads out
// of the enclosing function.
constexpr contracts::assertion_context
__create_assertion_context(const char *__comment, source_location __loc,
                           contracts::assertion_static_info __info,
                           contracts::assertion_kind __kind,
                           bool (*__check)(void **), void **__args) noexcept;

} // namespace std

namespace std::contracts {

// D4324: one evaluation of one contract. check() is the point of the whole
// design: the compiler no longer evaluates the predicate and reports failures,
// it hands the control object this and lets the object decide whether to
// evaluate at all, how many times, and what to do about the answer.
class assertion_context {
  const char *__comment_ = nullptr;
  source_location __loc_{};
  assertion_static_info __info_{};
  assertion_kind __kind_ = assertion_kind::__unknown;
  bool (*__check_)(void **) = nullptr;
  void **__args_ = nullptr;

  friend constexpr assertion_context std::__create_assertion_context(
      const char *, source_location, assertion_static_info, assertion_kind,
      bool (*)(void **), void **) noexcept;

public:
  constexpr const char *comment() const noexcept { return __comment_; }
  constexpr source_location location() const noexcept { return __loc_; }
  constexpr assertion_kind kind() const noexcept { return __kind_; }
  constexpr const assertion_static_info &static_info() const noexcept {
    return __info_;
  }
  constexpr evaluation_semantic semantic() const noexcept {
    return __info_.semantic();
  }

  /// Evaluate the predicate. May be called any number of times, including none.
  ///
  /// During constant evaluation there is no type-erased function to call - the
  /// checker's body is generated by the code generator - so the compiler
  /// evaluates the predicate directly instead.
  constexpr bool check() const {
    if consteval {
      return __builtin_contract_check();
    } else {
      return __check_(__args_);
    }
  }
};

} // namespace std::contracts

namespace std {

constexpr contracts::assertion_context
__create_assertion_context(const char *__comment, source_location __loc,
                           contracts::assertion_static_info __info,
                           contracts::assertion_kind __kind,
                           bool (*__check)(void **), void **__args) noexcept {
  contracts::assertion_context __ctx;
  __ctx.__comment_ = __comment;
  __ctx.__loc_ = __loc;
  __ctx.__info_ = __info;
  __ctx.__kind_ = __kind;
  __ctx.__check_ = __check;
  __ctx.__args_ = __args;
  return __ctx;
}

} // namespace std

namespace std::contracts {

template <class T>
concept assertion_control =
    std::is_empty_v<T> && requires(T c, const assertion_context &ctx,
                                   assertion_static_info info) {
      { T::is_ignored(info) } -> std::same_as<bool>;
      { T::constify(info) } -> std::same_as<bool>;
      { c(ctx) } -> std::same_as<void>;
    };

struct default_control {
  static consteval bool is_ignored(assertion_static_info info) {
    return info.semantic() == evaluation_semantic::ignore;
  }
  static consteval bool constify(assertion_static_info) { return false; }
  void operator()(const assertion_context &ctx) const {
    if (ctx.check())
      return;
    if (ctx.semantic() != evaluation_semantic::observe)
      __builtin_trap();
  }
};
inline constexpr default_control default_v{};

// Log-and-continue at the library level, always checked, constified.
struct review {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return true; }
  void operator()(const assertion_context &ctx) const {
    (void)ctx.check();
  }
};
inline constexpr review review_v{};

// Guaranteed-enforced and optimizable.
struct mandatory {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return false; }
  void operator()(const assertion_context &ctx) const {
    if (!ctx.check())
      __builtin_trap();
  }
};
inline constexpr mandatory mandatory_v{};

} // namespace std::contracts

#endif // CONTRACTS_INPUTS_ASSERTION_CONTROL_H
