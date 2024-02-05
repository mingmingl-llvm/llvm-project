; RUN: not llvm-as %s -o  /dev/null 2>&1 | FileCheck %s

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@_ZTV4Base = available_externally unnamed_addr constant { [3 x ptr] } { [3 x ptr] [ptr null, ptr null, ptr @_ZN4Base4funcEv] }, !type !0, !type !1
@_ZTV7Derived = available_externally unnamed_addr constant { [3 x ptr] } { [3 x ptr] [ptr null, ptr null, ptr @_ZN7Derived4funcEv] }, !type !0, !type !1, !type !2, !type !3, !type !4

declare i32 @_ZN4Base4funcEv(ptr)
declare i32 @_ZN7Derived4funcEv(ptr)

!0 = !{i64 16, !"_ZTS4Base"}
!1 = !{i64 16, !"_ZTSM4BaseFivE.virtual"}
!2 = !{i64 16, !"_ZTS7Derived"}
!3 = !{i64 16, !"_ZTSM7DerivedFivE.virtual"}
!4 = !{i64 16, !"_ZTS4Base"}

; CHECK: Global variable has type metadatas with duplicate type ids
