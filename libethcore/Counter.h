#pragma once

#include <atomic>

namespace dev {
namespace eth {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"

template < typename T >
class Counter {
public:
    Counter() { ++count; }

    Counter( const Counter& ) { ++count; }

    ~Counter() { --count; }

    static uint64_t howMany() { return count; }

private:
    static std::atomic_uint64_t count;
};

template < typename T >
std::atomic_uint64_t Counter< T >::count = 0;

#pragma GCC diagnostic pop

}  // namespace eth
}  // namespace dev
