#include "StatusAndControl.h"
#include "Log.h"

#include <fstream>

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

    boost::filesystem::path tmpPath = this->statusFilePath;
    tmpPath += ".tmp";

    {
        std::ofstream ofs( tmpPath.string() );
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
    }

    boost::filesystem::rename( tmpPath, statusFilePath );
}

StatusAndControlFile::~StatusAndControlFile() {
    on_change();
}
