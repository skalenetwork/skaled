add_executable(skale-vm main.cpp)

target_compile_options( skale-vm PRIVATE
    -Wno-error=deprecated-copy -Wno-error=unused-result -Wno-error=unused-parameter -Wno-error=unused-variable -Wno-error=maybe-uninitialized
    )

target_include_directories( skale PRIVATE
	"${UTILS_INCLUDE_DIR}"
	${SKUTILS_INCLUDE_DIRS}
	)

# skale-vm linking (force-in codecs + handle cyclic static deps)
target_link_libraries(skale-vm PRIVATE
    -Wl,--no-as-needed
    -Wl,--start-group

    ethereum
    evm
    ethashseal
    devcore
    skutils
    Boost::program_options
    $<$<BOOL:${HISTORIC_STATE}>:historic>
    "${DEPS_INSTALL_ROOT}/lib/libunwind.a"

    # Force archive extraction for codec providers discovered via static init:
    -Wl,--whole-archive
    "${DEPS_INSTALL_ROOT}/lib/libzstd.a"
    "${DEPS_INSTALL_ROOT}/lib/liblzma.a"
    -Wl,--no-whole-archive

    pthread
    -Wl,--end-group
)

target_include_directories(skale-vm PRIVATE ../utils)

if( NOT SKALE_SKIP_INSTALLING_DIRECTIVES )
	install( TARGETS skale-vm EXPORT skaleTargets DESTINATION bin )
endif()
