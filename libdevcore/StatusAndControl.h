#ifndef STATUSANDCONTROL_H
#define STATUSANDCONTROL_H

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"

#include <boost/filesystem.hpp>

#pragma GCC diagnostic pop

#include <array>
#include <iostream>
#include <map>

class StatusAndControl {
public:
    enum Subsystem {
        SnapshotDownloader,
        Blockchain,
        Rpc,
        Consensus,
        Snapshotting,
        WaitingForTimestamp
    };
    enum ExitState { ClearDataDir, StartAgain, StartFromSnapshot, ExitTimeReached };
    enum ConsensusRunningState { None, WaitingForPeers, Bootstrapping, Operation };

    std::array< std::string, 6 > subsystemString = { "SnapshotDownloader", "Blockchain", "Rpc",
        "Consensus", "Snapshotting", "WaitingForTimestamp" };
    std::array< std::string, 4 > exitStateString = { "ClearDataDir", "StartAgain",
        "StartFromSnapshot", "ExitTimeReached" };
    std::array< std::string, 4 > consensusRunningStateString = { "None", "WaitingForPeers",
        "Bootstrapping", "Operation" };

    virtual ~StatusAndControl();

    void setSubsystemRunning( Subsystem _ss, bool _run );

    bool isSubsystemRunning( Subsystem _ss ) const;

    void setConsensusRunningState( ConsensusRunningState _state );

    ConsensusRunningState getConsensusRunningState() const;

    void setExitState( ExitState _key, bool _val );

    bool getExitState( ExitState _key ) const;

protected:
    virtual void on_change() = 0;

private:
    std::map< Subsystem, bool > subsystemRunning;
    std::map< ExitState, bool > exitState;

    ConsensusRunningState consensusRunningState;
};

class StatusAndControlFile : public StatusAndControl {
public:
    StatusAndControlFile(
        const boost::filesystem::path& _dirPath, const std::string& _statusFile = "skaled.status" );
    virtual ~StatusAndControlFile();

protected:
    virtual void on_change();

private:
    const boost::filesystem::path statusFilePath;
};

#endif  // STATUSANDCONTROL_H
