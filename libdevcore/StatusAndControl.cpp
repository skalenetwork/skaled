#include "StatusAndControl.h"

#include <fstream>

StatusAndControl::StatusAndControl(const boost::filesystem::path& _dirPath, const std::string& _statusFile)
    :statusFilePath(_dirPath / _statusFile){

}

StatusAndControl::~StatusAndControl(){
    // NEXT Write to file!
    /* JSON:
     * {
     *     subsystemRunning:{
     *         StartFromSnapshot: false,
     *         Blockchain: true,
     *         Rpc: true
     *         !MAY BE ADDED IN FUTURE:
     *         Consensus: true,
     *         Snapshotting: true
     *     },
     *     exitState:{
     *         ClearDataDir: false,
     *         StartAgain: true,
     *         StartFromSnapshot: false
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
           'StartFromSnapshot': " << isSubsystemRunning(StartFromSnapshot) << ",\
           'Blockchain': " << isSubsystemRunning(Blockchain) << ",\
           'Rpc': " << isSubsystemRunning(Rpc) << "\
       },\
       exitState:{\
           'ClearDataDir': " << exitState.ClearDataDir << ",\
           'StartAgain': " << exitState.StartAgain << ",\
           'StartFromSnapshot': " << exitState.StartFromSnapshot << "\
       }\
    }\n";
    }

    boost::filesystem::rename(tmpPath, statusFilePath);
}
