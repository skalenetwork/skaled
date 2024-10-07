/*
    Copyright (C) 2018 SKALE Labs

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
/** @file SkaleStats.h
 * @authors:
 *   Sergiy Lavrynenko <sergiy@skalelabs.com>
 * @date 2019
 */

#pragma once

//#include <nlohmann/json.hpp>
#include <json.hpp>

namespace dev {
namespace rpc {

struct ISkaleStatsProvider;
struct ISkaleStatsConsumer;


struct ISkaleStatsProvider {
    ISkaleStatsProvider() {}
    virtual ~ISkaleStatsProvider() {}
    virtual ISkaleStatsConsumer* getConsumer() = 0;
    virtual void setConsumer( ISkaleStatsConsumer* pConsumer ) = 0;
    virtual nlohmann::json provideSkaleStats() = 0;
};



struct ISkaleStatsConsumer {
    ISkaleStatsConsumer() {}
    virtual ~ISkaleStatsConsumer() {}
    virtual ISkaleStatsProvider* getProvider() = 0;
    virtual void setProvider( ISkaleStatsProvider* pProvider ) = 0;
    virtual nlohmann::json consumeSkaleStats() = 0;
};

class SkaleStatsProviderImpl : public ISkaleStatsProvider {
    ISkaleStatsConsumer* m_pConsumer;

public:
    SkaleStatsProviderImpl() : m_pConsumer( nullptr ) {}
    ~SkaleStatsProviderImpl() override { setConsumer( nullptr ); }
    ISkaleStatsConsumer* getConsumer() override { return m_pConsumer; }
    void setConsumer( ISkaleStatsConsumer* pConsumer ) override {
        ISkaleStatsConsumer* pPrev = m_pConsumer;
        if ( pPrev == pConsumer )
            return;
        m_pConsumer = pConsumer;
        if ( pPrev && pPrev->getProvider() == this )
            pPrev->setProvider( nullptr );
    }
};


class SkaleStatsConsumerImpl : public ISkaleStatsConsumer {
    ISkaleStatsProvider* m_pProvider;

public:
    SkaleStatsConsumerImpl() : m_pProvider( nullptr ) {}
    ~SkaleStatsConsumerImpl() override { setProvider( nullptr ); }
    ISkaleStatsProvider* getProvider() override { return m_pProvider; }
    void setProvider( ISkaleStatsProvider* pProvider ) override {
        ISkaleStatsProvider* pPrev = m_pProvider;
        if ( pPrev == pProvider )
            return;
        m_pProvider = pProvider;
        if ( pPrev && pPrev->getConsumer() == this )
            pPrev->setConsumer( nullptr );
    }
    virtual nlohmann::json consumeSkaleStats() override {
        ISkaleStatsProvider* pProvider = getProvider();
        if ( !pProvider )
            return nlohmann::json::object();
        return pProvider->provideSkaleStats();
    }
};

};  // namespace rpc
};  // namespace dev
