#pragma once

#include <atomic>

namespace dev {
namespace eth {

template < typename T >
class Counter {
public:
    Counter() { ++count; }
    Counter( const Counter& ) { ++count; }
    ~Counter() { --count; }

    Counter& operator=( const Counter& other ) = default;

    static uint64_t howMany() { return count; }

private:
    static std::atomic_uint64_t count;
};

template < typename T >
std::atomic_uint64_t Counter< T >::count = 0;

}  // namespace eth
}  // namespace dev
