project(biscuit_tests)

add_executable(${PROJECT_NAME}
    src/assembler_autocompress_tests.cpp
    src/assembler_bfloat_tests.cpp
    src/assembler_branch_tests.cpp
    src/assembler_cfi_tests.cpp
    src/assembler_cmo_tests.cpp
    src/assembler_privileged_tests.cpp
    src/assembler_rv32i_tests.cpp
    src/assembler_rv64i_tests.cpp
    src/assembler_rva_tests.cpp
    src/assembler_rvb_tests.cpp
    src/assembler_rvc_tests.cpp
    src/assembler_rvd_tests.cpp
    src/assembler_rvf_tests.cpp
    src/assembler_rvk_tests.cpp
    src/assembler_rvm_tests.cpp
    src/assembler_rvq_tests.cpp
    src/assembler_rvv_tests.cpp
    src/assembler_vector_crypto_tests.cpp
    src/assembler_xthead_tests.cpp
    src/assembler_zabha_tests.cpp
    src/assembler_zacas_tests.cpp
    src/assembler_zalasr_tests.cpp
    src/assembler_zawrs_tests.cpp
    src/assembler_zc_tests.cpp
    src/assembler_zfa_tests.cpp
    src/assembler_zicond_tests.cpp
    src/assembler_zicsr_tests.cpp
    src/assembler_zihintntl_tests.cpp
    src/main.cpp

    src/assembler_test_utils.hpp
)

target_include_directories(${PROJECT_NAME}
PRIVATE
    externals/
)

target_link_libraries(${PROJECT_NAME}
PRIVATE
    biscuit
)

target_compile_features(${PROJECT_NAME}
PRIVATE
    cxx_std_20
)

if (MSVC)
    target_compile_options(${PROJECT_NAME}
    PRIVATE
        /MP
        /Zi
        /Zo
        /permissive-
        /EHsc
        /utf-8
        /volatile:iso
        /Zc:externConstexpr
        /Zc:inline
        /Zc:throwingNew

        # Warnings
        /W4
        /we4062 # enumerator 'identifier' in a switch of enum 'enumeration' is not handled
        /we4101 # 'identifier': unreferenced local variable
        /we4265 # 'class': class has virtual functions, but destructor is not virtual
        /we4388 # signed/unsigned mismatch
        /we4547 # 'operator' : operator before comma has no effect; expected operator with side-effect
        /we4549 # 'operator1': operator before comma has no effect; did you intend 'operator2'?
        /we4555 # Expression has no effect; expected expression with side-effect
        /we4715 # 'function': not all control paths return a value
        /we4834 # Discarding return value of function with 'nodiscard' attribute
        /we5038 # data member 'member1' will be initialized after data member 'member2'
    )
endif()

add_test(biscuit_tests_ctest ${PROJECT_NAME})