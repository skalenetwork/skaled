/*
Copyright (C) 2023-present, SKALE Labs

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


#pragma once

#include "libdevcore/Common.h"
#include "libevm/Instruction.h"


//  we limit the  memory and storage entries returned to avoid
// denial of service attack.
// see here https://banteg.mirror.xyz/3dbuIlaHh30IPITWzfT1MFfSg6fxSssMqJ7TcjaWecM

constexpr std::uint64_t MAX_MEMORY_VALUES_RETURNED = 1024;
constexpr std::uint64_t MAX_STORAGE_VALUES_RETURNED = 1024;
constexpr std::int64_t MAX_TRACE_DEPTH = 256;


#define STATE_CHECK( _EXPRESSION_ )                                                  \
    if ( !( _EXPRESSION_ ) ) {                                                       \
        auto __msg__ = std::string( "State check failed::" ) + #_EXPRESSION_ + " " + \
                       std::string( __FILE__ ) + ":" + to_string( __LINE__ );        \
        throw dev::eth::VMTracingError( __msg__ );                                   \
    }


namespace dev::eth {

using std::string, std::shared_ptr, std::make_shared, std::to_string, std::set, std::map,
    std::vector;

struct LogRecord {
    LogRecord( const std::vector< std::uint8_t >& _data, const std::vector< dev::u256 >& _topics )
        : m_data( _data ), m_topics( _topics ) {}

    const std::vector< std::uint8_t > m_data;
    const std::vector< dev::u256 > m_topics;
};

struct OpExecutionRecord {
    OpExecutionRecord( int64_t _depth, Instruction _op, uint64_t _gasRemaining, uint64_t _opGas,
        uint64_t _pc, int64_t _refund, string _opName )
        : m_depth( _depth ),
          m_op( _op ),
          m_gasRemaining( _gasRemaining ),
          m_opGas( _opGas ),
          m_pc( _pc ),
          m_refund( _refund ),
          m_opName( _opName ) {}

    std::int64_t m_depth;
    Instruction m_op;
    std::uint64_t m_gasRemaining;
    std::uint64_t m_opGas;
    std::uint64_t m_pc;
    std::int64_t m_refund;
    std::string m_opName;
    std::shared_ptr< std::map< dev::u256, dev::u256 > > m_accessedStorageValues = nullptr;
    std::shared_ptr< u256s > m_stack = nullptr;
    std::shared_ptr< bytes > m_memory = nullptr;
};

}  // namespace dev::eth
