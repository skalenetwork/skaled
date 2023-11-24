#include "ChainIdPatch.h"

time_t ChainIdPatch::contractStoragePatchTimestamp;
time_t ChainIdPatch::lastBlockTimestamp;

bool ChainIdPatch::isEnabled() {
    if ( contractStoragePatchTimestamp == 0 ) {
        return false;
    }
    return contractStoragePatchTimestamp <= lastBlockTimestamp;
}
