// RUN: %clang_cc1 -std=c++23 -triple x86_64-linux-gnu -emit-llvm -o - %s \
// RUN:   | FileCheck %s

// __builtin_char_cast only changes the type of the pointer, so it emits
// nothing; the second argument is there for its type alone and is not
// evaluated.

// CHECK-LABEL: define {{.*}}ptr @_Z6adoptsPc(
// CHECK-NEXT:  entry:
// CHECK-NOT:     call
// CHECK:         ret ptr
char8_t *adopts(char *p) { return __builtin_char_cast(p, (char8_t *)nullptr); }

// CHECK-LABEL: define {{.*}}ptr @_Z8extractsPDu(
// CHECK-NOT:     call
// CHECK:         ret ptr
char *extracts(char8_t *p) { return __builtin_char_cast(p, (char *)nullptr); }

int side_effect();

// The type-carrying argument is not evaluated, so its side effects do not
// happen.
// CHECK-LABEL: define {{.*}}@_Z17no_second_operandPc(
// CHECK-NOT:     call {{.*}}@_Z11side_effectv
// CHECK:         ret ptr
char8_t *no_second_operand(char *p) {
  return __builtin_char_cast(p, (char8_t *)(long)side_effect());
}
