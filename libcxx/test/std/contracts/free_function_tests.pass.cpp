// ADDITIONAL_COMPILE_FLAGS: -std=c++26 -Xclang -fcontracts -fcontract-evaluation-semantic=enforce -fcontract-group-evaluation-semantic=observe=observe,enforce=enforce -g

#include "nttp_string.h"
#include "contracts_support.h"
#include "contracts_handler.h"

#include "check_assertion.h"
using namespace std::contracts;

// test_register.h is deliberately not included: it defines its own main() for
// REGISTER_TEST-based tests, and this one drives itself.
void my_handler(const contract_violation&) {}

int main() {
  ContractHandlerInstaller install_handler_guard(my_handler) ;

}