
; RUN: rm -rf %t && mkdir %t && cd %t

; Generate unsplit module with summary for ThinLTO index-based WPD.
; RUN: opt -thinlto-bc -o summary.o %s

; RUN: llvm-dis -o - summary.o

;; Index based WPD
; RUN: ld.lld summary.o -o tmp -save-temps --lto-whole-program-visibility --lto-validate-all-vtables-have-type-infos -plugin-opt=thinlto \
; RUN:  --lto-emit-llvm \
; RUN:   -mllvm -pass-remarks=. \
; RUN:  --undefined=__cxa_pure_virtual \
; RUN:  --undefined=_ZdlPvm \
; RUN   --undefined=_Znwm  \ 
; RUN:  --undefined=_ZTI7Derived \
; RUN:  --undefined-glob create* \
; RUN:   --export-dynamic-symbol=_ZTV7Derived 2>&1 | FileCheck %s --check-prefix=REMARK

; REMARK: lib.h:17:32: single-impl: devirtualized a call to _ZN8DerivedN5printEv

source_filename = "main.cc"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

$_ZN8DerivedNC2Ev = comdat any

$_ZN7DerivedC2Ev = comdat any

$_ZN8DerivedN5printEv = comdat any

$_ZN4BaseC2Ev = comdat any

$_ZTV8DerivedN = comdat any

$_ZTI8DerivedN = comdat any

$_ZTS8DerivedN = comdat any

$_ZTV4Base = comdat any

$_ZTI4Base = comdat any

$_ZTS4Base = comdat any

@_ZTV8DerivedN = linkonce_odr hidden unnamed_addr constant { [3 x ptr] } { [3 x ptr] [ptr null, ptr @_ZTI8DerivedN, ptr @_ZN8DerivedN5printEv] }, comdat, align 8, !type !0, !type !1, !type !2, !type !3, !type !4, !type !5, !vcall_visibility !6
@_ZTI8DerivedN = linkonce_odr hidden constant { ptr, ptr, ptr } { ptr null, ptr @_ZTS8DerivedN, ptr @_ZTI7Derived }, comdat, align 8
@_ZTS8DerivedN = linkonce_odr hidden constant [10 x i8] c"8DerivedN\00", comdat, align 1
@_ZTI7Derived = constant { ptr, ptr } { ptr null, ptr null}
@_ZTV4Base = linkonce_odr hidden unnamed_addr constant { [3 x ptr] } { [3 x ptr] [ptr null, ptr @_ZTI4Base, ptr @__cxa_pure_virtual] }, comdat, align 8, !type !0, !type !1, !vcall_visibility !6
@_ZTI4Base = linkonce_odr hidden constant { ptr, ptr } { ptr null, ptr @_ZTS4Base }, comdat, align 8
@_ZTS4Base = linkonce_odr hidden constant [6 x i8] c"4Base\00", comdat, align 1
@.str = private unnamed_addr constant [10 x i8] c"DerivedN\0A\00", align 1

; Function Attrs: mustprogress noinline optnone uwtable
define hidden void @_ZN4Base8dispatchEv(ptr noundef nonnull align 8 dereferenceable(8) %this) #0 align 2 !dbg !18 !type !22 {
entry:
  %this.addr = alloca ptr, align 8
  store ptr %this, ptr %this.addr, align 8
  %this1 = load ptr, ptr %this.addr, align 8
  %vtable = load ptr, ptr %this1, align 8, !dbg !23
  %0 = call i1 @llvm.type.test(ptr %vtable, metadata !"_ZTS7Derived"), !dbg !23
  call void @llvm.assume(i1 %0), !dbg !23
  %vfn = getelementptr inbounds ptr, ptr %vtable, i64 0, !dbg !23
  %1 = load ptr, ptr %vfn, align 8, !dbg !23
  call void %1(ptr noundef nonnull align 8 dereferenceable(8) %this1), !dbg !23
  ret void, !dbg !24
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i1 @llvm.type.test(ptr, metadata) #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: write)
declare void @llvm.assume(i1 noundef) #2

; Function Attrs: mustprogress noinline optnone uwtable
define hidden noundef ptr @_Z6getPtri(i32 noundef %x) #0 !dbg !25 {
entry:
  %x.addr = alloca i32, align 4
  store i32 %x, ptr %x.addr, align 4
  %call = call noalias noundef nonnull ptr @_Znwm(i64 noundef 8) #9, !dbg !26
  call void @llvm.memset.p0.i64(ptr align 8 %call, i8 0, i64 8, i1 false), !dbg !27
  call void @_ZN8DerivedNC2Ev(ptr noundef nonnull align 8 dereferenceable(8) %call) #10, !dbg !27
  ret ptr %call, !dbg !28
}

; Function Attrs: nobuiltin allocsize(0)
declare noundef nonnull ptr @_Znwm(i64 noundef) #3

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr writeonly captures(none), i8, i64, i1 immarg) #4

; Function Attrs: mustprogress noinline nounwind optnone uwtable
define linkonce_odr hidden void @_ZN8DerivedNC2Ev(ptr noundef nonnull align 8 dereferenceable(8) %this) unnamed_addr #5 comdat align 2 !dbg !29 {
entry:
  %this.addr = alloca ptr, align 8
  store ptr %this, ptr %this.addr, align 8
  %this1 = load ptr, ptr %this.addr, align 8
  call void @_ZN7DerivedC2Ev(ptr noundef nonnull align 8 dereferenceable(8) %this1) #10, !dbg !30
  store ptr getelementptr inbounds inrange(-16, 8) ({ [3 x ptr] }, ptr @_ZTV8DerivedN, i32 0, i32 0, i32 2), ptr %this1, align 8, !dbg !30
  ret void, !dbg !30
}

; Function Attrs: mustprogress noinline norecurse optnone uwtable
define hidden noundef i32 @_start() #6 !dbg !31 {
entry:
  %retval = alloca i32, align 4
  %b = alloca ptr, align 8
  %a = alloca ptr, align 8
  store i32 0, ptr %retval, align 4
  %call = call noundef ptr @_Z6createi(i32 noundef 201), !dbg !32
  store ptr %call, ptr %b, align 8, !dbg !33
  %0 = load ptr, ptr %b, align 8, !dbg !34
  call void @_ZN4Base8dispatchEv(ptr noundef nonnull align 8 dereferenceable(8) %0), !dbg !35
  %1 = load ptr, ptr %b, align 8, !dbg !36
  %isnull = icmp eq ptr %1, null, !dbg !37
  br i1 %isnull, label %delete.end, label %delete.notnull, !dbg !37

delete.notnull:                                   ; preds = %entry
  call void @_ZdlPvm(ptr noundef %1, i64 noundef 8) #11, !dbg !37
  br label %delete.end, !dbg !37

delete.end:                                       ; preds = %delete.notnull, %entry
  %call1 = call noundef ptr @_Z6getPtri(i32 noundef 202), !dbg !38
  store ptr %call1, ptr %a, align 8, !dbg !39
  %2 = load ptr, ptr %a, align 8, !dbg !40
  call void @_ZN4Base8dispatchEv(ptr noundef nonnull align 8 dereferenceable(8) %2), !dbg !41
  %3 = load ptr, ptr %a, align 8, !dbg !42
  %isnull2 = icmp eq ptr %3, null, !dbg !43
  br i1 %isnull2, label %delete.end4, label %delete.notnull3, !dbg !43

delete.notnull3:                                  ; preds = %delete.end
  call void @_ZdlPvm(ptr noundef %3, i64 noundef 8) #11, !dbg !43
  br label %delete.end4, !dbg !43

delete.end4:                                      ; preds = %delete.notnull3, %delete.end
  ret i32 0, !dbg !44
}

declare noundef ptr @_Z6createi(i32 noundef) #7

; Function Attrs: nobuiltin nounwind
declare void @_ZdlPvm(ptr noundef, i64 noundef) #8

; Function Attrs: mustprogress noinline nounwind optnone uwtable
define linkonce_odr hidden void @_ZN7DerivedC2Ev(ptr noundef nonnull align 8 dereferenceable(8) %this) unnamed_addr #5 comdat align 2 !dbg !45 {
entry:
  %this.addr = alloca ptr, align 8
  store ptr %this, ptr %this.addr, align 8
  %this1 = load ptr, ptr %this.addr, align 8
  call void @_ZN4BaseC2Ev(ptr noundef nonnull align 8 dereferenceable(8) %this1) #10, !dbg !46
  ;store ptr getelementptr inbounds inrange(-16, 8) ({ [3 x ptr] }, ptr @_ZTV7Derived, i32 0, i32 0, i32 2), ptr %this1, align 8, !dbg !46
  ret void, !dbg !46
}

; Function Attrs: mustprogress noinline optnone uwtable
define linkonce_odr hidden void @_ZN8DerivedN5printEv(ptr noundef nonnull align 8 dereferenceable(8) %this) unnamed_addr #0 comdat align 2 !dbg !47 {
entry:
  %this.addr = alloca ptr, align 8
  store ptr %this, ptr %this.addr, align 8
  %this1 = load ptr, ptr %this.addr, align 8
  ;%call = call i32 (ptr, ...) @printf(ptr noundef @.str), !dbg !48
  ret void, !dbg !49
}

; Function Attrs: mustprogress noinline nounwind optnone uwtable
define linkonce_odr hidden void @_ZN4BaseC2Ev(ptr noundef nonnull align 8 dereferenceable(8) %this) unnamed_addr #5 comdat align 2 !dbg !50 {
entry:
  %this.addr = alloca ptr, align 8
  store ptr %this, ptr %this.addr, align 8
  %this1 = load ptr, ptr %this.addr, align 8
  store ptr getelementptr inbounds inrange(-16, 8) ({ [3 x ptr] }, ptr @_ZTV4Base, i32 0, i32 0, i32 2), ptr %this1, align 8, !dbg !51
  ret void, !dbg !51
}

declare void @__cxa_pure_virtual() unnamed_addr

;declare i32 @printf(ptr noundef, ...) #7

@llvm.used = appending global [1 x ptr] [ptr @_ZTV8DerivedN], section "llvm.metadata"

attributes #0 = { mustprogress noinline optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #2 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: write) }
attributes #3 = { nobuiltin allocsize(0) "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { nocallback nofree nounwind willreturn memory(argmem: write) }
attributes #5 = { mustprogress noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #6 = { mustprogress noinline norecurse optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #7 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #8 = { nobuiltin nounwind "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #9 = { builtin allocsize(0) }
attributes #10 = { nounwind }
attributes #11 = { builtin nounwind }

!llvm.dbg.cu = !{!7}
!llvm.module.flags = !{!9, !10, !11, !12, !13, !14, !15}
!llvm.ident = !{!17}

!0 = !{i64 16, !"_ZTS4Base"}
!1 = !{i64 16, !"_ZTSM4BaseFvvE.virtual"}
!2 = !{i64 16, !"_ZTS7Derived"}
!3 = !{i64 16, !"_ZTSM7DerivedFvvE.virtual"}
!4 = !{i64 16, !"_ZTS8DerivedN"}
!5 = !{i64 16, !"_ZTSM8DerivedNFvvE.virtual"}
!6 = !{i64 1}
!7 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !8, producer: "clang version 21.0.0git (https://github.com/mingmingl-llvm/llvm-project.git 61614d7950e413b4a69a39989aabe578423eb36b)", isOptimized: true, runtimeVersion: 0, emissionKind: NoDebug, splitDebugInlining: false, nameTableKind: None)
!8 = !DIFile(filename: "main.cc", directory: "/usr/local/google/home/mingmingl/llvm-import-dec/llvm-project/build")
!9 = !{i32 2, !"Debug Info Version", i32 3}
!10 = !{i32 1, !"wchar_size", i32 4}
!11 = !{i32 1, !"Virtual Function Elim", i32 0}
!12 = !{i32 8, !"PIC Level", i32 2}
!13 = !{i32 7, !"PIE Level", i32 2}
!14 = !{i32 7, !"uwtable", i32 2}
!15 = !{i32 7, !"frame-pointer", i32 2}
;!16 = !{i32 1, !"EnableSplitLTOUnit", i32 0}
!17 = !{!"clang version 21.0.0git (https://github.com/mingmingl-llvm/llvm-project.git 61614d7950e413b4a69a39989aabe578423eb36b)"}
!18 = distinct !DISubprogram(name: "dispatch", scope: !19, file: !19, line: 16, type: !20, scopeLine: 16, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !7)
!19 = !DIFile(filename: "./lib.h", directory: "/usr/local/google/home/mingmingl/llvm-import-dec/llvm-project/build")
!20 = !DISubroutineType(types: !21)
!21 = !{}
!22 = !{i64 0, !"_ZTSM4BaseFvvE"}
!23 = !DILocation(line: 17, column: 32, scope: !18)
!24 = !DILocation(line: 18, column: 1, scope: !18)
!25 = distinct !DISubprogram(name: "getPtr", scope: !8, file: !8, line: 13, type: !20, scopeLine: 13, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !7)
!26 = !DILocation(line: 14, column: 10, scope: !25)
!27 = !DILocation(line: 14, column: 14, scope: !25)
!28 = !DILocation(line: 14, column: 3, scope: !25)
!29 = distinct !DISubprogram(name: "DerivedN", scope: !8, file: !8, line: 6, type: !20, scopeLine: 6, flags: DIFlagArtificial | DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !7)
!30 = !DILocation(line: 6, column: 7, scope: !29)
!31 = distinct !DISubprogram(name: "main", scope: !8, file: !8, line: 18, type: !20, scopeLine: 18, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !7)
!32 = !DILocation(line: 19, column: 37, scope: !31)
!33 = !DILocation(line: 19, column: 11, scope: !31)
!34 = !DILocation(line: 20, column: 3, scope: !31)
!35 = !DILocation(line: 20, column: 6, scope: !31)
!36 = !DILocation(line: 21, column: 10, scope: !31)
!37 = !DILocation(line: 21, column: 3, scope: !31)
!38 = !DILocation(line: 23, column: 38, scope: !31)
!39 = !DILocation(line: 23, column: 12, scope: !31)
!40 = !DILocation(line: 24, column: 3, scope: !31)
!41 = !DILocation(line: 24, column: 6, scope: !31)
!42 = !DILocation(line: 25, column: 10, scope: !31)
!43 = !DILocation(line: 25, column: 3, scope: !31)
!44 = !DILocation(line: 26, column: 3, scope: !31)
!45 = distinct !DISubprogram(name: "Derived", scope: !19, file: !19, line: 11, type: !20, scopeLine: 11, flags: DIFlagArtificial | DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !7)
!46 = !DILocation(line: 11, column: 7, scope: !45)
!47 = distinct !DISubprogram(name: "print", scope: !8, file: !8, line: 8, type: !20, scopeLine: 8, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !7)
!48 = !DILocation(line: 9, column: 5, scope: !47)
!49 = !DILocation(line: 10, column: 3, scope: !47)
!50 = distinct !DISubprogram(name: "Base", scope: !19, file: !19, line: 3, type: !20, scopeLine: 3, flags: DIFlagArtificial | DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !7)
!51 = !DILocation(line: 3, column: 7, scope: !50)

