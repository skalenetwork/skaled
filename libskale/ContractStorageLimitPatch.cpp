#include "ContractStorageLimitPatch.h"

time_t ContractStorageLimitPatch::contractStoragePatchTimestamp;
time_t ContractStorageLimitPatch::lastBlockTimestamp;

bool ContractStorageLimitPatch::isEnabled() {
    return contractStoragePatchTimestamp <= lastBlockTimestamp;
}
