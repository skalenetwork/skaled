#ifndef SCHAINPATCH_H
#define SCHAINPATCH_H

namespace dev {
namespace eth {
class EVMSchedule;
}
}  // namespace dev

#include <libethereum/BlockChain.h>

#include <libdevcore/Log.h>

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
};

#define DEFINE_BASIC_PATCH( BlaBlaPatch )                                                          \
    class BlaBlaPatch : public SchainPatch {                                                       \
    public:                                                                                        \
        static std::string getName() { return #BlaBlaPatch; }                                      \
        static bool isEnabled(                                                                     \
            const dev::eth::BlockChain& _bc, dev::eth::BlockNumber _bn = dev::eth::LatestBlock ) { \
            time_t timestamp = _bc.chainParams().getPatchTimestamp( getName() );                   \
            return _bc.isPatchTimestampActiveInBlockNumber( timestamp, _bn );                      \
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
        static dev::eth::EVMSchedule makeSchedule( const dev::eth::EVMSchedule& base );            \
    };

#endif  // SCHAINPATCH_H
