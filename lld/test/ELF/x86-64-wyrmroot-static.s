# REQUIRES: x86

## x86_64-unknown-wyrmroot executables are static, non-PIE ELF with no dynamic
## linker. Link an object for that triple the way the clang driver does and
## check the shape the Wyrmroot loader accepts: ET_EXEC, OS/ABI 0, a read-only
## PT_PHDR, exactly one RX and one RW PT_LOAD, PT_GNU_STACK, and no interpreter, dynamic
## section, RELRO, note or eh_frame_hdr segment, and no relocations.

# RUN: llvm-mc -filetype=obj -triple=x86_64-unknown-wyrmroot %s -o %t.o
# RUN: llvm-readelf -h %t.o | FileCheck %s --check-prefix=OBJ
# RUN: ld.lld -m elf_x86_64 -static --no-rosegment -z norelro \
# RUN:   -z max-page-size=4096 -z noexecstack %t.o -o %t
# RUN: llvm-readelf -h -l -d -r %t | FileCheck %s \
# RUN:   --implicit-check-not=INTERP --implicit-check-not=DYNAMIC \
# RUN:   --implicit-check-not=GNU_RELRO --implicit-check-not=NOTE \
# RUN:   --implicit-check-not=GNU_EH_FRAME --implicit-check-not=LOAD

# OBJ: OS/ABI: UNIX - System V
# OBJ: Type: REL (Relocatable file)
# OBJ: Machine: Advanced Micro Devices X86-64

# CHECK: OS/ABI: UNIX - System V
# CHECK: Type: EXEC (Executable file)
# CHECK: Machine: Advanced Micro Devices X86-64
# CHECK: Number of program headers: 4
# CHECK: Program Headers:
# CHECK-NEXT: Type
# CHECK-NEXT: PHDR {{.*}} R   0x8
# CHECK-NEXT: LOAD {{.*}} R E 0x1000
# CHECK-NEXT: LOAD {{.*}} RW  0x1000
# CHECK-NEXT: GNU_STACK {{.*}} RW  0x0
# CHECK: There are no relocations in this file.

.globl _start
_start:
  movq counter(%rip), %rax
  leaq message(%rip), %rdi
  ret

.section .rodata,"a",@progbits
message:
  .asciz "wyrmroot"

.data
.globl counter
counter:
  .quad 7
