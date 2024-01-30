; RUN: opt < %s -passes=pgo-icall-prom -S  | FileCheck %s --check-prefix=ICALL-FUNC
; RUN: opt < %s -passes=pgo-icall-prom -enable-vtable-cmp -icp-vtable-cmp-inst-threshold=4 -icp-vtable-cmp-inst-last-candidate-threshold=4 -icp-vtable-cmp-total-inst-threshold=4 -S | FileCheck %s --check-prefix=ICALL-VTABLE

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%class.Error = type { i8 }

@_ZTI5Error = constant { ptr, ptr } { ptr getelementptr inbounds (ptr, ptr null, i64 2), ptr null }
@_ZTV4Base = constant { [3 x ptr] } { [3 x ptr] [ptr null, ptr null, ptr @_ZN4Base10get_ticketEv] }, !type !15, !type !16
@_ZTV7Derived = constant { [3 x ptr] } { [3 x ptr] [ptr null, ptr null, ptr @_ZN7Derived10get_ticketEv] }, !type !15, !type !16, !type !17, !type !18

@.str = private unnamed_addr constant [15 x i8] c"out of tickets\00"

;.
; ICALL-FUNC: @_ZTI5Error = constant { ptr, ptr } { ptr getelementptr inbounds (ptr, ptr null, i64 2), ptr null }
; ICALL-FUNC: @_ZTV4Base = constant { [3 x ptr] } { [3 x ptr] [ptr null, ptr null, ptr @_ZN4Base10get_ticketEv] }, !type [[META0:![0-9]+]], !type [[META1:![0-9]+]]
; ICALL-FUNC: @_ZTV7Derived = constant { [3 x ptr] } { [3 x ptr] [ptr null, ptr null, ptr @_ZN7Derived10get_ticketEv] }, !type [[META0]], !type [[META1]], !type [[META2:![0-9]+]], !type [[META3:![0-9]+]]
; ICALL-FUNC: @.str = private unnamed_addr constant [15 x i8] c"out of tickets\00"
;.
; ICALL-VTABLE: @_ZTI5Error = constant { ptr, ptr } { ptr getelementptr inbounds (ptr, ptr null, i64 2), ptr null }
; ICALL-VTABLE: @_ZTV4Base = constant { [3 x ptr] } { [3 x ptr] [ptr null, ptr null, ptr @_ZN4Base10get_ticketEv] }, !type [[META0:![0-9]+]], !type [[META1:![0-9]+]]
; ICALL-VTABLE: @_ZTV7Derived = constant { [3 x ptr] } { [3 x ptr] [ptr null, ptr null, ptr @_ZN7Derived10get_ticketEv] }, !type [[META0]], !type [[META1]], !type [[META2:![0-9]+]], !type [[META3:![0-9]+]]
; ICALL-VTABLE: @.str = private unnamed_addr constant [15 x i8] c"out of tickets\00"
; ICALL-VTABLE: @_ZTV7Derived.icp.16 = constant i64 add (i64 ptrtoint (ptr @_ZTV7Derived to i64), i64 16), comdat
; ICALL-VTABLE: @_ZTV4Base.icp.16 = constant i64 add (i64 ptrtoint (ptr @_ZTV4Base to i64), i64 16), comdat
;.
define i32 @_Z4testP4Base(ptr %b) personality ptr @__gxx_personality_v0 {
; ICALL-FUNC-LABEL: define i32 @_Z4testP4Base(
; ICALL-FUNC-SAME: ptr [[B:%.*]]) personality ptr @__gxx_personality_v0 {
; ICALL-FUNC-NEXT:  entry:
; ICALL-FUNC-NEXT:    [[E:%.*]] = alloca [[CLASS_ERROR:%.*]], align 8
; ICALL-FUNC-NEXT:    [[VTABLE:%.*]] = load ptr, ptr [[B]], align 8
; ICALL-FUNC-NEXT:    [[TMP0:%.*]] = tail call i1 @llvm.type.test(ptr [[VTABLE]], metadata !"_ZTS4Base")
; ICALL-FUNC-NEXT:    tail call void @llvm.assume(i1 [[TMP0]])
; ICALL-FUNC-NEXT:    [[TMP1:%.*]] = load ptr, ptr [[VTABLE]], align 8
; ICALL-FUNC-NEXT:    [[TMP2:%.*]] = icmp eq ptr [[TMP1]], @_ZN7Derived10get_ticketEv
; ICALL-FUNC-NEXT:    br i1 [[TMP2]], label [[IF_TRUE_DIRECT_TARG:%.*]], label [[IF_FALSE_ORIG_INDIRECT:%.*]], !prof [[PROF18:![0-9]+]]
; ICALL-FUNC:       if.true.direct_targ:
; ICALL-FUNC-NEXT:    [[TMP3:%.*]] = invoke i32 @_ZN7Derived10get_ticketEv(ptr [[B]])
; ICALL-FUNC-NEXT:            to label [[IF_END_ICP:%.*]] unwind label [[LPAD:%.*]]
; ICALL-FUNC:       if.false.orig_indirect:
; ICALL-FUNC-NEXT:    [[TMP4:%.*]] = icmp eq ptr [[TMP1]], @_ZN4Base10get_ticketEv
; ICALL-FUNC-NEXT:    br i1 [[TMP4]], label [[IF_TRUE_DIRECT_TARG1:%.*]], label [[IF_FALSE_ORIG_INDIRECT2:%.*]], !prof [[PROF19:![0-9]+]]
; ICALL-FUNC:       if.true.direct_targ1:
; ICALL-FUNC-NEXT:    [[TMP5:%.*]] = invoke i32 @_ZN4Base10get_ticketEv(ptr [[B]])
; ICALL-FUNC-NEXT:            to label [[IF_END_ICP3:%.*]] unwind label [[LPAD]]
; ICALL-FUNC:       if.false.orig_indirect2:
; ICALL-FUNC-NEXT:    [[CALL:%.*]] = invoke i32 [[TMP1]](ptr [[B]])
; ICALL-FUNC-NEXT:            to label [[IF_END_ICP3]] unwind label [[LPAD]]
; ICALL-FUNC:       if.end.icp3:
; ICALL-FUNC-NEXT:    [[TMP6:%.*]] = phi i32 [ [[CALL]], [[IF_FALSE_ORIG_INDIRECT2]] ], [ [[TMP5]], [[IF_TRUE_DIRECT_TARG1]] ]
; ICALL-FUNC-NEXT:    br label [[IF_END_ICP]]
; ICALL-FUNC:       if.end.icp:
; ICALL-FUNC-NEXT:    [[TMP7:%.*]] = phi i32 [ [[TMP6]], [[IF_END_ICP3]] ], [ [[TMP3]], [[IF_TRUE_DIRECT_TARG]] ]
; ICALL-FUNC-NEXT:    br label [[TRY_CONT:%.*]]
; ICALL-FUNC:       lpad:
; ICALL-FUNC-NEXT:    [[TMP8:%.*]] = landingpad { ptr, i32 }
; ICALL-FUNC-NEXT:            cleanup
; ICALL-FUNC-NEXT:            catch ptr @_ZTI5Error
; ICALL-FUNC-NEXT:    [[TMP9:%.*]] = extractvalue { ptr, i32 } [[TMP8]], 1
; ICALL-FUNC-NEXT:    [[TMP10:%.*]] = tail call i32 @llvm.eh.typeid.for(ptr nonnull @_ZTI5Error)
; ICALL-FUNC-NEXT:    [[MATCHES:%.*]] = icmp eq i32 [[TMP9]], [[TMP10]]
; ICALL-FUNC-NEXT:    br i1 [[MATCHES]], label [[CATCH:%.*]], label [[EHCLEANUP:%.*]]
; ICALL-FUNC:       catch:
; ICALL-FUNC-NEXT:    [[TMP11:%.*]] = extractvalue { ptr, i32 } [[TMP8]], 0
; ICALL-FUNC-NEXT:    [[CALL3:%.*]] = invoke i32 @_ZN5Error10error_codeEv(ptr nonnull align 1 dereferenceable(1) [[E]])
; ICALL-FUNC-NEXT:            to label [[INVOKE_CONT2:%.*]] unwind label [[LPAD1:%.*]]
; ICALL-FUNC:       invoke.cont2:
; ICALL-FUNC-NEXT:    call void @__cxa_end_catch()
; ICALL-FUNC-NEXT:    br label [[TRY_CONT]]
; ICALL-FUNC:       try.cont:
; ICALL-FUNC-NEXT:    [[RET_0:%.*]] = phi i32 [ [[CALL3]], [[INVOKE_CONT2]] ], [ [[TMP7]], [[IF_END_ICP]] ]
; ICALL-FUNC-NEXT:    ret i32 [[RET_0]]
; ICALL-FUNC:       lpad1:
; ICALL-FUNC-NEXT:    [[TMP12:%.*]] = landingpad { ptr, i32 }
; ICALL-FUNC-NEXT:            cleanup
; ICALL-FUNC-NEXT:    invoke void @__cxa_end_catch()
; ICALL-FUNC-NEXT:            to label [[INVOKE_CONT4:%.*]] unwind label [[TERMINATE_LPAD:%.*]]
; ICALL-FUNC:       invoke.cont4:
; ICALL-FUNC-NEXT:    br label [[EHCLEANUP]]
; ICALL-FUNC:       ehcleanup:
; ICALL-FUNC-NEXT:    [[LPAD_VAL7_MERGED:%.*]] = phi { ptr, i32 } [ [[TMP12]], [[INVOKE_CONT4]] ], [ [[TMP8]], [[LPAD]] ]
; ICALL-FUNC-NEXT:    resume { ptr, i32 } [[LPAD_VAL7_MERGED]]
; ICALL-FUNC:       terminate.lpad:
; ICALL-FUNC-NEXT:    [[TMP13:%.*]] = landingpad { ptr, i32 }
; ICALL-FUNC-NEXT:            catch ptr null
; ICALL-FUNC-NEXT:    [[TMP14:%.*]] = extractvalue { ptr, i32 } [[TMP13]], 0
; ICALL-FUNC-NEXT:    unreachable
;
; ICALL-VTABLE-LABEL: define i32 @_Z4testP4Base(
; ICALL-VTABLE-SAME: ptr [[B:%.*]]) personality ptr @__gxx_personality_v0 {
; ICALL-VTABLE-NEXT:  entry:
; ICALL-VTABLE-NEXT:    [[E:%.*]] = alloca [[CLASS_ERROR:%.*]], align 8
; ICALL-VTABLE-NEXT:    [[VTABLE:%.*]] = load ptr, ptr [[B]], align 8
; ICALL-VTABLE-NEXT:    [[TMP0:%.*]] = tail call i1 @llvm.type.test(ptr [[VTABLE]], metadata !"_ZTS4Base")
; ICALL-VTABLE-NEXT:    tail call void @llvm.assume(i1 [[TMP0]])
; ICALL-VTABLE-NEXT:    [[TMP1:%.*]] = icmp eq ptr [[VTABLE]], @_ZTV7Derived.icp.16
; ICALL-VTABLE-NEXT:    br i1 [[TMP1]], label [[IF_THEN_DIRECT_CALL:%.*]], label [[IF_ELSE_ORIG_INDIRECT:%.*]], !prof [[PROF18:![0-9]+]]
; ICALL-VTABLE:       if.then.direct_call:
; ICALL-VTABLE-NEXT:    [[TMP2:%.*]] = invoke i32 @_ZN7Derived10get_ticketEv(ptr [[B]])
; ICALL-VTABLE-NEXT:            to label [[IF_END_ICP:%.*]] unwind label [[LPAD:%.*]]
; ICALL-VTABLE:       if.else.orig_indirect:
; ICALL-VTABLE-NEXT:    [[TMP3:%.*]] = icmp eq ptr [[VTABLE]], @_ZTV4Base.icp.16
; ICALL-VTABLE-NEXT:    br i1 [[TMP3]], label [[IF_THEN_DIRECT_CALL1:%.*]], label [[IF_ELSE_ORIG_INDIRECT2:%.*]], !prof [[PROF19:![0-9]+]]
; ICALL-VTABLE:       if.then.direct_call1:
; ICALL-VTABLE-NEXT:    [[TMP4:%.*]] = invoke i32 @_ZN4Base10get_ticketEv(ptr [[B]])
; ICALL-VTABLE-NEXT:            to label [[IF_END_ICP3:%.*]] unwind label [[LPAD]]
; ICALL-VTABLE:       if.else.orig_indirect2:
; ICALL-VTABLE-NEXT:    [[TMP5:%.*]] = load ptr, ptr [[VTABLE]], align 8
; ICALL-VTABLE-NEXT:    [[CALL:%.*]] = invoke i32 [[TMP5]](ptr [[B]])
; ICALL-VTABLE-NEXT:            to label [[IF_END_ICP3]] unwind label [[LPAD]]
; ICALL-VTABLE:       if.end.icp3:
; ICALL-VTABLE-NEXT:    [[TMP6:%.*]] = phi i32 [ [[CALL]], [[IF_ELSE_ORIG_INDIRECT2]] ], [ [[TMP4]], [[IF_THEN_DIRECT_CALL1]] ]
; ICALL-VTABLE-NEXT:    br label [[IF_END_ICP]]
; ICALL-VTABLE:       if.end.icp:
; ICALL-VTABLE-NEXT:    [[TMP7:%.*]] = phi i32 [ [[TMP6]], [[IF_END_ICP3]] ], [ [[TMP2]], [[IF_THEN_DIRECT_CALL]] ]
; ICALL-VTABLE-NEXT:    br label [[TRY_CONT:%.*]]
; ICALL-VTABLE:       lpad:
; ICALL-VTABLE-NEXT:    [[TMP8:%.*]] = landingpad { ptr, i32 }
; ICALL-VTABLE-NEXT:            cleanup
; ICALL-VTABLE-NEXT:            catch ptr @_ZTI5Error
; ICALL-VTABLE-NEXT:    [[TMP9:%.*]] = extractvalue { ptr, i32 } [[TMP8]], 1
; ICALL-VTABLE-NEXT:    [[TMP10:%.*]] = tail call i32 @llvm.eh.typeid.for(ptr nonnull @_ZTI5Error)
; ICALL-VTABLE-NEXT:    [[MATCHES:%.*]] = icmp eq i32 [[TMP9]], [[TMP10]]
; ICALL-VTABLE-NEXT:    br i1 [[MATCHES]], label [[CATCH:%.*]], label [[EHCLEANUP:%.*]]
; ICALL-VTABLE:       catch:
; ICALL-VTABLE-NEXT:    [[TMP11:%.*]] = extractvalue { ptr, i32 } [[TMP8]], 0
; ICALL-VTABLE-NEXT:    [[CALL3:%.*]] = invoke i32 @_ZN5Error10error_codeEv(ptr nonnull align 1 dereferenceable(1) [[E]])
; ICALL-VTABLE-NEXT:            to label [[INVOKE_CONT2:%.*]] unwind label [[LPAD1:%.*]]
; ICALL-VTABLE:       invoke.cont2:
; ICALL-VTABLE-NEXT:    call void @__cxa_end_catch()
; ICALL-VTABLE-NEXT:    br label [[TRY_CONT]]
; ICALL-VTABLE:       try.cont:
; ICALL-VTABLE-NEXT:    [[RET_0:%.*]] = phi i32 [ [[CALL3]], [[INVOKE_CONT2]] ], [ [[TMP7]], [[IF_END_ICP]] ]
; ICALL-VTABLE-NEXT:    ret i32 [[RET_0]]
; ICALL-VTABLE:       lpad1:
; ICALL-VTABLE-NEXT:    [[TMP12:%.*]] = landingpad { ptr, i32 }
; ICALL-VTABLE-NEXT:            cleanup
; ICALL-VTABLE-NEXT:    invoke void @__cxa_end_catch()
; ICALL-VTABLE-NEXT:            to label [[INVOKE_CONT4:%.*]] unwind label [[TERMINATE_LPAD:%.*]]
; ICALL-VTABLE:       invoke.cont4:
; ICALL-VTABLE-NEXT:    br label [[EHCLEANUP]]
; ICALL-VTABLE:       ehcleanup:
; ICALL-VTABLE-NEXT:    [[LPAD_VAL7_MERGED:%.*]] = phi { ptr, i32 } [ [[TMP12]], [[INVOKE_CONT4]] ], [ [[TMP8]], [[LPAD]] ]
; ICALL-VTABLE-NEXT:    resume { ptr, i32 } [[LPAD_VAL7_MERGED]]
; ICALL-VTABLE:       terminate.lpad:
; ICALL-VTABLE-NEXT:    [[TMP13:%.*]] = landingpad { ptr, i32 }
; ICALL-VTABLE-NEXT:            catch ptr null
; ICALL-VTABLE-NEXT:    [[TMP14:%.*]] = extractvalue { ptr, i32 } [[TMP13]], 0
; ICALL-VTABLE-NEXT:    unreachable
;
entry:
  %e = alloca %class.Error
  %vtable = load ptr, ptr %b, !prof !19
  %0 = tail call i1 @llvm.type.test(ptr %vtable, metadata !"_ZTS4Base")
  tail call void @llvm.assume(i1 %0)
  %1 = load ptr, ptr %vtable
  %call = invoke i32 %1(ptr %b)
  to label %try.cont unwind label %lpad, !prof !20

lpad:
  %2 = landingpad { ptr, i32 }
  cleanup
  catch ptr @_ZTI5Error
  %3 = extractvalue { ptr, i32 } %2, 1
  %4 = tail call i32 @llvm.eh.typeid.for(ptr nonnull @_ZTI5Error)
  %matches = icmp eq i32 %3, %4
  br i1 %matches, label %catch, label %ehcleanup

catch:
  %5 = extractvalue { ptr, i32 } %2, 0

  %call3 = invoke i32 @_ZN5Error10error_codeEv(ptr nonnull align 1 dereferenceable(1) %e)
  to label %invoke.cont2 unwind label %lpad1

invoke.cont2:
  call void @__cxa_end_catch()
  br label %try.cont

try.cont:
  %ret.0 = phi i32 [ %call3, %invoke.cont2 ], [ %call, %entry ]
  ret i32 %ret.0

lpad1:
  %6 = landingpad { ptr, i32 }
  cleanup
  invoke void @__cxa_end_catch()
  to label %invoke.cont4 unwind label %terminate.lpad

invoke.cont4:
  br label %ehcleanup

ehcleanup:
  %lpad.val7.merged = phi { ptr, i32 } [ %6, %invoke.cont4 ], [ %2, %lpad ]
  resume { ptr, i32 } %lpad.val7.merged

terminate.lpad:
  %7 = landingpad { ptr, i32 }
  catch ptr null
  %8 = extractvalue { ptr, i32 } %7, 0
  unreachable
}

declare i1 @llvm.type.test(ptr, metadata)
declare void @llvm.assume(i1 noundef)
declare i32 @__gxx_personality_v0(...)
declare i32 @llvm.eh.typeid.for(ptr)

declare i32 @_ZN5Error10error_codeEv(ptr nonnull align 1 dereferenceable(1))

declare void @__cxa_end_catch()

define i32 @_ZN4Base10get_ticketEv(ptr %this) align 2 personality ptr @__gxx_personality_v0 {
entry:
  %call = tail call i32 @_Z13get_ticket_idv()
  %cmp.not = icmp eq i32 %call, -1
  br i1 %cmp.not, label %if.end, label %if.then

if.then:
  ret i32 %call

if.end:
  %exception = tail call ptr @__cxa_allocate_exception(i64 1)
  invoke void @_ZN5ErrorC1EPKci(ptr nonnull align 1 dereferenceable(1) %exception, ptr nonnull @.str, i32 1)
  to label %invoke.cont unwind label %lpad

invoke.cont:
  unreachable

lpad:
  %0 = landingpad { ptr, i32 }
  cleanup
  resume { ptr, i32 } %0
}

define i32 @_ZN7Derived10get_ticketEv(ptr %this) align 2 personality ptr @__gxx_personality_v0 {
entry:
  %call = tail call i32 @_Z13get_ticket_idv()
  %cmp.not = icmp eq i32 %call, -1
  br i1 %cmp.not, label %if.end, label %if.then

if.then:
  ret i32 %call

if.end:
  %exception = tail call ptr @__cxa_allocate_exception(i64 1)
  invoke void @_ZN5ErrorC1EPKci(ptr nonnull align 1 dereferenceable(1) %exception, ptr nonnull @.str, i32 2)
  to label %invoke.cont unwind label %lpad

invoke.cont:
  unreachable

lpad:
  %0 = landingpad { ptr, i32 }
  cleanup
  resume { ptr, i32 } %0
}

declare i32 @_Z13get_ticket_idv()
declare ptr @__cxa_allocate_exception(i64)
declare void @_ZN5ErrorC1EPKci(ptr nonnull align 1 dereferenceable(1), ptr, i32)


!llvm.module.flags = !{!1}

!1 = !{i32 1, !"ProfileSummary", !2}
!2 = !{!3, !4, !5, !6, !7, !8, !9, !10}
!3 = !{!"ProfileFormat", !"InstrProf"}
!4 = !{!"TotalCount", i64 10000}
!5 = !{!"MaxCount", i64 200}
!6 = !{!"MaxInternalCount", i64 200}
!7 = !{!"MaxFunctionCount", i64 200}
!8 = !{!"NumCounts", i64 3}
!9 = !{!"NumFunctions", i64 3}
!10 = !{!"DetailedSummary", !11}
!11 = !{!12, !13, !14}
!12 = !{i32 10000, i64 100, i32 1}
!13 = !{i32 990000, i64 100, i32 1}
!14 = !{i32 999999, i64 1, i32 2}
!15 = !{i64 16, !"_ZTS4Base"}
!16 = !{i64 16, !"_ZTSM4BaseFivE.virtual"}
!17 = !{i64 16, !"_ZTS7Derived"}
!18 = !{i64 16, !"_ZTSM7DerivedFivE.virtual"}
!19 = !{!"VP", i32 2, i64 1600, i64 13870436605473471591, i64 900, i64 1960855528937986108, i64 700}
!20 = !{!"VP", i32 0, i64 1600, i64 14811317294552474744, i64 900, i64 9261744921105590125, i64 700}
;.
; ICALL-FUNC: attributes #[[ATTR0:[0-9]+]] = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
; ICALL-FUNC: attributes #[[ATTR1:[0-9]+]] = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: write) }
; ICALL-FUNC: attributes #[[ATTR2:[0-9]+]] = { nounwind memory(none) }
;.
; ICALL-VTABLE: attributes #[[ATTR0:[0-9]+]] = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
; ICALL-VTABLE: attributes #[[ATTR1:[0-9]+]] = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: write) }
; ICALL-VTABLE: attributes #[[ATTR2:[0-9]+]] = { nounwind memory(none) }
;.
; ICALL-FUNC: [[META0]] = !{i64 16, !"_ZTS4Base"}
; ICALL-FUNC: [[META1]] = !{i64 16, !"_ZTSM4BaseFivE.virtual"}
; ICALL-FUNC: [[META2]] = !{i64 16, !"_ZTS7Derived"}
; ICALL-FUNC: [[META3]] = !{i64 16, !"_ZTSM7DerivedFivE.virtual"}
; ICALL-FUNC: [[META4:![0-9]+]] = !{i32 1, !"ProfileSummary", [[META5:![0-9]+]]}
; ICALL-FUNC: [[META5]] = !{[[META6:![0-9]+]], [[META7:![0-9]+]], [[META8:![0-9]+]], [[META9:![0-9]+]], [[META10:![0-9]+]], [[META11:![0-9]+]], [[META12:![0-9]+]], [[META13:![0-9]+]]}
; ICALL-FUNC: [[META6]] = !{!"ProfileFormat", !"InstrProf"}
; ICALL-FUNC: [[META7]] = !{!"TotalCount", i64 10000}
; ICALL-FUNC: [[META8]] = !{!"MaxCount", i64 200}
; ICALL-FUNC: [[META9]] = !{!"MaxInternalCount", i64 200}
; ICALL-FUNC: [[META10]] = !{!"MaxFunctionCount", i64 200}
; ICALL-FUNC: [[META11]] = !{!"NumCounts", i64 3}
; ICALL-FUNC: [[META12]] = !{!"NumFunctions", i64 3}
; ICALL-FUNC: [[META13]] = !{!"DetailedSummary", [[META14:![0-9]+]]}
; ICALL-FUNC: [[META14]] = !{[[META15:![0-9]+]], [[META16:![0-9]+]], [[META17:![0-9]+]]}
; ICALL-FUNC: [[META15]] = !{i32 10000, i64 100, i32 1}
; ICALL-FUNC: [[META16]] = !{i32 990000, i64 100, i32 1}
; ICALL-FUNC: [[META17]] = !{i32 999999, i64 1, i32 2}
; ICALL-FUNC: [[PROF18]] = !{!"branch_weights", i32 900, i32 700}
; ICALL-FUNC: [[PROF19]] = !{!"branch_weights", i32 700, i32 0}
;.
; ICALL-VTABLE: [[META0]] = !{i64 16, !"_ZTS4Base"}
; ICALL-VTABLE: [[META1]] = !{i64 16, !"_ZTSM4BaseFivE.virtual"}
; ICALL-VTABLE: [[META2]] = !{i64 16, !"_ZTS7Derived"}
; ICALL-VTABLE: [[META3]] = !{i64 16, !"_ZTSM7DerivedFivE.virtual"}
; ICALL-VTABLE: [[META4:![0-9]+]] = !{i32 1, !"ProfileSummary", [[META5:![0-9]+]]}
; ICALL-VTABLE: [[META5]] = !{[[META6:![0-9]+]], [[META7:![0-9]+]], [[META8:![0-9]+]], [[META9:![0-9]+]], [[META10:![0-9]+]], [[META11:![0-9]+]], [[META12:![0-9]+]], [[META13:![0-9]+]]}
; ICALL-VTABLE: [[META6]] = !{!"ProfileFormat", !"InstrProf"}
; ICALL-VTABLE: [[META7]] = !{!"TotalCount", i64 10000}
; ICALL-VTABLE: [[META8]] = !{!"MaxCount", i64 200}
; ICALL-VTABLE: [[META9]] = !{!"MaxInternalCount", i64 200}
; ICALL-VTABLE: [[META10]] = !{!"MaxFunctionCount", i64 200}
; ICALL-VTABLE: [[META11]] = !{!"NumCounts", i64 3}
; ICALL-VTABLE: [[META12]] = !{!"NumFunctions", i64 3}
; ICALL-VTABLE: [[META13]] = !{!"DetailedSummary", [[META14:![0-9]+]]}
; ICALL-VTABLE: [[META14]] = !{[[META15:![0-9]+]], [[META16:![0-9]+]], [[META17:![0-9]+]]}
; ICALL-VTABLE: [[META15]] = !{i32 10000, i64 100, i32 1}
; ICALL-VTABLE: [[META16]] = !{i32 990000, i64 100, i32 1}
; ICALL-VTABLE: [[META17]] = !{i32 999999, i64 1, i32 2}
; ICALL-VTABLE: [[PROF18]] = !{!"branch_weights", i32 900, i32 700}
; ICALL-VTABLE: [[PROF19]] = !{!"branch_weights", i32 700, i32 0}
;.
