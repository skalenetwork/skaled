#include "StatusAndControl.h"

#include <fstream>

StatusAndControl::~StatusAndControl() {}

StatusAndControlFile::StatusAndControlFile(
    const boost::filesystem::path& _dirPath, const std::string& _statusFile )
    : statusFilePath( _dirPath / _statusFile ) {}

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
