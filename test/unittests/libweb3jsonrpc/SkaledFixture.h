/*
Modifications Copyright (C) 2024 SKALE Labs

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

#ifndef SKALE_SKALEDFIXTURE_H
#define SKALE_SKALEDFIXTURE_H

#include <test/tools/libtesteth/TestHelper.h>
#include <test/tools/libtesteth/TestOutputHelper.h>

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace dev::test;

#define CHECK( __EXPRESSION__ )                                                                  \
    if ( !( __EXPRESSION__ ) ) {                                                                 \
        auto __msg__ = string( "Check failed::" ) + #__EXPRESSION__ + " " + string( __FILE__ ) + \
                       ":" + to_string( __LINE__ );                                              \
        throw std::runtime_error( __msg__ );                                                     \
    }

// Callback function to handle data received from the server
size_t WriteCallback( void* contents, size_t size, size_t nmemb, void* userp );

enum class TransactionWait { WAIT_FOR_COMPLETION, DONT_WAIT_FOR_COMPLETION };

class SkaledAccount {
    Secret key;
    u256 currentTransactionCountOnChain = 0;
    std::optional< u256 > lastSentNonce = std::nullopt;

    mutable std::shared_mutex mutex;

    static std::map< string, std::shared_ptr< SkaledAccount > > accounts;

    // Grant std::make_shared access to the private constructor
    friend std::shared_ptr< SkaledAccount > std::make_shared< SkaledAccount >();

public:
    static shared_ptr< SkaledAccount > getInstance(
        const Secret key, const u256 _currentTransactionCountOnChain ) {
        static std::mutex addressesMutex;
        std::lock_guard< std::mutex > lock( addressesMutex );
        auto account = std::shared_ptr< SkaledAccount >(
            new SkaledAccount( key, _currentTransactionCountOnChain ) );
        if ( !accounts.try_emplace( account->getAddressAsString() ).second ) {
            // another object was created, so return the existing one
            return accounts[account->getAddressAsString()] = account;
        } else {
            // return the newly created one
            return account;
        };
    }


    static shared_ptr< SkaledAccount > generate() {
        static std::mutex addressesMutex;
        Secret newKey = KeyPair::create().secret();
        return getInstance( newKey, 0 );
    }


    string getAddressAsString() const { return "0x" + KeyPair( key ).address().hex(); }


    const Secret& getKey() const;

    u256 getCurrentTransactionCountOnBlockchain() const {
        std::shared_lock< std::shared_mutex > lock( mutex );
        return currentTransactionCountOnChain;
    }

    u256 getLastSentNonce() const {
        std::shared_lock< std::shared_mutex > lock( mutex );
        if ( !lastSentNonce.has_value() ) {
            throw std::runtime_error( "No transaction has been sent from this account" );
        }
        return lastSentNonce.value();
    }

    u256 computeNonceForNextTransaction() {
        std::unique_lock< std::shared_mutex > lock( mutex );





        if ( lastSentNonce.has_value()) {
            throw std::runtime_error( "Previous transaction has not yet been confirmed" );
        }
        lastSentNonce = currentTransactionCountOnChain;



        return lastSentNonce.value();
    }

    void notifyLastTransactionCompleted() {
        std::unique_lock< std::shared_mutex > lock( mutex );


        if ( !lastSentNonce.has_value() ) {
            throw std::runtime_error( "No pending transaction for this account" );
        }

        CHECK( lastSentNonce == currentTransactionCountOnChain );

        currentTransactionCountOnChain++;

        lastSentNonce = std::nullopt;

    }

private:
    SkaledAccount( const Secret key, const u256 _currentTransactionCountOnChain );


    SkaledAccount& operator=( const SkaledAccount& ) {
        // these objects should exist in a single copy per account
        // thats why we use a shared pointer
        throw std::runtime_error( "Copying SkaledAccount objects is not allowed" );
    }
};


class SkaledFixture;

class CurlClient {
    CURL* curl;
    std::string readBuffer;
    struct curl_slist* headers = nullptr;

    string skaledEndpoint;

public:

    static std::atomic< uint64_t > totalCallsCount;

    static uint64_t getTotalCallsCount();

    void resetCurl();
    CurlClient( SkaledFixture& _fixture );

    void setRequest( const string& _json_rpc_request );

    Json::Value doRequestResponse();

    Json::Value parseResponse();

    string eth_sendRawTransaction( const std::string& _rawTransactionHex );
    void doRequestResponseAndCheckForError( std::string jsonPayload, Json::Value& response );


    u256 eth_getBalance( const std::string& _addressString );

    u256 eth_getTransactionCount( const std::string& _addressString );

    Json::Value  eth_getTransactionReceipt( const std::string& _hash );

    ~CurlClient();
};


class SkaledFixture : public TestOutputHelperFixture {
    static string readFile( const std::string& _path );

    static thread_local std::shared_ptr< CurlClient > curlClient;

public:
    std::shared_ptr< CurlClient > getThreadLocalCurlClient();

    void setupFirstKey();

    string deployERC20();

    void setupTwoToTheNKeys( uint64_t _n );
    void doOneTinyTransfersIteration();

    void sendTinyTransfersForAllAccounts(uint64_t _iterations);

    void readInsecurePrivateKeyFromHardhatConfig();

    static uint64_t getCurrentTimeMs();


    SkaledFixture( const std::string& _configPath );

    ~SkaledFixture() override;

    u256 getTransactionCount( const string& _address );

    u256 getCurrentGasPrice();

    u256 getBalance( const string& _address ) const;


    u256 getBalance( const SkaledAccount& _account ) const;

    void sendSingleTransfer( u256 _amount, std::shared_ptr< SkaledAccount > _from,
        const string& _to, const u256& _gasPrice,
        TransactionWait _wait = TransactionWait::WAIT_FOR_COMPLETION );

    string sendSingleDeploy( u256 _amount, std::shared_ptr< SkaledAccount > _from,
        const string& _byteCode, const u256& _gasPrice,
        TransactionWait _wait = TransactionWait::WAIT_FOR_COMPLETION );


    void splitAccountInHalves( std::shared_ptr< SkaledAccount > _from,
        std::shared_ptr< SkaledAccount > _to, u256& _gasPrice,
        TransactionWait _wait = TransactionWait::WAIT_FOR_COMPLETION );


    void sendTinyTransfer( std::shared_ptr< SkaledAccount > _from, const u256& _gasPrice,
        TransactionWait _wait = TransactionWait::WAIT_FOR_COMPLETION );


    unique_ptr< WebThreeStubClient > rpcClient() const;

    string skaledEndpoint;
    string ownerAddressStr;
    string ip;
    uint64_t basePort;
    uint64_t chainId;
    std::shared_ptr< SkaledAccount > ownerAccount;
    // map of test key addresses to secret keys
    map< string, std::shared_ptr< SkaledAccount > > testAccounts;
    vector< shared_ptr<SkaledAccount>> testAccountsVector;


    const string HARDHAT_CONFIG_FILE_NAME = "../../test/historicstate/hardhat/hardhat.config.js";
    uint64_t transactionTimeoutMs = 60000;
    bool verifyTransactions = false;
    bool useThreadsForTestKeyCreation = false;
    uint64_t threadsCountForTestTransactions = 1;

    void waitForTransaction( std::shared_ptr< SkaledAccount > _account );

    int timeBetweenTransactionCompletionChecksMs = 1000;
};


#endif  // SKALE_SKALEDFIXTURE_H
