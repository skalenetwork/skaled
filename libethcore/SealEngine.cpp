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
#include "Exceptions.h"
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
        if ( _bi.difficulty() < chainParams().getMinimumDifficulty() )
            BOOST_THROW_EXCEPTION(
                InvalidDifficulty() << RequirementError(
                    bigint( chainParams().getMinimumDifficulty() ), bigint( _bi.difficulty() ) ) );

        if ( _bi.gasLimit() < chainParams().getMinGasLimit() )
            BOOST_THROW_EXCEPTION(
                InvalidGasLimit() << RequirementError(
                    bigint( chainParams().getMinGasLimit() ), bigint( _bi.gasLimit() ) ) );

        if ( _bi.gasLimit() > chainParams().getMaxGasLimit() )
            BOOST_THROW_EXCEPTION(
                InvalidGasLimit() << RequirementError(
                    bigint( chainParams().getMaxGasLimit() ), bigint( _bi.gasLimit() ) ) );

        if ( _bi.number() && _bi.extraData().size() > chainParams().getMaximumExtraDataSize() ) {
            BOOST_THROW_EXCEPTION( ExtraDataTooBig()
                                   << RequirementError(
                                          bigint( chainParams().getMaximumExtraDataSize() ),
                                          bigint( _bi.extraData().size() ) )
                                   << errinfo_extraData( _bi.extraData() ) );
        }

        u256 const& daoHardfork = chainParams().getDaoHardforkBlock();
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
        if ( gasLimit < chainParams().getMinGasLimit() ||
             gasLimit > chainParams().getMaxGasLimit() ||
             gasLimit <=
                 parentGasLimit - parentGasLimit / chainParams().getGasLimitBoundDivisor() ||
             gasLimit >= parentGasLimit + parentGasLimit / chainParams().getGasLimitBoundDivisor() )
            BOOST_THROW_EXCEPTION(
                InvalidGasLimit()
                << errinfo_min( static_cast< bigint >(
                       static_cast< bigint >( parentGasLimit ) -
                       static_cast< bigint >(
                           parentGasLimit / chainParams().getGasLimitBoundDivisor() ) ) )
                << errinfo_got( static_cast< bigint >( gasLimit ) )
                << errinfo_max( static_cast< bigint >( static_cast< bigint >(
                       parentGasLimit +
                       parentGasLimit / chainParams().getGasLimitBoundDivisor() ) ) ) );
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
#ifdef FAIR
    gas = _t.gas();
#else
    if ( PowCheckPatch::isEnabledWhen( _committedBlockTimestamp ) ) {
        // new behavior is to use pow-enabled gas
        gas = _t.gas();
    }

    else {
        // old behavior is to use non-POW gas
        gas = _t.nonPowGas();
    }
#endif

    MICROPROFILE_SCOPEI( "SealEngineFace", "verifyTransaction", MP_ORCHID );

    if ( _ir & ImportRequirements::TransactionSignatures ) {
        const bool isPreEIP155 = !_t.isReplayProtected();
        const bool beforeEIP155 = _header.number() < _chainParams.getEIP158ForkBlock();
        const bool needToEnforceChainId = !_chainParams.isChainIdCheckDisabled();
        const bool hasZeroSignature = _t.hasZeroSignature();
        const bool beforeExperimentalFork =
            _header.number() < _chainParams.getExperimentalForkBlock();

#ifdef FAIR
        const bool allowPreEIP155Txns = _chainParams.getAllowPreEIP155Txns();

        // Pre-EIP-155 tx not allowed
        if ( !allowPreEIP155Txns && isPreEIP155 ) {
            BOOST_THROW_EXCEPTION( PreEIP155LegacyTransactionNotAllowed()
                                   << errinfo_blockNumber( _header.number() )
                                   << errinfo_txHash( _t.sha3() ) );
        }
#endif
        if ( beforeEIP155 && !isPreEIP155 ) {
            BOOST_THROW_EXCEPTION( PreEIP155ReplayProtectionViolation()
                                   << errinfo_blockNumber( _header.number() )
                                   << errinfo_txHash( _t.sha3() ) );
        }

        if ( beforeExperimentalFork && hasZeroSignature ) {
            BOOST_THROW_EXCEPTION( InvalidSignature() );
        }

        if ( ( !beforeEIP155 && needToEnforceChainId ) ||
             ( !beforeEIP155 && !needToEnforceChainId && !isPreEIP155 ) ) {  // !isPreEIP155 = has
                                                                             // chainId
            _t.checkChainId( _chainParams.getChainId() );
        }
    }

    if ( ( _ir & ImportRequirements::TransactionBasic ) &&
         _header.number() >= _chainParams.getExperimentalForkBlock() && _t.hasZeroSignature() &&
         ( _t.value() != 0 || _t.gasPrice() != 0 || _t.nonce() != 0 ) )
        BOOST_THROW_EXCEPTION( InvalidZeroSignatureTransaction()
                               << errinfo_got( static_cast< bigint >( _t.gasPrice() ) )
                               << errinfo_got( static_cast< bigint >( _t.value() ) )
                               << errinfo_got( static_cast< bigint >( _t.nonce() ) ) );

    if ( _header.number() >= _chainParams.getHomesteadForkBlock() &&
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
        // Skip this check for CTX - they are not a subject for block gas limit
#ifdef BITE
    if ( !_t.isCTX() ) {
#endif
        if ( _gasUsed + static_cast< bigint >( gas ) > _header.gasLimit() )
            BOOST_THROW_EXCEPTION(
                BlockGasLimitReached()
                << RequirementErrorComment( static_cast< bigint >( _header.gasLimit() - _gasUsed ),
                       static_cast< bigint >( gas ),
                       string( "_gasUsed + (bigint)_t.gas() > _header.gasLimit()" ) ) );
#ifdef BITE
    }
#endif

    // EIP-1559: for Type 2 transactions, maxFeePerGas must be >= block baseFeePerGas.
    // maxPriorityFeePerGas <= maxFeePerGas is already enforced in TransactionBase parsing.
    if ( _t.txType() == 2 ) {
        u256 baseFee = _header.baseFeePerGas();
        if ( _t.maxFeePerGas() < baseFee )
            BOOST_THROW_EXCEPTION( InvalidTransactionFormat()
                << errinfo_comment( "maxFeePerGas < baseFeePerGas" ) );
    }
}

SealEngineFace* SealEngineRegistrar::create( ChainOperationParams const& _params ) {
    SealEngineFace* ret = create( _params.getSealEngineName() );
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
