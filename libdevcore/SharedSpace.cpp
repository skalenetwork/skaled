#include "SharedSpace.h"

#include <boost/filesystem.hpp>

#include <sys/file.h>

static const std::string LOCK_FILE = ".lock";

SharedSpace::SharedSpace( const std::string& _path ) : path( _path ) {}
const std::string& SharedSpace::getPath() const {
    return path;
}
bool SharedSpace::lock() {
    do {
        lock_fd = open( ( path + "/" + LOCK_FILE ).c_str(), O_WRONLY | O_TRUNC | O_CREAT );
        if ( lock_fd < 0 )
            sleep( 1 );
    } while ( lock_fd < 0 );

    int res = flock( lock_fd, LOCK_EX );
    if ( res == 0 ) {
        boost::filesystem::directory_iterator it( boost::filesystem::path( path.c_str() ) ), end;
        boost::filesystem::path exclude( ( path + "/" + LOCK_FILE ) );

        while ( it != end ) {
            if ( it->path() != exclude )
                boost::filesystem::remove_all( it->path() );
            ++it;
        }  // while
    }      // res == 0
    return res == 0;
}
bool SharedSpace::try_lock() {
    lock_fd = open( ( path + "/" + LOCK_FILE ).c_str(), O_WRONLY | O_TRUNC | O_CREAT );
    if ( lock_fd < 0 )
        return false;
    int res = flock( lock_fd, LOCK_EX | LOCK_NB );
    if ( res == 0 ) {
        boost::filesystem::directory_iterator it( boost::filesystem::path( path.c_str() ) ), end;
        boost::filesystem::path exclude( ( path + "/" + LOCK_FILE ) );

        while ( it != end ) {
            if ( it->path() != exclude )
                boost::filesystem::remove_all( it->path() );
            ++it;
        }  // while
    }      // res == 0
    return res == 0;
}
void SharedSpace::unlock() {
    flock( lock_fd, LOCK_UN );
    close( lock_fd );
}
