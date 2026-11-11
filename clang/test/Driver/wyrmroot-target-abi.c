// REQUIRES: x86-registered-target

// The C target must agree with the Rust x86_64-unknown-wyrmroot target:
// same data layout, cpu x86-64, soft-float with x87/MMX/SSE disabled, inline
// stack probes, static relocation model and the default (small) code model.

// RUN: %clang --target=x86_64-unknown-wyrmroot -O1 -S -emit-llvm -o - %s \
// RUN:     | FileCheck --check-prefix=IR %s
// IR: target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
// IR: target triple = "x86_64-unknown-wyrmroot"
// IR: define{{.*}} float @add_float(float {{.*}}, float {{.*}}){{.*}} [[ATTRS:#[0-9]+]] {
// IR: attributes [[ATTRS]] = { {{.*}}"probe-stack"="inline-asm"{{.*}}"target-cpu"="x86-64"{{.*}}"target-features"="{{[^"]*}}+soft-float{{[^"]*}}-fxsr{{[^"]*}}-mmx{{[^"]*}}-sse{{[^"]*}}-x87{{[^"]*}}"
// IR-NOT: "PIC Level"
// IR-NOT: "PIE Level"

// Floating point is lowered to compiler-rt soft-float calls with arguments in
// general-purpose registers; no x87 or SSE register is touched.
// RUN: %clang --target=x86_64-unknown-wyrmroot -O1 -S -o - %s \
// RUN:     | FileCheck --check-prefix=ASM %s \
// RUN:         --implicit-check-not=%%xmm --implicit-check-not=%%st --implicit-check-not=%%mm
// ASM-LABEL: add_float:
// ASM: {{call|jmp}}{{q?}} __addsf3
// ASM-LABEL: add_double:
// ASM: {{call|jmp}}{{q?}} __adddf3
// ASM-LABEL: big_frame:
// ASM: subq $4096, %rsp
// ASM: movq $0, (%rsp)

// RUN: %clang --target=x86_64-unknown-wyrmroot -E -dM -o - %s \
// RUN:     | FileCheck --check-prefix=PP %s
// PP-DAG: #define __wyrmroot__ 1
// PP-DAG: #define __ELF__ 1
// PP-DAG: #define __x86_64__ 1
// PP-NOT: #define __unix__
// PP-NOT: #define __linux__
// PP-NOT: #define __SSE__
// PP-NOT: #define __pic__
// PP-NOT: #define __PIE__

// With x87 disabled clang has no long double format for this target, so the
// type is rejected until the floating-point ABI decision settles it.
// RUN: not %clang --target=x86_64-unknown-wyrmroot -fsyntax-only -DLONG_DOUBLE \
// RUN:     %s 2>&1 | FileCheck --check-prefix=LDBL %s
// LDBL: error: 'a' requires  'long double' type support, but target 'x86_64-unknown-wyrmroot' does not support it

#ifdef LONG_DOUBLE
long double add_long_double(long double a) { return a + 1; }
#endif

float add_float(float a, float b) { return a + b; }
double add_double(double a, double b) { return a + b; }

void consume(char *);
void big_frame(void) {
  char buffer[16384];
  consume(buffer);
}
