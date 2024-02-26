#include "SchainPatch.h"

dev::eth::ChainOperationParams SchainPatch::chainParams;
time_t SchainPatch::committedBlockTimestamp;

void SchainPatch::init( const dev::eth::ChainOperationParams& _cp ) {
    chainParams = _cp;
}

void SchainPatch::useLatestBlockTimestamp( time_t _timestamp ) {
    committedBlockTimestamp = _timestamp;
}
