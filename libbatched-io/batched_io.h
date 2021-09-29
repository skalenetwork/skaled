#ifndef BATCHED_IO_H
#define BATCHED_IO_H

namespace batched_io {

// 1 Derive from this class
// 2 Implement in your class your specific I/O operations (DB write, file write etc)
// 3 Implement commit() and proper recover() at opening
class batched_face {
public:
    virtual void commit() = 0;
    virtual void revert() = 0;
    virtual ~batched_face() {}

protected:
    // sometimes it's not needed
    // but it should never be forgotten!
    virtual void recover() = 0;
};

}  // namespace batched_io

#endif  // BATCHED_IO_H
