PATH_TO_LLVM_PROJECT=/usr/local/google/home/mingmingl/llvm-fork/llvm-project
# Assumes prebuild.sh already run.
PATH_TO_INSTALL_BUILD=$PATH_TO_LLVM_PROJECT/build

PATH_TO_LLVM_SOURCES=$PATH_TO_LLVM_PROJECT/llvm
BASE_DIR=$PATH_TO_LLVM_PROJECT
PATH_TO_INSTRUMENTED_BINARY=$PATH_TO_LLVM_PROJECT/instrumented_build

CLANG_VERSION=17

# convert PGO profiles to profdata
cd ${PATH_TO_INSTRUMENTED_BINARY}/profiles
${PATH_TO_INSTALL_BUILD}/bin/llvm-profdata merge --output=clang.profdata *.profraw

PATH_TO_BASELINE_BUILD=${BASE_DIR}/experiment_build

# Enable ThinLTO
COMMON_CMAKE_FLAGS=(
  "-DLLVM_OPTIMIZED_TABLEGEN=On"
  "-DCMAKE_BUILD_TYPE=Release"
  "-DLLVM_ENABLE_PROJECTS=clang;lld"
  "-DCMAKE_C_COMPILER=${PATH_TO_INSTALL_BUILD}/bin/clang"
  "-DCMAKE_CXX_COMPILER=${PATH_TO_INSTALL_BUILD}/bin/clang++"
  "-DLLVM_USE_LINKER=lld"
  "-DQWERTY_MINGMINGL_ENABLE_HYBRID=ON"
  "-DLLVM_ENABLE_LTO=Thin"
  "-DCMAKE_RANLIB=${PATH_TO_INSTALL_BUILD}/bin/llvm-ranlib"
  "-DCMAKE_AR=${PATH_TO_INSTALL_BUILD}/bin/llvm-ar"
  "-DLLVM_PROFDATA_FILE=${PATH_TO_INSTRUMENTED_BINARY}/profiles/clang.profdata" )

PRISTINE_CC_LD_CMAKE_FLAGS=(
  "-DCMAKE_C_FLAGS=-funique-internal-linkage-names"
  "-DCMAKE_CXX_FLAGS=-funique-internal-linkage-names"
  "-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld -Wl,-gc-sections -Wl,-z,keep-text-section-prefix"
  "-DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld -Wl,-gc-sections -Wl,-z,keep-text-section-prefix"
  "-DCMAKE_MODULE_LINKER_FLAGS=-fuse-ld=lld -Wl,-gc-sections -Wl,-z,keep-text-section-prefix" )

mkdir -p ${PATH_TO_BASELINE_BUILD} && cd ${PATH_TO_BASELINE_BUILD}
cmake -G Ninja "${COMMON_CMAKE_FLAGS[@]}" "${PRISTINE_CC_LD_CMAKE_FLAGS[@]}" ${PATH_TO_LLVM_SOURCES}
ninja clang