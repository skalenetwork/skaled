#pragma once

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
/** @file KeyAux.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 * CLI module for key management.
 */

#include <libdevcore/CommonIO.h>
#include <libdevcore/FileSystem.h>
#include <libdevcore/Log.h>
#include <libdevcore/SHA3.h>
#include <libethcore/KeyManager.h>
#include <libethcore/TransactionBase.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim_all.hpp>
#include <chrono>
#include <fstream>
#include <iosfwd>
#include <thread>

#include <skutils/console_colors.h>

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace boost::algorithm;

class BadArgument : public Exception {};

string createPassword( std::string const& _prompt ) {
    string ret;
    while ( true ) {
        ret = getPassword( _prompt );
        string confirm = getPassword( "Please confirm the passphrase by entering it again: " );
        if ( ret == confirm )
            break;
        cwarn << "Passwords were different. Try again.\n";
    }
    return ret;
    //	cdebug << "Enter a hint to help you remember this passphrase: " << flush;
    //	cin >> hint;
    //	return make_pair(ret, hint);
}

pair< string, string > createPassword( KeyManager& _keyManager, std::string const& _prompt,
    std::string const& _pass = std::string(), std::string const& _hint = std::string() ) {
    string pass = _pass;
    if ( pass.empty() )
        while ( true ) {
            pass = getPassword( _prompt );
            string confirm = getPassword( "Please confirm the passphrase by entering it again: " );
            if ( pass == confirm )
                break;
            cwarn << "Passwords were different. Try again.\n";
        }
    string hint = _hint;
    if ( hint.empty() && !pass.empty() && !_keyManager.haveHint( pass ) ) {
        cdebug << "Enter a hint to help you remember this passphrase: " << flush;
        getline( cin, hint );
    }
    return make_pair( pass, hint );
}

class KeyCLI {
public:
    enum class OperationMode {
        None,
        ListBare,
        NewBare,
        ImportBare,
        ExportBare,
        RecodeBare,
        KillBare,
        InspectBare,
        CreateWallet,
        List,
        New,
        Import,
        ImportWithAddress,
        ImportPresale,
        Export,
        Recode,
        Kill,
        Inspect,
        SignTx,
        DecodeTx,
    };

    KeyCLI( OperationMode _mode = OperationMode::None ) : m_mode( _mode ) {
        m_toSign.creation = true;
    }

    bool interpretOption( size_t& i, vector< string > const& argv ) {
        size_t argc = argv.size();
        string arg = argv[i];
        if ( arg == "--wallet-path" && i + 1 < argc )
            m_walletPath = argv[++i];
        else if ( arg == "--secrets-path" && i + 1 < argc )
            m_secretsPath = argv[++i];
        else if ( ( arg == "-m" || arg == "--master" ) && i + 1 < argc )
            m_masterPassword = argv[++i];
        else if ( arg == "--unlock" && i + 1 < argc )
            m_unlocks.push_back( argv[++i] );
        else if ( arg == "--lock" && i + 1 < argc )
            m_lock = argv[++i];
        else if ( arg == "--kdf" && i + 1 < argc )
            m_kdf = argv[++i];
        else if ( arg == "--kdf-param" && i + 2 < argc ) {
            auto n = argv[++i];
            auto v = argv[++i];
            m_kdfParams[n] = v;
        } else if ( arg == "newbare" )
            m_mode = OperationMode::NewBare;
        else if ( arg == "inspect" )
            m_mode = OperationMode::Inspect;
        else if ( ( arg == "-s" || arg == "--sign-tx" || arg == "sign" ) && i + 1 < argc ) {
            m_mode = OperationMode::SignTx;
            m_signKey = argv[++i];
        } else if ( arg == "--show-me-the-secret" )
            m_showSecret = true;
        else if ( arg == "--tx-data" && i + 1 < argc )
            try {
                m_toSign.data = fromHex( argv[++i] );
            } catch ( ... ) {
                cerror << "Invalid argument to " << arg << "\n";
                exit( -1 );
            }
        else if ( arg == "--tx-nonce" && i + 1 < argc )
            try {
                m_toSign.nonce = u256( argv[++i] );
            } catch ( ... ) {
                cerror << "Invalid argument to " << arg << "\n";
                exit( -1 );
            }
        else if ( arg == "--force-nonce" && i + 1 < argc )
            try {
                m_forceNonce = u256( argv[++i] );
            } catch ( ... ) {
                cerror << "Invalid argument to " << arg << "\n";
                exit( -1 );
            }
        else if ( ( arg == "--tx-dest" || arg == "--tx-to" || arg == "--tx-destination" ) &&
                  i + 1 < argc )
            try {
                m_toSign.creation = false;
                m_toSign.to = toAddress( argv[++i] );
            } catch ( ... ) {
                cerror << "Invalid argument to " << arg << "\n";
                exit( -1 );
            }
        else if ( arg == "--tx-gas" && i + 1 < argc )
            try {
                m_toSign.gas = u256( argv[++i] );
            } catch ( ... ) {
                cerror << "Invalid argument to " << arg << "\n";
                exit( -1 );
            }
        else if ( arg == "--tx-gasprice" && i + 1 < argc )
            try {
                m_toSign.gasPrice = u256( argv[++i] );
            } catch ( ... ) {
                cerror << "Invalid argument to " << arg << "\n";
                exit( -1 );
            }
        else if ( arg == "--tx-value" && i + 1 < argc )
            try {
                m_toSign.value = u256( argv[++i] );
            } catch ( ... ) {
                cerror << "Invalid argument to " << arg << "\n";
                exit( -1 );
            }
        else if ( arg == "--decode-tx" || arg == "decode" )
            m_mode = OperationMode::DecodeTx;
        else if ( arg == "--import-bare" || arg == "importbare" )
            m_mode = OperationMode::ImportBare;
        else if ( arg == "--list-bare" || arg == "listbare" )
            m_mode = OperationMode::ListBare;
        else if ( arg == "--export-bare" || arg == "exportbare" )
            m_mode = OperationMode::ExportBare;
        else if ( arg == "--inspect-bare" || arg == "inspectbare" )
            m_mode = OperationMode::InspectBare;
        else if ( arg == "--recode-bare" || arg == "recodebare" )
            m_mode = OperationMode::RecodeBare;
        else if ( arg == "--kill-bare" || arg == "killbare" )
            m_mode = OperationMode::KillBare;
        else if ( arg == "--create-wallet" || arg == "createwallet" )
            m_mode = OperationMode::CreateWallet;
        else if ( arg == "-l" || arg == "--list" || arg == "list" )
            m_mode = OperationMode::List;
        else if ( ( arg == "-n" || arg == "--new" || arg == "new" ) && i + 1 < argc ) {
            m_mode = OperationMode::New;
            m_name = argv[++i];
        } else if ( ( arg == "-i" || arg == "--import" || arg == "import" ) && i + 2 < argc ) {
            m_mode = OperationMode::Import;
            m_inputs = strings( 1, argv[++i] );
            m_name = argv[++i];
        } else if ( ( arg == "--import-presale" || arg == "importpresale" ) && i + 2 < argc ) {
            m_mode = OperationMode::ImportPresale;
            m_inputs = strings( 1, argv[++i] );
            m_name = argv[++i];
        } else if ( ( arg == "--import-with-address" || arg == "importwithaddress" ) &&
                    i + 3 < argc ) {
            m_mode = OperationMode::ImportWithAddress;
            m_inputs = strings( 1, argv[++i] );
            m_address = Address( argv[++i] );
            m_name = argv[++i];
        } else if ( arg == "--export" || arg == "export" )
            m_mode = OperationMode::Export;
        else if ( arg == "--recode" || arg == "recode" )
            m_mode = OperationMode::Recode;
        else if ( arg == "kill" )
            m_mode = OperationMode::Kill;
        else if ( arg == "--no-icap" )
            m_icap = false;
        else if ( m_mode == OperationMode::DecodeTx || m_mode == OperationMode::Inspect ||
                  m_mode == OperationMode::Kill || m_mode == OperationMode::SignTx ||
                  m_mode == OperationMode::ImportBare || m_mode == OperationMode::InspectBare ||
                  m_mode == OperationMode::KillBare || m_mode == OperationMode::Recode ||
                  m_mode == OperationMode::Export || m_mode == OperationMode::RecodeBare ||
                  m_mode == OperationMode::ExportBare )
            m_inputs.push_back( arg );
        else
            return false;
        return true;
    }

    KeyPair makeKey() const {
        KeyPair k( Secret::random() );
        while ( m_icap && k.address()[0] )
            k = KeyPair( Secret( sha3( k.secret().ref() ) ) );
        return k;
    }

    Secret getSecret( std::string const& _signKey ) {
        string json = contentsString( _signKey );
        if ( !json.empty() )
            return Secret( secretStore().secret( secretStore().readKeyContent( json ),
                [&]() { return getPassword( "Enter passphrase for key: " ); } ) );
        else {
            if ( h128 u = fromUUID( _signKey ) )
                return Secret( secretStore().secret(
                    u, [&]() { return getPassword( "Enter passphrase for key: " ); } ) );
            Address a;
            try {
                a = toAddress( _signKey );
            } catch ( ... ) {
                for ( Address const& aa : keyManager().accounts() )
                    if ( keyManager().accountName( aa ) == _signKey ) {
                        a = aa;
                        break;
                    }
            }
            if ( a )
                return keyManager().secret( a, [&]() {
                    return getPassword( "Enter passphrase for key (hint:" +
                                        keyManager().passwordHint( a ) + "): " );
                } );
            cerror << "Bad file, UUID or address: " << _signKey << "\n";
            exit( -1 );
        }
    }

    void execute() {
        switch ( m_mode ) {
        case OperationMode::CreateWallet: {
            KeyManager wallet( m_walletPath, m_secretsPath );
            if ( m_masterPassword.empty() )
                m_masterPassword = createPassword(
                    "Please enter a MASTER passphrase to protect your key store (make it "
                    "strong!): " );
            try {
                wallet.create( m_masterPassword );
            } catch ( Exception const& _e ) {
                cerror << "unable to create wallet" << "\n" << boost::diagnostic_information( _e );
            }
            break;
        }
        case OperationMode::DecodeTx: {
            try {
                TransactionBase t = m_inputs.empty() ? TransactionBase( m_toSign ) :
                                                       TransactionBase( inputData( m_inputs[0] ),
                                                           CheckTransaction::None );
                cdebug << "Transaction " << t.sha3().hex() << "\n";
                if ( t.isCreation() ) {
                    cdebug << "  type: creation" << "\n";
                    cdebug << "  code: " << toHex( t.data() ) << "\n";
                } else {
                    cdebug << "  type: message" << "\n";
                    cdebug << "  to: " << t.to() << "\n";
                    cdebug << "  data: " << ( t.data().empty() ? "none" : toHex( t.data() ) )
                           << "\n";
                }
                try {
                    auto s = t.sender();
                    if ( t.isCreation() )
                        cdebug << "  creates: " << toAddress( s, t.nonce() ) << "\n";
                    cdebug << "  from: " << s << "\n";
                } catch ( ... ) {
                    cdebug << "  from: <unsigned>" << "\n";
                }
                cdebug << "  value: " << formatBalance( t.value() ) << " (" << t.value() << " wei)"
                       << "\n";
                cdebug << "  nonce: " << t.nonce() << "\n";
                cdebug << "  gas: " << t.gas() << "\n";
                cdebug << "  gas price: " << formatBalance( t.gasPrice() ) << " (" << t.gasPrice()
                       << " wei)" << "\n";
                cdebug << "  signing hash: " << t.sha3( WithoutSignature ).hex() << "\n";
                if ( t.safeSender() ) {
                    cdebug << "  v: " << ( int ) t.signature().v << "\n";
                    cdebug << "  r: " << t.signature().r << "\n";
                    cdebug << "  s: " << t.signature().s << "\n";
                }
            } catch ( Exception& ex ) {
                cerror << "Invalid transaction: " << ex.what() << "\n";
            }
            break;
        }
        case OperationMode::SignTx: {
            Secret s = getSecret( m_signKey );
            if ( m_inputs.empty() )
                m_inputs.push_back( string() );
            for ( string const& i : m_inputs ) {
                bool isFile = false;
                try {
                    TransactionBase t = i.empty() ? TransactionBase( m_toSign ) :
                                                    TransactionBase( inputData( i, &isFile ),
                                                        CheckTransaction::None );
                    if ( m_forceNonce )
                        t.setNonce( m_forceNonce );
                    t.sign( s );
                    cdebug << t.sha3() << ": ";
                    if ( isFile ) {
                        writeFile( i + ".signed", toHex( t.toBytes() ) );
                        cdebug << i + ".signed" << "\n";
                    } else
                        cdebug << toHex( t.toBytes() ) << "\n";
                } catch ( Exception& ex ) {
                    cerror << "Invalid transaction: " << ex.what() << "\n";
                }
            }
            break;
        }
        case OperationMode::Inspect: {
            keyManager( true );
            for ( auto i : m_inputs ) {
                Address a = userToAddress( i );
                if ( !keyManager().accountName( a ).empty() )
                    cdebug << keyManager().accountName( a ) << " (" << a.abridged() << ")" << "\n";
                else
                    cdebug << a.abridged() << "\n";
                cdebug << "  Address: " << a.hex() << "\n";
                if ( m_showSecret ) {
                    Secret s = keyManager( true ).secret( a );
                    cdebug << "  Secret: "
                           << ( m_showSecret ? toHex( s.ref() ) :
                                               ( toHex( s.ref().cropped( 0, 8 ) ) + "..." ) )
                           << "\n";
                }
            }
            break;
        }
        case OperationMode::ListBare:
            if ( secretStore().keys().empty() )
                cdebug << "No keys found." << "\n";
            else
                for ( h128 const& u : std::set< h128 >() + secretStore().keys() )
                    cdebug << toUUID( u ) << "\n";
            break;
        case OperationMode::NewBare: {
            if ( m_lock.empty() )
                m_lock = createPassword( "Enter a passphrase with which to secure this account: " );
            auto k = makeKey();
            h128 u = secretStore().importSecret( k.secret().ref(), m_lock );
            cdebug << "Created key " << toUUID( u ) << "\n";
            cdebug << "  Address: " << k.address().hex() << "\n";
            break;
        }
        case OperationMode::ImportBare:
            for ( string const& input : m_inputs ) {
                h128 u;
                bytesSec b;
                b.writable() = fromHex( input );
                if ( b.size() != 32 ) {
                    std::string s = contentsString( input );
                    b.writable() = fromHex( s );
                    if ( b.size() != 32 )
                        u = secretStore().importKey( input );
                }
                if ( !u && b.size() == 32 )
                    u = secretStore().importSecret(
                        b, lockPassword( toAddress( Secret( b ) ).abridged() ) );
                if ( !u ) {
                    cerror << cc::warn( "Cannot import " ) << input
                           << cc::warn( " not a file or secret." ) << "\n";
                    continue;
                }
                cdebug << cc::success( "Successfully imported " ) << input << cc::success( " as " )
                       << toUUID( u );
            }
            break;
        case OperationMode::InspectBare:
            for ( auto const& i : m_inputs )
                if ( !contents( i ).empty() ) {
                    h128 u = secretStore().readKey( i, false );
                    bytesSec s = secretStore().secret( u,
                        [&]() { return getPassword( "Enter passphrase for key " + i + ": " ); } );
                    cdebug << "Key " << i << ":" << "\n";
                    cdebug << "  UUID: " << toUUID( u ) << ":" << "\n";
                    cdebug << "  Address: " << toAddress( Secret( s ) ).hex() << "\n";
                    cdebug << "  Secret: "
                           << ( m_showSecret ? toHex( s.ref() ) :
                                               ( toHex( s.ref().cropped( 0, 8 ) ) + "..." ) )
                           << "\n";
                } else if ( h128 u = fromUUID( i ) ) {
                    bytesSec s = secretStore().secret( u, [&]() {
                        return getPassword( "Enter passphrase for key " + toUUID( u ) + ": " );
                    } );
                    cdebug << "Key " << i << ":" << "\n";
                    cdebug << "  Address: " << toAddress( Secret( s ) ).hex() << "\n";
                    cdebug << "  Secret: "
                           << ( m_showSecret ? toHex( s.ref() ) :
                                               ( toHex( s.ref().cropped( 0, 8 ) ) + "..." ) )
                           << "\n";
                } else if ( Address a = toAddress( i ) ) {
                    cdebug << "Key " << a.abridged() << ":" << "\n";
                    cdebug << "  Address: " << a.hex() << "\n";
                } else
                    cerror << "Couldn't inspect " << i << "; not found." << "\n";
            break;
        case OperationMode::ExportBare:
            break;
        case OperationMode::RecodeBare:
            for ( auto const& i : m_inputs )
                if ( h128 u = fromUUID( i ) )
                    if ( secretStore().recode(
                             u, lockPassword( toUUID( u ) ),
                             [&]() {
                                 return getPassword(
                                     "Enter passphrase for key " + toUUID( u ) + ": " );
                             },
                             kdf() ) )
                        cerror << "Re-encoded " << toUUID( u ) << "\n";
                    else
                        cerror << "Couldn't re-encode " << toUUID( u )
                               << "; key corrupt or incorrect passphrase supplied." << "\n";
                else
                    cerror << "Couldn't re-encode " << i << "; not found." << "\n";
            break;
        case OperationMode::KillBare:
            for ( auto const& i : m_inputs )
                if ( h128 u = fromUUID( i ) )
                    secretStore().kill( u );
                else
                    cerror << "Couldn't kill " << i << "; not found." << "\n";
            break;
        case OperationMode::New: {
            keyManager();
            tie( m_lock, m_lockHint ) = createPassword( keyManager(),
                "Enter a passphrase with which to secure this account (or nothing to use the "
                "master passphrase): ",
                m_lock, m_lockHint );
            auto k = makeKey();
            bool usesMaster = m_lock.empty();
            h128 u = usesMaster ? keyManager().import( k.secret(), m_name ) :
                                  keyManager().import( k.secret(), m_name, m_lock, m_lockHint );
            cdebug << "Created key " << toUUID( u ) << "\n";
            cdebug << "  Name: " << m_name << "\n";
            if ( usesMaster )
                cdebug << "  Uses master passphrase." << "\n";
            else
                cdebug << "  Password hint: " << m_lockHint << "\n";
            cdebug << "  Address: " << k.address().hex() << "\n";
            break;
        }
        case OperationMode::Import: {
            if ( m_inputs.size() != 1 ) {
                cerror << "Error: exactly one key must be given to import." << "\n";
                break;
            }

            h128 u = keyManager().store().importKey( m_inputs[0] );
            string pw;
            bytesSec s = keyManager().store().secret(
                u, [&]() { return ( pw = getPassword( "Enter the passphrase for the key: " ) ); } );
            if ( s.size() != 32 ) {
                cerror << "Error: couldn't decode key or invalid secret size." << "\n";
                break;
            }

            bool usesMaster = true;
            if ( pw != m_masterPassword && m_lockHint.empty() ) {
                cdebug << "Enter a hint to help you remember the key's passphrase: " << flush;
                getline( cin, m_lockHint );
                usesMaster = false;
            }
            keyManager().importExisting( u, m_name, pw, m_lockHint );
            auto a = keyManager().address( u );

            cdebug << "Imported key " << toUUID( u ) << "\n";
            cdebug << "  Name: " << m_name << "\n";
            if ( usesMaster )
                cdebug << "  Uses master passphrase." << "\n";
            else
                cdebug << "  Password hint: " << m_lockHint << "\n";
            cdebug << "  Address: " << a.hex() << "\n";
            break;
        }
        case OperationMode::ImportWithAddress: {
            if ( m_inputs.size() != 1 ) {
                cerror << "Error: exactly one key must be given to import." << "\n";
                break;
            }
            keyManager();
            string const& i = m_inputs[0];
            h128 u;
            bytesSec b;
            b.writable() = fromHex( i );
            if ( b.size() != 32 ) {
                std::string s = contentsString( i );
                b.writable() = fromHex( s );
                if ( b.size() != 32 )
                    u = keyManager().store().importKey( i );
            }
            if ( !u && b.size() == 32 )
                u = keyManager().store().importSecret(
                    b, lockPassword( toAddress( Secret( b ) ).abridged() ) );
            if ( !u ) {
                cerror << cc::warn( "Cannot import " ) << i << cc::warn( " not a file or secret." )
                       << "\n";
                break;
            }
            keyManager().importExisting( u, m_name, m_address );
            cdebug << cc::success( "Successfully imported " ) << i << cc::success( ":" ) << "\n";
            cdebug << cc::success( "  Name: " ) << m_name << "\n";
            cdebug << cc::success( "  UUID: " ) << toUUID( u ) << "\n";
            cdebug << cc::success( "  Address: " ) << m_address << "\n";
            break;
        }
        case OperationMode::ImportPresale: {
            keyManager();
            std::string pw;
            KeyPair k = keyManager().presaleSecret( contentsString( m_inputs[0] ), [&]( bool ) {
                return ( pw = getPassword( "Enter the passphrase for the presale key: " ) );
            } );
            keyManager().import(
                k.secret(), m_name, pw, "Same passphrase as used for presale key" );
            break;
        }
        case OperationMode::Recode:
            for ( auto const& i : m_inputs )
                if ( Address a = userToAddress( i ) ) {
                    string pw;
                    Secret s = keyManager().secret( a, [&]() {
                        return pw = getPassword( "Enter old passphrase for key '" + i +
                                                 "' (hint: " + keyManager().passwordHint( a ) +
                                                 "): " );
                    } );
                    if ( !s ) {
                        cerror << "Invalid password for address " << a << "\n";
                        continue;
                    }
                    pair< string, string > np = createPassword(
                        keyManager(), "Enter new passphrase for key '" + i + "': " );
                    if ( keyManager().recode(
                             a, np.first, np.second, [&]() { return pw; }, kdf() ) )
                        cdebug << "Re-encoded key '" << i << "' successfully." << "\n";
                    else
                        cerror << "Couldn't re-encode '" << i
                               << "''; key corrupt or incorrect passphrase supplied." << "\n";
                } else
                    cerror << "Couldn't re-encode " << i << "; not found." << "\n";
            break;
        case OperationMode::Kill: {
            unsigned count = 0;
            for ( auto const& i : m_inputs ) {
                if ( Address a = userToAddress( i ) )
                    keyManager().kill( a );
                else
                    cerror << "Couldn't kill " << i << "; not found." << "\n";
                ++count;
            }
            cdebug << count << " key(s) deleted." << "\n";
            break;
        }
        case OperationMode::List: {
            if ( keyManager().store().keys().empty() ) {
                cdebug << "No keys found." << "\n";
                break;
            }

            vector< u128 > bare;
            AddressHash got;
            for ( auto const& u : keyManager().store().keys() )
                if ( Address a = keyManager().address( u ) ) {
                    got.insert( a );
                    cdebug << toUUID( u ) << " " << a.abridged();
                    cdebug << " " << a << " ";
                    cdebug << " " << keyManager().accountName( a ) << "\n";
                } else
                    bare.push_back( u );
            for ( auto const& u : bare )
                cdebug << toUUID( u ) << " (Bare)" << "\n";
            break;
        }
        default:
            break;
        }
    }

    std::string lockPassword( std::string const& _accountName ) {
        return m_lock.empty() ? createPassword( "Enter a passphrase with which to secure account " +
                                                _accountName + ": " ) :
                                m_lock;
    }

    static void streamHelp( ostream& _out ) {
        _out << "Secret-store (\"bare\") operation modes:" << "\n"
             << "    listbare  List all secret available in secret-store." << "\n"
             << "    newbare  Generate and output a key without interacting with wallet and dump "
                "the JSON."
             << "\n"
             << "    importbare [ <file>|<secret-hex> , ... ] Import keys from given sources."
             << "\n"
             << "    recodebare [ <uuid>|<file> , ... ]  Decrypt and re-encrypt given keys." << "\n"
             << "    inspectbare [ <uuid>|<file> , ... ]  Output information on given keys."
             << "\n"
             //			<< "    exportbare [ <uuid> , ... ]  Export given keys." << "\n"
             << "    killbare [ <uuid> , ... ]  Delete given keys." << "\n"
             << "Secret-store configuration:" << "\n"
             << "    --secrets-path <path>  Specify Web3 secret-store path (default: "
             << SecretStore::defaultPath() << ")" << "\n"
             << "\n"
             << "Wallet operating modes:" << "\n"
             << "    createwallet  Create an Ethereum master wallet." << "\n"
             << "    list  List all keys available in wallet." << "\n"
             << "    new <name>  Create a new key with given name and add it in the wallet." << "\n"
             << "    import [<uuid>|<file>|<secret-hex>] <name>  Import keys from given source and "
                "place in wallet."
             << "\n"
             << "    importpresale <file> <name>  Import a presale wallet into a key with the "
                "given name."
             << "\n"
             << "    importwithaddress [<uuid>|<file>|<secret-hex>] <address> <name>  Import keys "
                "from given source with given address and place in wallet."
             << "\n"
             << "    export [ <address>|<uuid> , ... ]  Export given keys." << "\n"
             << "    inspect [ <address>|<name>|<uuid> ] ...  Print information on the given keys."
             << "\n"
             //			<< "    recode [ <address>|<uuid>|<file> , ... ]  Decrypt and re-encrypt
             // given keys." << "\n"
             << "    kill [ <address>|<uuid>, ... ]  Delete given keys." << "\n"
             << "Wallet configuration:" << "\n"
             << "    --wallet-path <path>  Specify Ethereum wallet path (default: "
             << KeyManager::defaultPath() << ")" << "\n"
             << "    -m, --master <passphrase>  Specify wallet (master) passphrase." << "\n"
             << "\n"
             << "Transaction operating modes:" << "\n"
             << "    decode ( [ <hex>|<file> ] )  Decode given transaction." << "\n"
             << "    sign [ <address>|<uuid>|<file> ] ( [ <hex>|<file> , ... ] )  (Re-)Sign given "
                "transaction."
             << "\n"
             << "Transaction specification options (to be used when no transaction hex or file is "
                "given):"
             << "\n"
             << "    --tx-dest <address>  Specify the destination address for the transaction to "
                "be signed."
             << "\n"
             << "    --tx-data <hex>  Specify the hex data for the transaction to be signed."
             << "\n"
             << "    --tx-nonce <n>  Specify the nonce for the transaction to be signed." << "\n"
             << "    --tx-gas <n>  Specify the gas for the transaction to be signed." << "\n"
             << "    --tx-gasprice <wei>  Specify the gas price for the transaction to be signed."
             << "\n"
             << "    --tx-value <wei>  Specify the value for the transaction to be signed." << "\n"
             << "Transaction signing options:" << "\n"
             << "    --force-nonce <n>  Override the nonce for any transactions to be signed."
             << "\n"
             << "\n"
             << "Encryption configuration:" << "\n"
             << "    --kdf <kdfname>  Specify KDF to use when encrypting (default: sc	rypt)"
             << "\n"
             << "    --kdf-param <name> <value>  Specify a parameter for the KDF."
             << "\n"
             //			<< "    --cipher <ciphername>  Specify cipher to use when encrypting
             //(default: aes-128-ctr)" << "\n"
             //			<< "    --cipher-param <name> <value>  Specify a parameter for the cipher."
             //<< "\n"
             << "    --lock <passphrase>  Specify passphrase for when encrypting a (the) key."
             << "\n"
             << "    --hint <hint>  Specify hint for the --lock passphrase." << "\n"
             << "\n"
             << "Decryption configuration:" << "\n"
             << "    --unlock <passphrase>  Specify passphrase for a (the) key." << "\n"
             << "Key generation configuration:" << "\n"
             << "    --no-icap  Don't bother to make a direct-ICAP capable key." << "\n";
    }

    static bytes inputData( std::string const& _input, bool* _isFile = nullptr ) {
        bytes b = fromHex( _input );
        if ( _isFile )
            *_isFile = false;
        if ( b.empty() ) {
            if ( _isFile )
                *_isFile = true;
            std::string s = boost::trim_copy_if( contentsString( _input ), is_any_of( " \t\n" ) );
            b = fromHex( s );
            if ( b.empty() )
                b = asBytes( s );
        }
        return b;
    }

private:
    void openWallet( KeyManager& _w ) {
        while ( true ) {
            if ( _w.load( m_masterPassword ) )
                break;
            if ( !m_masterPassword.empty() ) {
                cdebug << "Password invalid. Try again." << "\n";
                m_masterPassword.clear();
            }
            m_masterPassword = getPassword( "Please enter your MASTER passphrase: " );
        }
    }

    KDF kdf() const { return m_kdf == "pbkdf2" ? KDF::PBKDF2_SHA256 : KDF::Scrypt; }

    Address userToAddress( std::string const& _s ) {
        if ( h128 u = fromUUID( _s ) )
            return keyManager().address( u );
        DEV_IGNORE_EXCEPTIONS( return toAddress( _s ) );
        for ( Address const& a : keyManager().accounts() )
            if ( keyManager().accountName( a ) == _s )
                return a;
        return Address();
    }

    KeyManager& keyManager( bool walletLess = false ) {
        if ( !m_keyManager ) {
            m_keyManager.reset( new KeyManager( m_walletPath, m_secretsPath ) );
            if ( m_keyManager->exists() )
                openWallet( *m_keyManager );
            else if ( !walletLess ) {
                cerror << "Couldn't open wallet. Does it exist?" << "\n";
                exit( -1 );
            }
        }
        return *m_keyManager;
    }

    SecretStore& secretStore() {
        if ( m_keyManager )
            return m_keyManager->store();
        if ( !m_secretStore )
            m_secretStore.reset( new SecretStore( m_secretsPath ) );
        return *m_secretStore;
    }

    /// Where the keys are.
    unique_ptr< SecretStore > m_secretStore;
    unique_ptr< KeyManager > m_keyManager;

    /// Operating mode.
    OperationMode m_mode;

    /// Wallet stuff
    boost::filesystem::path m_secretsPath = SecretStore::defaultPath();
    boost::filesystem::path m_walletPath = KeyManager::defaultPath();

    /// Wallet passphrase stuff
    string m_masterPassword;
    strings m_unlocks;
    string m_lock;
    string m_lockHint;
    bool m_icap = true;

    /// Creating/importing
    string m_name;
    Address m_address;

    /// Inspecting
    bool m_showSecret = false;

    /// Importing
    strings m_inputs;

    /// Signing
    string m_signKey;
    TransactionSkeleton m_toSign;
    u256 m_forceNonce;

    string m_kdf = "scrypt";
    map< string, string > m_kdfParams;
};
