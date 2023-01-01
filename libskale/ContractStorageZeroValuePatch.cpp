#include "ContractStorageZeroValuePatch.h"

time_t ContractStorageZeroValuePatch::contractStorageZeroValuePatchTimestamp = 0;
time_t ContractStorageZeroValuePatch::lastBlockTimestamp = 0;

bool ContractStorageZeroValuePatch::isEnabled() {
    return contractStorageZeroValuePatchTimestamp <= lastBlockTimestamp;
}
