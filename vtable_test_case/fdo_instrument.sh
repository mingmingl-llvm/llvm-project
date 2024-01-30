if [  $# -ne 1 ]; then
 echo "Usage: fdo_instument.sh clang_path"
 exit 1
fi
 rm a.out
 rm -r -f alloc
 CLANG=$1

 func() {
   $CLANG -mllvm -enable-vtable-value-profiling -v -O2 -fprofile-generate=. sanity_check/plain/lib.h sanity_check/plain/lib.cpp sanity_check/plain/main.cpp
 }


 (export LLVM_PROFILE_FILE=main.profraw; func)
