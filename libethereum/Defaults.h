/*
    Modifications Copyright (C) 2018-2019 SKALE Labs

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
/** @file Defaults.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#pragma once

#include <memory>

#include <boost/filesystem/path.hpp>

#include <libdevcore/Common.h>

namespace dev {
namespace eth {

struct Defaults {
    friend class BlockChain;

public:
    Defaults();

    static Defaults* get() {
        if ( !s_this )
            s_this = std::make_unique< Defaults >();
        return s_this.get();
    }
    static void setDBPath( boost::filesystem::path const& _dbPath ) { get()->m_dbPath = _dbPath; }
    static boost::filesystem::path const& dbPath() { return get()->m_dbPath; }

private:
    boost::filesystem::path m_dbPath;

    static std::unique_ptr< Defaults > s_this;
};

}  // namespace eth
}  // namespace dev
