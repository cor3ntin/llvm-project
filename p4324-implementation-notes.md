# D4324 Minimal Contracts - CodeGen Implementation Notes

- Branch: `p4324` (in `cppalliance/clang`, based on `efcs/llvm-project` `contracts-nightly`)
- Scope: the CodeGen backend for D4324 named assertion-control objects (`pre<T>`, `post<T>`, `contract_assert<T>`)
- Status: complete. Five commits, net negative line count, no regressions on Windows (clang-cl) or Linux (gcc)

## TL;DR

D4324 lets a contract name an *assertion-control object* type `T` that decides, per assertion, whether the check is ignored, whether the predicate is constified, whether it may be assumed, and what happens on failure. The frontend (parser, AST, Sema validation) already existed on this branch. This work is the code generator: it teaches Clang to lower every contract - named or default - through one uniform three-step algorithm, and it deletes the old bespoke dispatch that preceded it.

The headline result is that the feature was added while the file that implements it, `CGContracts.cpp`, *shrank* from 512 to 186 lines. The new behavior reuses the compiler's ordinary expression code generation instead of hand-rolling an ABI-correct runtime call.

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

A control object is an empty class modeling the `std::contracts::assertion_control` concept:

```cpp
template <class T>
concept assertion_control =
    std::is_empty_v<T> &&
    requires(T c, const char *comment, std::source_location loc,
             evaluation_config cfg) {
      { T::is_ignored(cfg) } -> std::same_as<bool>;
      { T::constify }        -> std::convertible_to<bool>;
      { T::assumable }       -> std::convertible_to<bool>;
      { c(comment, loc, cfg) } -> std::same_as<violation_response>;
    };
```

The three worked reference types behave as follows:

| Type | `is_ignored(cfg)` | `constify` | `assumable` | `operator()` returns |
|---|---|---|---|---|
| `default_control` | `cfg == ignore` | `false` | `false` | `terminate` |
| `review` | `false` (always checked) | `true` | `false` | `proceed` |
| `mandatory` | `false` (always checked) | `false` | `true` | `terminate` |

## What was already done (the frontend)

The frontend was complete before this work and is not touched here except to synthesize the violation call:

- Library types live in the test-local header `clang/test/Contracts/Inputs/assertion_control.h` (no dependency on a real `<contracts>` header).
- `ContractStmt` (`clang/include/clang/AST/StmtCXX.h`) already carried `QualType ControlObjectType` with `hasExplicitControlType()`, serialized, profiled, printed as `pre<T>(...)`, and dumped.
- The parser reads the optional `<type-id>` after the contract keyword and late-parses angle-bracket tokens for inline member contracts; instantiation substitutes the type in `TransformContractStmt`.
- Sema `CheckContractControlType` validates that `T` is an empty, complete class with `is_ignored` / `constify` / `assumable` / `operator()`, and `shouldConstifyContractPredicate` gates constification per assertion.

## The algorithm the compiler must emit

For a contract with control-object type `T` and the build-selected config `cfg`:

```text
1. If T::is_ignored(cfg):
       if T::assumable: emit llvm.assume(pred)
       else:            emit nothing
       stop.
2. Evaluate the predicate.
3. If the predicate is false:
       r = T{}(comment, loc, cfg)
       if r == violation_response::terminate:  trap / terminate
       else:                                   fall through and continue.
```

The default (unnamed) path is the same algorithm with the built-in defaults of `default_control`: ignored exactly when the build semantic is `ignore`, never assumable, constify false, and a violation reaction keyed by the build semantic rather than a user `operator()`.

## Core design decision: synthesize the violation call in Sema

Rather than hand-build an ABI-correct call to `operator()` in the code generator, the call is synthesized as an ordinary AST expression in Sema and stored on the `ContractStmt`. CodeGen then just emits that expression and branches on its result. This reuses overload resolution, argument conversions, temporary materialization, the calling convention, and ODR-use / emission machinery that already exist and are correct.

The synthesized expression is exactly:

```cpp
T{}(comment, loc, cfg)
```

built from these pieces (all in `synthesizeControlObjectDispatch`, `clang/lib/Sema/SemaContract.cpp`):

- `T{}` - `BuildCXXTypeConstructExpr` with empty args and list-initialization.
- `comment` - a `StringLiteral` of the predicate source text (`ContractStmt::getMessage`), which decays to `const char*` during argument passing.
- `loc` - `std::source_location::current()`. This is the subtle one (see Gotchas). It is built by looking up `current` on the `source_location` type taken from `operator()`'s second parameter and calling it; being `consteval`, it folds to a constant the aggregate emitter can lower.
- `cfg` - a `DeclRefExpr` to the `evaluation_config` enumerator whose value corresponds to the build-selected semantic.
- The call itself - `BuildCallToObjectOfClassType(nullptr, T{}, ...)`, wrapped in `MaybeCreateExprWithCleanups` so any temporaries are destroyed at end of full-expression.

Two booleans are const-evaluated at the same time and stored alongside the call:

- `ControlIsIgnored` - `T::is_ignored(cfg)`, evaluated by building and folding a call to the static member.
- `ControlAssumable` - `T::assumable`, read via `VarDecl::evaluateValue`.

Synthesis runs only when the control type is non-null, non-dependent, and the enclosing context is non-dependent. Dependent cases (templates, class templates, contracts in template bodies) are re-synthesized when `TransformContractStmt` rebuilds the statement during instantiation, so each instantiation gets a correctly-typed call.

## Data model: what `ContractStmt` grew

Three plain members were added next to the existing `ControlObjectType` (mirroring how that member was threaded in the frontend commit):

```cpp
Stmt *ViolationCall = nullptr;   // the synthesized T{}(comment, loc, cfg)
bool ControlIsIgnored = false;   // const-evaluated T::is_ignored(cfg)
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
  // r = T{}(comment, loc, cfg); trap iff r == terminate.
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

## Commit log

```text
e26bcac8  [Contracts][D4324] Remove the dead -fcontract-exceptions flag
a2186b6b  [Contracts][D4324] Emit llvm.assume for ignored, assumable contracts
893e4a0d  [Contracts][D4324] Remove the old contract dispatch machinery
5676205e  [Contracts][D4324] Unify the default path onto the three-step algorithm
8f84764d  [Contracts][D4324] Emit the three-step algorithm for explicit control types
```
