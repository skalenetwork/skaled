/*
    Modifications Copyright (C) 2018 SKALE Labs

    This file is part of cpp-ethereum.

    cpp-ethereum is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    cpp-ethereum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
 */
/** @file FixedClient.h
 * @author Marek Kotewicz <marek@ethdev.com>
 * @date 2015
 */

#pragma once

#include <libethcore/Common.h>
#include <libethereum/BlockChain.h>
#include <libethereum/ClientBase.h>

using BlockNumber = unsigned;

namespace dev {
namespace test {

/**
 * @brief mvp implementation of ClientBase
 * Doesn't support mining interface
 */
class FixedClient : public dev::eth::ClientBase {
public:
    FixedClient( eth::BlockChain const& _bc, eth::Block const& _block )
        : m_bc( _bc ), m_block( _block ) {}

    // stub
    void flushTransactions() override {}
    eth::Transactions pending() const override {
        eth::Transactions res;
        return res;
    }
    eth::BlockChain& bc() override {
        BOOST_THROW_EXCEPTION(
            InterfaceNotSupported() << errinfo_interface( "FixedClient::bc()" ) );
    }
    eth::BlockChain const& bc() const override { return m_bc; }
    using ClientBase::latestBlock;
    eth::Block preSeal() const override {
        ReadGuard l( x_stateDB );
        return m_block;
    }
    eth::Block postSeal() const override {
        ReadGuard l( x_stateDB );
        return m_block;
    }
    void setAuthor( Address const& _us ) override {
        WriteGuard l( x_stateDB );
        m_block.setAuthor( _us );
    }
    void prepareForTransaction() override {}
    h256 submitTransaction( eth::TransactionSkeleton const&, Secret const& ) override {
        return {};
    };
    h256 importTransaction( eth::Transaction const& ) override { return {}; }
    eth::ExecutionResult call(
        Address const&, u256, Address, bytes const&, u256, u256,
#ifndef NO_ALETH_STATE
                                  BlockNumber,
#endif
eth::FudgeFactor ) override {
        return {};
    };
    eth::TransactionSkeleton populateTransactionWithDefaults(
        eth::TransactionSkeleton const& ) const override {
        return {};
    };

#ifndef NO_ALETH_STATE
        u256 alethStateBalanceAt(Address, BlockNumber) const override
           {return 0;}
        u256 alethStateCountAt(Address, BlockNumber) const override
           {return 0;}
        u256 alethStateAt(Address, u256, BlockNumber) const override
            {return 0;}
        h256 alethStateRootAt(Address, BlockNumber) const override
            {return h256();}
        bytes alethStateCodeAt(Address, BlockNumber) const override
            {return bytes();};
#endif

private:
    eth::BlockChain const& m_bc;
    eth::Block m_block;
    mutable SharedMutex x_stateDB;  ///< Lock on the state DB, effectively a lock on m_postSeal.
};

}  // namespace test
}  // namespace dev
