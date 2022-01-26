#ifndef UNSAFEREGION_H
#define UNSAFEREGION_H

#include <assert.h>
#include <boost/filesystem.hpp>
#include <chrono>
#include <mutex>

class UnsafeRegion {
private:
    static int counter;
    static boost::filesystem::path path;
    static std::mutex local_mutex;
    static std::chrono::system_clock::time_point last_start_time;
    static std::chrono::system_clock::duration total_time;

    static bool is_initialized() { return !path.empty(); }
    static void sync_with_file();

public:
    static void init( const boost::filesystem::path& _dirPath ) {
        assert( path.empty() );
        path = _dirPath / "skaled.lock";
        counter = boost::filesystem::exists( path ) ? 1 : 0;
    }
    static void start();
    static void end();
    static bool isActive() {
        assert( is_initialized() );
        return counter != 0;
    }

    static std::chrono::high_resolution_clock::duration getTotalTime() {
        assert( !isActive() );
        return total_time;
    }

    class lock {
    public:
        lock() { UnsafeRegion::start(); }
        ~lock() { UnsafeRegion::end(); }
    };
};

#endif  // UNSAFEREGION_H
