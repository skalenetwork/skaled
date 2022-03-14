#if ( !defined __SKALE_NETWORK_BROWSER_H )
#define __SKALE_NETWORK_BROWSER_H 1

#include <string>
#include <vector>

#include <json.hpp>

#include <libdevcore/Common.h>
#include <libdevcore/CommonJS.h>

#include <skutils/url.h>

namespace skale {
namespace network {
namespace browser {

extern bool g_bVerboseLogging;
extern size_t g_nRefreshIntervalInSeconds;

struct node_t {
    dev::u256 node_id;
    std::string name, ip, publicIP;
    int nPort = -1;  // base port
    // downloaded via other calls:
    std::string domainName;
    bool isMaintenance = false;
    int schain_base_port = -1;
    // computed ports:
    int httpRpcPort = -1, httpsRpcPort = -1, wsRpcPort = -1, wssRpcPort = -1, infoHttpRpcPort = -1;
    // computed endpoints:
    skutils::url http_endpoint_ip, http_endpoint_domain, https_endpoint_ip, https_endpoint_domain,
        ws_endpoint_ip, ws_endpoint_domain, wss_endpoint_ip, wss_endpoint_domain,
        info_http_endpoint_ip, info_http_endpoint_domain;
};  // struct node_t

typedef std::vector< node_t > vec_nodes_t;

struct s_chain_t {
    std::string name;
    dev::u256 owner;          // address
    size_t indexInOwnerList;  // uint
    size_t partOfNode;        // uint8
    size_t lifetime;          // uint
    size_t startDate;         // uint
    dev::u256 startBlock;     // uint
    dev::u256 deposit;        // uint
    size_t index;             // uint64
    size_t generation;        // uint
    dev::u256 originator;     // address
    vec_nodes_t vecNodes;
    // computed:
    dev::h256 schain_id;  // keccak256(name)
    dev::u256 chainId;    // part of schain_id
};                        // struct s_chain_t

typedef std::vector< s_chain_t > vec_s_chains_t;

dev::u256 get_schains_count( const skutils::url& u, const dev::u256& addressFrom );

s_chain_t load_schain( const skutils::url& u, const dev::u256& addressFrom,
    const dev::u256& idxSChain, const dev::u256& cntSChains,
    const dev::u256& addressSchainsInternal, const dev::u256& addressNodes );

vec_s_chains_t load_schains( const skutils::url& u, const dev::u256& addressFrom,
    const dev::u256& addressSchainsInternal, const dev::u256& addressNodes );

nlohmann::json to_json( const node_t& node );

nlohmann::json to_json( const s_chain_t& s_chain );

nlohmann::json to_json( const vec_s_chains_t& vec );

vec_s_chains_t refreshing_cached();

bool refreshing_start( const std::string& configPath );

void refreshing_stop();

vec_s_chains_t refreshing_do_now();

skutils::url refreshing_pick_s_chain_url( const std::string& strSChainName );

}  // namespace browser
}  // namespace network
}  // namespace skale

#endif  /// (!defined __SKALE_NETWORK_BROWSER_H)
