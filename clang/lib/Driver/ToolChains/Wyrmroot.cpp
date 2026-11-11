//===--- Wyrmroot.cpp - Wyrmroot ToolChain Implementations ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Wyrmroot.h"
#include "clang/Config/config.h"
#include "clang/Driver/CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Options/Options.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/Path.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

void wyrmroot::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                    const InputInfo &Output,
                                    const InputInfoList &Inputs,
                                    const ArgList &Args,
                                    const char *LinkingOutput) const {
  const auto &ToolChain = static_cast<const Wyrmroot &>(getToolChain());
  const Driver &D = ToolChain.getDriver();
  const llvm::Triple &Triple = ToolChain.getEffectiveTriple();

  // Wyrmroot has no dynamic linker: only static executables and relocatable
  // objects can be produced.
  for (const Arg *A :
       Args.filtered(options::OPT_shared, options::OPT_pie,
                     options::OPT_static_pie, options::OPT_rdynamic)) {
    D.Diag(diag::err_drv_unsupported_opt_for_target)
        << A->getAsString(Args) << Triple.str();
  }
  Args.ClaimAllArgs(options::OPT_static);
  Args.ClaimAllArgs(options::OPT_no_pie);

  // Silence warning for "clang -g foo.o -o foo"
  Args.ClaimAllArgs(options::OPT_g_Group);
  // and "clang -emit-llvm foo.o -o foo"
  Args.ClaimAllArgs(options::OPT_emit_llvm);
  // and for "clang -w foo.o -o foo". Other warning options are already
  // handled somewhere else.
  Args.ClaimAllArgs(options::OPT_w);

  ArgStringList CmdArgs;
  const bool Relocatable = Args.hasArg(options::OPT_r);

  if (!D.SysRoot.empty())
    CmdArgs.push_back(Args.MakeArgString("--sysroot=" + D.SysRoot));

  CmdArgs.push_back("-m");
  CmdArgs.push_back("elf_x86_64");

  if (Relocatable) {
    CmdArgs.push_back("-r");
  } else {
    // The Wyrmroot loader accepts ET_EXEC images whose program headers are
    // PT_LOAD (R, RW or RX, never W+X), PT_PHDR and PT_GNU_STACK only. Fold
    // read-only data into the RX segment and drop RELRO, which nothing would
    // apply without a dynamic linker, so a program is one RX and one RW
    // segment. --build-id (PT_NOTE) and --eh-frame-hdr (PT_GNU_EH_FRAME) stay
    // opt-in through -Wl until the loader accepts those headers.
    CmdArgs.push_back("-static");
    CmdArgs.push_back("--no-rosegment");
    CmdArgs.push_back("-z");
    CmdArgs.push_back("norelro");
    CmdArgs.push_back("-z");
    CmdArgs.push_back("max-page-size=4096");
    CmdArgs.push_back("-z");
    CmdArgs.push_back("noexecstack");
  }

  if (Args.hasArg(options::OPT_s))
    CmdArgs.push_back("-s");

  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  const bool StartFiles =
      !Relocatable &&
      !Args.hasArg(options::OPT_nostdlib, options::OPT_nostartfiles);
  if (StartFiles) {
    CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath("crt1.o")));
    CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath("crti.o")));
  }

  Args.addAllArgs(CmdArgs, {options::OPT_L, options::OPT_u});

  ToolChain.AddFilePathLibArgs(Args, CmdArgs);

  if (D.isUsingLTO())
    addLTOOptions(ToolChain, Args, CmdArgs, Output, Inputs,
                  D.getLTOMode() == LTOK_Thin);

  addLinkerCompressDebugSectionsOption(ToolChain, Args, CmdArgs);
  AddLinkerInputs(ToolChain, Inputs, Args, CmdArgs, JA);

  // Sample these options first so they are claimed even under -nostdlib et al.
  const bool NoLibc = Args.hasArg(options::OPT_nolibc);
  const bool Pthreads =
      Args.hasArg(options::OPT_pthread, options::OPT_pthreads);
  if (!Relocatable &&
      !Args.hasArg(options::OPT_nostdlib, options::OPT_nodefaultlibs)) {
    CmdArgs.push_back("--start-group");
    if (D.CCCIsCXX() && ToolChain.ShouldLinkCXXStdlib(Args)) {
      ToolChain.AddCXXStdlibLibArgs(Args, CmdArgs);
      CmdArgs.push_back("-lm");
    }
    AddRunTimeLibs(ToolChain, D, CmdArgs, Args);
    if (Pthreads)
      CmdArgs.push_back("-lpthread");
    if (!NoLibc)
      CmdArgs.push_back("-lc");
    CmdArgs.push_back("--end-group");
  }

  if (StartFiles)
    CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath("crtn.o")));

  const char *Exec = Args.MakeArgString(ToolChain.GetLinkerPath());
  C.addCommand(std::make_unique<Command>(JA, *this,
                                         ResponseFileSupport::AtFileCurCP(),
                                         Exec, CmdArgs, Inputs, Output));
}

/// Wyrmroot - Wyrmroot tool chain which calls ld.lld directly and takes all
/// headers and libraries from the Wyrmroot SDK sysroot, never from the host.
Wyrmroot::Wyrmroot(const Driver &D, const llvm::Triple &Triple,
                   const ArgList &Args)
    : ToolChain(D, Triple, Args) {
  getProgramPaths().push_back(getDriver().Dir);

  if (!D.SysRoot.empty()) {
    SmallString<128> P(D.SysRoot);
    llvm::sys::path::append(P, "lib");
    getFilePaths().push_back(std::string(P));
  }
}

Tool *Wyrmroot::buildLinker() const {
  return new tools::wyrmroot::Linker(*this);
}

ToolChain::RuntimeLibType
Wyrmroot::GetRuntimeLibType(const ArgList &Args) const {
  if (Arg *A = Args.getLastArg(options::OPT_rtlib_EQ)) {
    StringRef Value = A->getValue();
    if (Value != "compiler-rt" && Value != "platform")
      getDriver().Diag(clang::diag::err_drv_invalid_rtlib_name)
          << A->getAsString(Args);
  }
  return ToolChain::RLT_CompilerRT;
}

ToolChain::CXXStdlibType Wyrmroot::GetCXXStdlibType(const ArgList &Args) const {
  if (Arg *A = Args.getLastArg(options::OPT_stdlib_EQ)) {
    StringRef Value = A->getValue();
    if (Value != "libc++" && Value != "platform")
      getDriver().Diag(diag::err_drv_invalid_stdlib_name)
          << A->getAsString(Args);
  }
  return ToolChain::CST_Libcxx;
}

void Wyrmroot::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
                                         ArgStringList &CC1Args) const {
  const Driver &D = getDriver();

  if (DriverArgs.hasArg(options::OPT_nostdinc))
    return;

  if (!DriverArgs.hasArg(options::OPT_nobuiltininc)) {
    SmallString<128> P(D.ResourceDir);
    llvm::sys::path::append(P, "include");
    addSystemInclude(DriverArgs, CC1Args, P);
  }

  if (DriverArgs.hasArg(options::OPT_nostdlibinc))
    return;

  // Only the SDK sysroot supplies libc headers; there is no host fallback.
  if (!D.SysRoot.empty()) {
    SmallString<128> P(D.SysRoot);
    llvm::sys::path::append(P, "include");
    addExternCSystemInclude(DriverArgs, CC1Args, P.str());
  }
}

void Wyrmroot::AddClangCXXStdlibIncludeArgs(const ArgList &DriverArgs,
                                            ArgStringList &CC1Args) const {
  if (DriverArgs.hasArg(options::OPT_nostdinc, options::OPT_nostdlibinc,
                        options::OPT_nostdincxx))
    return;

  const Driver &D = getDriver();
  if (D.SysRoot.empty())
    return;

  switch (GetCXXStdlibType(DriverArgs)) {
  case ToolChain::CST_Libcxx: {
    SmallString<128> P(D.SysRoot);
    llvm::sys::path::append(P, "include", "c++", "v1");
    addSystemInclude(DriverArgs, CC1Args, P);
    break;
  }
  case ToolChain::CST_Libstdcxx:
    llvm_unreachable("invalid stdlib name");
  }
}

void Wyrmroot::AddCXXStdlibLibArgs(const ArgList &Args,
                                   ArgStringList &CmdArgs) const {
  switch (GetCXXStdlibType(Args)) {
  case ToolChain::CST_Libcxx:
    CmdArgs.push_back("-lc++");
    CmdArgs.push_back("-lc++abi");
    break;
  case ToolChain::CST_Libstdcxx:
    llvm_unreachable("invalid stdlib name");
  }
}
