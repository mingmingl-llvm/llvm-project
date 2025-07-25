; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: opt -passes=instcombine -S < %s | FileCheck %s

declare double @llvm.powi.f64.i32(double, i32)
declare float @llvm.powi.f32.i32(float, i32)
declare double @llvm.powi.f64.i64(double, i64)
declare double @llvm.fabs.f64(double)
declare double @llvm.copysign.f64(double, double)
declare void @use(double)

define double @powi_fneg_even_int(double %x) {
; CHECK-LABEL: @powi_fneg_even_int(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[R:%.*]] = tail call double @llvm.powi.f64.i32(double [[X:%.*]], i32 4)
; CHECK-NEXT:    ret double [[R]]
;
entry:
  %fneg = fneg double %x
  %r = tail call double @llvm.powi.f64.i32(double %fneg, i32 4)
  ret double %r
}

define double @powi_fabs_even_int(double %x) {
; CHECK-LABEL: @powi_fabs_even_int(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[R:%.*]] = tail call double @llvm.powi.f64.i32(double [[X:%.*]], i32 4)
; CHECK-NEXT:    ret double [[R]]
;
entry:
  %f = tail call double @llvm.fabs.f64(double %x)
  %r = tail call double @llvm.powi.f64.i32(double %f, i32 4)
  ret double %r
}

define double @powi_copysign_even_int(double %x, double %y) {
; CHECK-LABEL: @powi_copysign_even_int(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[R:%.*]] = tail call double @llvm.powi.f64.i32(double [[X:%.*]], i32 4)
; CHECK-NEXT:    ret double [[R]]
;
entry:
  %cs = tail call double @llvm.copysign.f64(double %x, double %y)
  %r = tail call double @llvm.powi.f64.i32(double %cs, i32 4)
  ret double %r
}

define double @powi_fneg_odd_int(double %x) {
; CHECK-LABEL: @powi_fneg_odd_int(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[FNEG:%.*]] = fneg double [[X:%.*]]
; CHECK-NEXT:    [[R:%.*]] = tail call double @llvm.powi.f64.i32(double [[FNEG]], i32 5)
; CHECK-NEXT:    ret double [[R]]
;
entry:
  %fneg = fneg double %x
  %r = tail call double @llvm.powi.f64.i32(double %fneg, i32 5)
  ret double %r
}

define double @powi_fabs_odd_int(double %x) {
; CHECK-LABEL: @powi_fabs_odd_int(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[F:%.*]] = tail call double @llvm.fabs.f64(double [[X:%.*]])
; CHECK-NEXT:    [[R:%.*]] = tail call double @llvm.powi.f64.i32(double [[F]], i32 5)
; CHECK-NEXT:    ret double [[R]]
;
entry:
  %f = tail call double @llvm.fabs.f64(double %x)
  %r = tail call double @llvm.powi.f64.i32(double %f, i32 5)
  ret double %r
}

define double @powi_copysign_odd_int(double %x, double %y) {
; CHECK-LABEL: @powi_copysign_odd_int(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[CS:%.*]] = tail call double @llvm.copysign.f64(double [[X:%.*]], double [[Y:%.*]])
; CHECK-NEXT:    [[R:%.*]] = tail call double @llvm.powi.f64.i32(double [[CS]], i32 5)
; CHECK-NEXT:    ret double [[R]]
;
entry:
  %cs = tail call double @llvm.copysign.f64(double %x, double %y)
  %r = tail call double @llvm.powi.f64.i32(double %cs, i32 5)
  ret double %r
}

define double @powi_fmul_arg0_no_reassoc(double %x, i32 %i) {
; CHECK-LABEL: @powi_fmul_arg0_no_reassoc(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[POW:%.*]] = tail call double @llvm.powi.f64.i32(double [[X:%.*]], i32 [[I:%.*]])
; CHECK-NEXT:    [[MUL:%.*]] = fmul double [[POW]], [[X]]
; CHECK-NEXT:    ret double [[MUL]]
;
entry:
  %pow = tail call double @llvm.powi.f64.i32(double %x, i32 %i)
  %mul = fmul double %pow, %x
  ret double %mul
}


define double @powi_fmul_arg0(double %x, i32 %i) {
; CHECK-LABEL: @powi_fmul_arg0(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[POW:%.*]] = tail call double @llvm.powi.f64.i32(double [[X:%.*]], i32 [[I:%.*]])
; CHECK-NEXT:    [[MUL:%.*]] = fmul reassoc double [[POW]], [[X]]
; CHECK-NEXT:    ret double [[MUL]]
;
entry:
  %pow = tail call double @llvm.powi.f64.i32(double %x, i32 %i)
  %mul = fmul reassoc double %pow, %x
  ret double %mul
}

define double @powi_fmul_arg0_use(double %x, i32 %i) {
; CHECK-LABEL: @powi_fmul_arg0_use(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[POW:%.*]] = tail call double @llvm.powi.f64.i32(double [[X:%.*]], i32 [[I:%.*]])
; CHECK-NEXT:    tail call void @use(double [[POW]])
; CHECK-NEXT:    [[MUL:%.*]] = fmul reassoc double [[POW]], [[X]]
; CHECK-NEXT:    ret double [[MUL]]
;
entry:
  %pow = tail call double @llvm.powi.f64.i32(double %x, i32 %i)
  tail call void @use(double %pow)
  %mul = fmul reassoc double %pow, %x
  ret double %mul
}

; Negative test: Missing reassoc flag on fmul
define double @powi_fmul_powi_no_reassoc1(double %x, i32 %y, i32 %z) {
; CHECK-LABEL: @powi_fmul_powi_no_reassoc1(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[P1:%.*]] = tail call reassoc double @llvm.powi.f64.i32(double [[X:%.*]], i32 [[Y:%.*]])
; CHECK-NEXT:    [[P2:%.*]] = tail call reassoc double @llvm.powi.f64.i32(double [[X]], i32 [[Z:%.*]])
; CHECK-NEXT:    [[MUL:%.*]] = fmul double [[P2]], [[P1]]
; CHECK-NEXT:    ret double [[MUL]]
;
entry:
  %p1 = tail call reassoc double @llvm.powi.f64.i32(double %x, i32 %y)
  %p2 = tail call reassoc double @llvm.powi.f64.i32(double %x, i32 %z)
  %mul = fmul double %p2, %p1
  ret double %mul
}

; Negative test: Missing reassoc flag on 2nd operand
define double @powi_fmul_powi_no_reassoc2(double %x, i32 %y, i32 %z) {
; CHECK-LABEL: @powi_fmul_powi_no_reassoc2(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[P1:%.*]] = tail call reassoc double @llvm.powi.f64.i32(double [[X:%.*]], i32 [[Y:%.*]])
; CHECK-NEXT:    [[P2:%.*]] = tail call double @llvm.powi.f64.i32(double [[X]], i32 [[Z:%.*]])
; CHECK-NEXT:    [[MUL:%.*]] = fmul reassoc double [[P2]], [[P1]]
; CHECK-NEXT:    ret double [[MUL]]
;
entry:
  %p1 = tail call reassoc double @llvm.powi.f64.i32(double %x, i32 %y)
  %p2 = tail call double @llvm.powi.f64.i32(double %x, i32 %z)
  %mul = fmul reassoc double %p2, %p1
  ret double %mul
}

; Negative test: Missing reassoc flag on 1st operand
define double @powi_fmul_powi_no_reassoc3(double %x, i32 %y, i32 %z) {
; CHECK-LABEL: @powi_fmul_powi_no_reassoc3(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[P1:%.*]] = tail call double @llvm.powi.f64.i32(double [[X:%.*]], i32 [[Y:%.*]])
; CHECK-NEXT:    [[P2:%.*]] = tail call reassoc double @llvm.powi.f64.i32(double [[X]], i32 [[Z:%.*]])
; CHECK-NEXT:    [[MUL:%.*]] = fmul reassoc double [[P2]], [[P1]]
; CHECK-NEXT:    ret double [[MUL]]
;
entry:
  %p1 = tail call double @llvm.powi.f64.i32(double %x, i32 %y)
  %p2 = tail call reassoc double @llvm.powi.f64.i32(double %x, i32 %z)
  %mul = fmul reassoc double %p2, %p1
  ret double %mul
}

; All of the fmul and its operands should have the reassoc flags
define double @powi_fmul_powi(double %x, i32 %y, i32 %z) {
; CHECK-LABEL: @powi_fmul_powi(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[TMP0:%.*]] = add i32 [[Z:%.*]], [[Y:%.*]]
; CHECK-NEXT:    [[MUL:%.*]] = call reassoc double @llvm.powi.f64.i32(double [[X:%.*]], i32 [[TMP0]])
; CHECK-NEXT:    ret double [[MUL]]
;
entry:
  %p1 = tail call reassoc double @llvm.powi.f64.i32(double %x, i32 %y)
  %p2 = tail call reassoc double @llvm.powi.f64.i32(double %x, i32 %z)
  %mul = fmul reassoc double %p2, %p1
  ret double %mul
}

define double @powi_fmul_powi_fast_on_fmul(double %x, i32 %y, i32 %z) {
; CHECK-LABEL: @powi_fmul_powi_fast_on_fmul(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[TMP0:%.*]] = add i32 [[Z:%.*]], [[Y:%.*]]
; CHECK-NEXT:    [[MUL:%.*]] = call fast double @llvm.powi.f64.i32(double [[X:%.*]], i32 [[TMP0]])
; CHECK-NEXT:    ret double [[MUL]]
;
entry:
  %p1 = tail call fast double @llvm.powi.f64.i32(double %x, i32 %y)
  %p2 = tail call fast double @llvm.powi.f64.i32(double %x, i32 %z)
  %mul = fmul fast double %p2, %p1
  ret double %mul
}

define double @powi_fmul_powi_fast_on_powi(double %x, i32 %y, i32 %z) {
; CHECK-LABEL: @powi_fmul_powi_fast_on_powi(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[P1:%.*]] = tail call fast double @llvm.powi.f64.i32(double [[X:%.*]], i32 [[Y:%.*]])
; CHECK-NEXT:    [[P2:%.*]] = tail call fast double @llvm.powi.f64.i32(double [[X]], i32 [[Z:%.*]])
; CHECK-NEXT:    [[MUL:%.*]] = fmul double [[P2]], [[P1]]
; CHECK-NEXT:    ret double [[MUL]]
;
entry:
  %p1 = tail call fast double @llvm.powi.f64.i32(double %x, i32 %y)
  %p2 = tail call fast double @llvm.powi.f64.i32(double %x, i32 %z)
  %mul = fmul double %p2, %p1
  ret double %mul
}

define double @powi_fmul_powi_same_power(double %x, i32 %y, i32 %z) {
; CHECK-LABEL: @powi_fmul_powi_same_power(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[TMP0:%.*]] = shl i32 [[Y:%.*]], 1
; CHECK-NEXT:    [[MUL:%.*]] = call reassoc double @llvm.powi.f64.i32(double [[X:%.*]], i32 [[TMP0]])
; CHECK-NEXT:    ret double [[MUL]]
;
entry:
  %p1 = tail call reassoc double @llvm.powi.f64.i32(double %x, i32 %y)
  %p2 = tail call reassoc double @llvm.powi.f64.i32(double %x, i32 %y)
  %mul = fmul reassoc double %p2, %p1
  ret double %mul
}

define double @powi_fmul_powi_different_integer_types(double %x, i32 %y, i16 %z) {
; CHECK-LABEL: @powi_fmul_powi_different_integer_types(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[P1:%.*]] = tail call reassoc double @llvm.powi.f64.i32(double [[X:%.*]], i32 [[Y:%.*]])
; CHECK-NEXT:    [[P2:%.*]] = tail call reassoc double @llvm.powi.f64.i16(double [[X]], i16 [[Z:%.*]])
; CHECK-NEXT:    [[MUL:%.*]] = fmul reassoc double [[P2]], [[P1]]
; CHECK-NEXT:    ret double [[MUL]]
;
entry:
  %p1 = tail call reassoc double @llvm.powi.f64.i32(double %x, i32 %y)
  %p2 = tail call reassoc double @llvm.powi.f64.i16(double %x, i16 %z)
  %mul = fmul reassoc double %p2, %p1
  ret double %mul
}

define double @powi_fmul_powi_use_first(double %x, i32 %y, i32 %z) {
; CHECK-LABEL: @powi_fmul_powi_use_first(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[P1:%.*]] = tail call reassoc double @llvm.powi.f64.i32(double [[X:%.*]], i32 [[Y:%.*]])
; CHECK-NEXT:    tail call void @use(double [[P1]])
; CHECK-NEXT:    [[TMP0:%.*]] = add i32 [[Y]], [[Z:%.*]]
; CHECK-NEXT:    [[MUL:%.*]] = call reassoc double @llvm.powi.f64.i32(double [[X]], i32 [[TMP0]])
; CHECK-NEXT:    ret double [[MUL]]
;
entry:
  %p1 = tail call reassoc double @llvm.powi.f64.i32(double %x, i32 %y)
  tail call void @use(double %p1)
  %p2 = tail call reassoc double @llvm.powi.f64.i32(double %x, i32 %z)
  %mul = fmul reassoc double %p1, %p2
  ret double %mul
}

define double @powi_fmul_powi_use_second(double %x, i32 %y, i32 %z) {
; CHECK-LABEL: @powi_fmul_powi_use_second(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[P1:%.*]] = tail call reassoc double @llvm.powi.f64.i32(double [[X:%.*]], i32 [[Z:%.*]])
; CHECK-NEXT:    tail call void @use(double [[P1]])
; CHECK-NEXT:    [[TMP0:%.*]] = add i32 [[Y:%.*]], [[Z]]
; CHECK-NEXT:    [[MUL:%.*]] = call reassoc double @llvm.powi.f64.i32(double [[X]], i32 [[TMP0]])
; CHECK-NEXT:    ret double [[MUL]]
;
entry:
  %p1 = tail call reassoc double @llvm.powi.f64.i32(double %x, i32 %z)
  tail call void @use(double %p1)
  %p2 = tail call reassoc double @llvm.powi.f64.i32(double %x, i32 %y)
  %mul = fmul reassoc double %p2, %p1
  ret double %mul
}

define double @powi_fmul_different_base(double %x, double %m, i32 %y, i32 %z) {
; CHECK-LABEL: @powi_fmul_different_base(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[P1:%.*]] = tail call double @llvm.powi.f64.i32(double [[X:%.*]], i32 [[Y:%.*]])
; CHECK-NEXT:    [[P2:%.*]] = tail call double @llvm.powi.f64.i32(double [[M:%.*]], i32 [[Z:%.*]])
; CHECK-NEXT:    [[MUL:%.*]] = fmul reassoc double [[P2]], [[P1]]
; CHECK-NEXT:    ret double [[MUL]]
;
entry:
  %p1 = tail call double @llvm.powi.f64.i32(double %x, i32 %y)
  %p2 = tail call double @llvm.powi.f64.i32(double %m, i32 %z)
  %mul = fmul reassoc double %p2, %p1
  ret double %mul
}

define double @different_types_powi(double %x, i32 %y, i64 %z) {
; CHECK-LABEL: @different_types_powi(
; CHECK-NEXT:    [[P1:%.*]] = tail call double @llvm.powi.f64.i32(double [[X:%.*]], i32 [[Y:%.*]])
; CHECK-NEXT:    [[P2:%.*]] = tail call double @llvm.powi.f64.i64(double [[X]], i64 [[Z:%.*]])
; CHECK-NEXT:    [[MUL:%.*]] = fmul reassoc double [[P2]], [[P1]]
; CHECK-NEXT:    ret double [[MUL]]
;
  %p1 = tail call double @llvm.powi.f64.i32(double %x, i32 %y)
  %p2 = tail call double @llvm.powi.f64.i64(double %x, i64 %z)
  %mul = fmul reassoc double %p2, %p1
  ret double %mul
}

define double @fdiv_pow_powi(double %x) {
; CHECK-LABEL: @fdiv_pow_powi(
; CHECK-NEXT:    [[DIV:%.*]] = fmul reassoc nnan double [[X:%.*]], [[X]]
; CHECK-NEXT:    ret double [[DIV]]
;
  %p1 = call reassoc double @llvm.powi.f64.i32(double %x, i32 3)
  %div = fdiv reassoc nnan double %p1, %x
  ret double %div
}

define float @fdiv_powf_powi(float %x) {
; CHECK-LABEL: @fdiv_powf_powi(
; CHECK-NEXT:    [[DIV:%.*]] = call reassoc nnan float @llvm.powi.f32.i32(float [[X:%.*]], i32 99)
; CHECK-NEXT:    ret float [[DIV]]
;
  %p1 = call reassoc float @llvm.powi.f32.i32(float %x, i32 100)
  %div = fdiv reassoc nnan float %p1, %x
  ret float %div
}

; TODO: Multi-use may be also better off creating Powi(x,y-1) then creating
; (mul, Powi(x,y-1),x) to replace the Powi(x,y).
define double @fdiv_pow_powi_multi_use(double %x) {
; CHECK-LABEL: @fdiv_pow_powi_multi_use(
; CHECK-NEXT:    [[P1:%.*]] = call double @llvm.powi.f64.i32(double [[X:%.*]], i32 3)
; CHECK-NEXT:    [[DIV:%.*]] = fdiv reassoc nnan double [[P1]], [[X]]
; CHECK-NEXT:    tail call void @use(double [[P1]])
; CHECK-NEXT:    ret double [[DIV]]
;
  %p1 = call double @llvm.powi.f64.i32(double %x, i32 3)
  %div = fdiv reassoc nnan double %p1, %x
  tail call void @use(double %p1)
  ret double %div
}

; Negative test: Miss part of the fmf flag for the fdiv instruction
define float @fdiv_powf_powi_missing_reassoc(float %x) {
; CHECK-LABEL: @fdiv_powf_powi_missing_reassoc(
; CHECK-NEXT:    [[P1:%.*]] = call float @llvm.powi.f32.i32(float [[X:%.*]], i32 100)
; CHECK-NEXT:    [[DIV:%.*]] = fdiv reassoc nnan float [[P1]], [[X]]
; CHECK-NEXT:    ret float [[DIV]]
;
  %p1 = call float @llvm.powi.f32.i32(float %x, i32 100)
  %div = fdiv reassoc nnan float %p1, %x
  ret float %div
}

define float @fdiv_powf_powi_missing_reassoc1(float %x) {
; CHECK-LABEL: @fdiv_powf_powi_missing_reassoc1(
; CHECK-NEXT:    [[P1:%.*]] = call reassoc float @llvm.powi.f32.i32(float [[X:%.*]], i32 100)
; CHECK-NEXT:    [[DIV:%.*]] = fdiv nnan float [[P1]], [[X]]
; CHECK-NEXT:    ret float [[DIV]]
;
  %p1 = call reassoc float @llvm.powi.f32.i32(float %x, i32 100)
  %div = fdiv nnan float %p1, %x
  ret float %div
}

define float @fdiv_powf_powi_missing_nnan(float %x) {
; CHECK-LABEL: @fdiv_powf_powi_missing_nnan(
; CHECK-NEXT:    [[P1:%.*]] = call float @llvm.powi.f32.i32(float [[X:%.*]], i32 100)
; CHECK-NEXT:    [[DIV:%.*]] = fdiv reassoc float [[P1]], [[X]]
; CHECK-NEXT:    ret float [[DIV]]
;
  %p1 = call float @llvm.powi.f32.i32(float %x, i32 100)
  %div = fdiv reassoc float %p1, %x
  ret float %div
}

; Negative test: Illegal because (Y - 1) wraparound
define double @fdiv_pow_powi_negative(double %x) {
; CHECK-LABEL: @fdiv_pow_powi_negative(
; CHECK-NEXT:    [[P1:%.*]] = call double @llvm.powi.f64.i32(double [[X:%.*]], i32 -2147483648)
; CHECK-NEXT:    [[DIV:%.*]] = fdiv reassoc nnan double [[P1]], [[X]]
; CHECK-NEXT:    ret double [[DIV]]
;
  %p1 = call double @llvm.powi.f64.i32(double %x, i32 -2147483648) ; INT_MIN
  %div = fdiv reassoc nnan double %p1, %x
  ret double %div
}

; Negative test: The 2nd powi argument is a variable
define double @fdiv_pow_powi_negative_variable(double %x, i32 %y) {
; CHECK-LABEL: @fdiv_pow_powi_negative_variable(
; CHECK-NEXT:    [[P1:%.*]] = call reassoc double @llvm.powi.f64.i32(double [[X:%.*]], i32 [[Y:%.*]])
; CHECK-NEXT:    [[DIV:%.*]] = fdiv reassoc nnan double [[P1]], [[X]]
; CHECK-NEXT:    ret double [[DIV]]
;
  %p1 = call reassoc double @llvm.powi.f64.i32(double %x, i32 %y)
  %div = fdiv reassoc nnan double %p1, %x
  ret double %div
}

; powi(X,C1)/ (X * Z) --> powi(X,C1 - 1)/ Z
define double @fdiv_fmul_powi(double %a, double %z) {
; CHECK-LABEL: @fdiv_fmul_powi(
; CHECK-NEXT:    [[TMP1:%.*]] = call reassoc nnan double @llvm.powi.f64.i32(double [[A:%.*]], i32 4)
; CHECK-NEXT:    [[DIV:%.*]] = fdiv reassoc nnan double [[TMP1]], [[Z:%.*]]
; CHECK-NEXT:    ret double [[DIV]]
;
  %pow = call reassoc double @llvm.powi.f64.i32(double %a, i32 5)
  %square = fmul reassoc double %z, %a
  %div = fdiv reassoc nnan double %pow, %square
  ret double %div
}

; powi(X, 5)/ (X * X) --> powi(X, 4)/ X -> powi(X, 3)
define double @fdiv_fmul_powi_2(double %a) {
; CHECK-LABEL: @fdiv_fmul_powi_2(
; CHECK-NEXT:    [[DIV:%.*]] = call reassoc nnan double @llvm.powi.f64.i32(double [[A:%.*]], i32 3)
; CHECK-NEXT:    ret double [[DIV]]
;
  %pow = call reassoc double @llvm.powi.f64.i32(double %a, i32 5)
  %square = fmul reassoc double %a, %a
  %div = fdiv reassoc nnan double %pow, %square
  ret double %div
}

define <2 x float> @fdiv_fmul_powi_vector(<2 x float> %a) {
; CHECK-LABEL: @fdiv_fmul_powi_vector(
; CHECK-NEXT:    [[DIV:%.*]] = call reassoc nnan <2 x float> @llvm.powi.v2f32.i32(<2 x float> [[A:%.*]], i32 3)
; CHECK-NEXT:    ret <2 x float> [[DIV]]
;
  %pow = call reassoc <2 x float> @llvm.powi.v2f32.i32(<2 x float> %a, i32 5)
  %square = fmul reassoc <2 x float> %a, %a
  %div = fdiv reassoc nnan <2 x float> %pow, %square
  ret <2 x float> %div
}

; Negative test
define double @fdiv_fmul_powi_missing_reassoc1(double %a) {
; CHECK-LABEL: @fdiv_fmul_powi_missing_reassoc1(
; CHECK-NEXT:    [[POW:%.*]] = call reassoc double @llvm.powi.f64.i32(double [[A:%.*]], i32 5)
; CHECK-NEXT:    [[SQUARE:%.*]] = fmul reassoc double [[A]], [[A]]
; CHECK-NEXT:    [[DIV:%.*]] = fdiv nnan double [[POW]], [[SQUARE]]
; CHECK-NEXT:    ret double [[DIV]]
;
  %pow = call reassoc double @llvm.powi.f64.i32(double %a, i32 5)
  %square = fmul reassoc double %a, %a
  %div = fdiv nnan double %pow, %square
  ret double %div
}

define double @fdiv_fmul_powi_missing_reassoc2(double %a) {
; CHECK-LABEL: @fdiv_fmul_powi_missing_reassoc2(
; CHECK-NEXT:    [[POW:%.*]] = call reassoc double @llvm.powi.f64.i32(double [[A:%.*]], i32 5)
; CHECK-NEXT:    [[SQUARE:%.*]] = fmul double [[A]], [[A]]
; CHECK-NEXT:    [[DIV:%.*]] = fdiv reassoc nnan double [[POW]], [[SQUARE]]
; CHECK-NEXT:    ret double [[DIV]]
;
  %pow = call reassoc double @llvm.powi.f64.i32(double %a, i32 5)
  %square = fmul double %a, %a
  %div = fdiv reassoc nnan double %pow, %square
  ret double %div
}

define double @fdiv_fmul_powi_missing_reassoc3(double %a) {
; CHECK-LABEL: @fdiv_fmul_powi_missing_reassoc3(
; CHECK-NEXT:    [[POW:%.*]] = call double @llvm.powi.f64.i32(double [[A:%.*]], i32 5)
; CHECK-NEXT:    [[SQUARE:%.*]] = fmul reassoc double [[A]], [[A]]
; CHECK-NEXT:    [[DIV:%.*]] = fdiv reassoc nnan double [[POW]], [[SQUARE]]
; CHECK-NEXT:    ret double [[DIV]]
;
  %pow = call double @llvm.powi.f64.i32(double %a, i32 5)
  %square = fmul reassoc double %a, %a
  %div = fdiv reassoc nnan double %pow, %square
  ret double %div
}

define double @fdiv_fmul_powi_missing_nnan(double %a) {
; CHECK-LABEL: @fdiv_fmul_powi_missing_nnan(
; CHECK-NEXT:    [[POW:%.*]] = call reassoc double @llvm.powi.f64.i32(double [[A:%.*]], i32 5)
; CHECK-NEXT:    [[SQUARE:%.*]] = fmul reassoc double [[A]], [[A]]
; CHECK-NEXT:    [[DIV:%.*]] = fdiv reassoc double [[POW]], [[SQUARE]]
; CHECK-NEXT:    ret double [[DIV]]
;
  %pow = call reassoc double @llvm.powi.f64.i32(double %a, i32 5)
  %square = fmul reassoc double %a, %a
  %div = fdiv reassoc double %pow, %square
  ret double %div
}

define double @fdiv_fmul_powi_negative_wrap(double noundef %x) {
; CHECK-LABEL: @fdiv_fmul_powi_negative_wrap(
; CHECK-NEXT:    [[P1:%.*]] = tail call double @llvm.powi.f64.i32(double [[X:%.*]], i32 -2147483648)
; CHECK-NEXT:    [[MUL:%.*]] = fmul reassoc double [[P1]], [[X]]
; CHECK-NEXT:    ret double [[MUL]]
;
  %p1 = tail call double @llvm.powi.f64.i32(double %x, i32 -2147483648) ; INT_MIN
  %mul = fmul reassoc double %p1, %x
  ret double %mul
}

define double @fdiv_fmul_powi_multi_use(double %a) {
; CHECK-LABEL: @fdiv_fmul_powi_multi_use(
; CHECK-NEXT:    [[POW:%.*]] = call reassoc double @llvm.powi.f64.i32(double [[A:%.*]], i32 5)
; CHECK-NEXT:    tail call void @use(double [[POW]])
; CHECK-NEXT:    [[SQUARE:%.*]] = fmul reassoc double [[A]], [[A]]
; CHECK-NEXT:    [[DIV:%.*]] = fdiv reassoc nnan double [[POW]], [[SQUARE]]
; CHECK-NEXT:    ret double [[DIV]]
;
  %pow = call reassoc double @llvm.powi.f64.i32(double %a, i32 5)
  tail call void @use(double %pow)
  %square = fmul reassoc double %a, %a
  %div = fdiv reassoc nnan double %pow, %square
  ret double %div
}

; powi(X, Y) * X --> powi(X, Y+1)
define double @powi_fmul_powi_x(double noundef %x) {
; CHECK-LABEL: @powi_fmul_powi_x(
; CHECK-NEXT:    [[MUL:%.*]] = call reassoc double @llvm.powi.f64.i32(double [[X:%.*]], i32 4)
; CHECK-NEXT:    ret double [[MUL]]
;
  %p1 = tail call reassoc double @llvm.powi.f64.i32(double %x, i32 3)
  %mul = fmul reassoc double %p1, %x
  ret double %mul
}

; Negative test: Multi-use
define double @powi_fmul_powi_x_multi_use(double noundef %x) {
; CHECK-LABEL: @powi_fmul_powi_x_multi_use(
; CHECK-NEXT:    [[P1:%.*]] = tail call double @llvm.powi.f64.i32(double [[X:%.*]], i32 3)
; CHECK-NEXT:    tail call void @use(double [[P1]])
; CHECK-NEXT:    [[MUL:%.*]] = fmul reassoc double [[P1]], [[X]]
; CHECK-NEXT:    ret double [[MUL]]
;
  %p1 = tail call double @llvm.powi.f64.i32(double %x, i32 3)
  tail call void @use(double %p1)
  %mul = fmul reassoc double %p1, %x
  ret double %mul
}

; Negative test: Miss fmf flag
define double @powi_fmul_powi_x_missing_reassoc(double noundef %x) {
; CHECK-LABEL: @powi_fmul_powi_x_missing_reassoc(
; CHECK-NEXT:    [[P1:%.*]] = tail call double @llvm.powi.f64.i32(double [[X:%.*]], i32 3)
; CHECK-NEXT:    [[MUL:%.*]] = fmul double [[P1]], [[X]]
; CHECK-NEXT:    ret double [[MUL]]
;
  %p1 = tail call double @llvm.powi.f64.i32(double %x, i32 3)
  %mul = fmul double %p1, %x
  ret double %mul
}

; Negative test: overflow
define double @powi_fmul_powi_x_overflow(double noundef %x) {
; CHECK-LABEL: @powi_fmul_powi_x_overflow(
; CHECK-NEXT:    [[P1:%.*]] = tail call double @llvm.powi.f64.i32(double [[X:%.*]], i32 2147483647)
; CHECK-NEXT:    [[MUL:%.*]] = fmul reassoc double [[P1]], [[X]]
; CHECK-NEXT:    ret double [[MUL]]
;
  %p1 = tail call double @llvm.powi.f64.i32(double %x, i32 2147483647) ; INT_MAX
  %mul = fmul reassoc double %p1, %x
  ret double %mul
}

define <3 x float> @powi_unary_shuffle_ops(<3 x float> %x, i32 %power) {
; CHECK-LABEL: @powi_unary_shuffle_ops(
; CHECK-NEXT:    [[TMP1:%.*]] = call <3 x float> @llvm.powi.v3f32.i32(<3 x float> [[X:%.*]], i32 [[POWER:%.*]])
; CHECK-NEXT:    [[R:%.*]] = shufflevector <3 x float> [[TMP1]], <3 x float> poison, <3 x i32> <i32 1, i32 0, i32 2>
; CHECK-NEXT:    ret <3 x float> [[R]]
;
  %sx = shufflevector <3 x float> %x, <3 x float> poison, <3 x i32> <i32 1, i32 0, i32 2>
  %r = call <3 x float> @llvm.powi(<3 x float> %sx, i32 %power)
  ret <3 x float> %r
}

; Negative test - multiple uses

define <3 x float> @powi_unary_shuffle_ops_use(<3 x float> %x, i32 %power, ptr %p) {
; CHECK-LABEL: @powi_unary_shuffle_ops_use(
; CHECK-NEXT:    [[SX:%.*]] = shufflevector <3 x float> [[X:%.*]], <3 x float> poison, <3 x i32> <i32 1, i32 0, i32 2>
; CHECK-NEXT:    store <3 x float> [[SX]], ptr [[P:%.*]], align 16
; CHECK-NEXT:    [[R:%.*]] = call <3 x float> @llvm.powi.v3f32.i32(<3 x float> [[SX]], i32 [[POWER:%.*]])
; CHECK-NEXT:    ret <3 x float> [[R]]
;
  %sx = shufflevector <3 x float> %x, <3 x float> poison, <3 x i32> <i32 1, i32 0, i32 2>
  store <3 x float> %sx, ptr %p
  %r = call <3 x float> @llvm.powi(<3 x float> %sx, i32 %power)
  ret <3 x float> %r
}
