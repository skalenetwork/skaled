
file( GLOB sources "*.cpp" "*.h" )
add_library( ethereum ${sources} InstanceMonitor.cpp InstanceMonitor.h
    SchainPatchEnum.h)

target_compile_options( ethereum PRIVATE
    -Wno-error=deprecated-copy -Wno-error=unused-result -Wno-error=unused-parameter -Wno-error=unused-variable -Wno-error=maybe-uninitialized
    -Wno-nonnull -Wno-stringop-overflow -Wno-array-bounds -Wno-restrict
    )

target_include_directories( ethereum PRIVATE "${UTILS_INCLUDE_DIR}" ${SKUTILS_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/libconsensus/jsoncpp/include
        ${CMAKE_SOURCE_DIR}/libconsensus/spdlog/include
        ${CMAKE_SOURCE_DIR}/libconsensus/libjson/include)
target_link_libraries( ethereum PUBLIC evm ethcore p2p devcrypto devcore skale PRIVATE historic skutils snappy
        jsoncpp # ${CMAKE_SOURCE_DIR}/libconsensus/jsoncpp/build/src/lib_json/libjsoncpp.a
        Boost::fiber Boost::context Boost::chrono
        batched-io
        )

#target_compile_options( ethereum PRIVATE -Weffc++ )
#target_compile_options( ethereum PRIVATE -Wno-error=effc++ )
