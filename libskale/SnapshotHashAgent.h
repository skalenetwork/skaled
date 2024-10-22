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
#include <boost/algorithm/string.hpp>
#include <libff/algebra/curves/alt_bn128/alt_bn128_pp.hpp>

namespace dev {
namespace test {
class SnapshotHashAgentTest;
}
namespace eth {
class Client;
}
}  // namespace dev

class SnapshotHashAgentException : public std::exception {
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
    SnapshotHashAgent( const dev::eth::ChainParams& chainParams,
        const std::array< std::string, 4 >& commonPublicKey,
        const std::string& urlToDownloadSnapshotFrom = "" );

    std::vector< std::string > getNodesToDownloadSnapshotFrom( unsigned blockNumber );

    std::pair< dev::h256, libff::alt_bn128_G1 > getVotedHash() const;

    friend class dev::test::SnapshotHashAgentTest;

private:
    dev::eth::ChainParams chainParams_;
    unsigned n_;
    std::string urlToDownloadSnapshotFrom_;
    std::shared_ptr< libBLS::Bls > bls_;

    std::vector< dev::h256 > hashes_;
    std::vector< libff::alt_bn128_G1 > signatures_;
    std::vector< libff::alt_bn128_G2 > public_keys_;
    std::vector< size_t > nodesToDownloadSnapshotFrom_;
    std::vector< bool > isReceived_;
    std::mutex hashesMutex;
    libff::alt_bn128_G2 commonPublicKey_;

    bool voteForHash();
    void readPublicKeyFromConfig();
    std::tuple< dev::h256, libff::alt_bn128_G1, libff::alt_bn128_G2 > askNodeForHash(
        const std::string& url, unsigned blockNumber );
    std::pair< dev::h256, libff::alt_bn128_G1 > votedHash_;

    size_t verifyAllData() const;
};

#endif  // SNAPSHOTHASHAGENT_H
