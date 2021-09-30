#ifndef BATCHED_IO_H
#define BATCHED_IO_H

#include <string>

namespace batched_io {

// 1 Derive from this class
// 2 Implement in your class your specific I/O operations (DB write, file write etc)
// 3 Implement commit() and proper recover() at opening
class batched_face {
public:
    virtual void commit( const std::string& test_crash_string = std::string() ) = 0;
    virtual void revert() = 0;
    virtual ~batched_face() {}

protected:
    // sometimes it's not needed
    // but it should never be forgotten!
    virtual void recover() = 0;
};

extern void test_crash_before_commit( const std::string& );
extern void test_enable_crash_at( const std::string& );

}  // namespace batched_io

#endif  // BATCHED_IO_H
