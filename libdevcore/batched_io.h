#ifndef BATCHED_IO_H
#define BATCHED_IO_H

namespace batched_io{

template<typename Key>
class batched_face{
public:

    // sometimes it's not needed in case if backing storage is opened in ctor
    // but better be awre of this function
    virtual bool is_open() const = 0;

    virtual Key latest() const = 0;

    // sometimes cost Key& argument is not needed
    // but better keep it in mind
    virtual void commit(const Key&) = 0;

protected:
    // sometimes it's not needed
    // but...you got it
    virtual void recover() = 0;
};

}// namespace

#endif // BATCHED_IO_H
