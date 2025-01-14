// RUN: %clang -### --target=x86_64 -fprofile-use=default.profdata -fsplit-data-sections %s 2>&1 | FileCheck %s --check-prefixes=CHECK
// RUN: not %clang -### --target=aarch64 -fprofile-use=default.profdata -fsplit-data-sections %s 2>&1 | FileCheck %s --check-prefixes=ERR

// CHECK: "-cc1" {{.*}} "-fsplit-data-sections"

// ERR: error: unsupported option '-fsplit-data-sections' for target 'aarch64'
