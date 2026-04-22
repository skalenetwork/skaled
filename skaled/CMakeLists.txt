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

target_link_libraries(${EXECUTABLE_NAME} PRIVATE
  -Wl,--no-as-needed
  -Wl,--start-group

  ethereum ethashseal evm web3jsonrpc devcore skutils batched-io Boost::program_options
  $<$<BOOL:${HISTORIC_STATE}>:historic>
  $<$<BOOL:${CONSENSUS}>:consensus>
  "${DEPS_INSTALL_ROOT}/lib/libunwind.a"

  # Force archive extraction for providers:
  -Wl,--whole-archive
  "${DEPS_INSTALL_ROOT}/lib/libzstd.a"
  "${DEPS_INSTALL_ROOT}/lib/liblzma.a"
  -Wl,--no-whole-archive

  dl pthread
  -Wl,--end-group
)



target_include_directories(${EXECUTABLE_NAME} PRIVATE ../utils ${SKUTILS_INCLUDE_DIRS})

if(MINIUPNPC)
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ETH_MINIUPNPC)
endif()

if( NOT SKALE_SKIP_INSTALLING_DIRECTIVES )
	install( TARGETS ${EXECUTABLE_NAME} DESTINATION bin )
endif()
