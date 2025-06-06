; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc --mtriple=loongarch32 -mattr=-32s,+d < %s | FileCheck %s --check-prefix=LA32R
; RUN: llc --mtriple=loongarch32 -mattr=+32s,+d < %s | FileCheck %s --check-prefix=LA32S
; RUN: llc --mtriple=loongarch64 -mattr=+d < %s | FileCheck %s --check-prefix=LA64

;; Exercise the 'lshr' LLVM IR: https://llvm.org/docs/LangRef.html#lshr-instruction

define i1 @lshr_i1(i1 %x, i1 %y) {
; LA32R-LABEL: lshr_i1:
; LA32R:       # %bb.0:
; LA32R-NEXT:    ret
;
; LA32S-LABEL: lshr_i1:
; LA32S:       # %bb.0:
; LA32S-NEXT:    ret
;
; LA64-LABEL: lshr_i1:
; LA64:       # %bb.0:
; LA64-NEXT:    ret
  %lshr = lshr i1 %x, %y
  ret i1 %lshr
}

define i8 @lshr_i8(i8 %x, i8 %y) {
; LA32R-LABEL: lshr_i8:
; LA32R:       # %bb.0:
; LA32R-NEXT:    andi $a0, $a0, 255
; LA32R-NEXT:    srl.w $a0, $a0, $a1
; LA32R-NEXT:    ret
;
; LA32S-LABEL: lshr_i8:
; LA32S:       # %bb.0:
; LA32S-NEXT:    andi $a0, $a0, 255
; LA32S-NEXT:    srl.w $a0, $a0, $a1
; LA32S-NEXT:    ret
;
; LA64-LABEL: lshr_i8:
; LA64:       # %bb.0:
; LA64-NEXT:    andi $a0, $a0, 255
; LA64-NEXT:    srl.d $a0, $a0, $a1
; LA64-NEXT:    ret
  %lshr = lshr i8 %x, %y
  ret i8 %lshr
}

define i16 @lshr_i16(i16 %x, i16 %y) {
; LA32R-LABEL: lshr_i16:
; LA32R:       # %bb.0:
; LA32R-NEXT:    lu12i.w $a2, 15
; LA32R-NEXT:    ori $a2, $a2, 4095
; LA32R-NEXT:    and $a0, $a0, $a2
; LA32R-NEXT:    srl.w $a0, $a0, $a1
; LA32R-NEXT:    ret
;
; LA32S-LABEL: lshr_i16:
; LA32S:       # %bb.0:
; LA32S-NEXT:    bstrpick.w $a0, $a0, 15, 0
; LA32S-NEXT:    srl.w $a0, $a0, $a1
; LA32S-NEXT:    ret
;
; LA64-LABEL: lshr_i16:
; LA64:       # %bb.0:
; LA64-NEXT:    bstrpick.d $a0, $a0, 15, 0
; LA64-NEXT:    srl.d $a0, $a0, $a1
; LA64-NEXT:    ret
  %lshr = lshr i16 %x, %y
  ret i16 %lshr
}

define i32 @lshr_i32(i32 %x, i32 %y) {
; LA32R-LABEL: lshr_i32:
; LA32R:       # %bb.0:
; LA32R-NEXT:    srl.w $a0, $a0, $a1
; LA32R-NEXT:    ret
;
; LA32S-LABEL: lshr_i32:
; LA32S:       # %bb.0:
; LA32S-NEXT:    srl.w $a0, $a0, $a1
; LA32S-NEXT:    ret
;
; LA64-LABEL: lshr_i32:
; LA64:       # %bb.0:
; LA64-NEXT:    srl.w $a0, $a0, $a1
; LA64-NEXT:    ret
  %lshr = lshr i32 %x, %y
  ret i32 %lshr
}

define i64 @lshr_i64(i64 %x, i64 %y) {
; LA32R-LABEL: lshr_i64:
; LA32R:       # %bb.0:
; LA32R-NEXT:    addi.w $a3, $a2, -32
; LA32R-NEXT:    bltz $a3, .LBB4_2
; LA32R-NEXT:  # %bb.1:
; LA32R-NEXT:    srl.w $a0, $a1, $a3
; LA32R-NEXT:    b .LBB4_3
; LA32R-NEXT:  .LBB4_2:
; LA32R-NEXT:    srl.w $a0, $a0, $a2
; LA32R-NEXT:    xori $a4, $a2, 31
; LA32R-NEXT:    slli.w $a5, $a1, 1
; LA32R-NEXT:    sll.w $a4, $a5, $a4
; LA32R-NEXT:    or $a0, $a0, $a4
; LA32R-NEXT:  .LBB4_3:
; LA32R-NEXT:    slti $a3, $a3, 0
; LA32R-NEXT:    sub.w $a3, $zero, $a3
; LA32R-NEXT:    srl.w $a1, $a1, $a2
; LA32R-NEXT:    and $a1, $a3, $a1
; LA32R-NEXT:    ret
;
; LA32S-LABEL: lshr_i64:
; LA32S:       # %bb.0:
; LA32S-NEXT:    srl.w $a0, $a0, $a2
; LA32S-NEXT:    xori $a3, $a2, 31
; LA32S-NEXT:    slli.w $a4, $a1, 1
; LA32S-NEXT:    sll.w $a3, $a4, $a3
; LA32S-NEXT:    or $a0, $a0, $a3
; LA32S-NEXT:    addi.w $a3, $a2, -32
; LA32S-NEXT:    slti $a4, $a3, 0
; LA32S-NEXT:    maskeqz $a0, $a0, $a4
; LA32S-NEXT:    srl.w $a5, $a1, $a3
; LA32S-NEXT:    masknez $a4, $a5, $a4
; LA32S-NEXT:    or $a0, $a0, $a4
; LA32S-NEXT:    srl.w $a1, $a1, $a2
; LA32S-NEXT:    srai.w $a2, $a3, 31
; LA32S-NEXT:    and $a1, $a2, $a1
; LA32S-NEXT:    ret
;
; LA64-LABEL: lshr_i64:
; LA64:       # %bb.0:
; LA64-NEXT:    srl.d $a0, $a0, $a1
; LA64-NEXT:    ret
  %lshr = lshr i64 %x, %y
  ret i64 %lshr
}

define i1 @lshr_i1_3(i1 %x) {
; LA32R-LABEL: lshr_i1_3:
; LA32R:       # %bb.0:
; LA32R-NEXT:    ret
;
; LA32S-LABEL: lshr_i1_3:
; LA32S:       # %bb.0:
; LA32S-NEXT:    ret
;
; LA64-LABEL: lshr_i1_3:
; LA64:       # %bb.0:
; LA64-NEXT:    ret
  %lshr = lshr i1 %x, 3
  ret i1 %lshr
}

define i8 @lshr_i8_3(i8 %x) {
; LA32R-LABEL: lshr_i8_3:
; LA32R:       # %bb.0:
; LA32R-NEXT:    andi $a0, $a0, 248
; LA32R-NEXT:    srli.w $a0, $a0, 3
; LA32R-NEXT:    ret
;
; LA32S-LABEL: lshr_i8_3:
; LA32S:       # %bb.0:
; LA32S-NEXT:    bstrpick.w $a0, $a0, 7, 3
; LA32S-NEXT:    ret
;
; LA64-LABEL: lshr_i8_3:
; LA64:       # %bb.0:
; LA64-NEXT:    bstrpick.d $a0, $a0, 7, 3
; LA64-NEXT:    ret
  %lshr = lshr i8 %x, 3
  ret i8 %lshr
}

define i16 @lshr_i16_3(i16 %x) {
; LA32R-LABEL: lshr_i16_3:
; LA32R:       # %bb.0:
; LA32R-NEXT:    lu12i.w $a1, 15
; LA32R-NEXT:    ori $a1, $a1, 4088
; LA32R-NEXT:    and $a0, $a0, $a1
; LA32R-NEXT:    srli.w $a0, $a0, 3
; LA32R-NEXT:    ret
;
; LA32S-LABEL: lshr_i16_3:
; LA32S:       # %bb.0:
; LA32S-NEXT:    bstrpick.w $a0, $a0, 15, 3
; LA32S-NEXT:    ret
;
; LA64-LABEL: lshr_i16_3:
; LA64:       # %bb.0:
; LA64-NEXT:    bstrpick.d $a0, $a0, 15, 3
; LA64-NEXT:    ret
  %lshr = lshr i16 %x, 3
  ret i16 %lshr
}

define i32 @lshr_i32_3(i32 %x) {
; LA32R-LABEL: lshr_i32_3:
; LA32R:       # %bb.0:
; LA32R-NEXT:    srli.w $a0, $a0, 3
; LA32R-NEXT:    ret
;
; LA32S-LABEL: lshr_i32_3:
; LA32S:       # %bb.0:
; LA32S-NEXT:    srli.w $a0, $a0, 3
; LA32S-NEXT:    ret
;
; LA64-LABEL: lshr_i32_3:
; LA64:       # %bb.0:
; LA64-NEXT:    bstrpick.d $a0, $a0, 31, 3
; LA64-NEXT:    ret
  %lshr = lshr i32 %x, 3
  ret i32 %lshr
}

define i64 @lshr_i64_3(i64 %x) {
; LA32R-LABEL: lshr_i64_3:
; LA32R:       # %bb.0:
; LA32R-NEXT:    slli.w $a2, $a1, 29
; LA32R-NEXT:    srli.w $a0, $a0, 3
; LA32R-NEXT:    or $a0, $a0, $a2
; LA32R-NEXT:    srli.w $a1, $a1, 3
; LA32R-NEXT:    ret
;
; LA32S-LABEL: lshr_i64_3:
; LA32S:       # %bb.0:
; LA32S-NEXT:    slli.w $a2, $a1, 29
; LA32S-NEXT:    srli.w $a0, $a0, 3
; LA32S-NEXT:    or $a0, $a0, $a2
; LA32S-NEXT:    srli.w $a1, $a1, 3
; LA32S-NEXT:    ret
;
; LA64-LABEL: lshr_i64_3:
; LA64:       # %bb.0:
; LA64-NEXT:    srli.d $a0, $a0, 3
; LA64-NEXT:    ret
  %lshr = lshr i64 %x, 3
  ret i64 %lshr
}
