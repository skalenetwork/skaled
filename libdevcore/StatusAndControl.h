#ifndef STATUSANDCONTROL_H
#define STATUSANDCONTROL_H

#include <libconsensus/spdlog/include/spdlog/spdlog.h>

#include <boost/filesystem.hpp>

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

    std::array< std::string, 6 > subsystemString = { "SnapshotDownloader", "Blockchain", "Rpc", "Consensus", "Snapshotting", "WaitingForTimestamp" };
    std::array< std::string, 4 > exitStateString = { "ClearDataDir", "StartAgain", "StartFromSnapshot", "ExitTimeReached" };
    std::array< std::string, 4 > consensusRunningStateString = { "None", "WaitingForPeers", "Bootstrapping", "Operation" };

    virtual ~StatusAndControl();

    void setSubsystemRunning( Subsystem _ss, bool _run ) {
        spdlog::info("Skaled status: setSubsystemRunning: {} to {}", subsystemString[ _ss ], ( _run ? "true\n" : "false\n" ));
        subsystemRunning[_ss] = _run;
        on_change();
    }
    bool isSubsystemRunning( Subsystem _ss ) const {
        return subsystemRunning.count( _ss ) && subsystemRunning.at( _ss );
    }
    void setConsensusRunningState( ConsensusRunningState _state ) {
        spdlog::info("Skaled status: setConsensusRunningState to {}", consensusRunningStateString[ _state ]);
        consensusRunningState = _state;
        on_change();
    }
    ConsensusRunningState getConsensusRunningState() const { return consensusRunningState; }

    void setExitState( ExitState _key, bool _val ) {
        spdlog::info("Skaled status: setExitState: {} to {}", exitStateString[ _key ], ( _val ? "true" : "false" ));
        exitState[_key] = _val;
        on_change();
    }

    bool getExitState( ExitState _key ) const {
        return exitState.count( _key ) && exitState.at( _key );
    }

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
