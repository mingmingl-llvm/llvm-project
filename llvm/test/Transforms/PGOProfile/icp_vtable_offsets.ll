; RUN: opt < %s -passes=pgo-icall-prom -S | FileCheck %s --check-prefix=ICALL-PROM
; RUN: opt < %s -passes=pgo-icall-prom -enable-vtable-cmp -S | FileCheck %s --check-prefix=ICALL-PROM
; RUN: opt < %s -passes=pgo-icall-prom -enable-vtable-cmp -S -icp-vtable-cmp-inst-threshold=5 -icp-vtable-cmp-inst-last-candidate-threshold=5 -icp-vtable-cmp-total-inst-threshold=5 | FileCheck %s --check-prefix=ICALL-VTABLE

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@_ZTV5Base1 = constant { [4 x ptr] } { [4 x ptr] [ptr null, ptr null, ptr @_ZN5Base15func0Ev, ptr @_ZN5Base15func1Ev] }, !type !0
@_ZTV8Derived1 = constant { [4 x ptr], [3 x ptr] } { [4 x ptr] [ptr inttoptr (i64 -8 to ptr), ptr null, ptr @_ZN5Base15func0Ev, ptr @_ZN5Base15func1Ev], [3 x ptr] [ptr null, ptr null, ptr @_ZN5Base25func2Ev] }, !type !1, !type !2, !type !3
@_ZTV5Base2 = constant { [3 x ptr] } { [3 x ptr] [ptr null, ptr null, ptr @_ZN5Base25func2Ev] }, !type !2
@_ZTV8Derived2 = constant { [3 x ptr], [3 x ptr], [4 x ptr] } { [3 x ptr] [ptr null, ptr null, ptr @_ZN5Base35func3Ev], [3 x ptr] [ptr inttoptr (i64 -8 to ptr), ptr null, ptr @_ZN5Base25func2Ev], [4 x ptr] [ptr inttoptr (i64 -16 to ptr), ptr null, ptr @_ZN5Base15func0Ev, ptr @_ZN5Base15func1Ev] }, !type !4, !type !5, !type !6, !type !7
@_ZTV5Base3 = constant { [3 x ptr] } { [3 x ptr] [ptr null, ptr null, ptr @_ZN5Base35func3Ev] }, !type !6

; Indirect call has one function candidate. The vtable profiles show the function
; might come from three vtables, and these three vtables have two different offsets.
define i32 @test_one_function_two_offsets_three_vtables(ptr %d) {
; ICALL-PROM-LABEL: define i32 @test_one_function_two_offsets_three_vtables(
; ICALL-PROM-SAME: ptr [[D:%.*]]) {
; ICALL-PROM-NEXT:  entry:
; ICALL-PROM-NEXT:    [[VTABLE:%.*]] = load ptr, ptr [[D]], align 8
; ICALL-PROM-NEXT:    [[TMP0:%.*]] = tail call i1 @llvm.type.test(ptr [[VTABLE]], metadata !"_ZTS5Base1")
; ICALL-PROM-NEXT:    tail call void @llvm.assume(i1 [[TMP0]])
; ICALL-PROM-NEXT:    [[VFN:%.*]] = getelementptr inbounds ptr, ptr [[VTABLE]], i64 1
; ICALL-PROM-NEXT:    [[TMP1:%.*]] = load ptr, ptr [[VFN]], align 8
; ICALL-PROM-NEXT:    [[TMP2:%.*]] = icmp eq ptr [[TMP1]], @_ZN5Base15func1Ev
; ICALL-PROM-NEXT:    br i1 [[TMP2]], label [[IF_TRUE_DIRECT_TARG:%.*]], label [[IF_FALSE_ORIG_INDIRECT:%.*]], !prof [[PROF7:![0-9]+]]
; ICALL-PROM:       if.true.direct_targ:
; ICALL-PROM-NEXT:    [[TMP3:%.*]] = tail call i32 @_ZN5Base15func1Ev(ptr [[D]])
; ICALL-PROM-NEXT:    br label [[IF_END_ICP:%.*]]
; ICALL-PROM:       if.false.orig_indirect:
; ICALL-PROM-NEXT:    [[CALL:%.*]] = tail call i32 [[TMP1]](ptr [[D]])
; ICALL-PROM-NEXT:    br label [[IF_END_ICP]]
; ICALL-PROM:       if.end.icp:
; ICALL-PROM-NEXT:    [[TMP4:%.*]] = phi i32 [ [[CALL]], [[IF_FALSE_ORIG_INDIRECT]] ], [ [[TMP3]], [[IF_TRUE_DIRECT_TARG]] ]
; ICALL-PROM-NEXT:    ret i32 [[TMP4]]
;
; ICALL-VTABLE-LABEL: define i32 @test_one_function_two_offsets_three_vtables(
; ICALL-VTABLE-SAME: ptr [[D:%.*]]) {
; ICALL-VTABLE-NEXT:  entry:
; ICALL-VTABLE-NEXT:    [[VTABLE:%.*]] = load ptr, ptr [[D]], align 8
; ICALL-VTABLE-NEXT:    [[TMP0:%.*]] = ptrtoint ptr [[VTABLE]] to i64
; ICALL-VTABLE-NEXT:    [[OFFSET_VAR:%.*]] = sub nuw i64 [[TMP0]], 16
; ICALL-VTABLE-NEXT:    [[TMP1:%.*]] = tail call i1 @llvm.type.test(ptr [[VTABLE]], metadata !"_ZTS5Base1")
; ICALL-VTABLE-NEXT:    tail call void @llvm.assume(i1 [[TMP1]])
; ICALL-VTABLE-NEXT:    [[OFFSET_VAR1:%.*]] = sub nuw i64 [[TMP0]], 64
; ICALL-VTABLE-NEXT:    [[TMP2:%.*]] = icmp eq i64 ptrtoint (ptr @_ZTV8Derived1 to i64), [[OFFSET_VAR]]
; ICALL-VTABLE-NEXT:    [[TMP3:%.*]] = icmp eq i64 ptrtoint (ptr @_ZTV8Derived2 to i64), [[OFFSET_VAR1]]
; ICALL-VTABLE-NEXT:    [[TMP4:%.*]] = icmp eq i64 ptrtoint (ptr @_ZTV5Base1 to i64), [[OFFSET_VAR]]
; ICALL-VTABLE-NEXT:    [[ICMP_OR:%.*]] = or i1 [[TMP2]], [[TMP3]]
; ICALL-VTABLE-NEXT:    [[ICMP_OR2:%.*]] = or i1 [[ICMP_OR]], [[TMP4]]
; ICALL-VTABLE-NEXT:    br i1 [[ICMP_OR2]], label [[IF_THEN_DIRECT_CALL:%.*]], label [[IF_ELSE_ORIG_INDIRECT:%.*]], !prof [[PROF7:![0-9]+]]
; ICALL-VTABLE:       if.then.direct_call:
; ICALL-VTABLE-NEXT:    [[TMP5:%.*]] = tail call i32 @_ZN5Base15func1Ev(ptr [[D]])
; ICALL-VTABLE-NEXT:    br label [[IF_END_ICP:%.*]]
; ICALL-VTABLE:       if.else.orig_indirect:
; ICALL-VTABLE-NEXT:    [[VFN:%.*]] = getelementptr inbounds ptr, ptr [[VTABLE]], i64 1
; ICALL-VTABLE-NEXT:    [[TMP6:%.*]] = load ptr, ptr [[VFN]], align 8
; ICALL-VTABLE-NEXT:    [[CALL:%.*]] = tail call i32 [[TMP6]](ptr [[D]])
; ICALL-VTABLE-NEXT:    br label [[IF_END_ICP]]
; ICALL-VTABLE:       if.end.icp:
; ICALL-VTABLE-NEXT:    [[TMP7:%.*]] = phi i32 [ [[CALL]], [[IF_ELSE_ORIG_INDIRECT]] ], [ [[TMP5]], [[IF_THEN_DIRECT_CALL]] ]
; ICALL-VTABLE-NEXT:    ret i32 [[TMP7]]
;
entry:
  %vtable = load ptr, ptr %d, !prof !8
  %0 = tail call i1 @llvm.type.test(ptr %vtable, metadata !"_ZTS5Base1")
  tail call void @llvm.assume(i1 %0)
  %vfn = getelementptr inbounds ptr, ptr %vtable, i64 1
  %1 = load ptr, ptr %vfn
  %call = tail call i32 %1(ptr %d), !prof !9
  ret i32 %call
}

define i32 @_ZN5Base15func1Ev(ptr %this) {
entry:
  ret i32 2
}


declare i1 @llvm.type.test(ptr, metadata)
declare void @llvm.assume(i1)
declare i32 @_ZN5Base25func2Ev(ptr)
declare i32 @_ZN5Base15func0Ev(ptr)
declare void @_ZN5Base35func3Ev(ptr)

!0 = !{i64 16, !"_ZTS5Base1"}
!1 = !{i64 16, !"_ZTS5Base1"}
!2 = !{i64 48, !"_ZTS5Base2"}
!3 = !{i64 16, !"_ZTS8Derived1"}
!4 = !{i64 64, !"_ZTS5Base1"}
!5 = !{i64 40, !"_ZTS5Base2"}
!6 = !{i64 16, !"_ZTS5Base3"}
!7 = !{i64 16, !"_ZTS8Derived2"}
!8 = !{!"VP", i32 2, i64 1600, i64 -9064381665493407289, i64 800, i64 5035968517245772950, i64 500, i64 3215870116411581797, i64 300}
!9 = !{!"VP", i32 0, i64 1600, i64 6804820478065511155, i64 1600}
