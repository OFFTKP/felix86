cmake_minimum_required(VERSION 3.16)

option(FELIX86_BUILD_THUNKING "Build with thunking support" ON)
option(FELIX86_MONTHLY_RELEASE "Build as a monthly release" OFF)

project(felix86)
include(CheckGit.cmake)

get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (NOT IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo
        CACHE STRING "Choose the type of build." FORCE)
    message(STATUS "No build type set, defaulting to a RelWithDebInfo build")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Enabling link time optimization for Release build")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Set the "SOURCE_PATH_SIZE" variable to the size of the source directory
# We can abuse this to truncate the __FILE__ macro to only show the filename
string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
add_definitions("-DSOURCE_PATH_SIZE=${SOURCE_PATH_SIZE}")

CheckGitSetup()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
        AND CMAKE_CXX_COMPILER_VERSION MATCHES "^14.*")
        # It seems that gcc-14 has a bug where it compiles a badly aligned
        # vector load from one of our packed structs, namely the conversion
        # of x86_flock64 in Portal. This bug seems to be fixed in later gcc
        # versions, but isn't backported so we need to warn and disable the
        # V extension from static compilation.
        message(WARNING "GCC 14 has a bug that causes unaligned vector"
            " load/stores which may cause bus errors in some of our packed"
            " structs. It is suggested you use a different compiler. More info:"
            " https://gcc.gnu.org/bugzilla/show_bug.cgi?id=115789")

        # Don't use vector extension to avoid this bug
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=rv64gc_zicsr_zifencei -mno-relax")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=rv64gc_zicsr_zifencei -mno-relax")
    else()
        # Since felix86 needs RVV, specify the ISA here to get some free wins in the C++ side
        # Also use --no-relax so that the `gp` register is free to do as we please
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=rv64gcv_zicsr_zifencei -mno-relax")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=rv64gcv_zicsr_zifencei -mno-relax")
    endif()

    # Just an extra layer of making sure the compiler doesn't decide to use GP as an allocatable reg
    # -mno-relax should be enough but you never know
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffixed-gp")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffixed-gp")
endif()


set(CMAKE_POSITION_INDEPENDENT_CODE ON)
cmake_policy(SET CMP0083 NEW)
include(CheckPIESupported)
check_pie_supported()
set(BISCUIT_CODE_BUFFER_MMAP ON)
set(BUILD_TESTING OFF)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-rtti> $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>)
add_subdirectory(external/biscuit)
add_subdirectory(external/zydis)
add_subdirectory(external/fmt)
add_subdirectory(external/berkeley-softfloat-3)
add_subdirectory(src/felix86/hle/vdso)
target_compile_definitions(fmt PRIVATE FMT_USE_EXCEPTIONS=0)

if (FELIX86_BUILD_THUNKING)
    message("Building with thunking support")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLX REQUIRED gl)
    pkg_check_modules(VULKAN REQUIRED vulkan)
    pkg_check_modules(X11 REQUIRED x11)
    include_directories(${GLX_INCLUDE_DIRS})
    include_directories(${VULKAN_INCLUDE_DIRS})
    include_directories(${X11_INCLUDE_DIRS})
else()
    message("Building without thunking support")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=implicit-fallthrough -Werror=incompatible-pointer-types -Wall -fPIE")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=implicit-fallthrough -Wall -Werror=switch -fPIE")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

set(FELIX86_COMMON_SOURCES
    src/felix86/common/config.cpp
    src/felix86/common/elf.cpp
    src/felix86/common/exit.cpp
    src/felix86/common/feature.cpp
    src/felix86/common/freelist.cpp
    src/felix86/common/gdbjit.cpp
    src/felix86/common/global.cpp
    src/felix86/common/info.cpp
    src/felix86/common/log.cpp
    src/felix86/common/overlay.cpp
    src/felix86/common/print.cpp
    src/felix86/common/process_lock.cpp
    src/felix86/common/script.cpp
    src/felix86/common/state.cpp
    src/felix86/common/strace.cpp
    src/felix86/common/utility.cpp
    src/felix86/common/binfmt.cpp
    src/felix86/common/xsave.cpp
)

set(FELIX86_HLE_SOURCES
    src/felix86/hle/abi.cpp
    src/felix86/hle/brk.cpp
    src/felix86/hle/cpuid.cpp
    src/felix86/hle/fd.cpp
    src/felix86/hle/filesystem.cpp
    src/felix86/hle/ioctl32.cpp
    src/felix86/hle/ipc32.cpp
    src/felix86/hle/mmap.cpp
    src/felix86/hle/ptrace.cpp
    src/felix86/hle/seccomp.cpp
    src/felix86/hle/signals.cpp
    src/felix86/hle/socket32.cpp
    src/felix86/hle/syscall.cpp
    src/felix86/hle/thread.cpp
    src/felix86/hle/thunks.cpp
    src/felix86/hle/thunks_luajit.cpp
    src/felix86/hle/vdso.cpp

    src/felix86/hle/ioctl/amdgpu.cpp
    src/felix86/hle/ioctl/drm.cpp
    src/felix86/hle/ioctl/fat.cpp
    src/felix86/hle/ioctl/fs.cpp
    src/felix86/hle/ioctl/tty.cpp
    src/felix86/hle/ioctl/radeon.cpp
)

set(FELIX86_V2_SOURCES
    src/felix86/v2/recompiler.cpp
    src/felix86/v2/handlers.cpp
)

set(FELIX86_CORE_SOURCES
    ${FELIX86_COMMON_SOURCES}
    ${FELIX86_HLE_SOURCES}
    ${FELIX86_V2_SOURCES}
    src/felix86/emulator.cpp
)

set(FELIX86_REPL_SOURCES
    src/felix86/repl.cpp
    external/box64/rv64_printer.c
)

set(FELIX86_INCLUDES
    include
    src
    external/robin-map/include
    external/json
    external/biscuit/include
    external/tomlc17
)

add_library(tomlc17 STATIC external/tomlc17/tomlc17.c)
add_library(felix86_core STATIC)
target_sources(felix86_core PRIVATE ${FELIX86_CORE_SOURCES})
target_include_directories(felix86_core PUBLIC ${FELIX86_INCLUDES})
target_link_libraries(felix86_core PUBLIC Zydis fmt tomlc17 biscuit berkeley-softfloat git_version x64_vdso)

if (FELIX86_MONTHLY_RELEASE)
    message("Building as a monthly release")
    target_compile_definitions(felix86_core PUBLIC FELIX86_MONTHLY_RELEASE)
else()
    message("Building as a daily release")
endif()


if (FELIX86_BUILD_THUNKING)
    target_compile_definitions(felix86_core PUBLIC FELIX86_BUILD_THUNKING)
endif()

add_executable(felix86)
target_sources(felix86 PRIVATE src/felix86/main.cpp)
target_link_libraries(felix86 PRIVATE felix86_core)

# If custom glibc is installed you can use these environment variables to patch the executable
# This is helpful for developers to automatically patch felix86 after building
if (FELIX86_LDSO AND FELIX86_RUNPATH)
    find_program(PATCHELF_EXECUTABLE patchelf)

    if (NOT PATCHELF_EXECUTABLE)
        message(FATAL_ERROR "patchelf not found but FELIX86_LDSO/FELIX86_RUNPATH were supplied. "
            "Install patchelf or unset these options.")
    endif()

    message(STATUS "patchelf found: ${PATCHELF_EXECUTABLE}")

    add_custom_command(TARGET felix86 POST_BUILD
        COMMAND ${PATCHELF_EXECUTABLE} --set-interpreter "${FELIX86_LDSO}" --set-rpath "${FELIX86_RUNPATH}" $<TARGET_FILE:felix86>
        COMMENT "Patching felix86 with patchelf"
    )
endif()

if (BUILD_REPL)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(READLINE REQUIRED readline)
    target_sources(felix86 PRIVATE ${FELIX86_REPL_SOURCES})
    target_link_libraries(felix86 PRIVATE readline)
    target_compile_definitions(felix86 PRIVATE BUILD_REPL)
endif()

if (BUILD_THUNKGEN)
    add_executable(felix86_thunkgen src/felix86/hle/guest_libs/generator.cpp)
    target_include_directories(felix86_thunkgen PRIVATE src/felix86/hle)
endif()

if (BUILD_INSTRUCTION_COUNTER)
    # Generated x86 code may slightly change between xbyak versions
    # One example is order of operands in xchg reg, reg has changed in the past
    # which causes slightly different generated code, so we pin it to a vendored version
    add_library(vendored_xbyak INTERFACE)
    target_include_directories(vendored_xbyak INTERFACE external/xbyak)
    add_executable(felix86_instruction_counter src/felix86/tools/generate_instruction_count.cpp external/box64/rv64_printer.c)
    target_include_directories(felix86_instruction_counter PRIVATE src external/box64)
    target_link_libraries(felix86_instruction_counter PRIVATE vendored_xbyak felix86_core)
endif()


if (BUILD_TESTS)
    add_subdirectory(external/Catch2)
    set(FELIX86_TEST_SOURCES
        tests/common.cpp
        tests/FEX/fex_test_loader.cpp
        tests/FEX/primary.cpp
        tests/FEX/primary_group.cpp
        tests/FEX/op_size.cpp
        tests/FEX/secondary.cpp
        tests/FEX/secondary_modrm.cpp
        tests/FEX/rep.cpp
        tests/FEX/tertiary.cpp
        tests/FEX/two_byte.cpp
        tests/FEX/vex.cpp
        tests/FEX/felix86_bugs.cpp
        tests/FEX/x87.cpp
        tests/Binaries/binary_test_loader.cpp
        tests/Unit/mmap.cpp
        tests/Unit/paths.cpp
    )

    add_executable(felix86_test)
    target_sources(felix86_test PRIVATE ${FELIX86_TEST_SOURCES})
    target_include_directories(felix86_test PRIVATE tests)
    target_link_libraries(felix86_test PRIVATE felix86_core Catch2::Catch2WithMain)
    target_compile_options(felix86_test PRIVATE $<$<COMPILE_LANGUAGE:CPP>:-fno-operator-names>)
    target_compile_definitions(felix86_test PRIVATE CATCH_CONFIG_PREFIX_ALL)
    file(COPY external/FEX/ASM DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(COPY external/FEX/32Bit_ASM DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(COPY tests/ASM DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(COPY tests/Binaries DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()
