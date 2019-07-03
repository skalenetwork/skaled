add_executable(skale-vm main.cpp)

target_include_directories( skale PRIVATE
	"${UTILS_INCLUDE_DIR}"
	${SKUTILS_INCLUDE_DIRS}
	)

target_link_libraries(skale-vm PRIVATE ethereum evm ethashseal devcore skutils Boost::program_options)

target_include_directories(skale-vm PRIVATE ../utils)

if( NOT SKALE_SKIP_INSTALLING_DIRECTIVES )
	install( TARGETS skale-vm EXPORT skaleTargets DESTINATION bin )
endif()
