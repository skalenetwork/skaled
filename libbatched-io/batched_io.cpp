#include "batched_io.h"

#include <assert.h>
#include <unistd.h>
#include <iostream>

#include <libdevcore/Log.h>

namespace {
std::string test_crash_at;
}

namespace batched_io {
void test_crash_before_commit( const std::string& id ) {
    if ( !test_crash_at.empty() ) {
        cnote << "test_crash_before_commit: " << id << std::endl;
        if ( id == test_crash_at ) {
            cerror << "test_crash_before_commit: crashing at " << test_crash_at << std::endl;
            _exit( 33 );
        }
    }  // if 1
}
void test_enable_crash_at( const std::string& id ) {
    assert( test_crash_at.empty() );
    test_crash_at = id;
}

}  // namespace batched_io
