//===- TemplateName.h - C++ Template Name Representation --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines the TemplateName interface and subclasses.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_UNIVERSALTEMPLATEPARAMETERNAME_H
#define LLVM_CLANG_AST_UNIVERSALTEMPLATEPARAMETERNAME_H

#include "clang/AST/DeclarationName.h"
#include "clang/AST/NestedNameSpecifier.h"
#include "clang/Basic/SourceLocation.h"

namespace clang {

class UniversalTemplateParmDecl;

class UniversalTemplateParameterName {

  SourceLocation Loc;
  DeclarationNameInfo Name;
  UniversalTemplateParmDecl *Param;

public:
  UniversalTemplateParameterName(SourceLocation Loc, DeclarationNameInfo Name,
                                 UniversalTemplateParmDecl *Param);

  SourceLocation getLocation() const { return Loc; }

  UniversalTemplateParmDecl *getDecl() const { return Param; }

  DeclarationNameInfo getNameInfo() const { return Name; }

  bool containsUnexpandedParameterPack() const;

  void Profile(const ASTContext &Context, llvm::FoldingSetNodeID &ID) const {
    Profile(ID, Name, Param, Context);
  }

  static void Profile(llvm::FoldingSetNodeID &ID, DeclarationNameInfo Name,
                      const UniversalTemplateParmDecl *Decl,
                      const ASTContext &Context);

  void print(raw_ostream &OS, const PrintingPolicy &Policy) const;

  void dump(raw_ostream &OS) const;
  void dump() const;
};

/// Insertion operator for diagnostics.  This allows sending TemplateName's
/// into a diagnostic with <<.
const StreamingDiagnostic &operator<<(const StreamingDiagnostic &DB,
                                      UniversalTemplateParameterName N);

} // namespace clang

#endif // LLVM_CLANG_AST_UNIVERSALTEMPLATEPARAMETERNAME_H
