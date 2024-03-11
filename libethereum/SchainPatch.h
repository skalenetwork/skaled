#ifndef SCHAINPATCH_H
#define SCHAINPATCH_H

#include "SchainPatchEnum.h"

#include <libethereum/BlockChain.h>

#include <libdevcore/Log.h>

#include <string>

namespace dev {
namespace eth {
class EVMSchedule;
}
}  // namespace dev


class SchainPatch {
public:
    static void init( const dev::eth::ChainOperationParams& _cp );
    static void useLatestBlockTimestamp( time_t _timestamp );

    static SchainPatchEnum getEnumForPatchName( const std::string& _patchName );

protected:
    static void printInfo( const std::string& _patchName, time_t _timeStamp );
    static bool isPatchEnabled( SchainPatchEnum _patchEnum, const dev::eth::BlockChain& _bc,
        dev::eth::BlockNumber _bn = dev::eth::LatestBlock );
    static bool isPatchEnabledWhen( SchainPatchEnum _patchEnum, time_t _committedBlockTimestamp );

protected:
    static dev::eth::ChainOperationParams chainParams;
    static std::atomic< time_t > committedBlockTimestamp;
};

#define DEFINE_AMNESIC_PATCH( BlaBlaPatch )                                                    \
    class BlaBlaPatch : public SchainPatch {                                                   \
    public:                                                                                    \
        static SchainPatchEnum getEnum() { return SchainPatchEnum::BlaBlaPatch; }              \
        static bool isEnabledInPendingBlock() {                                                \
            time_t activationTimestamp = chainParams.getPatchTimestamp( getEnum() );           \
            return activationTimestamp != 0 && committedBlockTimestamp >= activationTimestamp; \
        }                                                                                      \
    };

// TODO One more overload - with EnvInfo?
#define DEFINE_SIMPLE_PATCH( BlaBlaPatch )                                                         \
    class BlaBlaPatch : public SchainPatch {                                                       \
    public:                                                                                        \
        static SchainPatchEnum getEnum() { return SchainPatchEnum::BlaBlaPatch; }                  \
        static bool isEnabled(                                                                     \
            const dev::eth::BlockChain& _bc, dev::eth::BlockNumber _bn = dev::eth::LatestBlock ) { \
            return isPatchEnabled( getEnum(), _bc, _bn );                                          \
        }                                                                                          \
        static bool isEnabledWhen( time_t _committedBlockTimestamp ) {                             \
            return isPatchEnabledWhen( getEnum(), _committedBlockTimestamp );                      \
        }                                                                                          \
    };

#define DEFINE_EVM_PATCH( BlaBlaPatch )                                                            \
    class BlaBlaPatch : public SchainPatch {                                                       \
    public:                                                                                        \
        static SchainPatchEnum getEnum() { return SchainPatchEnum::BlaBlaPatch; }                  \
        static bool isEnabled(                                                                     \
            const dev::eth::BlockChain& _bc, dev::eth::BlockNumber _bn = dev::eth::LatestBlock ) { \
            return isPatchEnabled( getEnum(), _bc, _bn );                                          \
        }                                                                                          \
        static bool isEnabledWhen( time_t _committedBlockTimestamp ) {                             \
            return isPatchEnabledWhen( getEnum(), _committedBlockTimestamp );                      \
        }                                                                                          \
        static dev::eth::EVMSchedule makeSchedule( const dev::eth::EVMSchedule& base );            \
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

#endif  // SCHAINPATCH_H
