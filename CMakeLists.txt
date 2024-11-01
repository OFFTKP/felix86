cmake_minimum_required(VERSION 3.20)

project(felix86)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(BISCUIT_CODE_BUFFER_MMAP ON)
set(BUILD_TESTING OFF)
add_subdirectory(external/biscuit)
# add_subdirectory(external/plog)
add_subdirectory(external/Catch2)
add_subdirectory(external/xbyak)
add_subdirectory(external/riscv-disassembler)
add_subdirectory(external/zydis)
add_subdirectory(external/fmt)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=implicit-fallthrough -Werror=incompatible-pointer-types -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=implicit-fallthrough -Wall -Werror=switch")

if (CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
    # GDB decides to tell you the perfectly fine vector instructions don't exist if you
    # don't specify the ISA here
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=rv64gcv")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=rv64gcv")
endif()

add_compile_options($<$<COMPILE_LANGUAGE:CPP>:-fno-rtti> $<$<COMPILE_LANGUAGE:CPP>:-fno-exceptions>)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

set(FELIX86_FRONTEND_SOURCES
    src/felix86/frontend/frontend.cpp
)

set(FELIX86_AOT_SOURCES
    # src/felix86/aot/control_flow_analysis.cpp
    # src/felix86/aot/function_start_finder.cpp
)

set(FELIX86_IR_SOURCES
    src/felix86/ir/block.cpp
    src/felix86/ir/emitter.cpp
    src/felix86/ir/function.cpp
    src/felix86/ir/handlers.cpp
    src/felix86/ir/instruction.cpp
    src/felix86/ir/opcode.cpp
    src/felix86/ir/passes/copy_propagation_pass.cpp
    src/felix86/ir/passes/critical_edge_splitting_pass.cpp
    src/felix86/ir/passes/dead_code_elimination_pass.cpp
    src/felix86/ir/passes/extraneous_writeback_pass.cpp
    src/felix86/ir/passes/graph_coloring_pass.cpp
    src/felix86/ir/passes/immediate_transformation_pass.cpp
    src/felix86/ir/passes/local_cse_pass.cpp
    src/felix86/ir/passes/peephole_pass.cpp
    src/felix86/ir/passes/ssa_pass.cpp
    src/felix86/ir/passes/vector_state_pass.cpp
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
    ${FELIX86_AOT_SOURCES}
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
target_sources(felix86_core PRIVATE ${FELIX86_CORE_SOURCES})
target_include_directories(felix86_core PUBLIC ${FELIX86_INCLUDES})
target_link_libraries(felix86_core PUBLIC Zydis fmt biscuit rvasm)

add_executable(felix86)
target_sources(felix86 PRIVATE src/felix86/main.cpp)
target_link_libraries(felix86 PRIVATE felix86_core)

if (BUILD_TESTS)
    set(FELIX86_TEST_SOURCES
        tests/common.cpp
        tests/FEX/fex_test_loader.cpp
        tests/FEX/primary.cpp
        tests/FEX/primary_group.cpp
        tests/FEX/op_size.cpp
        tests/FEX/secondary.cpp
        tests/FEX/rep.cpp
        tests/FEX/two_byte.cpp
        tests/FEX/felix86_bugs.cpp
    )

    add_executable(felix86_test)
    target_sources(felix86_test PRIVATE ${FELIX86_TEST_SOURCES})
    target_include_directories(felix86_test PRIVATE tests)
    target_link_libraries(felix86_test PRIVATE felix86_core Catch2::Catch2WithMain xbyak)
    target_compile_options(felix86_test PRIVATE $<$<COMPILE_LANGUAGE:CPP>:-fno-operator-names>)
    target_compile_definitions(felix86_test PRIVATE CATCH_CONFIG_PREFIX_ALL)
    file(COPY external/FEX/ASM DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(COPY tests/ASM DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()