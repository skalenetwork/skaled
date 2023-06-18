#include "StorageDestructionPatch.h"

time_t StorageDestructionPatch::storageDestructionPatchTimestamp;
time_t StorageDestructionPatch::lastBlockTimestamp;

bool StorageDestructionPatch::isEnabled() {
    return storageDestructionPatchTimestamp <= lastBlockTimestamp;
}