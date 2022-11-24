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

#pragma once

#include "Executive.h"

#include <libethcore/Common.h>
#include <libethcore/SealEngine.h>
#include <libevm/ExtVMFace.h>
#include <libskale/State.h>

#include <functional>
#include <map>

namespace dev {
namespace eth {
class SealEngineFace;

/// Externality interface for the Virtual Machine providing access to world state.
class ExtVM : public ExtVMFace {
public:
    /// Full constructor.
    ExtVM( State& _s, EnvInfo const& _envInfo, SealEngineFace const& _sealEngine,
        Address _myAddress, Address _caller, Address _origin, u256 _value, u256 _gasPrice,
        bytesConstRef _data, bytesConstRef _code, h256 const& _codeHash, u256 const& _version,
        unsigned _depth, bool _isCreate, bool _staticCall, bool _readOnly = true )
        : ExtVMFace( _envInfo, _myAddress, _caller, _origin, _value, _gasPrice, _data,
              _code.toBytes(), _codeHash, _version, _depth, _isCreate, _staticCall ),
          m_s( _s ),
          m_sealEngine( _sealEngine ),
          m_evmSchedule( initEvmSchedule( envInfo().number(), _version ) ),
          m_readOnly( _readOnly ) {
        // Contract: processing account must exist. In case of CALL, the ExtVM
        // is created only if an account has code (so exist). In case of CREATE
        // the account must be created first.
        assert( m_s.addressInUse( _myAddress ) );
    }

    /// Read storage location.
    virtual u256 store( u256 _n ) override final { return m_s.storage( myAddress, _n ); }

    /// Write a value in storage.
    virtual void setStore( u256 _n, u256 _v ) override final;

    /// Read original storage value (before modifications in the current transaction).
    u256 originalStorageValue( u256 const& _key ) final {
        return m_s.originalStorageValue( myAddress, _key );
    }

    /// Read address's code.
    virtual bytes const& codeAt( Address _a ) override final { return m_s.code( _a ); }

    /// @returns the size of the code in  bytes at the given address.
    virtual size_t codeSizeAt( Address _a ) override final;

    /// @returns the hash of the code at the given address.
    h256 codeHashAt( Address _a ) final;

    /// Create a new contract.
    CreateResult create( u256 _endowment, u256& io_gas, bytesConstRef _code, Instruction _op,
        u256 _salt, OnOpFunc const& _onOp = {} ) final;

    /// Create a new message call.
    CallResult call( CallParameters& _params ) final;

    /// Read address's balance.
    virtual u256 balance( Address _a ) override final { return m_s.balance( _a ); }

    /// Does the account exist?
    virtual bool exists( Address _a ) override final {
        if ( evmSchedule().emptinessIsNonexistence() )
            return m_s.accountNonemptyAndExisting( _a );
        else
            return m_s.addressInUse( _a );
    }

    /// Suicide the associated contract to the given address.
    virtual void suicide( Address _a ) override final;

    /// Return the EVM gas-price schedule for this execution context.
    virtual EVMSchedule const& evmSchedule() const override final {
        return m_sealEngine.evmSchedule( envInfo().number() );
    }

    State const& state() const { return m_s; }

    /// Hash of a block if within the last 256 blocks, or h256() otherwise.
    h256 blockHash( u256 _number ) override;

private:
    EVMSchedule const& initEvmSchedule( int64_t _blockNumber, u256 const& _version ) const {
        // If _version is latest for the block, select corresponding latest schedule.
        // Otherwise run with the latest schedule known to correspond to the _version.
        EVMSchedule const& currentBlockSchedule = m_sealEngine.evmSchedule( _blockNumber );
        if ( currentBlockSchedule.accountVersion == _version )
            return currentBlockSchedule;
        else
            return latestScheduleForAccountVersion( _version );
    }

    State& m_s;  ///< A reference to the base state.
    SealEngineFace const& m_sealEngine;
    EVMSchedule const& m_evmSchedule;
    bool m_readOnly;
};

}  // namespace eth
}  // namespace dev
