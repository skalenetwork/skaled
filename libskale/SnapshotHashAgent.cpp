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
#include "SkaleClient.h"

#include <jsonrpccpp/client/connectors/httpclient.h>
#include <libconsensus/libBLS/tools/utils.h>
#include <libethcore/CommonJS.h>
#include <libskale/AmsterdamFixPatch.h>
#include <libweb3jsonrpc/Skale.h>
#include <skutils/rest_call.h>
#include <libff/common/profiling.hpp>

SnapshotHashAgent::SnapshotHashAgent( const dev::eth::ChainParams& chain_params,
    const std::array< std::string, 4 >& common_public_key )
    : chain_params_( chain_params ), n_( chain_params.sChain.nodes.size() ) {
    this->hashes_.resize( n_ );
    this->signatures_.resize( n_ );
    this->public_keys_.resize( n_ );
    this->is_received_.resize( n_ );
    for ( size_t i = 0; i < n_; ++i ) {
        this->is_received_[i] = false;
    }

    this->bls_.reset( new libBLS::Bls( ( 2 * this->n_ + 1 ) / 3, this->n_ ) );
    common_public_key_.X.c0 = libff::alt_bn128_Fq( common_public_key[0].c_str() );
    common_public_key_.X.c1 = libff::alt_bn128_Fq( common_public_key[1].c_str() );
    common_public_key_.Y.c0 = libff::alt_bn128_Fq( common_public_key[2].c_str() );
    common_public_key_.Y.c1 = libff::alt_bn128_Fq( common_public_key[3].c_str() );
    common_public_key_.Z = libff::alt_bn128_Fq2::one();
    if ( ( common_public_key_.X == libff::alt_bn128_Fq2::zero() &&
             common_public_key_.Y == libff::alt_bn128_Fq2::one() ) ||
         !common_public_key_.is_well_formed() ) {
        // zero or corrupted public key was provided in command line
        this->readPublicKeyFromConfig();
    }
}

void SnapshotHashAgent::readPublicKeyFromConfig() {
    this->common_public_key_.X.c0 =
        libff::alt_bn128_Fq( chain_params_.nodeInfo.commonBLSPublicKeys[0].c_str() );
    this->common_public_key_.X.c1 =
        libff::alt_bn128_Fq( chain_params_.nodeInfo.commonBLSPublicKeys[1].c_str() );
    this->common_public_key_.Y.c0 =
        libff::alt_bn128_Fq( chain_params_.nodeInfo.commonBLSPublicKeys[2].c_str() );
    this->common_public_key_.Y.c1 =
        libff::alt_bn128_Fq( chain_params_.nodeInfo.commonBLSPublicKeys[3].c_str() );
    this->common_public_key_.Z = libff::alt_bn128_Fq2::one();
}

size_t SnapshotHashAgent::verifyAllData() const {
    size_t verified = 0;
    for ( size_t i = 0; i < this->n_; ++i ) {
        if ( this->chain_params_.nodeInfo.id == this->chain_params_.sChain.nodes[i].id ) {
            continue;
        }

        if ( this->is_received_[i] ) {
            bool is_verified = false;
            libff::inhibit_profiling_info = true;
            try {
                is_verified = this->bls_->Verification(
                    std::make_shared< std::array< uint8_t, 32 > >( this->hashes_[i].asArray() ),
                    this->signatures_[i], this->public_keys_[i] );
            } catch ( std::exception& ex ) {
                cerror << ex.what();
            }

            verified += is_verified;
            if ( !is_verified ) {
                cerror << "WARNING "
                       << " Signature from " + std::to_string( i ) +
                              "-th node was not verified during "
                              "getNodesToDownloadSnapshotFrom ";
            }
        }
    }

    return verified;
}

bool SnapshotHashAgent::voteForHash() {
    std::map< dev::h256, size_t > map_hash;

    if ( 3 * this->verifyAllData() < 2 * this->n_ + 1 ) {
        return false;
    }

    const std::lock_guard< std::mutex > lock( this->hashes_mutex );

    for ( size_t i = 0; i < this->n_; ++i ) {
        if ( this->chain_params_.nodeInfo.id == this->chain_params_.sChain.nodes[i].id ) {
            continue;
        }

        map_hash[this->hashes_[i]] += 1;
    }

    auto it = std::find_if( map_hash.begin(), map_hash.end(),
        [this]( const std::pair< dev::h256, size_t > p ) { return 3 * p.second > 2 * this->n_; } );
    cnote << "Snapshot hash is: " << ( *it ).first << " .Verifying it...\n";

    if ( it == map_hash.end() ) {
        throw NotEnoughVotesException( "note enough votes to choose hash" );
        return false;
    } else {
        std::vector< size_t > idx;
        std::vector< libff::alt_bn128_G1 > signatures;
        for ( size_t i = 0; i < this->n_; ++i ) {
            if ( this->chain_params_.nodeInfo.id == this->chain_params_.sChain.nodes[i].id ) {
                continue;
            }

            if ( this->hashes_[i] == ( *it ).first ) {
                this->nodes_to_download_snapshot_from_.push_back( i );
                idx.push_back( i + 1 );
                signatures.push_back( this->signatures_[i] );
            }
        }

        std::vector< libff::alt_bn128_Fr > lagrange_coeffs;
        libff::alt_bn128_G1 common_signature;
        try {
            lagrange_coeffs =
                libBLS::ThresholdUtils::LagrangeCoeffs( idx, ( 2 * this->n_ + 1 ) / 3 );
            common_signature = this->bls_->SignatureRecover( signatures, lagrange_coeffs );
        } catch ( libBLS::ThresholdUtils::IncorrectInput& ex ) {
            std::cerr << cc::error(
                             "Exception while recovering common signature from other skaleds: " )
                      << cc::warn( ex.what() ) << std::endl;
        } catch ( libBLS::ThresholdUtils::IsNotWellFormed& ex ) {
            std::cerr << cc::error(
                             "Exception while recovering common signature from other skaleds: " )
                      << cc::warn( ex.what() ) << std::endl;
        }

        bool is_verified = false;

        try {
            libff::inhibit_profiling_info = true;
            is_verified = this->bls_->Verification(
                std::make_shared< std::array< uint8_t, 32 > >( ( *it ).first.asArray() ),
                common_signature, this->common_public_key_ );
        } catch ( libBLS::ThresholdUtils::IsNotWellFormed& ex ) {
            std::cerr << cc::error(
                             "Exception while verifying common signature from other skaleds: " )
                      << cc::warn( ex.what() ) << std::endl;
        }

        if ( !is_verified ) {
            std::cerr << cc::error(
                             "Common BLS signature wasn't verified, probably using incorrect "
                             "common public key specified in command line. Trying again with "
                             "common public key from config" )
                      << std::endl;

            libff::alt_bn128_G2 common_public_key_from_config;
            common_public_key_from_config.X.c0 =
                libff::alt_bn128_Fq( this->chain_params_.nodeInfo.commonBLSPublicKeys[0].c_str() );
            common_public_key_from_config.X.c1 =
                libff::alt_bn128_Fq( this->chain_params_.nodeInfo.commonBLSPublicKeys[1].c_str() );
            common_public_key_from_config.Y.c0 =
                libff::alt_bn128_Fq( this->chain_params_.nodeInfo.commonBLSPublicKeys[2].c_str() );
            common_public_key_from_config.Y.c1 =
                libff::alt_bn128_Fq( this->chain_params_.nodeInfo.commonBLSPublicKeys[3].c_str() );
            common_public_key_from_config.Z = libff::alt_bn128_Fq2::one();
            std::cout << "NEW BLS COMMON PUBLIC KEY:\n";
            common_public_key_from_config.print_coordinates();
            try {
                is_verified = this->bls_->Verification(
                    std::make_shared< std::array< uint8_t, 32 > >( ( *it ).first.asArray() ),
                    common_signature, common_public_key_from_config );
            } catch ( libBLS::ThresholdUtils::IsNotWellFormed& ex ) {
                std::cerr << cc::error(
                                 "Exception while verifying common signature from other skaleds: " )
                          << cc::warn( ex.what() ) << std::endl;
            }

            if ( !is_verified ) {
                std::cerr << cc::error(
                                 "Common BLS signature wasn't verified, snapshot will not be "
                                 "downloaded. Try to backup node manually using skale-node-cli." )
                          << std::endl;
                return false;
            } else {
                std::cout << cc::info(
                                 "Common BLS signature was verified with common public key "
                                 "from config." )
                          << std::endl;
                this->common_public_key_ = common_public_key_from_config;
            }
        }

        this->voted_hash_.first = ( *it ).first;
        this->voted_hash_.second = common_signature;

        return true;
    }
}

std::vector< std::string > SnapshotHashAgent::getNodesToDownloadSnapshotFrom(
    unsigned block_number ) {
    libff::init_alt_bn128_params();
    std::vector< std::thread > threads;

    for ( size_t i = 0; i < this->n_; ++i ) {
        if ( this->chain_params_.nodeInfo.id == this->chain_params_.sChain.nodes[i].id ) {
            continue;
        }

        threads.push_back( std::thread( [this, i, block_number]() {
            try {
                jsonrpc::HttpClient* jsonRpcClient = new jsonrpc::HttpClient(
                    "http://" + this->chain_params_.sChain.nodes[i].ip + ':' +
                    ( this->chain_params_.sChain.nodes[i].port + 3 ).convert_to< std::string >() );
                SkaleClient skaleClient( *jsonRpcClient );

                // just ask block number in this special case
                if ( block_number == 0 ) {
                    unsigned n = skaleClient.skale_getLatestSnapshotBlockNumber();
                    if ( n == 0 ) {
                        const std::lock_guard< std::mutex > lock( this->hashes_mutex );
                        this->nodes_to_download_snapshot_from_.push_back( i );
                        delete jsonRpcClient;
                        return;
                    }
                }

                Json::Value joSignatureResponse;
                try {
                    joSignatureResponse = skaleClient.skale_getSnapshotSignature( block_number );
                } catch ( jsonrpc::JsonRpcException& ex ) {
                    cerror << "WARNING "
                           << "Error while trying to get snapshot signature from "
                           << this->chain_params_.sChain.nodes[i].ip << " : " << ex.what();
                    delete jsonRpcClient;
                    return;
                }

                if ( !joSignatureResponse.get( "hash", 0 ) || !joSignatureResponse.get( "X", 0 ) ||
                     !joSignatureResponse.get( "Y", 0 ) ) {
                    cerror << "WARNING "
                           << " Signature from " + std::to_string( i ) +
                                  "-th node was not received during "
                                  "getNodesToDownloadSnapshotFrom ";
                    delete jsonRpcClient;
                } else {
                    const std::lock_guard< std::mutex > lock( this->hashes_mutex );

                    this->is_received_[i] = true;

                    std::string str_hash = joSignatureResponse["hash"].asString();
                    cnote << "Received snapshot hash from "
                          << "http://" + this->chain_params_.sChain.nodes[i].ip + ':' +
                                 ( this->chain_params_.sChain.nodes[i].port + 3 )
                                     .convert_to< std::string >()
                          << " : " << str_hash << '\n';

                    libff::alt_bn128_G1 signature = libff::alt_bn128_G1(
                        libff::alt_bn128_Fq( joSignatureResponse["X"].asCString() ),
                        libff::alt_bn128_Fq( joSignatureResponse["Y"].asCString() ),
                        libff::alt_bn128_Fq::one() );

                    Json::Value joPublicKeyResponse = skaleClient.skale_imaInfo();

                    libff::alt_bn128_G2 public_key;
                    public_key.X.c0 =
                        libff::alt_bn128_Fq( joPublicKeyResponse["BLSPublicKey0"].asCString() );
                    public_key.X.c1 =
                        libff::alt_bn128_Fq( joPublicKeyResponse["BLSPublicKey1"].asCString() );
                    public_key.Y.c0 =
                        libff::alt_bn128_Fq( joPublicKeyResponse["BLSPublicKey2"].asCString() );
                    public_key.Y.c1 =
                        libff::alt_bn128_Fq( joPublicKeyResponse["BLSPublicKey3"].asCString() );
                    public_key.Z = libff::alt_bn128_Fq2::one();

                    this->hashes_[i] = dev::h256( str_hash );
                    this->signatures_[i] = signature;
                    this->public_keys_[i] = public_key;

                    delete jsonRpcClient;
                }
            } catch ( std::exception& ex ) {
                std::cerr
                    << cc::error(
                           "Exception while collecting snapshot signatures from other skaleds: " )
                    << cc::warn( ex.what() ) << std::endl;
            }
        } ) );
    }

    for ( auto& thr : threads ) {
        thr.join();
    }

    bool result = false;

    if ( !AmsterdamFixPatch::snapshotHashCheckingEnabled( this->chain_params_ ) ) {
        // keep only nodes from majorityNodesIds
        auto majorityNodesIds = AmsterdamFixPatch::majorityNodesIds();
        dev::h256 common_hash;  // should be same everywhere!
        for ( size_t pos = 0; pos < this->n_; ++pos ) {
            if ( !this->is_received_[pos] )
                continue;

            u256 id = this->chain_params_.sChain.nodes[pos].id;
            bool good = majorityNodesIds.end() !=
                        std::find( majorityNodesIds.begin(), majorityNodesIds.end(), id );
            if ( !good )
                continue;

            if ( common_hash == dev::h256() ) {
                common_hash = this->hashes_[pos];
                this->voted_hash_.first = common_hash;
                // .second will ne ignored!
            } else if ( this->hashes_[pos] != common_hash ) {
                result = false;
                break;
            }

            nodes_to_download_snapshot_from_.push_back( pos );

        }  // for i
        result = this->nodes_to_download_snapshot_from_.size() > 0;
    } else if ( block_number == 0 )
        result = this->nodes_to_download_snapshot_from_.size() * 3 >= 2 * this->n_ + 1;
    else
        try {
            result = this->voteForHash();
        } catch ( SnapshotHashAgentException& ex ) {
            std::cerr << cc::error(
                             "Exception while voting for snapshot hash from other skaleds: " )
                      << cc::warn( ex.what() ) << std::endl;
        } catch ( std::exception& ex ) {
            std::cerr << cc::error(
                             "Exception while voting for snapshot hash from other skaleds: " )
                      << cc::warn( ex.what() ) << std::endl;
        }  // catch

    if ( !result ) {
        cnote << "Not enough nodes to choose snapshot hash for block "
              << std::to_string( block_number );
        return {};
    }

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

std::pair< dev::h256, libff::alt_bn128_G1 > SnapshotHashAgent::getVotedHash() const {
    if ( this->voted_hash_.first == dev::h256() ) {
        throw std::invalid_argument( "Hash is empty" );
    }

    if ( AmsterdamFixPatch::snapshotHashCheckingEnabled( this->chain_params_ ) ) {
        if ( this->voted_hash_.second == libff::alt_bn128_G1::zero() ||
             !this->voted_hash_.second.is_well_formed() ) {
            throw std::invalid_argument( "Signature is not well formed" );
        }
    }

    return this->voted_hash_;
}
