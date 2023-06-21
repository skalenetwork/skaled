#include "StorageDestructionPatch.h"

time_t StorageDestructionPatch::storageDestructionPatchTimestamp;
time_t StorageDestructionPatch::lastBlockTimestamp;

bool StorageDestructionPatch::isEnabled() {
    if ( storageDestructionPatchTimestamp == 0 ) {
        return false;
    }
    return storageDestructionPatchTimestamp <= lastBlockTimestamp;
}
