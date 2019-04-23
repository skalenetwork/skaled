file( GLOB sources "*.cpp" "*.h" )
add_library( ethereum ${sources} )
target_include_directories( ethereum PRIVATE "${UTILS_INCLUDE_DIR}" ${SKUTILS_INCLUDE_DIRS} )
target_link_libraries( ethereum PUBLIC evm ethcore p2p devcrypto devcore skale PRIVATE skutils jsoncpp Snappy::snappy Boost::fiber Boost::context Boost::chrono )
