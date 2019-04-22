#include <skutils/cxx_replacements.h>

#include <sys/types.h>
#include <time.h>
#include <chrono>
#include <cmath>
#include <thread>

#if ( defined _WIN32 )
#include <windows.h>
#else
#include <unistd.h>
#endif  // (defined _WIN32)

namespace skutils {

void sleep_for_microseconds( uint64_t n ) {
#if ( defined __BUILDING_4_MAC_OS_X__ )
    ::usleep( ( useconds_t ) n );
#else
    std::this_thread::sleep_for( std::chrono::microseconds( n ) );
#endif
}
void sleep_for_milliseconds( uint64_t n ) {
#if ( defined __BUILDING_4_MAC_OS_X__ )
    sleep_for_microseconds( n / 1000 + ( ( ( n % 1000 ) != 0 ) ? 1 : 0 ) );
#else
    std::this_thread::sleep_for( std::chrono::milliseconds( n ) );
#endif
}
void sleep_for_seconds( uint64_t n ) {
#if ( defined __BUILDING_4_MAC_OS_X__ )
    sleep_for_milliseconds( n / 1000 + ( ( ( n % 1000 ) != 0 ) ? 1 : 0 ) );
#else
    std::this_thread::sleep_for( std::chrono::seconds( n ) );
#endif
}

bool is_nan( float f ) {
    return std::isnan( f );
}
bool is_nan( double d ) {
    return std::isnan( d );
}

};  // namespace skutils
