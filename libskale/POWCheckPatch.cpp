#include "POWCheckPatch.h"

time_t POWCheckPatch::powCheckPatchTimestamp;
time_t POWCheckPatch::lastBlockTimestamp;

bool POWCheckPatch::isEnabled() {
    if ( powCheckPatchTimestamp == 0 ) {
        return false;
    }
    return powCheckPatchTimestamp <= lastBlockTimestamp;
}
