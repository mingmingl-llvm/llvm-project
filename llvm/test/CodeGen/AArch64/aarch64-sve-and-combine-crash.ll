; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc -mtriple=aarch64-linux-gnu -mattr=+sve < %s | FileCheck %s

target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-unknown-linux-gnu"

; Check that this test does not crash at performSVEAndCombine.

define <vscale x 4 x i32> @test(<vscale x 8 x i16> %in1, <vscale x 4 x i32> %in2) {
; CHECK-LABEL: test:
; CHECK:       // %bb.0: // %entry
; CHECK-NEXT:    uunpkhi z0.s, z0.h
; CHECK-NEXT:    mov z1.s, s1
; CHECK-NEXT:    and z0.d, z0.d, z1.d
; CHECK-NEXT:    ret
entry:
  %i1 = call <vscale x 4 x i32> @llvm.aarch64.sve.uunpkhi.nxv4i32(<vscale x 8 x i16> %in1)
  %i2 = shufflevector <vscale x 4 x i32> %in2, <vscale x 4 x i32> poison, <vscale x 4 x i32> zeroinitializer
  %i3 = and <vscale x 4 x i32> %i1, %i2
  ret <vscale x 4 x i32> %i3
}

declare <vscale x 4 x i32> @llvm.aarch64.sve.uunpkhi.nxv4i32(<vscale x 8 x i16>)
