/*
    Modifications Copyright (C) 2018-2019 SKALE Labs

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

#include "SealEngine.h"
#include "TransactionBase.h"

#include <libethereum/SchainPatch.h>

#include "../libdevcore/microprofile.h"

using namespace std;
using namespace dev;
using namespace eth;

std::unique_ptr< SealEngineRegistrar > SealEngineRegistrar::s_this;

void NoProof::init() {
    ETH_REGISTER_SEAL_ENGINE( NoProof );
}

void SealEngineFace::verify( Strictness _s, BlockHeader const& _bi, BlockHeader const& _parent,
    bytesConstRef _block ) const {
    MICROPROFILE_ENTERI( "SealEngineFace", "verify", MP_TAN );
    _bi.verify( _s, _parent, _block );

    if ( _s != CheckNothingNew ) {
        if ( _bi.difficulty() < chainParams().minimumDifficulty )
            BOOST_THROW_EXCEPTION(
                InvalidDifficulty() << RequirementError(
                    bigint( chainParams().minimumDifficulty ), bigint( _bi.difficulty() ) ) );

        if ( _bi.gasLimit() < chainParams().minGasLimit )
            BOOST_THROW_EXCEPTION(
                InvalidGasLimit() << RequirementError(
                    bigint( chainParams().minGasLimit ), bigint( _bi.gasLimit() ) ) );

        if ( _bi.gasLimit() > chainParams().maxGasLimit )
            BOOST_THROW_EXCEPTION(
                InvalidGasLimit() << RequirementError(
                    bigint( chainParams().maxGasLimit ), bigint( _bi.gasLimit() ) ) );

        if ( _bi.number() && _bi.extraData().size() > chainParams().maximumExtraDataSize ) {
            BOOST_THROW_EXCEPTION(
                ExtraDataTooBig() << RequirementError( bigint( chainParams().maximumExtraDataSize ),
                                         bigint( _bi.extraData().size() ) )
                                  << errinfo_extraData( _bi.extraData() ) );
        }

        u256 const& daoHardfork = chainParams().daoHardforkBlock;
        if ( daoHardfork != 0 && daoHardfork + 9 >= daoHardfork && _bi.number() >= daoHardfork &&
             _bi.number() <= daoHardfork + 9 )
            if ( _bi.extraData() != fromHex( "0x64616f2d686172642d666f726b" ) )
                BOOST_THROW_EXCEPTION(
                    ExtraDataIncorrect() << errinfo_comment(
                        "Received block from the wrong fork (invalid extradata)." ) );
    }

    if ( _parent ) {
        auto gasLimit = _bi.gasLimit();
        auto parentGasLimit = _parent.gasLimit();
        if ( gasLimit < chainParams().minGasLimit || gasLimit > chainParams().maxGasLimit ||
             gasLimit <= parentGasLimit - parentGasLimit / chainParams().gasLimitBoundDivisor ||
             gasLimit >= parentGasLimit + parentGasLimit / chainParams().gasLimitBoundDivisor )
            BOOST_THROW_EXCEPTION(
                InvalidGasLimit()
                << errinfo_min( static_cast< bigint >(
                       static_cast< bigint >( parentGasLimit ) -
                       static_cast< bigint >(
                           parentGasLimit / chainParams().gasLimitBoundDivisor ) ) )
                << errinfo_got( static_cast< bigint >( gasLimit ) )
                << errinfo_max( static_cast< bigint >( static_cast< bigint >(
                       parentGasLimit + parentGasLimit / chainParams().gasLimitBoundDivisor ) ) ) );
    }
    MICROPROFILE_LEAVE();
}

void SealEngineFace::populateFromParent( BlockHeader& _bi, BlockHeader const& _parent ) const {
    _bi.populateFromParent( _parent );
}

void SealEngineFace::verifyTransaction( ChainOperationParams const& _chainParams,
    ImportRequirements::value _ir, TransactionBase const& _t, time_t _committedBlockTimestamp,
    BlockHeader const& _header, u256 const& _gasUsed ) {
    // verifyTransaction is the only place where TransactionBase is used instead of Transaction.
    u256 gas;
    if ( PowCheckPatch::isEnabledWhen( _committedBlockTimestamp ) ) {
        // new behavior is to use pow-enabled gas
        gas = _t.gas();
    } else {
        // old behavior is to use non-POW gas
        gas = _t.nonPowGas();
    }

    MICROPROFILE_SCOPEI( "SealEngineFace", "verifyTransaction", MP_ORCHID );
    if ( ( _ir & ImportRequirements::TransactionSignatures ) &&
         _header.number() < _chainParams.EIP158ForkBlock && _t.isReplayProtected() )
        BOOST_THROW_EXCEPTION( InvalidSignature() );

    if ( ( _ir & ImportRequirements::TransactionSignatures ) &&
         _header.number() < _chainParams.experimentalForkBlock && _t.hasZeroSignature() )
        BOOST_THROW_EXCEPTION( InvalidSignature() );

    if ( ( _ir & ImportRequirements::TransactionBasic ) &&
         _header.number() >= _chainParams.experimentalForkBlock && _t.hasZeroSignature() &&
         ( _t.value() != 0 || _t.gasPrice() != 0 || _t.nonce() != 0 ) )
        BOOST_THROW_EXCEPTION( InvalidZeroSignatureTransaction()
                               << errinfo_got( static_cast< bigint >( _t.gasPrice() ) )
                               << errinfo_got( static_cast< bigint >( _t.value() ) )
                               << errinfo_got( static_cast< bigint >( _t.nonce() ) ) );

    if ( _header.number() >= _chainParams.homesteadForkBlock &&
         ( _ir & ImportRequirements::TransactionSignatures ) && _t.hasSignature() )
        _t.checkLowS();

    eth::EVMSchedule const& schedule =
        _chainParams.makeEvmSchedule( _committedBlockTimestamp, _header.number() );

    // Pre calculate the gas needed for execution
    if ( ( _ir & ImportRequirements::TransactionBasic ) && _t.baseGasRequired( schedule ) > gas )
        BOOST_THROW_EXCEPTION( OutOfGasIntrinsic() << RequirementError(
                                   static_cast< bigint >( _t.baseGasRequired( schedule ) ),
                                   static_cast< bigint >( gas ) ) );

    // Avoid transactions that would take us beyond the block gas limit.
    if ( _gasUsed + static_cast< bigint >( gas ) > _header.gasLimit() )
        BOOST_THROW_EXCEPTION( BlockGasLimitReached() << RequirementErrorComment(
                                   static_cast< bigint >( _header.gasLimit() - _gasUsed ),
                                   static_cast< bigint >( gas ),
                                   string( "_gasUsed + (bigint)_t.gas() > _header.gasLimit()" ) ) );

    if ( _ir & ImportRequirements::TransactionSignatures ) {
        if ( _header.number() >= _chainParams.EIP158ForkBlock ) {
            uint64_t chainID = _chainParams.chainID;
            _t.checkChainId( chainID, _chainParams.skaleDisableChainIdCheck );
        }  // if
    }
}

SealEngineFace* SealEngineRegistrar::create( ChainOperationParams const& _params ) {
    SealEngineFace* ret = create( _params.sealEngineName );
    assert( ret && "Seal engine not found." );
    if ( ret )
        ret->setChainParams( _params );
    return ret;
}

EVMSchedule SealEngineBase::evmSchedule(
    time_t _committedBlockTimestamp, u256 const& _workingBlockNumber ) const {
    return chainParams().makeEvmSchedule( _committedBlockTimestamp, _workingBlockNumber );
}

u256 SealEngineBase::blockReward(
    time_t _committedBlockTimestamp, u256 const& _workingBlockNumber ) const {
    EVMSchedule const& schedule{ evmSchedule( _committedBlockTimestamp, _workingBlockNumber ) };
    return chainParams().blockReward( schedule );
}
