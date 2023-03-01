#include "ContractStorageLimitPatch.h"

time_t ContractStorageLimitPatch::contractStoragePatchTimestamp;
time_t ContractStorageLimitPatch::lastBlockTimestamp;

bool ContractStorageLimitPatch::isEnabled() {
    if ( contractStoragePatchTimestamp == 0 ) {
        return false;
    }
    return contractStoragePatchTimestamp <= lastBlockTimestamp;
}
