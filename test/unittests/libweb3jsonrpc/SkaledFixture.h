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

class SkaledAccount  {
    Secret key;
    u256 currentTransactionCountOnChain = 0;
    std::optional<u256> lastSentNonce = std::nullopt;

    mutable std::shared_mutex mutex;

public:

    static SkaledAccount generate() {
        Secret newKey = KeyPair::create().secret();
        return SkaledAccount(newKey, 0);
    }


    string getAddressAsString() const {
        return "0x" + KeyPair(key).address().hex();
    }

    // generate a copy constructor below
    SkaledAccount(const SkaledAccount& account) {
        key = account.key;
        currentTransactionCountOnChain = account.currentTransactionCountOnChain;
        lastSentNonce = account.lastSentNonce;
    }


    // generate a move constructor below

    SkaledAccount(SkaledAccount&& account) noexcept {
        key = std::move(account.key);
        currentTransactionCountOnChain = account.currentTransactionCountOnChain;
        lastSentNonce = std::move(account.lastSentNonce);
    }



    SkaledAccount& operator=(const SkaledAccount& account) {
        key = account.key;
        currentTransactionCountOnChain = account.currentTransactionCountOnChain;
        lastSentNonce = account.lastSentNonce;
        return *this;
    }

    const Secret &getKey() const;

    SkaledAccount(const Secret key, const u256 _currentTransactionCountOnChain);

    u256 getCurrentTransactionCountOnBlockchain()  const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return currentTransactionCountOnChain;
    }

    u256 getLastSentNonce() {
        std::shared_lock<std::shared_mutex> lock(mutex);
        if (!lastSentNonce.has_value()) {
            throw std::runtime_error("No transaction has been sent from this account");
        }
        return lastSentNonce.value();
    }

    u256 computeNonceForNextTransaction() {
        std::unique_lock<std::shared_mutex> lock(mutex);
        if ( lastSentNonce.has_value() && lastSentNonce != currentTransactionCountOnChain) {
            throw std::runtime_error("Previous transaction has not yet been confirmed");
        }
        lastSentNonce = currentTransactionCountOnChain;
        return lastSentNonce.value();
    }

    void notifyLastTransactionCompleted() {
        std::unique_lock<std::shared_mutex> lock(mutex);

        if (! lastSentNonce.has_value()) {
            throw std::runtime_error("No pending transaction for this account");
        }

        CHECK(lastSentNonce == currentTransactionCountOnChain);

        currentTransactionCountOnChain++;

        lastSentNonce = std::nullopt;
    }


};


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

    void readInsecurePrivateKeyFromHardhatConfig();

    static uint64_t getCurrentTimeMs();


    SkaledFixture(const std::string &_configPath);

    ~SkaledFixture() override;

    u256 getTransactionCount(const string &_address);

    u256 getCurrentGasPrice();

    u256 getBalance(const string& _address) const;


    u256 getBalance(const SkaledAccount &_account) const ;

    uint64_t sendSingleTransfer(u256 _amount, SkaledAccount &_from, const string& _to, u256 &_gasPrice, bool _noWait = false);

    u256 splitAccountInHalves(SkaledAccount _from, SkaledAccount _to, u256& _gasPrice, bool _noWait = false);


    unique_ptr<WebThreeStubClient> rpcClient() const;

    string skaledEndpoint;
    string ownerAddressStr;
    string ip;
    uint64_t basePort;
    uint64_t chainId;
    std::shared_ptr<SkaledAccount> ownerAccount;
    // map of test key addresses to secret keys
    map<string, SkaledAccount> testAccounts;
    const string HARDHAT_CONFIG_FILE_NAME = "../../test/historicstate/hardhat/hardhat.config.js";
    uint64_t transactionTimeoutMs = 60000;
    bool verifyTransactions = false;

    void waitForTransaction(const string& _address, const u256& _transactionNonce);

    int timeBetweenTransactionCompletionChecksMs = 1000;
};


#endif //SKALE_SKALEDFIXTURE_H
