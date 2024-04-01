#ifndef CONSENSUSGASPRICER_H
#define CONSENSUSGASPRICER_H

#include <libethereum/GasPricer.h>

class SkaleHost;

class ConsensusGasPricer : public dev::eth::GasPricer {
public:
    ConsensusGasPricer( const SkaleHost& _host );

    virtual dev::u256 ask( dev::eth::Block const& ) const override;
    virtual dev::u256 bid( unsigned _blockNumber = dev::eth::LatestBlock,
        dev::eth::TransactionPriority _p = dev::eth::TransactionPriority::Medium ) const override;

private:
    const SkaleHost& m_skaleHost;
};

#endif  // CONSENSUSGASPRICER_H
