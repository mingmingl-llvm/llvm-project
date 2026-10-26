; Tests that SampleProfileLoader's early function-based ICP guards are
; upgraded to vtable comparisons (with function-pointer load sinking) by
; the canonical ICP pass when vtable comparison is profitable, or kept as
; function comparisons (with !samplepgo.icp.count stripped and vtable profile
; counts updated) when vtable comparison is not profitable.
;
; RUN: opt < %s -passes='sample-profile' -sample-profile-file=%S/Inputs/icp-vtable-cmp.prof -enable-vtable-profile-use -S | FileCheck %s --check-prefix=SAMPLE-ONLY
; RUN: opt < %s -passes='sample-profile,pgo-icall-prom' -sample-profile-file=%S/Inputs/icp-vtable-cmp.prof -pass-remarks=pgo-icall-prom -enable-vtable-profile-use -icp-max-num-vtable-last-candidate=2 -S 2>&1 | FileCheck %s --check-prefixes=VTABLE-COMMON,VTABLE-CMP-EARLY,VTABLE-CMP
; RUN: opt < %s -passes='sample-profile,pgo-icall-prom' -sample-profile-file=%S/Inputs/icp-vtable-cmp.prof -pass-remarks=pgo-icall-prom -enable-vtable-profile-use -icp-max-num-vtable-last-candidate=1 -S 2>&1 | FileCheck %s --check-prefixes=VTABLE-COMMON,VTABLE-CMP-EARLY,FUNC-CMP
; RUN: opt < %s -passes='sample-profile,pgo-icall-prom' -sample-profile-file=%S/Inputs/icp-vtable-cmp.prof -pass-remarks=pgo-icall-prom -enable-vtable-profile-use -icp-max-num-vtable-last-candidate=2 -icp-ignored-base-types='Base1' -S 2>&1 | FileCheck %s --check-prefixes=VTABLE-COMMON,FUNC-CMP-EARLY,FUNC-CMP

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@Base1 = constant { [4 x ptr] } { [4 x ptr] [ptr null, ptr null, ptr @Base1_foo, ptr @Base1_bar] }, !type !0
@Base2 = constant { [3 x ptr] } { [3 x ptr] [ptr null, ptr null, ptr @Base2_foo] }, !type !2
@Base3 = constant { [3 x ptr] } { [3 x ptr] [ptr null, ptr null, ptr @Base3_foo] }, !type !6

@Derived1 = constant { [3 x ptr], [4 x ptr] } { [3 x ptr] [ptr null, ptr null, ptr @Base2_foo], [4 x ptr] [ptr null, ptr null, ptr @Base1_foo, ptr @Derived1_bar] }, !type !1, !type !2, !type !3
@Derived2 = constant { [3 x ptr], [3 x ptr], [4 x ptr] } { [3 x ptr] [ptr null, ptr null, ptr @Base3_foo], [3 x ptr] [ptr null, ptr null, ptr @Base2_foo], [4 x ptr] [ptr null, ptr null, ptr @Base1_foo, ptr @Derived2_bar] }, !type !4, !type !5, !type !6, !type !7
@Derived3 = constant { [4 x ptr] } { [4 x ptr] [ptr null, ptr null, ptr @Base1_foo, ptr @Base1_bar] }, !type !0, !type !8

; VTABLE-CMP: remark: test.cc:2:0: Promote indirect call to Derived1_bar with count 900 out of 900, sink 2 instruction(s) and compare 1 vtable(s): {Derived1}
; VTABLE-CMP: remark: test.cc:6:0: Promote indirect call to Derived1_bar with count 600 out of 1500, sink 2 instruction(s) and compare 1 vtable(s): {Derived1}
; VTABLE-CMP: remark: test.cc:6:0: Promote indirect call to Derived2_bar with count 500 out of 900, sink 2 instruction(s) and compare 1 vtable(s): {Derived2}
; VTABLE-CMP: remark: test.cc:6:0: Promote indirect call to Base1_bar with count 400 out of 400, sink 2 instruction(s) and compare 2 vtable(s): {Derived3, Base1}

; Case 1: All hot targets are promoted and inlined by SampleProfileLoader.
; Canonical ICP still rewrites the early function comparison to a vtable
; comparison and sinks the function-pointer load into the fallback block.
define void @test_early_only(ptr %d) #0 !dbg !11 {
; SAMPLE-ONLY-LABEL: define void @test_early_only(
; SAMPLE-ONLY:         [[CMP:%.*]] = icmp eq ptr {{.*}}, @Derived1_bar, !dbg {{.*}}, !samplepgo.icp.count ![[ICP_CNT1:[0-9]+]]
;
; VTABLE-CMP-EARLY-LABEL: define void @test_early_only(
; VTABLE-CMP-EARLY-SAME: ptr [[D:%.*]]) {{.*}} {
; VTABLE-CMP-EARLY-NEXT:  [[ENTRY:.*:]]
; VTABLE-CMP-EARLY-NEXT:    [[VTABLE:%.*]] = load ptr, ptr [[D]], align 8, !dbg [[DBG12:![0-9]+]], !prof [[PROF_EARLY_VT:![0-9]+]]
; VTABLE-CMP-EARLY-NEXT:    [[TMP0:%.*]] = tail call i1 @llvm.type.test(ptr [[VTABLE]], metadata !"Base1"), !dbg [[DBG12]]
; VTABLE-CMP-EARLY-NEXT:    tail call void @llvm.assume(i1 [[TMP0]]), !dbg [[DBG12]]
; VTABLE-CMP-EARLY-NEXT:    [[TMP1:%.*]] = icmp eq ptr [[VTABLE]], getelementptr inbounds (i8, ptr @Derived1, i32 40), !dbg [[DBG12]]
; VTABLE-CMP-EARLY-NEXT:    br i1 [[TMP1]], label %[[IF_TRUE:.*]], label %[[IF_FALSE:.*]], !dbg [[DBG12]]
; VTABLE-CMP-EARLY:       [[IF_TRUE]]:
; VTABLE-CMP-EARLY-NEXT:    call void @sideeffect(), !dbg [[DBG_INLINED1:![0-9]+]]
; VTABLE-CMP-EARLY-NEXT:    br label %[[IF_END:.*]], !dbg [[DBG12]]
; VTABLE-CMP-EARLY:       [[IF_FALSE]]:
; VTABLE-CMP-EARLY-NEXT:    [[VFN:%.*]] = getelementptr inbounds ptr, ptr [[VTABLE]], i64 1, !dbg [[DBG12]]
; VTABLE-CMP-EARLY-NEXT:    [[FPTR:%.*]] = load ptr, ptr [[VFN]], align 8, !dbg [[DBG12]]
; VTABLE-CMP-EARLY-NEXT:    call void [[FPTR]](ptr [[D]]), !dbg [[DBG12]]
; VTABLE-CMP-EARLY-NEXT:    br label %[[IF_END]], !dbg [[DBG12]]
;
; FUNC-CMP-EARLY-LABEL: define void @test_early_only(
; FUNC-CMP-EARLY:         [[VFN:%.*]] = getelementptr inbounds ptr, ptr [[VTABLE:%.*]], i64 1
; FUNC-CMP-EARLY-NEXT:    [[FPTR:%.*]] = load ptr, ptr [[VFN]], align 8
; FUNC-CMP-EARLY-NEXT:    [[CMP:%.*]] = icmp eq ptr [[FPTR]], @Derived1_bar, !dbg [[DBG12:![0-9]+]]{{$}}
entry:
  %vtable = load ptr, ptr %d, !dbg !12, !prof !9
  %0 = tail call i1 @llvm.type.test(ptr %vtable, metadata !"Base1"), !dbg !12
  tail call void @llvm.assume(i1 %0), !dbg !12
  %vfn = getelementptr inbounds ptr, ptr %vtable, i64 1, !dbg !12
  %1 = load ptr, ptr %vfn, !dbg !12
  call void %1(ptr %d), !dbg !12
  ret void, !dbg !12
}

; Case 2: First target (@Derived1_bar) is promoted and inlined by
; SampleProfileLoader, while remaining targets (@Derived2_bar, @Base1_bar) are
; promoted by canonical ICP.
define void @test_early_and_remaining(ptr %d) #0 !dbg !13 {
; SAMPLE-ONLY-LABEL: define void @test_early_and_remaining(
; SAMPLE-ONLY:         [[CMP:%.*]] = icmp eq ptr {{.*}}, @Derived1_bar, !dbg {{.*}}, !samplepgo.icp.count ![[ICP_CNT2:[0-9]+]]
;
; VTABLE-CMP-LABEL: define void @test_early_and_remaining(
; VTABLE-CMP-SAME: ptr [[D:%.*]]) {{.*}} {
; VTABLE-CMP-NEXT:  [[ENTRY:.*:]]
; VTABLE-CMP-NEXT:    [[VTABLE:%.*]] = load ptr, ptr [[D]], align 8, !dbg [[DBG14:![0-9]+]], !prof [[PROF_REM_VT:![0-9]+]]
; VTABLE-CMP-NEXT:    [[TMP0:%.*]] = tail call i1 @llvm.type.test(ptr [[VTABLE]], metadata !"Base1"), !dbg [[DBG14]]
; VTABLE-CMP-NEXT:    tail call void @llvm.assume(i1 [[TMP0]]), !dbg [[DBG14]]
; VTABLE-CMP-NEXT:    [[TMP1:%.*]] = icmp eq ptr [[VTABLE]], getelementptr inbounds (i8, ptr @Derived1, i32 40), !dbg [[DBG14]]
; VTABLE-CMP-NEXT:    br i1 [[TMP1]], label %[[IF_TRUE1:.*]], label %[[IF_FALSE1:.*]], !dbg [[DBG14]]
; VTABLE-CMP:       [[IF_TRUE1]]:
; VTABLE-CMP-NEXT:    call void @sideeffect(), !dbg [[DBG_INLINED2:![0-9]+]]
; VTABLE-CMP-NEXT:    br label %[[IF_END1:.*]], !dbg [[DBG14]]
; VTABLE-CMP:       [[IF_FALSE1]]:
; VTABLE-CMP-NEXT:    [[TMP2:%.*]] = icmp eq ptr [[VTABLE]], getelementptr inbounds (i8, ptr @Derived2, i32 64), !dbg [[DBG14]]
; VTABLE-CMP-NEXT:    br i1 [[TMP2]], label %[[IF_TRUE2:.*]], label %[[IF_FALSE2:.*]], !dbg [[DBG14]]
; VTABLE-CMP:       [[IF_TRUE2]]:
; VTABLE-CMP-NEXT:    call void @Derived2_bar(ptr [[D]]), !dbg [[DBG14]]
; VTABLE-CMP-NEXT:    br label %[[IF_END2:.*]], !dbg [[DBG14]]
; VTABLE-CMP:       [[IF_FALSE2]]:
; VTABLE-CMP-NEXT:    [[TMP3:%.*]] = icmp eq ptr [[VTABLE]], getelementptr inbounds (i8, ptr @Base1, i32 16), !dbg [[DBG14]]
; VTABLE-CMP-NEXT:    [[TMP4:%.*]] = icmp eq ptr [[VTABLE]], getelementptr inbounds (i8, ptr @Derived3, i32 16), !dbg [[DBG14]]
; VTABLE-CMP-NEXT:    [[TMP5:%.*]] = or i1 [[TMP3]], [[TMP4]], !dbg [[DBG14]]
; VTABLE-CMP-NEXT:    br i1 [[TMP5]], label %[[IF_TRUE3:.*]], label %[[IF_FALSE3:.*]], !dbg [[DBG14]]
; VTABLE-CMP:       [[IF_TRUE3]]:
; VTABLE-CMP-NEXT:    call void @Base1_bar(ptr [[D]]), !dbg [[DBG14]]
; VTABLE-CMP-NEXT:    br label %[[IF_END3:.*]], !dbg [[DBG14]]
; VTABLE-CMP:       [[IF_FALSE3]]:
; VTABLE-CMP-NEXT:    [[VFN:%.*]] = getelementptr inbounds ptr, ptr [[VTABLE]], i64 1, !dbg [[DBG14]]
; VTABLE-CMP-NEXT:    [[FPTR:%.*]] = load ptr, ptr [[VFN]], align 8, !dbg [[DBG14]]
; VTABLE-CMP-NEXT:    call void [[FPTR]](ptr [[D]]), !dbg [[DBG14]]
; VTABLE-CMP-NEXT:    br label %[[IF_END3]]
;
; FUNC-CMP-LABEL: define void @test_early_and_remaining(
; FUNC-CMP:         [[VFN:%.*]] = getelementptr inbounds ptr, ptr [[VTABLE:%.*]], i64 1, !dbg [[DBG14:![0-9]+]]
; FUNC-CMP-NEXT:    [[FPTR:%.*]] = load ptr, ptr [[VFN]], align 8, !dbg [[DBG14]]
; FUNC-CMP-NEXT:    [[CMP1:%.*]] = icmp eq ptr [[FPTR]], @Derived1_bar, !dbg [[DBG14]]{{$}}
; FUNC-CMP:         [[CMP2:%.*]] = icmp eq ptr [[FPTR]], @Derived2_bar, !dbg [[DBG14]]
; FUNC-CMP:         [[CMP3:%.*]] = icmp eq ptr [[FPTR]], @Base1_bar, !dbg [[DBG14]]
; FUNC-CMP:         call void [[FPTR]](ptr [[D:%.*]]), !dbg [[DBG14]]
entry:
  %vtable = load ptr, ptr %d, !dbg !14, !prof !10
  %0 = tail call i1 @llvm.type.test(ptr %vtable, metadata !"Base1"), !dbg !14
  tail call void @llvm.assume(i1 %0), !dbg !14
  %vfn = getelementptr inbounds ptr, ptr %vtable, i64 1, !dbg !14
  %1 = load ptr, ptr %vfn, !dbg !14
  call void %1(ptr %d), !dbg !14
  ret void, !dbg !14
}

define void @Base1_bar(ptr %this) #0 !dbg !15 {
  ret void, !dbg !16
}

define void @Derived1_bar(ptr %this) #0 !dbg !17 {
  call void @sideeffect(), !dbg !18
  ret void, !dbg !18
}

define void @Derived2_bar(ptr %this) #0 !dbg !19 {
  ret void, !dbg !20
}

declare void @sideeffect()
declare i1 @llvm.type.test(ptr, metadata)
declare void @llvm.assume(i1)
declare i32 @Base2_foo(ptr)
declare i32 @Base1_foo(ptr)
declare void @Base3_foo(ptr)

attributes #0 = { "use-sample-profile" }

!llvm.dbg.cu = !{!21}
!llvm.module.flags = !{!22}

!0 = !{i64 16, !"Base1"}
!1 = !{i64 40, !"Base1"}
!2 = !{i64 16, !"Base2"}
!3 = !{i64 16, !"Derived1"}
!4 = !{i64 64, !"Base1"}
!5 = !{i64 40, !"Base2"}
!6 = !{i64 16, !"Base3"}
!7 = !{i64 16, !"Derived2"}
!8 = !{i64 16, !"Derived3"}
!9 = !{!"VP", i32 2, i64 1000, i64 -4123858694673519054, i64 900, i64 12345678, i64 100}
!10 = !{!"VP", i32 2, i64 1600, i64 -4123858694673519054, i64 600, i64 -7211198353767973908, i64 500, i64 -3574436251470806727, i64 200, i64 6288809125658696740, i64 200, i64 12345678, i64 100}

!11 = distinct !DISubprogram(name: "test_early_only", scope: !23, file: !23, line: 1, type: !24, unit: !21)
!12 = !DILocation(line: 2, scope: !11)
!13 = distinct !DISubprogram(name: "test_early_and_remaining", scope: !23, file: !23, line: 5, type: !24, unit: !21)
!14 = !DILocation(line: 6, scope: !13)
!15 = distinct !DISubprogram(name: "Base1_bar", scope: !23, file: !23, line: 10, type: !24, unit: !21)
!16 = !DILocation(line: 11, scope: !15)
!17 = distinct !DISubprogram(name: "Derived1_bar", scope: !23, file: !23, line: 15, type: !24, unit: !21)
!18 = !DILocation(line: 16, scope: !17)
!19 = distinct !DISubprogram(name: "Derived2_bar", scope: !23, file: !23, line: 20, type: !24, unit: !21)
!20 = !DILocation(line: 21, scope: !19)
!21 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus, file: !23)
!22 = !{i32 2, !"Debug Info Version", i32 3}
!23 = !DIFile(filename: "test.cc", directory: "/")
!24 = !DISubroutineType(types: !25)
!25 = !{null}

; SAMPLE-ONLY: ![[ICP_CNT1]] = !{i64 900}
; SAMPLE-ONLY: ![[ICP_CNT2]] = !{i64 600}
; VTABLE-COMMON-NOT: samplepgo.icp.count
; VTABLE-COMMON: [[PROF_EARLY_VT:![0-9]+]] = !{!"VP", i32 2, i64 100, i64 12345678, i64 100}
