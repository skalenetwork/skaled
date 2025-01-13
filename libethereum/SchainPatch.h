#ifndef SCHAINPATCH_H
#define SCHAINPATCH_H

#include "SchainPatchEnum.h"

#include <libethcore/ChainOperationParams.h>

#include <libdevcore/Log.h>

#include <string>

namespace dev {
namespace eth {
struct EVMSchedule;
}
}  // namespace dev


class SchainPatch {
public:
    static void init( const dev::eth::ChainOperationParams& _cp );
    static void useLatestBlockTimestamp( time_t _timestamp );

    static SchainPatchEnum getEnumForPatchName( const std::string& _patchName );

protected:
    static void printInfo( const std::string& _patchName, time_t _timeStamp );
    static bool isPatchEnabledInWorkingBlock( SchainPatchEnum _patchEnum ) {
        time_t activationTimestamp = chainParams.getPatchTimestamp( _patchEnum );
        return activationTimestamp != 0 && committedBlockTimestamp >= activationTimestamp;
    }
    static bool isPatchEnabledWhen( SchainPatchEnum _patchEnum, time_t _committedBlockTimestamp );

protected:
    static dev::eth::ChainOperationParams chainParams;
    static std::atomic< time_t > committedBlockTimestamp;
};

#define DEFINE_AMNESIC_PATCH( CustomPatch )                                       \
    class CustomPatch : public SchainPatch {                                      \
    public:                                                                       \
        static SchainPatchEnum getEnum() { return SchainPatchEnum::CustomPatch; } \
        static bool isEnabledInWorkingBlock() {                                   \
            return isPatchEnabledInWorkingBlock( getEnum() );                     \
        }                                                                         \
    };

// TODO One more overload - with EnvInfo?
#define DEFINE_SIMPLE_PATCH( CustomPatch )                                        \
    class CustomPatch : public SchainPatch {                                      \
    public:                                                                       \
        static SchainPatchEnum getEnum() { return SchainPatchEnum::CustomPatch; } \
        static bool isEnabledInWorkingBlock() {                                   \
            return isPatchEnabledInWorkingBlock( getEnum() );                     \
        }                                                                         \
        static bool isEnabledWhen( time_t _committedBlockTimestamp ) {            \
            return isPatchEnabledWhen( getEnum(), _committedBlockTimestamp );     \
        }                                                                         \
    };

#define DEFINE_EVM_PATCH( CustomPatch )                                                 \
    class CustomPatch : public SchainPatch {                                            \
    public:                                                                             \
        static SchainPatchEnum getEnum() { return SchainPatchEnum::CustomPatch; }       \
        static bool isEnabledInWorkingBlock() {                                         \
            return isPatchEnabledInWorkingBlock( getEnum() );                           \
        }                                                                               \
        static bool isEnabledWhen( time_t _committedBlockTimestamp ) {                  \
            return isPatchEnabledWhen( getEnum(), _committedBlockTimestamp );           \
        }                                                                               \
        static dev::eth::EVMSchedule makeSchedule( const dev::eth::EVMSchedule& base ); \
    };

/*
 * Context: enable revertable filestorage precompileds
 */
DEFINE_SIMPLE_PATCH( RevertableFSPatch )

/*
 * Context: enable precompiled contracts to read historical config data
 */
DEFINE_AMNESIC_PATCH( PrecompiledConfigPatch )

/*
 * Context: enable fix for POW txns gas limit check
 */
DEFINE_SIMPLE_PATCH( PowCheckPatch )

/*
 * Context: use current, and not Constantinople,  fork in Transaction::checkOutExternalGas()
 */
DEFINE_SIMPLE_PATCH( CorrectForkInPowPatch )

/*
 * Context: contractStorageUsed counter didn't work well in one case
 * Solution: we fixed the bug and added new config field introudceChangesTimestamp
 * Purpose: avoid incorrect txn behaviour
 * Version introduced:
 */
DEFINE_AMNESIC_PATCH( ContractStorageZeroValuePatch )

/*
 * Context: enable effective storage destruction
 */
DEFINE_EVM_PATCH( PushZeroPatch )

/*
 * Context: contractStorageUsed counter didn't work well in one case
 * Solution: we fixed the bug and added new config field introudceChangesTimestamp
 * Purpose: avoid incorrect txn behaviour
 * Version introduced:
 */
DEFINE_SIMPLE_PATCH( VerifyDaSigsPatch )

/*
 * Context: contractStorageUsed counter didn't work well in one case
 * Solution: we fixed the bug and added new config field introudceChangesTimestamp
 * Purpose: avoid incorrect txn behaviour
 * Version introduced:
 */
DEFINE_AMNESIC_PATCH( ContractStoragePatch )

/*
 * Context: enable effective storage destruction
 */
DEFINE_AMNESIC_PATCH( StorageDestructionPatch );

/*
 * Enable restriction on contract storage size, when it's doing selfdestruct
 */
DEFINE_SIMPLE_PATCH( SelfdestructStorageLimitPatch );

/*
 * Enable restriction on contract storage size, when it's doing selfdestruct
 */
DEFINE_SIMPLE_PATCH( EIP1559TransactionsPatch );

/*
 * Enable bls signatures verification for sync node
 */
DEFINE_AMNESIC_PATCH( VerifyBlsSyncPatch );

/*
 * Purpose: passing both transaction origin and sender to the ConfigController contract
 * Version introduced: 3.19.0
 */
DEFINE_SIMPLE_PATCH( FlexibleDeploymentPatch );

/*
 * Context: fix externalGas calculation
 */
DEFINE_SIMPLE_PATCH( ExternalGasPatch );

#endif  // SCHAINPATCH_H
