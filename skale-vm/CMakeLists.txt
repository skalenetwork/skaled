


add_executable(skale-vm main.cpp)

target_compile_options( skale-vm PRIVATE
    -Wno-error=deprecated-copy -Wno-error=unused-result -Wno-error=unused-parameter -Wno-error=unused-variable -Wno-error=maybe-uninitialized
    )

target_include_directories( skale PRIVATE
	"${UTILS_INCLUDE_DIR}"
	${SKUTILS_INCLUDE_DIRS}
	)

target_link_libraries(
    skale-vm
    PRIVATE
    ethereum
    historic
    evm
    ethashseal
    devcore
    skutils
    Boost::program_options
    "${DEPS_INSTALL_ROOT}/lib/liblzma.a"
    "${DEPS_INSTALL_ROOT}/lib/libunwind.a"
    pthread
    )

target_include_directories(skale-vm PRIVATE ../utils)

if( NOT SKALE_SKIP_INSTALLING_DIRECTIVES )
	install( TARGETS skale-vm EXPORT skaleTargets DESTINATION bin )
endif()
