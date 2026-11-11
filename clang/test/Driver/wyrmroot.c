// Compile and link for Wyrmroot: static, non-PIE ELF through ld.lld, with
// headers and libraries taken only from the SDK sysroot.
// RUN: %clang -### %s --target=x86_64-unknown-wyrmroot \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:     --sysroot=%S/Inputs/basic_wyrmroot_tree 2>&1 \
// RUN:     | FileCheck --check-prefix=CHECK %s
// RUN: %clangxx -### %s --target=x86_64-unknown-wyrmroot \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:     --sysroot=%S/Inputs/basic_wyrmroot_tree 2>&1 \
// RUN:     | FileCheck --check-prefixes=CHECK,CHECK-CXX %s
// CHECK: "-cc1"
// CHECK-SAME: "-triple" "x86_64-unknown-wyrmroot"
// CHECK-NOT: "-pic-level"
// CHECK-NOT: "-pic-is-pie"
// CHECK-SAME: "-mrelocation-model" "static"
// CHECK-SAME: "-funwind-tables=2"
// CHECK-SAME: "-target-feature" "-x87" "-target-feature" "-fxsr" "-target-feature" "-mmx" "-target-feature" "-sse" "-target-feature" "+soft-float"
// CHECK-SAME: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-SAME: "-isysroot" "[[SYSROOT:[^"]+]]"
// CHECK-CXX-SAME: "-internal-isystem" "[[SYSROOT]]{{/|\\\\}}include{{/|\\\\}}c++{{/|\\\\}}v1"
// CHECK-SAME: "-internal-isystem" "[[RESOURCE_DIR]]{{/|\\\\}}include"
// CHECK-SAME: "-internal-externc-isystem" "[[SYSROOT]]{{/|\\\\}}include"
// CHECK-NOT: "-internal-externc-isystem" "/usr/include"
// CHECK-SAME: "-fstack-clash-protection"
// CHECK-NOT: "-stack-protector"
// CHECK: {{.*}}ld.lld{{(.exe)?}}" "--sysroot=[[SYSROOT]]" "-m" "elf_x86_64" "-static" "--no-rosegment" "-z" "norelro" "-z" "max-page-size=4096" "-z" "noexecstack"
// CHECK-NOT: "-pie"
// CHECK-NOT: "--build-id"
// CHECK-NOT: "--eh-frame-hdr"
// CHECK-NOT: "-dynamic-linker"
// CHECK-SAME: "[[SYSROOT]]{{/|\\\\}}lib{{/|\\\\}}crt1.o" "[[SYSROOT]]{{/|\\\\}}lib{{/|\\\\}}crti.o"
// CHECK-SAME: "-L[[SYSROOT]]{{/|\\\\}}lib"
// CHECK-SAME: "--start-group"
// CHECK-CXX-SAME: "-lc++" "-lc++abi" "-lm"
// CHECK-SAME: "{{[^"]*}}libclang_rt.builtins{{[^"]*}}.a"
// CHECK-SAME: "-lc" "--end-group" "[[SYSROOT]]{{/|\\\\}}lib{{/|\\\\}}crtn.o"

// -nostdlib drops the start files, compiler-rt and libc.
// RUN: %clang -### %s --target=x86_64-unknown-wyrmroot -nostdlib \
// RUN:     --sysroot=%S/Inputs/basic_wyrmroot_tree 2>&1 \
// RUN:     | FileCheck --check-prefix=NOSTDLIB %s
// NOSTDLIB: ld.lld{{(.exe)?}}" {{.*}}"-static"
// NOSTDLIB-NOT: crt1.o
// NOSTDLIB-NOT: libclang_rt.builtins
// NOSTDLIB-NOT: "-lc"
// NOSTDLIB-NOT: crtn.o

// -nostartfiles keeps the libraries but drops the start files.
// RUN: %clang -### %s --target=x86_64-unknown-wyrmroot -nostartfiles \
// RUN:     --sysroot=%S/Inputs/basic_wyrmroot_tree 2>&1 \
// RUN:     | FileCheck --check-prefix=NOSTARTFILES %s
// NOSTARTFILES-NOT: crt1.o
// NOSTARTFILES: "-lc" "--end-group"
// NOSTARTFILES-NOT: crtn.o

// -r produces a relocatable object with no start files or libraries.
// RUN: %clang -### %s --target=x86_64-unknown-wyrmroot -r \
// RUN:     --sysroot=%S/Inputs/basic_wyrmroot_tree 2>&1 \
// RUN:     | FileCheck --check-prefix=RELOCATABLE %s
// RELOCATABLE: ld.lld{{(.exe)?}}" {{.*}}"-m" "elf_x86_64" "-r"
// RELOCATABLE-NOT: "-static"
// RELOCATABLE-NOT: crt1.o
// RELOCATABLE-NOT: "-lc"

// There is no dynamic linker, so dynamic and position-independent
// executables are rejected.
// RUN: not %clang -### %s --target=x86_64-unknown-wyrmroot -shared 2>&1 \
// RUN:     | FileCheck --check-prefix=SHARED %s
// SHARED: error: unsupported option '-shared' for target 'x86_64-unknown-wyrmroot'
// RUN: not %clang -### %s --target=x86_64-unknown-wyrmroot -pie 2>&1 \
// RUN:     | FileCheck --check-prefix=PIE %s
// PIE: error: unsupported option '-pie' for target 'x86_64-unknown-wyrmroot'
// RUN: not %clang -### %s --target=x86_64-unknown-wyrmroot -static-pie 2>&1 \
// RUN:     | FileCheck --check-prefix=STATIC-PIE %s
// STATIC-PIE: error: unsupported option '-static-pie' for target 'x86_64-unknown-wyrmroot'
// RUN: %clang -### %s --target=x86_64-unknown-wyrmroot -static 2>&1 \
// RUN:     | FileCheck --check-prefix=STATIC %s
// STATIC-NOT: warning:
// STATIC-NOT: error:
// STATIC: ld.lld{{(.exe)?}}" {{.*}}"-static"

// Only compiler-rt and libc++ are accepted.
// RUN: not %clang -### %s --target=x86_64-unknown-wyrmroot -rtlib=libgcc 2>&1 \
// RUN:     | FileCheck --check-prefix=RTLIB %s
// RTLIB: error: invalid runtime library name in argument '-rtlib=libgcc'
// RUN: not %clangxx -### %s --target=x86_64-unknown-wyrmroot -stdlib=libstdc++ 2>&1 \
// RUN:     | FileCheck --check-prefix=STDLIB %s
// STDLIB: error: invalid library name in argument '-stdlib=libstdc++'

// Explicit flags override the defaults.
// RUN: %clang -### %s --target=x86_64-unknown-wyrmroot -fno-stack-clash-protection \
// RUN:     -msse2 -c 2>&1 | FileCheck --check-prefix=OVERRIDE %s
// OVERRIDE: "-target-feature" "-sse" "-target-feature" "+soft-float"
// OVERRIDE-SAME: "-target-feature" "+sse2"
// OVERRIDE-NOT: "-fstack-clash-protection"

// Without a sysroot no libc include directory is added at all.
// RUN: %clang -### %s --target=x86_64-unknown-wyrmroot -c 2>&1 \
// RUN:     | FileCheck --check-prefix=NOSYSROOT %s
// NOSYSROOT-NOT: "-internal-externc-isystem"
// NOSYSROOT-NOT: "/usr/include"
