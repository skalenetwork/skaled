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

#include "Common.h"
#include "Exceptions.h"
#include "Log.h"
#include "taskmon.h"

#include <skale/buildinfo.h>

#include <thread>

#include <skutils/utils.h>

using namespace std;

namespace dev {
char const* Version = skale_get_buildinfo()->project_version;
bytes const NullBytes;
std::string const EmptyString;

bool ExitHandler::shouldExit() {
    return s_bStop;
}
int ExitHandler::getSignal() {
    return s_nStopSignal;
}

void ExitHandler::exitHandler( int s ) {
    exitHandler( s, ec_success );
}

void ExitHandler::exitHandler( int nSignalNo, ExitHandler::exit_code_t ec ) {
    std::string strMessagePrefix;
    if ( nSignalNo > 0 ) {
        strMessagePrefix = ( ExitHandler::shouldExit() && s_nStopSignal > 0 ) ?
                               string( "\nStop flag was already raised on. " ) +
                                   "WILL FORCE TERMINATE." + " Caught (second) signal. " :
                               "\nCaught (first) signal. ";
    } else {
        strMessagePrefix = ExitHandler::shouldExit() ?
                               string( "\nInternal exit requested while already exiting. " ) :
                               "\nInternal exit initiated. ";
    }
    std::cerr << strMessagePrefix << skutils::signal::signal2str( nSignalNo ) << "\n\n";

    switch ( nSignalNo ) {
    case SIGINT:
    case SIGTERM:
    case SIGHUP:
        // exit normally
        // just fall through
        break;

    case SIGSTOP:
    case SIGTSTP:
    case SIGPIPE:
        // ignore
        return;
        break;

    case SIGQUIT:
        // exit immediately
        _exit( ExitHandler::ec_termninated_by_signal );
        break;

    case SIGILL:
    case SIGABRT:
    case SIGFPE:
    case SIGSEGV:
        // abort signals
        std::cout << "\n" << skutils::signal::generate_stack_trace() << "\n";
        std::cout.flush();
        std::cout << skutils::signal::read_maps() << "\n";
        std::cout.flush();

        _exit( nSignalNo + 128 );

    default:
        // exit normally
        break;
    }  // switch

    // try to exit nicely - then abort
    if ( !ExitHandler::shouldExit() ) {
        static volatile bool g_bSelfKillStarted = false;
        if ( !g_bSelfKillStarted ) {
            g_bSelfKillStarted = true;

            auto start_time = std::chrono::steady_clock::now();

            std::thread( [nSignalNo, start_time]() {
                std::cerr << ( "\n" + string( "SELF-KILL:" ) + " " + "Will sleep " +
                                 cc::size10( ExitHandler::KILL_TIMEOUT ) +
                                 " seconds before force exit..." ) +
                                 "\n\n";

                clog( VerbosityInfo, "exit" ) << "THREADS timer started";

                // while waiting, every 0.1s check whch threades exited
                vector< string > threads;
                for ( int i = 0; i < ExitHandler::KILL_TIMEOUT * 10; ++i ) {
                    auto end_time = std::chrono::steady_clock::now();
                    float seconds = std::chrono::duration< float >( end_time - start_time ).count();

                    try {
                        vector< string > new_threads = taskmon::list_names();
                        vector< string > threads_diff = taskmon::lists_diff( threads, new_threads );
                        threads = new_threads;

                        if ( threads_diff.size() ) {
                            cerr << seconds << " THREADS " << threads.size() << ":";
                            for ( const string& t : threads_diff )
                                cerr << " " << t;
                            cerr << endl;
                        }
                    } catch ( ... ) {
                        // swallow it
                    }

                    std::this_thread::sleep_for( 100ms );
                }

                std::cerr << ( "\n" + string( "SELF-KILL:" ) + " " +
                               "Will force exit after sleeping " +
                               cc::size10( ExitHandler::KILL_TIMEOUT ) + cc::error( " second(s)" ) +
                               "\n\n" );

                // TODO deduplicate this with main() before return
                ExitHandler::exit_code_t ec = ExitHandler::requestedExitCode();
                if ( ec == ExitHandler::ec_success ) {
                    if ( nSignalNo != SIGINT && nSignalNo != SIGTERM )
                        ec = ExitHandler::ec_failure;
                }

                _exit( ec );
            } ).detach();
        }  // if( ! g_bSelfKillStarted )
    }      // if ( !skutils::signal::g_bStop )

    // nice exit here:

    // TODO deduplicate with first if()
    if ( ExitHandler::shouldExit() && s_nStopSignal > 0 && nSignalNo > 0 ) {
        std::cerr << ( "\n" + string( "SIGNAL-HANDLER:" ) + " " + "Will force exit now...\n\n" );
        _exit( 13 );
    }

    s_nStopSignal = nSignalNo;

    if ( ec != ec_success ) {
        s_ec = ec;
    }

    // indicate failure if signal is not INT or TERM or internal (-1)
    if ( s_ec == ec_success && nSignalNo > 0 && nSignalNo != SIGINT && nSignalNo != SIGTERM )
        s_ec = ExitHandler::ec_failure;

    s_bStop = true;
}

void InvariantChecker::checkInvariants(
    HasInvariants const* _this, char const* _fn, char const* _file, int _line, bool _pre ) {
    if ( !_this->invariants() ) {
        cwarn << ( _pre ? "Pre" : "Post" ) << "invariant failed in" << _fn << "at" << _file << ":"
              << _line;
        ::boost::exception_detail::throw_exception_( FailedInvariant(), _fn, _file, _line );
    }
}

TimerHelper::~TimerHelper() {
    auto e = std::chrono::high_resolution_clock::now() - m_t;
    if ( !m_ms || e > chrono::milliseconds( m_ms ) )
        clog( VerbosityDebug, "timer" )
            << m_id << " " << chrono::duration_cast< chrono::milliseconds >( e ).count() << " ms";
}

int64_t utcTime() {
    // TODO: Fix if possible to not use time(0) and merge only after testing in all platforms
    // time_t t = time(0);
    // return mktime(gmtime(&t));
    return time( 0 );
}

string inUnits( bigint const& _b, strings const& _units ) {
    ostringstream ret;
    u256 b;
    if ( _b < 0 ) {
        ret << "-";
        b = ( u256 ) -_b;
    } else
        b = ( u256 ) _b;

    u256 biggest = 1;
    for ( unsigned i = _units.size() - 1; !!i; --i )
        biggest *= 1000;

    if ( b > biggest * 1000 ) {
        ret << ( b / biggest ) << " " << _units.back();
        return ret.str();
    }
    ret << setprecision( 3 );

    u256 unit = biggest;
    for ( auto it = _units.rbegin(); it != _units.rend(); ++it ) {
        auto i = *it;
        if ( i != _units.front() && b >= unit ) {
            ret << ( double( b / ( unit / 1000 ) ) / 1000.0 ) << " " << i;
            return ret.str();
        } else
            unit /= 1000;
    }
    ret << b << " " << _units.front();
    return ret.str();
}

std::atomic< ExitHandler::exit_code_t > ExitHandler::s_ec = ExitHandler::ec_success;
std::atomic_int ExitHandler::s_nStopSignal{ 0 };
std::atomic_bool ExitHandler::s_bStop{ false };

}  // namespace dev
