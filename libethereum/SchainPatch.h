#ifndef SCHAINPATCH_H
#define SCHAINPATCH_H

namespace dev {
namespace eth {
class EVMSchedule;
}
}  // namespace dev

#include <libethereum/BlockChain.h>

#include <libethcore/ChainOperationParams.h>

#include <libdevcore/Log.h>
#include <libethcore/ChainOperationParams.h>

#include <string>

class SchainPatch {
public:
    static void init( const dev::eth::ChainOperationParams& _cp );
    static void useLatestBlockTimestamp( time_t _timestamp );

protected:
    static void printInfo( const std::string& _patchName, time_t _timeStamp ) {
        if ( _timeStamp == 0 ) {
            cnote << "Patch " << _patchName << " is disabled";
        } else {
            cnote << "Patch " << _patchName << " is set at timestamp " << _timeStamp;
        }
    }
    static bool isPatchEnabled( const std::string& _patchName, const dev::eth::BlockChain& _bc,
        dev::eth::BlockNumber _bn = dev::eth::LatestBlock ) {
        time_t timestamp = chainParams.getPatchTimestamp( _patchName );
        return _bc.isPatchTimestampActiveInBlockNumber( timestamp, _bn );
    }
    static bool isPatchEnabledWhen(
        const std::string& _patchName, time_t _committedBlockTimestamp ) {
        time_t activationTimestamp = chainParams.getPatchTimestamp( _patchName );
        return activationTimestamp != 0 && _committedBlockTimestamp >= activationTimestamp;
    }

protected:
    static dev::eth::ChainOperationParams chainParams;
    static time_t committedBlockTimestamp;
};

#define DEFINE_AMNESIC_PATCH( BlaBlaPatch )                                                    \
    class BlaBlaPatch : public SchainPatch {                                                   \
    public:                                                                                    \
        static std::string getName() { return #BlaBlaPatch; }                                  \
        static bool isEnabledInPendingBlock() {                                                \
            time_t activationTimestamp = chainParams.getPatchTimestamp( getName() );           \
            return activationTimestamp != 0 && committedBlockTimestamp >= activationTimestamp; \
        }                                                                                      \
    };

// TODO One more overload - with EnvInfo?
#define DEFINE_SIMPLE_PATCH( BlaBlaPatch )                                                         \
    class BlaBlaPatch : public SchainPatch {                                                       \
    public:                                                                                        \
        static std::string getName() { return #BlaBlaPatch; }                                      \
        static bool isEnabled(                                                                     \
            const dev::eth::BlockChain& _bc, dev::eth::BlockNumber _bn = dev::eth::LatestBlock ) { \
            return isPatchEnabled( getName(), _bc, _bn );                                          \
        }                                                                                          \
        static bool isEnabledWhen( time_t _committedBlockTimestamp ) {                             \
            return isPatchEnabledWhen( getName(), _committedBlockTimestamp );                      \
        }                                                                                          \
    };

#define DEFINE_EVM_PATCH( BlaBlaPatch )                                                            \
    class BlaBlaPatch : public SchainPatch {                                                       \
    public:                                                                                        \
        static std::string getName() { return #BlaBlaPatch; }                                      \
        static bool isEnabled(                                                                     \
            const dev::eth::BlockChain& _bc, dev::eth::BlockNumber _bn = dev::eth::LatestBlock ) { \
            return isPatchEnabled( getName(), _bc, _bn );                                          \
        }                                                                                          \
        static bool isEnabledWhen( time_t _committedBlockTimestamp ) {                             \
            return isPatchEnabledWhen( getName(), _committedBlockTimestamp );                      \
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
DEFINE_SIMPLE_PATCH( POWCheckPatch )

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
DEFINE_AMNESIC_PATCH( ContractStorageLimitPatch )

#endif  // SCHAINPATCH_H
