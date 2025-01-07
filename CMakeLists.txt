# skale: Skale C++ client, tools and libraries.
# Licensed under the GNU General Public License, Version 3. See the LICENSE file.
#

cmake_minimum_required(VERSION 3.5.1)

option( SKALE_SKIP_INSTALLING_DIRECTIVES "Do not use CMAKE install() directives " ON )

############################################################################################################
############################################################################################################
############################################################################################################
set( DEPS_SOURCES_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/deps" )
set( DEPS_INSTALL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/libconsensus/libBLS/deps/deps_inst/x86_or_x64" )
include_directories( "${DEPS_INSTALL_ROOT}/include" "${DEPS_INSTALL_ROOT}/include/cryptopp" ${CMAKE_BINARY_DIR}/deps/include libconsensus)
link_directories( "${DEPS_INSTALL_ROOT}/lib" )
set( CMAKE_PREFIX_PATH "${DEPS_INSTALL_ROOT}" )
list( APPEND CMAKE_MODULE_PATH ${DEPS_INSTALL_ROOT}/lib/cmake )

link_directories( ${CMAKE_BINARY_DIR}/deps/lib )          # HACK for not-found -lff in testeth

if( NOT CMAKE_BUILD_TYPE MATCHES "Debug" )
    set( CMAKE_CXX_FLAGS           "${CMAKE_CXX_FLAGS}           -rdynamic" )
    set( CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS}    -rdynamic" )
    set( CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -rdynamic" )
endif()

option( SKALED_PROFILING "Build for profiling" OFF )
if( SKALED_PROFILING )
    set( CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -pg" )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg" )
    set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg" )
    set( CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pg" )
endif()

if( CMAKE_BUILD_TYPE STREQUAL "Release" )
    set( CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -O3" )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3" )
elseif( CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" )
    set( CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -O3" )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3" )
elseif( CMAKE_BUILD_TYPE STREQUAL "MinSizeRel" )
    set( CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -Os" )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os" )
elseif( CMAKE_BUILD_TYPE STREQUAL "Debug" )
    set( CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -O0" )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0" )
else()
	message( INFO "---- CMAKE_BUILD_TYPE = ${DEPS_INSTALL_ROOT} is not supported explicitly" )
endif()

############################################################################################################
############################################################################################################
############################################################################################################

if( NOT EXISTS ${CMAKE_SOURCE_DIR}/evmc/.git OR NOT EXISTS ${CMAKE_SOURCE_DIR}/cmake/cable/.git )
    message( FATAL_ERROR "Git submodules not initialized, execute:\n  git submodule update --init" )
endif()

include( cmake/cable/bootstrap.cmake )
include( CableBuildInfo )
include( CableBuildType )
include( CableToolchains )
include( GNUInstallDirs )

if( UNIX )
    # Build deps with PIC to allow building shared libraries.
    set( toolchain cxx17-pic )
else()
    set( toolchain cxx17 )
endif()
cable_configure_toolchain( DEFAULT ${toolchain} )

set( ETH_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}/cmake" CACHE PATH "The path to the cmake directory" )
list( APPEND CMAKE_MODULE_PATH ${ETH_CMAKE_DIR} )

# Map current configuration to configurations of imported targets.
set( CMAKE_MAP_IMPORTED_CONFIG_DEBUG Release )
set( CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release )

find_program( CCACHE_FOUND ccache )
if( CCACHE_FOUND )
    set_property( GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache )
    set_property( GLOBAL PROPERTY RULE_LAUNCH_LINK ccache )
endif(CCACHE_FOUND)

# In CI builds upload the binaries if the GITHUB_USER_PASSWORD was decrypted
# (only for branches and internal PRs).
if( "$ENV{CI}" AND NOT "$ENV{GITHUB_USER_PASSWORD}" STREQUAL "" )
    set( run_upload YES )
else()
    set( run_upload NO )
endif()

set( CMAKE_CXX_STANDARD 17 )

cable_set_build_type( DEFAULT RelWithDebInfo CONFIGURATION_TYPES Debug;Release;RelWithDebInfo )

project( skale )

file( READ VERSION PROJECT_VERSION )
string( STRIP ${PROJECT_VERSION} PROJECT_VERSION )

if( NOT EXISTS ${CMAKE_SOURCE_DIR}/evmjit/.git )
    message( FATAL_ERROR "Git submodules not initialized, execute:\n  git submodule update --init" )
endif()

cable_add_buildinfo_library( PROJECT_NAME skale )

set( CMAKE_SKIP_INSTALL_ALL_DEPENDENCY ON )

include( EthCcache )
include( EthOptions )
include( EthCompilerSettings )
include( EthExecutableHelper )
include( EthDependencies )
include( EthUtils )

set( CLANG_FORMAT_EXCLUDE_PATTERNS
    ${CMAKE_BINARY_DIR}
    ${DEPS_SOURCES_ROOT}
    "${CMAKE_CURRENT_SOURCE_DIR}/evmc/"
    "${CMAKE_CURRENT_SOURCE_DIR}/evmjit/"
    "${CMAKE_CURRENT_SOURCE_DIR}/libconsensus/"
    )
include( FindClangFormat )
#find_package( ClangFormat )

set( Boost_USE_STATIC_LIBS ON )
set( Boost_USE_MULTITHREADED ON )
set( Boost_NO_BOOST_CMAKE ON )
find_package( Boost MODULE REQUIRED program_options filesystem system thread context fiber log chrono)

option( BUILD_LEVELDB "Build leveldb from leveldb directory" OFF )

if( BUILD_LEVELDB )
    add_subdirectory( leveldb )
else()
    find_package( leveldb CONFIG REQUIRED )
endif()

find_package( Snappy CONFIG REQUIRED )
find_package( Crc32c CONFIG REQUIRED )

set( LIB_NAME_cryptopp "cryptopp" )

#set( MHD_DIR "${DEPS_INSTALL_ROOT}" )
#find_package( MHD PATHS "${DEPS_INSTALL_ROOT}/libs/cmake" )
#find_library( MHD microhttpd )

find_library( SCRYPT_LIBRARIES NAMES scrypt HINTS "${DEPS_INSTALL_ROOT}/lib")

find_package( ethash CONFIG REQUIRED )

include( ProjectSecp256k1 )

find_package( Threads )

if( MINIUPNPC )
    find_package( Miniupnpc 1.8.2013 REQUIRED )
endif()

set( UTILS_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/utils" )

configure_project()

if( TESTS )
    # Import binaryen library for testeth.
    # TODO: remove this once testeth uses binaryen as a CLI tool.
    include( ProjectBinaryen )
endif()

if (HISTORIC_STATE)
add_definitions(-DHISTORIC_STATE=1)
endif (HISTORIC_STATE)

add_subdirectory( evmc )

#Global include path for all libs:
include_directories( "${CMAKE_SOURCE_DIR}" )

set( SKUTILS_INCLUDE_DIRS
    "${CMAKE_SOURCE_DIR}/libskutils/include"
    "${CMAKE_SOURCE_DIR}/libconsensus/thirdparty"
    "${CMAKE_SOURCE_DIR}/libconsensus/cppzmq"
    "${CMAKE_SOURCE_DIR}/libconsensus/libzmq/include"
    )
include_directories( ${SKUTILS_INCLUDE_DIRS} )
add_subdirectory( libskutils )

option( SKALE_INTERPRETER_SHARED "Build skale-interpreter as a shared library" OFF )
add_subdirectory( libskale-interpreter )

add_subdirectory( libdevcore )
add_subdirectory( libdevcrypto )
add_subdirectory( libp2p )

add_subdirectory( libhistoric )
add_subdirectory( libskale )


add_subdirectory( libethcore )
add_subdirectory( libevm )
add_subdirectory( libethereum )
add_subdirectory( libethashseal )

add_subdirectory( libweb3jsonrpc )

if( EVMJIT )
    add_subdirectory( evmjit )
endif()

add_subdirectory( skaled )
add_subdirectory( skaled_ssl_test )

if( TOOLS )
    add_subdirectory( skale-key )
    add_subdirectory( skale-vm )
endif()

if( TESTS )
    enable_testing()
    add_subdirectory( test )
    add_subdirectory( storage_benchmark )
endif()

set( CPACK_GENERATOR TGZ )
set( CPACK_PACKAGE_FILE_NAME ${PROJECT_NAME} )
set( CPACK_PACKAGE_CHECKSUM SHA256 )
set( CPACK_INCLUDE_TOPLEVEL_DIRECTORY FALSE )
include( CPack )

if ( CONSENSUS )
    set( SKALED_LATEST_STANDARD ON CACHE BOOL "Use latest standards" ) # depricated option. TODO: remove from consensus
    set( BUILD_TESTS OFF CACHE BOOL "Build tests" ) # do not build libbls tests
    add_subdirectory( libconsensus EXCLUDE_FROM_ALL)
    target_compile_options( consensus PRIVATE -Wno-unused-variable ) # silence libff warnings
    target_compile_options( bls PRIVATE -Wno-unused-variable ) # silence libff warnings
else()
    add_subdirectory( libconsensus/libBLS )
    target_compile_options( bls PRIVATE -Wno-unused-variable ) # silence libff warnings
    set( BUILD_TESTS OFF CACHE BOOL "Build tests" ) # do not build libbls tests
endif()

add_subdirectory( libbatched-io )

set( SANITIZE "" CACHE STRING "contents of -fsanitize option")

if ( SANITIZE )
    SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=${SANITIZE}" )
    SET( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=${SANITIZE}" )
    SET( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${SANITIZE}" )
endif()

