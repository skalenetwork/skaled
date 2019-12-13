file(GLOB sources "*.cpp" "*.c")
file(GLOB headers "*.h")

if( NOT MICROPROFILE )
    file(GLOB_RECURSE microprofile_files "*microprofile*" "miniz.*")
    foreach(file ${microprofile_files})
        list(REMOVE_ITEM sources ${file})
    endforeach()
endif()

add_library(devcore ${sources} ${headers})

# Needed to prevent including system-level boost headers:
target_include_directories(devcore SYSTEM PUBLIC ${Boost_INCLUDE_DIR} PRIVATE ../utils)
target_include_directories( devcore PRIVATE
	${SKUTILS_INCLUDE_DIRS}
	)

target_link_libraries(devcore PUBLIC skale-buildinfo Boost::filesystem Boost::system Boost::log Boost::thread Threads::Threads PRIVATE ethash::ethash)

#find_package(LevelDB)
#target_include_directories(devcore SYSTEM PUBLIC ${LEVELDB_INCLUDE_DIRS})
#target_link_libraries(devcore PRIVATE ${LEVELDB_LIBRARIES})

target_link_libraries(devcore PRIVATE leveldb skutils)
