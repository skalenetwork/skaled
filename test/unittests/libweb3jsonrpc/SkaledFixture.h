//
// Created by kladko on 08-08-2024.
//

#ifndef SKALE_SKALEDFIXTURE_H
#define SKALE_SKALEDFIXTURE_H

#include <test/tools/libtesteth/TestHelper.h>
#include <test/tools/libtesteth/TestOutputHelper.h>

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace dev::test;

#define CHECK(__EXPRESSION__)                                             \
    if ( !( __EXPRESSION__ ) ) {                                                  \
        auto __msg__ = string( "Check failed::" ) + #__EXPRESSION__ + " " + \
                       string( __FILE__ ) + ":" + to_string( __LINE__ );        \
        throw std::runtime_error( __msg__);                 \
    }

// Callback function to handle data received from the server
size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp);


class SkaledFixture;

class CurlClient {


    CURL *curl;
    std::string readBuffer;
    struct curl_slist *headers = nullptr;
    static std::atomic<uint64_t> totalCallsCount;

public:
    static uint64_t getTotalCallsCount();

    CurlClient(SkaledFixture &_fixture);

    void setRequest(const string &_json_rpc_request);

    uint64_t doRequestResponse();

    Json::Value parseResponse();

    void eth_sendRawTransaction(const std::string &_rawTransactionHex);

    ~CurlClient();
};


class SkaledFixture : public TestOutputHelperFixture {

    static string readFile(const std::string &_path);

    static thread_local std::shared_ptr<CurlClient> curlClient;

public:

    std::shared_ptr<CurlClient> getThreadLocalCurlClient();

    void setupFirstKey();

    void setupTwoToTheNKeys(uint64_t _n);

    string getAddressAsString(Secret &_secret);

    string getAddressAsString(Address &_address);

    void readInsecurePrivateKeyFromHardhatConfig();

    uint64_t getCurrentTimeMs();


    SkaledFixture(const std::string &_configPath);

    ~SkaledFixture() override;

    u256 getTransactionCount(const string &_address);

    u256 getCurrentGasPrice();

    u256 getBalance(const string& _address);

    uint64_t sendSingleTransfer(u256 _amount, Secret &_from, Address _to, bool _noWait = false);

    u256 splitAccountInHalves(Secret _fromKey, Secret _toKey, bool _noWait = false);


    unique_ptr<WebThreeStubClient> rpcClient();

    string skaledEndpoint;
    string ownerAddressStr;
    string ip;
    uint64_t basePort;
    uint64_t chainId;
    Secret ownerKey;
    // map of test key addresses to secret keys
    map<string, Secret> testKeys;
    const string HARDHAT_CONFIG_FILE_NAME = "../../test/historicstate/hardhat/hardhat.config.js";
    uint64_t transactionTimeoutMs = 60000;
    bool verifyTransactions = false;

    void waitForTransaction(const string& _address, const u256& _transactionNonce);

    int timeBetweenTransactionCompletionChecksMs = 1000;
};


#endif //SKALE_SKALEDFIXTURE_H
