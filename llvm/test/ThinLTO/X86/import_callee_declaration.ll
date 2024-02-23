; RUN: rm -rf %t && split-file %s %t && cd %t

; RUN: opt -module-summary main.ll -o main.bc

; RUN: opt -module-summary lib.ll -o lib.bc
; RUN: llvm-lto -thinlto -o summary main.bc lib.bc

; At this point, update ComputeImportForModule to compute the list of declared
; functions. And pass it onto bitcode summary.

; RUN: llvm-dis main.bc -o - | FileCheck %s

;--- main.ll
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define i32 @main() {
  call void @small_func()
  call void @large_func()
  ret i32 0
}

declare void @small_func()

; large_func without attributes
declare void @large_func()

;--- lib.ll
source_filename = "lib.cc"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@calleeAddrs = global [2 x ptr] [ptr @large_indirect_callee, ptr @small_indirect_callee]

declare void @callee()

define void @large_indirect_callee() {
  call void @callee()
  call void @callee()
  call void @callee()
  call void @callee()
  call void @callee()
  ret void
}

define internal void @small_indirect_callee() {
  ret void
}

define void @small_func() {
entry:
  %0 = load ptr, ptr @calleeAddrs
  call void %0()
  %1 = load ptr, ptr getelementptr inbounds ([2 x ptr], ptr @calleeAddrs, i64 0, i64 1)
  call void %1()
  ret void
}

define void @large_func() #0 {
entry:
  call void @callee()
  call void @callee()
  call void @callee()
  call void @callee()
  call void @callee()
  ret void
}

attributes #0 = { nounwind norecurse }

; CHECK-NOT: main
