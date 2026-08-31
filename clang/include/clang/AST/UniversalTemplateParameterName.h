//===- UniversalTemplateParameterName.h - Universal Parameters --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines UniversalTemplateParameterName, which names a use of a
//  universal template parameter.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_UNIVERSALTEMPLATEPARAMETERNAME_H
#define LLVM_CLANG_AST_UNIVERSALTEMPLATEPARAMETERNAME_H

#include "clang/AST/DeclarationName.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/ADT/FoldingSet.h"

namespace clang {

class ASTContext;
class StreamingDiagnostic;
struct PrintingPolicy;
class UniversalTemplateParmDecl;

/// Names a universal template parameter where it is used, for instance as a
/// template argument:
///
/// \code
/// template <universal template U> struct S { using type = T<U>; };
/// \endcode
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

  void Profile(llvm::FoldingSetNodeID &ID) const { Profile(ID, Param); }

  static void Profile(llvm::FoldingSetNodeID &ID,
                      const UniversalTemplateParmDecl *Param);

  void print(raw_ostream &OS, const PrintingPolicy &Policy) const;

  void dump(raw_ostream &OS) const;
  void dump() const;
};

/// Insertion operator for diagnostics. This allows sending
/// UniversalTemplateParameterNames into a diagnostic with <<.
const StreamingDiagnostic &operator<<(const StreamingDiagnostic &DB,
                                      const UniversalTemplateParameterName *N);

} // namespace clang

#endif // LLVM_CLANG_AST_UNIVERSALTEMPLATEPARAMETERNAME_H
