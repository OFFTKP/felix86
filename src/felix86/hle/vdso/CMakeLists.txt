cmake_minimum_required(VERSION 3.16)

find_program(NASM nasm REQUIRED)
find_program(XXD xxd REQUIRED)

if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "riscv64")
    set(FELIX86_X86_CROSS x86_64-linux-gnu)
else()
    set(FELIX86_X86_CROSS "")
endif()

if(FELIX86_X86_CROSS)
    find_program(CROSS_LD "${FELIX86_X86_CROSS}-ld")
    find_program(CROSS_STRIP "${FELIX86_X86_CROSS}-strip")
    find_program(CROSS_READELF "${FELIX86_X86_CROSS}-readelf")
else()
    message("Cross-compiler not used for VDSO")
    find_program(CROSS_LD ld REQUIRED)
    find_program(CROSS_STRIP strip)
    find_program(CROSS_READELF readelf)
endif()

add_custom_command(
    OUTPUT x64_vdso.c
    COMMAND ${NASM} -f elf64 ${CMAKE_CURRENT_SOURCE_DIR}/x64-linux-vdso.asm -o x64-linux-vdso.o
    COMMAND ${CROSS_LD} -shared --hash-style=both
            -T ${CMAKE_CURRENT_SOURCE_DIR}/x64-vdso-version.map
            -soname=linux-vdso.so.1
            -o x64-linux-vdso.so.1
            x64-linux-vdso.o
    # Ensure everything is fine, gvisor tests depend on some of these
    COMMAND ${CROSS_READELF} -h x64-linux-vdso.so.1 | grep -q "Class:.*ELF64"
    COMMAND ${CROSS_READELF} -h x64-linux-vdso.so.1 | grep -q "Machine:.*X86-64"
    COMMAND ${CROSS_READELF} -h x64-linux-vdso.so.1 | grep -q "Type:.*DYN"
    COMMAND ${CROSS_READELF} -d x64-linux-vdso.so.1 | grep -q "HASH"
    COMMAND ${CROSS_READELF} -d x64-linux-vdso.so.1 | grep -q "SYMTAB"
    COMMAND ${CROSS_READELF} -d x64-linux-vdso.so.1 | grep -q "STRTAB"
    COMMAND ${CROSS_READELF} -d x64-linux-vdso.so.1 | grep -q "VERSYM"
    COMMAND ${CROSS_READELF} -d x64-linux-vdso.so.1 | grep -q "VERDEF"
    COMMAND ${CROSS_READELF} -d x64-linux-vdso.so.1 | grep -q "STRSZ"
    COMMAND ${CROSS_READELF} -d x64-linux-vdso.so.1 | grep -q "SONAME"
    COMMAND ${CROSS_STRIP} -s x64-linux-vdso.so.1
    COMMAND ${XXD} -i x64-linux-vdso.so.1 x64_vdso.c
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/x64-linux-vdso.asm
            ${CMAKE_CURRENT_SOURCE_DIR}/x64-vdso-version.map
    COMMENT "Building and validating x64_vdso.c"
)

add_library(x64_vdso STATIC x64_vdso.c)
