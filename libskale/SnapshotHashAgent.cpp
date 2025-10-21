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
#include <libethcore/CommonJS.h>
#include <libskale/AmsterdamFixPatch.h>
#include <libweb3jsonrpc/Skale.h>
#include <skutils/rest_call.h>
#include <libconsensus/libBLS/backends/interface/functions.hpp>

SnapshotHashAgent::SnapshotHashAgent( const dev::eth::ChainParams& chainParams,
    const std::array< std::string, 4 >& common_public_key,
    const std::string& urlToDownloadSnapshotFrom )
    : chainParams_( chainParams ),
      n_( chainParams.getNodesCount() ),
      urlToDownloadSnapshotFrom_( urlToDownloadSnapshotFrom ) {
    this->hashes_.resize( n_ );
    this->signatures_.resize( n_ );
    this->public_keys_.resize( n_ );
    this->isReceived_.resize( n_ );
    for ( size_t i = 0; i < n_; ++i ) {
        this->isReceived_[i] = false;
    }

    this->bls_.reset( new libBLS::Bls( ( 2 * this->n_ + 1 ) / 3, this->n_ ) );

    commonPublicKey_ = libBLS::algebra::G2Point::fromString( common_public_key, libBLS::Base::DEC );
    if ( !commonPublicKey_.isValid() || commonPublicKey_.isIdentity() ) {
        // zero or corrupted public key was provided in command line
        this->readPublicKeyFromConfig();
    }
}

void SnapshotHashAgent::readPublicKeyFromConfig() {
    auto commonBlsPublicKeyArray = chainParams_.getCommonBlsPublicKey();
    this->commonPublicKey_ =
        libBLS::algebra::G2Point::fromString( commonBlsPublicKeyArray, libBLS::Base::DEC );
    if ( !this->commonPublicKey_.isValid() ) {
        throw SnapshotHashAgentException( "Invalid common public key" );
    }
}

size_t SnapshotHashAgent::verifyAllData() const {
    size_t verified = 0;
    for ( size_t i = 0; i < this->n_; ++i ) {
        if ( this->chainParams_.getSelfNodeId() == this->chainParams_.getNodeByIndex( i ).id ) {
            continue;
        }

        if ( this->isReceived_.at( i ) ) {
            bool is_verified = false;
            try {
                is_verified = this->bls_->Verify( this->hashes_.at( i ).asArray(),
                    this->signatures_.at( i ), this->public_keys_.at( i ) );
            } catch ( std::exception& ex ) {
                BOOST_LOG( m_loggerError ) << ex.what();
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
        if ( this->chainParams_.getSelfNodeId() == this->chainParams_.getNodeByIndex( i ).id ) {
            continue;
        }

        map_hash[this->hashes_.at( i )] += 1;
    }

    std::map< dev::h256, size_t >::iterator it;
    it = std::find_if( map_hash.begin(), map_hash.end(),
        [this]( const std::pair< dev::h256, size_t > p ) { return 3 * p.second > 2 * this->n_; } );
    BOOST_LOG( m_loggerInfo ) << "Snapshot hash is: " << ( *it ).first << ". Verifying it...";

    if ( it == map_hash.end() ) {
        throw NotEnoughVotesException( "note enough votes to choose hash" );
        return false;
    } else {
        std::vector< size_t > idx;
        std::vector< libBLS::algebra::G1Point > signatures;
        for ( size_t i = 0; i < this->n_; ++i ) {
            if ( this->chainParams_.getSelfNodeId() == this->chainParams_.getNodeByIndex( i ).id ) {
                continue;
            }

            if ( this->hashes_.at( i ) == ( *it ).first ) {
                this->nodesToDownloadSnapshotFrom_.push_back( i );
                idx.push_back( i + 1 );
                signatures.push_back( this->signatures_.at( i ) );
            }
        }

        std::vector< libBLS::algebra::FrScalar > lagrange_coeffs;
        libBLS::algebra::G1Point common_signature;
        try {
            lagrange_coeffs = libBLS::algebra::lagrangeCoeffs( idx, ( 2 * this->n_ + 1 ) / 3 );
            common_signature = this->bls_->SignatureRecover( signatures, lagrange_coeffs );
        } catch ( libBLS::ThresholdUtils::IncorrectInput& ex ) {
            BOOST_LOG( m_loggerError )
                << "Exception while recovering common signature from other skaleds: " << ex.what();
        } catch ( libBLS::ThresholdUtils::IsNotWellFormed& ex ) {
            BOOST_LOG( m_loggerError )
                << "Exception while recovering common signature from other skaleds: " << ex.what();
        }

        bool is_verified = false;

        try {
            is_verified = this->bls_->Verify(
                ( *it ).first.asArray(), common_signature, this->commonPublicKey_ );
        } catch ( libBLS::ThresholdUtils::IsNotWellFormed& ex ) {
            BOOST_LOG( m_loggerError )
                << "Exception while verifying common signature from other skaleds: " << ex.what();
        }

        if ( !is_verified ) {
            BOOST_LOG( m_loggerError )
                << "Common BLS signature wasn't verified, probably using incorrect "
                   "common public key specified in command line. Trying again with "
                   "common public key from config";

            auto commonBlsPublicKeyArray = chainParams_.getCommonBlsPublicKey();
            libBLS::algebra::G2Point commonPublicKeyFromConfig =
                libBLS::algebra::G2Point::fromString( commonBlsPublicKeyArray, libBLS::Base::DEC );

            BOOST_LOG( m_loggerDebug ) << "NEW BLS COMMON PUBLIC KEY:";
            auto coords = commonPublicKeyFromConfig.toStringArray( libBLS::Base::DEC );
            BOOST_LOG( m_loggerDebug ) << "X.c0: " << coords[0];
            BOOST_LOG( m_loggerDebug ) << "X.c1: " << coords[1];
            BOOST_LOG( m_loggerDebug ) << "Y.c0: " << coords[2];
            BOOST_LOG( m_loggerDebug ) << "Y.c1: " << coords[3];
            try {
                is_verified = this->bls_->Verify(
                    ( *it ).first.asArray(), common_signature, commonPublicKeyFromConfig );
            } catch ( libBLS::ThresholdUtils::IsNotWellFormed& ex ) {
                BOOST_LOG( m_loggerError )
                    << "Exception while verifying common signature from other skaleds: "
                    << ex.what();
            }

            if ( !is_verified ) {
                BOOST_LOG( m_loggerError )
                    << "Common BLS signature wasn't verified, snapshot will not be "
                       "downloaded. Try to backup node manually using skale-node-cli.";
                return false;
            } else {
                BOOST_LOG( m_loggerInfo )
                    << "Common BLS signature was verified with common public key "
                       "from config.";
                this->commonPublicKey_ = commonPublicKeyFromConfig;
            }
        }

        this->votedHash_.first = ( *it ).first;
        this->votedHash_.second = common_signature;

        return true;
    }

    return true;
}

std::tuple< dev::h256, libBLS::algebra::G1Point, libBLS::algebra::G2Point >
SnapshotHashAgent::askNodeForHash( const std::string& url, unsigned blockNumber ) {
    jsonrpc::HttpClient* jsonRpcClient = new jsonrpc::HttpClient( url );
    SkaleClient skaleClient( *jsonRpcClient );

    Json::Value joSignatureResponse;
    try {
        joSignatureResponse = skaleClient.skale_getSnapshotSignature( blockNumber );
    } catch ( jsonrpc::JsonRpcException& ex ) {
        BOOST_LOG( m_loggerError )
            << "WARNING "
            << "Error while trying to get snapshot signature from " << url << " : " << ex.what();
        delete jsonRpcClient;
        return {};
    }

    if ( !joSignatureResponse.get( "hash", 0 ) || !joSignatureResponse.get( "X", 0 ) ||
         !joSignatureResponse.get( "Y", 0 ) ) {
        BOOST_LOG( m_loggerError ) << "WARNING "
                                   << " Signature from " + url +
                                          "-th node was not received during "
                                          "getNodesToDownloadSnapshotFrom ";
        delete jsonRpcClient;

        return {};
    } else {
        std::string strHash = joSignatureResponse["hash"].asString();
        BOOST_LOG( m_loggerInfo ) << "Received snapshot hash from " << url << " : " << strHash;

        std::string xElement = joSignatureResponse["X"].asString();
        std::string yElement = joSignatureResponse["Y"].asString();
        libBLS::algebra::G1Point signature = libBLS::algebra::G1Point(
            libBLS::algebra::FqElement::fromString( xElement, libBLS::Base::DEC ),
            libBLS::algebra::FqElement::fromString( yElement, libBLS::Base::DEC ),
            libBLS::algebra::FqElement::one() );

        libBLS::algebra::G2Point publicKey;
        if ( urlToDownloadSnapshotFrom_.empty() ) {
#ifdef FAIR
            Json::Value joPublicKeyResponse = skaleClient.skale_getBLSPublicKey();
#else
            Json::Value joPublicKeyResponse = skaleClient.skale_imaInfo();
#endif
            std::array< std::string, libBLS::algebra::G2_NUM_COMPONENTS_AFFINE > arrPublicKey;
            arrPublicKey[0] = joPublicKeyResponse["BLSPublicKey0"].asString();
            arrPublicKey[1] = joPublicKeyResponse["BLSPublicKey1"].asString();
            arrPublicKey[2] = joPublicKeyResponse["BLSPublicKey2"].asString();
            arrPublicKey[3] = joPublicKeyResponse["BLSPublicKey3"].asString();

            publicKey = libBLS::algebra::G2Point::fromString( arrPublicKey, libBLS::Base::DEC );
        } else {
            publicKey = libBLS::algebra::G2Point::generator();
            publicKey.toAffineCoordinates();
        }

        delete jsonRpcClient;

        return { dev::h256( strHash ), signature, publicKey };
    }
}

std::vector< std::string > SnapshotHashAgent::getNodesToDownloadSnapshotFrom(
    unsigned blockNumber ) {
    std::vector< std::thread > threads;

    if ( urlToDownloadSnapshotFrom_.empty() ) {
        for ( size_t i = 0; i < this->n_; ++i ) {
            if ( this->chainParams_.getSelfNodeId() == this->chainParams_.getNodeByIndex( i ).id ) {
                continue;
            }

            threads.push_back( std::thread( [this, i, blockNumber]() {
                try {
                    std::string nodeUrl = "http://" + this->chainParams_.getNodeByIndex( i ).ip +
                                          ':' +
                                          ( this->chainParams_.getNodeByIndex( i ).port + 3 )
                                              .convert_to< std::string >();

                    auto [snapshotHash, snapshotSignature, nodeBlsPublicKey] =
                        askNodeForHash( nodeUrl, blockNumber );
                    if ( snapshotHash != dev::h256() ) {
                        const std::lock_guard< std::mutex > lock( this->hashesMutex );

                        this->isReceived_.at( i ) = true;
                        this->hashes_.at( i ) = snapshotHash;
                        this->signatures_.at( i ) = snapshotSignature;
                        this->public_keys_.at( i ) = nodeBlsPublicKey;
                    }
                } catch ( std::exception& ex ) {
                    BOOST_LOG( m_loggerError )
                        << "Exception while collecting snapshot signatures from other skaleds: "
                        << ex.what();
                }
            } ) );
        }

        for ( auto& thr : threads ) {
            thr.join();
        }
    } else {
        auto [snapshotHash, snapshotSignature, nodeBlsPublicKey] =
            askNodeForHash( urlToDownloadSnapshotFrom_, blockNumber );
        this->votedHash_ = { snapshotHash, snapshotSignature };
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

            u256 id = this->chainParams_.getNodeByIndex( pos ).id;
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
            BOOST_LOG( m_loggerError )
                << "Exception while voting for snapshot hash from other skaleds: " << ex.what();
        } catch ( std::exception& ex ) {
            BOOST_LOG( m_loggerError )
                << "Exception while voting for snapshot hash from other skaleds: " << ex.what();
        }  // catch

    if ( !result ) {
        BOOST_LOG( m_loggerInfo ) << "Not enough nodes to choose snapshot hash for block "
                                  << blockNumber;
        return {};
    }

    std::vector< std::string > ret;
    for ( const size_t idx : this->nodesToDownloadSnapshotFrom_ ) {
        std::string ret_value =
            std::string( "http://" ) + std::string( this->chainParams_.getNodeByIndex( idx ).ip ) +
            std::string( ":" ) +
            ( this->chainParams_.getNodeByIndex( idx ).port + 3 ).convert_to< std::string >();
        ret.push_back( ret_value );
    }

    return ret;
}

std::pair< dev::h256, libBLS::algebra::G1Point > SnapshotHashAgent::getVotedHash() const {
    if ( this->votedHash_.first == dev::h256() ) {
        throw std::invalid_argument( "Hash is empty" );
    }

    if ( AmsterdamFixPatch::snapshotHashCheckingEnabled( this->chainParams_ ) ) {
        if ( !this->votedHash_.second.isValid() || this->votedHash_.second.isIdentity() ) {
            throw std::invalid_argument( "Signature is not well formed" );
        }
    }

    return this->votedHash_;
}
