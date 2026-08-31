//===--- ASTConcept.cpp - Concepts Related AST Data Structures --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief This file defines AST data structures related to concepts.
///
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTConcept.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ExprConcepts.h"
#include "clang/AST/NestedNameSpecifier.h"
#include "clang/AST/PrettyPrinter.h"
#include "llvm/ADT/StringExtras.h"

using namespace clang;

static void
CreateUnsatisfiedConstraintRecord(const ASTContext &C,
                                  const UnsatisfiedConstraintRecord &Detail,
                                  UnsatisfiedConstraintRecord *TrailingObject) {
  if (Detail.isNull())
    new (TrailingObject) UnsatisfiedConstraintRecord(nullptr);
  else if (const auto *E = llvm::dyn_cast<const Expr *>(Detail))
    new (TrailingObject) UnsatisfiedConstraintRecord(E);
  else if (const auto *Concept =
               llvm::dyn_cast<const ConceptReference *>(Detail))
    new (TrailingObject) UnsatisfiedConstraintRecord(Concept);
  else {
    auto &SubstitutionDiagnostic =
        *cast<const clang::ConstraintSubstitutionDiagnostic *>(Detail);
    StringRef Message = C.backupStr(SubstitutionDiagnostic.second);
    auto *NewSubstDiag = new (C) clang::ConstraintSubstitutionDiagnostic(
        SubstitutionDiagnostic.first, Message);
    new (TrailingObject) UnsatisfiedConstraintRecord(NewSubstDiag);
  }
}

ASTConstraintSatisfaction::ASTConstraintSatisfaction(
    const ASTContext &C, const ConstraintSatisfaction &Satisfaction)
    : NumRecords{Satisfaction.Details.size()},
      IsSatisfied{Satisfaction.IsSatisfied}, ContainsErrors{
                                                 Satisfaction.ContainsErrors} {
  for (unsigned I = 0; I < NumRecords; ++I)
    CreateUnsatisfiedConstraintRecord(C, Satisfaction.Details[I],
                                      getTrailingObjects() + I);
}

ASTConstraintSatisfaction::ASTConstraintSatisfaction(
    const ASTContext &C, const ASTConstraintSatisfaction &Satisfaction)
    : NumRecords{Satisfaction.NumRecords},
      IsSatisfied{Satisfaction.IsSatisfied},
      ContainsErrors{Satisfaction.ContainsErrors} {
  for (unsigned I = 0; I < NumRecords; ++I)
    CreateUnsatisfiedConstraintRecord(C, *(Satisfaction.begin() + I),
                                      getTrailingObjects() + I);
}

ASTConstraintSatisfaction *
ASTConstraintSatisfaction::Create(const ASTContext &C,
                                  const ConstraintSatisfaction &Satisfaction) {
  std::size_t size =
      totalSizeToAlloc<UnsatisfiedConstraintRecord>(
          Satisfaction.Details.size());
  void *Mem = C.Allocate(size, alignof(ASTConstraintSatisfaction));
  return new (Mem) ASTConstraintSatisfaction(C, Satisfaction);
}

ASTConstraintSatisfaction *ASTConstraintSatisfaction::Rebuild(
    const ASTContext &C, const ASTConstraintSatisfaction &Satisfaction) {
  std::size_t size =
      totalSizeToAlloc<UnsatisfiedConstraintRecord>(Satisfaction.NumRecords);
  void *Mem = C.Allocate(size, alignof(ASTConstraintSatisfaction));
  return new (Mem) ASTConstraintSatisfaction(C, Satisfaction);
}

void ConstraintSatisfaction::Profile(llvm::FoldingSetNodeID &ID,
                                     const ASTContext &C,
                                     const NamedDecl *ConstraintOwner,
                                     ArrayRef<TemplateArgument> TemplateArgs) {
  ID.AddPointer(ConstraintOwner);
  ID.AddInteger(TemplateArgs.size());
  for (auto &Arg : TemplateArgs)
    Arg.Profile(ID, C);
}

ConceptReference *
ConceptReference::Create(const ASTContext &C, NestedNameSpecifierLoc NNS,
                         SourceLocation TemplateKWLoc,
                         DeclarationNameInfo ConceptNameInfo,
                         NamedDecl *FoundDecl, TemplateName NamedConcept,
                         const ASTTemplateArgumentListInfo *ArgsAsWritten) {

  assert(NamedConcept.isConceptName() &&
         "concept reference does not name a concept");

  return new (C) ConceptReference(NNS, TemplateKWLoc, ConceptNameInfo,
                                  FoundDecl, NamedConcept, ArgsAsWritten);
}

SourceLocation ConceptReference::getBeginLoc() const {
  // Note that if the qualifier is null the template KW must also be null.
  if (auto QualifierLoc = getNestedNameSpecifierLoc())
    return QualifierLoc.getBeginLoc();
  return getConceptNameInfo().getBeginLoc();
}

void ConceptReference::print(llvm::raw_ostream &OS,
                             const PrintingPolicy &Policy) const {
  NestedNameSpec.getNestedNameSpecifier().print(OS, Policy);
  NamedConcept.print(OS, Policy, TemplateName::Qualified::None);
  if (hasExplicitTemplateArgs()) {
    OS << "<";
    llvm::ListSeparator Sep(", ");
    // FIXME: Find corresponding parameter for argument
    for (auto &ArgLoc : ArgsAsWritten->arguments()) {
      OS << Sep;
      ArgLoc.getArgument().print(Policy, OS, /*IncludeType*/ false);
    }
    OS << ">";
  }
}

const StreamingDiagnostic &clang::operator<<(const StreamingDiagnostic &DB,
                                             const ConceptReference *C) {
  std::string NameStr;
  llvm::raw_string_ostream OS(NameStr);
  LangOptions LO;
  LO.CPlusPlus = true;
  LO.Bool = true;
  OS << '\'';
  C->print(OS, PrintingPolicy(LO));
  OS << '\'';
  return DB << NameStr;
}

PartiallyAppliedConcept *PartiallyAppliedConcept::Create(
    const ASTContext &C, NestedNameSpecifierLoc NNS,
    DeclarationNameInfo ConceptNameInfo, SourceLocation ConceptKWLoc,
    NamedDecl *FoundDecl, TemplateName NamedConcept,
    const TemplateArgumentListInfo &TemplateArgs) {
  assert(NamedConcept.isConceptName() &&
         "partially applied concept does not name a concept");

  unsigned NumArgs = TemplateArgs.size();
  auto *BoundArgs = new (C) TemplateArgument[NumArgs];
  for (unsigned I = 0; I != NumArgs; ++I)
    BoundArgs[I] = TemplateArgs[I].getArgument();

  return new (C) PartiallyAppliedConcept(
      NNS, ConceptNameInfo, ConceptKWLoc, FoundDecl, NamedConcept,
      ASTTemplateArgumentListInfo::Create(C, TemplateArgs),
      ArrayRef(BoundArgs, NumArgs));
}

PartiallyAppliedConcept *PartiallyAppliedConcept::CreateWithTrivialLocs(
    const ASTContext &C, NestedNameSpecifierLoc NNS,
    DeclarationNameInfo ConceptNameInfo, SourceLocation ConceptKWLoc,
    NamedDecl *FoundDecl, TemplateName NamedConcept,
    ArrayRef<TemplateArgument> BoundArgs) {
  SourceLocation Loc = ConceptNameInfo.getLoc();
  TemplateArgumentListInfo ArgsAsWritten(Loc, Loc);
  for (const TemplateArgument &Arg : BoundArgs) {
    TemplateArgumentLocInfo LocInfo;
    switch (Arg.getKind()) {
    case TemplateArgument::Type:
      LocInfo = TemplateArgumentLocInfo(
          C.getTrivialTypeSourceInfo(Arg.getAsType(), Loc));
      break;
    case TemplateArgument::Expression:
      LocInfo = TemplateArgumentLocInfo(Arg.getAsExpr());
      break;
    case TemplateArgument::Template:
    case TemplateArgument::TemplateExpansion:
    case TemplateArgument::Concept:
      LocInfo = TemplateArgumentLocInfo(
          const_cast<ASTContext &>(C), /*TemplateKWLoc=*/SourceLocation(),
          NestedNameSpecifierLoc(), Loc, SourceLocation());
      break;
    default:
      LocInfo = TemplateArgumentLocInfo(const_cast<ASTContext &>(C), Loc);
      break;
    }
    ArgsAsWritten.addArgument(TemplateArgumentLoc(Arg, LocInfo));
  }

  auto *StoredArgs = new (C) TemplateArgument[BoundArgs.size()];
  std::copy(BoundArgs.begin(), BoundArgs.end(), StoredArgs);

  return new (C) PartiallyAppliedConcept(
      NNS, ConceptNameInfo, ConceptKWLoc, FoundDecl, NamedConcept,
      ASTTemplateArgumentListInfo::Create(C, ArgsAsWritten),
      ArrayRef(StoredArgs, BoundArgs.size()));
}

TemplateArgumentDependence PartiallyAppliedConcept::getDependence() const {
  auto Deps = TemplateArgumentDependence::None;

  // Naming the concept through a concept template parameter means we cannot
  // know which concept is being applied until instantiation.
  if (getNamedConcept().getAsTemplateTemplateParmDecl())
    Deps |= TemplateArgumentDependence::DependentInstantiation;

  constexpr auto InterestingDeps = TemplateArgumentDependence::Instantiation |
                                   TemplateArgumentDependence::UnexpandedPack;
  for (const TemplateArgumentLoc &ArgLoc :
       getTemplateArgsAsWritten()->arguments()) {
    Deps |= ArgLoc.getArgument().getDependence() & InterestingDeps;
    if ((Deps & InterestingDeps) == InterestingDeps)
      break;
  }
  return Deps;
}

void PartiallyAppliedConcept::Profile(
    llvm::FoldingSetNodeID &ID, const ASTContext &C, TemplateName NamedConcept,
    const ASTTemplateArgumentListInfo *ArgsAsWritten) {
  ID.AddPointer(NamedConcept.getAsVoidPointer());
  ID.AddInteger(ArgsAsWritten->getNumTemplateArgs());
  for (const TemplateArgumentLoc &Arg : ArgsAsWritten->arguments())
    Arg.getArgument().Profile(ID, C);
}

const StreamingDiagnostic &clang::operator<<(const StreamingDiagnostic &DB,
                                             const PartiallyAppliedConcept *C) {
  std::string NameStr;
  llvm::raw_string_ostream OS(NameStr);
  LangOptions LO;
  LO.CPlusPlus = true;
  LO.Bool = true;
  OS << "'concept ";
  C->print(OS, PrintingPolicy(LO));
  OS << '\'';
  return DB << NameStr;
}

concepts::ExprRequirement::ExprRequirement(
    Expr *E, bool IsSimple, SourceLocation NoexceptLoc,
    ReturnTypeRequirement Req, SatisfactionStatus Status,
    ConceptSpecializationExpr *SubstitutedConstraintExpr)
    : Requirement(IsSimple ? RK_Simple : RK_Compound, Status == SS_Dependent,
                  Status == SS_Dependent &&
                      (E->containsUnexpandedParameterPack() ||
                       Req.containsUnexpandedParameterPack()),
                  Status == SS_Satisfied),
      Value(E), NoexceptLoc(NoexceptLoc), TypeReq(Req),
      SubstitutedConstraintExpr(SubstitutedConstraintExpr), Status(Status) {
  assert((!IsSimple || (Req.isEmpty() && NoexceptLoc.isInvalid())) &&
         "Simple requirement must not have a return type requirement or a "
         "noexcept specification");
  assert((Status > SS_TypeRequirementSubstitutionFailure &&
          Req.isTypeConstraint()) == (SubstitutedConstraintExpr != nullptr));
}

concepts::ExprRequirement::ExprRequirement(
    SubstitutionDiagnostic *ExprSubstDiag, bool IsSimple,
    SourceLocation NoexceptLoc, ReturnTypeRequirement Req)
    : Requirement(IsSimple ? RK_Simple : RK_Compound, Req.isDependent(),
                  Req.containsUnexpandedParameterPack(), /*IsSatisfied=*/false),
      Value(ExprSubstDiag), NoexceptLoc(NoexceptLoc), TypeReq(Req),
      Status(SS_ExprSubstitutionFailure) {
  assert((!IsSimple || (Req.isEmpty() && NoexceptLoc.isInvalid())) &&
         "Simple requirement must not have a return type requirement or a "
         "noexcept specification");
}

concepts::ExprRequirement::ReturnTypeRequirement::ReturnTypeRequirement(
    TemplateParameterList *TPL)
    : TypeConstraintInfo(TPL, false) {
  assert(TPL->size() == 1);
  const TypeConstraint *TC =
      cast<TemplateTypeParmDecl>(TPL->getParam(0))->getTypeConstraint();
  assert(TC &&
         "TPL must have a template type parameter with a type constraint");
  auto *Constraint =
      cast<ConceptSpecializationExpr>(TC->getImmediatelyDeclaredConstraint());
  bool Dependent =
      Constraint->getTemplateArgsAsWritten() &&
      TemplateSpecializationType::anyInstantiationDependentTemplateArguments(
          Constraint->getTemplateArgsAsWritten()->arguments().drop_front(1));
  TypeConstraintInfo.setInt(Dependent ? true : false);
}

concepts::ExprRequirement::ReturnTypeRequirement::ReturnTypeRequirement(
    TemplateParameterList *TPL, bool IsDependent)
    : TypeConstraintInfo(TPL, IsDependent) {}

concepts::TypeRequirement::TypeRequirement(TypeSourceInfo *T)
    : Requirement(RK_Type, T->getType()->isInstantiationDependentType(),
                  T->getType()->containsUnexpandedParameterPack(),
                  // We reach this ctor with either dependent types (in which
                  // IsSatisfied doesn't matter) or with non-dependent type in
                  // which the existence of the type indicates satisfaction.
                  /*IsSatisfied=*/true),
      Value(T),
      Status(T->getType()->isInstantiationDependentType() ? SS_Dependent
                                                          : SS_Satisfied) {}
