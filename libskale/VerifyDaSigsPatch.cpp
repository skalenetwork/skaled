#include "VerifyDaSigsPatch.h"

time_t VerifyDaSigsPatch::verifyDaSigsPatchTimestamp = 0;
time_t VerifyDaSigsPatch::lastBlockTimestamp = 0;

bool VerifyDaSigsPatch::isEnabled() {
    if ( verifyDaSigsPatchTimestamp == 0 ) {
        return false;
    }
    return verifyDaSigsPatchTimestamp <= lastBlockTimestamp;
}
time_t VerifyDaSigsPatch::getVerifyDaSigsPatchTimestamp() {
    return verifyDaSigsPatchTimestamp;
}
