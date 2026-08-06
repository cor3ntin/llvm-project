// ADDITIONAL_COMPILE_FLAGS: -std=c++26 -Xclang -fcontracts -fcontract-evaluation-semantic=enforce -fcontract-group-evaluation-semantic=observe=observe,enforce=enforce

// This test expects an exception thrown from a predicate to be turned into a
// contract violation, with detection_mode::evaluation_exception and
// std::current_exception() set while the handler runs. Under D4324 a predicate's
// exception propagates instead, and deciding whether to catch it becomes the
// control object's business: its operator() is handed an assertion_context and
// calls check() itself, so it can wrap that call in a try block. Until that
// inversion of control is implemented, the throw here escapes and terminates.
// XFAIL: *


#include <contracts>
#include "nttp_string.h"
#include "contracts_support.h"
#include "contracts_handler.h"
#include "test_register.h"
#include <exception>
#include <stdexcept>

namespace exception_handling {
#define OBSERVE [[clang::contract_group("observe")]]
void foo() {

  contract_assert [[clang::contract_group("observe")]] ((true) ? throw 42 : true);


}

REGISTER_TEST(test_ex_handling) {
  ContractHandlerInstaller CHI;
  int called = 0;
  CHI.install([&]() {
    ++called;
    assert(std::current_exception());
  });
  contract_assert OBSERVE ((true) ? throw 42 : true);
  assert(called);
  called = 0;

  CHI.install([&]() {
    ++called;
    assert(!std::current_exception());
    assert(KV<"Local"> == 1);
    assert(KV<"Local2"> == 0);

    throw 42;
  });

  try {

    [] {
      CAliveCounter<"Local"> Local;
      contract_assert((CAliveCounter<"Local2">{}, false));
      assert(KV<"Local2"> == 1);
    }();
    assert(false);
  } catch (int) {
    KV<"InCatch">++;
  }
  assert(called == 1);
  assert(NC<"Local"> == 0);
  assert(KV<"Local2"> == 0);
  assert(NC<"InCatch">.consume() == 1);

};

} // namespace exception_handling
