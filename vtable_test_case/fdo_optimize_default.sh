if [  $# -ne 1 ]; then
 echo "Usage: fdo_instument.sh clang_path"
 exit 1
fi

 CLANG=$1

$CLANG -v -save-temps -mllvm -pass-remarks=pgo-icall-prom -mllvm -enable-vtable-value-profiling -mllvm -enable-vtable-cmp=false -mllvm -icp-vtable-cmp-inst-threshold=2 -mllvm -icp-vtable-cmp-inst-last-candidate-threshold=2 -mllvm -icp-vtable-cmp-total-inst-threshold=4 -O2 -fprofile-use=main.profdata -fuse-ld=lld -Wl,-plugin-opt,-pass-remarks=pgo-icall-prom -flto=thin -Wl,-plugin-opt,-print-after-all -Wl,-plugin-opt,-filter-print-funcs=main -Wl,-plugin-opt,-enable-vtable-cmp=false -Wl,-plugin-opt,-enable-vtable-value-profiling -Wl,-plugin-opt,-icp-vtable-cmp-inst-threshold=2 -Wl,-plugin-opt,-icp-vtable-cmp-inst-last-candidate-threshold=2 -fwhole-program-vtables -fno-split-lto-unit -Wl,--lto-whole-program-visibility -Wl,--lto-validate-all-vtables-have-type-infos -Wl,-plugin-opt,-icp-vtable-cmp-total-inst-threshold=4 sanity_check/plain/lib.h sanity_check/plain/lib.cpp sanity_check/plain/main.cpp
