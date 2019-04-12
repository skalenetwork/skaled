#include "Web3.h"
#include <libdevcore/SHA3.h>
#include <libethcore/CommonJS.h>

using namespace std;
using namespace dev;

std::string rpc::Web3::web3_sha3( std::string const& _param1 ) {
    return toJS( sha3( jsToBytes( _param1 ) ) );
}