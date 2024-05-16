#include "ConsensusGasPricer.h"
#include <libethereum/Client.h>

using namespace dev::eth;

ConsensusGasPricer::ConsensusGasPricer( const SkaleHost& _host ) : m_skaleHost( _host ) {}

dev::u256 ConsensusGasPricer::ask( dev::eth::Block const& ) const {
    return bid();
}

dev::u256 ConsensusGasPricer::bid(
    unsigned _blockNumber, dev::eth::TransactionPriority /*_p*/ ) const {
    return m_skaleHost.getGasPrice( _blockNumber );
}
