#ifndef SHAREDSPACE_H
#define SHAREDSPACE_H

#include <boost/filesystem.hpp>

#include <string>

class SharedSpace {
public:
    SharedSpace( const std::string& path );
    const std::string& getPath() const;
    bool lock();  // can fail if signal delivered
    bool try_lock();
    void unlock();

    SharedSpace( const SharedSpace& rhs ) = delete;
    SharedSpace& operator=( const SharedSpace& rhs ) = delete;

private:
    const std::string path;
    int lock_fd;
};

#endif  // SHAREDSPACE_H
