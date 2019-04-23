
add_library( skutils STATIC
                                    ./include/skutils/async_work.h
    ./src/atomic_shared_ptr.cpp     ./include/skutils/atomic_shared_ptr.h
    ./src/command_line_parser.cpp   ./include/skutils/command_line_parser.h
    ./src/console_colors.cpp        ./include/skutils/console_colors.h
    ./src/cxx_replacements.cpp      ./include/skutils/cxx_replacements.h
    ./src/dispatch.cpp              ./include/skutils/dispatch.h
    ./src/mail.cpp                  ./include/skutils/mail.h
                                    ./include/skutils/multifunction.h
    ./src/multithreading.cpp        ./include/skutils/multithreading.h
    ./src/network.cpp               ./include/skutils/network.h
	./src/stats.cpp                 ./include/skutils/stats.h
    ./src/thread_pool.cpp           ./include/skutils/thread_pool.h
    ./src/url.cpp                   ./include/skutils/url.h
    ./src/utils.cpp                 ./include/skutils/utils.h
    ./src/ws.cpp                    ./include/skutils/ws.h
    )
if( APPLE )
	target_compile_definitions( skutils PRIVATE __BUILDING_4_MAC_OS_X__=1 )
endif()
target_include_directories( skutils PRIVATE
    ./include
	${SKUTILS_INCLUDE_DIRS}
    )
target_link_libraries( skutils
    websockets uv
    )
