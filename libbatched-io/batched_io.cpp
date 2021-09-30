#include "batched_io.h"

#include <assert.h>

namespace {
std::string test_crash_at;
}

namespace batched_io {
void test_crash_before_commit( const std::string& id ) {
    if ( !id.empty() && id == test_crash_at )
        exit( 33 );
}
void test_enable_crash_at( const std::string& id ) {
    assert( test_crash_at.empty() );
    test_crash_at = id;
}

}  // namespace batched_io
