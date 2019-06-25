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
add_library(skale-interpreter STATIC ${sources})
target_link_libraries(skale-interpreter PRIVATE devcore skale-buildinfo evmc::evmc evmc::instructions)

if(SKALE_INTERPRETER_SHARED)
    # Build skale-interpreter additionally as a shared library to include it in the package
    add_library(skale-interpreter-shared SHARED ${sources})
    target_link_libraries(skale-interpreter-shared PRIVATE devcore skale-buildinfo evmc::evmc evmc::instructions)
    set_target_properties(skale-interpreter-shared PROPERTIES OUTPUT_NAME skale-interpreter)
	if( NOT SKALE_SKIP_INSTALLING_DIRECTIVES )
		install( TARGETS skale-interpreter-shared EXPORT skaleTargets
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
			LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
			ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
			)
	endif()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
    target_compile_options(skale-interpreter PRIVATE -fstack-usage)
endif()
