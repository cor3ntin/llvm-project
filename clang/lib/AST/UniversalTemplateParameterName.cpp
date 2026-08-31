//===- UniversalTemplateParameterName.cpp - Universal Parameters ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements UniversalTemplateParameterName.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/UniversalTemplateParameterName.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/Basic/Diagnostic.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;

UniversalTemplateParameterName::UniversalTemplateParameterName(
    SourceLocation Loc, DeclarationNameInfo Name,
    UniversalTemplateParmDecl *Param)
    : Loc(Loc), Name(Name), Param(Param) {}

bool UniversalTemplateParameterName::containsUnexpandedParameterPack() const {
  return Param->isParameterPack();
}

void UniversalTemplateParameterName::Profile(
    llvm::FoldingSetNodeID &ID, const UniversalTemplateParmDecl *Param) {
  ID.AddPointer(Param);
}

void UniversalTemplateParameterName::print(
    raw_ostream &OS, const PrintingPolicy &Policy) const {
  Name.printName(OS, Policy);
}

void UniversalTemplateParameterName::dump(raw_ostream &OS) const {
  LangOptions LO;
  LO.CPlusPlus = true;
  LO.Bool = true;
  print(OS, PrintingPolicy(LO));
}

LLVM_DUMP_METHOD void UniversalTemplateParameterName::dump() const {
  dump(llvm::errs());
}

const StreamingDiagnostic &
clang::operator<<(const StreamingDiagnostic &DB,
                  const UniversalTemplateParameterName *N) {
  std::string NameStr;
  llvm::raw_string_ostream OS(NameStr);
  LangOptions LO;
  LO.CPlusPlus = true;
  LO.Bool = true;
  OS << '\'';
  N->print(OS, PrintingPolicy(LO));
  OS << '\'';
  return DB << NameStr;
}
