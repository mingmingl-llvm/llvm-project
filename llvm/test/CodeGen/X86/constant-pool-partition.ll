target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-grtev4-linux-gnu"

; RUN: llc -mtriple=x86_64-unknown-linux-gnu -enable-split-machine-functions \
; RUN:     -partition-static-data-sections=true -function-sections=true \
; RUN:     -unique-section-names=false \
; RUN:     %s -o - 2>&1 | FileCheck %s --check-prefix=NUM

; NUM: constantpool

@.str = private unnamed_addr constant [4 x i8] c"%f\0A\00", align 1
@.str.1 = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

define internal fastcc void @_Z8coldFunci(i32 noundef %0) unnamed_addr #0 align 32 !prof !40 {
  %2 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, double noundef 6.800000e-01)
  %3 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %0)
  ret void
}

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #1

; Function Attrs: hot inlinehint mustprogress nofree noinline nounwind uwtable
define internal fastcc void @_Z7hotFunci(i32 noundef %0) unnamed_addr #2 align 32 !prof !41 {
  %2 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, double noundef 6.800000e-01)
  %3 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, double noundef 6.900000e-01)
  %4 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %0)
  ret void
}

; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main(i32 noundef %0, ptr nocapture noundef readnone %1) local_unnamed_addr #3 align 32 !prof !40 {
  %3 = tail call i64 @time(ptr noundef null) #5
  %4 = trunc i64 %3 to i32
  tail call void @srand(i32 noundef %4) #5
  br label %7

5:                                                ; preds = %7
  %6 = tail call i32 @rand() #5
  tail call fastcc void @_Z8coldFunci(i32 noundef %6) #6
  ret i32 0

7:                                                ; preds = %7, %2
  %8 = phi i32 [ 0, %2 ], [ %10, %7 ]
  %9 = tail call i32 @rand() #5
  tail call fastcc void @_Z7hotFunci(i32 noundef %9) #7
  %10 = add nuw nsw i32 %8, 1
  %11 = icmp eq i32 %10, 100000
  br i1 %11, label %5, label %7, !prof !42, !llvm.loop !43
}

; Function Attrs: nounwind
declare void @srand(i32 noundef) local_unnamed_addr #4

; Function Attrs: nounwind
declare i64 @time(ptr noundef) local_unnamed_addr #4

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #4

attributes #0 = { cold mustprogress nofree noinline nounwind optsize uwtable "frame-pointer"="non-leaf" "min-legal-vector-width"="0" "no-trapping-math"="true" "prefer-vector-width"="128" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+aes,+avx,+cmov,+crc32,+cx16,+cx8,+fxsr,+mmx,+pclmul,+popcnt,+prfchw,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" "tune-cpu"="generic" }
attributes #1 = { nofree nounwind "frame-pointer"="non-leaf" "no-trapping-math"="true" "prefer-vector-width"="128" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+aes,+avx,+cmov,+crc32,+cx16,+cx8,+fxsr,+mmx,+pclmul,+popcnt,+prfchw,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" "tune-cpu"="generic" }
attributes #2 = { hot inlinehint mustprogress nofree noinline nounwind uwtable "frame-pointer"="non-leaf" "min-legal-vector-width"="0" "no-trapping-math"="true" "prefer-vector-width"="128" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+aes,+avx,+cmov,+crc32,+cx16,+cx8,+fxsr,+mmx,+pclmul,+popcnt,+prfchw,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" "tune-cpu"="generic" }
attributes #3 = { mustprogress norecurse nounwind uwtable "frame-pointer"="non-leaf" "min-legal-vector-width"="0" "no-trapping-math"="true" "prefer-vector-width"="128" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+aes,+avx,+cmov,+crc32,+cx16,+cx8,+fxsr,+mmx,+pclmul,+popcnt,+prfchw,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" "tune-cpu"="generic" }
attributes #4 = { nounwind "frame-pointer"="non-leaf" "no-trapping-math"="true" "prefer-vector-width"="128" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+aes,+avx,+cmov,+crc32,+cx16,+cx8,+fxsr,+mmx,+pclmul,+popcnt,+prfchw,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave" "tune-cpu"="generic" }
attributes #5 = { nounwind }
attributes #6 = { cold }
attributes #7 = { hot }

!llvm.linker.options = !{}
!llvm.module.flags = !{!0, !1, !2, !3, !4, !5, !6, !7, !8, !9, !10}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"Virtual Function Elim", i32 0}
!2 = !{i32 8, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 1, !"Code Model", i32 3}
!5 = !{i32 1, !"Large Data Threshold", i64 65536}
!6 = !{i32 7, !"direct-access-external-data", i32 1}
!7 = !{i32 7, !"uwtable", i32 2}
!8 = !{i32 7, !"frame-pointer", i32 1}
!9 = !{i32 1, !"EnableSplitLTOUnit", i32 0}
!10 = !{i32 1, !"ProfileSummary", !11}
!11 = !{!12, !13, !14, !15, !16, !17, !18, !19, !20, !21}
!12 = !{!"ProfileFormat", !"InstrProf"}
!13 = !{!"TotalCount", i64 1460617}
!14 = !{!"MaxCount", i64 849536}
!15 = !{!"MaxInternalCount", i64 32769}
!16 = !{!"MaxFunctionCount", i64 849536}
!17 = !{!"NumCounts", i64 23784}
!18 = !{!"NumFunctions", i64 3301}
!19 = !{!"IsPartialProfile", i64 0}
!20 = !{!"PartialProfileRatio", double 0.000000e+00}
!21 = !{!"DetailedSummary", !22}
!22 = !{!23, !24, !25, !26, !27, !28, !29, !30, !31, !32, !33, !34, !35, !36, !37, !38}
!23 = !{i32 10000, i64 849536, i32 1}
!24 = !{i32 100000, i64 849536, i32 1}
!25 = !{i32 200000, i64 849536, i32 1}
!26 = !{i32 300000, i64 849536, i32 1}
!27 = !{i32 400000, i64 849536, i32 1}
!28 = !{i32 500000, i64 849536, i32 1}
!29 = !{i32 600000, i64 100000, i32 3}
!30 = !{i32 700000, i64 100000, i32 3}
!31 = !{i32 800000, i64 32640, i32 10}
!32 = !{i32 900000, i64 26548, i32 11}
!33 = !{i32 950000, i64 7904, i32 18}
!34 = !{i32 990000, i64 166, i32 73}
!35 = !{i32 999000, i64 5, i32 470}
!36 = !{i32 999900, i64 1, i32 1463}
!37 = !{i32 999990, i64 1, i32 1463}
!38 = !{i32 999999, i64 1, i32 1463}
!40 = !{!"function_entry_count", i64 1}
!41 = !{!"function_entry_count", i64 100000}
!42 = !{!"branch_weights", i32 1, i32 99999}
!43 = distinct !{!43, !44}
!44 = !{!"llvm.loop.mustprogress"}
