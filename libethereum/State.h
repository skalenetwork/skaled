/*
    Modifications Copyright (C) 2018 SKALE Labs

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

#pragma once

#include <libskale/State.h>

namespace dev {
namespace eth {
using StateClass = skale::State;

std::ostream& operator<<( std::ostream& _out, StateClass const& _s );

// Import-specific errinfos
using errinfo_uncleIndex = boost::error_info< struct tag_uncleIndex, unsigned >;
using errinfo_currentNumber = boost::error_info< struct tag_currentNumber, u256 >;
using errinfo_uncleNumber = boost::error_info< struct tag_uncleNumber, u256 >;
using errinfo_unclesExcluded = boost::error_info< struct tag_unclesExcluded, h256Hash >;
using errinfo_block = boost::error_info< struct tag_block, bytes >;
using errinfo_now = boost::error_info< struct tag_now, unsigned >;

}  // namespace eth
}  // namespace dev
