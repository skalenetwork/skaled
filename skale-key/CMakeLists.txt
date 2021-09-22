



add_executable(skale-key KeyAux.h main.cpp)

target_link_libraries(
    skale-key
    ethcore
    devcore
    skutils
    Boost::program_options
    "${DEPS_INSTALL_ROOT}/lib/liblzma.a"
    "${DEPS_INSTALL_ROOT}/lib/libunwind.a"
    )

if( NOT SKALE_SKIP_INSTALLING_DIRECTIVES )
	install( TARGETS skale-key EXPORT skaleTargets DESTINATION bin )
endif()
