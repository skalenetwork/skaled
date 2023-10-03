// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#include "AlethStandardTrace.h"
#include "AlethExtVM.h"
#include "libevm/LegacyVM.h"

namespace dev {
namespace eth {
namespace {
bool logStorage( Instruction _inst ) {
    return _inst == Instruction::SSTORE || _inst == Instruction::SLOAD;
}

}  // namespace

void AlethStandardTrace::operator()( uint64_t, uint64_t PC, Instruction inst, bigint,
    bigint gasCost, bigint gas, VMFace const* _vm, ExtVMFace const* voidExt ) {

    AlethExtVM const& ext = dynamic_cast< AlethExtVM const& >( *voidExt );
    auto vm = dynamic_cast< LegacyVM const* >( _vm );

    Json::Value r( Json::objectValue );

    Json::Value stack( Json::arrayValue );
    if ( vm && !m_options.disableStack ) {
        // Try extracting information about the stack from the VM is supported.
        for ( auto const& i : vm->stack() )
            stack.append( toCompactHexPrefixed( i, 1 ) );
        r["stack"] = stack;
    }

    if ( m_lastInst.size() == voidExt->depth ) {
        // starting a new context
        assert( m_lastInst.size() == voidExt->depth );
        m_lastInst.push_back( inst );
    } else if ( m_lastInst.size() == voidExt->depth + 2 ) {
        m_lastInst.pop_back();
    } else if ( m_lastInst.size() == voidExt->depth + 1 ) {
        // continuing in previous context
        m_lastInst.back() = inst;
    } else {
        cwarn << "Tracing VM and more than one new/deleted stack frame between steps!";
        cwarn << "Attempting naive recovery...";
        m_lastInst.resize( voidExt->depth + 1 );
    }

    if ( vm ) {
        bytes const& memory = vm->memory();

        Json::Value memJson( Json::arrayValue );
        if ( m_options.enableMemory ) {
            for ( unsigned i = 0; i < memory.size(); i += 32 ) {
                bytesConstRef memRef( memory.data() + i, 32 );
                memJson.append( toHex( memRef ) );
            }
            r["memory"] = memJson;
        }
        r["memSize"] = static_cast< uint64_t >( memory.size() );
    }

    r["op"] = static_cast< uint8_t >( inst );
    r["opName"] = instructionInfo( inst ).name;
    r["pc"] = PC;
    r["gas"] =  static_cast< uint64_t >(gas) ;
    r["gasCost"] = static_cast< uint64_t >(gasCost);
    r["depth"] = voidExt->depth + 1;  // depth in standard trace is 1-based
    auto refund = ext.sub.refunds;
    if ( refund > 0) {
        r["refund"] = ext.sub.refunds;
    }
    if ( !m_options.disableStorage) {
        if (logStorage( inst ) ) {
            Json::Value storage( Json::objectValue );
            for ( auto const& i : ext.m_accessedStateValues )
                storage[toHex( i.first)] =
                    toHex( i.second);
            r["storage"] = storage;
            std::cerr << r.toStyledString();
        }
    }



    if ( m_outValue )
        m_outValue->append( r );
    else
        *m_outStream << m_fastWriter.write( r ) << std::flush;
}
}  // namespace eth
}  // namespace dev
