; RUN: opt < %s -passes=pgo-icall-prom -S | FileCheck %s --check-prefix=ICALL-PROM
; RUN: opt < %s -passes=pgo-icall-prom -enable-vtable-cmp -icp-vtable-cmp-inst-threshold=0 -icp-vtable-cmp-inst-last-candidate-threshold=1 -icp-vtable-cmp-total-inst-threshold=1 -S | FileCheck %s --check-prefix=ICALL-VTABLE-PROM

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@_ZTV5Base1 = constant { [4 x ptr] } { [4 x ptr] [ptr null, ptr null, ptr @_ZN5Base15func1Ei, ptr @_ZN5Base15func2Ev] }, !type !0
@_ZTV8Derived1 = constant { [4 x ptr] } { [4 x ptr] [ptr null, ptr null, ptr @_ZN8Derived15func1Ei, ptr @_ZN8Derived15func2Ev] }, !type !0, !type !1
@_ZTV8Derived2 =  constant { [4 x ptr], [4 x ptr] } { [4 x ptr] [ptr null, ptr null, ptr @_ZN5Base25func3Ev, ptr @_ZN8Derived25func2Ev], [4 x ptr] [ptr inttoptr (i64 -8 to ptr), ptr null, ptr @_ZN5Base15func1Ei, ptr @_ZThn8_N8Derived25func2Ev] }, !type !2, !type !3, !type !4
@_ZTV5Base2 = constant { [3 x ptr] } { [3 x ptr] [ptr null, ptr null, ptr @_ZN5Base25func3Ev] }, !type !3

define i32 @_Z4funcP5Base1(ptr %d) {
; ICALL-PROM-LABEL: define i32 @_Z4funcP5Base1(
; ICALL-PROM-SAME: ptr [[D:%.*]]) {
; ICALL-PROM-NEXT:  entry:
; ICALL-PROM-NEXT:    [[VTABLE:%.*]] = load ptr, ptr [[D]], align 8
; ICALL-PROM-NEXT:    [[TMP0:%.*]] = tail call i1 @llvm.type.test(ptr [[VTABLE]], metadata !"_ZTS5Base1")
; ICALL-PROM-NEXT:    tail call void @llvm.assume(i1 [[TMP0]])
; ICALL-PROM-NEXT:    [[VFN:%.*]] = getelementptr inbounds ptr, ptr [[VTABLE]], i64 1
; ICALL-PROM-NEXT:    [[TMP1:%.*]] = load ptr, ptr [[VFN]], align 8
; ICALL-PROM-NEXT:    [[TMP2:%.*]] = icmp eq ptr [[TMP1]], @_ZN8Derived15func2Ev
; ICALL-PROM-NEXT:    br i1 [[TMP2]], label [[IF_TRUE_DIRECT_TARG:%.*]], label [[IF_FALSE_ORIG_INDIRECT:%.*]], !prof [[PROF5:![0-9]+]]
; ICALL-PROM:       if.true.direct_targ:
; ICALL-PROM-NEXT:    [[TMP3:%.*]] = tail call i32 @_ZN8Derived15func2Ev(ptr [[D]])
; ICALL-PROM-NEXT:    br label [[IF_END_ICP:%.*]]
; ICALL-PROM:       if.false.orig_indirect:
; ICALL-PROM-NEXT:    [[TMP4:%.*]] = icmp eq ptr [[TMP1]], @_ZThn8_N8Derived25func2Ev
; ICALL-PROM-NEXT:    br i1 [[TMP4]], label [[IF_TRUE_DIRECT_TARG1:%.*]], label [[IF_FALSE_ORIG_INDIRECT2:%.*]], !prof [[PROF6:![0-9]+]]
; ICALL-PROM:       if.true.direct_targ1:
; ICALL-PROM-NEXT:    [[TMP5:%.*]] = tail call i32 @_ZThn8_N8Derived25func2Ev(ptr [[D]])
; ICALL-PROM-NEXT:    br label [[IF_END_ICP3:%.*]]
; ICALL-PROM:       if.false.orig_indirect2:
; ICALL-PROM-NEXT:    [[CALL:%.*]] = tail call i32 [[TMP1]](ptr [[D]])
; ICALL-PROM-NEXT:    br label [[IF_END_ICP3]]
; ICALL-PROM:       if.end.icp3:
; ICALL-PROM-NEXT:    [[TMP6:%.*]] = phi i32 [ [[CALL]], [[IF_FALSE_ORIG_INDIRECT2]] ], [ [[TMP5]], [[IF_TRUE_DIRECT_TARG1]] ]
; ICALL-PROM-NEXT:    br label [[IF_END_ICP]]
; ICALL-PROM:       if.end.icp:
; ICALL-PROM-NEXT:    [[TMP7:%.*]] = phi i32 [ [[TMP6]], [[IF_END_ICP3]] ], [ [[TMP3]], [[IF_TRUE_DIRECT_TARG]] ]
; ICALL-PROM-NEXT:    ret i32 [[TMP7]]
;
; ICALL-VTABLE-PROM-LABEL: define i32 @_Z4funcP5Base1(
; ICALL-VTABLE-PROM-SAME: ptr [[D:%.*]]) {
; ICALL-VTABLE-PROM-NEXT:  entry:
; ICALL-VTABLE-PROM-NEXT:    [[VTABLE:%.*]] = load ptr, ptr [[D]], align 8
; ICALL-VTABLE-PROM-NEXT:    [[TMP0:%.*]] = ptrtoint ptr [[VTABLE]] to i64
; ICALL-VTABLE-PROM-NEXT:    [[OFFSET_VAR:%.*]] = sub nuw i64 [[TMP0]], 16
; ICALL-VTABLE-PROM-NEXT:    [[TMP1:%.*]] = tail call i1 @llvm.type.test(ptr [[VTABLE]], metadata !"_ZTS5Base1")
; ICALL-VTABLE-PROM-NEXT:    tail call void @llvm.assume(i1 [[TMP1]])
; ICALL-VTABLE-PROM-NEXT:    [[TMP2:%.*]] = icmp eq i64 ptrtoint (ptr @_ZTV8Derived1 to i64), [[OFFSET_VAR]]
; ICALL-VTABLE-PROM-NEXT:    br i1 [[TMP2]], label [[IF_THEN_DIRECT_CALL:%.*]], label [[IF_ELSE_ORIG_INDIRECT:%.*]], !prof [[PROF5:![0-9]+]]
; ICALL-VTABLE-PROM:       if.then.direct_call:
; ICALL-VTABLE-PROM-NEXT:    [[TMP3:%.*]] = tail call i32 @_ZN8Derived15func2Ev(ptr [[D]])
; ICALL-VTABLE-PROM-NEXT:    br label [[IF_END_ICP:%.*]]
; ICALL-VTABLE-PROM:       if.else.orig_indirect:
; ICALL-VTABLE-PROM-NEXT:    [[OFFSET_VAR1:%.*]] = sub nuw i64 [[TMP0]], 48
; ICALL-VTABLE-PROM-NEXT:    [[TMP4:%.*]] = icmp eq i64 ptrtoint (ptr @_ZTV8Derived2 to i64), [[OFFSET_VAR1]]
; ICALL-VTABLE-PROM-NEXT:    br i1 [[TMP4]], label [[IF_THEN_DIRECT_CALL2:%.*]], label [[IF_ELSE_ORIG_INDIRECT3:%.*]], !prof [[PROF6:![0-9]+]]
; ICALL-VTABLE-PROM:       if.then.direct_call2:
; ICALL-VTABLE-PROM-NEXT:    [[TMP5:%.*]] = tail call i32 @_ZThn8_N8Derived25func2Ev(ptr [[D]])
; ICALL-VTABLE-PROM-NEXT:    br label [[IF_END_ICP4:%.*]]
; ICALL-VTABLE-PROM:       if.else.orig_indirect3:
; ICALL-VTABLE-PROM-NEXT:    [[VFN:%.*]] = getelementptr inbounds ptr, ptr [[VTABLE]], i64 1
; ICALL-VTABLE-PROM-NEXT:    [[TMP6:%.*]] = load ptr, ptr [[VFN]], align 8
; ICALL-VTABLE-PROM-NEXT:    [[CALL:%.*]] = tail call i32 [[TMP6]](ptr [[D]])
; ICALL-VTABLE-PROM-NEXT:    br label [[IF_END_ICP4]]
; ICALL-VTABLE-PROM:       if.end.icp4:
; ICALL-VTABLE-PROM-NEXT:    [[TMP7:%.*]] = phi i32 [ [[CALL]], [[IF_ELSE_ORIG_INDIRECT3]] ], [ [[TMP5]], [[IF_THEN_DIRECT_CALL2]] ]
; ICALL-VTABLE-PROM-NEXT:    br label [[IF_END_ICP]]
; ICALL-VTABLE-PROM:       if.end.icp:
; ICALL-VTABLE-PROM-NEXT:    [[TMP8:%.*]] = phi i32 [ [[TMP7]], [[IF_END_ICP4]] ], [ [[TMP3]], [[IF_THEN_DIRECT_CALL]] ]
; ICALL-VTABLE-PROM-NEXT:    ret i32 [[TMP8]]
;
entry:
  %vtable = load ptr, ptr %d, !prof !5
  %0 = tail call i1 @llvm.type.test(ptr %vtable, metadata !"_ZTS5Base1")
  tail call void @llvm.assume(i1 %0)
  %vfn = getelementptr inbounds ptr, ptr %vtable, i64 1
  %1 = load ptr, ptr %vfn, align 8
  %call = tail call i32 %1(ptr %d), !prof !6
  ret i32 %call
}

declare i1 @llvm.type.test(ptr, metadata)

declare void @llvm.assume(i1 noundef)
declare i32 @_ZN8Derived15func1Ei(ptr, i32)
declare i32 @_ZN5Base15func1Ei(ptr, i32)
declare i32 @_ZN5Base15func2Ev(ptr)
declare i32 @_ZN8Derived25func2Ev(ptr)
declare i32 @_ZN5Base25func3Ev(ptr)

define i32 @_ZThn8_N8Derived25func2Ev(ptr %this) {
  ret i32 1
}

define i32 @_ZN8Derived15func2Ev(ptr %this) {
  ret i32 2
}

!0 = !{i64 16, !"_ZTS5Base1"}
!1 = !{i64 16, !"_ZTS8Derived1"}
!2 = !{i64 48, !"_ZTS5Base1"}
!3 = !{i64 16, !"_ZTS5Base2"}
!4 = !{i64 16, !"_ZTS8Derived2"}
!5 = !{!"VP", i32 2, i64 1600, i64 -9064381665493407289, i64 800, i64 5035968517245772950, i64 800}
!6 = !{!"VP", i32 0, i64 1600, i64 8283424862230071372, i64 800, i64 -7571493466221013720, i64 800}
