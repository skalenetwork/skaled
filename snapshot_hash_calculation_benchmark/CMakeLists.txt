set(
    sources
    main.cpp    
)

set(
    headers    
)

set(executable_name snapshot_benchmark)

add_executable(${executable_name} ${sources} ${headers})
target_compile_options( ${executable_name} PRIVATE
    -Wno-error=deprecated-copy -Wno-error=unused-result -Wno-error=unused-parameter -Wno-error=unused-variable -Wno-error=maybe-uninitialized
    )
target_link_libraries(
    ${executable_name}
    PRIVATE
        skutils
        skale
        devcore
        "${DEPS_INSTALL_ROOT}/lib/libunwind.a"
        "${DEPS_INSTALL_ROOT}/lib/liblzma.a"
    )