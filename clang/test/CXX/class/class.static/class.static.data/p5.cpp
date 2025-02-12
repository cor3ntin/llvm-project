// RUN: %clang_cc1 -std=c++26 -fsyntax-only -verify %s 
int bar();

int foo1() {
    struct S {
        static constexpr int x = 1;
        static int y;  // expected-warning {{variable 'foo1()::S::y' has internal linkage but is not defined}}
                       //   expected-note@#foo1-assign {{used here}}
        inline static int z = bar();
    };
    static_assert(S::x == 1);
    S::y = 2; // #foo1-assign
    return S::z;
}

int foo2() {
    struct {
        static constexpr int x = 1;
        static int y;  // expected-warning {{variable 'foo2()::(anonymous struct)::y' has internal linkage but is not defined}}
                       //   expected-note@#foo2-assign {{used here}}
        inline static int z = bar();
    } s;
    static_assert(s.x == 1);
    s.y = 2; // #foo2-assign
    return s.z;
}
