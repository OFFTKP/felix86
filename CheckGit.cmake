find_package(Git QUIET)

function(get_git_hash variable)
    if(NOT GIT_FOUND)
        set(${variable} "GIT-NOTFOUND" PARENT_SCOPE)
        return()
    endif()

    execute_process(COMMAND
        "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
        RESULT_VARIABLE res
        OUTPUT_VARIABLE out
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)

    if(NOT res EQUAL 0)
        set(out "${out}-${res}-NOTFOUND")
    endif()

    set(${variable} "${out}" PARENT_SCOPE)
endfunction()

if (NOT DEFINED pre_configure_dir)
    set(pre_configure_dir ${CMAKE_CURRENT_LIST_DIR}/src/felix86/common)
endif()

if (NOT DEFINED post_configure_dir)
    set(post_configure_dir ${CMAKE_BINARY_DIR}/generated)
endif()

set(pre_configure_file ${pre_configure_dir}/git_version.cpp.in)
set(post_configure_file ${post_configure_dir}/git_version.cpp)

function(CheckGitVersion)
    get_git_hash(GIT_HASH)

    if (NOT EXISTS ${post_configure_dir})
        file(MAKE_DIRECTORY ${post_configure_dir})
    endif()

    if (NOT GIT_HASH)
        set(GIT_HASH UNKNOWN)
    endif()


    # Only update the git_version.cpp if the hash has changed. This will
    # prevent us from rebuilding the project more than we need to.
    if (NOT GIT_HASH STREQUAL GIT_HASH_CACHE OR
        NOT EXISTS "${post_configure_file}")
        # Set the GIT_HASH_CACHE variable so the next build won't have
        # to regenerate the source file.
        set(GIT_HASH_CACHE "${GIT_HASH}" CACHE INTERNAL "" FORCE)

        configure_file(${pre_configure_file} ${post_configure_file} @ONLY)
    endif()
endfunction()

function(CheckGitSetup)
    add_custom_target(AlwaysCheckGit COMMAND ${CMAKE_COMMAND}
        -DRUN_CHECK_GIT_VERSION=1
        -Dpre_configure_dir=${pre_configure_dir}
        -Dpost_configure_dir=${post_configure_dir}
        -DGIT_HASH_CACHE=${GIT_HASH_CACHE}
        -P ${CMAKE_CURRENT_LIST_DIR}/CheckGit.cmake
        BYPRODUCTS ${post_configure_file})

    add_library(git_version ${CMAKE_BINARY_DIR}/generated/git_version.cpp)
    target_include_directories(git_version PUBLIC ${CMAKE_BINARY_DIR}/generated)
    add_dependencies(git_version AlwaysCheckGit)

    CheckGitVersion()
endfunction()

# This is used to run this function from an external cmake process.
if (RUN_CHECK_GIT_VERSION)
    CheckGitVersion()
endif()
