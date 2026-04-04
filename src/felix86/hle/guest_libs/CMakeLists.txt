cmake_minimum_required(VERSION 3.16)

# TODO: our thunk library logic causes relocations in .text
# We need to rethink our constructor stuff to use a pointer to .data instead
project(felix86-thunks C ASM_NASM)

if (NOT CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    message(FATAL_ERROR "Thunk libraries require an x86-64 CPU")
endif()

set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)
set(CMAKE_BUILD_TYPE Release) # it doesn't make much sense to enable debug symbols for thunk libs
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
cmake_policy(SET CMP0083 NEW)
include(CheckPIESupported)
check_pie_supported()
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLX REQUIRED gl)
pkg_check_modules(VULKAN REQUIRED vulkan)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(WAYLAND REQUIRED wayland-client)
include_directories(${GLX_INCLUDE_DIRS})
include_directories(${VULKAN_INCLUDE_DIRS})
include_directories(${X11_INCLUDE_DIRS})
include_directories(${WAYLAND_INCLUDE_DIRS})

set(EMPTY_C ${CMAKE_CURRENT_BINARY_DIR}/empty.c)
file(WRITE ${EMPTY_C} "/* empty translation unit for NASM-only shared libraries */\n")

add_library(wayland-client-asm OBJECT libwayland-client.asm)
add_library(wayland-client SHARED libwayland-client.c $<TARGET_OBJECTS:wayland-client-asm>)
set_target_properties(wayland-client PROPERTIES
    OUTPUT_NAME "wayland-client"
    SOVERSION 0
)
 
add_library(vulkan-asm OBJECT libvulkan.asm)
add_library(vulkan SHARED ${EMPTY_C} $<TARGET_OBJECTS:vulkan-asm>)
set_target_properties(vulkan PROPERTIES
    OUTPUT_NAME "vulkan"
    SOVERSION 1
)
 
add_library(GLX-asm OBJECT libGLX.asm)
add_library(GLX SHARED libGLX.c $<TARGET_OBJECTS:GLX-asm>)
set_target_properties(GLX PROPERTIES
    OUTPUT_NAME "GLX"
    SOVERSION 0
)
target_link_libraries(GLX PRIVATE X11)
 
add_library(GL-asm OBJECT libGL.asm)
target_include_directories(GL-asm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_library(GL SHARED libGLX.c $<TARGET_OBJECTS:GL-asm>)
set_target_properties(GL PROPERTIES
    OUTPUT_NAME "GL"
    SOVERSION 1
)
target_link_libraries(GL PRIVATE X11)
 
add_library(luajit-5.1-asm OBJECT libluajit-5.1.asm)
add_library(luajit-5.1 SHARED libluajit-5.1.c $<TARGET_OBJECTS:luajit-5.1-asm>)
set_target_properties(luajit-5.1 PROPERTIES
    OUTPUT_NAME "luajit-5.1"
)