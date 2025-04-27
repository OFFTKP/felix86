cmake_minimum_required(VERSION 3.20)

project(felix86)
include(CheckGit.cmake)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Enabling link time optimization for Release build")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

CheckGitSetup()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
cmake_policy(SET CMP0083 NEW)
include(CheckPIESupported)
check_pie_supported()
set(BISCUIT_CODE_BUFFER_MMAP ON)
set(BUILD_TESTING OFF)
add_subdirectory(external/biscuit)
add_subdirectory(external/Catch2)
add_subdirectory(external/zydis)
add_subdirectory(external/fmt)

option(BUILD_THUNKING "Build with thunking support" OFF)

if (BUILD_THUNKING)
    message("Building with thunking support")
else()
    message("Building without thunking support")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=implicit-fallthrough -Werror=incompatible-pointer-types -Wall -fPIE")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=implicit-fallthrough -Wall -Werror=switch -fPIE")

if (CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
    # GDB decides to tell you the perfectly fine vector instructions don't exist if you
    # don't specify the ISA here
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=rv64gcv_zicsr_zifencei")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=rv64gcv_zicsr_zifencei")
endif()

add_compile_options($<$<COMPILE_LANGUAGE:CPP>:-fno-rtti> $<$<COMPILE_LANGUAGE:CPP>:-fno-exceptions>)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

set(FELIX86_COMMON_SOURCES
    src/felix86/common/alloc.cpp
    src/felix86/common/config.cpp
    src/felix86/common/elf.cpp
    src/felix86/common/exit.cpp
    src/felix86/common/gdbjit.cpp
    src/felix86/common/global.cpp
    src/felix86/common/info.cpp
    src/felix86/common/log.cpp
    src/felix86/common/overlay.cpp
    src/felix86/common/print.cpp
    src/felix86/common/process_lock.cpp
    src/felix86/common/script.cpp
    src/felix86/common/shared_memory.cpp
    src/felix86/common/state.cpp
    src/felix86/common/strace.cpp
    src/felix86/common/sudo.cpp
    src/felix86/common/utility.cpp
)

set(FELIX86_HLE_SOURCES
    src/felix86/hle/brk.cpp
    src/felix86/hle/cpuid.cpp
    src/felix86/hle/filesystem.cpp
    src/felix86/hle/ioctl32.cpp
    src/felix86/hle/ipc32.cpp
    src/felix86/hle/libgl_guest_ptrs.cpp
    src/felix86/hle/mmap.cpp
    src/felix86/hle/signals.cpp
    src/felix86/hle/socket32.cpp
    src/felix86/hle/syscall.cpp
    src/felix86/hle/thread.cpp
    src/felix86/hle/thunks.cpp
)

set(FELIX86_V2_SOURCES
    src/felix86/v2/recompiler.cpp
    src/felix86/v2/handlers.cpp
    src/felix86/v2/handlers_x87.cpp
)

set(FELIX86_CORE_SOURCES
    ${FELIX86_COMMON_SOURCES}
    ${FELIX86_HLE_SOURCES}
    ${FELIX86_V2_SOURCES}
    src/felix86/emulator.cpp
)

set(FELIX86_INCLUDES
    include
    src
    external/robin-map/include
    external/json
    external/biscuit/include
    external/toml11
)

add_library(felix86_core STATIC)
target_sources(felix86_core PRIVATE ${FELIX86_CORE_SOURCES})
target_include_directories(felix86_core PUBLIC ${FELIX86_INCLUDES})
target_link_libraries(felix86_core PUBLIC Zydis fmt biscuit git_version)

if (BUILD_THUNKING)
    target_compile_definitions(felix86_core PUBLIC BUILD_THUNKING)
endif()

add_executable(felix86)
target_sources(felix86 PRIVATE src/felix86/main.cpp)
target_link_libraries(felix86 PRIVATE felix86_core)

if (BUILD_THUNKGEN)
    add_executable(felix86_thunkgen src/felix86/hle/guest_libs/generator.cpp)
    target_include_directories(felix86_thunkgen PRIVATE src/felix86/hle)
endif()

if (BUILD_THUNKS)
    if (NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        message(FATAL_ERROR "Thunk libraries themselves need to be built in x86-64")
    endif()

    enable_language(ASM_NASM)
    set_source_files_properties(src/felix86/hle/guest_libs/libGLX.asm PROPERTIES LANGUAGE ASM_NASM)
    add_library(GLX-thunked SHARED src/felix86/hle/guest_libs/libGLX.asm src/felix86/hle/guest_libs/libGLX.c)
    target_link_libraries(GLX-thunked X11)
endif()

if (BUILD_INSTRUCTION_COUNTER)
    add_executable(felix86_instruction_counter src/felix86/tools/generate_instruction_count.cpp external/box64/rv64_printer.c)
    target_include_directories(felix86_instruction_counter PRIVATE src external/box64)
    target_link_libraries(felix86_instruction_counter PRIVATE felix86_core)
endif()

if (BUILD_TESTS)
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