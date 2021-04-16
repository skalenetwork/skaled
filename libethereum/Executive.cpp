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

#include "Executive.h"

#include <numeric>

#include <boost/timer.hpp>

#include <json/json.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/microprofile.h>
#include <libethcore/CommonJS.h>
#include <libevm/LegacyVM.h>
#include <libevm/VMFactory.h>

#include "Block.h"
#include "BlockChain.h"
#include "ExtVM.h"
#include "Interface.h"

using namespace std;
using namespace dev;
using namespace dev::eth;
using skale::State;

namespace {
std::string dumpStackAndMemory( LegacyVM const& _vm ) {
    ostringstream o;
    o << "\n    STACK\n";
    for ( auto i : _vm.stack() )
        o << ( h256 ) i << "\n";
    o << "    MEMORY\n"
      << ( ( _vm.memory().size() > 1000 ) ? " mem size greater than 1000 bytes " :
                                            memDump( _vm.memory() ) );
    return o.str();
}

std::string dumpStorage( ExtVM const& _ext ) {
    ostringstream o;
    o << "    STORAGE\n";
    for ( auto const& i : _ext.state().storage( _ext.myAddress ) )
        o << showbase << hex << i.second.first << ": " << i.second.second << "\n";
    return o.str();
}


}  // namespace

StandardTrace::StandardTrace() : m_trace( Json::arrayValue ) {}

bool changesMemory( Instruction _inst ) {
    return _inst == Instruction::MSTORE || _inst == Instruction::MSTORE8 ||
           _inst == Instruction::MLOAD || _inst == Instruction::CREATE ||
           _inst == Instruction::CALL || _inst == Instruction::CALLCODE ||
           _inst == Instruction::SHA3 || _inst == Instruction::CALLDATACOPY ||
           _inst == Instruction::CODECOPY || _inst == Instruction::EXTCODECOPY ||
           _inst == Instruction::DELEGATECALL;
}

bool changesStorage( Instruction _inst ) {
    return _inst == Instruction::SSTORE;
}

void StandardTrace::operator()( uint64_t _steps, uint64_t PC, Instruction inst, bigint newMemSize,
    bigint gasCost, bigint gas, VMFace const* _vm, ExtVMFace const* voidExt ) {
    ( void ) _steps;

    ExtVM const& ext = dynamic_cast< ExtVM const& >( *voidExt );
    auto vm = dynamic_cast< LegacyVM const* >( _vm );

    Json::Value r( Json::objectValue );

    Json::Value stack( Json::arrayValue );
    if ( vm && !m_options.disableStack ) {
        // Try extracting information about the stack from the VM is supported.
        for ( auto const& i : vm->stack() ) {
            Json::Value tmp = toCompactHexPrefixed( i, 1 );
            stack.append( tmp );
        }
        r["stack"] = stack;
    }

    bool newContext = false;
    Instruction lastInst = Instruction::STOP;

    if ( m_lastInst.size() == ext.depth ) {
        // starting a new context
        assert( m_lastInst.size() == ext.depth );
        m_lastInst.push_back( inst );
        newContext = true;
    } else if ( m_lastInst.size() == ext.depth + 2 ) {
        m_lastInst.pop_back();
        lastInst = m_lastInst.back();
    } else if ( m_lastInst.size() == ext.depth + 1 ) {
        // continuing in previous context
        lastInst = m_lastInst.back();
        m_lastInst.back() = inst;
    } else {
        cwarn << "GAA!!! Tracing VM and more than one new/deleted stack frame between steps!";
        cwarn << "Attmepting naive recovery...";
        m_lastInst.resize( ext.depth + 1 );
    }

    Json::Value memJson( Json::arrayValue );
    if ( vm && !m_options.disableMemory && ( changesMemory( lastInst ) || newContext ) ) {
        for ( unsigned i = 0; i < vm->memory().size(); i += 32 ) {
            bytesConstRef memRef( vm->memory().data() + i, 32 );
            Json::Value tmp = toHex( memRef );
            memJson.append( tmp );
        }
        r["memory"] = memJson;
    }

    if ( !m_options.disableStorage &&
         ( m_options.fullStorage || changesStorage( lastInst ) || newContext ) ) {
        Json::Value storage( Json::stringValue );
        storage = "Not supported";
        r["storage"] = storage;
    }

    if ( m_showMnemonics )
        r["op"] = instructionInfo( inst ).name;
    r["pc"] = toString( PC );
    r["gas"] = toString( gas );
    r["gasCost"] = toString( gasCost );
    r["depth"] = toString( ext.depth );
    if ( !!newMemSize )
        r["memexpand"] = toString( newMemSize );

    m_trace.append( r );
}

string StandardTrace::json( bool _styled ) const {
    if ( m_trace.empty() )
        return {};

    if ( _styled ) {
        return Json::StyledWriter().write( m_trace );
    }

    return std::accumulate( std::next( m_trace.begin() ), m_trace.end(),
        Json::FastWriter().write( m_trace[0] ),
        []( std::string a, Json::Value b ) { return a + Json::FastWriter().write( b ); } );
}

Executive::Executive(
    Block& _s, BlockChain const& _bc, const u256& _gasPrice, unsigned _level, bool _readOnly )
    : m_s( _s.mutableState() ),
      m_envInfo( _s.info(), _bc.lastBlockHashes(), 0, _bc.chainID() ),
      m_depth( _level ),
      m_readOnly( _readOnly ),
      m_sealEngine( *_bc.sealEngine() ),
      m_systemGasPrice( _gasPrice ) {}

Executive::Executive( Block& _s, LastBlockHashesFace const& _lh, const u256& _gasPrice,
    unsigned _level, bool _readOnly )
    : m_s( _s.mutableState() ),
      m_envInfo( _s.info(), _lh, 0, _s.sealEngine()->chainParams().chainID ),
      m_depth( _level ),
      m_readOnly( _readOnly ),
      m_sealEngine( *_s.sealEngine() ),
      m_systemGasPrice( _gasPrice ) {}

u256 Executive::gasUsed() const {
    return m_t.gas() - m_gas;
}

void Executive::accrueSubState( SubState& _parentContext ) {
    if ( m_ext )
        _parentContext += m_ext->sub;
}

void Executive::verifyTransaction( Transaction const& _transaction, BlockHeader const& _blockHeader,
    const State& _state, const SealEngineFace& _sealEngine, u256 const& _gasUsed,
    const u256& _gasPrice ) {
    MICROPROFILE_SCOPEI( "Executive", "verifyTransaction", MP_GAINSBORO );

    if ( !_transaction.hasExternalGas() && _transaction.gasPrice() < _gasPrice ) {
        BOOST_THROW_EXCEPTION(
            GasPriceTooLow() << RequirementError( static_cast< bigint >( _gasPrice ),
                static_cast< bigint >( _transaction.gasPrice() ) ) );
    }

    _sealEngine.verifyTransaction(
        ImportRequirements::Everything, _transaction, _blockHeader, _gasUsed );

    if ( !_transaction.hasZeroSignature() ) {
        // skip nonce check for calls
        if ( _transaction.hasSignature() ) {
            // Avoid invalid transactions.
            u256 nonceReq;
            nonceReq = _state.getNonce( _transaction.sender() );
            if ( _transaction.nonce() != nonceReq ) {
                std::cout << "WARNING: Transaction " << _transaction.sha3() << " nonce "
                          << _transaction.nonce() << " is not equal to required nonce " << nonceReq
                          << "\n";
                std::cout.flush();
                BOOST_THROW_EXCEPTION(
                    InvalidNonce() << RequirementError( static_cast< bigint >( nonceReq ),
                        static_cast< bigint >( _transaction.nonce() ) ) );
            }
        }

        // Avoid unaffordable transactions.
        bigint gasCost = static_cast< bigint >( _transaction.gas() * _transaction.gasPrice() );
        if ( _transaction.hasExternalGas() ) {
            gasCost = 0;
        }
        bigint totalCost = _transaction.value() + gasCost;
        auto sender_ballance = _state.balance( _transaction.sender() );
        if ( sender_ballance < totalCost ) {
            std::cout << "WARNING: Transaction " << _transaction.sha3() << " total cost "
                      << totalCost << " is less than sender " << _transaction.sender()
                      << " ballance " << sender_ballance << "\n";
            std::cout.flush();
            BOOST_THROW_EXCEPTION( NotEnoughCash()
                                   << RequirementError(
                                          totalCost, static_cast< bigint >(
                                                         _state.balance( _transaction.sender() ) ) )
                                   << errinfo_comment( _transaction.sender().hex() ) );
        }  // if balance
    }      // if !zero

    _transaction.verifiedOn = _blockHeader.number();
}

void Executive::initialize( Transaction const& _transaction ) {
    MICROPROFILE_SCOPEI( "Executive", "initialize", MP_GAINSBORO );
    m_t = _transaction;
    m_baseGasRequired = m_t.baseGasRequired( m_sealEngine.evmSchedule( m_envInfo.number() ) );

    try {
        verifyTransaction( _transaction, m_envInfo.header(), m_s, m_sealEngine, m_envInfo.gasUsed(),
            m_systemGasPrice );
    } catch ( Exception const& ex ) {
        m_excepted = toTransactionException( ex );
        throw;
    }

    bigint gasCost = ( bigint ) m_t.gas() * m_t.gasPrice();
    m_gasCost = ( u256 ) gasCost;
}

bool Executive::execute() {
    // Entry point for a user-executed transaction.

    if ( !m_t.hasExternalGas() ) {
        // Pay...
        LOG( m_detailsLogger ) << "Paying " << formatBalance( m_gasCost )
                               << " from sender for gas (" << m_t.gas() << " gas at "
                               << formatBalance( m_t.gasPrice() ) << ")";
        m_s.subBalance( m_t.sender(), m_gasCost );
    }

    assert( m_t.gas() >= ( u256 ) m_baseGasRequired );
    if ( m_t.isCreation() )
        return create( m_t.sender(), m_t.value(), m_t.gasPrice(),
            m_t.gas() - ( u256 ) m_baseGasRequired, &m_t.data(), m_t.sender() );
    else
        return call( m_t.receiveAddress(), m_t.sender(), m_t.value(), m_t.gasPrice(),
            bytesConstRef( &m_t.data() ), m_t.gas() - ( u256 ) m_baseGasRequired );
}

bool Executive::call( Address const& _receiveAddress, Address const& _senderAddress,
    u256 const& _value, u256 const& _gasPrice, bytesConstRef _data, u256 const& _gas ) {
    CallParameters params{
        _senderAddress, _receiveAddress, _receiveAddress, _value, _value, _gas, _data, {}};
    return call( params, _gasPrice, _senderAddress );
}

bool Executive::call( CallParameters const& _p, u256 const& _gasPrice, Address const& _origin ) {
    MICROPROFILE_SCOPEI( "Executive", "call", MP_NAVY );
    // If external transaction.
    if ( m_t ) {
        // FIXME: changelog contains unrevertable balance change that paid
        //        for the transaction.
        // Increment associated nonce for sender.
        if ( _p.senderAddress != MaxAddress ||
             m_envInfo.number() < m_sealEngine.chainParams().constantinopleForkBlock ) {  // EIP86
            MICROPROFILE_SCOPEI( "Executive", "call-incNonce", MP_SEAGREEN );
            m_s.incNonce( _p.senderAddress );
        }
    }

    m_savepoint = m_s.savepoint();

    if ( m_sealEngine.isPrecompiled( _p.codeAddress, m_envInfo.number() ) &&
         m_sealEngine.precompiledExecutionAllowedFrom(
             _p.codeAddress, _p.senderAddress, m_readOnly ) ) {
        MICROPROFILE_SCOPEI( "Executive", "call-precompiled", MP_CYAN );
        bigint g = m_sealEngine.costOfPrecompiled( _p.codeAddress, _p.data, m_envInfo.number() );
        if ( _p.gas < g ) {
            m_excepted = TransactionException::OutOfGasBase;
            // Bail from exception.

            // Empty precompiled contracts need to be deleted even in case of OOG
            // because the bug in both Geth and Parity led to deleting RIPEMD precompiled in this
            // case see
            // https://github.com/ethereum/go-ethereum/pull/3341/files#diff-2433aa143ee4772026454b8abd76b9dd
            // We mark the account as touched here, so that is can be removed among other touched
            // empty accounts (after tx finalization)
            if ( m_envInfo.number() >= m_sealEngine.chainParams().EIP158ForkBlock )
                m_s.addBalance( _p.codeAddress, 0 );

            return true;  // true actually means "all finished - nothing more to be done regarding
                          // go().
        } else {
            m_gas = ( u256 )( _p.gas - g );
            bytes output;
            bool success;
            tie( success, output ) =
                m_sealEngine.executePrecompiled( _p.codeAddress, _p.data, m_envInfo.number() );
            size_t outputSize = output.size();
            m_output = owning_bytes_ref{std::move( output ), 0, outputSize};
            if ( !success ) {
                m_gas = 0;
                m_excepted = TransactionException::OutOfGas;
                return true;  // true means no need to run go().
            }
        }
    } else {
        m_gas = _p.gas;
        if ( m_s.addressHasCode( _p.codeAddress ) ) {
            MICROPROFILE_SCOPEI( "Executive", "call create ExtVM", MP_DARKTURQUOISE );
            bytes const& c = m_s.code( _p.codeAddress );
            h256 codeHash = m_s.codeHash( _p.codeAddress );
            // Contract will be executed with the version stored in account
            auto const version = m_s.version( _p.codeAddress );
            m_ext = make_shared< ExtVM >( m_s, m_envInfo, m_sealEngine, _p.receiveAddress,
                _p.senderAddress, _origin, _p.apparentValue, _gasPrice, _p.data, &c, codeHash,
                version, m_depth, false, _p.staticCall, m_readOnly );
        }
    }

    // Transfer ether.
    {
        MICROPROFILE_SCOPEI( "Executive", "call-transferBalance", MP_PALEGREEN );
        m_s.transferBalance( _p.senderAddress, _p.receiveAddress, _p.valueTransfer );
    }
    return !m_ext;
}

bool Executive::create( Address const& _txSender, u256 const& _endowment, u256 const& _gasPrice,
    u256 const& _gas, bytesConstRef _init, Address const& _origin ) {
    MICROPROFILE_SCOPEI( "Executive", "create", MP_OLDLACE );
    // Contract creation by an external account is the same as CREATE opcode
    return createOpcode( _txSender, _endowment, _gasPrice, _gas, _init, _origin );
}

bool Executive::createOpcode( Address const& _sender, u256 const& _endowment, u256 const& _gasPrice,
    u256 const& _gas, bytesConstRef _init, Address const& _origin ) {
    u256 nonce = m_s.getNonce( _sender );
    m_newAddress = right160( sha3( rlpList( _sender, nonce ) ) );
    return executeCreate(
        _sender, _endowment, _gasPrice, _gas, _init, _origin, m_s.version( _sender ) );
}

bool Executive::create2Opcode( Address const& _sender, u256 const& _endowment,
    u256 const& _gasPrice, u256 const& _gas, bytesConstRef _init, Address const& _origin,
    u256 const& _salt ) {
    m_newAddress =
        right160( sha3( bytes{0xff} + _sender.asBytes() + toBigEndian( _salt ) + sha3( _init ) ) );
    return executeCreate(
        _sender, _endowment, _gasPrice, _gas, _init, _origin, m_s.version( _sender ) );
}

bool Executive::executeCreate( Address const& _sender, u256 const& _endowment,
    u256 const& _gasPrice, u256 const& _gas, bytesConstRef _init, Address const& _origin,
    u256 const& _version ) {
    if ( _sender != MaxAddress ||
         m_envInfo.number() < m_sealEngine.chainParams().experimentalForkBlock )  // EIP86
        m_s.incNonce( _sender );

    m_savepoint = m_s.savepoint();

    m_isCreation = true;

    // We can allow for the reverted state (i.e. that with which m_ext is constructed) to contain
    // the m_orig.address, since we delete it explicitly if we decide we need to revert.

    m_gas = _gas;
    bool accountAlreadyExist =
        ( m_s.addressHasCode( m_newAddress ) || m_s.getNonce( m_newAddress ) > 0 );
    if ( accountAlreadyExist ) {
        LOG( m_detailsLogger ) << "Address already used: " << m_newAddress;
        m_gas = 0;
        m_excepted = TransactionException::AddressAlreadyUsed;
        revert();
        m_ext = {};  // cancel the _init execution if there are any scheduled.
        return !m_ext;
    }

    // Transfer ether before deploying the code. This will also create new
    // account if it does not exist yet.
    m_s.transferBalance( _sender, m_newAddress, _endowment );

    u256 newNonce = m_s.requireAccountStartNonce();
    if ( m_envInfo.number() >= m_sealEngine.chainParams().EIP158ForkBlock )
        newNonce += 1;
    m_s.setNonce( m_newAddress, newNonce );

    m_s.clearStorage( m_newAddress );

    // Schedule _init execution if not empty.
    if ( !_init.empty() )
        m_ext = make_shared< ExtVM >( m_s, m_envInfo, m_sealEngine, m_newAddress, _sender, _origin,
            _endowment, _gasPrice, bytesConstRef(), _init, sha3( _init ), _version, m_depth, true,
            false );
    else
        // code stays empty, but we set the version
        m_s.setCode( m_newAddress, {}, _version );

    return !m_ext;
}

OnOpFunc Executive::simpleTrace() {
    Logger& traceLogger = m_vmTraceLogger;

    return [&traceLogger]( uint64_t steps, uint64_t PC, Instruction inst, bigint newMemSize,
               bigint gasCost, bigint gas, VMFace const* _vm, ExtVMFace const* voidExt ) {
        ExtVM const& ext = *static_cast< ExtVM const* >( voidExt );
        auto vm = dynamic_cast< LegacyVM const* >( _vm );

        ostringstream o;
        if ( vm )
            LOG( traceLogger ) << dumpStackAndMemory( *vm );
        LOG( traceLogger ) << dumpStorage( ext );
        LOG( traceLogger ) << " < " << dec << ext.depth << " : " << ext.myAddress << " : #" << steps
                           << " : " << hex << setw( 4 ) << setfill( '0' ) << PC << " : "
                           << instructionInfo( inst ).name << " : " << dec << gas << " : -" << dec
                           << gasCost << " : " << newMemSize << "x32"
                           << " >";
    };
}

bool Executive::go( OnOpFunc const& _onOp ) {
    MICROPROFILE_SCOPEI( "Executive", "go", MP_HONEYDEW );
    if ( m_ext ) {
#if ETH_TIMED_EXECUTIONS
        Timer t;
#endif
        try {
            // Create VM instance. Force Interpreter if tracing requested.
            auto vm = VMFactory::create();
            if ( m_isCreation ) {
                bytes in = getDeploymentControllerCallData( m_ext->caller );
                unique_ptr< CallParameters > deploymentCallParams(
                    new CallParameters( SystemAddress, c_deploymentControllerContractAddress,
                        c_deploymentControllerContractAddress, 0, 0, m_gas,
                        bytesConstRef( in.data(), in.size() ), {} ) );
                auto deploymentCallResult = m_ext->call( *deploymentCallParams );
                auto deploymentCallOutput = dev::toHex( deploymentCallResult.output );
                if ( !deploymentCallOutput.empty() && u256( deploymentCallOutput ) == 0 ) {
                    BOOST_THROW_EXCEPTION( InvalidContractDeployer() );
                }

                auto out = vm->exec( m_gas, *m_ext, _onOp );
                if ( m_res ) {
                    m_res->gasForDeposit = m_gas;
                    m_res->depositSize = out.size();
                }
                if ( out.size() > m_ext->evmSchedule().maxCodeSize )
                    BOOST_THROW_EXCEPTION( OutOfGas() );
                else if ( out.size() * m_ext->evmSchedule().createDataGas <= m_gas ) {
                    if ( m_res )
                        m_res->codeDeposit = CodeDeposit::Success;
                    m_gas -= out.size() * m_ext->evmSchedule().createDataGas;
                } else {
                    if ( m_ext->evmSchedule().exceptionalFailedCodeDeposit )
                        BOOST_THROW_EXCEPTION( OutOfGas() );
                    else {
                        if ( m_res )
                            m_res->codeDeposit = CodeDeposit::Failed;
                        out = {};
                    }
                }
                if ( m_res )
                    m_res->output = out.toVector();  // copy output to execution result
                m_s.setCode( m_ext->myAddress, out.toVector(), m_ext->version );
            } else
                m_output = vm->exec( m_gas, *m_ext, _onOp );
        } catch ( RevertInstruction& _e ) {
            revert();
            m_output = _e.output();
            m_excepted = TransactionException::RevertInstruction;
        } catch ( VMException const& _e ) {
            LOG( m_detailsLogger ) << "Safe VM Exception. " << diagnostic_information( _e );
            m_gas = 0;
            m_excepted = toTransactionException( _e );
            revert();
        } catch ( InternalVMError const& _e ) {
            cwarn << "Internal VM Error (" << *boost::get_error_info< errinfo_evmcStatusCode >( _e )
                  << ")\n"
                  << diagnostic_information( _e );
            revert();
            throw;
        } catch ( Exception const& _e ) {
            // TODO: AUDIT: check that this can never reasonably happen. Consider what to do if it
            // does.
            cwarn << "Unexpected exception in VM. There may be a bug in this implementation. "
                  << diagnostic_information( _e );
            exit( 1 );
            // Another solution would be to reject this transaction, but that also
            // has drawbacks. Essentially, the amount of ram has to be increased here.
        } catch ( std::exception const& _e ) {
            // TODO: AUDIT: check that this can never reasonably happen. Consider what to do if it
            // does.
            cwarn << "Unexpected std::exception in VM. Not enough RAM? " << _e.what();
            exit( 1 );
            // Another solution would be to reject this transaction, but that also
            // has drawbacks. Essentially, the amount of ram has to be increased here.
        }

        if ( m_res && m_output )
            // Copy full output:
            m_res->output = m_output.toVector();

#if ETH_TIMED_EXECUTIONS
        cnote << "VM took:" << t.elapsed() << "; gas used: " << ( sgas - m_endGas );
#endif
    }
    return true;
}

bool Executive::finalize() {
    MICROPROFILE_SCOPEI( "Executive", "finalize", MP_PAPAYAWHIP );
    if ( m_ext ) {
        // Accumulate refunds for suicides.
        m_ext->sub.refunds += m_ext->evmSchedule().suicideRefundGas * m_ext->sub.suicides.size();

        // Refunds must be applied before the miner gets the fees.
        assert( m_ext->sub.refunds >= 0 );
        int64_t maxRefund =
            ( static_cast< int64_t >( m_t.gas() ) - static_cast< int64_t >( m_gas ) ) / 2;
        m_gas += min( maxRefund, m_ext->sub.refunds );
    }

    if ( m_t ) {
        m_s.addBalance( m_t.sender(), m_gas * m_t.gasPrice() );

        u256 feesEarned = ( m_t.gas() - m_gas ) * m_t.gasPrice();
        m_s.addBalance( m_envInfo.author(), feesEarned );
    }

    // Suicides...
    if ( m_ext )
        for ( auto a : m_ext->sub.suicides )
            m_s.kill( a );

    // Logs..
    if ( m_ext )
        m_logs = m_ext->sub.logs;

    if ( m_res )  // Collect results
    {
        m_res->gasUsed = gasUsed();
        m_res->excepted = m_excepted;  // TODO: m_except is used only in ExtVM::call
        m_res->newAddress = m_newAddress;
        m_res->gasRefunded = m_ext ? m_ext->sub.refunds : 0;
    }
    return ( m_excepted == TransactionException::None );
}

void Executive::revert() {
    if ( m_ext )
        m_ext->sub.clear();

    // Set result address to the null one.
    m_newAddress = {};
    m_s.rollback( m_savepoint );
}
