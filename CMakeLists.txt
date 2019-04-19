# skale: Skale C++ client, tools and libraries.
# Licensed under the GNU General Public License, Version 3. See the LICENSE file.

cmake_minimum_required(VERSION 3.5.1)

############################################################################################################
############################################################################################################
############################################################################################################
set( DEPS_SOURCES_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/SkaleDeps" )
set( DEPS_INSTALL_ROOT "${DEPS_SOURCES_ROOT}/deps_inst/x86_or_x64" )
include_directories( "${DEPS_INSTALL_ROOT}/include" "${DEPS_INSTALL_ROOT}/include/cryptopp" ${CMAKE_BINARY_DIR}/deps/include )
link_directories( "${DEPS_INSTALL_ROOT}/lib" )
set( CMAKE_PREFIX_PATH "${DEPS_INSTALL_ROOT}" )

link_directories(${CMAKE_BINARY_DIR}/deps/lib)          # HACK for not-found -lff in testeth
############################################################################################################
############################################################################################################
############################################################################################################

include(evmc/cmake/cable/bootstrap.cmake)
include(CableBuildInfo)
include(CableBuildType)
include(CableToolchains)
include(GNUInstallDirs)

if(UNIX)
    # Build deps with PIC to allow building shared libraries.
    set(toolchain cxx11-pic)
else()
    set(toolchain cxx11)
endif()
cable_configure_toolchain(DEFAULT ${toolchain})

set(ETH_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}/cmake" CACHE PATH "The path to the cmake directory")
list(APPEND CMAKE_MODULE_PATH ${ETH_CMAKE_DIR})

# Map current configuration to configurations of imported targets.
set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG Release)
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)

set(HUNTER_CONFIGURATION_TYPES Release)
set(HUNTER_JOBS_NUMBER 4)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

# In CI builds upload the binaries if the GITHUB_USER_PASSWORD was decrypted
# (only for branches and internal PRs).
if("$ENV{CI}" AND NOT "$ENV{GITHUB_USER_PASSWORD}" STREQUAL "")
    set(run_upload YES)
else()
    set(run_upload NO)
endif()
option( HUNTER_RUN_UPLOAD "Upload binaries to the cache server" ${run_upload} )

include( HunterGate )
HunterGate( URL "https://github.com/ruslo/hunter/archive/v0.23.76.tar.gz" SHA1 "c7b60993e841850e2c449afd454f5d5aa4ec04e4" LOCAL )

option( SKALED_LATEST_STANDARD "Use latest standards" ON )
if( SKALED_LATEST_STANDARD )
    set( CMAKE_CXX_STANDARD 17 )
else()
    set( CMAKE_CXX_STANDARD 14 )
endif()

project(skale)
file (READ VERSION PROJECT_VERSION)
string(STRIP ${PROJECT_VERSION} PROJECT_VERSION)

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/evmjit/.git)
    message(FATAL_ERROR "Git submodules not initialized, execute:\n  git submodule update --init")
endif()

cable_set_build_type(DEFAULT RelWithDebInfo CONFIGURATION_TYPES Debug;Release;RelWithDebInfo)

cable_add_buildinfo_library(PROJECT_NAME skale)

set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY On)

include(EthCcache)
include(EthOptions)
include(EthCompilerSettings)
include(EthExecutableHelper)
include(EthDependencies)
include(EthUtils)

set(CLANG_FORMAT_EXCLUDE_PATTERNS
    ${CMAKE_BINARY_DIR}
    ${DEPS_SOURCES_ROOT}
    "${CMAKE_CURRENT_SOURCE_DIR}/evmc/"
    "${CMAKE_CURRENT_SOURCE_DIR}/evmjit/"
    "${CMAKE_CURRENT_SOURCE_DIR}/libconsensus/")
include(FindClangFormat)
#find_package(ClangFormat)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
hunter_add_package(Boost COMPONENTS program_options filesystem system thread context fiber log chrono)
find_package(Boost CONFIG REQUIRED program_options filesystem system thread context fiber log chrono)

hunter_add_package( leveldb )
find_package( leveldb CONFIG REQUIRED )

#hunter_add_package(jsoncpp)
find_package( jsoncpp PATHS "${DEPS_INSTALL_ROOT}/libs/cmake" ) #(jsoncpp CONFIG REQUIRED)

hunter_add_package(Snappy)
find_package(Snappy CONFIG REQUIRED)

if( SKALED_LATEST_STANDARD )
    set( LIB_NAME_cryptopp "cryptopp" )
else()
    hunter_add_package(cryptopp)
    find_package( cryptopp CONFIG REQUIRED )
    set( LIB_NAME_cryptopp "cryptopp-static" )
endif()

#hunter_add_package(libjson-rpc-cpp)
find_package( libjson-rpc-cpp PATHS "${DEPS_SOURCES_ROOT}/libjson-rpc-cpp/cmake" ) #(libjson-rpc-cpp CONFIG REQUIRED)

hunter_add_package(libscrypt)
find_package(libscrypt CONFIG REQUIRED)

hunter_add_package(ethash)
find_package(ethash CONFIG REQUIRED)

include(ProjectSecp256k1)
include(ProjectLibFF)

find_package(Threads)

if(MINIUPNPC)
    find_package(Miniupnpc 1.8.2013 REQUIRED)
endif()

set(UTILS_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/utils")

configure_project()

if(TESTS)
    # Import binaryen library for testeth.
    # TODO: remove this once testeth uses binaryen as a CLI tool.
    include(ProjectBinaryen)
endif()

add_subdirectory(evmc)

#Global include path for all libs.
include_directories("${CMAKE_SOURCE_DIR}")

option(SKALE_INTERPRETER_SHARED "Build skale-interpreter as a shared library" OFF)
add_subdirectory(libskale-interpreter)

add_subdirectory(libdevcore)
add_subdirectory(libdevcrypto)
add_subdirectory(libp2p)


set( SKUTILS_INCLUDE_DIRS
	"${CMAKE_SOURCE_DIR}/libskutils/include"
	"${CMAKE_SOURCE_DIR}/libconsensus/thirdparty"
	)
include_directories( ${SKUTILS_INCLUDE_DIRS} )
add_subdirectory(libskutils)

add_subdirectory(libskale)

add_subdirectory(libethcore)
add_subdirectory(libevm)
add_subdirectory(libethereum)
add_subdirectory(libethashseal)

add_subdirectory(libwebthree)
add_subdirectory(libweb3jsonrpc)

if (EVMJIT)
    add_subdirectory(evmjit)
endif()

if (CONSENSUS)
    set( BLS_WITH_SECP256K1_INCLUDE_DIR ${CMAKE_BINARY_DIR}/deps/include )
    set( BLS_WITH_SECP256K1_LIBRARY_DIR ${CMAKE_BINARY_DIR}/deps/lib )
    set( BLS_WITH_FF_INCLUDE_DIR ${CMAKE_BINARY_DIR}/deps/include )
    set( BLS_WITH_FF_LIBRARY_DIR ${CMAKE_BINARY_DIR}/deps/lib )
    message( INFO " - Using root BLS INC=${BLS_WITH_FF_INCLUDE_DIR}")
    message( INFO " - Using root BLS LIB=${BLS_WITH_FF_LIBRARY_DIR}")
endif()

add_subdirectory(skaled)

if (TOOLS)
    add_subdirectory(skale-key)
    add_subdirectory(skale-vm)
    add_subdirectory(rlp)
endif()

if (TESTS)
    enable_testing()
    add_subdirectory(test)
    add_subdirectory(storage_benchmark)
endif()

set(CPACK_GENERATOR TGZ)
set(CPACK_PACKAGE_FILE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY FALSE)
include(CPack)

if (CONSENSUS)
    add_subdirectory(libconsensus)
endif()
