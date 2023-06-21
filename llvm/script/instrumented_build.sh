PATH_TO_LLVM_PROJECT=/home/mingmingl/llvm-project2/llvm-project
PATH_TO_INSTALL_BUILD=$PATH_TO_LLVM_PROJECT/build

PATH_TO_LLVM_SOURCES=$PATH_TO_LLVM_PROJECT/llvm
BASE_DIR=$PATH_TO_LLVM_PROJECT
PATH_TO_INSTRUMENTED_BINARY=$PATH_TO_LLVM_PROJECT/instrumented_build

CLANG_VERSION=17

INSTRUMENTED_CMAKE_FLAGS=(
  "-DLLVM_OPTIMIZED_TABLEGEN=ON"
  "-DCMAKE_BUILD_TYPE=Release"
  "-DLLVM_ENABLE_PROJECTS=clang;lld;compiler-rt"
  "-DCMAKE_C_COMPILER=${PATH_TO_INSTALL_BUILD}/bin/clang"
  "-DCMAKE_CXX_COMPILER=${PATH_TO_INSTALL_BUILD}/bin/clang++"
  "-DLLVM_BUILD_INSTRUMENTED=IR"
  "-DLLVM_ENABLE_IR_PGO=ON"
  "-DLLVM_PROFILE_DATA_DIR=${PATH_TO_INSTRUMENTED_BINARY}/profiles"
  "-DLLVM_USE_LINKER=lld" )

BASELINE_CC_LD_CMAKE_FLAGS=(
  "-DCMAKE_C_FLAGS=-funique-internal-linkage-names"
  "-DCMAKE_CXX_FLAGS=-funique-internal-linkage-names"
  "-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld -Wl,-gc-sections -Wl,-z,keep-text-section-prefix"
  "-DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld -Wl,-gc-sections -Wl,-z,keep-text-section-prefix"
  "-DCMAKE_MODULE_LINKER_FLAGS=-fuse-ld=lld -Wl,-gc-sections -Wl,-z,keep-text-section-prefix" )

mkdir -p $PATH_TO_INSTRUMENTED_BINARY

cmake -G Ninja "${INSTRUMENTED_CMAKE_FLAGS[@]}" "${BASELINE_CC_LD_CMAKE_FLAGS[@]}" "${PATH_TO_LLVM_SOURCES}"
ninja clang

BENCHMARKING_CLANG_BUILD=$BASE_DIR/build_clang_benchmarking

mkdir -p ${BENCHMARKING_CLANG_BUILD} && cd ${BENCHMARKING_CLANG_BUILD}
mkdir symlink_to_clang_binary && cd symlink_to_clang_binary

ln -sf ${PATH_TO_INSTRUMENTED_BINARY}/bin/clang-${CLANG_VERSION} clang
ln -sf ${PATH_TO_INSTRUMENTED_BINARY}/bin/clang-${CLANG_VERSION} clang++

cd ${BENCHMARKING_CLANG_BUILD}

cmake -G Ninja -DCMAKE_BUILD_TYPE=Release  -DLLVM_ENABLE_PROJECTS="clang;lld" \
               -DCMAKE_C_COMPILER=${BENCHMARKING_CLANG_BUILD}/symlink_to_clang_binary/clang \
               -DCMAKE_CXX_COMPILER=${BENCHMARKING_CLANG_BUILD}/symlink_to_clang_binary/clang++ \
              ${PATH_TO_LLVM_SOURCES}

ninja -t commands | head -100 >& ./instr_commands.sh
chmod +x ./instr_commands.sh
./instr_commands.sh
  