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
/** @file BlockChain.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "BlockChain.h"

#include <memory>
#include <thread>

#include <boost/exception/errinfo_nested_exception.hpp>
#include <boost/filesystem.hpp>

#include <libdevcore/Assertions.h>
#include <libdevcore/Common.h>

// #include <libdevcore/DBImpl.h>
#include <libdevcore/ManuallyRotatingLevelDB.h>

#include <libdevcore/FileSystem.h>
#include <libdevcore/FixedHash.h>
#include <libdevcore/RLP.h>
#include <libdevcore/TrieHash.h>
#include <libdevcore/microprofile.h>
#include <libethashseal/Ethash.h>
#include <libethcore/BlockHeader.h>
#include <libethcore/Exceptions.h>

#include <libskale/AmsterdamFixPatch.h>
#include <libskale/SkipInvalidTransactionsPatch.h>
#include <libskale/TotalStorageUsedPatch.h>

#include "Block.h"
#include "Defaults.h"
#include "GenesisInfo.h"
#include "ImportPerformanceLogger.h"

#include <skutils/console_colors.h>

extern void dump_blocks_and_extras_db( const dev::eth::BlockChain& _bc, size_t _startBlock );

using namespace std;
using namespace dev;
using namespace dev::eth;
namespace fs = boost::filesystem;
using skale::BaseState;
using skale::State;
using namespace skale::error;

extern bytesConstRef bytesRefFromTransactionRlp( const RLP& _rlp );

#define ETH_TIMED_IMPORTS 1

namespace {
std::string const c_chainStart{ "chainStart" };
db::Slice const c_sliceChainStart{ c_chainStart };
}  // namespace

std::ostream& dev::eth::operator<<( std::ostream& _out, BlockChain const& _bc ) {
    string cmp = toBigEndianString( _bc.currentHash() );
    _bc.m_blocksDB->forEach( [&_out, &cmp]( db::Slice const& _key, db::Slice const& _value ) {
        if ( string( _key.data(), _key.size() ) != "best" ) {
            const string key( _key.data(), _key.size() );
            try {
                BlockHeader d( bytesConstRef{ _value } );
                _out << toHex( key ) << ":   " << d.number() << " @ " << d.parentHash()
                     << ( cmp == key ? "  BEST" : "" ) << std::endl;
            } catch ( ... ) {
                cwarn << "Invalid DB entry:" << toHex( key ) << " -> "
                      << toHex( bytesConstRef( _value ) );
            }
        }
        return true;
    } );
    return _out;
}

db::Slice dev::eth::toSlice( h256 const& _h, unsigned _sub ) {
    MICROPROFILE_SCOPEI( "BlockChain", "toSlice", MP_MAGENTA3 );

#if ALL_COMPILERS_ARE_CPP11_COMPLIANT
    static thread_local FixedHash< 33 > h = _h;
    h[32] = ( uint8_t ) _sub;
    return ( db::Slice ) h.ref();
#else
    static thread_local FixedHash< 33 > t_h;
    t_h = FixedHash< 33 >( _h );
    t_h[32] = ( uint8_t ) _sub;
    return ( db::Slice ) t_h.ref();
#endif  // ALL_COMPILERS_ARE_CPP11_COMPLIANT
}

db::Slice dev::eth::toSlice( uint64_t _n, unsigned _sub ) {
#if ALL_COMPILERS_ARE_CPP11_COMPLIANT
    static thread_local FixedHash< 33 > h;
    toBigEndian( _n, bytesRef( h.data() + 24, 8 ) );
    h[32] = ( uint8_t ) _sub;
    return ( db::Slice ) h.ref();
#else
    static thread_local FixedHash< 33 > t_h;
    bytesRef ref( t_h.data() + 24, 8 );
    toBigEndian( _n, ref );
    t_h[32] = ( uint8_t ) _sub;
    return ( db::Slice ) t_h.ref();
#endif
}

namespace {
class LastBlockHashes : public LastBlockHashesFace {
public:
    explicit LastBlockHashes( BlockChain const& _bc ) : m_bc( _bc ) {}

    h256s precedingHashes( h256 const& _mostRecentHash ) const override {
        Guard l( m_lastHashesMutex );
        if ( m_lastHashes.empty() || m_lastHashes.front() != _mostRecentHash ) {
            m_lastHashes.resize( 256 );
            m_lastHashes[0] = _mostRecentHash;
            for ( unsigned i = 0; i < 255; ++i )
                m_lastHashes[i + 1] =
                    m_lastHashes[i] ? m_bc.info( m_lastHashes[i] ).parentHash() : h256();
        }
        return m_lastHashes;
    }

    void clear() override {
        Guard l( m_lastHashesMutex );
        m_lastHashes.clear();
    }

private:
    BlockChain const& m_bc;

    mutable Mutex m_lastHashesMutex;
    mutable h256s m_lastHashes;
};

void addBlockInfo( Exception& io_ex, BlockHeader const& _header, bytes&& _blockData ) {
    io_ex << errinfo_now( time( 0 ) );
    io_ex << errinfo_block( std::move( _blockData ) );
    // only populate extraData if we actually managed to extract it. otherwise,
    // we might be clobbering the existing one.
    if ( !_header.extraData().empty() )
        io_ex << errinfo_extraData( _header.extraData() );
}

}  // namespace


/// Duration between flushes.
chrono::system_clock::duration c_collectionDuration = chrono::seconds( 60 );

/// Length of death row (total time in cache is multiple of this and collection duration).
unsigned c_collectionQueueSize = 20;

/// Max size, above which we start forcing cache reduction.
unsigned c_maxCacheSize = 1024 * 1024 * 64;

/// Min size, below which we don't bother flushing it.
unsigned c_minCacheSize = 1024 * 1024 * 32;

string BlockChain::getChainDirName( const ChainParams& _cp ) {
    return toHex( BlockHeader( _cp.genesisBlock() ).hash().ref().cropped( 0, 4 ) );
}

BlockChain::BlockChain(
    ChainParams const& _p, fs::path const& _dbPath, bool _applyPatches, WithExisting _we ) try
    : m_lastBlockHashes( new LastBlockHashes( *this ) ), m_dbPath( _dbPath ) {
    init( _p );
    open( _dbPath, _applyPatches, _we );
} catch ( ... ) {
    std::throw_with_nested( CreationException() );
}

BlockChain::~BlockChain() {
    close();
}

BlockHeader const& BlockChain::genesis() const {
    UpgradableGuard l( x_genesis );
    if ( !m_genesis ) {
        auto gb = m_params.genesisBlock();
        UpgradeGuard ul( l );
        m_genesis = BlockHeader( gb );
        m_genesisHeaderBytes = BlockHeader::extractHeader( &gb ).data().toBytes();
        m_genesisHash = m_genesis.hash();
    }
    return m_genesis;
}

void BlockChain::init( ChainParams const& _p ) {
    clockLastDbRotation_ = clock();
    // initialise deathrow.
    m_cacheUsage.resize( c_collectionQueueSize );
    m_lastCollection = chrono::system_clock::now();

    // Initialise with the genesis as the last block on the longest chain.
    m_params = _p;
    m_sealEngine.reset( m_params.createSealEngine() );
    m_genesis.clear();
    genesis();
}

void BlockChain::open( fs::path const& _path, bool _applyPatches, WithExisting _we ) {
    fs::path path = _path.empty() ? Defaults::get()->m_dbPath : _path;
    fs::path chainPath = path / getChainDirName( m_params );
    fs::path extrasPath = chainPath / fs::path( toString( c_databaseVersion ) );

    fs::create_directories( extrasPath );
    DEV_IGNORE_EXCEPTIONS( fs::permissions( extrasPath, fs::owner_all ) );

    if ( _we == WithExisting::Kill ) {
        cnote << "Killing blockchain & extras database (WithExisting::Kill).";
        fs::remove_all( chainPath / fs::path( "blocks_and_extras" ) );
    }

    try {
        fs::create_directories( chainPath / fs::path( "blocks_and_extras" ) );
        m_rotator = std::make_shared< batched_io::rotating_db_io >(
            chainPath / fs::path( "blocks_and_extras" ), 5, chainParams().nodeInfo.archiveMode );
        m_rotating_db = std::make_shared< db::ManuallyRotatingLevelDB >( m_rotator );
        auto db = std::make_shared< batched_io::batched_db >();
        db->open( m_rotating_db );
        m_db = db;
        m_db_splitter = std::make_unique< batched_io::db_splitter >( m_db );
        m_blocksDB = m_db_splitter->new_interface();
        m_extrasDB = m_db_splitter->new_interface();
        // m_blocksDB.reset( new db::DBImpl( chainPath / fs::path( "blocks" ) ) );
        // m_extrasDB.reset( new db::DBImpl( extrasPath / fs::path( "extras" ) ) );
    } catch ( db::DatabaseError const& ex ) {
        // Check the exact reason of errror, in case of IOError we can display user-friendly message
        if ( *boost::get_error_info< db::errinfo_dbStatusCode >( ex ) !=
             db::DatabaseStatus::IOError )
            throw;

        if ( fs::space( chainPath / fs::path( "blocks_and_extras" ) ).available < 1024 ) {
            cwarn << "Not enough available space found on hard drive. Please free some up and then "
                     "re-run. Bailing.";
            BOOST_THROW_EXCEPTION( NotEnoughAvailableSpace() );
        } else {
            cwarn << "Database " << ( chainPath / fs::path( "blocks_and_extras" ) ) << "or "
                  << ( extrasPath / fs::path( "extras" ) )
                  << "already open. You appear to have another instance of ethereum running. "
                     "Bailing.";
            BOOST_THROW_EXCEPTION( DatabaseAlreadyOpen() );
        }
    }

    if ( _applyPatches && AmsterdamFixPatch::isInitOnChainNeeded( *m_blocksDB, *m_extrasDB ) )
        AmsterdamFixPatch::initOnChain( *m_blocksDB, *m_extrasDB, *m_db, chainParams() );

    if ( _we != WithExisting::Verify && !details( m_genesisHash ) ) {
        BlockHeader gb( m_params.genesisBlock() );
        // Insert details of genesis block.
        bytes const& genesisBlockBytes = m_params.genesisBlock();
        BlockDetails details( 0, gb.difficulty(), h256(), {}, genesisBlockBytes.size() );
        auto r = details.rlp();
        details.size = r.size();
        m_details[m_genesisHash] = details;
        m_extrasDB->insert( toSlice( m_genesisHash, ExtraDetails ), ( db::Slice ) dev::ref( r ) );
        assert( isKnown( gb.hash() ) );
        m_db->commit( "insert_genesis" );
    }

#if ETH_PARANOIA
    checkConsistency();
#endif

    // HACK Unfortunate crash can leave us with rotated DB but not added pieceUsageBytes, best and
    // genesis! So, finish possibly unfinished rotation ( though better to do it in batched_*
    // classes :( )
    if ( m_params.sChain.dbStorageLimit > 0 &&
         !m_rotating_db->currentPiece()->exists( ( db::Slice ) "pieceUsageBytes" ) ) {
        // re-insert genesis
        BlockDetails details = this->details( m_genesisHash );
        auto r = details.rlp();
        m_details[m_genesisHash] = details;
        m_extrasDB->insert( toSlice( m_genesisHash, ExtraDetails ), ( db::Slice ) dev::ref( r ) );
        // update storage usage
        m_db->insert( db::Slice( "pieceUsageBytes" ), db::Slice( "0" ) );
        m_db->commit( "fix_bad_rotation" );
    }  // if

    // TODO: Implement ability to rebuild details map from DB.
    auto const l = m_extrasDB->lookup( db::Slice( "best" ) );
    m_lastBlockHash = l.empty() ? m_genesisHash : h256( l, h256::FromBinary );

    m_lastBlockNumber = number( m_lastBlockHash );

    cdebug << cc::info( "Opened blockchain DB. Latest: " ) << currentHash() << ' '
           << m_lastBlockNumber;

    //    dump_blocks_and_extras_db( *this, 0 );

    if ( _applyPatches && TotalStorageUsedPatch::isInitOnChainNeeded( *m_db ) )
        TotalStorageUsedPatch::initOnChain( *this );
}

void BlockChain::reopen( ChainParams const& _p, bool _applyPatches, WithExisting _we ) {
    close();
    init( _p );
    open( m_dbPath, _applyPatches, _we );
}

void BlockChain::close() {
    ctrace << "Closing blockchain DB";
    // Not thread safe...
    m_extrasDB = nullptr;
    m_blocksDB = nullptr;
    m_db_splitter.reset();
    m_db.reset();
    m_rotating_db.reset();

    DEV_WRITE_GUARDED( x_lastBlockHash ) {
        m_lastBlockHash = m_genesisHash;
        m_lastBlockNumber = 0;
    }

    clearCaches();

    m_lastBlockHashes->clear();
}

std::pair< h256, unsigned > BlockChain::transactionLocation( h256 const& _transactionHash ) const {
    // cached transactionAddresses for transactions with gasUsed==0 should be re-queried from DB
    bool cached = false;
    {
        ReadGuard g( x_transactionAddresses );
        cached = m_transactionAddresses.count( _transactionHash ) > 0;
    }

    // get transactionAddresses from DB or cache
    TransactionAddress ta = queryExtras< TransactionAddress, ExtraTransactionAddress >(
        _transactionHash, m_transactionAddresses, x_transactionAddresses, NullTransactionAddress );

    if ( !ta )
        return std::pair< h256, unsigned >( h256(), 0 );

    auto blockNumber = this->number( ta.blockHash );

    if ( !SkipInvalidTransactionsPatch::hasPotentialInvalidTransactionsInBlock(
             blockNumber, *this ) )
        return std::make_pair( ta.blockHash, ta.index );

    // rest is for blocks with possibility of invalid transactions

    // compute gas used
    TransactionReceipt receipt = transactionReceipt( ta.blockHash, ta.index );
    u256 cumulativeGasUsed = receipt.cumulativeGasUsed();
    u256 prevGasUsed =
        ta.index == 0 ? 0 : transactionReceipt( ta.blockHash, ta.index - 1 ).cumulativeGasUsed();
    u256 gasUsed = cumulativeGasUsed - prevGasUsed;

    // re-query receipt from DB if gasUsed==0 (and cache might have wrong value)
    if ( gasUsed == 0 && cached ) {
        // remove from cache
        {
            WriteGuard g( x_transactionAddresses );
            m_transactionAddresses.erase( _transactionHash );
        }
        // re-read from DB
        ta = queryExtras< TransactionAddress, ExtraTransactionAddress >( _transactionHash,
            m_transactionAddresses, x_transactionAddresses, NullTransactionAddress );
    }
    return std::make_pair( ta.blockHash, ta.index );
}

string BlockChain::dumpDatabase() const {
    ostringstream oss;
    oss << m_lastBlockHash << '\n';
    std::map< string, string > sorted;
    m_extrasDB->forEach( [&sorted]( db::Slice key, db::Slice value ) {
        // give priority to 1-st occurrence
        if ( sorted.count( toHex( key ) ) == 0 )
            sorted[toHex( key )] = toHex( value );
        return true;
    } );

    for ( const auto& p : sorted ) {
        oss << p.first << "/" << p.second << '\n';
    }

    return oss.str();
}

tuple< ImportRoute, bool, unsigned > BlockChain::sync(
    BlockQueue& _bq, State& _state, unsigned _max ) {
    MICROPROFILE_SCOPEI( "BlockChain", "sync many blocks", MP_LIGHTGOLDENROD );

    //  _bq.tick(*this);

    VerifiedBlocks blocks;
    _bq.drain( blocks, _max );

    h256s fresh;
    h256s dead;
    h256s badBlocks;
    Transactions goodTransactions;
    unsigned count = 0;
    for ( VerifiedBlock const& block : blocks ) {
        do {
            try {
                // Nonce & uncle nonces already verified in verification thread at this point.
                ImportRoute r;
                DEV_TIMED_ABOVE( "Block import " + toString( block.verified.info.number() ), 500 )
                r = import( block.verified, _state,
                    ( ImportRequirements::Everything & ~ImportRequirements::ValidSeal &
                        ~ImportRequirements::CheckUncles ) != 0 );
                fresh += r.liveBlocks;
                dead += r.deadBlocks;
                goodTransactions.reserve( goodTransactions.size() + r.goodTranactions.size() );
                std::move( std::begin( r.goodTranactions ), std::end( r.goodTranactions ),
                    std::back_inserter( goodTransactions ) );
                ++count;
            } catch ( dev::eth::AlreadyHaveBlock const& ) {
                cwarn << "ODD: Import queue contains already imported block";
                continue;
            } catch ( dev::eth::UnknownParent const& ) {
                cwarn << "ODD: Import queue contains block with unknown parent.";  // <<
                                                                                   // LogTag::Error
                // <<
                // boost::current_exception_diagnostic_information();
                // NOTE: don't reimport since the queue should guarantee everything in the right
                // order. Can't continue - chain bad.
                badBlocks.push_back( block.verified.info.hash() );
            } catch ( dev::eth::FutureTime const& ) {
                cwarn << "ODD: Import queue contains a block with future time.";
                this_thread::sleep_for( chrono::seconds( 1 ) );
                continue;
            } catch ( dev::eth::TransientError const& ) {
                this_thread::sleep_for( chrono::milliseconds( 100 ) );
                continue;
            } catch ( Exception& ex ) {
                cerror << "Exception while importing block. Someone (Jeff? That you?) seems to be "
                       << "giving us dodgy blocks !";
                cerror << diagnostic_information( ex );
                if ( m_onBad )
                    m_onBad( ex );
                // NOTE: don't reimport since the queue should guarantee everything in the right
                // order. Can't continue - chain  bad.
                badBlocks.push_back( block.verified.info.hash() );
            }
        } while ( false );
    }
    return make_tuple(
        ImportRoute{ dead, fresh, goodTransactions }, _bq.doneDrain( badBlocks ), count );
}

pair< ImportResult, ImportRoute > BlockChain::attemptImport(
    bytes const& _block, State& _state, bool _mustBeNew ) noexcept {
    try {
        return make_pair( ImportResult::Success,
            import( verifyBlock( &_block, m_onBad, ImportRequirements::OutOfOrderChecks ), _state,
                _mustBeNew ) );
    } catch ( UnknownParent& ) {
        return make_pair( ImportResult::UnknownParent, ImportRoute() );
    } catch ( AlreadyHaveBlock& ) {
        return make_pair( ImportResult::AlreadyKnown, ImportRoute() );
    } catch ( FutureTime& ) {
        return make_pair( ImportResult::FutureTimeKnown, ImportRoute() );
    } catch ( Exception& ex ) {
        if ( m_onBad )
            m_onBad( ex );
        return make_pair( ImportResult::Malformed, ImportRoute() );
    }
}

ImportRoute BlockChain::import( bytes const& _block, State& _state, bool _mustBeNew ) {
    // VERIFY: populates from the block and checks the block is internally coherent.
    VerifiedBlockRef const block =
        verifyBlock( &_block, m_onBad, ImportRequirements::OutOfOrderChecks );
    //    cerr << "Import block #" << block.info.number() << " with hash = " << block.info.hash() <<
    //    endl;
    return import( block, _state, _mustBeNew );
}

ImportRoute BlockChain::import( VerifiedBlockRef const& _block, State& _state, bool _mustBeNew ) {
    //@tidy This is a behemoth of a method - could do to be split into a few smaller ones.
    MICROPROFILE_SCOPEI( "BlockChain", "import", MP_GREENYELLOW );

    ImportPerformanceLogger performanceLogger;

    // Check block doesn't already exist first!
    if ( _mustBeNew )
        checkBlockIsNew( _block );

    // Work out its number as the parent's number + 1
    if ( !isKnown( _block.info.parentHash(), false ) )  // doesn't have to be current.
    {
        LOG( m_logger ) << _block.info.hash() << " : Unknown parent " << _block.info.parentHash();
        // We don't know the parent (yet) - discard for now. It'll get resent to us if we find out
        // about its ancestry later on.
        BOOST_THROW_EXCEPTION( UnknownParent() << errinfo_hash256( _block.info.parentHash() ) );
    }

    auto pd = details( _block.info.parentHash() );
    if ( !pd ) {
        auto pdata = pd.rlp();
        LOG( m_loggerError ) << "Details is returning false despite block known: " << RLP( pdata );
        auto parentBlock = block( _block.info.parentHash() );
        LOG( m_loggerError ) << "isKnown: " << isKnown( _block.info.parentHash() );
        LOG( m_loggerError ) << "last/number: " << m_lastBlockNumber << " " << m_lastBlockHash
                             << " " << _block.info.number();
        LOG( m_loggerError ) << "Block: " << BlockHeader( &parentBlock );
        LOG( m_loggerError ) << "RLP: " << RLP( parentBlock );
        LOG( m_loggerError ) << "DATABASE CORRUPTION: CRITICAL FAILURE";
        cerror << DETAILED_ERROR;
        exit( -1 );
    }

    checkBlockTimestamp( _block.info );

    // Verify parent-critical parts
    verifyBlock( _block.block, m_onBad, ImportRequirements::InOrderChecks );

    LOG( m_loggerDetail ) << "Attempting import of " << _block.info.hash() << " ...";

    performanceLogger.onStageFinished( "preliminaryChecks" );

    MICROPROFILE_ENTERI( "BlockChain", "enact", MP_INDIANRED );

    BlockReceipts blockReceipts;
    u256 totalDifficulty;
    try {
        // Check transactions are valid and that they result in a state equivalent to our
        // state_root. Get total difficulty increase and update state, checking it.
        Block s( *this, m_lastBlockHash, _state );
        auto tdIncrease = s.enactOn( _block, *this );

        for ( unsigned i = 0; i < s.pending().size(); ++i )
            blockReceipts.receipts.push_back( s.receipt( i ) );

        s.cleanup();

        _state = _state.createNewCopyWithLocks();

        totalDifficulty = pd.totalDifficulty + tdIncrease;

        performanceLogger.onStageFinished( "enactment" );

#if ETH_PARANOIA
        checkConsistency();
#endif  // ETH_PARANOIA
    } catch ( BadRoot& ex ) {
        cwarn << "*** BadRoot error! Trying to import" << _block.info.hash() << "needed root"
              << *boost::get_error_info< errinfo_hash256 >( ex );
        cwarn << _block.info;
        // Attempt in import later.
        BOOST_THROW_EXCEPTION( TransientError() );
    } catch ( Exception& ex ) {
        addBlockInfo( ex, _block.info, _block.block.toBytes() );
        throw;
    }

    MICROPROFILE_LEAVE();

    //
    // l_sergiy:
    //
    // We need to compute log blooms directly here without using Block::logBloom()
    // method because _receipts may contain extra receipt items corresponding to
    // partially caught-up transactions
    //
    // normally it's performed like: // LogBloom blockBloom = tbi.logBloom();
    //
    LogBloom blockBloomFull;
    for ( const TransactionReceipt& trWalk : blockReceipts.receipts )
        blockBloomFull |= trWalk.bloom();

    // All ok - insert into DB
    bytes const receipts = blockReceipts.rlp();
    return insertBlockAndExtras(
        _block, ref( receipts ), &blockBloomFull, totalDifficulty, performanceLogger );
}

ImportRoute BlockChain::import( const Block& _block ) {
    assert( _block.isSealed() );

    VerifiedBlockRef verifiedBlock;
    verifiedBlock.info = _block.info();
    verifiedBlock.block = ref( _block.blockData() );
    verifiedBlock.transactions = _block.pending();
    //    verifyBlock( ref( _block.blockData() ), m_onBad, ImportRequirements::OutOfOrderChecks );

    BlockReceipts blockReceipts;
    for ( unsigned i = 0; i < _block.pending().size(); ++i )
        blockReceipts.receipts.push_back( _block.receipt( i ) );
    bytes const receipts = blockReceipts.rlp();

    //
    // l_sergiy:
    //
    // We need to compute log blooms directly here without using Block::logBloom()
    // method because _receipts may contain extra receipt items corresponding to
    // partially cought-up transactions
    //
    // normally it's performed like: // LogBloom blockBloom = tbi.logBloom();
    //
    LogBloom blockBloomFull;
    for ( const TransactionReceipt& trWalk : blockReceipts.receipts )
        blockBloomFull |= trWalk.bloom();

    ImportPerformanceLogger performanceLogger;

    return insertBlockAndExtras( verifiedBlock, ref( receipts ), &blockBloomFull,
        _block.info().difficulty(), performanceLogger );
}

void BlockChain::checkBlockIsNew( VerifiedBlockRef const& _block ) const {
    if ( isKnown( _block.info.hash() ) ) {
        LOG( m_logger ) << _block.info.hash() << " : Not new.";
        BOOST_THROW_EXCEPTION( AlreadyHaveBlock() << errinfo_block( _block.block.toBytes() ) );
    }
}

void BlockChain::checkBlockTimestamp( BlockHeader const& _header ) const {
    // Check it's not crazy
    if ( _header.timestamp() > utcTime() && !m_params.allowFutureBlocks ) {
        LOG( m_loggerDetail ) << _header.hash() << " : Future time " << _header.timestamp()
                              << " (now at " << utcTime() << ")";
        // Block has a timestamp in the future. This is no good.
        BOOST_THROW_EXCEPTION( FutureTime() );
    }
}

bool BlockChain::rotateDBIfNeeded( uint64_t pieceUsageBytes ) {
    bool isRotate = false;
    if ( m_params.sChain.dbStorageLimit > 0 ) {
        // account for size of 1 piece
        isRotate =
            ( pieceUsageBytes > m_params.sChain.dbStorageLimit / m_rotating_db->piecesCount() ) ?
                true :
                false;
        if ( isRotate ) {
            clog( VerbosityTrace, "BlockChain" )
                << ( cc::debug( "Will perform " ) + cc::notice( "storage-based block rotation" ) );
        }
    }
    if ( clockLastDbRotation_ == 0 )
        clockLastDbRotation_ = clock();
    if ( ( !isRotate ) && clockDbRotationPeriod_ > 0 ) {
        // if time period based DB rotation is enabled
        clock_t clockNow = clock();
        if ( ( clockNow - clockLastDbRotation_ ) >= clockDbRotationPeriod_ ) {
            isRotate = true;
            clog( VerbosityTrace, "BlockChain" )
                << ( cc::debug( "Will perform " ) + cc::notice( "timer-based block rotation" ) );
        }
    }
    if ( !isRotate )
        return false;

    clockLastDbRotation_ = clock();
    // remember genesis
    BlockDetails details = this->details( m_genesisHash );

    clearCaches();
    m_db->revert();  // cancel pending changes
    m_rotating_db->rotate();

    // re-insert genesis
    auto r = details.rlp();
    m_details[m_genesisHash] = details;
    m_extrasDB->insert( toSlice( m_genesisHash, ExtraDetails ), ( db::Slice ) dev::ref( r ) );
    m_db->commit( "genesis_after_rotate" );

    batched_io::test_crash_before_commit( "after_genesis_after_rotate" );

    return true;
}

struct DbWriteProxy {
    DbWriteProxy( batched_io::db_operations_face& _backend ) : backend( _backend ) {}
    // HACK +1 is needed for SplitDB; of course, this should be redesigned!
    void insert( db::Slice _key, db::Slice _value ) {
        consumedBytes += _key.size() + _value.size() + 1;
        backend.insert( _key, _value );
    }
    batched_io::db_operations_face& backend;
    size_t consumedBytes = 0;
};

// TOOD ACHTUNG This function must be kept in sync with the next one!
size_t BlockChain::prepareDbDataAndReturnSize( VerifiedBlockRef const& _block,
    bytesConstRef _receipts, u256 const& _totalDifficulty, const LogBloom* pLogBloomFull,
    ImportPerformanceLogger& _performanceLogger ) {
    DbWriteProxy blocksWriteBatch( *m_blocksDB );
    DbWriteProxy extrasWriteBatch( *m_extrasDB );

    try {
        MICROPROFILE_SCOPEI( "BlockChain", "write", MP_DARKKHAKI );

        blocksWriteBatch.insert( toSlice( _block.info.hash() ), db::Slice( _block.block ) );

        // ensure parent is cached for later addition.
        // TODO: this is a bit horrible would be better refactored into an enveloping
        // UpgradableGuard together with an "ensureCachedWithUpdatableLock(l)" method. This is
        // safe in practice since the caches don't get flushed nearly often enough to be done
        // here.
        details( _block.info.parentHash() );
        DEV_WRITE_GUARDED( x_details ) {
            m_details[_block.info.parentHash()].children.clear();
            m_details[_block.info.parentHash()].children.push_back( _block.info.hash() );
            extrasWriteBatch.insert( toSlice( _block.info.parentHash(), ExtraDetails ),
                ( db::Slice ) dev::ref( m_details[_block.info.parentHash()].rlp() ) );
        }

        BlockDetails details( ( unsigned ) _block.info.number(), _totalDifficulty,
            _block.info.parentHash(), {}, _block.block.size() );
        bytes details_rlp = details.rlp();
        details.size = details_rlp.size();
        extrasWriteBatch.insert(
            toSlice( _block.info.hash(), ExtraDetails ), ( db::Slice ) dev::ref( details_rlp ) );

        BlockLogBlooms blb;
        for ( auto i : RLP( _receipts ) )
            blb.blooms.push_back( TransactionReceipt( i.data() ).bloom() );
        extrasWriteBatch.insert(
            toSlice( _block.info.hash(), ExtraLogBlooms ), ( db::Slice ) dev::ref( blb.rlp() ) );

        extrasWriteBatch.insert(
            toSlice( _block.info.hash(), ExtraReceipts ), ( db::Slice ) _receipts );

        _performanceLogger.onStageFinished( "writing" );
    } catch ( Exception& ex ) {
        addBlockInfo( ex, _block.info, _block.block.toBytes() );
        throw;
    }

    MICROPROFILE_SCOPEI( "insertBlockAndExtras", "difficulty", MP_HOTPINK );

    BlockHeader tbi = _block.info;

    // Collate transaction hashes and remember who they were.
    // h256s newTransactionAddresses;
    {
        MICROPROFILE_SCOPEI( "insertBlockAndExtras", "collate_txns", MP_LAVENDERBLUSH );

        RLP blockRLP( _block.block );
        TransactionAddress ta;
        ta.blockHash = tbi.hash();
        ta.index = 0;

        RLP txns_rlp = blockRLP[1];

        for ( RLP::iterator it = txns_rlp.begin(); it != txns_rlp.end(); ++it ) {
            MICROPROFILE_SCOPEI( "insertBlockAndExtras", "for2", MP_HONEYDEW );

            auto txBytes = bytesRefFromTransactionRlp( *it );
            extrasWriteBatch.insert( toSlice( sha3( txBytes ), ExtraTransactionAddress ),
                ( db::Slice ) dev::ref( ta.rlp() ) );

            ++ta.index;
        }
    }

    // Collate logs into blooms.
    h256s alteredBlooms;
    {
        MICROPROFILE_SCOPEI( "insertBlockAndExtras", "collate_logs", MP_PALETURQUOISE );

        //
        // l_sergiy:
        //
        // We need to compute log blooms directly here without using Block::logBloom()
        // method because _receipts may contain extra receipt items corresponding to
        // partially cought-up transactions
        //
        // old code was: // LogBloom blockBloom = tbi.logBloom();
        //
        LogBloom blockBloom = pLogBloomFull ? ( *pLogBloomFull ) : tbi.logBloom();
        //
        //

        blockBloom.shiftBloom< 3 >( sha3( tbi.author().ref() ) );

        // Pre-memoize everything we need before locking x_blocksBlooms
        for ( unsigned level = 0, index = ( unsigned ) tbi.number(); level < c_bloomIndexLevels;
              level++, index /= c_bloomIndexSize )
            blocksBlooms( chunkId( level, index / c_bloomIndexSize ) );

        WriteGuard l( x_blocksBlooms );
        for ( unsigned level = 0, index = ( unsigned ) tbi.number(); level < c_bloomIndexLevels;
              level++, index /= c_bloomIndexSize ) {
            unsigned i = index / c_bloomIndexSize;
            unsigned o = index % c_bloomIndexSize;
            alteredBlooms.push_back( chunkId( level, i ) );
            m_blocksBlooms[alteredBlooms.back()].blooms[o] |= blockBloom;
        }
    }

    for ( auto const& h : alteredBlooms )
        noteUsed( h, ExtraBlocksBlooms );

    // Update database with them.
    // ReadGuard l1( x_blocksBlooms );
    WriteGuard l1( x_blocksBlooms );
    {
        MICROPROFILE_SCOPEI( "insertBlockAndExtras", "insert_to_extras", MP_LIGHTSKYBLUE );

        for ( auto const& h : alteredBlooms )
            extrasWriteBatch.insert( toSlice( h, ExtraBlocksBlooms ),
                ( db::Slice ) dev::ref( m_blocksBlooms[h].rlp() ) );
        extrasWriteBatch.insert( toSlice( h256( tbi.number() ), ExtraBlockHash ),
            ( db::Slice ) dev::ref( BlockHash( tbi.hash() ).rlp() ) );
    }

    size_t writeSize = blocksWriteBatch.consumedBytes + extrasWriteBatch.consumedBytes;

    // HACK Since blooms are often re-used, let's adjust size for them
    writeSize -= ( 4147 + 34 ) * 2;                             // 2 big blooms altered by block
    writeSize += ( 4147 + 34 ) / 16 + ( 4147 + 34 ) / 256 + 2;  // 1+1/16th big bloom per block

    return writeSize;
}

// TOOD ACHTUNG This function must be kept in sync with prepareDbDataAndReturnSize defined above!!
// TODO move it to TotalStorageUsedPatch!
void BlockChain::recomputeExistingOccupiedSpaceForBlockRotation() try {
    unsigned number = this->number();

    size_t blocksBatchSize = 0;
    size_t extrasBatchSize = 0;

    LOG( m_logger ) << "Recomputing old blocks sizes...";

    // HACK 34 is key size + extra size + db prefix (blocks or extras)
    for ( unsigned i = 1; i <= number; ++i ) {
        h256 hash = this->numberHash( i );
        BlockHeader header = this->info( hash );

        // blocksWriteBatch.insert( toSlice( _block.info.hash() ), db::Slice( _block.block ) );
        blocksBatchSize += 34 + this->block( hash ).size();

        // extrasWriteBatch.insert( toSlice( _block.info.parentHash(), ExtraDetails ),
        //    ( db::Slice ) dev::ref( m_details[_block.info.parentHash()].rlp() ) );
        extrasBatchSize += 34 + this->details( header.parentHash() ).rlp().size();

        // extrasWriteBatch.insert(
        //    toSlice( _block.info.hash(), ExtraDetails ), ( db::Slice ) dev::ref( details_rlp ) );
        // HACK on insertion this field was empty - so we do it here!
        BlockDetails details = this->details( hash );
        details.children.resize( 0 );
        extrasBatchSize += 34 + details.rlp().size();

        // extrasWriteBatch.insert(
        //    toSlice( _block.info.hash(), ExtraLogBlooms ), ( db::Slice ) dev::ref( blb.rlp() ) );
        extrasBatchSize += 34 + this->logBlooms( hash ).rlp().size();

        // extrasWriteBatch.insert(
        //    toSlice( _block.info.hash(), ExtraReceipts ), ( db::Slice ) _receipts );
        extrasBatchSize += 34 + this->receipts( hash ).rlp().size();

        //        for ( RLP::iterator it = txns_rlp.begin(); it != txns_rlp.end(); ++it ) {
        //            extrasWriteBatch.insert( toSlice( sha3( ( *it ).data() ),
        //            ExtraTransactionAddress ),
        //                ( db::Slice ) dev::ref( ta.rlp() ) );
        //            ++ta.index;
        //        }
        h256s tx_hashes = this->transactionHashes( hash );
        for ( auto th : tx_hashes ) {
            std::pair< h256, unsigned > pair = this->transactionLocation( th );
            assert( pair.first == hash );
            TransactionAddress ta;
            ta.blockHash = pair.first;
            ta.index = pair.second;
            extrasBatchSize += 34 + ta.rlp().size();
        }

        //        for ( auto const& h : alteredBlooms )
        //            extrasWriteBatch.insert( toSlice( h, ExtraBlocksBlooms ),
        //                ( db::Slice ) dev::ref( m_blocksBlooms[h].rlp() ) );
        // blooms are always constant!!: 34+4147

        // extrasWriteBatch.insert( toSlice( h256( tbi.number() ), ExtraBlockHash ),
        //    ( db::Slice ) dev::ref( BlockHash( tbi.hash() ).rlp() ) );
        extrasBatchSize += 34 + BlockHash( hash ).rlp().size();

        // HACK Since blooms are often re-used, let's adjust size for them
        extrasBatchSize +=
            ( 4147 + 34 ) / 16 + ( 4147 + 34 ) / 256 + 2;  // 1+1/16th big bloom per block
        LOG( m_loggerDetail ) << "Computed block " << i
                              << " DB usage = " << blocksBatchSize + extrasBatchSize;
    }  // for block

    uint64_t pieceUsageBytes = 0;
    if ( this->m_db->exists( ( db::Slice ) "pieceUsageBytes" ) ) {
        pieceUsageBytes = std::stoull( this->m_db->lookup( ( db::Slice ) "pieceUsageBytes" ) );
    }

    LOG( m_logger ) << "pieceUsageBytes from DB = " << pieceUsageBytes
                    << " computed = " << blocksBatchSize + extrasBatchSize;

    if ( pieceUsageBytes == 0 ) {
        pieceUsageBytes = blocksBatchSize + extrasBatchSize;
        m_db->insert(
            db::Slice( "pieceUsageBytes" ), db::Slice( std::to_string( pieceUsageBytes ) ) );
        m_db->commit( "recompute_piece_usage" );
    } else {
        if ( pieceUsageBytes != blocksBatchSize + extrasBatchSize || true ) {
            LOG( m_loggerError ) << "Computed db usage value is not equal to stored one! This "
                                    "should happen only if block rotation has occured!";
        }
    }  // else
} catch ( const std::exception& ex ) {
    LOG( m_loggerError )
        << "Exception when recomputing old blocks sizes (but it's normal if DB has rotated): "
        << ex.what();
}

ImportRoute BlockChain::insertBlockAndExtras( VerifiedBlockRef const& _block,
    bytesConstRef _receipts, LogBloom* pLogBloomFull, u256 const& _totalDifficulty,
    ImportPerformanceLogger& _performanceLogger ) {
    MICROPROFILE_SCOPEI( "BlockChain", "insertBlockAndExtras", MP_YELLOWGREEN );

    // get "safeLastExecutedTransactionHash" value from state, for debug reasons only
    // dev::h256 shaLastTx = skale::OverlayDB::stat_safeLastExecutedTransactionHash( m_stateDB.get()
    // ); std::cout << "--- got \"safeLastExecutedTransactionHash\" = " << shaLastTx.hex() << "\n";
    // std::cout.flush();

    h256 newLastBlockHash = currentHash();
    unsigned newLastBlockNumber = number();
    BlockHeader tbi = _block.info;

    _performanceLogger.onStageFinished( "collation" );

    size_t writeSize = prepareDbDataAndReturnSize(
        _block, _receipts, _totalDifficulty, pLogBloomFull, _performanceLogger );

    uint64_t pieceUsageBytes = 0;
    if ( this->m_db->exists( ( db::Slice ) "pieceUsageBytes" ) ) {
        pieceUsageBytes = std::stoull( this->m_db->lookup( ( db::Slice ) "pieceUsageBytes" ) );
    }
    pieceUsageBytes += writeSize;

    LOG( m_loggerInfo ) << "Block " << tbi.number() << " DB usage is " << writeSize
                        << ". Piece DB usage is " << pieceUsageBytes << " bytes";

    // re-evaluate batches and reset total usage counter if rotated!
    if ( rotateDBIfNeeded( pieceUsageBytes ) ) {
        LOG( m_loggerInfo ) << "Rotated out some blocks";
        m_db->revert();
        writeSize = prepareDbDataAndReturnSize(
            _block, _receipts, _totalDifficulty, pLogBloomFull, _performanceLogger );
        pieceUsageBytes = writeSize;
        LOG( m_loggerInfo ) << "DB usage is " << pieceUsageBytes << " bytes";
    }

    // FINALLY! change our best hash.
    newLastBlockHash = _block.info.hash();
    newLastBlockNumber = ( unsigned ) _block.info.number();

    LOG( m_loggerDetail ) << cc::debug( "   Imported and best " ) << _totalDifficulty
                          << cc::debug( " (" ) << cc::warn( "#" )
                          << cc::num10( _block.info.number() ) << cc::debug( "). Has " )
                          << ( details( _block.info.parentHash() ).children.size() - 1 )
                          << cc::debug( " siblings." );

#if ETH_PARANOIA
    if ( isKnown( _block.info.hash() ) && !details( _block.info.hash() ) ) {
        LOG( m_loggerError ) << "Known block just inserted has no details.";
        LOG( m_loggerError ) << "Block: " << _block.info;
        LOG( m_loggerError ) << "DATABASE CORRUPTION: CRITICAL FAILURE";
        exit( -1 );
    }

    try {
        State canary( _db, BaseState::Empty );
        canary.populateFromChain( *this, _block.info.hash() );
    } catch ( ... ) {
        LOG( m_loggerError ) << "Failed to initialise State object form imported block.";
        LOG( m_loggerError ) << "Block: " << _block.info;
        LOG( m_loggerError ) << "DATABASE CORRUPTION: CRITICAL FAILURE";
        exit( -1 );
    }
#endif  // ETH_PARANOIA

    DEV_WRITE_GUARDED( x_lastBlockHash ) {
        MICROPROFILE_SCOPEI( "insertBlockAndExtras", "m_lastBlockHash", MP_LIGHTGOLDENROD );

        m_lastBlockHash = newLastBlockHash;
        m_lastBlockNumber = newLastBlockNumber;
        try {
            // update storage usage
            m_db->insert(
                db::Slice( "pieceUsageBytes" ), db::Slice( std::to_string( pieceUsageBytes ) ) );

            TotalStorageUsedPatch::onProgress( *m_db, _block.info.number() );

            m_db->insert( db::Slice( "\x1"
                                     "best" ),
                db::Slice( ( char const* ) &m_lastBlockHash, 32 ) );
            m_db->commit( "insertBlockAndExtras" );
        } catch ( boost::exception const& ex ) {
            cwarn << "Error writing to blocks_and_extras database: "
                  << boost::diagnostic_information( ex );
            cwarn << "Put" << toHex( bytesConstRef( db::Slice( "best" ) ) ) << "=>"
                  << toHex( bytesConstRef( db::Slice( ( char const* ) &m_lastBlockHash, 32 ) ) );
            cwarn << "Fail writing to blocks_and_extras database. Bombing out.";
            cerror << DETAILED_ERROR;
            exit( -1 );
        }
    }

#if ETH_PARANOIA
    checkConsistency();
#endif  // ETH_PARANOIA

    _performanceLogger.onStageFinished( "checkBest" );

    unsigned const gasPerSecond = static_cast< double >( _block.info.gasUsed() ) /
                                  _performanceLogger.stageDuration( "enactment" );
    _performanceLogger.onFinished( { { "blockHash", "\"" + _block.info.hash().abridged() + "\"" },
        { "blockNumber", toString( _block.info.number() ) },
        { "gasPerSecond", toString( gasPerSecond ) },
        { "transactions", toString( _block.transactions.size() ) },
        { "gasUsed", toString( _block.info.gasUsed() ) } } );

    noteCanonChanged();

    if ( m_onBlockImport )
        m_onBlockImport( _block.info );

    h256s dead;  // will be empty
    h256s fresh;
    fresh.push_back( tbi.hash() );

    clog( VerbosityTrace, "BlockChain" )
        << cc::debug( "Insterted block with " ) << _block.transactions.size()
        << cc::debug( " transactions" );

    return ImportRoute{ dead, fresh, _block.transactions };
}

void BlockChain::clearBlockBlooms( unsigned _begin, unsigned _end ) {
    //   ... c c c c c c c c c c C o o o o o o
    //   ...                               /=15        /=21
    // L0...| ' | ' | ' | ' | ' | ' | ' | 'b|x'x|x'x|x'e| /=11
    // L1...|   '   |   '   |   '   |   ' b | x ' x | x ' e |   /=6
    // L2...|       '       |       '   b   |   x   '   x   |   e   /=3
    // L3...|               '       b       |       x       '       e
    // model: c_bloomIndexLevels = 4, c_bloomIndexSize = 2

    //   ...                               /=15        /=21
    // L0...| ' ' ' | ' ' ' | ' ' ' | ' ' 'b|x'x'x'x|x'e' ' |
    // L1...|       '       '       '   b   |   x   '   x   '   e   '       |
    // L2...|               b               '               x               '                e '
    // | model: c_bloomIndexLevels = 2, c_bloomIndexSize = 4

    // algorithm doesn't have the best memoisation coherence, but eh well...

    unsigned beginDirty = _begin;
    unsigned endDirty = _end;
    for ( unsigned level = 0; level < c_bloomIndexLevels; level++, beginDirty /= c_bloomIndexSize,
                   endDirty = ( endDirty - 1 ) / c_bloomIndexSize + 1 ) {
        // compute earliest & latest index for each level, rebuild from previous levels.
        for ( unsigned item = beginDirty; item != endDirty; ++item ) {
            unsigned bunch = item / c_bloomIndexSize;
            unsigned offset = item % c_bloomIndexSize;
            auto id = chunkId( level, bunch );
            LogBloom acc;
            if ( !!level ) {
                // rebuild the bloom from the previous (lower) level (if there is one).
                auto lowerChunkId = chunkId( level - 1, item );
                for ( auto const& bloom : blocksBlooms( lowerChunkId ).blooms )
                    acc |= bloom;
            }
            blocksBlooms( id );  // make sure it has been memoized.
            m_blocksBlooms[id].blooms[offset] = acc;
        }
    }
}

void BlockChain::rescue( State const& /*_state*/ ) {
    clog( VerbosityInfo, "BlockChain" ) << "Rescuing database...";
    throw std::logic_error( "Rescueing is not implemented" );

    unsigned u = 1;
    while ( true ) {
        try {
            if ( isKnown( numberHash( u ) ) )
                u *= 2;
            else
                break;
        } catch ( ... ) {
            break;
        }
    }
    unsigned l = u / 2;
    clog( VerbosityInfo, "BlockChain" ) << cc::debug( "Finding last likely block number..." );
    while ( u - l > 1 ) {
        unsigned m = ( u + l ) / 2;
        clog( VerbosityInfo, "BlockChain" ) << " " << m << flush;
        if ( isKnown( numberHash( m ) ) )
            l = m;
        else
            u = m;
    }
    clog( VerbosityInfo, "BlockChain" ) << "  lowest is " << l;
    for ( ; l > 0; --l ) {
        h256 h = numberHash( l );
        clog( VerbosityInfo, "BlockChain" )
            << cc::debug( "Checking validity of " ) << l << cc::debug( " (" ) << h
            << cc::debug( ")..." ) << flush;
        try {
            clog( VerbosityInfo, "BlockChain" ) << cc::debug( "block..." ) << flush;
            BlockHeader bi( block( h ) );
            clog( VerbosityInfo, "BlockChain" ) << cc::debug( "extras..." ) << flush;
            details( h );
            clog( VerbosityInfo, "BlockChain" ) << cc::debug( "state..." ) << flush;
            clog( VerbosityInfo, "BlockChain" )
                << cc::warn( "STATE VALIDITY CHECK IS NOT SUPPORTED" ) << flush;
            //            if (_db.exists(bi.stateRoot()))
            //                break;
        } catch ( ... ) {
        }
    }
    clog( VerbosityInfo, "BlockChain" ) << "OK.";
    rewind( l );
}

void BlockChain::rewind( unsigned _newHead ) {
    DEV_WRITE_GUARDED( x_lastBlockHash ) {
        if ( _newHead >= m_lastBlockNumber )
            return;
        clearCachesDuringChainReversion( _newHead + 1 );
        m_lastBlockHash = numberHash( _newHead );
        m_lastBlockNumber = _newHead;
        try {
            m_extrasDB->insert(
                db::Slice( "best" ), db::Slice( ( char const* ) &m_lastBlockHash, 32 ) );
        } catch ( boost::exception const& ex ) {
            cwarn << "Error writing to extras database: " << boost::diagnostic_information( ex );
            cwarn << "Put" << toHex( bytesConstRef( db::Slice( "best" ) ) ) << "=>"
                  << toHex( bytesConstRef( db::Slice( ( char const* ) &m_lastBlockHash, 32 ) ) );
            cwarn << "Fail writing to extras database. Bombing out.";
            cerror << DETAILED_ERROR;
            exit( -1 );
        }
        noteCanonChanged();
    }
}

tuple< h256s, h256, unsigned > BlockChain::treeRoute(
    h256 const& _from, h256 const& _to, bool _common, bool _pre, bool _post ) const {
    if ( !_from || !_to )
        return make_tuple( h256s(), h256(), 0 );

    BlockDetails const fromDetails = details( _from );
    BlockDetails const toDetails = details( _to );
    // Needed to handle a special case when the parent of inserted block is not present in DB.
    if ( fromDetails.isNull() || toDetails.isNull() )
        return make_tuple( h256s(), h256(), 0 );

    unsigned fn = fromDetails.number;
    unsigned tn = toDetails.number;
    h256s ret;
    h256 from = _from;
    while ( fn > tn ) {
        if ( _pre )
            ret.push_back( from );
        from = details( from ).parent;
        fn--;
    }

    h256s back;
    h256 to = _to;
    while ( fn < tn ) {
        if ( _post )
            back.push_back( to );
        to = details( to ).parent;
        tn--;
    }
    for ( ;; from = details( from ).parent, to = details( to ).parent ) {
        if ( _pre && ( from != to || _common ) )
            ret.push_back( from );
        if ( _post && ( from != to || ( !_pre && _common ) ) )
            back.push_back( to );

        if ( from == to )
            break;
        if ( !from )
            assert( from );
        if ( !to )
            assert( to );
    }
    ret.reserve( ret.size() + back.size() );
    unsigned i = ret.size() - ( int ) ( _common && !ret.empty() && !back.empty() );
    for ( auto it = back.rbegin(); it != back.rend(); ++it )
        ret.push_back( *it );
    return make_tuple( ret, from, i );
}

void BlockChain::noteUsed( h256 const& _h, unsigned _extra ) const {
    auto id = CacheID( _h, _extra );
    Guard l( x_cacheUsage );
    m_cacheUsage[0].insert( id );
    if ( m_cacheUsage[1].count( id ) )
        m_cacheUsage[1].erase( id );
    else
        m_inUse.insert( id );
}

template < class K, class T >
static unsigned getHashSize( unordered_map< K, T > const& _map ) {
    unsigned ret = 0;
    for ( auto const& i : _map )
        ret += i.second.size + 64;
    return ret;
}

template < class K, class T >
static uint64_t getApproximateHashSize( unordered_map< K, T > const& _map ) {
    uint64_t ret = 0;
    uint64_t counter = 0;
    for ( auto const& i : _map ) {
        ret += i.second.size + 64;
        counter++;
        if ( counter >= 1024 ) {
            break;
        }
    }
    if ( _map.size() <= 1024 )
        return ret;
    else {
        // sample for large cache
        return ( ret * _map.size() ) / 1024;
    }
}


template < class K, class T >
static unsigned getBlockHashSize( map< K, T > const& _map ) {
    return _map.size() * ( BlockHash::size + 64 );
}

void BlockChain::updateStats() const {
    m_lastStats.memBlocks = 0;
    uint64_t counter = 0;
    {
        DEV_READ_GUARDED( x_blocks )

        for ( auto const& i : m_blocks ) {
            m_lastStats.memBlocks += i.second.size() + 64;
            counter++;
            if ( counter >= 1024 )
                break;
        }

        // sample for large cache
        if ( m_blocks.size() > 1024 ) {
            m_lastStats.memBlocks = ( m_lastStats.memBlocks * m_blocks.size() ) / 1024;
        }
    }
    { DEV_READ_GUARDED( x_details ) m_lastStats.memDetails = getApproximateHashSize( m_details ); }
    size_t logBloomsSize = 0;
    size_t blocksBloomsSize = 0;
    { DEV_READ_GUARDED( x_logBlooms ) logBloomsSize = getApproximateHashSize( m_logBlooms ); }
    {
        DEV_READ_GUARDED( x_blocksBlooms )
        blocksBloomsSize = getApproximateHashSize( m_blocksBlooms );
    }

    m_lastStats.memLogBlooms = logBloomsSize + blocksBloomsSize;

    {
        DEV_READ_GUARDED( x_receipts )
        m_lastStats.memReceipts = getApproximateHashSize( m_receipts );
    }
    {
        DEV_READ_GUARDED( x_blockHashes )
        m_lastStats.memBlockHashes = m_blockHashes.size() * ( 64 + BlockHash::size );
    }

    {
        DEV_READ_GUARDED( x_transactionAddresses )
        m_lastStats.memTransactionAddresses =
            m_transactionAddresses.size() * ( 64 + TransactionAddress::size );
    }
}

uint64_t BlockChain::getTotalCacheMemory() {
    return m_lastStats.memTotal();
}

void BlockChain::garbageCollect( bool _force ) {
    updateStats();

    if ( !_force && chrono::system_clock::now() < m_lastCollection + c_collectionDuration &&
         m_lastStats.memTotal() < c_maxCacheSize )
        return;
    if ( m_lastStats.memTotal() < c_minCacheSize )
        return;


    m_lastCollection = chrono::system_clock::now();

    // We subtract memory that blockhashes occupy because it is treated sepaparately
    while ( m_lastStats.memTotal() - m_lastStats.memBlockHashes >= c_maxCacheSize ) {
        Guard l( x_cacheUsage );
        for ( CacheID const& id : m_cacheUsage.back() ) {
            m_inUse.erase( id );
            // kill i from cache.
            switch ( id.second ) {
            case ( unsigned ) -1: {
                WriteGuard l( x_blocks );
                m_blocks.erase( id.first );
                break;
            }
            case ExtraDetails: {
                WriteGuard l( x_details );
                m_details.erase( id.first );
                break;
            }
            case ExtraBlockHash: {
                // m_cacheUsage should not contain ExtraBlockHash elements currently.  See the
                // second noteUsed() in BlockChain.h, which is a no-op.
                cerror << DETAILED_ERROR;
                assert( false );
                break;
            }
            case ExtraReceipts: {
                WriteGuard l( x_receipts );
                m_receipts.erase( id.first );
                break;
            }
            case ExtraLogBlooms: {
                WriteGuard l( x_logBlooms );
                m_logBlooms.erase( id.first );
                break;
            }
            case ExtraTransactionAddress: {
                WriteGuard l( x_transactionAddresses );
                m_transactionAddresses.erase( id.first );
                break;
            }
            case ExtraBlocksBlooms: {
                WriteGuard l( x_blocksBlooms );
                m_blocksBlooms.erase( id.first );
                break;
            }
            }
        }
        m_cacheUsage.pop_back();
        m_cacheUsage.push_front( std::unordered_set< CacheID >{} );
        updateStats();
    }


    {
        WriteGuard l( x_blockHashes );
        // This is where block hash memory cleanup is treated
        // allow only 4096 blockhashes in the cache
        if ( m_blockHashes.size() > 4096 ) {
            auto last = m_blockHashes.begin();
            std::advance( last, ( m_blockHashes.size() - 4096 ) );
            m_blockHashes.erase( m_blockHashes.begin(), last );
            assert( m_blockHashes.size() == 4096 );
        }
    }
}

void BlockChain::clearCaches() {
    {
        Guard l( x_cacheUsage );
        m_inUse.clear();

        int n = m_cacheUsage.size();
        m_cacheUsage.clear();
        m_cacheUsage.resize( n );
    }
    {
        WriteGuard l( x_details );
        m_details.clear();
    }
    {
        WriteGuard l( x_blocks );
        m_blocks.clear();
    }
    {
        WriteGuard l( x_logBlooms );
        m_logBlooms.clear();
    }
    {
        WriteGuard l( x_receipts );
        m_receipts.clear();
    }
    {
        WriteGuard l( x_transactionAddresses );
        m_transactionAddresses.clear();
    }
    {
        WriteGuard l( x_blocksBlooms );
        m_blocksBlooms.clear();
    }
    {
        WriteGuard l( x_blockHashes );
        m_blockHashes.clear();
    }
}

void BlockChain::doLevelDbCompaction() const {
    for ( auto it = m_rotator->begin(); it != m_rotator->end(); ++it ) {
        dev::db::LevelDB* ldb = dynamic_cast< dev::db::LevelDB* >( it->get() );
        assert( ldb );
        ldb->doCompaction();
    }
}

void BlockChain::checkConsistency() {
    DEV_WRITE_GUARDED( x_details ) { m_details.clear(); }

    m_blocksDB->forEach( [this]( db::Slice const& _key, db::Slice const& /* _value */ ) {
        if ( _key.size() == 32 ) {
            h256 h( ( _byte_ const* ) _key.data(), h256::ConstructFromPointer );
            auto dh = details( h );
            auto p = dh.parent;
            if ( p != h256() && p != m_genesisHash )  // TODO: for some reason the genesis
                                                      // details with the children get squished.
                                                      // not sure why.
            {
                auto dp = details( p );
                if ( asserts( contains( dp.children, h ) ) )
                    cnote << "Apparently the database is corrupt. Not much we can do at this "
                             "stage...";
                if ( assertsEqual( dp.number, dh.number - 1 ) )
                    cnote << "Apparently the database is corrupt. Not much we can do at this "
                             "stage...";
            }
        }
        return true;
    } );
}

void BlockChain::clearCachesDuringChainReversion( unsigned _firstInvalid ) {
    unsigned end = m_lastBlockNumber + 1;
    DEV_WRITE_GUARDED( x_blockHashes )
    for ( auto i = _firstInvalid; i < end; ++i )
        m_blockHashes.erase( i );
    DEV_WRITE_GUARDED( x_transactionAddresses )
    m_transactionAddresses.clear();  // TODO: could perhaps delete them individually?

    // If we are reverting previous blocks, we need to clear their blooms (in particular, to
    // rebuild any higher level blooms that they contributed to).
    clearBlockBlooms( _firstInvalid, end );
}

static inline unsigned upow( unsigned a, unsigned b ) {
    if ( !b )
        return 1;
    while ( --b > 0 )
        a *= a;
    return a;
}

static inline unsigned ceilDiv( unsigned n, unsigned d ) {
    return ( n + d - 1 ) / d;
}
// static inline unsigned floorDivPow(unsigned n, unsigned a, unsigned b) { return n / upow(a,
// b); } static inline unsigned ceilDivPow(unsigned n, unsigned a, unsigned b) { return
// ceilDiv(n, upow(a, b)); }

// Level 1
// [xxx.            ]

// Level 0
// [.x............F.]
// [........x.......]
// [T.............x.]
// [............    ]

// F = 14. T = 32

vector< unsigned > BlockChain::withBlockBloom(
    LogBloom const& _b, unsigned _earliest, unsigned _latest ) const {
    vector< unsigned > ret;

    // start from the top-level
    unsigned u = upow( c_bloomIndexSize, c_bloomIndexLevels );

    // run through each of the top-level blockbloom blocks
    // TODO here should be another blockBlooms() filtering!?
    for ( unsigned index = _earliest / u; index <= _latest / u; ++index )  // 0
        ret += withBlockBloom( _b, _earliest, _latest, c_bloomIndexLevels - 1, index );

    return ret;
}

vector< unsigned > BlockChain::withBlockBloom( LogBloom const& _b, unsigned _earliest,
    unsigned _latest, unsigned _level, unsigned _index ) const {
    // 14, 32, 1, 0
    // 14, 32, 0, 0
    // 14, 32, 0, 1
    // 14, 32, 0, 2

    vector< unsigned > ret;

    unsigned uCourse = upow( c_bloomIndexSize, _level + 1 );
    // 256
    // 16
    unsigned uFine = upow( c_bloomIndexSize, _level );
    // 16
    // 1

    unsigned obegin = _index == _earliest / uCourse ? _earliest / uFine % c_bloomIndexSize : 0;
    // 0
    // 14
    // 0
    // 0
    unsigned oend =
        _index == _latest / uCourse ? ( _latest / uFine ) % c_bloomIndexSize + 1 : c_bloomIndexSize;
    // 3
    // 16
    // 16
    // 1

    BlocksBlooms bb = blocksBlooms( _level, _index );
    for ( unsigned o = obegin; o < oend; ++o )
        if ( bb.blooms[o].contains( _b ) ) {
            // This level has something like what we want.
            if ( _level > 0 )
                ret += withBlockBloom(
                    _b, _earliest, _latest, _level - 1, o + _index * c_bloomIndexSize );
            else
                ret.push_back( o + _index * c_bloomIndexSize );
        }
    return ret;
}

h256Hash BlockChain::allKinFrom( h256 const& _parent, unsigned _generations ) const {
    // Get all uncles cited given a parent (i.e. featured as uncles/main in parent, parent + 1,
    // ... parent + 5).
    h256 p = _parent;
    h256Hash ret = { p };
    // p and (details(p).parent: i == 5) is likely to be overkill, but can't hurt to be
    // cautious.
    for ( unsigned i = 0; i < _generations && p != m_genesisHash; ++i, p = details( p ).parent ) {
        ret.insert( details( p ).parent );
        auto b = block( p );
        for ( auto i : RLP( b )[2] )
            ret.insert( sha3( i.data() ) );
    }
    return ret;
}

bool BlockChain::isKnown( h256 const& _hash, bool _isCurrent ) const {
    if ( _hash == m_genesisHash )
        return true;

    DEV_READ_GUARDED( x_blocks )
    if ( !m_blocks.count( _hash ) && !m_blocksDB->exists( toSlice( _hash ) ) ) {
        return false;
    }
    DEV_READ_GUARDED( x_details )
    if ( !m_details.count( _hash ) && !m_extrasDB->exists( toSlice( _hash, ExtraDetails ) ) ) {
        return false;
    }
    //  return true;
    return !_isCurrent ||
           details( _hash ).number <= m_lastBlockNumber;  // to allow rewind functionality.
}

bytes BlockChain::block( h256 const& _hash ) const {
    if ( _hash == m_genesisHash )
        return m_params.genesisBlock();

    {
        ReadGuard l( x_blocks );
        auto it = m_blocks.find( _hash );
        if ( it != m_blocks.end() )
            return it->second;
    }

    string d = m_blocksDB->lookup( toSlice( _hash ) );
    if ( d.empty() ) {
        cwarn << "Couldn't find requested block:" << _hash;
        return bytes();
    }

    noteUsed( _hash );

    WriteGuard l( x_blocks );
    m_blocks[_hash].resize( d.size() );
    memcpy( m_blocks[_hash].data(), d.data(), d.size() );

    return m_blocks[_hash];
}

bytes BlockChain::headerData( h256 const& _hash ) const {
    if ( _hash == m_genesisHash )
        return m_genesisHeaderBytes;

    {
        ReadGuard l( x_blocks );
        auto it = m_blocks.find( _hash );
        if ( it != m_blocks.end() )
            return BlockHeader::extractHeader( &it->second ).data().toBytes();
    }

    string d = m_blocksDB->lookup( toSlice( _hash ) );
    if ( d.empty() ) {
        cwarn << "Couldn't find requested block:" << _hash;
        return bytes();
    }

    noteUsed( _hash );

    WriteGuard l( x_blocks );
    m_blocks[_hash].resize( d.size() );
    memcpy( m_blocks[_hash].data(), d.data(), d.size() );

    return BlockHeader::extractHeader( &m_blocks[_hash] ).data().toBytes();
}

Block BlockChain::genesisBlock(
    boost::filesystem::path const& _dbPath, dev::h256 const& _genesis ) const {
    Block ret( *this, _dbPath, _genesis, skale::BaseState::Empty );

    ret.noteChain( *this );

    ret.mutableState().populateFrom( m_params.genesisState );
    ret.mutableState().commit();

    ret.m_previousBlock = BlockHeader( m_params.genesisBlock() );
    ret.resetCurrent();
    return ret;
}

Block BlockChain::genesisBlock( const State& _state ) const {
    Block ret( *this, m_genesisHash, _state, skale::BaseState::PreExisting );
    ret.m_previousBlock = BlockHeader( m_params.genesisBlock() );
    ret.resetCurrent();
    return ret;
}

VerifiedBlockRef BlockChain::verifyBlock( bytesConstRef _block,
    std::function< void( Exception& ) > const& _onBad, ImportRequirements::value _ir ) const {
    MICROPROFILE_SCOPEI( "BlockChain", "verifyBlock", MP_ROSYBROWN );

    VerifiedBlockRef res;
    BlockHeader h;
    try {
        MICROPROFILE_SCOPEI( "BlockChain", "verifyBlock try", MP_ROSYBROWN );
        h = BlockHeader( _block );
        if ( !!( _ir & ImportRequirements::PostGenesis ) && ( !h.parentHash() || h.number() == 0 ) )
            BOOST_THROW_EXCEPTION( InvalidParentHash() << errinfo_required_h256( h.parentHash() )
                                                       << errinfo_currentNumber( h.number() ) );

        BlockHeader parent;
        if ( !!( _ir & ImportRequirements::Parent ) ) {
            MICROPROFILE_SCOPEI( "BlockChain", "verifyBlock if parent", MP_ROSYBROWN );
            bytes parentHeader( headerData( h.parentHash() ) );
            if ( parentHeader.empty() )
                BOOST_THROW_EXCEPTION( InvalidParentHash()
                                       << errinfo_required_h256( h.parentHash() )
                                       << errinfo_currentNumber( h.number() ) );
            parent = BlockHeader( parentHeader, HeaderData, h.parentHash() );
        }

        MICROPROFILE_ENTERI( "BlockChain", "sealEngine()->verify", MP_ORANGERED );
        sealEngine()->verify( ( _ir & ImportRequirements::ValidSeal ) ?
                                  Strictness::CheckEverything :
                                  Strictness::QuickNonce,
            h, parent, _block );
        MICROPROFILE_LEAVE();

        MICROPROFILE_ENTERI( "BlockChain", "verifyBlock res.info = h", MP_ROSYBROWN );
        res.info = h;
        MICROPROFILE_LEAVE();
    } catch ( Exception& ex ) {
        MICROPROFILE_SCOPEI( "BlockChain", "verifyBlock catch", MP_ROSYBROWN );
        ex << errinfo_phase( 1 );
        addBlockInfo( ex, h, _block.toBytes() );
        if ( _onBad )
            _onBad( ex );
        throw;
    }

    MICROPROFILE_ENTERI( "BlockChain", "RLP r(_block)", MP_BURLYWOOD );
    RLP r( _block );
    MICROPROFILE_LEAVE();

    unsigned i = 0;
    if ( _ir & ( ImportRequirements::UncleBasic | ImportRequirements::UncleParent |
                   ImportRequirements::UncleSeals ) ) {
        MICROPROFILE_SCOPEI( "BlockChain", "check uncles", MP_ROSYBROWN );
        for ( auto const& uncle : r[2] ) {
            BlockHeader uh( uncle.data(), HeaderData );
            try {
                BlockHeader parent;
                if ( !!( _ir & ImportRequirements::UncleParent ) ) {
                    bytes parentHeader( headerData( uh.parentHash() ) );
                    if ( parentHeader.empty() )
                        BOOST_THROW_EXCEPTION( InvalidUncleParentHash()
                                               << errinfo_required_h256( uh.parentHash() )
                                               << errinfo_currentNumber( h.number() )
                                               << errinfo_uncleNumber( uh.number() ) );
                    parent = BlockHeader( parentHeader, HeaderData, uh.parentHash() );
                }
                sealEngine()->verify( ( _ir & ImportRequirements::UncleSeals ) ?
                                          Strictness::CheckEverything :
                                          Strictness::IgnoreSeal,
                    uh, parent );
            } catch ( Exception& ex ) {
                ex << errinfo_phase( 1 );
                ex << errinfo_uncleIndex( i );
                addBlockInfo( ex, uh, _block.toBytes() );
                if ( _onBad )
                    _onBad( ex );
                throw;
            }
            ++i;
        }
    }
    i = 0;
    if ( _ir &
         ( ImportRequirements::TransactionBasic | ImportRequirements::TransactionSignatures ) ) {
        MICROPROFILE_SCOPEI( "BlockChain", "check txns", MP_ROSYBROWN );
        for ( RLP const& tr : r[1] ) {
            bytesConstRef d = bytesRefFromTransactionRlp( tr );
            try {
                Transaction t( d,
                    ( _ir & ImportRequirements::TransactionSignatures ) ?
                        CheckTransaction::Everything :
                        CheckTransaction::None,
                    false,
                    EIP1559TransactionsPatch::isEnabledWhen(
                        this->info( numberHash( h.number() - 1 ) ).timestamp() ) );
                Ethash::verifyTransaction( chainParams(), _ir, t,
                    this->info( numberHash( h.number() - 1 ) ).timestamp(), h,
                    0 );  // the gasUsed vs
                // blockGasLimit is checked
                // later in enact function
                res.transactions.push_back( t );
            } catch ( Exception& ex ) {
                ex << errinfo_phase( 1 );
                ex << errinfo_transactionIndex( i );
                ex << errinfo_transaction( d.toBytes() );
                addBlockInfo( ex, h, _block.toBytes() );
                if ( _onBad )
                    _onBad( ex );
                throw;
            }
            ++i;
        }
    }
    res.block = bytesConstRef( _block );
    return res;
}

unsigned BlockChain::chainStartBlockNumber() const {
    auto const value = m_extrasDB->lookup( c_sliceChainStart );
    return value.empty() ? 0 : number( h256( value, h256::FromBinary ) );
}

bool BlockChain::isPatchTimestampActiveInBlockNumber( time_t _ts, BlockNumber _bn ) const {
    if ( _bn == 0 || _ts == 0 )
        return false;

    if ( _bn == LatestBlock )
        _bn = number();

    if ( _bn == PendingBlock )
        _bn = number() + 1;

    time_t prev_ts = this->info( this->numberHash( _bn - 1 ) ).timestamp();

    return prev_ts >= _ts;
}
