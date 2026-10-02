// RUN: %clang_cc1 -std=c++23 -triple x86_64-linux-gnu -fsyntax-only -verify %s

// __builtin_char_cast(from, to) reinterprets a range of code units as a
// different character type of the same width, as proposed by P2626. The second
// argument is only used for its type, which selects the target type.

namespace std {
enum class byte : unsigned char {};
}

template <class To, class From>
constexpr auto char_cast(From *p) {
  return __builtin_char_cast(p, (To *)nullptr);
}

namespace Accepted {
// Adopting existing code units into a UTF character type.
void adopt(char *c, unsigned char *uc, std::byte *b, unsigned short *us,
           unsigned int *ui, wchar_t *w) {
  (void)char_cast<char8_t>(c);
  (void)char_cast<char8_t>(uc);
  (void)char_cast<char8_t>(b);
  (void)char_cast<char16_t>(us);
  (void)char_cast<char32_t>(ui);
  (void)char_cast<char32_t>(w); // wchar_t is 32 bits here
}

// And extracting them back out.
void extract(char8_t *u8, char16_t *u16, char32_t *u32) {
  (void)char_cast<char>(u8);
  (void)char_cast<std::byte>(u8);
  (void)char_cast<unsigned short>(u16);
  (void)char_cast<unsigned int>(u32);
}
} // namespace Accepted

namespace ResultType {
char c[1];
const char cc[1]{};
volatile char vc[1];

// The result points at the same objects, so it keeps their qualifiers.
static_assert(__is_same(decltype(__builtin_char_cast(c, (char8_t *)nullptr)),
                        char8_t *));
static_assert(__is_same(decltype(__builtin_char_cast(cc, (char8_t *)nullptr)),
                        const char8_t *));
static_assert(__is_same(decltype(__builtin_char_cast(vc, (char8_t *)nullptr)),
                        volatile char8_t *));

// Qualifiers on the type-carrying argument are not part of the result.
static_assert(__is_same(decltype(__builtin_char_cast(c, (const char8_t *)nullptr)),
                        char8_t *));
} // namespace ResultType

namespace Constexpr {
// The novelty compared to start_lifetime_as: this works during constant
// evaluation.
constexpr char narrow[] = "hi";
static_assert(char_cast<const char8_t>(narrow)[0] == u8'h');
static_assert(char_cast<const char8_t>(narrow)[1] == u8'i');

// Reading back out through the original type.
static_assert(char_cast<const char>(char_cast<const char8_t>(narrow))[0] == 'h');

// A value whose top bit is set reads the same through both signednesses.
constexpr char high[] = {'\x80', 0};
static_assert(char_cast<const char8_t>(high)[0] == 0x80);
static_assert(high[0] == '\x80');

constexpr char8_t utf8[] = u8"ok";
static_assert(char_cast<const char>(utf8)[0] == 'o');
} // namespace Constexpr

namespace Rejected {
void f(char *c, char8_t *u8, int *i, void *v, char16_t *u16) {
  // Both sides are UTF character types.
  (void)__builtin_char_cast(u8, (char16_t *)nullptr); // expected-error {{'__builtin_char_cast' casts between a UTF character type and an integral or byte type; 'char8_t' and 'char16_t' are not such a pair}}

  // Neither side is a UTF character type.
  (void)__builtin_char_cast(c, (int *)nullptr); // expected-error {{'__builtin_char_cast' casts between a UTF character type and an integral or byte type; 'char' and 'int' are not such a pair}}

  // Same idea, with a class type.
  struct S {} s;
  (void)__builtin_char_cast(&s, (char8_t *)nullptr); // expected-error {{'__builtin_char_cast' casts between a UTF character type and an integral or byte type; 'struct S' and 'char8_t' are not such a pair}}

  // Widths differ.
  (void)__builtin_char_cast(c, (char16_t *)nullptr); // expected-error {{'__builtin_char_cast' cannot change the size of a code unit; 'char' and 'char16_t' have different widths}}
  (void)__builtin_char_cast(i, (char8_t *)nullptr);  // expected-error {{'__builtin_char_cast' cannot change the size of a code unit; 'int' and 'char8_t' have different widths}}

  // Not pointers to objects.
  (void)__builtin_char_cast(1, (char8_t *)nullptr);  // expected-error {{1st argument to '__builtin_char_cast' must be a pointer to an object type (was 'int')}}
  (void)__builtin_char_cast(v, (char8_t *)nullptr);  // expected-error {{1st argument to '__builtin_char_cast' must be a pointer to an object type (was 'void *')}}
  (void)__builtin_char_cast(c, (void *)nullptr);     // expected-error {{2nd argument to '__builtin_char_cast' must be a pointer to an object type (was 'void *')}}
  (void)__builtin_char_cast(c, 1);                   // expected-error {{2nd argument to '__builtin_char_cast' must be a pointer to an object type (was 'int')}}

  // Wrong arity.
  (void)__builtin_char_cast(c);                        // expected-error {{too few arguments to function call, expected 2, have 1}}
  (void)__builtin_char_cast(c, u8, u16);               // expected-error {{too many arguments to function call, expected 2, have 3}}
}
} // namespace Rejected
