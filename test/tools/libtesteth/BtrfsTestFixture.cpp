/*
    Modifications Copyright (C) 2018-2026 SKALE Labs

    This file is part of cpp-ethereum.

    cpp-ethereum is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    cpp-ethereum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "BtrfsTestFixture.h"

#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <utility>

#include <sys/stat.h>
#include <unistd.h>

using namespace std;

namespace dev {
namespace test {
namespace {

string uniquePathSuffix() {
    static atomic< unsigned > counter{ 0 };
    return to_string( getpid() ) + "_" + to_string( ++counter );
}

}  // namespace


// -------- BtrfsTestEnvironment --------- //

BtrfsTestEnvironment::BtrfsTestEnvironment() : m_sudo( checkSudo() ) {}

void BtrfsTestEnvironment::dropRoot() const {
    dropRoot( m_sudo );
}

void BtrfsTestEnvironment::gainRoot() const {
    gainRootProcess();
}

string BtrfsTestEnvironment::defaultImagePath() {
    return "btrfs.file";
}

string BtrfsTestEnvironment::defaultMountPath() {
    return "btrfs";
}

BtrfsTestEnvironment::SudoIdentity BtrfsTestEnvironment::checkSudo() {
    SudoIdentity sudo;
#if ( !defined __APPLE__ )
    const char* idStr = getenv( "SUDO_UID" );
    if ( idStr == nullptr ) {
        cerr << "Please run under sudo" << endl;
        exit( -1 );
    }

    sudo.uid = static_cast< uid_t >( atoi( idStr ) );

    if ( geteuid() != 0 ) {
        cerr << "Need to be root" << endl;
        exit( -1 );
    }

    idStr = getenv( "SUDO_GID" );
    if ( idStr == nullptr ) {
        cerr << "Missing SUDO_GID" << endl;
        exit( -1 );
    }
    sudo.gid = static_cast< gid_t >( atoi( idStr ) );

    gid_t rgid, egid, sgid;
    getresgid( &rgid, &egid, &sgid );
    cerr << "GIDS: " << rgid << " " << egid << " " << sgid << endl;
#endif
    return sudo;
}

void BtrfsTestEnvironment::dropRoot( const SudoIdentity& _sudo ) {
#if ( !defined __APPLE__ )
    int res = setresgid( _sudo.gid, _sudo.gid, 0 );
    cerr << "setresgid " << _sudo.gid << " " << res << endl;
    if ( res < 0 )
        cerr << strerror( errno ) << endl;
    res = setresuid( _sudo.uid, _sudo.uid, 0 );
    cerr << "setresuid " << _sudo.uid << " " << res << endl;
    if ( res < 0 )
        cerr << strerror( errno ) << endl;
#endif
}

void BtrfsTestEnvironment::gainRootProcess() {
#if ( !defined __APPLE__ )
    int res = setresuid( 0, 0, 0 );
    if ( res ) {
        cerr << strerror( errno ) << endl;
        assert( false );
    }
    res = setresgid( 0, 0, 0 );
    if ( res ) {
        cerr << strerror( errno ) << endl;
        assert( false );
    }
#endif
}

void BtrfsTestEnvironment::cleanupArtifacts(
    const string& _mountPath, const string& _imagePath, bool _removeMountPath ) {
    gainRootProcess();
#if ( !defined __APPLE__ )
    while ( system( ( "mountpoint -q " + _mountPath ).c_str() ) == 0 ) {
        int rv = system( ( "umount " + _mountPath ).c_str() );
        assert( rv == 0 );
    }
#endif
    int rv;
    if ( _removeMountPath ) {
        rv = system( ( "rm -rf " + _mountPath ).c_str() );
        assert( rv == 0 );
    }
    rv = system( ( "rm -f " + _imagePath ).c_str() );
    assert( rv == 0 );
}

// -------- BtrfsTestMount --------- //

BtrfsTestMount::BtrfsTestMount() : BtrfsTestMount( fixedPaths() ) {}

BtrfsTestMount::BtrfsTestMount( Options _options )
    : BtrfsTestEnvironment(), m_options( std::move( _options ) ) {
    cleanupArtifacts( m_options.mountPath, m_options.imagePath, m_options.removeMountPath );

    dropRoot();
    int rv = system( ( "dd if=/dev/zero of=" + m_options.imagePath + " bs=1M count=" +
                       to_string( m_options.imageSizeMb ) )
                         .c_str() );
    assert( rv == 0 );
    rv = system( ( "mkfs.btrfs " + m_options.imagePath ).c_str() );
    assert( rv == 0 );
    if ( m_options.createMountPath ) {
        rv = system( ( "mkdir -p " + m_options.mountPath ).c_str() );
        assert( rv == 0 );
    }

    gainRoot();
    rv = system(
        ( "mount -o user_subvol_rm_allowed " + m_options.imagePath + " " + m_options.mountPath )
            .c_str() );
    assert( rv == 0 );
    rv = chown( m_options.mountPath.c_str(), sudoUid(), sudoGid() );
    assert( rv == 0 );
    dropRoot();

    m_cleanupArmed = true;
    gainRoot();
}

BtrfsTestMount::~BtrfsTestMount() {
    if ( !m_cleanupArmed )
        return;

    if ( m_options.respectNoCleanupEnv && getenv( "NC" ) )
        return;

    cleanupArtifacts( m_options.mountPath, m_options.imagePath, m_options.removeMountPath );
}

BtrfsTestMount::Options BtrfsTestMount::fixedPaths(
    const string& _mountPath, const string& _imagePath ) {
    Options options;
    options.mountPath = _mountPath;
    options.imagePath = _imagePath;
    options.removeMountPath = true;
    options.createMountPath = true;
    return options;
}

BtrfsTestMount::Options BtrfsTestMount::externalMountPath(
    const string& _mountPath, const string& _imagePath ) {
    Options options;
    options.mountPath = _mountPath;
    options.imagePath = _imagePath;
    options.removeMountPath = false;
    options.createMountPath = false;
    return options;
}

BtrfsTestMount::Options BtrfsTestMount::uniqueTempPaths( const string& _prefix ) {
    Options options;
    string path = "/tmp/" + _prefix + "_" + uniquePathSuffix();
    options.mountPath = path;
    options.imagePath = path + ".img";
    options.removeMountPath = true;
    options.createMountPath = true;
    return options;
}

}  // namespace test
}  // namespace dev
