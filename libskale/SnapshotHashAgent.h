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
 * @author Oleh Nikolaiev
 * @date 2019
 */

#ifndef SNAPSHOTHASHAGENT_H
#define SNAPSHOTHASHAGENT_H

#include <libconsensus/libBLS/bls/bls.h>
#include <libethereum/ChainParams.h>
#include <libff/algebra/curves/alt_bn128/alt_bn128_pp.hpp>
#include <boost/algorithm/string.hpp>

namespace dev {
namespace test {
class SnapshotHashAgentTest;
}
}  // namespace dev

class SnapshotHashAgentException : std::exception {
protected:
    std::string what_str;

public:
    SnapshotHashAgentException( const std::string& err_str ) { what_str = err_str; }

    virtual const char* what() const noexcept override { return what_str.c_str(); }
};

class NotEnoughVotesException : public SnapshotHashAgentException {
public:
    NotEnoughVotesException( const std::string& err_str ) : SnapshotHashAgentException( err_str ) {
        what_str = "NotEnoughVotesException : " + err_str;
    }
};

class IsNotVerified : public SnapshotHashAgentException {
public:
    IsNotVerified( const std::string& err_str ) : SnapshotHashAgentException( err_str ) {
        what_str = "IsNotVerified : " + err_str;
    }
};

class SnapshotHashAgent {
public:
    SnapshotHashAgent( const dev::eth::ChainParams& chain_params, const std::string& common_public_key = "" )
        : chain_params_( chain_params ), n_( chain_params.sChain.nodes.size() ) {
        this->hashes_.resize( n_ );
        this->signatures_.resize( n_ );
        this->public_keys_.resize( n_ );
        if ( common_public_key == "" ) {
            common_public_key_.X.c0 =
                libff::alt_bn128_Fq( chain_params_.nodeInfo.insecureCommonBLSPublicKeys[0].c_str() );
            common_public_key_.X.c1 =
                libff::alt_bn128_Fq( chain_params_.nodeInfo.insecureCommonBLSPublicKeys[1].c_str() );
            common_public_key_.Y.c0 =
                libff::alt_bn128_Fq( chain_params_.nodeInfo.insecureCommonBLSPublicKeys[2].c_str() );
            common_public_key_.Y.c1 =
                libff::alt_bn128_Fq( chain_params_.nodeInfo.insecureCommonBLSPublicKeys[3].c_str() );
            common_public_key_.Z = libff::alt_bn128_Fq2::one();
        } else {
            std::vector<std::string> coords;
            boost::split(coords, common_public_key, [](char c){return c == ':';});
            common_public_key_.X.c0 = libff::alt_bn128_Fq( coords[0].c_str() );
            common_public_key_.X.c1 = libff::alt_bn128_Fq( coords[1].c_str() );
            common_public_key_.Y.c0 = libff::alt_bn128_Fq( coords[2].c_str() );
            common_public_key_.Y.c1 = libff::alt_bn128_Fq( coords[3].c_str() );
            common_public_key_.Z = libff::alt_bn128_Fq2::one();
        }
    }

    std::vector< std::string > getNodesToDownloadSnapshotFrom( unsigned block_number );

    std::pair< dev::h256, libff::alt_bn128_G1 > getVotedHash() const;

    friend class dev::test::SnapshotHashAgentTest;

private:
    dev::eth::ChainParams chain_params_;
    unsigned n_;
    std::shared_ptr< signatures::Bls > bls_;

    std::vector< dev::h256 > hashes_;
    std::vector< libff::alt_bn128_G1 > signatures_;
    std::vector< libff::alt_bn128_G2 > public_keys_;
    std::vector< size_t > nodes_to_download_snapshot_from_;
    std::mutex hashes_mutex;
    libff::alt_bn128_G2 common_public_key_;

    bool voteForHash( std::pair< dev::h256, libff::alt_bn128_G1 >& to_vote );
    std::pair< dev::h256, libff::alt_bn128_G1 > voted_hash_;

    void verifyAllData( bool& fl ) const;
};

#endif  // SNAPSHOTHASHAGENT_H
