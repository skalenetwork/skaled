set( CMAKE_CXX_STANDARD 17 )
set( sources
    main.cpp
    )
add_executable( skaled_ssl_test ${sources} )
target_compile_options( skaled_ssl_test PRIVATE -Wno-error=unused-variable )
if( APPLE )
    target_compile_definitions( skaled_ssl_test
        PRIVATE __BUILDING_4_MAC_OS_X__=1
        )
endif()
target_link_libraries(
    skaled_ssl_test
    PRIVATE
    skutils
    ${DEPS_INSTALL_ROOT}/lib/libcurl.a
    ${DEPS_INSTALL_ROOT}/lib/libssl.a
    ${DEPS_INSTALL_ROOT}/lib/libcrypto.a
    ${DEPS_INSTALL_ROOT}/lib/libz.a
    idn2
    dl
    )
target_include_directories( skaled_ssl_test
    PRIVATE ${SKUTILS_INCLUDE_DIRS}
    )
if(MINIUPNPC)
    target_compile_definitions( skaled_ssl_test
        PRIVATE ETH_MINIUPNPC
        )
endif()
if( NOT SKALE_SKIP_INSTALLING_DIRECTIVES )
    install( TARGETS skaled_ssl_test DESTINATION bin )
endif()
