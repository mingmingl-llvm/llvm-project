
; RUN: llvm-profdata merge %S/Inputs/select1.proftext -o %t.profdata
; RUN: opt < %s -passes=pgo-instr-use -pgo-test-profile-file=%t.profdata -pgo-instr-select=true -S | FileCheck %s --check-prefix=USE
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define i32 @test_br_2(i32 %i) {
entry:
  %cmp = icmp sgt i32 %i, 0
  br i1 %cmp, label %if.then, label %if.else, !prof !0

; USE: br i1 %cmp, label %if.then, label %if.else, !prof ![[USER_ANNOTATION:[0-9]+]]
; ![[USER_ANNOTATION]] = !{!"branch_weights", i32 1, i32 2}
if.then:
  %add = add nsw i32 %i, 2
  %s = select i1 %cmp, i32 %add, i32 0

  br label %if.end

if.else:
  %sub = sub nsw i32 %i, 2
  br label %if.end

if.end:
  %retv = phi i32 [ %add, %if.then ], [ %sub, %if.else ]
  ret i32 %retv
}

!0 = !{!"branch_weights", i32 1, i32 2}
