#include "RevertableFSPatch.h"

time_t RevertableFSPatch::revertableFSPatchTimestamp;
time_t RevertableFSPatch::lastBlockTimestamp;

bool RevertableFSPatch::isEnabled() {
    return revertableFSPatchTimestamp <= lastBlockTimestamp;
}