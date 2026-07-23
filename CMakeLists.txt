
#
# @author hxAri (hxari)
# @create 2025-02-24 15:15
# @update 2026-06-17 20:03
# @github https://github.com/uranite-lang/uranite
#
# Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
# Uranite Licence under GNU General Public Licence v3
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#

cmake_minimum_required( VERSION 3.22 )

# Enable Test Code
enable_testing()

project( Uranite DESCRIPTION "Accelerated Execution Through Quantum Precision" VERSION 2026.7 LANGUAGES C CXX )

# LLVM Configuration (core + support for JSON)
find_program( LLVM_CONFIG NAMES llvm-config llvm-config-19 REQUIRED )
execute_process( COMMAND ${LLVM_CONFIG} --cxxflags OUTPUT_VARIABLE LLVM_CXXFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE )
execute_process( COMMAND ${LLVM_CONFIG} --ldflags OUTPUT_VARIABLE LLVM_LDFLAGS_RAW OUTPUT_STRIP_TRAILING_WHITESPACE )
separate_arguments( LLVM_LDFLAGS NATIVE_COMMAND "${LLVM_LDFLAGS_RAW}" )
execute_process( COMMAND ${LLVM_CONFIG} --libs core support OUTPUT_VARIABLE LLVM_LIBS_RAW OUTPUT_STRIP_TRAILING_WHITESPACE )
separate_arguments( LLVM_LIBS NATIVE_COMMAND "${LLVM_LIBS_RAW}" )
execute_process( COMMAND ${LLVM_CONFIG} --bindir OUTPUT_VARIABLE LLVM_BINDIR OUTPUT_STRIP_TRAILING_WHITESPACE )

# Filter out -std= flags from LLVM to avoid conflict with our C++17 setting
separate_arguments( LLVM_CXXFLAGS_LIST NATIVE_COMMAND "${LLVM_CXXFLAGS}" )
list( FILTER LLVM_CXXFLAGS_LIST EXCLUDE REGEX "^-std=" )

# Dependencies
include( FetchContent )

find_package( argparse QUIET )
if( NOT argparse_FOUND )
    FetchContent_Declare(
        argparse
        GIT_REPOSITORY https://github.com/p-ranav/argparse.git
        GIT_TAG        v3.1
    )
    FetchContent_MakeAvailable( argparse )
endif()

find_package( fmt QUIET )
if( NOT fmt_FOUND )
    FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG        11.0.2
    )
    FetchContent_MakeAvailable( fmt )
endif()

find_package( GTest QUIET )
if( NOT GTest_FOUND )
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
    )
    set( INSTALL_GTEST OFF CACHE BOOL "" FORCE )
    FetchContent_MakeAvailable( googletest )
    include( GoogleTest )
    add_library( GTest::GTest ALIAS gtest )
    add_library( GTest::Main ALIAS gtest_main )
endif()

find_package( spdlog QUIET )
if( NOT spdlog_FOUND )
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.14.1
    )
    FetchContent_MakeAvailable( spdlog )
endif()

find_package( Git QUIET )

include( GNUInstallDirs )

# Set default git hash value
set( _URANITE_GIT_HASH_ "unknown" )

if( GIT_FOUND )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE _URANITE_GIT_HASH_
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

# Adding macro definition to C++
add_definitions( -D_URANITE_COMPILER_VERSION_="1.1.0" )
add_definitions( -D_URANITE_LANGUAGE_DESCRIPTION_="${PROJECT_DESCRIPTION}" )
add_definitions( -D_URANITE_LANGUAGE_VERSION_="${PROJECT_VERSION}" )
add_definitions( -D_URANITE_GIT_HASH_="${_URANITE_GIT_HASH_}" )
add_definitions( -D_URANITE_LLC_="${LLVM_BINDIR}/llc" )
add_definitions( -D_URANITE_OPT_="${LLVM_BINDIR}/opt" )

# Default modules directory path
# In development builds, use the source tree's stdlibs directory so the
# compiler can find modules without --modules-path regardless of CWD.
# For installed/packaged builds, override URANITE_MODULES_INSTALL_DIR.
if( NOT DEFINED URANITE_MODULES_INSTALL_DIR )
    set( URANITE_MODULES_INSTALL_DIR "${CMAKE_SOURCE_DIR}/stdlibs" )
endif()

add_definitions( -D_URANITE_MODULES_DIR_="${URANITE_MODULES_INSTALL_DIR}" )

# Default C runtime directory path
# In development builds, use the build tree's runtime directory.
# For installed/packaged builds, override URANITE_C_RUNTIME_INSTALL_DIR.
if( NOT DEFINED URANITE_C_RUNTIME_INSTALL_DIR )
    set( URANITE_C_RUNTIME_INSTALL_DIR "${CMAKE_BINARY_DIR}/runtime" )
endif()

add_definitions( -D_URANITE_C_RUNTIME_DIR_="${URANITE_C_RUNTIME_INSTALL_DIR}" )

# Source Files
file( GLOB_RECURSE URANITE_SOURCES "${CMAKE_SOURCE_DIR}/src/uranite/*.cpp" )
file( GLOB URANITE_SOURCES_TEST "${CMAKE_SOURCE_DIR}/src/uranite-test/*.cpp" )

# Set Global C++ standard
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

set( URANITE_MAIN src/uranite.cpp )
set( URANITE_SOURCES_LIBRARY ${URANITE_SOURCES} )

# Async Runtime (C library for compiled Uranite programs)
add_library( uranite-async-runtime STATIC src/uranite-crt/async-runtime.c )
target_include_directories( uranite-async-runtime PRIVATE src src/uranite src/uranite-crt )
set_target_properties( uranite-async-runtime PROPERTIES C_STANDARD 11 ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime" )

# Thread Runtime (C library for compiled Uranite programs)
add_library( uranite-thread-runtime STATIC src/uranite-crt/thread-runtime.c )
target_include_directories( uranite-thread-runtime PRIVATE src src/uranite src/uranite-crt )
target_link_libraries( uranite-thread-runtime PRIVATE pthread )
set_target_properties( uranite-thread-runtime PROPERTIES C_STANDARD 11 ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime" )

# Subprocess Runtime (C library for compiled Uranite programs)
add_library( uranite-subprocess-runtime STATIC src/uranite-crt/subprocess-runtime.c )
target_include_directories( uranite-subprocess-runtime PRIVATE src src/uranite src/uranite-crt )
set_target_properties( uranite-subprocess-runtime PROPERTIES C_STANDARD 11 ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime" )

# IPC Runtime (C library for compiled Uranite programs)
add_library( uranite-ipc-runtime STATIC src/uranite-crt/ipc-runtime.c )
target_include_directories( uranite-ipc-runtime PRIVATE src src/uranite src/uranite-crt )
target_link_libraries( uranite-ipc-runtime PRIVATE rt )
set_target_properties( uranite-ipc-runtime PROPERTIES C_STANDARD 11 ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime" )

# Exception Runtime (C library for LLVM-based exception handling)
add_library( uranite-exception-runtime STATIC src/uranite-crt/exception-runtime.c )
target_include_directories( uranite-exception-runtime PRIVATE src src/uranite src/uranite-crt )
set_target_properties( uranite-exception-runtime PROPERTIES C_STANDARD 11 ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime" )

# Define Executable Target
add_executable( uranite ${URANITE_MAIN} ${URANITE_SOURCES_LIBRARY} )
add_executable( uranite-tests ${URANITE_SOURCES_LIBRARY} ${URANITE_SOURCES_TEST} )

# Uranite Formatter
file( GLOB_RECURSE FMT_SOURCES "${CMAKE_SOURCE_DIR}/src/uranite-fmt/*.cpp" )
if( FMT_SOURCES )
    add_executable( uranite-fmt ${FMT_SOURCES} ${URANITE_SOURCES_LIBRARY} )
    target_include_directories( uranite-fmt PRIVATE src src/uranite src/uranite-fmt )
    target_compile_options( uranite-fmt PRIVATE ${LLVM_CXXFLAGS_LIST} -Wall -Wextra -Wno-unused-parameter -Werror=return-type -O2 -fexceptions -g -DNDEBUG )
    target_link_libraries( uranite-fmt PRIVATE argparse::argparse fmt::fmt spdlog::spdlog ${LLVM_LIBS} )
    target_link_options( uranite-fmt PRIVATE ${LLVM_LDFLAGS} )
endif()

# Uranite Documentation Generator
file( GLOB_RECURSE DOC_SOURCES "${CMAKE_SOURCE_DIR}/src/uranite-doc/*.cpp" )
set( COMMENT_EXTRACTOR_SOURCES
    "${CMAKE_SOURCE_DIR}/src/uranite-fmt/comments/comment-extractor.cpp"
    "${CMAKE_SOURCE_DIR}/src/uranite-fmt/comments/comment-reattacher.cpp"
)
if( DOC_SOURCES )
    add_executable( uranite-doc ${DOC_SOURCES} ${COMMENT_EXTRACTOR_SOURCES} ${URANITE_SOURCES_LIBRARY} )
    target_include_directories( uranite-doc PRIVATE src src/uranite src/uranite-doc src/uranite-fmt )
    target_compile_options( uranite-doc PRIVATE ${LLVM_CXXFLAGS_LIST} -Wall -Wextra -Wno-unused-parameter -Werror=return-type -O2 -fexceptions -g -DNDEBUG )
    target_link_libraries( uranite-doc PRIVATE argparse::argparse fmt::fmt spdlog::spdlog ${LLVM_LIBS} )
    target_link_options( uranite-doc PRIVATE ${LLVM_LDFLAGS} )
endif()

# Uranite Package Manager
file( GLOB_RECURSE PKG_SOURCES "${CMAKE_SOURCE_DIR}/src/uranite-pkg/*.cpp" )
if( PKG_SOURCES )
    add_executable( uranite-pkg ${PKG_SOURCES} ${URANITE_SOURCES_LIBRARY} )
    target_include_directories( uranite-pkg PRIVATE src src/uranite src/uranite-pkg )
    target_compile_options( uranite-pkg PRIVATE ${LLVM_CXXFLAGS_LIST} -Wall -Wextra -Wno-unused-parameter -Werror=return-type -O2 -fexceptions -g -DNDEBUG )
    target_link_libraries( uranite-pkg PRIVATE argparse::argparse fmt::fmt spdlog::spdlog ${LLVM_LIBS} )
    target_link_options( uranite-pkg PRIVATE ${LLVM_LDFLAGS} )
endif()

# FFI Runtime (C library for dynamic library loading)
add_library( uranite-ffi-runtime STATIC src/uranite-crt/ffi-runtime.c )
target_link_libraries( uranite-ffi-runtime PRIVATE dl )
set_target_properties( uranite-ffi-runtime PROPERTIES C_STANDARD 11 ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime" )

# Uranite Builtin C-Runtime
add_dependencies(
    uranite
    uranite-async-runtime
    uranite-thread-runtime
    uranite-subprocess-runtime
    uranite-ipc-runtime
    uranite-exception-runtime
    uranite-ffi-runtime
)

# Include Directories (scoped to target)
target_include_directories( uranite PRIVATE src src/uranite )
target_include_directories( uranite-tests PRIVATE src src/uranite )

# Compiler Flags (from llvm-config + additional)
target_compile_options( uranite PRIVATE ${LLVM_CXXFLAGS_LIST} -Wall -Wextra -Wno-unused-parameter -Werror=return-type -O2 -fexceptions -g -DNDEBUG )
target_compile_options( uranite-tests PRIVATE ${LLVM_CXXFLAGS_LIST} -Wall -Wextra -Wno-unused-parameter -Werror=return-type -O2 -fexceptions -g -DNDEBUG )

# Link Dependencies
target_link_libraries( uranite PRIVATE argparse::argparse fmt::fmt spdlog::spdlog ${LLVM_LIBS} )
target_link_libraries( uranite-tests PRIVATE fmt::fmt GTest::GTest GTest::Main spdlog::spdlog ${LLVM_LIBS} pthread )

# Linker Flags (e.g. -L...)
target_link_options( uranite PRIVATE ${LLVM_LDFLAGS} )
target_link_options( uranite-tests PRIVATE ${LLVM_LDFLAGS} )

# Install rules
install( TARGETS uranite RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )

if( FMT_SOURCES )
    install( TARGETS uranite-fmt RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )
endif()

if( DOC_SOURCES )
    install( TARGETS uranite-doc RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )
endif()

if( PKG_SOURCES )
    install( TARGETS uranite-pkg RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )
endif()

install( TARGETS
    uranite-async-runtime
    uranite-thread-runtime
    uranite-subprocess-runtime
    uranite-ipc-runtime
    uranite-exception-runtime
    uranite-ffi-runtime
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/uranite/runtime
)

install( DIRECTORY modules/ DESTINATION ${CMAKE_INSTALL_LIBDIR}/uranite/stdlibs
    FILES_MATCHING PATTERN "*.urn" )

# Register Test
include( GoogleTest )

gtest_discover_tests( uranite-tests )
