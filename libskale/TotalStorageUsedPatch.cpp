#include "TotalStorageUsedPatch.h"

TotalStorageUsedPatch::TotalStorageUsedPatch(batched_io::db_operations_face* _db)
    :db(_db)
{

}

bool activationNeeded();
bool isActive() const;
void activate();
void onProgress();
void deactivateAfterAllNodesUpdated();
