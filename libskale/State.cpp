/*
    Copyright (C) 2018-present, SKALE Labs

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
 * @file State.cpp
 * @author Dmytro Stebaiev
 * @date 2018
 */

#include "State.h"

#include <mutex>

#include <boost/filesystem.hpp>
#include <boost/timer.hpp>
#include <boost/utility/in_place_factory.hpp>

namespace fs = boost::filesystem;
using boost::shared_lock;
using boost::shared_mutex;
using boost::upgrade_lock;
using boost::upgrade_to_unique_lock;

#include <libdevcore/DBImpl.h>
#include <libethcore/SealEngine.h>
#include <libethereum/CodeSizeCache.h>
#include <libethereum/Defaults.h>

#include "libweb3jsonrpc/Eth.h"
#include "libweb3jsonrpc/JsonHelper.h"

#include <skutils/console_colors.h>
#include <skutils/eth_utils.h>

using namespace std;
using namespace dev;
using namespace skale;
using namespace skale::error;
using dev::eth::Account;
using dev::eth::EnvInfo;
using dev::eth::ExecutionResult;
using dev::eth::Executive;
using dev::eth::OnOpFunc;
using dev::eth::SealEngineFace;
using dev::eth::Transaction;
using dev::eth::TransactionReceipt;

#ifndef ETH_VMTRACE
#define ETH_VMTRACE 0
#endif

State::State(
    u256 const& _accountStartNonce, OverlayDB const& _db, BaseState _bs, u256 _initialFunds )
    : x_db_ptr( make_shared< shared_mutex >() ),
      m_db_ptr( make_shared< OverlayDB >( _db ) ),
      m_storedVersion( make_shared< size_t >( 0 ) ),
      m_currentVersion( *m_storedVersion ),
      m_accountStartNonce( _accountStartNonce ),
      m_initial_funds( _initialFunds ) {
    if ( _bs == BaseState::PreExisting ) {
        clog( VerbosityDebug, "statedb" ) << cc::debug( "Using existing database" );
    } else if ( _bs == BaseState::Empty ) {
        // Initialise to the state entailed by the genesis block; this guarantees the trie is built
        // correctly.
        m_db_ptr->clearDB();
    } else {
        throw std::logic_error( "Not implemented" );
    }
}

skale::OverlayDB State::openDB(
    fs::path const& _basePath, h256 const& _genesisHash, WithExisting _we ) {
    fs::path path = _basePath.empty() ? eth::Defaults::dbPath() : _basePath;

    if ( _we == WithExisting::Kill ) {
        clog( VerbosityDebug, "statedb" ) << "Killing state database (WithExisting::Kill).";
        fs::remove_all( path / fs::path( "state" ) );
    }

    path /= fs::path( toHex( _genesisHash.ref().cropped( 0, 4 ) ) ) /
            fs::path( toString( eth::c_databaseVersion ) );
    fs::create_directories( path );
    DEV_IGNORE_EXCEPTIONS( fs::permissions( path, fs::owner_all ) );

    fs::path state_path = path / fs::path( "state" );
    try {
        std::unique_ptr< db::DatabaseFace > db( new db::DBImpl( state_path ) );
        clog( VerbosityTrace, "statedb" ) << cc::success( "Opened state DB." );
        return OverlayDB( std::move( db ) );
    } catch ( boost::exception const& ex ) {
        cwarn << boost::diagnostic_information( ex ) << '\n';
        if ( fs::space( path / fs::path( "state" ) ).available < 1024 ) {
            cwarn << "Not enough available space found on hard drive. Please free some up and "
                     "then "
                     "re-run. Bailing.";
            BOOST_THROW_EXCEPTION( eth::NotEnoughAvailableSpace() );
        } else {
            cwarn << "Database " << ( path / fs::path( "state" ) )
                  << "already open. You appear to have another instance of ethereum running. "
                     "Bailing.";
            BOOST_THROW_EXCEPTION( eth::DatabaseAlreadyOpen() );
        }
    }
}

State::State( const State& _s ) {
    *this = _s;
}

State& State::operator=( const State& _s ) {
    x_db_ptr = _s.x_db_ptr;
    if ( _s.m_db_read_lock ) {
        m_db_read_lock.emplace( *x_db_ptr );
    }
    if ( _s.m_db_write_lock ) {
        std::logic_error( "Can't copy locked for writing state object" );
    }
    m_db_ptr = _s.m_db_ptr;
    m_storedVersion = _s.m_storedVersion;
    m_currentVersion = _s.m_currentVersion;
    m_cache = _s.m_cache;
    m_unchangedCacheEntries = _s.m_unchangedCacheEntries;
    m_nonExistingAccountsCache = _s.m_nonExistingAccountsCache;
    m_accountStartNonce = _s.m_accountStartNonce;
    m_changeLog = _s.m_changeLog;
    m_initial_funds = _s.m_initial_funds;

    return *this;
}

void State::populateFrom( eth::AccountMap const& _map ) {
    for ( auto const& addressAccountPair : _map ) {
        const Address& address = addressAccountPair.first;
        const eth::Account& account = addressAccountPair.second;

        if ( account.isDirty() ) {
            if ( !account.isAlive() ) {
                throw logic_error( "Removing of accounts is not implemented" );
            } else {
                setNonce( address, account.nonce() );
                setBalance( address, account.balance() );
                for ( auto const& storageAddressValuePair : account.storageOverlay() ) {
                    const u256& storageAddress = storageAddressValuePair.first;
                    const u256& value = storageAddressValuePair.second;
                    setStorage( address, storageAddress, value );
                }

                if ( account.hasNewCode() ) {
                    setCode( address, account.code(), account.version() );
                }
            }
        }
    }
    commit( State::CommitBehaviour::KeepEmptyAccounts );
}

std::unordered_map< Address, u256 > State::addresses() const {
    shared_lock< shared_mutex > lock( *x_db_ptr );
    if ( !checkVersion() ) {
        cerr << "Current state version is " << m_currentVersion << " but stored version is "
             << *m_storedVersion << endl;
        BOOST_THROW_EXCEPTION( AttemptToReadFromStateInThePast() );
    }

    std::unordered_map< Address, u256 > addresses;
    for ( auto const& h160StringPair : m_db_ptr->accounts() ) {
        Address const& address = h160StringPair.first;
        string const& rlpString = h160StringPair.second;
        RLP account( rlpString );
        u256 balance = account[1].toInt< u256 >();
        addresses[address] = balance;
    }
    for ( auto const& addressAccountPair : m_cache ) {
        addresses[addressAccountPair.first] = addressAccountPair.second.balance();
    }
    return addresses;
}

std::pair< State::AddressMap, h256 > State::addresses(
    const h256& _begin, size_t _maxResults ) const {
    // TODO: do not read all values from database;
    unordered_map< Address, u256 > balances = addresses();
    AddressMap addresses;
    h256 next;
    for ( auto const& pair : balances ) {
        Address const& address = pair.first;
        auto cache_ptr = m_cache.find( address );
        if ( cache_ptr != m_cache.end() ) {
            if ( !cache_ptr->second.isAlive() ) {
                continue;
            }
        }
        addresses[sha3( address )] = address;
    }
    addresses.erase( addresses.begin(), addresses.lower_bound( _begin ) );
    if ( addresses.size() > _maxResults ) {
        assert( numeric_limits< long >::max() >= _maxResults );
        auto next_ptr = std::next( addresses.begin(), static_cast< long >( _maxResults ) );
        next = next_ptr->first;
        addresses.erase( next_ptr, addresses.end() );
    }
    return {addresses, next};
}

u256 const& State::requireAccountStartNonce() const {
    if ( m_accountStartNonce == Invalid256 )
        BOOST_THROW_EXCEPTION( InvalidAccountStartNonceInState() );
    return m_accountStartNonce;
}

void State::noteAccountStartNonce( u256 const& _actual ) {
    if ( m_accountStartNonce == Invalid256 )
        m_accountStartNonce = _actual;
    else if ( m_accountStartNonce != _actual )
        BOOST_THROW_EXCEPTION( IncorrectAccountStartNonceInState() );
}

void State::removeEmptyAccounts() {
    for ( auto& i : m_cache )
        if ( i.second.isDirty() && i.second.isEmpty() )
            i.second.kill();
}

eth::Account const* State::account( Address const& _a ) const {
    return const_cast< State* >( this )->account( _a );
}

eth::Account* State::account( Address const& _address ) {
    auto it = m_cache.find( _address );
    if ( it != m_cache.end() )
        return &it->second;

    if ( m_nonExistingAccountsCache.count( _address ) )
        return nullptr;

    // Populate basic info.
    bytes stateBack;
    {
        shared_lock< shared_mutex > lock( *x_db_ptr );

        if ( !checkVersion() ) {
            cerr << "Current state version is " << m_currentVersion << " but stored version is "
                 << *m_storedVersion << endl;
            BOOST_THROW_EXCEPTION( AttemptToReadFromStateInThePast() );
        }

        stateBack = asBytes( m_db_ptr->lookup( _address ) );
    }
    if ( stateBack.empty() ) {
        m_nonExistingAccountsCache.insert( _address );
        return nullptr;
    }

    clearCacheIfTooLarge();

    RLP state( stateBack );
    u256 nonce = state[0].toInt< u256 >();
    u256 balance = state[1].toInt< u256 >();
    h256 codeHash = state[2].toInt< u256 >();
    // version is 0 if absent from RLP
    auto const version = state[4] ? state[4].toInt< u256 >() : 0;

    auto i = m_cache.emplace( std::piecewise_construct, std::forward_as_tuple( _address ),
        std::forward_as_tuple( nonce, balance, EmptyTrie, codeHash, version,
            dev::eth::Account::Changedness::Unchanged ) );
    m_unchangedCacheEntries.push_back( _address );
    return &i.first->second;
}

void State::clearCacheIfTooLarge() const {
    // TODO: Find a good magic number
    while ( m_unchangedCacheEntries.size() > 1000 ) {
        // Remove a random element
        // FIXME: Do not use random device as the engine. The random device should be only used to
        // seed other engine.
        size_t const randomIndex = std::uniform_int_distribution< size_t >(
            0, m_unchangedCacheEntries.size() - 1 )( dev::s_fixedHashEngine );

        Address const addr = m_unchangedCacheEntries[randomIndex];
        swap( m_unchangedCacheEntries[randomIndex], m_unchangedCacheEntries.back() );
        m_unchangedCacheEntries.pop_back();

        auto cacheEntry = m_cache.find( addr );
        if ( cacheEntry != m_cache.end() && !cacheEntry->second.isDirty() )
            m_cache.erase( cacheEntry );
    }
}

void State::commit( CommitBehaviour _commitBehaviour ) {
    if ( _commitBehaviour == CommitBehaviour::RemoveEmptyAccounts )
        removeEmptyAccounts();

    {
        if ( !m_db_write_lock ) {
            BOOST_THROW_EXCEPTION( AttemptToWriteToNotLockedStateObject() );
        }
        upgrade_to_unique_lock< shared_mutex > lock( *m_db_write_lock );
        if ( !checkVersion() ) {
            BOOST_THROW_EXCEPTION( AttemptToWriteToStateInThePast() );
        }

        for ( auto const& addressAccountPair : m_cache ) {
            const Address& address = addressAccountPair.first;
            const eth::Account& account = addressAccountPair.second;

            if ( account.isDirty() ) {
                if ( !account.isAlive() ) {
                    m_db_ptr->kill( address );
                    m_db_ptr->killAuxiliary( address, Auxiliary::CODE );
                    // TODO: remove account storage
                } else {
                    RLPStream rlpStream( 3 );
                    rlpStream << account.nonce() << account.balance() << u256( account.codeHash() );
                    auto rawValue = rlpStream.out();

                    m_db_ptr->insert( address, ref( rawValue ) );

                    for ( auto const& storageAddressValuePair : account.storageOverlay() ) {
                        const u256& storageAddress = storageAddressValuePair.first;
                        const u256& value = storageAddressValuePair.second;

                        m_db_ptr->insert( address, storageAddress, value );
                    }

                    if ( account.hasNewCode() ) {
                        m_db_ptr->insertAuxiliary(
                            address, ref( account.code() ), Auxiliary::CODE );
                    }
                }
            }
        }
        m_db_ptr->commit();
        ++*m_storedVersion;
        m_currentVersion = *m_storedVersion;
    }

    m_changeLog.clear();
    m_cache.clear();
    m_unchangedCacheEntries.clear();
}

bool State::addressInUse( Address const& _id ) const {
    return !!account( _id );
}

bool State::accountNonemptyAndExisting( Address const& _address ) const {
    if ( eth::Account const* a = account( _address ) )
        return !a->isEmpty();
    else
        return false;
}

bool State::addressHasCode( Address const& _id ) const {
    if ( auto a = account( _id ) )
        return a->codeHash() != EmptySHA3;
    else
        return false;
}

u256 State::balance( Address const& _id ) const {
    if ( auto a = account( _id ) )
        return a->balance();
    else
        return m_initial_funds;
}

void State::incNonce( Address const& _addr ) {
    if ( eth::Account* a = account( _addr ) ) {
        auto oldNonce = a->nonce();
        a->incNonce();
        m_changeLog.emplace_back( _addr, oldNonce );
    } else
        // This is possible if a transaction has gas price 0.
        createAccount( _addr, eth::Account( requireAccountStartNonce() + 1, m_initial_funds ) );
}

void State::setNonce( Address const& _addr, u256 const& _newNonce ) {
    if ( eth::Account* a = account( _addr ) ) {
        auto oldNonce = a->nonce();
        a->setNonce( _newNonce );
        m_changeLog.emplace_back( _addr, oldNonce );
    } else
        // This is possible when a contract is being created.
        createAccount( _addr, eth::Account( _newNonce, m_initial_funds ) );
}

void State::addBalance( Address const& _id, u256 const& _amount ) {
    if ( eth::Account* a = account( _id ) ) {
        // Log empty account being touched. Empty touched accounts are cleared
        // after the transaction, so this event must be also reverted.
        // We only log the first touch (not dirty yet), and only for empty
        // accounts, as other accounts does not matter.
        // TODO: to save space we can combine this event with Balance by having
        //       Balance and Balance+Touch events.
        if ( !a->isDirty() && a->isEmpty() )
            m_changeLog.emplace_back( Change::Touch, _id );

        // Increase the account balance. This also is done for value 0 to mark
        // the account as dirty. Dirty account are not removed from the cache
        // and are cleared if empty at the end of the transaction.
        a->addBalance( _amount );
    } else
        createAccount( _id, eth::Account( requireAccountStartNonce(), m_initial_funds + _amount ) );

    if ( _amount )
        m_changeLog.emplace_back( Change::Balance, _id, _amount );
}

void State::subBalance( Address const& _addr, u256 const& _value ) {
    if ( _value == 0 )
        return;

    eth::Account* a = account( _addr );
    if ( !a || a->balance() < _value )
        // TODO: I expect this never happens.
        BOOST_THROW_EXCEPTION( eth::NotEnoughCash() );

    // Fall back to addBalance().
    addBalance( _addr, 0 - _value );
}

void State::setBalance( Address const& _addr, u256 const& _value ) {
    eth::Account* a = account( _addr );
    u256 original = a ? a->balance() : 0;

    // Fall back to addBalance().
    addBalance( _addr, _value - original );
}

void State::createContract( Address const& _address ) {
    createAccount( _address, {requireAccountStartNonce(), m_initial_funds} );
}

void State::createAccount( Address const& _address, eth::Account const&& _account ) {
    assert( !addressInUse( _address ) && "Account already exists" );
    m_cache[_address] = std::move( _account );
    m_nonExistingAccountsCache.erase( _address );
    m_changeLog.emplace_back( Change::Create, _address );
}

void State::kill( Address _addr ) {
    if ( auto a = account( _addr ) )
        a->kill();
    // If the account is not in the db, nothing to kill.
}

std::map< h256, std::pair< u256, u256 > > State::storage( const Address& _contract ) const {
    shared_lock< shared_mutex > lock( *x_db_ptr );
    if ( !checkVersion() ) {
        cerr << "Current state version is " << m_currentVersion << " but stored version is "
             << *m_storedVersion << endl;
        BOOST_THROW_EXCEPTION( AttemptToReadFromStateInThePast() );
    }

    std::map< h256, std::pair< u256, u256 > > storage;
    for ( auto const& addressValuePair : m_db_ptr->storage( _contract ) ) {
        u256 const& address = addressValuePair.first;
        u256 const& value = addressValuePair.second;
        storage[sha3( address )] = {address, value};
    }
    for ( auto const& addressAccountPair : m_cache ) {
        Address const& accountAddress = addressAccountPair.first;
        eth::Account const& account = addressAccountPair.second;
        if ( account.isDirty() && accountAddress == _contract ) {
            for ( auto const& addressValuePair : account.storageOverlay() ) {
                storage[sha3( addressValuePair.first )] = addressValuePair;
            }
        }
    }
    return storage;
}

u256 State::getNonce( Address const& _addr ) const {
    if ( auto a = account( _addr ) )
        return a->nonce();
    else
        return m_accountStartNonce;
}

u256 State::storage( Address const& _id, u256 const& _key ) const {
    if ( eth::Account const* acc = account( _id ) ) {
        auto memoryIterator = acc->storageOverlay().find( _key );
        if ( memoryIterator != acc->storageOverlay().end() )
            return memoryIterator->second;

        memoryIterator = acc->originalStorageValue().find( _key );
        if ( memoryIterator != acc->originalStorageValue().end() )
            return memoryIterator->second;

        // Not in the storage cache - go to the DB.
        shared_lock< shared_mutex > lock( *x_db_ptr );
        if ( !checkVersion() ) {
            BOOST_THROW_EXCEPTION( AttemptToReadFromStateInThePast() );
        }
        u256 value = m_db_ptr->lookup( _id, _key );
        acc->setStorageCache( _key, value );
        return value;
    } else
        return 0;
}

void State::setStorage( Address const& _contract, u256 const& _key, u256 const& _value ) {
    m_changeLog.emplace_back( _contract, _key, storage( _contract, _key ) );
    m_cache[_contract].setStorage( _key, _value );
}

u256 State::originalStorageValue( Address const& _contract, u256 const& _key ) const {
    if ( Account const* acc = account( _contract ) ) {
        auto memoryPtr = acc->originalStorageValue().find( _key );
        if ( memoryPtr != acc->originalStorageValue().end() ) {
            return memoryPtr->second;
        }

        shared_lock< shared_mutex > lock( *x_db_ptr );
        if ( !checkVersion() ) {
            BOOST_THROW_EXCEPTION( AttemptToReadFromStateInThePast() );
        }
        u256 value = m_db_ptr->lookup( _contract, _key );
        acc->setStorageCache( _key, value );
        return value;
    } else {
        return 0;
    }
}

void State::clearStorage( Address const& _contract ) {
    // TODO: This is extremely inefficient
    Account* acc = account( _contract );
    for ( auto const& hashPairPair : storage( _contract ) ) {
        auto const& key = hashPairPair.second.first;
        setStorage( _contract, key, 0 );
        acc->setStorageCache( key, 0 );
    }
}

bytes const& State::code( Address const& _addr ) const {
    eth::Account const* a = account( _addr );
    if ( !a || a->codeHash() == EmptySHA3 )
        return NullBytes;

    if ( a->code().empty() ) {
        // Load the code from the backend.
        eth::Account* mutableAccount = const_cast< eth::Account* >( a );
        shared_lock< shared_mutex > lock( *x_db_ptr );
        if ( !checkVersion() ) {
            BOOST_THROW_EXCEPTION( AttemptToReadFromStateInThePast() );
        }
        mutableAccount->noteCode( m_db_ptr->lookupAuxiliary( _addr, Auxiliary::CODE ) );
        eth::CodeSizeCache::instance().store( a->codeHash(), a->code().size() );
    }

    return a->code();
}

void State::setCode( Address const& _address, bytes&& _code, u256 const& _version ) {
    // rollback assumes that overwriting of the code never happens
    // (not allowed in contract creation logic in Executive)
    assert( !addressHasCode( _address ) );
    m_changeLog.emplace_back( _address, code( _address ) );
    m_cache[_address].setCode( std::move( _code ), _version );
}

void State::setCode( const Address& _address, const bytes& _code, u256 const& _version ) {
    setCode( _address, bytes( _code ), _version );
}

h256 State::codeHash( Address const& _a ) const {
    if ( eth::Account const* a = account( _a ) )
        return a->codeHash();
    else
        return EmptySHA3;
}

size_t State::codeSize( Address const& _a ) const {
    if ( eth::Account const* a = account( _a ) ) {
        if ( a->hasNewCode() )
            return a->code().size();
        auto& codeSizeCache = eth::CodeSizeCache::instance();
        h256 codeHash = a->codeHash();
        if ( codeSizeCache.contains( codeHash ) )
            return codeSizeCache.get( codeHash );
        else {
            size_t size = code( _a ).size();
            codeSizeCache.store( codeHash, size );
            return size;
        }
    } else
        return 0;
}

u256 State::version( const Address& _contract ) const {
    Account const* a = account( _contract );
    return a ? a->version() : 0;
}

size_t State::savepoint() const {
    return m_changeLog.size();
}

void State::rollback( size_t _savepoint ) {
    assert( _savepoint <= m_changeLog.size() );
    while ( _savepoint != m_changeLog.size() ) {
        auto& change = m_changeLog.back();
        eth::Account* account_ptr = this->account( change.address );
        assert( account_ptr );
        auto& account = *( account_ptr );

        // Public State API cannot be used here because it will add another
        // change log entry.
        switch ( change.kind ) {
        case Change::Storage:
            account.setStorage( change.key, change.value );
            break;
        case Change::StorageRoot:
            account.setStorageRoot( change.value );
            break;
        case Change::Balance:
            account.addBalance( 0 - change.value );
            break;
        case Change::Nonce:
            account.setNonce( change.value );
            break;
        case Change::Create:
            m_cache.erase( change.address );
            break;
        case Change::Code:
            account.resetCode();
            break;
        case Change::Touch:
            account.untouch();
            m_unchangedCacheEntries.emplace_back( change.address );
            break;
        }
        m_changeLog.pop_back();
    }
}

void State::updateToLatestVersion() {
    m_changeLog.clear();
    m_cache.clear();
    m_unchangedCacheEntries.clear();
    m_nonExistingAccountsCache.clear();

    {
        shared_lock< shared_mutex > lock( *x_db_ptr );
        m_currentVersion = *m_storedVersion;
    }
}

State State::startRead() const {
    State stateCopy = State( *this );
    stateCopy.m_db_read_lock.emplace( *stateCopy.x_db_ptr );
    stateCopy.updateToLatestVersion();
    return stateCopy;
}

State State::startWrite() const {
    State stateCopy = State( *this );
    stateCopy.m_db_write_lock.emplace( *stateCopy.x_db_ptr );
    stateCopy.updateToLatestVersion();
    return stateCopy;
}

State State::delegateWrite() {
    if ( m_db_write_lock ) {
        upgrade_lock< shared_mutex > lock;
        lock.swap( *m_db_write_lock );
        m_db_write_lock = boost::none;
        State stateCopy = State( *this );
        stateCopy.m_db_write_lock = upgrade_lock< shared_mutex >();
        stateCopy.m_db_write_lock->swap( lock );
        return stateCopy;
    } else {
        return startWrite();
    }
}

void State::stopWrite() {
    m_db_write_lock = boost::none;
}

State State::startNew() const {
    State copy = State( *this );
    copy.updateToLatestVersion();
    return copy;
}

void State::clearAll() {
    if ( m_db_ptr ) {
        if ( !m_db_write_lock ) {
            BOOST_THROW_EXCEPTION( AttemptToWriteToNotLockedStateObject() );
        }
        upgrade_to_unique_lock< shared_mutex > lock( *m_db_write_lock );
        if ( !checkVersion() ) {
            BOOST_THROW_EXCEPTION( AttemptToWriteToStateInThePast() );
        }
        m_db_ptr->clearDB();
    }
}

bool State::connected() const {
    if ( m_db_ptr ) {
        return m_db_ptr->connected();
    }
    return false;
}

bool State::empty() const {
    if ( m_cache.empty() ) {
        if ( m_db_ptr ) {
            shared_lock< shared_mutex > lock( *x_db_ptr );
            if ( m_db_ptr->empty() ) {
                return true;
            }
        } else {
            return true;
        }
    }
    return false;
}

std::pair< ExecutionResult, TransactionReceipt > State::execute( EnvInfo const& _envInfo,
    SealEngineFace const& _sealEngine, Transaction const& _t, Permanence _p,
    OnOpFunc const& _onOp ) {
    // Create and initialize the executive. This will throw fairly cheaply and quickly if the
    // transaction is bad in any way.
    // HACK 0 here is for gasPrice
    Executive e( *this, _envInfo, _sealEngine, 0 );
    ExecutionResult res;
    e.setResultRecipient( res );

    auto onOp = _onOp;
#if ETH_VMTRACE
    if ( !onOp )
        onOp = e.simpleTrace();
#endif
    u256 const startGasUsed = _envInfo.gasUsed();
    bool const statusCode = executeTransaction( e, _t, onOp );

    std::string strRevertReason;
    if ( res.excepted == dev::eth::TransactionException::RevertInstruction ) {
        strRevertReason = skutils::eth::call_error_message_2_str( res.output );
        if ( strRevertReason.empty() )
            strRevertReason = "EVM revert instruction without description message";
        std::string strOut = cc::fatal( "Error message from eth_call():" ) + cc::error( " " ) +
                             cc::warn( strRevertReason );
        cerror << strOut;
    }

    bool removeEmptyAccounts = false;
    switch ( _p ) {
    case Permanence::Reverted:
    case Permanence::CommittedWithoutState:
        m_cache.clear();
        break;
    case Permanence::Committed:
        removeEmptyAccounts = _envInfo.number() >= _sealEngine.chainParams().EIP158ForkBlock;
        commit( removeEmptyAccounts ? State::CommitBehaviour::RemoveEmptyAccounts :
                                      State::CommitBehaviour::KeepEmptyAccounts );
        break;
    case Permanence::Uncommitted:
        break;
    }

    TransactionReceipt receipt =
        _envInfo.number() >= _sealEngine.chainParams().byzantiumForkBlock ?
            TransactionReceipt( statusCode, startGasUsed + e.gasUsed(), e.logs() ) :
            TransactionReceipt( EmptyTrie, startGasUsed + e.gasUsed(), e.logs() );
    receipt.setRevertReason( strRevertReason );
    return make_pair( res, receipt );
}

/// @returns true when normally halted; false when exceptionally halted; throws when internal VM
/// exception occurred.
bool State::executeTransaction(
    eth::Executive& _e, eth::Transaction const& _t, eth::OnOpFunc const& _onOp ) {
    size_t const savept = savepoint();
    try {
        _e.initialize( _t );

        if ( !_e.execute() )
            _e.go( _onOp );
        return _e.finalize();
    } catch ( dev::eth::RevertInstruction const& re ) {
        rollback( savept );
        throw;
    } catch ( Exception const& ) {
        rollback( savept );
        throw;
    }
}

bool State::checkVersion() const {
    return *m_storedVersion == m_currentVersion;
}

std::ostream& skale::operator<<( std::ostream& _out, State const& _s ) {
    _out << cc::debug( "--- Cache ---" ) << std::endl;
    std::set< Address > d;
    for ( auto i : _s.m_cache )
        d.insert( i.first );

    for ( auto i : d ) {
        auto it = _s.m_cache.find( i );
        eth::Account* cache = it != _s.m_cache.end() ? &it->second : nullptr;
        assert( cache );

        if ( cache && !cache->isAlive() )
            _out << cc::debug( "XXX  " ) << i << std::endl;
        else {
            string lead = cc::debug( " +   " );
            if ( cache )
                lead = cc::debug( " .   " );

            stringstream contout;

            if ( ( cache && cache->codeHash() == EmptySHA3 ) || ( !cache ) ) {
                std::map< u256, u256 > mem;
                std::set< u256 > back;
                std::set< u256 > delta;
                std::set< u256 > cached;
                if ( cache )
                    for ( auto const& j : cache->storageOverlay() ) {
                        if ( ( !mem.count( j.first ) && j.second ) ||
                             ( mem.count( j.first ) && mem.at( j.first ) != j.second ) ) {
                            mem[j.first] = j.second;
                            delta.insert( j.first );
                        } else if ( j.second )
                            cached.insert( j.first );
                    }
                if ( !delta.empty() )
                    lead = ( lead == " .   " ) ? "*.*  " : "***  ";

                contout << " @:";
                if ( cache && cache->hasNewCode() )
                    contout << cc::debug( " $" ) << toHex( cache->code() );
                else
                    contout << cc::debug( " $" ) << ( cache ? cache->codeHash() : dev::h256( 0 ) );

                for ( auto const& j : mem )
                    if ( j.second )
                        contout << std::endl
                                << ( delta.count( j.first ) ?
                                           back.count( j.first ) ? " *     " : " +     " :
                                           cached.count( j.first ) ? " .     " : "       " )
                                << std::hex << nouppercase << std::setw( 64 ) << j.first << ": "
                                << std::setw( 0 ) << j.second;
                    else
                        contout << std::endl
                                << cc::debug( "XXX    " ) << std::hex << nouppercase
                                << std::setw( 64 ) << j.first << "";
            } else
                contout << cc::debug( " [SIMPLE]" );
            _out << lead << i << cc::debug( ": " ) << std::dec
                 << ( cache ? cache->nonce() : u256( 0 ) ) << cc::debug( " #:" )
                 << ( cache ? cache->balance() : u256( 0 ) ) << contout.str() << std::endl;
        }
    }
    return _out;
}
