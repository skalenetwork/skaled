#include "UnsafeRegion.h"

#include <fstream>

int UnsafeRegion::counter = 0;
boost::filesystem::path UnsafeRegion::path;
std::mutex UnsafeRegion::local_mutex;
std::chrono::system_clock::time_point UnsafeRegion::last_start_time;
std::chrono::system_clock::duration UnsafeRegion::total_time;

void UnsafeRegion::sync_with_file() {
    assert( is_initialized() );
    if ( counter == 1 ) {
        std::ofstream out( path.string() );
        assert( out.is_open() );
        last_start_time = std::chrono::system_clock::now();
    } else if ( counter == 0 ) {
        boost::system::error_code ec;
        boost::filesystem::remove( path, ec );
        total_time += std::chrono::system_clock::now() - last_start_time;
    }
}

void UnsafeRegion::start() {
    std::lock_guard< std::mutex > lock( local_mutex );
    assert( is_initialized() );
    ++counter;
    sync_with_file();
}
void UnsafeRegion::end() {
    std::lock_guard< std::mutex > lock( local_mutex );
    assert( is_initialized() );
    --counter;
    sync_with_file();
}
