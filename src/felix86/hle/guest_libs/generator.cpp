// NOTE: this file is meant to aid developers (if anyone else is even reading this) to generate the thunk guest libraries
// After they are generated they are just shipped with the repo because imagine having to download a cross-compiler to build
// them yourself. And they should be teeny tiny. Because all they do is give a hint to the recompiler.

// This file will take ***_thunks.inc files like glx_thunks.inc and generate a .so library.
// Each thunked function will be:

// An INVLPG instruction <- this is a heuristic for our recompiler to know a thunked function is coming
// See FAST_HANDLE(INVLPG). INVLPG instructions won't appear in userspace code.
// Following the INVLPG will be a string for the symbol we are trying to run. This helps the recompiler
// immediately know the function it is currently jumping to, in order to generate an appropriate trampoline,
// while also verifying that it is one of our thunked functions and not a rogue INVLPG somehow.

// Example generated NASM code:
// global glXCreateContext
// align 16
// glxCreateContext:
// invlpg [rax]
// db "glXCreateContext", 0
// ret                      <----- we need a ret -- these functions won't always be called, sometimes they will be jumped to
//                                 (think function pointers), so we need to return to the original with a guest ret

// When the recompiler jumps to this function (because the guest dynamic linker found the symbol glXCreateContext and linked it there)
// it's going to decode an INVLPG and look for the name after the INVLPG.

// In the .inc files like glx_thunks.inc, the function signatures are specified so with just the name
// we have enough information about how to call this function and what it returns.
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <linux/limits.h>
#include <spawn.h>
#include <sys/wait.h>

std::string nasm_exe;
std::string gcc_exe;

std::string gen_init() {
    return "bits 64\n\n";
}

std::string gen_name(const std::string& name) {
    std::string ret = "global " + name + "\n";
    ret += "align 16\n";
    ret += name + ":\n";
    ret += "invlpg [rax]\n"; // <- magic instruction that tells the recompiler "Hey a thunk is here, act accordingly"
    ret += "ret\n";
    ret += "db \"" + name + "\", 0\n\n";
    return ret;
}

// clang-format off
void gen_finalize(const std::string& source, const std::filesystem::path& libname) {
    std::string asmfile = libname.stem().string() + ".asm";
    std::ofstream ofs(asmfile);
    ofs << source;
    ofs.close();

    std::string ofile = libname.stem().string() + ".o";

    std::vector<const char*> nasm_args = {
        nasm_exe.c_str(),
        "-felf64",
        "-o",
        ofile.c_str(),
        asmfile.c_str(),
        nullptr,
    };

    int status;

    // Start nasm and wait for it to finish
    int pid = posix_spawnp(&pid, nasm_args[0], nullptr, nullptr, (char**)nasm_args.data(), environ);
    waitpid(pid, &status, 0);

    if (WEXITSTATUS(status)) {
        printf("Nasm failed with code: %d while compiling %s\n", WEXITSTATUS(status), asmfile.c_str());
        exit(1);
    }

    std::string slibname = libname;
    std::vector<const char*> gcc_args = {
        gcc_exe.c_str(),
        "-shared",
        "-s",
        "-o",
        slibname.c_str(),
        ofile.c_str(),
        "-fPIC",
        nullptr,
    };

    // Start gcc to link and wait for it to finish
    pid = posix_spawnp(&pid, gcc_args[0], nullptr, nullptr, (char**)gcc_args.data(), environ);
    waitpid(pid, &status, 0);

    if (WEXITSTATUS(status)) {
        printf("GCC failed with code: %d while compiling %s\n", WEXITSTATUS(status), asmfile.c_str());
        exit(1);
    }

    if (!std::filesystem::exists(libname)) {
        printf("Something went wrong during compilation\n");
        exit(1);
    }
}

int main() {
    const char* nasm = getenv("FELIX86_NASM");
    if (!nasm) {
        nasm_exe = "nasm"; // hope for the best
    } else {
        nasm_exe = nasm;
    }

    const char* gcc = getenv("FELIX86_GCC");
    if (!gcc) {
        gcc_exe = "gcc"; // hope for the best
    } else {
        gcc_exe = gcc;
    }

    char path[PATH_MAX];
    size_t bytes = readlink("/proc/self/exe", path, PATH_MAX);
    path[bytes] = 0;

    std::filesystem::path exec = path;
    chdir(exec.parent_path().c_str());

    // As we thunk more libraries, add them here
    // libGLX.so.1 generation
#define X(libname, func, ...) source += gen_name(#func);
    {
        std::string source = gen_init();
        source += "section .text\n";

#include "glx_thunks.inc"

        // We need to export this silly thingy __GLXGL_CORE_FUNCTIONS with function pointers
        // Why it's needed? don't ask me. But apps use it.
        source += R"(
            section .data.rel.ro
            global __GLXGL_CORE_FUNCTIONS
            align 16

            __GLXGL_CORE_FUNCTIONS:
                dq glXChooseFBConfig
                dq glXChooseVisual
                dq glXCopyContext
                dq glXCreateContext
                dq glXCreateGLXPixmap
                dq glXCreateNewContext
                dq glXCreatePbuffer
                dq glXCreatePixmap
                dq glXCreateWindow
                dq glXDestroyContext
                dq glXDestroyGLXPixmap
                dq glXDestroyPbuffer
                dq glXDestroyPixmap
                dq glXDestroyWindow
                dq glXGetClientString
                dq glXGetConfig
                dq glXGetCurrentContext
                dq glXGetCurrentDrawable
                dq glXGetCurrentReadDrawable
                dq glXGetFBConfigAttrib
                dq glXGetFBConfigs
                dq glXGetProcAddress
                dq glXGetProcAddressARB
                dq glXGetSelectedEvent
                dq glXGetVisualFromFBConfig
                dq glXIsDirect
                dq glXMakeContextCurrent
                dq glXMakeCurrent
                dq glXQueryContext
                dq glXQueryDrawable
                dq glXQueryExtension
                dq glXQueryExtensionsString
                dq glXQueryServerString
                dq glXQueryVersion
                dq glXSelectEvent
                dq glXSwapBuffers
                dq glXUseXFont
                dq glXWaitGL
                dq glXWaitX
        )";


        gen_finalize(source, "libGLX.so");
    }

    {
        std::string source = gen_init();
        source += "section .text\n";

        #include "egl_thunks.inc"

        gen_finalize(source, "libEGL.so");
    }
#undef X

    return 0;
}