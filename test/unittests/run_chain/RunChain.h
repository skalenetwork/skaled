/*
    Modifications Copyright (C) 2018-2019 SKALE Labs

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
 * @author Sergiy <l_sergiy@skalelabs.com>
 * @date 2019
 */

#if ( !defined __RUN_CHAIN_H )
#define __RUN_CHAIN_H 1

#include <string>

namespace test {

class run_chain {
public:
    run_chain();
    run_chain( const run_chain& ) = delete;
    virtual ~run_chain();
    run_chain& operator=( const run_chain& ) = delete;

protected:
    enum class NodeMode { PeerServer, Full };

    NodeMode nodeMode = NodeMode::Full;
    bool is_ipc = false;
    int nExplicitPortHTTP4 = -1;
    int nExplicitPortHTTP6 = -1;
    int nExplicitPortHTTPS4 = -1;
    int nExplicitPortHTTPS6 = -1;
    int nExplicitPortWS4 = -1;
    int nExplicitPortWS6 = -1;
    int nExplicitPortWSS4 = -1;
    int nExplicitPortWSS6 = -1;
    bool bTraceJsonRpcCalls = true;
    bool bEnabledShutdownViaWeb3 = true;

    std::string clientVersion() const;
    virtual bool shouldExit() const;
    bool init();
};  /// class run_chain

};  // namespace test

#endif  // (! defined __RUN_CHAIN_H)
