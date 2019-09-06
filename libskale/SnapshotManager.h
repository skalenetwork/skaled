#ifndef SNAPSHOTAGENT_H
#define SNAPSHOTAGENT_H

#include <boost/filesystem.hpp>
#include <string>
#include <vector>

class SnapshotManager {
public:
    SnapshotManager(
        const boost::filesystem::path& _dataDir, const std::vector< std::string >& _volumes );
    void doSnapshot( unsigned _blockNumber );
    void restoreSnapshot( unsigned _blockNumber );
    boost::filesystem::path makeDiff( unsigned _fromBlock, unsigned _toBlock );
    void importDiff( unsigned _blockNumber, const boost::filesystem::path& _diffPath );

private:
    boost::filesystem::path data_dir;
    std::vector< std::string > volumes;
    boost::filesystem::path snapshots_dir;
};

#endif  // SNAPSHOTAGENT_H
