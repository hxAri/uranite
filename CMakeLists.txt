
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

cmake_minimum_required( VERSION 3.25 )

# Enable Test Code
enable_testing()

project( Uranite DESCRIPTION "Accelerated Execution Through Quantum Precision" VERSION 2026.1 LANGUAGES C CXX )

# LLVM Configuration (core + support for JSON)
execute_process( COMMAND llvm-config --cxxflags OUTPUT_VARIABLE LLVM_CXXFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE )
execute_process( COMMAND llvm-config --ldflags OUTPUT_VARIABLE LLVM_LDFLAGS_RAW OUTPUT_STRIP_TRAILING_WHITESPACE )
separate_arguments( LLVM_LDFLAGS NATIVE_COMMAND "${LLVM_LDFLAGS_RAW}" )
execute_process( COMMAND llvm-config --libs core support OUTPUT_VARIABLE LLVM_LIBS_RAW OUTPUT_STRIP_TRAILING_WHITESPACE )
separate_arguments( LLVM_LIBS NATIVE_COMMAND "${LLVM_LIBS_RAW}" )

# Filter out -std= flags from LLVM to avoid conflict with our C++17 setting
separate_arguments( LLVM_CXXFLAGS_LIST NATIVE_COMMAND "${LLVM_CXXFLAGS}" )
list( FILTER LLVM_CXXFLAGS_LIST EXCLUDE REGEX "^-std=" )

# Dependencies
find_package( argparse REQUIRED )
find_package( fmt REQUIRED )
find_package( Git QUIET )
find_package( GTest REQUIRED )
find_package( spdlog REQUIRED )

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
add_definitions( -D_URANITE_COMPILER_VERSION_="1.0.0" )
add_definitions( -D_URANITE_LANGUAGE_DESCRIPTION_="${PROJECT_DESCRIPTION}" )
add_definitions( -D_URANITE_LANGUAGE_VERSION_="${PROJECT_VERSION}" )
add_definitions( -D_URANITE_GIT_HASH_="${_URANITE_GIT_HASH_}" )

# Default modules directory path (for installed builds)
if( NOT DEFINED URANITE_MODULES_INSTALL_DIR )
    set( URANITE_MODULES_INSTALL_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/uranite/stdlibs" )
endif()

add_definitions( -D_URANITE_MODULES_DIR_="${URANITE_MODULES_INSTALL_DIR}" )

# Default C runtime directory path (for installed builds)
if( NOT DEFINED URANITE_C_RUNTIME_INSTALL_DIR )
    set( URANITE_C_RUNTIME_INSTALL_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/uranite/runtime" )
endif()

add_definitions( -D_URANITE_C_RUNTIME_DIR_="${URANITE_C_RUNTIME_INSTALL_DIR}" )

# Source Files
file( GLOB_RECURSE URANITE_SOURCES "${CMAKE_SOURCE_DIR}/src/uranite/*.cpp" )
file( GLOB URANITE_SOURCES_TEST "${CMAKE_SOURCE_DIR}/src/uranite-test/*.cpp" )
file( GLOB_RECURSE LSP_SOURCES_LIB "${CMAKE_SOURCE_DIR}/src/uranite-lsp/*.cpp" )
list( FILTER LSP_SOURCES_LIB EXCLUDE REGEX "lsp\\.cpp$" )

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
add_executable( uranite-tests ${URANITE_SOURCES_LIBRARY} ${LSP_SOURCES_LIB} ${URANITE_SOURCES_TEST} )

# Uranite LSP Server
file( GLOB_RECURSE LSP_SOURCES "${CMAKE_SOURCE_DIR}/src/uranite-lsp/*.cpp" )
if( LSP_SOURCES )
    add_executable( uranite-lsp ${LSP_SOURCES} ${URANITE_SOURCES_LIBRARY} )
    target_include_directories( uranite-lsp PRIVATE src src/uranite src/uranite-lsp )
    target_compile_options( uranite-lsp PRIVATE ${LLVM_CXXFLAGS_LIST} -Wall -Wextra -Wno-unused-parameter -Werror=return-type -O2 -fexceptions -g -DNDEBUG )
    target_link_libraries( uranite-lsp PRIVATE argparse::argparse fmt::fmt spdlog::spdlog ${LLVM_LIBS} pthread )
    target_link_options( uranite-lsp PRIVATE ${LLVM_LDFLAGS} )
endif()

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
target_include_directories( uranite-tests PRIVATE src src/uranite src/uranite-lsp )

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

if( LSP_SOURCES )
    install( TARGETS uranite-lsp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )
endif()

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
