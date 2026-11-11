//===--- Wyrmroot.h - Wyrmroot ToolChain Implementations --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Wyrmroot is a capability-based operating system. Its native userspace ABI is
// SysV x86-64 with static, non-PIE ELF executables; there is no dynamic
// linker. Applications build against a Wyrmroot SDK sysroot.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_WYRMROOT_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_WYRMROOT_H

#include "clang/Driver/Tool.h"
#include "clang/Driver/ToolChain.h"

namespace clang {
namespace driver {
namespace tools {
namespace wyrmroot {
class LLVM_LIBRARY_VISIBILITY Linker final : public Tool {
public:
  Linker(const ToolChain &TC) : Tool("wyrmroot::Linker", "ld.lld", TC) {}

  bool hasIntegratedCPP() const override { return false; }
  bool isLinkJob() const override { return true; }

  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;
};
} // end namespace wyrmroot
} // end namespace tools

namespace toolchains {

class LLVM_LIBRARY_VISIBILITY Wyrmroot : public ToolChain {
public:
  Wyrmroot(const Driver &D, const llvm::Triple &Triple,
           const llvm::opt::ArgList &Args);

  bool HasNativeLLVMSupport() const override { return true; }
  bool IsMathErrnoDefault() const override { return false; }

  RuntimeLibType GetDefaultRuntimeLibType() const override {
    return ToolChain::RLT_CompilerRT;
  }
  CXXStdlibType GetDefaultCXXStdlibType() const override {
    return ToolChain::CST_Libcxx;
  }
  RuntimeLibType
  GetRuntimeLibType(const llvm::opt::ArgList &Args) const override;
  CXXStdlibType GetCXXStdlibType(const llvm::opt::ArgList &Args) const override;

  // Static, non-PIE executables only: the Wyrmroot loader maps ET_EXEC images
  // at their linked addresses and there is no dynamic linker.
  bool isPICDefault() const override { return false; }
  bool isPIEDefault(const llvm::opt::ArgList &Args) const override {
    return false;
  }
  bool isPICDefaultForced() const override { return false; }

  UnwindTableLevel
  getDefaultUnwindTableLevel(const llvm::opt::ArgList &Args) const override {
    return UnwindTableLevel::Asynchronous;
  }
  llvm::DebuggerKind getDefaultDebuggerTuning() const override {
    return llvm::DebuggerKind::GDB;
  }

  void
  AddClangSystemIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                            llvm::opt::ArgStringList &CC1Args) const override;
  void AddClangCXXStdlibIncludeArgs(
      const llvm::opt::ArgList &DriverArgs,
      llvm::opt::ArgStringList &CC1Args) const override;
  void AddCXXStdlibLibArgs(const llvm::opt::ArgList &Args,
                           llvm::opt::ArgStringList &CmdArgs) const override;

  const char *getDefaultLinker() const override { return "ld.lld"; }

protected:
  Tool *buildLinker() const override;
};

} // end namespace toolchains
} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_WYRMROOT_H
