//===--- CGBlocks.cpp - Emit LLVM Code for declarations ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit blocks.
//
//===----------------------------------------------------------------------===//

#include "CGContracts.h"
#include "CGCXXABI.h"
#include "CGDebugInfo.h"
#include "CGObjCRuntime.h"
#include "CGOpenCLRuntime.h"
#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "ConstantEmitter.h"
#include "TargetInfo.h"
#include "clang/AST/Attr.h"
#include "clang/AST/DeclObjC.h"
#include "clang/CodeGen/ConstantInitBuilder.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/ScopedPrinter.h"
#include <algorithm>
#include <cstdio>
#include <optional>

using namespace clang;
using namespace CodeGen;

constexpr ContractEvaluationSemantic Enforce =
    ContractEvaluationSemantic::Enforce;
constexpr ContractEvaluationSemantic QuickEnforce =
    ContractEvaluationSemantic::QuickEnforce;
constexpr ContractEvaluationSemantic Observe =
    ContractEvaluationSemantic::Observe;
constexpr ContractEvaluationSemantic Ignore =
    ContractEvaluationSemantic::Ignore;

constexpr ContractDetectionMode PredicateFailed =
    ContractDetectionMode::PredicateFailed;

namespace clang::CodeGen {

template <class T>
static llvm::Constant *CreateConstantInt(CodeGenFunction &CGF, T Sem) {
  static_assert(std::is_same_v<T, ContractEvaluationSemantic> ||
                std::is_same_v<T, ContractDetectionMode>);
  return llvm::ConstantInt::get(CGF.IntTy, (int)Sem);
}

static void CreateTrap(CodeGenFunction &CGF) {
  auto &Builder = CGF.Builder;
  llvm::CallInst *TrapCall = CGF.EmitTrapCall(llvm::Intrinsic::trap);
  TrapCall->setDoesNotReturn();
  TrapCall->setDoesNotThrow();
  Builder.CreateUnreachable();
  Builder.ClearInsertionPoint();
}

} // namespace clang::CodeGen

void CodeGenFunction::EmitHandleContractViolationCall(
    llvm::Constant *EvalSemantic, llvm::Constant *DetectionMode,
    llvm::Value *ViolationInfoGV, bool IsNoReturn) {
  auto &Ctx = getContext();

  CanQualType ArgTypes[3] = {Ctx.UnsignedIntTy, Ctx.UnsignedIntTy,
                             Ctx.VoidPtrTy};

  const CGFunctionInfo &VFuncInfo =
      CGM.getTypes().arrangeBuiltinFunctionDeclaration(getContext().VoidTy,
                                                       ArgTypes);

  StringRef TargetFuncName = "__handle_contract_violation_v3";
  llvm::FunctionType *VFTy = CGM.getTypes().GetFunctionType(VFuncInfo);
  llvm::FunctionCallee VFunc = CGM.CreateRuntimeFunction(VFTy, TargetFuncName);

  if (IsNoReturn) {
    llvm::Value *Args[3] = {EvalSemantic, DetectionMode, ViolationInfoGV};
    EmitNoreturnRuntimeCallOrInvoke(VFunc, Args);
    Builder.ClearInsertionPoint();
  } else {
    CallArgList Args;
    Args.add(RValue::get(EvalSemantic), getContext().UnsignedIntTy);
    Args.add(RValue::get(DetectionMode), getContext().UnsignedIntTy);
    Args.add(RValue::get(ViolationInfoGV), getContext().VoidPtrTy);
    EmitCall(VFuncInfo, CGCallee::forDirect(VFunc), ReturnValueSlot(), Args);
  }
}

// D4324 assertion_context::check(): generate the body of the synthesized
// `bool(void **)` that evaluates this contract's predicate.
//
// The predicate is emitted exactly as written. What makes that work outside the
// function it belongs to is the prologue: each captured declaration is bound to
// the address the array carries, so when the predicate names a parameter, the
// ordinary lvalue emission finds it. That is where the reinterpret_cast to the
// captured entity's real type happens - as a pointer bitcast on the way in -
// rather than in a rewritten copy of the predicate.
void CodeGenFunction::GenerateContractCheckFunction(const ContractStmt &S,
                                                    llvm::Function *Fn,
                                                    const FunctionDecl *FD) {
  ASTContext &Ctx = getContext();
  const ParmVarDecl *ArgsParm = FD->getParamDecl(0);

  FunctionArgList Args;
  Args.push_back(const_cast<ParmVarDecl *>(ArgsParm));

  const CGFunctionInfo &FnInfo =
      CGM.getTypes().arrangeFunctionDeclaration(FD);
  StartFunction(GlobalDecl(FD), Ctx.BoolTy, Fn, FnInfo, Args,
                S.getKeywordLoc(), S.getKeywordLoc());

  // void **__args
  Address ArgsAddr = GetAddrOfLocalVar(ArgsParm);
  llvm::Value *ArgsPtr = Builder.CreateLoad(ArgsAddr, "contract.args");

  unsigned Index = 0;
  for (const ValueDecl *Captured : S.getCaptures()) {
    llvm::Value *Slot = Builder.CreateConstInBoundsGEP1_32(
        VoidPtrTy, ArgsPtr, Index++, "contract.arg.slot");
    llvm::Value *Raw =
        Builder.CreateLoad(Address(Slot, VoidPtrTy, getPointerAlign()),
                           "contract.arg");

    if (!Captured) {
      // A null entry is `this`; the array holds the object pointer itself.
      CXXThisValue = Raw;
      continue;
    }

    QualType T = Captured->getType();
    // A reference is captured by the address of what it binds to, so the
    // pointer in the array already is the referent.
    QualType Pointee = T.getNonReferenceType();
    Address Bound(Raw, ConvertTypeForMem(Pointee),
                  Ctx.getTypeAlignInChars(Pointee));
    setAddrOfLocalVar(cast<VarDecl>(Captured), Bound);
  }

  // Store through the return slot rather than emitting a bare ret, so the
  // epilogue FinishFunction builds stays the single exit.
  llvm::Value *Result = EvaluateExprAsBool(S.getCond());
  EmitStoreOfScalar(Result, MakeAddrLValue(ReturnValue, Ctx.BoolTy),
                    /*isInit=*/true);
  FinishFunction(S.getKeywordLoc());
}

// Emit the contract expression.
void CodeGenFunction::EmitContractStmt(const ContractStmt &S) {
  EmitContractStmtAsFullStmt(S);
}

void CodeGenFunction::EmitContractStmtAsFullStmt(const ContractStmt &S) {
  // D4324: every contract - whether it names an explicit control object or uses
  // the built-in defaults - lowers to the same three-step algorithm:
  //
  //   1. If is_ignored(cfg): stop, but if the control object is assumable hand
  //      the predicate to the optimizer via llvm.assume first.
  //   2. Evaluate the predicate. Exceptions propagate; there is no try/catch.
  //   3. If it is false, react. For an explicit control object T the reaction is
  //      r = T{}(comment, loc, cfg), trapping iff r is violation_response::
  //      terminate. For the default path the reaction is keyed by the build
  //      semantic: observe reports and continues, enforce reports and does not
  //      return, quick_enforce traps.

  const bool HasControl = S.hasExplicitControl();
  const ContractEvaluationSemantic Semantic = S.getSemantic(getContext());

  // D4324: the predicate also has to exist as a standalone function so the
  // control object can call it through assertion_context::check(). It is
  // generated on its own CodeGenFunction, since we are in the middle of
  // emitting the function the contract is attached to.
  if (const FunctionDecl *CheckFD = S.getCheckFn()) {
    auto *CheckFn = cast<llvm::Function>(
        CGM.GetAddrOfFunction(GlobalDecl(CheckFD))->stripPointerCasts());
    if (CheckFn->empty()) {
      CodeGenFunction(CGM).GenerateContractCheckFunction(S, CheckFn, CheckFD);
    }
  }

  // Step 1. For the default path the built-in default_control is ignored exactly
  // when the semantic is 'ignore'.
  const bool IsIgnored =
      HasControl ? S.controlIsIgnored() : (Semantic == Ignore);
  if (IsIgnored) {
    // A skipped check emits nothing, unless the control object is assumable, in
    // which case the predicate is handed to the optimizer as an assumption. The
    // default path is never assumable.
    if (HasControl && S.controlAssumable())
      Builder.CreateAssumption(EvaluateExprAsBool(S.getCond()));
    return;
  }

  // A named control object owns the whole evaluation: the predicate is not
  // evaluated here at all, it is handed over as assertion_context::check() and
  // the object decides whether to call it, how often, and what a false answer
  // means. So there is nothing to branch on - just make the call.
  if (HasControl) {
    const Expr *ViolationCall = S.getViolationCall();
    assert(ViolationCall &&
           "explicit control object without a synthesized dispatch call");
    EmitIgnoredExpr(ViolationCall);
    return;
  }

  // Step 2. The default path still evaluates the predicate itself.
  llvm::BasicBlock *Violation = createBasicBlock("contract.violation");
  llvm::BasicBlock *End = createBasicBlock("contract.end");
  Builder.CreateCondBr(EvaluateExprAsBool(S.getCond()), End, Violation);

  // Step 3.
  EmitBlock(Violation);
  if (Semantic == QuickEnforce) {
    CreateTrap(*this);
  } else {
    assert((Semantic == Observe || Semantic == Enforce) &&
           "ignore handled above; quick_enforce handled above");
    llvm::Constant *ViolationInfo =
        CGM.GetAddrOfUnnamedGlobalConstantDecl(
               getContext().BuildViolationObject(
                   &S, dyn_cast_or_null<FunctionDecl>(CurFuncDecl)),
               "contract.loc")
            .getPointer();
    EmitHandleContractViolationCall(CreateConstantInt(*this, Semantic),
                                    CreateConstantInt(*this, PredicateFailed),
                                    ViolationInfo,
                                    /*IsNoReturn=*/Semantic == Enforce);
    if (Semantic != Enforce)
      Builder.CreateBr(End);
  }

  EmitBlock(End);
}
