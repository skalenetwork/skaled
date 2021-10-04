#include "batched_io.h"

#include <assert.h>
#include <iostream>

namespace {
std::string test_crash_at;
}

namespace batched_io {
void test_crash_before_commit( const std::string& id ) {
    if ( !id.empty() && id == test_crash_at ) {
        std::cerr << "Deliberately crashing at " << test_crash_at << std::endl;
        exit( 33 );
    }
}
void test_enable_crash_at( const std::string& id ) {
    assert( test_crash_at.empty() );
    test_crash_at = id;
}

}  // namespace batched_io
