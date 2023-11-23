#include "PrecompiledConfigPatch.h"

time_t PrecompiledConfigPatch::precompiledConfigPatchTimestamp;
time_t PrecompiledConfigPatch::lastBlockTimestamp;

bool PrecompiledConfigPatch::isEnabled() {
    if ( precompiledConfigPatchTimestamp == 0 ) {
        return false;
    }
    return precompiledConfigPatchTimestamp <= lastBlockTimestamp;
}
