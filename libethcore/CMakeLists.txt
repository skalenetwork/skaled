file( GLOB sources "*.cpp" "*.h" )

set( CMAKE_CXX_STANDARD 17 )

add_library( ethcore ${sources} )

target_include_directories( ethcore PRIVATE
	"${UTILS_INCLUDE_DIR}"
	${SKUTILS_INCLUDE_DIRS}
	)

target_link_libraries( ethcore PUBLIC
	devcrypto
	devcore
	)
