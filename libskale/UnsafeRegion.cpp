#include "UnsafeRegion.h"

#include <fstream>

int UnsafeRegion::counter = 0;
boost::filesystem::path UnsafeRegion::path;
std::mutex UnsafeRegion::local_mutex;
std::chrono::high_resolution_clock::time_point UnsafeRegion::last_start_time;
std::chrono::high_resolution_clock::duration UnsafeRegion::total_time;

void UnsafeRegion::sync_with_file() {
    assert( is_initialized() );
    // if ( counter == 1 ) {
    //     std::ofstream out( path.string() );
    //     assert( out.is_open() );
    // } else if ( counter == 0 ) {
    //     boost::system::error_code ec;
    //     boost::filesystem::remove( path, ec );
    // }
}

void UnsafeRegion::start() {
    std::lock_guard< std::mutex > lock( local_mutex );
    assert( is_initialized() );
    ++counter;
    last_start_time = std::chrono::high_resolution_clock::now();
    sync_with_file();
}
void UnsafeRegion::end() {
    std::lock_guard< std::mutex > lock( local_mutex );
    assert( is_initialized() );
    --counter;
    total_time += std::chrono::high_resolution_clock::now() - last_start_time;
    sync_with_file();
}
