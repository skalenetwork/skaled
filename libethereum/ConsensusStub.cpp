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

#include "ConsensusStub.h"

#include <libdevcore/Exceptions.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

using namespace dev;

using namespace std;

ConsensusStub::ConsensusStub( ConsensusExtFace& _extFace )
    : dev::Worker( "consensus_stub", 0 ),  // call doWork in a tight loop
      m_extFace( _extFace ) {
    // TODO Auto-generated constructor stub
}

ConsensusStub::~ConsensusStub() {
    // TODO Auto-generated destructor stub
}

void ConsensusStub::parseFullConfigAndCreateNode( const std::string& ) {
    // TODO think this architecture thoroughly
}

void ConsensusStub::startAll() {
    Worker::startWorking();
}

void ConsensusStub::bootStrapAll() {}

void ConsensusStub::stop() {
    Worker::stopWorking();
}

void ConsensusStub::exitGracefully() {
    Worker::terminate();
}

void ConsensusStub::startedWorking() {}

void ConsensusStub::doWork() {
    // Get some number of pending txns. Possibly in several calls.
    // Then loose some of them.
    // Then return block

    static const unsigned wanted_txn_count = 10;
    static const unsigned max_sleep = 2000;

    using transactions_vector = ConsensusExtFace::transactions_vector;

    transactions_vector txns = m_extFace.pendingTransactions( wanted_txn_count );
    // TODO Can return 0 on time-out. Needed for nice thread termination. Rethink this.
    if ( txns.size() == 0 )  // check for exit
        return;

    std::cout << "Taken " << txns.size() << " transactions for consensus" << std::endl;

    size_t txns_in_block = txns.size();  // rand()%txns.size();
                                         // any subset but not zero
    txns_in_block = txns_in_block ? txns_in_block : 1;

    transactions_vector out_vector;
    auto it = txns.begin();
    for ( unsigned i = 0; i < txns_in_block; i++ ) {
        out_vector.push_back( *it );
        ++it;
    }

    this_thread::sleep_for( std::chrono::milliseconds(
        static_cast< unsigned int >( rand() ) % ( max_sleep - 1000 ) + 1000 ) );

    try {
        m_extFace.createBlock( out_vector, ++blockCounter, time( NULL ), 0, 0 );  // TODO -
                                                                                  // implement
                                                                                  // pricing
        std::cout << "createBlock" << std::endl;
    } catch ( const dev::Exception& x ) {
        std::cout << x.what() << std::endl;
    }  // catch
}

void ConsensusStub::workLoop() {
    Worker::workLoop();
}

void ConsensusStub::doneWorking() {}
