//===- llvm/CodeGen/StaticDataSplitter.h -------------------------------*- C++
//-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_STATIC_DATA_SPLITTER_H
#define LLVM_CODEGEN_STATIC_DATA_SPLITTER_H

namespace llvm {
struct StaticDataSplitterOptions {
  bool SplitJumpTables = false;
  bool SplitConstantPool = false;
};
} // namespace llvm

#endif // LLVM_CODEGEN_STATIC_DATA_SPLITTER_H
