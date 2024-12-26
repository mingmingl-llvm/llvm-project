; https://gcc.godbolt.org/z/1nMM7MMEd
;   -> (simplified) https://gcc.godbolt.org/z/Pxzsds336
;   -> (simplified IR) https://gcc.godbolt.org/z/WvWj8erhr 

; RUN: opt -passes=typerefine < %s -S -o - | FileCheck %s

; CHECK: _ZN7Derived5PaintEii 

; ModuleID = '/app/example.ll'
source_filename = "/app/example.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i1 @llvm.type.test(ptr, metadata) 

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: write)
declare void @llvm.assume(i1 ) 

; Function Attrs: mustprogress uwtable
define dso_local void @_ZN7Derived5PaintEii(ptr %this, i32 noundef %a, i32 noundef %depth) align 2 {
  %vtable = load ptr, ptr %this, align 8
  %res = tail call i1 @llvm.type.test(ptr %vtable, metadata !"_ZTS7Derived")
  tail call void @llvm.assume(i1 %res)
  %1 = load ptr, ptr %vtable, align 8
  tail call void %1(ptr %this)
  %vtable.i = load ptr, ptr %this, align 8
  %2 = tail call i1 @llvm.type.test(ptr %vtable.i, metadata !"_ZTS4Base")
  tail call void @llvm.assume(i1 %2)
  %3 = load ptr, ptr %vtable.i, align 8
  tail call void %3(ptr %this)
  ret void
}
