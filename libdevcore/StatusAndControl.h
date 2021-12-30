#ifndef STATUSANDCONTROL_H
#define STATUSANDCONTROL_H

#include<boost/filesystem.hpp>

#include<map>

class StatusAndControl {
public:
    enum Subsystem {StartFromSnapshot, Blockchain, Rpc, Consensus, Snapshotting};
    enum ConsensusRunningState {None, WaitingForPeers, Bootstrapping, Operation};

    StatusAndControl(const boost::filesystem::path& _dirPath, const std::string& _statusFile = "skaled.status");
    ~StatusAndControl();

    void setSubsystemRunning(Subsystem _ss, bool _run){
        subsystemRunning[_ss] = _run;
    }
    bool isSubsystemRunning(Subsystem _ss) const {
        return subsystemRunning.count(_ss) && subsystemRunning.at(_ss);
    }
    void setConsensusRunningState(ConsensusRunningState _state){
        consensusRunningState = _state;
    }
    ConsensusRunningState getConsensusRunningState() const {
        return consensusRunningState;
    }

    struct ExitState {
        bool ClearDataDir;
        bool StartAgain = true;
        bool StartFromSnapshot;
    };

    ExitState exitState;
private:
    const boost::filesystem::path statusFilePath;

    std::map<Subsystem, bool> subsystemRunning;

    ConsensusRunningState consensusRunningState;
};

#endif // STATUSANDCONTROL_H
