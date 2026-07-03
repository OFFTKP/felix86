cmake_minimum_required(VERSION 3.16)

find_program(NASM nasm REQUIRED)
find_program(XXD xxd REQUIRED)

if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "amd64|x86_64")
    set(FELIX86_X86_CROSS_DEFAULT "")
else()
    set(FELIX86_X86_CROSS_DEFAULT "x86_64-linux-gnu-")
endif()
set(FELIX86_X86_CROSS "${FELIX86_X86_CROSS_DEFAULT}" CACHE STRING "Prefix for x86-64 binutils (e.g. x86_64-linux-gnu- or /opt/x86_64-linux-gnu/bin/x86_64-linux-gnu-)")

if(FELIX86_X86_CROSS)
    message(STATUS "x86-64 binutils prefix: ${FELIX86_X86_CROSS}")
elseif(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "amd64|x86_64")
    message(STATUS "Using host binutils")
else()
    message(FATAL_ERROR "x86-64 binutils required for VDSO, set FELIX86_X86_CROSS to an x86-64 binutils prefix")
endif()

find_program(CROSS_LD "${FELIX86_X86_CROSS}ld")
find_program(CROSS_STRIP "${FELIX86_X86_CROSS}strip")
find_program(CROSS_READELF "${FELIX86_X86_CROSS}readelf")

if(NOT CROSS_LD)
    message(FATAL_ERROR "Could not find ${FELIX86_X86_CROSS}ld, set FELIX86_X86_CROSS to an x86-64 binutils prefix")
endif()

if(NOT CROSS_STRIP)
    message(FATAL_ERROR "Could not find ${FELIX86_X86_CROSS}strip, set FELIX86_X86_CROSS to an x86-64 binutils prefix")
endif()

if(NOT CROSS_READELF)
    message(FATAL_ERROR "Could not find ${FELIX86_X86_CROSS}readelf, set FELIX86_X86_CROSS to an x86-64 binutils prefix")
endif()

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/x64_vdso.c
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
    COMMAND ${CROSS_READELF} -d x64-linux-vdso.so.1 | grep -q "GNU_HASH"
    COMMAND ${CROSS_READELF} -d x64-linux-vdso.so.1 | grep -q "SYMTAB"
    COMMAND ${CROSS_READELF} -d x64-linux-vdso.so.1 | grep -q "STRTAB"
    COMMAND ${CROSS_READELF} -d x64-linux-vdso.so.1 | grep -q "VERSYM"
    COMMAND ${CROSS_READELF} -d x64-linux-vdso.so.1 | grep -q "VERDEF"
    COMMAND ${CROSS_READELF} -d x64-linux-vdso.so.1 | grep -q "STRSZ"
    COMMAND ${CROSS_READELF} -d x64-linux-vdso.so.1 | grep -q "SONAME"
    COMMAND ${CROSS_STRIP} -s x64-linux-vdso.so.1
    COMMAND ${XXD} -i x64-linux-vdso.so.1 ${CMAKE_CURRENT_BINARY_DIR}/x64_vdso.c
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/x64-linux-vdso.asm
            ${CMAKE_CURRENT_SOURCE_DIR}/x64-vdso-version.map
    COMMENT "Building and validating x64_vdso.c"
)

add_library(x64_vdso STATIC ${CMAKE_CURRENT_BINARY_DIR}/x64_vdso.c)
