add_executable(skale-key KeyAux.h main.cpp)
target_link_libraries(skale-key PRIVATE ethcore devcore Boost::program_options)
install(TARGETS skale-key EXPORT skaleTargets DESTINATION bin)
