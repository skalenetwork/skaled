set( CMAKE_CXX_STANDARD 17 )

file( GLOB sources "*.cpp" "*.h" )

set(EXECUTABLE_NAME skaled)

add_executable(${EXECUTABLE_NAME} ${sources})

target_compile_options( ${EXECUTABLE_NAME} PRIVATE
    -Wno-error=deprecated-copy -Wno-error=unused-result -Wno-error=unused-parameter -Wno-error=unused-variable -Wno-error=maybe-uninitialized
    -Wno-nonnull
    )

if( APPLE )
	target_compile_definitions( ${EXECUTABLE_NAME} PRIVATE __BUILDING_4_MAC_OS_X__=1 )
endif()

target_link_libraries(
    ${EXECUTABLE_NAME}
    PRIVATE
        ethereum
        historic
        ethashseal
        evm
        web3jsonrpc
        devcore
        Boost::program_options
        skutils
        "${DEPS_INSTALL_ROOT}/lib/libunwind.a"
        "${DEPS_INSTALL_ROOT}/lib/liblzma.a"
        pthread
        idn2
        batched-io
    )
if (CONSENSUS)
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE consensus)
endif()

target_include_directories(${EXECUTABLE_NAME} PRIVATE ../utils ${SKUTILS_INCLUDE_DIRS})

if(MINIUPNPC)
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ETH_MINIUPNPC)
endif()

if( NOT SKALE_SKIP_INSTALLING_DIRECTIVES )
	install( TARGETS ${EXECUTABLE_NAME} DESTINATION bin )
endif()
