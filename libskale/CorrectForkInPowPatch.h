#ifndef CORRECTFORKINPOWPATCH_H
#define CORRECTFORKINPOWPATCH_H

#include <libethereum/SchainPatch.h>

/*
 * Context: use current, and not Constantinople,  fork in Transaction::checkOutExternalGas()
 */

DEFINE_SIMPLE_PATCH( CorrectForkInPowPatch )

#endif  // CORRECTFORKINPOWPATCH_H
