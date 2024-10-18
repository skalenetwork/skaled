
if( CMAKE_BUILD_TYPE MATCHES "Debug" )
    set( DEBUG_SUFFIX_D "d" )
    set( DEBUG_SUFFIX__DEBUG "_debug" )
else()
    set( DEBUG_SUFFIX_D "" )
    set( DEBUG_SUFFIX__DEBUG "" )
endif()


add_library( skutils STATIC
                                    ./include/skutils/async_work.h
    ./src/atomic_shared_ptr.cpp     ./include/skutils/atomic_shared_ptr.h
    ./src/command_line_parser.cpp   ./include/skutils/command_line_parser.h
    ./src/console_colors.cpp        ./include/skutils/console_colors.h
    ./src/dispatch.cpp              ./include/skutils/dispatch.h
    ./src/eth_utils.cpp             ./include/skutils/eth_utils.h
    ./src/http.cpp                  ./include/skutils/http.h
    ./src/http_pg.cpp               ./include/skutils/http_pg.h
    ./src/mail.cpp                  ./include/skutils/mail.h
                                    ./include/skutils/multifunction.h
    ./src/multithreading.cpp        ./include/skutils/multithreading.h
    ./src/network.cpp               ./include/skutils/network.h
    ./src/rest_call.cpp             ./include/skutils/rest_call.h
        ./src/task_performance.cpp      ./include/skutils/task_performance.h
    ./src/thread_pool.cpp           ./include/skutils/thread_pool.h
    ./src/unddos.cpp                ./include/skutils/unddos.h
    ./src/url.cpp                   ./include/skutils/url.h
    ./src/utils.cpp                 ./include/skutils/utils.h
    ./src/ws.cpp                    ./include/skutils/ws.h
    ./src/btrfs.c                   ./include/skutils/btrfs.h
    )
if( APPLE )
    target_compile_definitions( skutils PRIVATE __BUILDING_4_MAC_OS_X__=1 )
endif()
target_compile_options( skutils PRIVATE
    -Wno-error=deprecated-copy -Wno-error=unused-result -Wno-error=unused-parameter -Wno-error=unused-variable -Wno-error=maybe-uninitialized
    )
target_include_directories( skutils PRIVATE
    ./include
    ${SKUTILS_INCLUDE_DIRS}
    "${DEPS_INSTALL_ROOT}/include"
    "${DEPS_INSTALL_ROOT}/include/cryptopp"
    )
target_link_libraries( skutils
    "${DEPS_INSTALL_ROOT}/lib/libjsoncpp.a"
    "${DEPS_INSTALL_ROOT}/lib/libproxygenhttpserver.a"
    "${DEPS_INSTALL_ROOT}/lib/libproxygen.a"
    "${DEPS_INSTALL_ROOT}/lib/libwangle.a"
    "${DEPS_INSTALL_ROOT}/lib/libfizz.a"
    "${DEPS_INSTALL_ROOT}/lib/libfolly.a"
    "${DEPS_INSTALL_ROOT}/lib/libgtest${DEBUG_SUFFIX_D}.a"
    "${DEPS_INSTALL_ROOT}/lib/libgmock${DEBUG_SUFFIX_D}.a"
    "${DEPS_INSTALL_ROOT}/lib/libglog${DEBUG_SUFFIX_D}.a"
    "${DEPS_INSTALL_ROOT}/lib/libgflags${DEBUG_SUFFIX__DEBUG}.a"
    "${DEPS_INSTALL_ROOT}/lib/libboost_context.a"
    "${DEPS_INSTALL_ROOT}/lib/libevent.a"
    "${DEPS_INSTALL_ROOT}/lib/libfmt${DEBUG_SUFFIX_D}.a"
    "${DEPS_INSTALL_ROOT}/lib/libdouble-conversion.a"
    "${DEPS_INSTALL_ROOT}/lib/libzstd.a"
    "${DEPS_INSTALL_ROOT}/lib/libzmq.a"
    "${DEPS_INSTALL_ROOT}/lib/libssl.a"
    "${DEPS_INSTALL_ROOT}/lib/libcrypto.a"
    "${DEPS_INSTALL_ROOT}/lib/libsodium.a"
    "${DEPS_INSTALL_ROOT}/lib/liblzma.a"
    "${DEPS_INSTALL_ROOT}/lib/libunwind.a"
    websockets uv pthread dl z
    )

