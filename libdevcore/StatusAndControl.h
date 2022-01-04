#ifndef STATUSANDCONTROL_H
#define STATUSANDCONTROL_H

#include<boost/filesystem.hpp>

#include<map>

class StatusAndControl {
public:
    enum Subsystem {SnapshotDownloader, Blockchain, Rpc, Consensus, Snapshotting};
    enum ConsensusRunningState {None, WaitingForPeers, Bootstrapping, Operation};

    virtual ~StatusAndControl();

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
        bool ExitTimeReached;
    };

    ExitState exitState;
private:

    std::map<Subsystem, bool> subsystemRunning;

    ConsensusRunningState consensusRunningState;
};

class StatusAndControlFile: public StatusAndControl {
public:
    StatusAndControlFile(const boost::filesystem::path& _dirPath, const std::string& _statusFile = "skaled.status");
    virtual ~StatusAndControlFile();
private:
    const boost::filesystem::path statusFilePath;
};

#endif // STATUSANDCONTROL_H
