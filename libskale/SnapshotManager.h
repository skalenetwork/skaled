#ifndef SNAPSHOTAGENT_H
#define SNAPSHOTAGENT_H

#include <boost/filesystem.hpp>
#include <string>
#include <vector>

class SnapshotManager {
    //////////////////////// EXCEPTIONS /////////////////////

private:
    class FsException : public std::exception {
    protected:
        std::string what_str;

    public:
        boost::filesystem::path path;

        FsException( const boost::filesystem::path& _path ) : path( _path ) {
            what_str = "Cannot process path " + path.string();
        }
        virtual const char* what() const noexcept override { return what_str.c_str(); }
    };

public:
    class InvalidPath : public FsException {
    public:
        InvalidPath( const boost::filesystem::path& _path ) : FsException( _path ) {
            what_str = "Invalid path " + path.string();
        }
    };
    class CannotRead : public FsException {
    public:
        CannotRead( const boost::filesystem::path& _path ) : FsException( _path ) {
            what_str = "Cannot read " + path.string();
        }
    };
    class CannotWrite : public FsException {
    public:
        CannotWrite( const boost::filesystem::path& _path ) : FsException( _path ) {
            what_str = "Cannot write " + path.string();
        }
    };
    class CannotCreate : public CannotWrite {
    public:
        CannotCreate( const boost::filesystem::path& _path ) : CannotWrite( _path ) {
            what_str = "Cannot create " + path.string();
        }
    };
    class CannotDelete : public CannotWrite {
    public:
        CannotDelete( const boost::filesystem::path& _path ) : CannotWrite( _path ) {
            what_str = "Cannote delete " + path.string();
        }
    };
    class CannotCreateTmpFile : public FsException {
    public:
        CannotCreateTmpFile( const boost::filesystem::path& _path ) : FsException( _path ) {
            what_str = "Cannot create tmp file " + path.string();
        }
    };

    class CannotPerformBtrfsOperation : public std::exception {
    protected:
        std::string what_str;

    public:
        std::string cmd;
        std::string strerror;

    public:
        CannotPerformBtrfsOperation( const char* _cmd, const char* _msg ) {
            this->cmd = _cmd;
            this->strerror = _msg;
            what_str = "BTRFS operation: " + this->cmd + " - failed: " + this->strerror;
        }
        virtual const char* what() const noexcept override { return what_str.c_str(); }
    };

    class SnapshotAbsent : public std::exception {
    protected:
        std::string what_str;

    public:
        unsigned block_number;

        SnapshotAbsent( unsigned _blockNumber ) {
            block_number = _blockNumber;
            what_str = "Snapshot for block " + std::to_string( block_number ) + " is absent";
        }
        virtual const char* what() const noexcept override { return what_str.c_str(); }
    };

    class SnapshotPresent : public std::exception {
    protected:
        std::string what_str;

    public:
        unsigned block_number;

        SnapshotPresent( unsigned _blockNumber ) {
            block_number = _blockNumber;
            what_str =
                "Snapshot for block " + std::to_string( block_number ) + " is already present";
        }
        virtual const char* what() const noexcept override { return what_str.c_str(); }
    };

    /////////////// MORE INTERESTING STUFF ////////////////

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
