
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

enable_testing()

project( Uranite DESCRIPTION "Accelerated Execution Through Quantum Precision" VERSION 2026.8 LANGUAGES C CXX )

find_program( LLVM_CONFIG NAMES llvm-config llvm-config-19 REQUIRED )
execute_process( COMMAND ${LLVM_CONFIG} --cxxflags OUTPUT_VARIABLE LLVM_CXXFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE )
execute_process( COMMAND ${LLVM_CONFIG} --ldflags OUTPUT_VARIABLE LLVM_LDFLAGS_RAW OUTPUT_STRIP_TRAILING_WHITESPACE )
separate_arguments( LLVM_LDFLAGS NATIVE_COMMAND "${LLVM_LDFLAGS_RAW}" )
execute_process( COMMAND ${LLVM_CONFIG} --libs core support OUTPUT_VARIABLE LLVM_LIBS_RAW OUTPUT_STRIP_TRAILING_WHITESPACE )
separate_arguments( LLVM_LIBS NATIVE_COMMAND "${LLVM_LIBS_RAW}" )
execute_process( COMMAND ${LLVM_CONFIG} --bindir OUTPUT_VARIABLE LLVM_BINDIR OUTPUT_STRIP_TRAILING_WHITESPACE )
execute_process( COMMAND ${LLVM_CONFIG} --includedir OUTPUT_VARIABLE LLVM_INCLUDEDIR OUTPUT_STRIP_TRAILING_WHITESPACE )

separate_arguments( LLVM_CXXFLAGS_LIST NATIVE_COMMAND "${LLVM_CXXFLAGS}" )
list( FILTER LLVM_CXXFLAGS_LIST EXCLUDE REGEX "^-std=" )

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

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

if( NOT DEFINED URANITE_MODULES_INSTALL_DIR )
    set( URANITE_MODULES_INSTALL_DIR "${CMAKE_SOURCE_DIR}/stdlibs" )
endif()

if( NOT DEFINED URANITE_C_RUNTIME_INSTALL_DIR )
    set( URANITE_C_RUNTIME_INSTALL_DIR "${CMAKE_BINARY_DIR}/runtime" )
endif()

file( GLOB_RECURSE URANITE_SOURCES "${CMAKE_SOURCE_DIR}/src/uranite/*.cpp" )
file( GLOB URANITE_SOURCES_TEST "${CMAKE_SOURCE_DIR}/src/uranite-test/*.cpp" )

add_library( uranite-core SHARED ${URANITE_SOURCES} )

target_include_directories( uranite-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/uranite>
        $<BUILD_INTERFACE:${LLVM_INCLUDEDIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/uranite>
)

target_compile_definitions( uranite-core PUBLIC
    _URANITE_COMPILER_VERSION_="1.3.0"
    _URANITE_LANGUAGE_DESCRIPTION_="${PROJECT_DESCRIPTION}"
    _URANITE_LANGUAGE_VERSION_="${PROJECT_VERSION}"
    _URANITE_GIT_HASH_="${_URANITE_GIT_HASH_}"
    _URANITE_LLC_="${LLVM_BINDIR}/llc"
    _URANITE_OPT_="${LLVM_BINDIR}/opt"
    _URANITE_MODULES_DIR_="${URANITE_MODULES_INSTALL_DIR}"
    _URANITE_C_RUNTIME_DIR_="${URANITE_C_RUNTIME_INSTALL_DIR}"
)

target_compile_options( uranite-core PRIVATE
    ${LLVM_CXXFLAGS_LIST} -Wall -Wextra -Wno-unused-parameter
    -Werror=return-type -O2 -fexceptions -g -DNDEBUG
)

target_link_libraries( uranite-core PUBLIC fmt::fmt spdlog::spdlog ${LLVM_LIBS} )
target_link_options( uranite-core PUBLIC ${LLVM_LDFLAGS} )

set_target_properties( uranite-core PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    OUTPUT_NAME uranite-core
)

add_library( uranite-async-runtime STATIC src/uranite-crt/async-runtime.c )
target_include_directories( uranite-async-runtime PRIVATE src src/uranite src/uranite-crt )
set_target_properties( uranite-async-runtime PROPERTIES C_STANDARD 11 ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime" )

add_library( uranite-thread-runtime STATIC src/uranite-crt/thread-runtime.c )
target_include_directories( uranite-thread-runtime PRIVATE src src/uranite src/uranite-crt )
target_link_libraries( uranite-thread-runtime PRIVATE pthread )
set_target_properties( uranite-thread-runtime PROPERTIES C_STANDARD 11 ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime" )

add_library( uranite-subprocess-runtime STATIC src/uranite-crt/subprocess-runtime.c )
target_include_directories( uranite-subprocess-runtime PRIVATE src src/uranite src/uranite-crt )
set_target_properties( uranite-subprocess-runtime PROPERTIES C_STANDARD 11 ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime" )

add_library( uranite-ipc-runtime STATIC src/uranite-crt/ipc-runtime.c )
target_include_directories( uranite-ipc-runtime PRIVATE src src/uranite src/uranite-crt )
target_link_libraries( uranite-ipc-runtime PRIVATE rt )
set_target_properties( uranite-ipc-runtime PROPERTIES C_STANDARD 11 ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime" )

add_library( uranite-exception-runtime STATIC src/uranite-crt/exception-runtime.c )
target_include_directories( uranite-exception-runtime PRIVATE src src/uranite src/uranite-crt )
set_target_properties( uranite-exception-runtime PROPERTIES C_STANDARD 11 ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime" )

add_library( uranite-ffi-runtime STATIC src/uranite-crt/ffi-runtime.c )
target_link_libraries( uranite-ffi-runtime PRIVATE dl )
set_target_properties( uranite-ffi-runtime PROPERTIES C_STANDARD 11 ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime" )

add_executable( uranite src/uranite.cpp )
target_link_libraries( uranite PRIVATE uranite-core argparse::argparse )

add_dependencies( uranite
    uranite-async-runtime
    uranite-thread-runtime
    uranite-subprocess-runtime
    uranite-ipc-runtime
    uranite-exception-runtime
    uranite-ffi-runtime
)

add_executable( uranite-tests ${URANITE_SOURCES_TEST} )
target_link_libraries( uranite-tests PRIVATE uranite-core GTest::GTest GTest::Main pthread )

file( GLOB_RECURSE FMT_SOURCES "${CMAKE_SOURCE_DIR}/src/uranite-fmt/*.cpp" )
if( FMT_SOURCES )
    add_executable( uranite-fmt ${FMT_SOURCES} )
    target_include_directories( uranite-fmt PRIVATE src/uranite-fmt )
    target_link_libraries( uranite-fmt PRIVATE uranite-core argparse::argparse )
endif()

file( GLOB_RECURSE DOC_SOURCES "${CMAKE_SOURCE_DIR}/src/uranite-doc/*.cpp" )
set( COMMENT_EXTRACTOR_SOURCES
    "${CMAKE_SOURCE_DIR}/src/uranite-fmt/comments/comment-extractor.cpp"
    "${CMAKE_SOURCE_DIR}/src/uranite-fmt/comments/comment-reattacher.cpp"
)
if( DOC_SOURCES )
    add_executable( uranite-doc ${DOC_SOURCES} ${COMMENT_EXTRACTOR_SOURCES} )
    target_include_directories( uranite-doc PRIVATE src/uranite-doc src/uranite-fmt )
    target_link_libraries( uranite-doc PRIVATE uranite-core argparse::argparse )
endif()

file( GLOB_RECURSE PKG_SOURCES "${CMAKE_SOURCE_DIR}/src/uranite-pkg/*.cpp" )
if( PKG_SOURCES )
    add_executable( uranite-pkg ${PKG_SOURCES} )
    target_include_directories( uranite-pkg PRIVATE src/uranite-pkg )
    target_link_libraries( uranite-pkg PRIVATE uranite-core argparse::argparse )
endif()

install( TARGETS uranite-core
    EXPORT UraniteCoreTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/uranite
)

install( DIRECTORY src/uranite/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/uranite
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

install( EXPORT UraniteCoreTargets
    FILE UraniteCoreTargets.cmake
    NAMESPACE uranite::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UraniteCore
)

include( CMakePackageConfigHelpers )

configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmake/UraniteCoreConfig.cmake.in"
    "${CMAKE_BINARY_DIR}/UraniteCoreConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UraniteCore
)

write_basic_package_version_file(
    "${CMAKE_BINARY_DIR}/UraniteCoreConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install( FILES
    "${CMAKE_BINARY_DIR}/UraniteCoreConfig.cmake"
    "${CMAKE_BINARY_DIR}/UraniteCoreConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UraniteCore
)

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

install( DIRECTORY stdlibs/ DESTINATION ${CMAKE_INSTALL_LIBDIR}/uranite/stdlibs
    FILES_MATCHING PATTERN "*.urn" )

include( GoogleTest )

gtest_discover_tests( uranite-tests )
