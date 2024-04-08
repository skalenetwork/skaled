set(
    sources
    main.cpp    
)

set(
    headers    
)

set(executable_name storage_benchmark)

add_executable(${executable_name} ${sources} ${headers})
target_compile_options( ${executable_name} PRIVATE
    -Wno-error=deprecated-copy -Wno-error=unused-result -Wno-error=unused-parameter -Wno-error=unused-variable -Wno-error=maybe-uninitialized
    )
target_link_libraries(
    ${executable_name}
    PRIVATE
        ethereum
        skale
        historic
        skutils
        devcore
        skale
        "${DEPS_INSTALL_ROOT}/lib/libunwind.a"
        "${DEPS_INSTALL_ROOT}/lib/liblzma.a"
    )

target_include_directories(${executable_name} PRIVATE ../utils ${SKUTILS_INCLUDE_DIRS})

#if(MINIUPNPC)
#    target_compile_definitions(evm_benchmark PRIVATE ETH_MINIUPNPC)
#endif()

#if( NOT SKALE_SKIP_INSTALLING_DIRECTIVES )
#	install( TARGETS evm_benchmark DESTINATION bin )
#endif()
