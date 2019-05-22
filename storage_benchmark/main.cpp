#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>

using namespace std;

#include <libdevcore/Address.h>
#include <libdevcore/FileSystem.h>

#include <libskale/State.h>

using namespace skale;
using namespace dev;

namespace fs = boost::filesystem;

double measure_performance( function< void() > code, const double accuracity = 1.0 ) {
    const double test_duration = 1;
    double frequency = 1;
    size_t total_count = 0;
    double start = clock();
    while ( true ) {
        const size_t count =
            max( static_cast< size_t >( round( frequency * test_duration ) ), size_t( 1 ) );
        for ( size_t i = 0; i < count; ++i ) {
            code();
        }
        total_count += count;
        double finish = clock();

        double new_freqency = total_count / ( finish - start ) * CLOCKS_PER_SEC;
        if ( fabs( new_freqency - frequency ) < accuracity ) {
            return new_freqency;
        } else {
            frequency = new_freqency;
        }
    }
}

void debug() {
    //  State state = State(0);
    Address address( 5 );

    //  fs::path db_path = getDataDir("/tmp/ethereum/");
    fs::path db_path = "/tmp/ethereum/";
    cout << db_path << endl;

    //    dev::OverlayDB db = State::openDB(db_path, h256(12345), WithExisting::Kill);
    //    State state = State(0, db, eth::BaseState::Empty);
    //    state.setBalance(address, 5);
    //    state.commit(State::CommitBehaviour::RemoveEmptyAccounts);
    //    state.db().commit();
    //    cout << state.balance(address) << endl;
    //    cout << state << endl;

    State state( 0, db_path, h256( 12345 ) );

    state.addBalance( address, 5 );
    for ( auto const& b : state.code( address ) ) {
        cout << ( int ) b << ' ';
    }
    cout << endl;

    bytes code( 50 );
    srand( 13 );
    for ( auto& b : code ) {
        b = rand();
    }
    state.setCode( address, code );
    state.commit( State::CommitBehaviour::KeepEmptyAccounts );
    return;

    state.addBalance( address, 5 );
    u256 value = state.storage( address, 7 );
    state.setStorage( address, 7, value + 7 );
    state.commit( State::CommitBehaviour::RemoveEmptyAccounts );
    cout << state.balance( address ) << endl;
    cout << state.storage( address, 7 ) << endl;

    //  OverlayDB db = State::openDB(db_path, h256(12345), WithExisting::Trust);
    //  State state = State(0, db);
    //  cout << "Balance: " << state.balance(address) << endl;

    //  h256 key = h256(5);
    //  vector<_byte_> data;
    //  for (_byte_ i = '0'; i < '5'; ++i) {
    //    data.push_back(i + 1);
    //  }
    //  bytesConstRef value = bytesConstRef(data.data(), data.size());

    //  db.insert(key, value);
    //  db.commit();
    //  string returned_value = db.lookup(key);
    //  cout << returned_value << endl;
    //  return;

    //  State state = State(0, db, BaseState::Empty);
    //  //  State state = State(0, db);

    //  cout << state.balance(address) << endl;
    //  state.addBalance(address, 5);
    //  cout << state.balance(address) << endl;
    //  state.commit(State::CommitBehaviour::RemoveEmptyAccounts);
    //  cout << "Commit state" << endl;
    //  state.db().commit();
}

void testState() {
    Address address( 5 );
    fs::path db_path = "/tmp/ethereum/";

    State state( 0, db_path, h256( 12345 ) );

    cout << "Balances writes:" << endl;
    cout << measure_performance(
                [&state, &address]() {
                    State writeState = state.startWrite();
                    writeState.addBalance( address, 1 );
                    writeState.commit( State::CommitBehaviour::KeepEmptyAccounts );
                },
                1000 )
         << " writes per second" << endl;
    cout << endl;

    cout << "Balances reads:" << endl;
    cout << measure_performance(
                [&state, &address]() { state.startRead().balance( address ); }, 100000 ) /
                1e6
         << " Mreads per second" << endl;
    cout << endl;

    cout << "EVM storate writes:" << endl;
    size_t memory_address = 0;
    cout << measure_performance(
                [&state, &address, &memory_address]() {
                    State writeState = state.startWrite();
                    writeState.setStorage( address, memory_address, memory_address );
                    memory_address = ( memory_address + 1 ) % 1024;
                    writeState.commit( State::CommitBehaviour::KeepEmptyAccounts );
                },
                10 )
         << " writes per second" << endl;
    cout << endl;

    cout << "EVM storate reads:" << endl;
    cout << measure_performance(
                [&state, &address, &memory_address]() {
                    state.startRead().storage( address, memory_address );
                    memory_address = ( memory_address + 1 ) % 1024;
                },
                1000 ) /
                1e6
         << " Mreads per second" << endl;
    cout << endl;

    cout << "EVM code writes:" << endl;
    bytes code( 50 );
    srand( 13 );
    for ( auto& b : code ) {
        b = rand();
    }
    cout << measure_performance(
                [&state, &address, &code]() {
                    State writeState = state.startWrite();
                    writeState.setCode( address, code );
                    writeState.commit( State::CommitBehaviour::KeepEmptyAccounts );
                },
                10 )
         << " writes per second" << endl;
    cout << endl;

    cout << "EVM code reads:" << endl;
    cout << measure_performance(
                [&state, &address]() { state.startRead().code( address ); }, 1000 ) /
                1e6
         << " Mreads per second" << endl;
}

int main() {
    //    debug();
    testState();
    return 0;

    //    State state = State(0);
    //    Address address(5);

    //    cout << "Balances writes:" << endl;
    //    cout << measure_performance(
    //                [&state, &address]() {
    //                    state.addBalance(address, 1);
    //                    state.commit(State::CommitBehaviour::KeepEmptyAccounts);
    //                },
    //                1000)
    //         << " writes per second" << endl;
    //    cout << endl;

    //    cout << "Balances reads:" << endl;
    //    cout << measure_performance([&state, &address]() { state.balance(address); }, 100000) /
    //    1e6
    //         << " Mreads per second" << endl;
    //    cout << endl;

    //    cout << "EVM storate writes:" << endl;
    //    size_t memory_address;
    //    cout << measure_performance(
    //                [&state, &address, &memory_address]() {
    //                    state.setStorage(address, memory_address, memory_address);
    //                    memory_address = (memory_address + 1) % 1024;
    //                    state.commit(State::CommitBehaviour::KeepEmptyAccounts);
    //                },
    //                10)
    //         << " writes per second" << endl;
    //    cout << endl;

    //    cout << "EVM storate reads:" << endl;
    //    cout << measure_performance(
    //                [&state, &address, &memory_address]() {
    //                    state.storage(address, memory_address);
    //                    memory_address = (memory_address + 1) % 1024;
    //                },
    //                1000) /
    //                1e6
    //         << " Mreads per second" << endl;

    return 0;
}
