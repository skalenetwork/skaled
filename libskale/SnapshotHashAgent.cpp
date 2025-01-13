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

SnapshotHashAgent::SnapshotHashAgent( const dev::eth::ChainParams& chainParams,
    const std::array< std::string, 4 >& common_public_key,
    const std::string& urlToDownloadSnapshotFrom )
    : chainParams_( chainParams ),
      n_( chainParams.sChain.nodes.size() ),
      urlToDownloadSnapshotFrom_( urlToDownloadSnapshotFrom ) {
    this->hashes_.resize( n_ );
    this->signatures_.resize( n_ );
    this->public_keys_.resize( n_ );
    this->isReceived_.resize( n_ );
    for ( size_t i = 0; i < n_; ++i ) {
        this->isReceived_[i] = false;
    }

    this->bls_.reset( new libBLS::Bls( ( 2 * this->n_ + 1 ) / 3, this->n_ ) );
    commonPublicKey_.X.c0 = libff::alt_bn128_Fq( common_public_key[0].c_str() );
    commonPublicKey_.X.c1 = libff::alt_bn128_Fq( common_public_key[1].c_str() );
    commonPublicKey_.Y.c0 = libff::alt_bn128_Fq( common_public_key[2].c_str() );
    commonPublicKey_.Y.c1 = libff::alt_bn128_Fq( common_public_key[3].c_str() );
    commonPublicKey_.Z = libff::alt_bn128_Fq2::one();
    if ( ( commonPublicKey_.X == libff::alt_bn128_Fq2::zero() &&
             commonPublicKey_.Y == libff::alt_bn128_Fq2::one() ) ||
         !commonPublicKey_.is_well_formed() ) {
        // zero or corrupted public key was provided in command line
        this->readPublicKeyFromConfig();
    }
}

void SnapshotHashAgent::readPublicKeyFromConfig() {
    this->commonPublicKey_.X.c0 =
        libff::alt_bn128_Fq( chainParams_.nodeInfo.commonBLSPublicKeys[0].c_str() );
    this->commonPublicKey_.X.c1 =
        libff::alt_bn128_Fq( chainParams_.nodeInfo.commonBLSPublicKeys[1].c_str() );
    this->commonPublicKey_.Y.c0 =
        libff::alt_bn128_Fq( chainParams_.nodeInfo.commonBLSPublicKeys[2].c_str() );
    this->commonPublicKey_.Y.c1 =
        libff::alt_bn128_Fq( chainParams_.nodeInfo.commonBLSPublicKeys[3].c_str() );
    this->commonPublicKey_.Z = libff::alt_bn128_Fq2::one();
}

size_t SnapshotHashAgent::verifyAllData() const {
    size_t verified = 0;
    for ( size_t i = 0; i < this->n_; ++i ) {
        if ( this->chainParams_.nodeInfo.id == this->chainParams_.sChain.nodes.at( i ).id ) {
            continue;
        }

        if ( this->isReceived_.at( i ) ) {
            bool is_verified = false;
            libff::inhibit_profiling_info = true;
            try {
                is_verified =
                    this->bls_->Verification( std::make_shared< std::array< uint8_t, 32 > >(
                                                  this->hashes_.at( i ).asArray() ),
                        this->signatures_.at( i ), this->public_keys_.at( i ) );
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

    if ( 3 * this->verifyAllData() < 2 * this->n_ + 1 && urlToDownloadSnapshotFrom_.empty() ) {
        return false;
    }

    const std::lock_guard< std::mutex > lock( this->hashesMutex );

    for ( size_t i = 0; i < this->n_; ++i ) {
        if ( this->chainParams_.nodeInfo.id == this->chainParams_.sChain.nodes.at( i ).id ) {
            continue;
        }

        map_hash[this->hashes_.at( i )] += 1;
    }

    std::map< dev::h256, size_t >::iterator it;
    it = std::find_if( map_hash.begin(), map_hash.end(),
        [this]( const std::pair< dev::h256, size_t > p ) { return 3 * p.second > 2 * this->n_; } );
    cnote << "Snapshot hash is: " << ( *it ).first << ". Verifying it...";

    if ( it == map_hash.end() ) {
        throw NotEnoughVotesException( "note enough votes to choose hash" );
        return false;
    } else {
        std::vector< size_t > idx;
        std::vector< libff::alt_bn128_G1 > signatures;
        for ( size_t i = 0; i < this->n_; ++i ) {
            if ( this->chainParams_.nodeInfo.id == this->chainParams_.sChain.nodes.at( i ).id ) {
                continue;
            }

            if ( this->hashes_.at( i ) == ( *it ).first ) {
                this->nodesToDownloadSnapshotFrom_.push_back( i );
                idx.push_back( i + 1 );
                signatures.push_back( this->signatures_.at( i ) );
            }
        }

        std::vector< libff::alt_bn128_Fr > lagrange_coeffs;
        libff::alt_bn128_G1 common_signature;
        try {
            lagrange_coeffs =
                libBLS::ThresholdUtils::LagrangeCoeffs( idx, ( 2 * this->n_ + 1 ) / 3 );
            common_signature = this->bls_->SignatureRecover( signatures, lagrange_coeffs );
        } catch ( libBLS::ThresholdUtils::IncorrectInput& ex ) {
            cerror << "Exception while recovering common signature from other skaleds: "
                   << ex.what();
        } catch ( libBLS::ThresholdUtils::IsNotWellFormed& ex ) {
            cerror << "Exception while recovering common signature from other skaleds: "
                   << ex.what();
        }

        bool is_verified = false;

        try {
            libff::inhibit_profiling_info = true;
            is_verified = this->bls_->Verification(
                std::make_shared< std::array< uint8_t, 32 > >( ( *it ).first.asArray() ),
                common_signature, this->commonPublicKey_ );
        } catch ( libBLS::ThresholdUtils::IsNotWellFormed& ex ) {
            cerror << "Exception while verifying common signature from other skaleds: "
                   << ex.what();
        }

        if ( !is_verified ) {
            cerror << "Common BLS signature wasn't verified, probably using incorrect "
                      "common public key specified in command line. Trying again with "
                      "common public key from config";

            libff::alt_bn128_G2 commonPublicKey_from_config;
            commonPublicKey_from_config.X.c0 =
                libff::alt_bn128_Fq( this->chainParams_.nodeInfo.commonBLSPublicKeys[0].c_str() );
            commonPublicKey_from_config.X.c1 =
                libff::alt_bn128_Fq( this->chainParams_.nodeInfo.commonBLSPublicKeys[1].c_str() );
            commonPublicKey_from_config.Y.c0 =
                libff::alt_bn128_Fq( this->chainParams_.nodeInfo.commonBLSPublicKeys[2].c_str() );
            commonPublicKey_from_config.Y.c1 =
                libff::alt_bn128_Fq( this->chainParams_.nodeInfo.commonBLSPublicKeys[3].c_str() );
            commonPublicKey_from_config.Z = libff::alt_bn128_Fq2::one();
            std::cout << "NEW BLS COMMON PUBLIC KEY:\n";
            commonPublicKey_from_config.print_coordinates();
            try {
                is_verified = this->bls_->Verification(
                    std::make_shared< std::array< uint8_t, 32 > >( ( *it ).first.asArray() ),
                    common_signature, commonPublicKey_from_config );
            } catch ( libBLS::ThresholdUtils::IsNotWellFormed& ex ) {
                cerror << "Exception while verifying common signature from other skaleds: "
                       << ex.what();
            }

            if ( !is_verified ) {
                cerror << "Common BLS signature wasn't verified, snapshot will not be "
                          "downloaded. Try to backup node manually using skale-node-cli.";
                return false;
            } else {
                cnote << "Common BLS signature was verified with common public key "
                         "from config.";
                this->commonPublicKey_ = commonPublicKey_from_config;
            }
        }

        this->votedHash_.first = ( *it ).first;
        this->votedHash_.second = common_signature;

        return true;
    }

    return true;
}

std::tuple< dev::h256, libff::alt_bn128_G1, libff::alt_bn128_G2 > SnapshotHashAgent::askNodeForHash(
    const std::string& url, unsigned blockNumber ) {
    jsonrpc::HttpClient* jsonRpcClient = new jsonrpc::HttpClient( url );
    SkaleClient skaleClient( *jsonRpcClient );

    Json::Value joSignatureResponse;
    try {
        joSignatureResponse = skaleClient.skale_getSnapshotSignature( blockNumber );
    } catch ( jsonrpc::JsonRpcException& ex ) {
        cerror << "WARNING "
               << "Error while trying to get snapshot signature from " << url << " : " << ex.what();
        delete jsonRpcClient;
        return {};
    }

    if ( !joSignatureResponse.get( "hash", 0 ) || !joSignatureResponse.get( "X", 0 ) ||
         !joSignatureResponse.get( "Y", 0 ) ) {
        cerror << "WARNING "
               << " Signature from " + url +
                      "-th node was not received during "
                      "getNodesToDownloadSnapshotFrom ";
        delete jsonRpcClient;

        return {};
    } else {
        std::string strHash = joSignatureResponse["hash"].asString();
        cnote << "Received snapshot hash from " << url << " : " << strHash << '\n';

        libff::alt_bn128_G1 signature =
            libff::alt_bn128_G1( libff::alt_bn128_Fq( joSignatureResponse["X"].asCString() ),
                libff::alt_bn128_Fq( joSignatureResponse["Y"].asCString() ),
                libff::alt_bn128_Fq::one() );

        libff::alt_bn128_G2 publicKey;
        if ( urlToDownloadSnapshotFrom_.empty() ) {
            Json::Value joPublicKeyResponse = skaleClient.skale_imaInfo();

            publicKey.X.c0 =
                libff::alt_bn128_Fq( joPublicKeyResponse["BLSPublicKey0"].asCString() );
            publicKey.X.c1 =
                libff::alt_bn128_Fq( joPublicKeyResponse["BLSPublicKey1"].asCString() );
            publicKey.Y.c0 =
                libff::alt_bn128_Fq( joPublicKeyResponse["BLSPublicKey2"].asCString() );
            publicKey.Y.c1 =
                libff::alt_bn128_Fq( joPublicKeyResponse["BLSPublicKey3"].asCString() );
            publicKey.Z = libff::alt_bn128_Fq2::one();
        } else {
            publicKey = libff::alt_bn128_G2::one();
            publicKey.to_affine_coordinates();
        }

        delete jsonRpcClient;

        return { dev::h256( strHash ), signature, publicKey };
    }
}

std::vector< std::string > SnapshotHashAgent::getNodesToDownloadSnapshotFrom(
    unsigned blockNumber ) {
    libff::init_alt_bn128_params();
    std::vector< std::thread > threads;

    if ( urlToDownloadSnapshotFrom_.empty() ) {
        for ( size_t i = 0; i < this->n_; ++i ) {
            if ( this->chainParams_.nodeInfo.id == this->chainParams_.sChain.nodes.at( i ).id ) {
                continue;
            }

            threads.push_back( std::thread( [this, i, blockNumber]() {
                try {
                    std::string nodeUrl = "http://" + this->chainParams_.sChain.nodes.at( i ).ip +
                                          ':' +
                                          ( this->chainParams_.sChain.nodes.at( i ).port + 3 )
                                              .convert_to< std::string >();
                    auto snapshotData = askNodeForHash( nodeUrl, blockNumber );
                    if ( std::get< 0 >( snapshotData ).size > 0 ) {
                        const std::lock_guard< std::mutex > lock( this->hashesMutex );

                        this->isReceived_.at( i ) = true;
                        this->hashes_.at( i ) = std::get< 0 >( snapshotData );
                        this->signatures_.at( i ) = std::get< 1 >( snapshotData );
                        this->public_keys_.at( i ) = std::get< 2 >( snapshotData );
                    }
                } catch ( std::exception& ex ) {
                    cerror << "Exception while collecting snapshot signatures from other skaleds: "
                           << ex.what();
                }
            } ) );
        }

        for ( auto& thr : threads ) {
            thr.join();
        }
    } else {
        auto snapshotData = askNodeForHash( urlToDownloadSnapshotFrom_, blockNumber );
        this->votedHash_ = { std::get< 0 >( snapshotData ), std::get< 1 >( snapshotData ) };
        return { urlToDownloadSnapshotFrom_ };
    }

    bool result = false;

    if ( !AmsterdamFixPatch::snapshotHashCheckingEnabled( this->chainParams_ ) ) {
        // keep only nodes from majorityNodesIds
        auto majorityNodesIds = AmsterdamFixPatch::majorityNodesIds();
        dev::h256 common_hash;  // should be same everywhere!
        for ( size_t pos = 0; pos < this->n_; ++pos ) {
            if ( !this->isReceived_.at( pos ) )
                continue;

            u256 id = this->chainParams_.sChain.nodes.at( pos ).id;
            bool good = majorityNodesIds.end() !=
                        std::find( majorityNodesIds.begin(), majorityNodesIds.end(), id );
            if ( !good )
                continue;

            if ( common_hash == dev::h256() ) {
                common_hash = this->hashes_.at( pos );
                this->votedHash_.first = common_hash;
                // .second will ne ignored!
            } else if ( this->hashes_.at( pos ) != common_hash ) {
                result = false;
                break;
            }

            nodesToDownloadSnapshotFrom_.push_back( pos );

        }  // for i
        result = this->nodesToDownloadSnapshotFrom_.size() > 0;
    } else
        try {
            result = this->voteForHash();
        } catch ( SnapshotHashAgentException& ex ) {
            cerror << "Exception while voting for snapshot hash from other skaleds: " << ex.what();
        } catch ( std::exception& ex ) {
            cerror << "Exception while voting for snapshot hash from other skaleds: " << ex.what();
        }  // catch

    if ( !result ) {
        cnote << "Not enough nodes to choose snapshot hash for block " << blockNumber;
        return {};
    }

    std::vector< std::string > ret;
    for ( const size_t idx : this->nodesToDownloadSnapshotFrom_ ) {
        std::string ret_value =
            std::string( "http://" ) + std::string( this->chainParams_.sChain.nodes.at( idx ).ip ) +
            std::string( ":" ) +
            ( this->chainParams_.sChain.nodes.at( idx ).port + 3 ).convert_to< std::string >();
        ret.push_back( ret_value );
    }

    return ret;
}

std::pair< dev::h256, libff::alt_bn128_G1 > SnapshotHashAgent::getVotedHash() const {
    if ( this->votedHash_.first == dev::h256() ) {
        throw std::invalid_argument( "Hash is empty" );
    }

    if ( AmsterdamFixPatch::snapshotHashCheckingEnabled( this->chainParams_ ) ) {
        if ( this->votedHash_.second == libff::alt_bn128_G1::zero() ||
             !this->votedHash_.second.is_well_formed() ) {
            throw std::invalid_argument( "Signature is not well formed" );
        }
    }

    return this->votedHash_;
}
