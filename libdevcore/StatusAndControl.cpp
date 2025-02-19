#include "StatusAndControl.h"

#include "Exceptions.h"
#include "Log.h"

#include <fstream>
#include <thread>

using namespace dev;

StatusAndControl::~StatusAndControl() {}

StatusAndControlFile::StatusAndControlFile(
    const boost::filesystem::path& _dirPath, const std::string& _statusFile )
    : statusFilePath( _dirPath / _statusFile ) {}

void StatusAndControl::setSubsystemRunning( Subsystem _ss, bool _run ) {
    cnote << "Skaled status: setSubsystemRunning: " << subsystemString[_ss] << " to "
          << ( _run ? "true\n" : "false\n" );
    subsystemRunning[_ss] = _run;
    on_change();
}
bool StatusAndControl::isSubsystemRunning( Subsystem _ss ) const {
    return subsystemRunning.count( _ss ) && subsystemRunning.at( _ss );
}
void StatusAndControl::setConsensusRunningState( ConsensusRunningState _state ) {
    cnote << "Skaled status: setConsensusRunningState to " << consensusRunningStateString[_state];
    consensusRunningState = _state;
    on_change();
}

StatusAndControl::ConsensusRunningState StatusAndControl::getConsensusRunningState() const {
    return consensusRunningState;
}

void StatusAndControl::setExitState( ExitState _key, bool _val ) {
    cnote << "Skaled status: setExitState: " << exitStateString[_key] << " to "
          << ( _val ? "true" : "false" );
    exitState[_key] = _val;
    on_change();
}

bool StatusAndControl::getExitState( ExitState _key ) const {
    return exitState.count( _key ) && exitState.at( _key );
}

void StatusAndControlFile::on_change() {
    /* JSON:
     * {
     *     subsystemRunning:{
     *         SnapshotDownloader: false,
     *         Blockchain: true,
     *         Rpc: true
     *         !MAY BE ADDED IN FUTURE:
     *         Consensus: true,
     *         Snapshotting: true
     *     },
     *     exitState:{
     *         ClearDataDir: false,
     *         StartAgain: true,
     *         StartFromSnapshot: false,
     *         exitTimeReached: false
     *     }
     *     !MAY BE ADDED IN FUTURE:
     *     runningState: {
     *         Consensus: None|WaitingForPeers|Bootstrapping|Operation
     *     }
     * }
     */

    try {

        boost::filesystem::path tmpPath = this->statusFilePath;
        // make sure this can be donne from parallel threads
        tmpPath += ".tmp." + std::to_string(pthread_self());

        {
            std::ofstream ofs( tmpPath.string() );
            ofs.exceptions( std::ofstream::failbit | std::ofstream::badbit );
            ofs << "\
    {\n\
       \"subsystemRunning\":{\n\
           \"SnapshotDownloader\": "
                << ( isSubsystemRunning( SnapshotDownloader ) ? "true" : "false" ) << ",\n\
           \"WaitingForTimestamp\": "
                << ( isSubsystemRunning( WaitingForTimestamp ) ? "true" : "false" ) << ",\n\
           \"Blockchain\": "
                << ( isSubsystemRunning( Blockchain ) ? "true" : "false" ) << ",\n\
           \"Rpc\": "
                << ( isSubsystemRunning( Rpc ) ? "true" : "false" ) << "\n\
       },\n\
       \"exitState\":{\n\
           \"ClearDataDir\": "
                << ( getExitState( ClearDataDir ) ? "true" : "false" ) << ",\n\
           \"StartAgain\": "
                << ( getExitState( StartAgain ) ? "true" : "false" ) << ",\n\
           \"StartFromSnapshot\": "
                << ( getExitState( StartFromSnapshot ) ? "true" : "false" ) << ",\n\
           \"ExitTimeReached\": "
                << ( getExitState( ExitTimeReached ) ? "true" : "false" ) << "\n\
       }\n\
    }\n";
            ofs.close();
        }

        boost::filesystem::rename( tmpPath, statusFilePath );
    } catch ( Exception& _e ) {
        // sometimes there is an exception here during skaled exit. Will investigate more later
        // for now catching exception so skaled can exit nicely
        // standard logging may not be available at this time
        std::cerr << "CRITICAL: Exception in StatusAndControlFile::on_change()" << _e.what()
                  << std::endl;
    }
}

StatusAndControlFile::~StatusAndControlFile() {
    on_change();
}
