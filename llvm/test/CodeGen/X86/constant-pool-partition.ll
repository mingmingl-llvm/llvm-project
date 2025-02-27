target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-grtev4-linux-gnu"

; TODO(endianness and constant pool)
; TODO(unknown hotness and unlikely hotness)

; RUN: llc -mtriple=x86_64-unknown-linux-gnu -enable-split-machine-functions \
; RUN:     -partition-static-data-sections=true -function-sections=true \
; RUN:     -unique-section-names=false \
; RUN:     %s -o - 2>&1 | FileCheck %s --check-prefix=NUM

; NUM:     .section	.rodata.cst8.hot,"aM",@progbits,8
; NUM: .LCPI0_0:
; NUM:	   .quad	0x3fe5c28f5c28f5c3              # double 0.68000000000000005
; NUM: 	   .section	.rodata.cst8.unlikely,"aM",@progbits,8
; NUM: .LCPI0_1:
; NUM:	   .quad	0x3eb0000000000000              # double 9.5367431640625E-7

; NUM:	   .section	.rodata.cst8.hot,"aM",@progbits,8
; NUM: .LCPI1_0:
; NUM:     .quad	0x3fe5c28f5c28f5c3              # double 0.68000000000000005
; NUM:     .section        .rodata.cst16.hot,"aM",@progbits,16
; NUM: .LCPI1_1:
; NUM:     .long   2147483648                      # 0x80000000
; NUM:     .long   2147483648                      # 0x80000000
; NUM:     .long   2147483648                      # 0x80000000
; NUM:     .long   2147483648                      # 0x80000000
; NUM: .LCPI1_2:
; NUM:     .long   2147484090                      # 0x800001ba
; NUM:     .long   2147483748                      # 0x80000064
; NUM:     .long   2147483648                      # 0x80000000
; NUM:     .long   2147483648                      # 0x80000000

; NUM:    .section	.rodata.cst32,"aM",@progbits,32
; MUM:    .globl	val

@.str = private unnamed_addr constant [4 x i8] c"%f\0A\00", align 1

define double @cold_func(double %x) !prof !16 {
  %2 = tail call i32 (ptr, ...) @printf(ptr dereferenceable(1) @.str, double 6.800000e-01)
  %y = fmul double %x, 0x3EB0000000000000 
  ret double %y
}

declare i32 @printf(ptr nocapture readonly, ...)

define <4 x i1> @hot_func(i32 %0, <4 x i32> %a) !prof !17 {
  %2 = tail call i32 (ptr, ...) @printf(ptr dereferenceable(1) @.str, double 6.800000e-01)
  %b = icmp ule <4 x i32> %a, <i32 442, i32 100, i32 0, i32 0>
  ret <4 x i1> %b
}

@val = unnamed_addr constant i256 1

define i32 @main(i32 %0, ptr %1) !prof !16 {
  br label %7

5:                                                ; preds = %7
  %x = call double @double_func()
  call void @cold_func(double %x)
  ret i32 0

7:                                                ; preds = %7, %2
  %8 = phi i32 [ 0, %2 ], [ %10, %7 ]
  %9 = call i32 @rand()
  call void @hot_func(i32 %9)
  %10 = add i32 %8, 1
  %11 = icmp eq i32 %10, 100000
  br i1 %11, label %5, label %7, !prof !18
}

declare i32 @rand()
declare double @double_func()
declare <4 x i32> @vector_func()

!llvm.module.flags = !{!1}

!1 = !{i32 1, !"ProfileSummary", !2}
!2 = !{!3, !4, !5, !6, !7, !8, !9, !10, !11, !12}
!3 = !{!"ProfileFormat", !"InstrProf"}
!4 = !{!"TotalCount", i64 1460617}
!5 = !{!"MaxCount", i64 849536}
!6 = !{!"MaxInternalCount", i64 32769}
!7 = !{!"MaxFunctionCount", i64 849536}
!8 = !{!"NumCounts", i64 23784}
!9 = !{!"NumFunctions", i64 3301}
!10 = !{!"IsPartialProfile", i64 0}
!11 = !{!"PartialProfileRatio", double 0.000000e+00}
!12 = !{!"DetailedSummary", !13}
!13 = !{!14, !15}
!14 = !{i32 990000, i64 166, i32 73}
!15 = !{i32 999999, i64 1, i32 1463}
!16 = !{!"function_entry_count", i64 1}
!17 = !{!"function_entry_count", i64 100000}
!18 = !{!"branch_weights", i32 1, i32 99999}
