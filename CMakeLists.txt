cmake_minimum_required(VERSION 3.20)

project(felix86)

# add_compile_options(-fsanitize=address)
# add_link_options(-fsanitize=address)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=implicit-fallthrough -Werror=incompatible-pointer-types -Wall")

add_compile_options($<$<COMPILE_LANGUAGE:CPP>:-fno-rtti> $<$<COMPILE_LANGUAGE:CPP>:-fno-exceptions>)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

# add_subdirectory(external/biscuit)
# add_subdirectory(external/plog)
add_subdirectory(external/Catch2)
add_subdirectory(external/xbyak)
# add_subdirectory(external/riscv-disassembler)
add_subdirectory(external/zydis)

set(FELIX86_FRONTEND_SOURCES
    src/felix86/frontend/frontend.c
)

set(FELIX86_IR_SOURCES
    src/felix86/ir/block.c
    src/felix86/ir/function_cache.cpp
    src/felix86/ir/emitter.c
    src/felix86/ir/handlers_weak.c
    src/felix86/ir/handlers.c
    src/felix86/ir/interpreter.c
    src/felix86/ir/instruction.c
    src/felix86/ir/instruction_list.c
    src/felix86/ir/passes.cpp
    src/felix86/ir/print.cpp
)

set(FELIX86_COMMON_SOURCES
    src/felix86/common/allocator.c
    src/felix86/common/file.c
    src/felix86/common/global.c
    src/felix86/common/log.c
)

set(FELIX86_BACKEND_SOURCES
    src/felix86/backend/block_metadata.cpp
)

set(FELIX86_LOADER_SOURCES
    src/felix86/loader/elf.c
    src/felix86/loader/loader.c
)

set(FELIX86_CORE_SOURCES
    ${FELIX86_COMMON_SOURCES}
    ${FELIX86_FRONTEND_SOURCES}
    ${FELIX86_IR_SOURCES}
    ${FELIX86_BACKEND_SOURCES}
    ${FELIX86_LOADER_SOURCES}
    src/felix86/felix86.c
)

set(FELIX86_INCLUDES
    include
    src
    external/robin-map/include
)

add_library(felix86_core STATIC)
target_sources(felix86_core PUBLIC ${FELIX86_CORE_SOURCES})
target_include_directories(felix86_core PUBLIC ${FELIX86_INCLUDES})
target_link_libraries(felix86_core PUBLIC Zydis)

add_executable(felix86)
target_sources(felix86 PRIVATE src/felix86/main.c src/felix86/gui.cpp)
target_link_libraries(felix86 PRIVATE felix86_core)

set(FELIX86_TEST_SOURCES
    tests/add.cpp
    tests/and.cpp
    tests/sub.cpp
    tests/mov.cpp
    tests/lea.cpp
    tests/special.cpp
    tests/xmm.cpp
    tests/ir/control_flow.cpp
)

add_executable(felix86_test)
target_sources(felix86_test PRIVATE ${FELIX86_TEST_SOURCES})
target_include_directories(felix86_test PRIVATE tests)
target_link_libraries(felix86_test PRIVATE felix86_core Catch2::Catch2WithMain xbyak)
target_compile_options(felix86_test PRIVATE $<$<COMPILE_LANGUAGE:CPP>:-fno-operator-names>)