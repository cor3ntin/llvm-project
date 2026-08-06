# D4324 Minimal Contracts - CodeGen Implementation Notes

- Branch: `p4324` (in `cppalliance/clang`, based on `efcs/llvm-project` `contracts-nightly`)
- Scope: the CodeGen backend for D4324 named assertion-control objects (`pre<T>`, `post<T>`, `contract_assert<T>`)
- Status: complete. Five commits, net negative line count, no regressions on Windows (clang-cl) or Linux (gcc)

## TL;DR

D4324 lets a contract name an *assertion-control object* type `T` that decides, per assertion, whether the check is ignored, whether the predicate is constified, whether it may be assumed, and what happens on failure. The frontend (parser, AST, Sema validation) already existed on this branch. This work is the code generator: it teaches Clang to lower every contract - named or default - through one uniform three-step algorithm, and it deletes the old bespoke dispatch that preceded it.

The headline result is that the feature was added while the file that implements it, `CGContracts.cpp`, *shrank* from 512 to 186 lines. The new behavior reuses the compiler's ordinary expression code generation instead of hand-rolling an ABI-correct runtime call.

## Running the tests

The clang-side tests need only `ninja clang`, but the libc++ side needs the
runtimes, and the `libcxx/test/std/contracts` tests are `.pass.cpp` - they
compile, link *and run* against the built library, so they catch behavior that no
`-verify` or FileCheck test can:

```bash
cmake -S llvm -B build -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind"
ninja -C build -j8 clang runtimes-test-depends
build/bin/llvm-lit -j8 build/runtimes/runtimes-bins/libcxx/test/std/contracts
```

`runtimes-test-depends` is the part that is easy to miss: without it the headers
are not staged into the build tree and every test fails on `'cassert' file not
found`. Enabling the runtimes also builds `opt` and friends, which several
`clang/test` cases need, so a few CodeGenCXX and Modules tests stop failing as a
side effect.

For a wider run, point lit at `libcxx/test/std` and `libcxx/test/libcxx` rather
than `libcxx/test`. The latter also picks up `libcxx/test/benchmarks`, whose
`.bench.cpp` cases really do run Google Benchmark to completion - single
benchmarks run for tens of minutes and starve the worker pool, so the run looks
hung rather than slow.

## Background: what D4324 changes

A plain contract just carries a predicate:

```cpp
int f(int x) pre(x > 0);
```

D4324 adds an optional control-object type argument:

```cpp
int f(int x) pre<review>(x > 0);          // log-and-continue, always checked
int g(int x) pre<mandatory>(x > 0);       // terminate, optimizer may assume
void h(int x) { contract_assert<default_control>(x != 0); }
```

A control object is an empty class exposing the members the compiler reads. The
shape is what the frontend requires, not something `<contracts>` declares: the
paper proposes the static-property building blocks, so the concept and the worked
control objects below live in the test-local header
`clang/test/Contracts/Inputs/assertion_control.h` rather than in the standard
header.

```cpp
template <class T>
concept assertion_control =
    std::is_empty_v<T> &&
    requires(T c, const char *comment, std::source_location loc,
             assertion_static_info info, evaluation_semantic sem) {
      { T::is_ignored(info) } -> std::same_as<bool>;
      { T::constify(info) }   -> std::same_as<bool>;
      { T::assumable }        -> std::convertible_to<bool>;
      { c(comment, loc, sem) } -> std::same_as<violation_response>;
    };
```

The three worked reference types behave as follows:

| Type | `is_ignored(info)` | `constify(info)` | `assumable` | `operator()` returns |
|---|---|---|---|---|
| `default_control` | `info.semantic() == ignore` | `false` | `false` | `terminate` |
| `review` | `false` (always checked) | `true` | `false` | `proceed` |
| `mandatory` | `false` (always checked) | `false` | `true` | `terminate` |

### `assertion_static_info`

`is_ignored` and `constify` are `consteval` and take the contract's static
(compile-time-known) properties, rather than being handed a bare config value or
being plain data members. This is what lets one control object make a different
decision for, say, a precondition than for a `contract_assert`:

```cpp
class assertion_static_info {
public:
  constexpr evaluation_semantic semantic() const noexcept;
  constexpr assertion_check_side side() const noexcept;   // not_applicable / definition / client
  constexpr bool is_virtual() const noexcept;
  constexpr bool overrides_virtual() const noexcept;
};
```

The type lives in `libcxx/include/contracts` with private members. The compiler
does not construct it directly: it synthesizes a call to the reserved
`std::__create_assertion_static_info(semantic, side, is_virtual,
overrides_virtual)`, a `consteval` function that is the type's only friend. The
two enumeration argument types are read off that function's parameters rather
than looked up by name, so the library keeps control of what they are.

Naming a control object in a translation unit that never included `<contracts>`
therefore fails that lookup, which is reported as
`err_contract_control_no_static_info` - once per translation unit, since every
contract naming a control object would otherwise repeat it twice (once for
`constify`, once for the dispatch).

`std::contracts::evaluation_semantic` gained the `ignore` and `quick_enforce`
enumerators it was missing, so it now matches clang's
`ContractEvaluationSemantic` one for one and a semantic can be passed through
without a translation table. `ignore` deliberately shares its value with the
pre-existing `__unknown`, because the runtime ABI already fixes `enforce` and
`observe` at 1 and 2.

That made the separate `evaluation_config` enum redundant, so it is gone and
`operator()` takes an `evaluation_semantic`. Keeping both would have shipped two
enumerations in one public header that disagree about which value `enforce` is
(2 in the old `evaluation_config`, 1 in `evaluation_semantic`), which is the kind
of thing that silently misroutes a violation.

`is_virtual()` and `overrides_virtual()` are groundwork for virtual-function
semantics; like the reference implementation, the compiler fills both with
`false` today whatever the enclosing function is.

## What was already done (the frontend)

The frontend was complete before this work and is not touched here except to synthesize the violation call:

- `libcxx/include/contracts` carries only what the paper proposes: `evaluation_semantic`, `assertion_check_side`, `assertion_static_info` and the reserved `std::__create_assertion_static_info`. The control-object interface the compiler reads (the concept, `default_control` / `review` / `mandatory`, `violation_response`) lives in the test-local header `clang/test/Contracts/Inputs/assertion_control.h`, which also mirrors the static-info types so the clang tests need no real libc++.
- `ContractStmt` (`clang/include/clang/AST/StmtCXX.h`) already carried `QualType ControlObjectType` with `hasExplicitControlType()`, serialized, profiled, printed as `pre<T>(...)`, and dumped.
- The parser reads the optional `<type-id>` after the contract keyword and late-parses angle-bracket tokens for inline member contracts; instantiation substitutes the type in `TransformContractStmt`.
- Sema `CheckContractControlType` validates that `T` is an empty, complete class with `is_ignored` / `constify` / `assumable` / `operator()`, and `shouldConstifyContractPredicate` gates constification per assertion by folding `T::constify(info)`. That query runs while the contract is still being parsed - the predicate has not been parsed yet, because whether it is constified is what is being decided - so there is no `ContractStmt` to read a per-contract semantic off, and the build-selected default is used for `info.semantic()`.

## The algorithm the compiler must emit

For a contract with control-object type `T` and the build-selected semantic `sem`:

```text
1. If T::is_ignored(info):
       if T::assumable: emit llvm.assume(pred)
       else:            emit nothing
       stop.
2. Evaluate the predicate.
3. If the predicate is false:
       r = T{}(comment, loc, sem)
       if r == violation_response::terminate:  trap / terminate
       else:                                   fall through and continue.
```

The default (unnamed) path is the same algorithm with the built-in defaults of `default_control`: ignored exactly when the build semantic is `ignore`, never assumable, constify false, and a violation reaction keyed by the build semantic rather than a user `operator()`.

## Core design decision: synthesize the violation call in Sema

Rather than hand-build an ABI-correct call to `operator()` in the code generator, the call is synthesized as an ordinary AST expression in Sema and stored on the `ContractStmt`. CodeGen then just emits that expression and branches on its result. This reuses overload resolution, argument conversions, temporary materialization, the calling convention, and ODR-use / emission machinery that already exist and are correct.

The synthesized expression is exactly:

```cpp
T{}(comment, loc, sem)
```

built from these pieces (all in `synthesizeControlObjectDispatch`, `clang/lib/Sema/SemaContract.cpp`):

- `T{}` - `BuildCXXTypeConstructExpr` with empty args and list-initialization.
- `comment` - a `StringLiteral` of the predicate source text (`ContractStmt::getMessage`), which decays to `const char*` during argument passing.
- `loc` - `std::source_location::current()`. This is the subtle one (see Gotchas). It is built by looking up `current` on the `source_location` type taken from `operator()`'s second parameter and calling it; being `consteval`, it folds to a constant the aggregate emitter can lower.
- `sem` - a `DeclRefExpr` to the `evaluation_semantic` enumerator for the build-selected semantic. No translation is needed: `ContractEvaluationSemantic` and `std::contracts::evaluation_semantic` share their values.
- The call itself - `BuildCallToObjectOfClassType(nullptr, T{}, ...)`, wrapped in `MaybeCreateExprWithCleanups` so any temporaries are destroyed at end of full-expression.

Two booleans are const-evaluated at the same time and stored alongside the call:

- `ControlIsIgnored` - `T::is_ignored(info)`, evaluated by building and folding a call to the static member, where `info` is the synthesized `std::__create_assertion_static_info(...)` call.
- `ControlAssumable` - `T::assumable`, read via `VarDecl::evaluateValue`.

Synthesis runs only when the control type is non-null, non-dependent, and the enclosing context is non-dependent. Dependent cases (templates, class templates, contracts in template bodies) are re-synthesized when `TransformContractStmt` rebuilds the statement during instantiation, so each instantiation gets a correctly-typed call.

## Data model: what `ContractStmt` grew

Three plain members were added next to the existing `ControlObjectType` (mirroring how that member was threaded in the frontend commit):

```cpp
Stmt *ViolationCall = nullptr;   // the synthesized T{}(comment, loc, sem)
bool ControlIsIgnored = false;   // const-evaluated T::is_ignored(info)
bool ControlAssumable = false;   // const-evaluated T::assumable
```

`ViolationCall` is stored as a plain member rather than a child of the statement: it is fully derived from `ControlObjectType` and must not be walked or transformed generically (it is re-synthesized per instantiation instead). All three are threaded through AST read/write in `ASTReaderStmt.cpp` / `ASTWriterStmt.cpp`, with scalar and sub-statement read order kept in lock-step with write order. The Modules test (`clang/test/Modules/contracts.cppm`) exercises this round-trip.

## Lowering: one function for both paths

After the work, the entire code generator is a single function, `CodeGenFunction::EmitContractStmtAsFullStmt` (`clang/lib/CodeGen/CGContracts.cpp`):

```cpp
const bool HasControl = S.hasExplicitControlType();
const ContractEvaluationSemantic Semantic = S.getSemantic(getContext());

// Step 1: is_ignored (+ optional assume).
const bool IsIgnored =
    HasControl ? S.controlIsIgnored() : (Semantic == Ignore);
if (IsIgnored) {
  if (HasControl && S.controlAssumable())
    Builder.CreateAssumption(EvaluateExprAsBool(S.getCond()));
  return;
}

// Step 2: evaluate the predicate. Exceptions propagate; no try/catch.
llvm::BasicBlock *Violation = createBasicBlock("contract.violation");
llvm::BasicBlock *End = createBasicBlock("contract.end");
Builder.CreateCondBr(EvaluateExprAsBool(S.getCond()), End, Violation);

// Step 3: react.
EmitBlock(Violation);
if (HasControl) {
  // r = T{}(comment, loc, sem); trap iff r == terminate.
  ...compare EmitScalarExpr(ViolationCall) against the `terminate` enumerator...
} else if (Semantic == QuickEnforce) {
  CreateTrap(*this);
} else {
  // observe reports and continues; enforce reports and does not return.
  EmitHandleContractViolationCall(..., /*IsNoReturn=*/Semantic == Enforce);
}
EmitBlock(End);
```

The `terminate` value is not hard-coded: it is looked up by enumerator name from the `violation_response` enum, which is the return type of the synthesized call.

### What the IR looks like

For `int f(int x) pre<review>(x > 0)` the violation path is exactly the three steps, with no `__handle_contract_violation` in sight:

```llvm
  %cmp = icmp sgt i32 %0, 0
  br i1 %cmp, label %contract.end, label %contract.violation

contract.violation:
  ; ... build source_location temporary ...
  %call = call i32 @_ZNKSt9contracts6reviewclE...(ptr %ref.tmp, ptr @.str, ptr %loc, i32 noundef 2)
  %t = icmp eq i32 %call, 1            ; 1 == violation_response::terminate
  br i1 %t, label %contract.trap, label %contract.end

contract.trap:
  call void @llvm.trap()
  unreachable

contract.end:
  ret i32 %x
```

An ignored-but-assumable control object collapses to a single hint:

```llvm
  call void @llvm.assume(i1 %cmp)
```

The default (unnamed) path still calls the runtime handler, now inlined per contract:

```llvm
contract.violation.handler:
  call void @__handle_contract_violation_v3(i32 1, i32 1, ptr @contract.loc)
  unreachable
```

## Implementation walkthrough (the five commits)

Each commit builds, runs its new test, and runs the full `clang/test/Contracts` suite before landing, and never regresses a passing test.

1. **`8f84764d` - Emit the three-step algorithm for explicit control types.** Adds the three `ContractStmt` fields and their serialization, synthesizes the violation call and the two flags in Sema, and adds an additive CodeGen path gated on `hasExplicitControlType()`. The default path is untouched. Adds `control-object-codegen.cpp`.
2. **`5676205e` - Unify the default path onto the three-step algorithm.** Folds the explicit-type path and the default path into one function. The default path uses `default_control`'s built-in behavior, and predicates are now evaluated directly, so a throwing predicate propagates to the caller instead of being converted into a violation. Adds `exception-propagation.cpp`.
3. **`893e4a0d` - Remove the old contract dispatch machinery.** Deletes the now-dead shared enforce/trap blocks and their PHI plumbing, `GetSharedContractViolation*Block`, `CurrentContractInfo` / `CurrentContractRAII` / `CGContractData`, the emitting-try-body / emitting-catch-body checkpoints, `BuildTryCatch`, the contract-local `StmtCanThrow` / `FunctionCanThrow`, the unused mangled-handler helper, and `ContractDetectionMode::ExceptionRaised`. Pure dead-code removal, no behavior change.
4. **`a2186b6b` - Emit `llvm.assume` for ignored, assumable contracts.** Completes step 1: an ignored contract whose control object is assumable hands the predicate to the optimizer. Adds `control-object-assume.cpp`.
5. **`e26bcac8` - Remove the dead `-fcontract-exceptions` flag.** Its only consumer was the predicate try/catch removed in commit 2. `ContractConstification` and `ContractLambdaCaptureRestrictions` are intentionally kept: both are still read by Sema.

## Files touched

- `clang/include/clang/AST/StmtCXX.h` - new `ContractStmt` fields and accessors
- `clang/lib/Sema/SemaContract.cpp` - `synthesizeControlObjectDispatch` and its wiring into `BuildContractStmt`
- `clang/lib/CodeGen/CGContracts.cpp` - the unified `EmitContractStmtAsFullStmt`; removal of the old dispatch
- `clang/lib/CodeGen/CodeGenFunction.{h,cpp}` - removed dead declarations and finish-function hooks
- `clang/lib/Serialization/ASTReaderStmt.cpp`, `ASTWriterStmt.cpp` - serialize the three new fields
- `clang/include/clang/Basic/ContractOptions.h` - drop `ExceptionRaised`
- `clang/include/clang/Basic/LangOptions.def`, `clang/include/clang/Options/Options.td`, `clang/lib/Driver/ToolChains/Clang.cpp` - drop `-fcontract-exceptions`
- `clang/test/Contracts/{control-object-codegen,control-object-assume,exception-propagation}.cpp` - new tests

## Testing and verification

- `clang/test/Contracts` + `clang/test/Parser/cxx-contracts.cpp` + `clang/test/Parser/contract-inline-methods.cpp` + `clang/test/Modules/contracts.cppm`: all green except the nine failures that pre-existed on this branch (they fail identically on Linux).
- Broader `clang/test/CodeGenCXX` + `clang/test/SemaCXX`: 2561 pass. The only failures are ThinLTO / profile tests that need external tools (`opt`, `llvm-profdata`) not built in this clang-only configuration, and are unrelated to contracts.
- End-to-end features exercised: preconditions, postconditions with a result name (`post<review>(r: r > 0)`), `contract_assert`, control types on late-parsed inline member functions, and template-dependent control types instantiated with `review` / `mandatory` / `default_control`.
- Linux/gcc cross-check: the branch builds clean under gcc in WSL and the Contracts / Parser / Modules suite is green with the same nine pre-existing failures.

## Gotchas and deviations from the original plan

- **`source_location` is not a bare `SourceLocExpr`.** Building a `SourceLocExpr` of the struct type produces the `__builtin_source_location()` *pointer*, which the aggregate emitter cannot lower ("cannot compile this aggregate expression yet"). The fix is to build `std::source_location::current()` and let the consteval call fold to a constant. The relevant enumerator is `SourceLocIdentKind::SourceLocStruct`, not `SourceLocation`.
- **The `terminate` value is discovered, not assumed.** CodeGen finds the `terminate` enumerator by name on the call's return enum, so it does not depend on the layout of `violation_response`.
- **`mandatory` never actually assumes.** `mandatory::is_ignored()` returns `false` for every config, so it is always checked at runtime and never reaches the assume branch. The assume path is real and is tested with a control type that is genuinely ignored and assumable; the paper's `assumable` flag on `mandatory` only matters to a build that forces the check off.
- **Not every flag was removable.** Only `ContractExceptions` was fully dead. `ContractConstification` (constification fallback and `this`-adjustment) and `ContractLambdaCaptureRestrictions` (the lambda-capture checker) are still live in Sema, and the default-path violation-info machinery (`BuildViolationObject` / `GetAddrOfUnnamedGlobalConstantDecl`) is still used by the observe/enforce reaction.
- **Constification is broken at baseline.** `constification.cpp` fails on this branch independently of this work; the CodeGen phase does not rely on it.
- **A dependent control object must not constify the template pattern.** Everything the compiler reads off a control object is deferred to instantiation when the object is dependent - a nested class of a class template, a `Outer<T>::ctl` qualifier, a reference NTTP. `TransformContractStmt` substitutes the object first and re-asks before transforming the predicate, so the per-instantiation policy is applied correctly. The trap is the fallback used while the pattern is parsed: returning `LangOpts.ContractConstification` (on by default) constifies the pattern, which rejects a mutating predicate before any instantiation exists and cannot be undone later, since constification only ever adds const. The fallback for a dependent object is therefore `false` - be permissive on the pattern and let instantiation decide. `control-object-dependent.cpp` pins this down.
- **Control-object validation looks through bases.** The members the compiler reads (`is_ignored`, `constify`, `assumable`, `operator()`) are found with `LookupQualifiedName`, not `CXXRecordDecl::lookup`, so an object may inherit the interface instead of restating it. The two had disagreed: validation used the non-inheriting lookup and rejected objects the dispatch would then have called happily.

## Commit log

```text
e26bcac8  [Contracts][D4324] Remove the dead -fcontract-exceptions flag
a2186b6b  [Contracts][D4324] Emit llvm.assume for ignored, assumable contracts
893e4a0d  [Contracts][D4324] Remove the old contract dispatch machinery
5676205e  [Contracts][D4324] Unify the default path onto the three-step algorithm
8f84764d  [Contracts][D4324] Emit the three-step algorithm for explicit control types
```
