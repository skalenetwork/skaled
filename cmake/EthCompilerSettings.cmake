#------------------------------------------------------------------------------
# EthCompilerSettings.cmake
#
# CMake file for skaled project which specifies our compiler settings
# for each supported platform and build configuration.
#
# The documentation for skaled is hosted at https://developers.skalelabs.com/
#
# Copyright (c) 2014-2016 cpp-ethereum contributors.
# Modifications Copyright (C) 2018-2019 SKALE Labs 
#------------------------------------------------------------------------------

# Clang seeks to be command-line compatible with GCC as much as possible, so
# most of our compiler settings are common between GCC and Clang.
#
# These settings then end up spanning all POSIX platforms (Linux, OS X, BSD, etc)

include(EthCheckCXXCompilerFlag)

eth_add_cxx_compiler_flag_if_supported(-fstack-protector-strong have_stack_protector_strong_support)
if(NOT have_stack_protector_strong_support)
    eth_add_cxx_compiler_flag_if_supported(-fstack-protector)
endif()

eth_add_cxx_compiler_flag_if_supported(-Wimplicit-fallthrough)

option( SKALED_HATE_WARNINGS "Treat most of warings as errors" ON )
# Ensures that CMAKE_BUILD_TYPE has a default value
if( NOT DEFINED CMAKE_BUILD_TYPE )
    set( CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel Coverage." )
endif()
if( NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Release|RelWithDebInfo|MinSizeRel)$" )
    message( FATAL_ERROR "Invalid value for CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}" )
else()
    message( STATUS "Discovered CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}" )
endif()
if( CMAKE_BUILD_TYPE MATCHES "Debug" )
    remove_definitions( -DNDEBUG )
    add_definitions(    -D_DEBUG )
else()
    remove_definitions( -D_DEBUG )
    add_definitions(    -DNDEBUG )
endif()
if( SKALED_HATE_WARNINGS )
    add_compile_options( -Wall )
    add_compile_options( -Wextra )
    add_compile_options( -Werror )
    #add_compile_options( -Wno-error=sign-compare )
    #add_compile_options( -Wno-error=reorder )
    #add_compile_options( -Wno-error=deprecated )
    #add_compile_options( -Wno-error=unused-variable )
    #add_compile_options( -Wno-error=unused-parameter )
    #add_compile_options( -Wno-error=unused-but-set-variable )
    add_compile_options(-Wno-error=int-in-bool-context)
endif()

if (("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU") OR ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang"))

    # Disable warnings about unknown pragmas (which is enabled by -Wall).
    add_compile_options(-Wno-unknown-pragmas)

    # Configuration-specific compiler settings.
    set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g -DETH_DEBUG")
    set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")

    option(USE_LD_GOLD "Use GNU gold linker" ON)
    if (USE_LD_GOLD)
        execute_process(COMMAND ${CMAKE_C_COMPILER} -fuse-ld=gold -Wl,--version ERROR_QUIET OUTPUT_VARIABLE LD_VERSION)
        if ("${LD_VERSION}" MATCHES "GNU gold")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold")
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=gold")
        endif ()
    endif ()

    # Hide all symbols by default.
    add_compile_options(-fvisibility=hidden)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Do not export symbols from dependencies.
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--exclude-libs,ALL")
    endif()

    # Check GCC compiler version.
    if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
        if("${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS 5)
            message(FATAL_ERROR "This compiler ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} is not supported. GCC 5 or newer is required.")
        endif()

    # Stop if buggy clang compiler detected.
    elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES AppleClang)
        if("${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS 8.4)
            message(FATAL_ERROR "This compiler ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} is not able to compile required libff. Install clang 4+ from Homebrew or XCode 9.")
        endif()
    endif()
# If you don't have GCC or Clang then you are on your own.  Good luck!
else ()
    message(WARNING "Your compiler is not tested, if you run into any issues, we'd welcome any patches.")
endif ()

if (SANITIZE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=${SANITIZE}")
    if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
        set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fsanitize-blacklist=${CMAKE_SOURCE_DIR}/sanitizer-blacklist.txt")
    endif()
endif()

option(COVERAGE "Build with code coverage support" OFF)
if(COVERAGE)
    message( STATUS "**********************" )
    message( STATUS "*** COVERAGE is ON ***" )
    message( STATUS "**********************" )
    add_compile_options(-g --coverage)
    #
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    #
    set(CMAKE_SHARED_LINKER_FLAGS "--coverage ${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "--coverage ${CMAKE_EXE_LINKER_FLAGS}")
endif()

if(UNIX AND NOT APPLE)
    option(STATIC_LIBSTDCPP "Link libstdc++ staticly")
    if(STATIC_LIBSTDCPP)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++")
    endif()
endif()

