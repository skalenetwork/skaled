#include <libdevcore/CommonIO.h>
#include <libskale/SnapshotManager.h>

#include <iostream>
#include <unistd.h>

int main(int argc, char *argv[]) {
    std::string dataDir, configPath;
    unsigned blockNumber;

    int opt;

    while ((opt = getopt(argc, argv, "d:c:b:")) != -1) {
        switch (opt) {
            case 'b':
            blockNumber = std::stoul(optarg);
            break;
            case 'c':
            configPath = optarg;
            break;
            case 'd':
            dataDir = optarg;
            break;
            case '?': // fprintf(stderr, "unknown flag\n");
            exit(1);
        }
    }

    std::string configJSON = dev::contentsString( configPath );

    dev::eth::ChainParams chainParams;
    chainParams.loadConfig( configJSON, configPath );

    std::shared_ptr< SnapshotManager > snapshotManager;

    snapshotManager.reset( new SnapshotManager( chainParams, dataDir,
            { dev::eth::BlockChain::getChainDirName( chainParams ), "filestorage",
                "prices_" + chainParams.nodeInfo.id.str() + ".db",
                "blocks_" + chainParams.nodeInfo.id.str() + ".db" }, "" ) );

    std::cout << "SLEEPING FOR 60 seconds\n";
    sleep(60);

    snapshotManager->doSnapshot( blockNumber );

    std::cout << "SNAPSHOT IS READY, CALCULATING ITS HASH NOW\n";

    snapshotManager->computeSnapshotHash( blockNumber );
    
    std::cout << "SNAPSHOT HASH IS READY, EXITING\n";

    return 0;
}