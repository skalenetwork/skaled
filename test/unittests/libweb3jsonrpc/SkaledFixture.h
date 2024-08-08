//
// Created by kladko on 08-08-2024.
//

#ifndef SKALE_SKALEDFIXTURE_H
#define SKALE_SKALEDFIXTURE_H


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

class CurlHandle {
public:
    CURL *curl;
    std::string readBuffer;
    struct curl_slist *headers = nullptr;


    CurlHandle(SkaledFixture &_fixture);

    void setRequest(const string &_json_rpc_request);

    ~CurlHandle();
};


class SkaledFixture : public TestOutputHelperFixture {

    static string readFile(const std::string &_path);

    static thread_local ptr<CurlHandle> curlHandle;

public:

    ptr<CurlHandle> getThreadLocalCurlHandle();

    void setupFirstKey();

    void setupTwoToTheNKeys(uint64_t _n);

    string getAddressAsString(Secret &_secret);

    string getAddressAsString(Address &_address);

    void readInsecurePrivateKeyFromHardhatConfig();

    uint64_t getCurrentTimeMs();


    SkaledFixture(const std::string &_configPath);

    ~SkaledFixture() override;

    u256 getTransactionCount(string &_address);

    u256 getCurrentGasPrice();

    u256 getBalance(string _address);

    bool sendSingleTransfer(u256 _amount, Secret &_from, Address _to);

    void splitAccountInHalves(Secret _fromKey, Secret _toKey);


    unique_ptr<WebThreeStubClient> rpcClient();

    string skaledEndpoint;
    string ownerAddressStr;
    string ip;
    uint64_t basePort;
    uint64_t chainId;
    Secret ownerKey;
    map<string, Secret> testKeys;
    const string HARDHAT_CONFIG_FILE_NAME = "../../test/historicstate/hardhat/hardhat.config.js";
};


#endif //SKALE_SKALEDFIXTURE_H
