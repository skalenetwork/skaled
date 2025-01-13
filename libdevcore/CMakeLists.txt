file(GLOB sources "*.cpp" "*.c")
file(GLOB headers "*.h")

set( CMAKE_CXX_STANDARD 17 )


if( NOT MICROPROFILE )
    file(GLOB_RECURSE microprofile_files "*microprofile*" "miniz.*")
    foreach(file ${microprofile_files})
        list(REMOVE_ITEM sources ${file})
    endforeach()
endif()

add_library(devcore ${sources} ${headers})
add_dependencies(devcore secp256k1)

target_compile_options( devcore PRIVATE
    -Wno-deprecated-copy -Wno-error=unused-result -Wno-error=unused-parameter
    -Wno-error=unused-variable -Wno-error=maybe-uninitialized -Wno-error=class-memaccess
    -Wno-nonnull -Wno-sign-compare -Wno-maybe-uninitialized
)

# Needed to prevent including system-level boost headers:
target_include_directories(devcore SYSTEM PUBLIC ${Boost_INCLUDE_DIR} PRIVATE ../utils)
target_include_directories( devcore PRIVATE
	${SKUTILS_INCLUDE_DIRS}
	)

target_link_libraries(devcore PUBLIC skale-buildinfo Boost::filesystem Boost::system Boost::log Boost::thread Threads::Threads PRIVATE ethash::ethash)

#find_package(LevelDB)
#target_include_directories(devcore SYSTEM PUBLIC ${LEVELDB_INCLUDE_DIRS})
#target_link_libraries(devcore PRIVATE ${LEVELDB_LIBRARIES})

if( BUILD_LEVELDB )
    target_link_libraries(devcore PRIVATE leveldb skutils)
else()
    target_link_libraries(devcore PRIVATE leveldb::leveldb skutils)
endif()
