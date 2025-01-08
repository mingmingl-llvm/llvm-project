; RUN: llc -stop-after=block-placement %s -o - | llc --run-pass=static-data-splitter -stats -x mir -o - 2>&1 | FileCheck %s --check-prefix=STAT

; STAT: 1 static-data-splitter - Number of cold jump tables seen
; STAT: 1 static-data-splitter - Number of hot jump tables seen

; ModuleID = 'instr_jump_table.cc'
source_filename = "instr_jump_table.cc"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str.2 = private unnamed_addr constant [7 x i8] c"case 3\00", align 1
@.str.3 = private unnamed_addr constant [7 x i8] c"case 4\00", align 1
@.str.4 = private unnamed_addr constant [7 x i8] c"case 5\00", align 1
@.str.6 = private unnamed_addr constant [11 x i8] c"sum is %d\0A\00", align 1
@str.9 = private unnamed_addr constant [7 x i8] c"case 2\00", align 1
@str.10 = private unnamed_addr constant [7 x i8] c"case 1\00", align 1
@str.11 = private unnamed_addr constant [8 x i8] c"default\00", align 1

; Function Attrs: inlinehint mustprogress nofree noinline nounwind uwtable
define dso_local noundef range(i32 -715827882, 715827883) i32 @_Z12hotJumptablei(i32 noundef %num) local_unnamed_addr #0 !prof !42 !section_prefix !43 {
entry:
  switch i32 %num, label %sw.default [
    i32 1, label %sw.bb
    i32 2, label %sw.bb1
    i32 3, label %sw.bb3
    i32 4, label %sw.bb5
    i32 5, label %sw.bb7
  ], !prof !44

sw.bb:                                            ; preds = %entry
  %puts11 = tail call i32 @puts(ptr nonnull dereferenceable(1) @str.10)
  br label %sw.epilog

sw.bb1:                                           ; preds = %entry
  %puts = tail call i32 @puts(ptr nonnull dereferenceable(1) @str.9)
  br label %sw.epilog

sw.bb3:                                           ; preds = %entry
  %call4 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2)
  br label %sw.bb5

sw.bb5:                                           ; preds = %entry, %sw.bb3
  %call6 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.3)
  br label %sw.bb7

sw.bb7:                                           ; preds = %entry, %sw.bb5
  %call8 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.4)
  br label %sw.epilog

sw.default:                                       ; preds = %entry
  %puts12 = tail call i32 @puts(ptr nonnull dereferenceable(1) @str.11)
  br label %sw.epilog

sw.epilog:                                        ; preds = %sw.default, %sw.bb7, %sw.bb1, %sw.bb
  %div = sdiv i32 %num, 3
  ret i32 %div
}

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #1

; Function Attrs: cold mustprogress nofree noinline nounwind uwtable
define dso_local void @_Z13coldJumptablei(i32 noundef %num) local_unnamed_addr #2 !prof !45 {
entry:
  switch i32 %num, label %sw.default [
    i32 1, label %sw.bb
    i32 2, label %sw.bb1
    i32 3, label %sw.bb3
    i32 4, label %sw.bb5
    i32 5, label %sw.bb7
  ], !prof !46

sw.bb:                                            ; preds = %entry
  %puts10 = tail call i32 @puts(ptr nonnull dereferenceable(1) @str.10)
  br label %sw.epilog

sw.bb1:                                           ; preds = %entry
  %puts = tail call i32 @puts(ptr nonnull dereferenceable(1) @str.9)
  br label %sw.epilog

sw.bb3:                                           ; preds = %entry
  %call4 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2)
  br label %sw.bb5

sw.bb5:                                           ; preds = %entry, %sw.bb3
  %call6 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.3)
  br label %sw.bb7

sw.bb7:                                           ; preds = %entry, %sw.bb5
  %call8 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.4)
  br label %sw.epilog

sw.default:                                       ; preds = %entry
  %puts11 = tail call i32 @puts(ptr nonnull dereferenceable(1) @str.11)
  br label %sw.epilog

sw.epilog:                                        ; preds = %sw.default, %sw.bb7, %sw.bb1, %sw.bb
  ret void
}

; Function Attrs: mustprogress nofree norecurse nounwind uwtable
define dso_local noundef i32 @main(i32 noundef %argc, ptr nocapture noundef readnone %argv) local_unnamed_addr #3 !prof !45 {
entry:
  br label %for.body

for.cond.cleanup:                                 ; preds = %for.body
  tail call void @_Z13coldJumptablei(i32 noundef 123)
  %call1 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.6, i32 noundef %add)
  ret i32 0

for.body:                                         ; preds = %entry, %for.body
  %i.06 = phi i32 [ 0, %entry ], [ %inc, %for.body ]
  %sum.05 = phi i32 [ 0, %entry ], [ %add, %for.body ]
  %rem = urem i32 %i.06, 10
  %call = tail call noundef i32 @_Z12hotJumptablei(i32 noundef %rem)
  %add = add nsw i32 %call, %sum.05
  %inc = add nuw nsw i32 %i.06, 1
  %exitcond.not = icmp eq i32 %inc, 100000
  br i1 %exitcond.not, label %for.cond.cleanup, label %for.body, !prof !47, !llvm.loop !48
}

; Function Attrs: nofree nounwind
declare noundef i32 @puts(ptr nocapture noundef readonly) local_unnamed_addr #4

attributes #0 = { inlinehint mustprogress nofree noinline nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #2 = { cold mustprogress nofree noinline nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { mustprogress nofree norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { nofree nounwind }

!llvm.module.flags = !{!0, !1, !2, !3, !4, !33}
!llvm.ident = !{!41}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 1, !"ProfileSummary", !5}
!5 = !{!6, !7, !8, !9, !10, !11, !12, !13, !14, !15}
!6 = !{!"ProfileFormat", !"InstrProf"}
!7 = !{!"TotalCount", i64 230002}
!8 = !{!"MaxCount", i64 100000}
!9 = !{!"MaxInternalCount", i64 50000}
!10 = !{!"MaxFunctionCount", i64 100000}
!11 = !{!"NumCounts", i64 14}
!12 = !{!"NumFunctions", i64 3}
!13 = !{!"IsPartialProfile", i64 0}
!14 = !{!"PartialProfileRatio", double 0.000000e+00}
!15 = !{!"DetailedSummary", !16}
!16 = !{!17, !18, !19, !20, !21, !22, !23, !24, !25, !26, !27, !28, !29, !30, !31, !32}
!17 = !{i32 10000, i64 100000, i32 1}
!18 = !{i32 100000, i64 100000, i32 1}
!19 = !{i32 200000, i64 100000, i32 1}
!20 = !{i32 300000, i64 100000, i32 1}
!21 = !{i32 400000, i64 100000, i32 1}
!22 = !{i32 500000, i64 50000, i32 2}
!23 = !{i32 600000, i64 50000, i32 2}
!24 = !{i32 700000, i64 30000, i32 3}
!25 = !{i32 800000, i64 20000, i32 4}
!26 = !{i32 900000, i64 10000, i32 7}
!27 = !{i32 950000, i64 10000, i32 7}
!28 = !{i32 990000, i64 10000, i32 7}
!29 = !{i32 999000, i64 10000, i32 7}
!30 = !{i32 999900, i64 10000, i32 7}
!31 = !{i32 999990, i64 10000, i32 7}
!32 = !{i32 999999, i64 1, i32 9}
!33 = !{i32 5, !"CG Profile", !34}
!34 = distinct !{!35, !36, !37, !38, !39, !40}
!35 = !{ptr @_Z12hotJumptablei, ptr @puts, i64 70000}
!36 = !{ptr @_Z12hotJumptablei, ptr @printf, i64 60000}
!37 = !{ptr @_Z13coldJumptablei, ptr @puts, i64 1}
!38 = !{ptr @main, ptr @_Z13coldJumptablei, i64 1}
!39 = !{ptr @main, ptr @printf, i64 1}
!40 = !{ptr @main, ptr @_Z12hotJumptablei, i64 99999}
!41 = !{!"clang version 20.0.0git (https://github.com/mingmingl-llvm/llvm-project.git 181b480e1b117a1fd6f9d508b04cbfbc0abc94f0)"}
!42 = !{!"function_entry_count", i64 100000}
!43 = !{!"function_section_prefix", !"hot"}
!44 = !{!"branch_weights", i32 50000, i32 10000, i32 10000, i32 10000, i32 10000, i32 10000}
!45 = !{!"function_entry_count", i64 1}
!46 = !{!"branch_weights", i32 1, i32 0, i32 0, i32 0, i32 0, i32 0}
!47 = !{!"branch_weights", i32 1, i32 99999}
!48 = distinct !{!48, !49}
!49 = !{!"llvm.loop.mustprogress"}

