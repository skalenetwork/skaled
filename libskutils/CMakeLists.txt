

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
    ./src/stats.cpp                 ./include/skutils/stats.h
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
target_include_directories( skutils PRIVATE
    ./include
    ${SKUTILS_INCLUDE_DIRS}
    "${DEPS_INSTALL_ROOT}/include"
    "${DEPS_INSTALL_ROOT}/include/cryptopp"
    )
target_link_libraries( skutils
    "${DEPS_INSTALL_ROOT}/lib/libzmq.a"
    websockets uv pthread
    "${DEPS_INSTALL_ROOT}/lib/libjsoncpp.a"
    "${DEPS_INSTALL_ROOT}/lib/libssl.a"
    "${DEPS_INSTALL_ROOT}/lib/libcrypto.a"
    )

