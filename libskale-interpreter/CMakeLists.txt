


# SKALED: SKALE C++ client, tools and libraries.
# Licensed under the GNU General Public License, Version 3. See the LICENSE file.

set(
    sources
    interpreter.h
    VM.cpp
    VM.h
    VMCalls.cpp
    VMConfig.h
    VMOpt.cpp
)

set(
    dependencies
    skale-buildinfo
    ethash
    evmc::evmc
    evmc::instructions
    Boost::boost
)

add_library(skale-interpreter STATIC ${sources})
target_compile_options( skale-interpreter PRIVATE
    -Wno-error=deprecated-copy -Wno-error=unused-result -Wno-error=unused-parameter -Wno-error=unused-variable -Wno-error=maybe-uninitialized
    )
target_link_libraries(skale-interpreter PRIVATE ${dependencies})

if(EVM_OPTIMIZE)
    target_compile_definitions(skale-interpreter PRIVATE EVM_OPTIMIZE)
endif()

if(SKALE_INTERPRETER_SHARED)
    # Build skale-interpreter additionally as a shared library to include it in the package
    add_library(skale-interpreter-shared SHARED ${sources})
    target_link_libraries(skale-interpreter-shared PRIVATE ${dependencies})
    set_target_properties(skale-interpreter-shared PROPERTIES OUTPUT_NAME skale-interpreter)
    if( NOT SKALE_SKIP_INSTALLING_DIRECTIVES )
        install( TARGETS skale-interpreter-shared EXPORT skaleTargets
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
    endif()
    if(EVM_OPTIMIZE)
        target_compile_definitions(aleth-interpreter-shared PRIVATE EVM_OPTIMIZE)
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
    target_compile_options(skale-interpreter PRIVATE -fstack-usage)
endif()
