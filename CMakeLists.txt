cmake_minimum_required(VERSION 3.20)

project(felix86)

add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=implicit-fallthrough -Werror=incompatible-pointer-types -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=implicit-fallthrough -Wall -Werror=switch")

add_compile_options($<$<COMPILE_LANGUAGE:CPP>:-fno-rtti> $<$<COMPILE_LANGUAGE:CPP>:-fno-exceptions>)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

set(BISCUIT_CODE_BUFFER_MMAP ON)
add_subdirectory(external/biscuit)
# add_subdirectory(external/plog)
add_subdirectory(external/Catch2)
add_subdirectory(external/xbyak)
add_subdirectory(external/riscv-disassembler)
add_subdirectory(external/zydis)
add_subdirectory(external/fmt)

set(FELIX86_FRONTEND_SOURCES
    src/felix86/frontend/frontend.cpp
)

set(FELIX86_IR_SOURCES
    src/felix86/ir/block.cpp
    src/felix86/ir/emitter.cpp
    src/felix86/ir/function.cpp
    src/felix86/ir/handlers_weak.cpp
    src/felix86/ir/handlers.cpp
    src/felix86/ir/instruction.cpp
    src/felix86/ir/opcode.cpp
    src/felix86/ir/passes/copy_propagation_pass.cpp
    src/felix86/ir/passes/critical_edge_splitting_pass.cpp
    src/felix86/ir/passes/dead_code_elimination_pass.cpp
    src/felix86/ir/passes/extraneous_writeback_pass.cpp
    src/felix86/ir/passes/graph_coloring_pass.cpp
    src/felix86/ir/passes/local_cse_pass.cpp
    src/felix86/ir/passes/replace_setguest_pass.cpp
    src/felix86/ir/passes/spill_everything_pass.cpp
    src/felix86/ir/passes/ssa_pass.cpp
)

set(FELIX86_COMMON_SOURCES
    src/felix86/common/elf.cpp
    src/felix86/common/exit.cpp
    src/felix86/common/global.cpp
    src/felix86/common/log.cpp
    src/felix86/common/print.cpp
    src/felix86/common/utility.cpp
)

set(FELIX86_BACKEND_SOURCES
    src/felix86/backend/backend.cpp
    src/felix86/backend/block.cpp
    src/felix86/backend/emitter.cpp
    src/felix86/backend/emitter_dispatch.cpp
    src/felix86/backend/function.cpp
    src/felix86/backend/instruction.cpp
)

set(FELIX86_HLE_SOURCES
    src/felix86/hle/cpuid.cpp
    src/felix86/hle/filesystem.cpp
    src/felix86/hle/filesystem.cpp
    src/felix86/hle/syscall.cpp
)

set(FELIX86_CORE_SOURCES
    ${FELIX86_COMMON_SOURCES}
    ${FELIX86_FRONTEND_SOURCES}
    ${FELIX86_IR_SOURCES}
    ${FELIX86_BACKEND_SOURCES}
    ${FELIX86_HLE_SOURCES}
    src/felix86/emulator.cpp
)

set(FELIX86_INCLUDES
    include
    src
    external/robin-map/include
    external/json
    external/biscuit/include
)

add_library(felix86_core STATIC)
target_sources(felix86_core PUBLIC ${FELIX86_CORE_SOURCES})
target_include_directories(felix86_core PUBLIC ${FELIX86_INCLUDES})
target_link_libraries(felix86_core PUBLIC Zydis fmt biscuit rvasm)

add_executable(felix86)
target_sources(felix86 PRIVATE src/felix86/main.cpp)
target_link_libraries(felix86 PRIVATE felix86_core)

if (BUILD_TESTS)
    set(FELIX86_TEST_SOURCES
    )

    add_executable(felix86_test)
    target_sources(felix86_test PRIVATE ${FELIX86_TEST_SOURCES})
    target_include_directories(felix86_test PRIVATE tests)
    target_link_libraries(felix86_test PRIVATE felix86_core Catch2::Catch2WithMain xbyak)
    target_compile_options(felix86_test PRIVATE $<$<COMPILE_LANGUAGE:CPP>:-fno-operator-names>)
endif()