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

#include "libweb3jsonrpc/SkaleFace.h"

#include <libweb3jsonrpc/Debug.h>
#include <test/tools/libtesteth/TestHelper.h>
#include <test/tools/libtesteth/TestOutputHelper.h>
#include <boost/test/unit_test.hpp>
#include <libconsensus/SkaleCommon.h>
#include <libconsensus/oracle/OracleRequestSpec.h>

#include <cstdlib>
#include "SkaledFixture.h"


// Callback function to handle data received from the server
size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string *) userp)->append((char *) contents, size * nmemb);
    return size * nmemb;
}


CurlClient::CurlClient::CurlClient(SkaledFixture &_fixture) {
    curl = curl_easy_init();
    CHECK(curl);
    curl_easy_setopt(curl, CURLOPT_URL, _fixture.skaledEndpoint.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    // Set up callback to capture response
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    // Set HTTP headers
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
}

void CurlClient::setRequest(const string &_json_rpc_request) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, _json_rpc_request.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, _json_rpc_request.size());
}

uint64_t CurlClient::doRequestResponse() {
    auto res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        throw std::runtime_error(string("curl_easy_perform() failed")
                                 + curl_easy_strerror(res));
    }
    return ++totalCallsCount;
}

std::atomic<uint64_t> CurlClient::totalCallsCount = 0;

Json::Value CurlClient::parseResponse() {
    Json::CharReaderBuilder readerBuilder;
    Json::Value jsonData;
    std::string errs;
    std::istringstream s(readBuffer);
    if (!Json::parseFromStream(readerBuilder, s, &jsonData, &errs)) {
        throw std::runtime_error("Failed to parse JSON response: " + errs);
    }
    return jsonData;
}

CurlClient::~CurlClient() {
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

uint64_t CurlClient::getTotalCallsCount() {
    return totalCallsCount;
}

void CurlClient::eth_sendRawTransaction(const std::string &_rawTransactionHex) {
    std::string jsonPayload =
            R"({"jsonrpc":"2.0","method":"eth_sendRawTransaction","params":[")" + _rawTransactionHex + R"("],"id":1})";
    setRequest(jsonPayload);
    doRequestResponse();
}


string SkaledFixture::readFile(const std::string &_path) {

    CHECK(boost::filesystem::exists(_path));

    boost::filesystem::ifstream stream(_path, std::ios::in | std::ios::binary);

    CHECK(stream.is_open());

    string contents((std::istreambuf_iterator<char>(stream)),
                    std::istreambuf_iterator<char>());

    return contents;
}

thread_local ptr<CurlClient> SkaledFixture::curlClient;

ptr<CurlClient> SkaledFixture::getThreadLocalCurlClient() {
    if (!curlClient) {
        curlClient = make_shared<CurlClient>(*this);
    }
    return curlClient;
};



void SkaledFixture::setupFirstKey() {
    auto firstKey = Secret::random();
    testKeys[getAddressAsString(firstKey)] = firstKey;
    string amount = "100000000000000000000000000000000000000000000000000000000";
    u256 num(amount);
    sendSingleTransfer(num, ownerKey, KeyPair(firstKey).address());
}

void SkaledFixture::setupTwoToTheNKeys(uint64_t _n) {
    setupFirstKey();


    mutex testKeysMutex;

    for (uint64_t j = 0; j < _n; j++) {

        cerr << "Iteration " << j << "Number of keys:" << testKeys.size() << endl;


        map<string, Secret> testKeysCopy;

        {
            lock_guard<mutex> lock(testKeysMutex);
            testKeysCopy = testKeys;
        }

        map<string, Secret> keyPairs;

        for (auto &&testKey: testKeysCopy) {
            lock_guard<mutex> lock(testKeysMutex);
            Secret newKey = KeyPair::create().secret();
            string address = getAddressAsString(newKey);
            CHECK(testKeys.count(address) == 0);
            testKeys[address] = newKey;
            keyPairs[address] = newKey;
            keyPairs[getAddressAsString(testKey.second)] = newKey;
        }

        vector<shared_ptr<thread>> threads;
        for (auto &&testKey: testKeysCopy) {
            Secret newKey;
            auto t = make_shared<thread>([&]() {
                splitAccountInHalves(testKey.second, keyPairs[testKey.first]);
            });
            threads.push_back(t);
        }
        for (auto &&t: threads) {
            t->join();
        }
    }
}

string SkaledFixture::getAddressAsString(Secret &_secret) {
    return "0x" + KeyPair(_secret).address().hex();
}

string SkaledFixture::getAddressAsString(Address &_address) {
    return "0x" + _address.hex();
}

void SkaledFixture::readInsecurePrivateKeyFromHardhatConfig() {
    // get insecure test private key from hardhat config
    auto hardHatConfig = readFile(HARDHAT_CONFIG_FILE_NAME);

    std::istringstream stream(hardHatConfig);
    std::string line;
    string insecurePrivateKey;
    while (std::getline(stream, line)) {
        if (line.find("INSECURE_PRIVATE_KEY") != std::string::npos) {
            size_t start = line.find('"') + 1;
            size_t end = line.rfind('"');
            insecurePrivateKey = line.substr(start, end - start);
            break;
        }
    }


    CHECK(!insecurePrivateKey.empty());
    string ownerKeyStr = "0x" + insecurePrivateKey;
    Secret ownerSecret(ownerKeyStr);
    ownerKey = ownerSecret;

    auto balance = getBalance(getAddressAsString(ownerKey));
    CHECK(balance > 0);

}

uint64_t SkaledFixture::getCurrentTimeMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}


SkaledFixture::SkaledFixture(const std::string &_configPath) {

    static atomic_bool isCurlInited(false);

    if (isCurlInited.exchange(true)) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    auto config = readFile(_configPath);

    Json::Value ret;
    Json::Reader().parse(config, ret);

    ownerAddressStr = ret["skaleConfig"]["sChain"]["schainOwner"].asString();
    ip = ret["skaleConfig"]["sChain"]["nodes"][0]["ip"].asString();
    CHECK(!ip.empty())
    basePort = ret["skaleConfig"]["sChain"]["nodes"][0]["basePort"].asInt();
    CHECK(basePort > 0)


    auto chainIdStr = ret["params"]["chainID"].asString();

    // trim 0x
    chainIdStr = chainIdStr.substr(2);
    chainId = std::stoull(chainIdStr, nullptr, 16);

    auto coinbaseTest = dev::KeyPair(
            dev::Secret("0x1c2cd4b70c2b8c6cd7144bbbfbd1e5c6eacb4a5efd9c86d0e29cbbec4e8483b9"));
    auto account3Test = dev::KeyPair(
            dev::Secret("0x23ABDBD3C61B5330AF61EBE8BEF582F4E5CC08E554053A718BDCE7813B9DC1FC"));

    skaledEndpoint = "http://" + ip + ":" + std::to_string(basePort + 3);

    cout << "Skaled Endpoint: " << skaledEndpoint << std::endl;


    u256 blockNumber = 0;

    cout << "Waiting for skaled ...";

    while (blockNumber == 0) {
        try {
            blockNumber = jsToU256(rpcClient()->eth_blockNumber());
            cout << "Got block number " << blockNumber << std::endl;
        } catch (std::exception &e) {
            cerr << e.what() << std::endl;
            sleep(1);
        };
    }

    cout << "Starting test" << std::endl;


    readInsecurePrivateKeyFromHardhatConfig();

}

SkaledFixture::~SkaledFixture() {
    BOOST_TEST_MESSAGE("Destructed SkaledFixture");
}

u256 SkaledFixture::getTransactionCount(string &_address) {
    return jsToU256(this->rpcClient()->eth_getTransactionCount(toJS(_address), "latest"));
}

u256 SkaledFixture::getCurrentGasPrice() {
    return jsToU256(rpcClient()->eth_gasPrice());
}

u256 SkaledFixture::getBalance(string _address) {
    return jsToU256(rpcClient()->eth_getBalance(_address, "latest"));
}

bool SkaledFixture::sendSingleTransfer(u256 _amount, Secret &_from, Address _to) {
    auto addressStr = "0x" + KeyPair(_from).address().hex();
    auto accountNonce = getTransactionCount(addressStr);
    u256 gasPrice = getCurrentGasPrice();
    u256 srcBalanceBefore = getBalance(getAddressAsString(_from));
    CHECK(srcBalanceBefore > 0);
    //u256 dstBalanceBefore = getBalance(getAddressAsString(_to));
    Json::Value t;
    t["from"] = toJS(KeyPair(_from).address());
    t["value"] = jsToDecimal(toJS(_amount));
    t["to"] = toJS(_to);
    TransactionSkeleton ts = toTransactionSkeleton(t);
    ts.nonce = accountNonce;
    ts.gas = 90000;
    ts.gasPrice = gasPrice;

    Transaction transaction(ts);  // always legacy, no prefix byte
    transaction.forceChainId(chainId);
    transaction.sign(_from);
    CHECK(transaction.chainId());
    auto result = dev::eth::toJson(transaction, transaction.toBytes());

    CHECK(result["raw"]);
    CHECK(result["tx"]);

    auto beginTime = getCurrentTimeMs();


    try {
        auto payload = result["raw"].asString();
        auto txHash = rpcClient()->eth_sendRawTransaction(payload);
        //getThreadLocalCurlClient()->eth_sendRawTransaction(payload);
        //CHECK(!txHash.empty());
    } catch (std::exception &e) {
        cerr << "EXCEPTION  " << transaction.from() << ": nonce: " << transaction.nonce() << endl;
        cerr << e.what() << endl;
        throw e;
    }


    u256 newAccountNonce;
    uint64_t completionTime;

    do {
        newAccountNonce = getTransactionCount(addressStr);
        sleep(1);
        completionTime = getCurrentTimeMs();
        if (completionTime - beginTime > 60000) {
            return false;
        }
    } while (newAccountNonce == accountNonce);

    CHECK(newAccountNonce - accountNonce == 1);
    //auto balanceAfter = getBalance(getAddressAsString(_to));
    //CHECK(balanceAfter - dstBalanceBefore == _amount);

    return true;
}

void SkaledFixture::splitAccountInHalves(Secret _fromKey, Secret _toKey) {
    auto dstAddress = KeyPair(_toKey).address();
    auto balance = getBalance("0x" + KeyPair(_fromKey).address().hex());
    CHECK(balance > 0);
    auto fee = getCurrentGasPrice() * 21000;
    CHECK(fee <= balance);
    CHECK(balance > 0)
    auto amount = (balance - fee) / 2;

    if (!sendSingleTransfer(amount, _fromKey, dstAddress)) {
      throw std::runtime_error("Transaction timeout");
    }

    CHECK(getBalance("0x" + dstAddress.hex()) > 0);
}


unique_ptr<WebThreeStubClient> SkaledFixture::rpcClient() {
    auto httpClient = new jsonrpc::HttpClient(skaledEndpoint);
    httpClient->SetTimeout(10000);
    auto rpcClient = unique_ptr<WebThreeStubClient>(new WebThreeStubClient(*httpClient));
    return rpcClient;
}

