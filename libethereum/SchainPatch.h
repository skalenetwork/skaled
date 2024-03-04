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
    static void printInfo( const std::string& _patchName, time_t _timeStamp ) {
        if ( _timeStamp == 0 ) {
            cnote << "Patch " << _patchName << " is disabled";
        } else {
            cnote << "Patch " << _patchName << " is set at timestamp " << _timeStamp;
        }
    }
    static void init( const dev::eth::ChainOperationParams& _cp );
    static void useLatestBlockTimestamp( time_t _timestamp );

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
            time_t timestamp = chainParams.getPatchTimestamp( getName() );                         \
            return _bc.isPatchTimestampActiveInBlockNumber( timestamp, _bn );                      \
        }                                                                                          \
        static bool isEnabledWhen( time_t _committedBlockTimestamp ) {                             \
            time_t activationTimestamp = chainParams.getPatchTimestamp( getName() );               \
            return activationTimestamp != 0 && _committedBlockTimestamp >= activationTimestamp;    \
        }                                                                                          \
    };

#define DEFINE_EVM_PATCH( BlaBlaPatch )                                                            \
    class BlaBlaPatch : public SchainPatch {                                                       \
    public:                                                                                        \
        static std::string getName() { return #BlaBlaPatch; }                                      \
        static bool isEnabled(                                                                     \
            const dev::eth::BlockChain& _bc, dev::eth::BlockNumber _bn = dev::eth::LatestBlock ) { \
            time_t timestamp = _bc.chainParams().getPatchTimestamp( getName() );                   \
            return _bc.isPatchTimestampActiveInBlockNumber( timestamp, _bn );                      \
        }                                                                                          \
        static bool isEnabledWhen( time_t _committedBlockTimestamp ) {                             \
            time_t my_timestamp = chainParams.getPatchTimestamp( getName() );                      \
            return _committedBlockTimestamp >= my_timestamp;                                       \
        }                                                                                          \
        static dev::eth::EVMSchedule makeSchedule( const dev::eth::EVMSchedule& base );            \
    };

DEFINE_SIMPLE_PATCH( RevertableFSPatch )
DEFINE_AMNESIC_PATCH( PrecompiledConfigPatch )
DEFINE_SIMPLE_PATCH( POWCheckPatch )
DEFINE_SIMPLE_PATCH( CorrectForkInPowPatch )
DEFINE_AMNESIC_PATCH( ContractStorageZeroValuePatch )
DEFINE_EVM_PATCH( PushZeroPatch )
DEFINE_SIMPLE_PATCH( VerifyDaSigsPatch )

#endif  // SCHAINPATCH_H
