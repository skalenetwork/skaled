set(
    sources
    main.cpp    
)

set(
    headers    
)

set(executable_name storage_benchmark)

add_executable(${executable_name} ${sources} ${headers})
target_link_libraries(
    ${executable_name}
    PRIVATE
        ethereum
        skutils
        devcore
        "${DEPS_INSTALL_ROOT}/lib/liblzma.a"
        "${DEPS_INSTALL_ROOT}/lib/libunwind.a"
    )

#target_include_directories(evm_benchmark PRIVATE ../utils)

#if(MINIUPNPC)
#    target_compile_definitions(evm_benchmark PRIVATE ETH_MINIUPNPC)
#endif()

#if( NOT SKALE_SKIP_INSTALLING_DIRECTIVES )
#	install( TARGETS evm_benchmark DESTINATION bin )
#endif()
