// Contracts do not survive AST serialization yet, independently of the
// control-object work: ContractSpecifierDecl::CreateDeserialized builds the decl
// with a null DeclContext, and both the IsUninstantiated initializer in
// ContractSpecifierDecl's constructor and the reader's CXXRecordDecl dyn_cast
// then dereference it. This test covers the ContractStmt round-trip, including
// the control object and its synthesized violation call, so the coverage exists
// once that is fixed.
// XFAIL: *

// Test this without pch.
// RUN: %clang_cc1 -std=c++26 -fcontracts -include %s -fsyntax-only -verify %s

// Test with pch.
// RUN: %clang_cc1 -std=c++26 -fcontracts -emit-pch -o %t %s
// RUN: %clang_cc1 -std=c++26 -fcontracts -include-pch %t -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++26 -fcontracts -include-pch %t -triple x86_64-linux-gnu \
// RUN:   -emit-llvm -o - %s | FileCheck %s

// Round-trips D4324 contracts through a PCH. The control object is a child
// expression of ContractStmt and the synthesized violation call is a derived
// member, so both paths through ASTWriterStmt/ASTReaderStmt are exercised, as is
// the HasControlExpr bit that sizes the node before it is created.

#ifndef HEADER
#define HEADER

namespace std {
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
} // namespace std

namespace std::contracts {
enum class evaluation_semantic : unsigned {
  ignore = 0,
  enforce = 1,
  observe = 2,
  quick_enforce = 3,
};
enum class violation_response { proceed, terminate };

struct review {
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return false; }
  static constexpr bool assumable = false;
  violation_response operator()(const char *, std::source_location,
                                evaluation_semantic) const {
    return violation_response::proceed;
  }
};
inline constexpr review review_v{};

struct labeled {
  const char *label;
  static consteval bool is_ignored(assertion_static_info) { return false; }
  static consteval bool constify(assertion_static_info) { return false; }
  static constexpr bool assumable = false;
  violation_response operator()(const char *, std::source_location,
                                evaluation_semantic) const {
    return violation_response::proceed;
  }
};
inline constexpr labeled tagged_v{"tagged"};
} // namespace std::contracts

using namespace std::contracts;

// A named control object.
inline int named(int x) pre<review_v>(x > 0) { return x; }

// A stateful control object: the value has to survive, not just the type.
inline int tagged(int x) pre<tagged_v>(x > 0) { return x; }

// A temporary control object.
inline int temporary(int x) pre<labeled("temp")>(x > 0) { return x; }

// A postcondition with both a result name and a control object, so the trailing
// slot layout [result name][control object][predicate] is exercised in full.
inline int both(int x) post<review_v>(r: r > 0) { return x; }

// contract_assert inside a body.
inline int asserted(int x) {
  contract_assert<review_v>(x > 0);
  return x;
}

// No control object at all.
inline int plain(int x) pre(x > 0) { return x; }

// A template whose contract is re-synthesized on instantiation.
template <class T> T dependent(T x) pre<review_v>(x > 0) { return x; }

// A dependent control object as a non-type template argument.
template <auto C> int with_control(int x) pre<C>(x > 0) { return x; }

#else

// expected-no-diagnostics

// CHECK-LABEL: define {{.*}} @_Z3usei(
int use(int x) {
  return named(x) + tagged(x) + temporary(x) + both(x) + asserted(x) +
         plain(x) + dependent<int>(x) + with_control<review_v>(x);
}

// Each deserialized contract still lowers to the three-step algorithm driven by
// its own control object.
// CHECK: call noundef i32 @{{.*}}6review
// CHECK: call noundef i32 @{{.*}}7labeled

#endif // HEADER
