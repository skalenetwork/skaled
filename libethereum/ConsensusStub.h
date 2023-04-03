/*
    Copyright (C) 2018-present, SKALE Labs

    This file is part of skaled.

    skaled is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skaled is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skaled.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * @file SkaleHost.cpp
 * @author Dima Litvinov
 * @date 2018
 */

#ifndef LIBETHEREUM_CONSENSUSSTUB_H_
#define LIBETHEREUM_CONSENSUSSTUB_H_

using namespace std;
#include <libconsensus/node/ConsensusInterface.h>
#include <libdevcore/Common.h>
#include <libdevcore/FixedHash.h>
#include <libdevcore/Worker.h>
#include <map>
#include <vector>

class ConsensusExtFace;

class ConsensusStub : private dev::Worker, public ConsensusInterface {
public:
    ConsensusStub( ConsensusExtFace& _extFace, uint64_t _lastCommittedBlockID, u256 _stateRoot );
    ~ConsensusStub() override;
    void parseFullConfigAndCreateNode(
        const std::string& _jsonConfig, const string& _gethURL ) override;
    void startAll() override;
    void bootStrapAll() override;
    void exitGracefully() override;
    u256 getPriceForBlockId( uint64_t /*_blockId*/ ) const override { return 1000; }
    consensus_engine_status getStatus() const override { return CONSENSUS_ACTIVE; }  // moch

    void stop();

private:
    /// Called after thread is started from startWorking().
    virtual void startedWorking() override;
    /// Called continuously following sleep for m_idleWaitMs.
    virtual void doWork() override;
    /// Implement work loop here if it needs to be customized.
    virtual void workLoop() override;
    /// Called when is to be stopped, just prior to thread being joined.
    virtual void doneWorking() override;

private:
    ConsensusExtFace& m_extFace;
    int64_t blockCounter = 0;
    u256 stateRoot = 0;
};

#endif /* LIBETHEREUM_CONSENSUSSTUB_H_ */
