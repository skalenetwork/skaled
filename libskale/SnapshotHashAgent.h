/*
    Copyright (C) 2019-present, SKALE Labs

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
 * @file SnapshotHashAgent.h
 * @author oleh Nikolaiev
 * @date 2019
 */

#ifndef SNAPSHOTHASHAGENT_H
#define SNAPSHOTHASHAGENT_H

#include <libethereum/ChainParams.h>

class SnapshotHashAgent {
public:
    SnapshotHashAgent( const dev::eth::ChainParams& chain_params )
        : chain_params_( chain_params ), n_( chain_params.sChain.nodes.size() ), idx_( 0 ) {
        this->hashes_.resize( n_ );
    }

    unsigned getBlockNumber( const std::string& strURLWeb3 );

    void getHashFromOthers();

    dev::h256 voteForHash();

    std::string getNodeToDownloadSnapshotFrom();

private:
    dev::eth::ChainParams chain_params_;
    unsigned n_;

    unsigned block_number_;
    std::vector< dev::h256 > hashes_;
    std::vector< size_t > nodes_to_download_snapshot_from_;
    unsigned idx_;
    std::mutex hashes_mutex;
};

#endif  // SNAPSHOTHASHAGENT_H