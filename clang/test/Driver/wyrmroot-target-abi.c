// REQUIRES: x86-registered-target

// The C target must agree with the Rust x86_64-unknown-wyrmroot target:
// same data layout, cpu x86-64 with its baseline features (the SysV x86-64
// psABI: SSE2, x87 80-bit long double), inline stack probes, static
// relocation model and the default (small) code model.

// RUN: %clang --target=x86_64-unknown-wyrmroot -O1 -S -emit-llvm -o - %s \
// RUN:     | FileCheck --check-prefix=IR %s
// IR: target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
// IR: target triple = "x86_64-unknown-wyrmroot"
// IR: define{{.*}} x86_fp80 @add_long_double(x86_fp80 {{.*}}){{.*}} [[ATTRS:#[0-9]+]] {
// IR: define{{.*}} float @add_float(float {{.*}}, float {{.*}}){{.*}} [[ATTRS]] {
// IR: attributes [[ATTRS]] = { {{.*}}"probe-stack"="inline-asm"{{.*}}"target-cpu"="x86-64"{{.*}}"target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87"
// IR-NOT: soft-float
// IR-NOT: "PIC Level"
// IR-NOT: "PIE Level"

// float and double travel in XMM registers and use SSE arithmetic; long
// double is the x87 80-bit format, passed in memory.
// RUN: %clang --target=x86_64-unknown-wyrmroot -O1 -S -o - %s \
// RUN:     | FileCheck --check-prefix=ASM %s
// ASM-LABEL: add_long_double:
// ASM: fldt
// ASM: fadd
// ASM-LABEL: add_float:
// ASM: addss %xmm1, %xmm0
// ASM-LABEL: add_double:
// ASM: addsd %xmm1, %xmm0
// ASM-LABEL: big_frame:
// ASM: subq $4096, %rsp
// ASM: movq $0, (%rsp)

// RUN: %clang --target=x86_64-unknown-wyrmroot -E -dM -o - %s \
// RUN:     | FileCheck --check-prefix=PP %s
// PP-DAG: #define __wyrmroot__ 1
// PP-DAG: #define __ELF__ 1
// PP-DAG: #define __x86_64__ 1
// PP-DAG: #define __SSE__ 1
// PP-DAG: #define __SSE2__ 1
// PP-DAG: #define __SIZEOF_LONG_DOUBLE__ 16
// PP-DAG: #define __LDBL_MANT_DIG__ 64
// PP-NOT: #define __unix__
// PP-NOT: #define __linux__
// PP-NOT: #define __AVX__
// PP-NOT: #define __pic__
// PP-NOT: #define __PIE__

long double add_long_double(long double a) { return a + 1; }

float add_float(float a, float b) { return a + b; }
double add_double(double a, double b) { return a + b; }

void consume(char *);
void big_frame(void) {
  char buffer[16384];
  consume(buffer);
}
