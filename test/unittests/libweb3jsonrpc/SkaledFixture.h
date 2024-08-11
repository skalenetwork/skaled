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

    static std::map<string, std::shared_ptr<SkaledAccount>> accounts;

    // Grant std::make_shared access to the private constructor
    friend std::shared_ptr<SkaledAccount> std::make_shared<SkaledAccount>();

public:

    static shared_ptr<SkaledAccount> getInstance(const Secret key, const u256 _currentTransactionCountOnChain) {
        static std::mutex addressesMutex;
        std::lock_guard<std::mutex> lock(addressesMutex);
        auto account =  std::shared_ptr<SkaledAccount>(new SkaledAccount(key, _currentTransactionCountOnChain));
        if (!accounts.try_emplace(account->getAddressAsString()).second) {
            // another object was created, so return the existing one
            return accounts[account->getAddressAsString()] = account;
        } else {
            //return the newly created one
            return account;
        };
    }


    static shared_ptr<SkaledAccount> generate() {
        static std::mutex addressesMutex;
        Secret newKey = KeyPair::create().secret();
        return getInstance(newKey, 0);
    }


    string getAddressAsString() const {
        return "0x" + KeyPair(key).address().hex();
    }


    const Secret &getKey() const;

    u256 getCurrentTransactionCountOnBlockchain()  const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return currentTransactionCountOnChain;
    }

    u256 getLastSentNonce() const {
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
        cerr << "Started " +  this->getAddressAsString() + "\n";
        return lastSentNonce.value();
    }

    void notifyLastTransactionCompleted() {
        std::unique_lock<std::shared_mutex> lock(mutex);

        cerr << "Completed " +  this->getAddressAsString() + "\n";

        if (! lastSentNonce.has_value()) {
            throw std::runtime_error("No pending transaction for this account");
        }

        CHECK(lastSentNonce == currentTransactionCountOnChain);

        currentTransactionCountOnChain++;

        lastSentNonce = std::nullopt;


    }

private:

    SkaledAccount(const Secret key, const u256 _currentTransactionCountOnChain);



    SkaledAccount& operator=(const SkaledAccount& ) {
        // these objects should exist in a single copy per account
        // thats why we use a shared pointer
        throw std::runtime_error("Copying SkaledAccount objects is not allowed");
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

    uint64_t sendSingleTransfer(u256 _amount, std::shared_ptr<SkaledAccount> _from, const string& _to, u256 &_gasPrice, bool _noWait = false);

    u256 splitAccountInHalves(std::shared_ptr<SkaledAccount> _from, std::shared_ptr<SkaledAccount> _to, u256& _gasPrice, bool _noWait = false);


    unique_ptr<WebThreeStubClient> rpcClient() const;

    string skaledEndpoint;
    string ownerAddressStr;
    string ip;
    uint64_t basePort;
    uint64_t chainId;
    std::shared_ptr<SkaledAccount> ownerAccount;
    // map of test key addresses to secret keys
    map<string, std::shared_ptr<SkaledAccount>> testAccounts;
    const string HARDHAT_CONFIG_FILE_NAME = "../../test/historicstate/hardhat/hardhat.config.js";
    uint64_t transactionTimeoutMs = 60000;
    bool verifyTransactions = false;

    void waitForTransaction(const string& _address, const u256& _transactionNonce);

    int timeBetweenTransactionCompletionChecksMs = 1000;
};


#endif //SKALE_SKALEDFIXTURE_H
