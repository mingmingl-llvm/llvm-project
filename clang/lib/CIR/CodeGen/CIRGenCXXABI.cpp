//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This provides an abstract class for C++ code generation. Concrete subclasses
// of this implement code generation for specific C++ ABIs.
//
//===----------------------------------------------------------------------===//

#include "CIRGenCXXABI.h"
#include "CIRGenFunction.h"

#include "clang/AST/Decl.h"
#include "clang/AST/GlobalDecl.h"

using namespace clang;
using namespace clang::CIRGen;

CIRGenCXXABI::~CIRGenCXXABI() {}

void CIRGenCXXABI::buildThisParam(CIRGenFunction &cgf,
                                  FunctionArgList &params) {
  const auto *md = cast<CXXMethodDecl>(cgf.curGD.getDecl());

  // FIXME: I'm not entirely sure I like using a fake decl just for code
  // generation. Maybe we can come up with a better way?
  auto *thisDecl =
      ImplicitParamDecl::Create(cgm.getASTContext(), nullptr, md->getLocation(),
                                &cgm.getASTContext().Idents.get("this"),
                                md->getThisType(), ImplicitParamKind::CXXThis);
  params.push_back(thisDecl);
  cgf.cxxabiThisDecl = thisDecl;

  // Classic codegen computes the alignment of thisDecl and saves it in
  // CodeGenFunction::CXXABIThisAlignment, but it is only used in emitTypeCheck
  // in CodeGenFunction::StartFunction().
  assert(!cir::MissingFeatures::cxxabiThisAlignment());
}

cir::GlobalLinkageKind CIRGenCXXABI::getCXXDestructorLinkage(
    GVALinkage linkage, const CXXDestructorDecl *dtor, CXXDtorType dt) const {
  // Delegate back to cgm by default.
  return cgm.getCIRLinkageForDeclarator(dtor, linkage,
                                        /*isConstantVariable=*/false);
}

mlir::Value CIRGenCXXABI::loadIncomingCXXThis(CIRGenFunction &cgf) {
  ImplicitParamDecl *vd = getThisDecl(cgf);
  Address addr = cgf.getAddrOfLocalVar(vd);
  return cgf.getBuilder().create<cir::LoadOp>(
      cgf.getLoc(vd->getLocation()), addr.getElementType(), addr.getPointer());
}

void CIRGenCXXABI::setCXXABIThisValue(CIRGenFunction &cgf,
                                      mlir::Value thisPtr) {
  /// Initialize the 'this' slot.
  assert(getThisDecl(cgf) && "no 'this' variable for function");
  cgf.cxxabiThisValue = thisPtr;
}
