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
 * @file SnapshotHashAgent.cpp
 * @author Oleh Nikolaiev
 * @date 2019
 */

#include "SnapshotHashAgent.h"

#include <libethcore/CommonJS.h>
#include <libweb3jsonrpc/Skale.h>
#include <skutils/rest_call.h>

dev::h256 SnapshotHashAgent::voteForHash() {
    std::map< dev::h256, size_t > map_hash;

    const std::lock_guard< std::mutex > lock( this->hashes_mutex );

    for ( size_t i = 0; i < this->n_; ++i ) {
        if ( this->chain_params_.nodeInfo.id == this->chain_params_.sChain.nodes[i].id ) {
            continue;
        }

        map_hash[this->hashes_[i]] += 1;
    }

    auto it = std::find_if( map_hash.begin(), map_hash.end(),
        [this]( const std::pair< dev::h256, size_t > p ) { return 3 * p.second > 2 * this->n_; } );

    if ( it == map_hash.end() ) {
        throw std::logic_error( "note enough votes to choose hash" );
    } else {
        for ( size_t i = 0; i < this->n_; ++i ) {
            if ( this->chain_params_.nodeInfo.id == this->chain_params_.sChain.nodes[i].id ) {
                continue;
            }

            if ( this->hashes_[i] == ( *it ).first ) {
                this->nodes_to_download_snapshot_from_.push_back( i );
            }
        }
        return ( *it ).first;
    }
}

std::vector< std::string > SnapshotHashAgent::getNodesToDownloadSnapshotFrom(
    unsigned block_number ) {
    std::vector< std::thread > threads;
    for ( size_t i = 0; i < this->n_; ++i ) {
        if ( this->chain_params_.nodeInfo.id == this->chain_params_.sChain.nodes[i].id ) {
            continue;
        }

        threads.push_back( std::thread( [this, i, block_number]() {
            try {
                nlohmann::json joCall = nlohmann::json::object();
                joCall["jsonrpc"] = "2.0";
                joCall["method"] = "skale_getSnapshotHash";
                nlohmann::json obj = {block_number};
                joCall["params"] = obj;
                skutils::rest::client cli;
                bool fl = cli.open(
                    "http://" + this->chain_params_.sChain.nodes[i].ip + ':' +
                    ( this->chain_params_.sChain.nodes[i].port + 3 ).convert_to< std::string >() );
                if ( !fl ) {
                    std::cerr << cc::fatal( "FATAL:" )
                              << cc::error(
                                     " Exception while trying to connect to another skaled: " )
                              << cc::warn( "connection refused" ) << "\n";
                }
                skutils::rest::data_t d = cli.call( joCall );
                if ( d.empty() ) {
                    throw std::runtime_error( "Main Net call to skale_getSnapshotHash failed" );
                }
                std::string str_hash = nlohmann::json::parse( d.s_ )["result"];

                const std::lock_guard< std::mutex > lock( this->hashes_mutex );

                this->hashes_[i] = dev::h256( str_hash );
            } catch ( std::exception& ex ) {
                std::cerr << cc::error(
                                 "Exception while collecting snapshot hash from other skaleds: " )
                          << cc::warn( ex.what() ) << "\n";
            }
        } ) );
    }

    for ( auto& thr : threads ) {
        thr.join();
    }

    this->voted_hash_ = this->voteForHash();

    std::vector< std::string > ret;
    for ( const size_t idx : this->nodes_to_download_snapshot_from_ ) {
        std::string ret_value =
            std::string( "http://" ) + std::string( this->chain_params_.sChain.nodes[idx].ip ) +
            std::string( ":" ) +
            ( this->chain_params_.sChain.nodes[idx].port + 3 ).convert_to< std::string >();
        ret.push_back( ret_value );
    }

    return ret;
}

dev::h256 SnapshotHashAgent::getVotedHash() const {
    assert( this->voted_hash_ != dev::h256() );
    return this->voted_hash_;
}
