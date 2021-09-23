#include "UnsafeRegion.h"

#include <fstream>

int UnsafeRegion::counter = 0;
boost::filesystem::path UnsafeRegion::path;

void UnsafeRegion::sync_with_file(){
    assert(is_initialized());
    if(counter == 1){
        std::ofstream out(path.string());
        assert(out.is_open());
    }
    else if(counter == 0){
        boost::system::error_code ec;
        boost::filesystem::remove(path, ec);
    }
}

void UnsafeRegion::start(){
    assert(is_initialized());
    ++counter;
    sync_with_file();
}
void UnsafeRegion::end(){
    assert(is_initialized());
    --counter;
    sync_with_file();
}

