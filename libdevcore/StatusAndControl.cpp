#include "StatusAndControl.h"

#include <fstream>

StatusAndControl::~StatusAndControl(){}

StatusAndControlFile::StatusAndControlFile(const boost::filesystem::path& _dirPath, const std::string& _statusFile)
    :statusFilePath(_dirPath / _statusFile){

}

StatusAndControlFile::~StatusAndControlFile(){
    // NEXT Write to file!
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
    std::ofstream ofs(tmpPath.string());
    ofs << \
    "{\
       'subsystemRunning':{\
           'SnapshotDownloader': " << isSubsystemRunning(SnapshotDownloader) << ",\
           'Blockchain': " << isSubsystemRunning(Blockchain) << ",\
           'Rpc': " << isSubsystemRunning(Rpc) << "\
       },\
       exitState:{\
           'ClearDataDir': " << exitState.ClearDataDir << ",\
           'StartAgain': " << exitState.StartAgain << ",\
           'StartFromSnapshot': " << exitState.StartFromSnapshot << "\
           'ExitTimeReached': " << exitState.ExitTimeReached << "\
       }\
    }\n";
    }

    boost::filesystem::rename(tmpPath, statusFilePath);
}
