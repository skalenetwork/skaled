add_executable(skale-vm main.cpp)

target_link_libraries(skale-vm PRIVATE ethereum evm ethashseal devcore Boost::program_options)

target_include_directories(skale-vm PRIVATE ../utils)

install(TARGETS skale-vm EXPORT skaleTargets DESTINATION bin)
