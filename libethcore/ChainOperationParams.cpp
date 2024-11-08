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
/** @file ChainOperationParams.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2015
 */

#include "ChainOperationParams.h"

#include <libethereum/SchainPatch.h>

#include <libdevcore/CommonData.h>
#include <libdevcore/Log.h>

#include <skutils/utils.h>

using namespace std;
using namespace dev;
using namespace eth;

PrecompiledContract::PrecompiledContract( unsigned _base, unsigned _word,
    PrecompiledExecutor const& _exec, u256 const& _startingBlock, h160Set const& _allowedAddresses )
    : PrecompiledContract(
          [=]( bytesConstRef _in, ChainOperationParams const&, u256 const& ) -> bigint {
              bigint s = _in.size();
              bigint b = _base;
              bigint w = _word;
              return b + ( s + 31 ) / 32 * w;
          },
          _exec, _startingBlock, _allowedAddresses ) {}

time_t SChain::getPatchTimestamp( SchainPatchEnum _patchEnum ) const {
    assert( _patchEnum < SchainPatchEnum::PatchesCount );
    if ( _patchEnum < SchainPatchEnum::PatchesCount )
        return _patchTimestamps[static_cast< size_t >( _patchEnum )];
    else
        return 0;
}

ChainOperationParams::ChainOperationParams()
    : m_blockReward( "0x4563918244F40000" ),
      minGasLimit( 0x1388 ),
      maxGasLimit( "0x7fffffffffffffff" ),
      gasLimitBoundDivisor( 0x0400 ),
      networkID( 0x0 ),
      minimumDifficulty( 0x020000 ),
      difficultyBoundDivisor( 0x0800 ),
      durationLimit( 0x0d ) {}

EVMSchedule const ChainOperationParams::makeEvmSchedule(
    time_t _committedBlockTimestamp, u256 const& _workingBlockNumber ) const {
    EVMSchedule result;

    // 1 decide by block number
    if ( _workingBlockNumber >= experimentalForkBlock )
        result = ExperimentalSchedule;
    else if ( _workingBlockNumber >= istanbulForkBlock )
        result = IstanbulSchedule;
    else if ( _workingBlockNumber >= constantinopleFixForkBlock )
        result = ConstantinopleFixSchedule;
    else if ( _workingBlockNumber >= constantinopleForkBlock )
        result = ConstantinopleSchedule;
    else if ( _workingBlockNumber >= byzantiumForkBlock )
        result = ByzantiumSchedule;
    else if ( _workingBlockNumber >= EIP158ForkBlock )
        result = EIP158Schedule;
    else if ( _workingBlockNumber >= EIP150ForkBlock )
        result = EIP150Schedule;
    else if ( _workingBlockNumber >= homesteadForkBlock )
        return HomesteadSchedule;
    else
        return FrontierSchedule;

    // 2 based on previous - decide by timestamp
    if ( PushZeroPatch::isEnabledWhen( _committedBlockTimestamp ) )
        result = PushZeroPatch::makeSchedule( result );

    return result;
}

u256 ChainOperationParams::blockReward( EVMSchedule const& _schedule ) const {
    if ( _schedule.blockRewardOverwrite )
        return *_schedule.blockRewardOverwrite;
    else
        return m_blockReward;
}

u256 ChainOperationParams::blockReward(
    time_t _committedBlockTimestamp, u256 const& _workingBlockNumber ) const {
    EVMSchedule const& schedule{ makeEvmSchedule( _committedBlockTimestamp, _workingBlockNumber ) };
    return blockReward( schedule );
}

void ChainOperationParams::setBlockReward( u256 const& _newBlockReward ) {
    m_blockReward = _newBlockReward;
}

time_t ChainOperationParams::getPatchTimestamp( SchainPatchEnum _patchEnum ) const {
    return sChain.getPatchTimestamp( _patchEnum );
}
