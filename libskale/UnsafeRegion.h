#ifndef UNSAFEREGION_H
#define UNSAFEREGION_H

#include <boost/filesystem.hpp>
#include <assert.h>

class UnsafeRegion
{
private:
    static int counter;
    static boost::filesystem::path path;
    static bool is_initialized(){
        return !path.empty();
    }
    static void sync_with_file();
public:
    static void init(const boost::filesystem::path& _dirPath){
        assert(path.empty());
        path = _dirPath / "skaled.lock";
        counter = boost::filesystem::exists(path) ? 1 : 0;
    }
    static void start();
    static void end();
    static bool isActive(){
        assert(is_initialized());
        return counter != 0;
    }
};

#endif // UNSAFEREGION_H
