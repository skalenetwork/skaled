/*
Modifications Copyright (C) 2024- SKALE Labs

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

#pragma GCC diagnostic ignored "-Wdeprecated"

#include "WebThreeStubClient.h"


#include <jsonrpccpp/client/connectors/httpclient.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/TransientDirectory.h>
#include <libethcore/CommonJS.h>
#include <libethcore/KeyManager.h>
#include <libethereum/SchainPatch.h>
#include <libskale/httpserveroverride.h>
#include <libweb3jsonrpc/AccountHolder.h>
#include <libweb3jsonrpc/AdminEth.h>
#include <libweb3jsonrpc/JsonHelper.h>
#include <boost/algorithm/string.hpp>

#include "libweb3jsonrpc/SkaleFace.h"

#include <libconsensus/SkaleCommon.h>
#include <libconsensus/oracle/OracleRequestSpec.h>
#include <libweb3jsonrpc/Debug.h>
#include <test/tools/libtesteth/TestHelper.h>
#include <test/tools/libtesteth/TestOutputHelper.h>
#include <boost/test/unit_test.hpp>

#include "SkaledFixture.h"
#include <cstdlib>

#include <boost/asio/placeholders.hpp>


// Callback function to handle data received from the server
size_t WriteCallback( void* contents, size_t size, size_t nmemb, void* userp ) {
    ( ( std::string* ) userp )->append( ( char* ) contents, size * nmemb );
    return size * nmemb;
}


void CurlClient::resetCurl() {
    if ( headers ) {
        curl_slist_free_all( headers );
        headers = curl_slist_append( nullptr, "Content-Type: application/json" );
    }
    curl_easy_reset( curl );
    curl_easy_setopt( curl, CURLOPT_URL, this->skaledEndpoint.c_str() );
    curl_easy_setopt( curl, CURLOPT_POST, 1L );
    curl_easy_setopt( curl, CURLOPT_POST, 1L );
    // Set up callback to capture response
    curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, WriteCallback );
    readBuffer = "";
    curl_easy_setopt( curl, CURLOPT_WRITEDATA, &readBuffer );
    // Set HTTP headers
    headers = curl_slist_append( nullptr, "Content-Type: application/json" );
    curl_easy_setopt( curl, CURLOPT_HTTPHEADER, headers );
}
CurlClient::CurlClient::CurlClient( SkaledFixture& _fixture ) {
    curl = curl_easy_init();
    CHECK( curl );
    skaledEndpoint = _fixture.skaledEndpoint;
    CHECK( !skaledEndpoint.empty() )
    resetCurl();
}

void CurlClient::setRequest( const string& _json_rpc_request ) {
    resetCurl();
    curl_easy_setopt( curl, CURLOPT_POSTFIELDS, _json_rpc_request.c_str() );
    curl_easy_setopt( curl, CURLOPT_POSTFIELDSIZE, _json_rpc_request.size() );
}

uint64_t CurlClient::doRequestResponse() {
    auto res = curl_easy_perform( curl );
    if ( res != CURLE_OK ) {
        throw std::runtime_error(
            string( "curl_easy_perform() failed" ) + curl_easy_strerror( res ) );
    }
    return ++totalCallsCount;
}

std::atomic< uint64_t > CurlClient::totalCallsCount = 0;

Json::Value CurlClient::parseResponse() {
    Json::CharReaderBuilder readerBuilder;
    Json::Value jsonData;
    std::string errs;
    std::istringstream s( readBuffer );
    if ( !Json::parseFromStream( readerBuilder, s, &jsonData, &errs ) ) {
        throw std::runtime_error( "Failed to parse JSON response: " + errs );
    }
    return jsonData;
}

CurlClient::~CurlClient() {
    curl_slist_free_all( headers );
    curl_easy_cleanup( curl );
}

uint64_t CurlClient::getTotalCallsCount() {
    return totalCallsCount;
}

void CurlClient::eth_sendRawTransaction( const std::string& _rawTransactionHex ) {
    std::string jsonPayload = R"({"jsonrpc":"2.0","method":"eth_sendRawTransaction","params":[")" +
                              _rawTransactionHex + R"("],"id":1})";
    setRequest( jsonPayload );
    doRequestResponse();
}

void CurlClient::doRequestResponseAndCheckForError(
    std::string jsonPayload, Json::Value& response ) {
    setRequest( jsonPayload );
    doRequestResponse();
    response = parseResponse();
    if ( response.isMember( "error" ) ) {
        auto errorObject = response["error"];
        string errorMessage = "eth_getBalance returned error.";
        if ( errorObject.isMember( "message" ) ) {
            errorMessage += errorObject["message"].asString();
        }
        throw runtime_error( errorMessage );
    }
}
u256 CurlClient::eth_getBalance( const std::string& _addressString ) {
    std::string jsonPayload = R"({"jsonrpc":"2.0","method":"eth_getBalance","params":[")" +
                              _addressString + R"(","latest"],"id":1})";
    Json::Value response;
    doRequestResponseAndCheckForError( jsonPayload, response );

    CHECK( response.isMember( "result" ) );
    auto resultStr = response["result"].asString();
    return jsToU256( resultStr );
}

u256 CurlClient::eth_getTransactionCount( const std::string& _addressString ) {
    std::string jsonPayload = R"({"jsonrpc":"2.0","method":"eth_getTransactionCount","params":[")" +
                              _addressString + R"(","latest"],"id":1})";
    Json::Value response;
    doRequestResponseAndCheckForError( jsonPayload, response );

    CHECK( response.isMember( "result" ) );
    auto resultStr = response["result"].asString();
    return jsToU256( resultStr );
}


string SkaledFixture::readFile( const std::string& _path ) {
    CHECK( boost::filesystem::exists( _path ) );

    boost::filesystem::ifstream stream( _path, std::ios::in | std::ios::binary );

    CHECK( stream.is_open() );

    string contents(
        ( std::istreambuf_iterator< char >( stream ) ), std::istreambuf_iterator< char >() );

    return contents;
}

thread_local ptr< CurlClient > SkaledFixture::curlClient;

ptr< CurlClient > SkaledFixture::getThreadLocalCurlClient() {
    if ( !curlClient ) {
        curlClient = make_shared< CurlClient >( *this );
    }
    return curlClient;
};

const u256 FIRST_WALLET_FUNDING( "10000000000000000000000000000000000000000000" );

void SkaledFixture::setupFirstKey() {
    auto firstAccount = SkaledAccount::generate();
    CHECK( testAccounts.try_emplace( firstAccount->getAddressAsString(), firstAccount ).second );
    auto gasPrice = getCurrentGasPrice();
    auto ownerBalance = getBalance( ownerAccount->getAddressAsString() );

    cout << "Owner wallet:" << ownerAccount->getAddressAsString() << endl;
    cout << "Owner balance, wei:" << ownerBalance << endl;
    cout << "First wallet:" << firstAccount->getAddressAsString() << endl;
    cout << "Gas price, wei " << gasPrice << endl;
    sendSingleTransfer(
        FIRST_WALLET_FUNDING, ownerAccount, firstAccount->getAddressAsString(), gasPrice );
    cout << "Transferred " << FIRST_WALLET_FUNDING << " wei to the first wallet" << endl;
    CHECK( getBalance( firstAccount->getAddressAsString() ) == FIRST_WALLET_FUNDING );
    CHECK( getBalance( ownerAccount->getAddressAsString() ) > FIRST_WALLET_FUNDING );
    // set owner account to null to make sure it is not in the test anymore
    // the owner account is used to fund th first
    ownerAccount = nullptr;
}


void SkaledFixture::setupTwoToTheNKeys( uint64_t _n ) {
    mutex testAccountsMutex;

    for ( uint64_t j = 0; j < _n; j++ ) {
        cout << "Creating test wallets. Iteration " << j
             << " wallets created: " << testAccounts.size() << endl;


        map< string, std::shared_ptr< SkaledAccount > > testAccountsCopy;

        {
            lock_guard< mutex > lock( testAccountsMutex );
            testAccountsCopy = testAccounts;
        }

        map< string, shared_ptr< SkaledAccount > > oldNewPairs;

        for ( auto&& testAccount : testAccountsCopy ) {
            auto newAccount = SkaledAccount::generate();
            string address = newAccount->getAddressAsString();
            CHECK( testAccounts.count( address ) == 0 );
            oldNewPairs.emplace( testAccount.first, newAccount );
            // add the new account to the map
            lock_guard< mutex > lock( testAccountsMutex );
            CHECK( testAccounts.count( newAccount->getAddressAsString() ) == 0 );
            testAccounts.emplace( newAccount->getAddressAsString(), newAccount );
        }

        auto begin = getCurrentTimeMs();


        auto gasPrice = getCurrentGasPrice();

        vector< shared_ptr< thread > > threads;

        for ( auto&& testAccount : testAccountsCopy ) {
            if ( useThreadsForTestKeyCreation ) {
                auto t = make_shared< thread >( [&]() {
                    auto oldAccount = testAccount.second;
                    auto newAccount = oldNewPairs.at( testAccount.first );
                    splitAccountInHalves( oldAccount, newAccount, gasPrice,
                        TransactionWait::DONT_WAIT_FOR_COMPLETION );
                } );
                threads.push_back( t );
            } else {
                auto oldAccount = testAccount.second;
                auto newAccount = oldNewPairs.at( testAccount.first );
                splitAccountInHalves(
                    oldAccount, newAccount, gasPrice, TransactionWait::DONT_WAIT_FOR_COMPLETION );
            }
        }


        cout << 1000.0 * testAccountsCopy.size() / ( getCurrentTimeMs() - begin )
             << " submission tps " << endl;

        if ( useThreadsForTestKeyCreation ) {
            for ( auto&& t : threads ) {
                t->join();
            }
        }

        for ( auto&& account : testAccountsCopy ) {
            waitForTransaction( account.second );
        };

        cout << 1000.0 * testAccountsCopy.size() / ( getCurrentTimeMs() - begin ) << " total tps "
             << endl;
    }


    for ( auto&& testAccount : testAccounts ) {
        testAccountsVector.push_back( testAccount.second );
    }

    cout << "Creating keys completed. Total test wallets created:" << testAccounts.size() << endl;
}


void SkaledFixture::doOneTinyTransfersIteration() {
    CHECK( threadsCountForTestTransactions <= testAccounts.size() );
    auto transactionsPerThreaad = testAccounts.size() / threadsCountForTestTransactions;

    auto begin = getCurrentTimeMs();

    auto gasPrice = getCurrentGasPrice();

    vector< shared_ptr< thread > > threads;

    CHECK( testAccountsVector.size() == testAccounts.size() );


    for ( uint64_t  accountNum = 0; accountNum < testAccountsVector.size(); accountNum++ ) {
        if ( threadsCountForTestTransactions > 1 ) {
            if ( accountNum % transactionsPerThreaad == 0 ) {
                uint64_t threadNumber = accountNum / transactionsPerThreaad;
                auto t = make_shared< thread >( [transactionsPerThreaad, threadNumber, gasPrice, this]() {
                    for ( uint64_t j = 0; j < transactionsPerThreaad; j++ ) {
                        auto account =
                            testAccountsVector.at( threadNumber * transactionsPerThreaad + j );
                        sendTinyTransfer(
                            account, gasPrice, TransactionWait::DONT_WAIT_FOR_COMPLETION );
                    }
                } );
                threads.push_back( t );
            }
        } else {
            auto oldAccount = testAccountsVector.at( accountNum );
            sendTinyTransfer( oldAccount, gasPrice, TransactionWait::DONT_WAIT_FOR_COMPLETION );
            CHECK( oldAccount->getLastSentNonce() >= 0 )
            CHECK( testAccountsVector.at( accountNum )->getLastSentNonce() >= 0 );
        }
    }



    if ( threadsCountForTestTransactions > 1 ) {
        CHECK( threads.size() == threadsCountForTestTransactions );
        for ( auto&& t : threads ) {
            t->join();
        }
    }


    cout << 1000.0 * testAccounts.size() / ( getCurrentTimeMs() - begin ) << " submission tps" << endl;



    for ( auto&& account : testAccounts ) {
        waitForTransaction( account.second );
    };

    cout << 1000.0 * testAccounts.size() / ( getCurrentTimeMs() - begin ) << " total tps" << endl;
}
void SkaledFixture::sendTinyTransfersForAllAccounts( uint64_t _iterations ) {
    cout << "Running tiny transfers for accounts :" << testAccounts.size() << endl;

    for ( uint64_t iteration = 0; iteration < _iterations; iteration++ ) {
        doOneTinyTransfersIteration();
    }
}


void SkaledFixture::readInsecurePrivateKeyFromHardhatConfig() {
    // get insecure test private key from hardhat config
    auto hardHatConfig = readFile( HARDHAT_CONFIG_FILE_NAME );

    std::istringstream stream( hardHatConfig );
    std::string line;
    string insecurePrivateKey;
    while ( std::getline( stream, line ) ) {
        if ( line.find( "INSECURE_PRIVATE_KEY" ) != std::string::npos ) {
            size_t start = line.find( '"' ) + 1;
            size_t end = line.rfind( '"' );
            insecurePrivateKey = line.substr( start, end - start );
            break;
        }
    }


    CHECK( !insecurePrivateKey.empty() );
    string ownerKeyStr = "0x" + insecurePrivateKey;
    Secret ownerSecret( ownerKeyStr );


    auto transactionCount = getTransactionCount( ownerAddressStr );
    ownerAccount = SkaledAccount::getInstance( ownerSecret, transactionCount );
    auto balance = getBalance( ownerAccount->getAddressAsString() );
    CHECK( balance > 0 );
}

uint64_t SkaledFixture::getCurrentTimeMs() {
    using namespace std::chrono;
    return duration_cast< milliseconds >( system_clock::now().time_since_epoch() ).count();
}


SkaledFixture::SkaledFixture( const std::string& _configPath ) {
    static atomic_bool isCurlInited( false );

    if ( isCurlInited.exchange( true ) ) {
        curl_global_init( CURL_GLOBAL_DEFAULT );
    }

    auto config = readFile( _configPath );

    Json::Value ret;
    Json::Reader().parse( config, ret );

    ownerAddressStr = ret["skaleConfig"]["sChain"]["schainOwner"].asString();
    boost::algorithm::to_lower( ownerAddressStr );
    ip = ret["skaleConfig"]["sChain"]["nodes"][0]["ip"].asString();
    CHECK( !ip.empty() )
    basePort = ret["skaleConfig"]["sChain"]["nodes"][0]["basePort"].asInt();
    CHECK( basePort > 0 )


    auto chainIdStr = ret["params"]["chainID"].asString();

    // trim 0x
    chainIdStr = chainIdStr.substr( 2 );
    chainId = std::stoull( chainIdStr, nullptr, 16 );

    auto coinbaseTest = dev::KeyPair(
        dev::Secret( "0x1c2cd4b70c2b8c6cd7144bbbfbd1e5c6eacb4a5efd9c86d0e29cbbec4e8483b9" ) );
    auto account3Test = dev::KeyPair(
        dev::Secret( "0x23ABDBD3C61B5330AF61EBE8BEF582F4E5CC08E554053A718BDCE7813B9DC1FC" ) );

    skaledEndpoint = "http://" + ip + ":" + std::to_string( basePort + 3 );

    cout << "Skaled Endpoint: " << skaledEndpoint << std::endl;


    u256 blockNumber = 0;

    cout << "Waiting for skaled ...";

    while ( blockNumber == 0 ) {
        try {
            blockNumber = jsToU256( rpcClient()->eth_blockNumber() );
            cout << "Got block number " << blockNumber << std::endl;
        } catch ( std::exception& e ) {
            cout << e.what() << std::endl;
            sleep( 1 );
        };
    }

    cout << "Starting test" << std::endl;


    readInsecurePrivateKeyFromHardhatConfig();
}

SkaledFixture::~SkaledFixture() {
    BOOST_TEST_MESSAGE( "Destructed SkaledFixture" );
}

u256 SkaledFixture::getTransactionCount( const string& _address ) {
    auto count = jsToU256( this->rpcClient()->eth_getTransactionCount( _address, "latest" ) );

    return count;
}

u256 SkaledFixture::getCurrentGasPrice() {
    auto gasPrice = jsToU256( rpcClient()->eth_gasPrice() );
    CHECK( gasPrice < 1000000 )
    return gasPrice;
}

u256 SkaledFixture::getBalance( const string& _address ) const {
    return jsToU256( rpcClient()->eth_getBalance( _address, "latest" ) );
}

u256 SkaledFixture::getBalance( const SkaledAccount& _account ) const {
    return getBalance( _account.getAddressAsString() );
}

void SkaledFixture::sendSingleTransfer( u256 _amount, std::shared_ptr< SkaledAccount > _from,
    const string& _to, const u256& _gasPrice, TransactionWait _wait ) {
    auto from = _from->getAddressAsString();
    auto accountNonce = _from->computeNonceForNextTransaction();
    u256 dstBalanceBefore;


    if ( this->verifyTransactions ) {
        CHECK( accountNonce == getTransactionCount( from ) );
        u256 srcBalanceBefore = getBalance( _from->getAddressAsString() );
        CHECK( srcBalanceBefore > 0 );
        if ( 21000 * _gasPrice + _amount > srcBalanceBefore ) {
            cout << "Not enough balance to send a transfer" << endl;
            cout << "Wallet:" << from << endl;
            cout << "Balance:" << srcBalanceBefore << endl;
            cout << "Transfer amount: " << _amount << endl;
            cout << "Gas price" << _gasPrice << endl;
            cout << "Missing amount:" << 21000 * _gasPrice + _amount - srcBalanceBefore << endl;
        }

        CHECK( 21000 * _gasPrice + _amount <= srcBalanceBefore );

        CHECK( srcBalanceBefore > 0 );
        dstBalanceBefore = getBalance( _to );
        CHECK( dstBalanceBefore == 0 );
    }

    Json::Value t;
    t["from"] = from;
    t["value"] = jsToDecimal( toJS( _amount ) );
    t["to"] = _to;
    TransactionSkeleton ts = toTransactionSkeleton( t );
    ts.nonce = accountNonce;
    ts.nonce = accountNonce;
    ts.gas = 90000;
    ts.gasPrice = _gasPrice;

    Transaction transaction( ts );  // always legacy, no prefix byte
    transaction.forceChainId( chainId );
    transaction.sign( _from->getKey() );
    CHECK( transaction.chainId() );
    auto result = dev::eth::toJson( transaction, transaction.toBytes() );

    CHECK( result["raw"] );
    CHECK( result["tx"] );


    try {
        auto payload = result["raw"].asString();
        // auto txHash = rpcClient()->eth_sendRawTransaction( payload );
        getThreadLocalCurlClient()->eth_sendRawTransaction( payload );
        // CHECK(!txHash.empty());
    } catch ( std::exception& e ) {
        cout << "EXCEPTION  " << transaction.from() << ": nonce: " << transaction.nonce() << endl;
        cout << e.what() << endl;
        throw e;
    }

    if ( _wait == TransactionWait::DONT_WAIT_FOR_COMPLETION ) {
        // dont wait for it to finish and return immediately
        return;
    }

    waitForTransaction( _from );

    if ( this->verifyTransactions ) {
        auto balanceAfter = getBalance( _to );
        CHECK( balanceAfter - dstBalanceBefore == _amount );
    }
}

void SkaledFixture::waitForTransaction( std::shared_ptr< SkaledAccount > _account ) {
    u256 transactionCount;

    auto transactionNonce = _account->getLastSentNonce();

    auto beginTime = getCurrentTimeMs();


    while ( ( transactionCount = getThreadLocalCurlClient()->eth_getTransactionCount(
                  _account->getAddressAsString() ) ) == transactionNonce ) {
        if ( this->verifyTransactions ) {
            CHECK( getTransactionCount( _account->getAddressAsString() ) == transactionCount )
        }

        if ( getCurrentTimeMs() - beginTime > transactionTimeoutMs ) {
            throw runtime_error( "Transaction timeout" );
        }
        // wait for a bit before checking again
        usleep( 300 * this->timeBetweenTransactionCompletionChecksMs );
    }

    // the count should now be one more than the last transaction nonce
    CHECK( transactionCount - transactionNonce == 1 );

    _account->notifyLastTransactionCompleted();
}

void SkaledFixture::splitAccountInHalves( std::shared_ptr< SkaledAccount > _from,
    std::shared_ptr< SkaledAccount > _to, u256& _gasPrice, TransactionWait _wait ) {
    auto balance = getThreadLocalCurlClient()->eth_getBalance( _from->getAddressAsString() );

    if ( this->verifyTransactions ) {
        CHECK( balance == getBalance( _from->getAddressAsString() ) )
    }

    CHECK( balance > 0 );
    auto fee = _gasPrice * 21000;
    CHECK( fee <= balance );
    CHECK( balance > 0 )
    auto amount = ( balance - fee ) / 2;

    sendSingleTransfer( amount, _from, _to->getAddressAsString(), _gasPrice, _wait );
}


void SkaledFixture::sendTinyTransfer(
    std::shared_ptr< SkaledAccount > _from, const u256& _gasPrice, TransactionWait _wait ) {
    auto fee = _gasPrice * 21000;

    if ( this->verifyTransactions ) {
        CHECK( fee <= getBalance( _from->getAddressAsString() ) )
    }

    sendSingleTransfer( 1, _from, _from->getAddressAsString(), _gasPrice, _wait );
}


unique_ptr< WebThreeStubClient > SkaledFixture::rpcClient() const {
    auto httpClient = new jsonrpc::HttpClient( skaledEndpoint );
    httpClient->SetTimeout( 10000 );
    auto rpcClient = unique_ptr< WebThreeStubClient >( new WebThreeStubClient( *httpClient ) );
    return rpcClient;
}

SkaledAccount::SkaledAccount( const Secret _key, const u256 _currentTransactionCountOnChain )
    : key( _key ), currentTransactionCountOnChain( _currentTransactionCountOnChain ) {}

const Secret& SkaledAccount::getKey() const {
    return key;
};

std::map< string, std::shared_ptr< SkaledAccount > > SkaledAccount::accounts;
