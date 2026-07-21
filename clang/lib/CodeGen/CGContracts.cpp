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

// Emit the contract expression.
void CodeGenFunction::EmitContractStmt(const ContractStmt &S) {
  EmitContractStmtAsFullStmt(S);
}

void CodeGenFunction::EmitContractStmtAsFullStmt(const ContractStmt &S) {
  // D4324: every contract - whether it names an explicit control object or uses
  // the built-in defaults - lowers to the same three-step algorithm:
  //
  //   1. If is_ignored(cfg): stop. (An assume for assumable control objects is
  //      emitted here in a later step.)
  //   2. Evaluate the predicate. Exceptions propagate; there is no try/catch.
  //   3. If it is false, react. For an explicit control object T the reaction is
  //      r = T{}(comment, loc, cfg), trapping iff r is violation_response::
  //      terminate. For the default path the reaction is keyed by the build
  //      semantic: observe reports and continues, enforce reports and does not
  //      return, quick_enforce traps.

  const bool HasControl = S.hasExplicitControlType();
  const ContractEvaluationSemantic Semantic = S.getSemantic(getContext());

  // Step 1. For the default path the built-in default_control is ignored exactly
  // when the semantic is 'ignore'.
  const bool IsIgnored =
      HasControl ? S.controlIsIgnored() : (Semantic == Ignore);
  if (IsIgnored)
    return;

  // Step 2.
  llvm::BasicBlock *Violation = createBasicBlock("contract.violation");
  llvm::BasicBlock *End = createBasicBlock("contract.end");
  Builder.CreateCondBr(EvaluateExprAsBool(S.getCond()), End, Violation);

  // Step 3.
  EmitBlock(Violation);
  if (HasControl) {
    // r = T{}(comment, loc, cfg); trap iff r == violation_response::terminate.
    // The terminate value is read from the call's (enumeration) return type
    // rather than hard-coded.
    const Expr *ViolationCall = S.getViolationCall();
    assert(ViolationCall &&
           "explicit control type without a synthesized violation call");
    llvm::Value *Response = EmitScalarExpr(ViolationCall);

    const EnumType *ET = ViolationCall->getType()->getAs<EnumType>();
    assert(ET && "control operator() must return an enumeration");
    llvm::Value *TerminateVal = nullptr;
    for (const EnumConstantDecl *ECD : ET->getDecl()->enumerators()) {
      if (ECD->getName() == "terminate") {
        TerminateVal = llvm::ConstantInt::get(Response->getType(),
                                              ECD->getInitVal().getZExtValue());
        break;
      }
    }
    assert(TerminateVal && "violation_response has no terminate enumerator");

    llvm::BasicBlock *Trap = createBasicBlock("contract.trap");
    Builder.CreateCondBr(Builder.CreateICmpEQ(Response, TerminateVal), Trap,
                         End);
    EmitBlock(Trap);
    CreateTrap(*this);
  } else if (Semantic == QuickEnforce) {
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
