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

#pragma once

#include <cstddef>
#include <string>
#include <sys/types.h>

namespace dev {
namespace test {

// Shared privilege helpers for tests that create a loopback BTRFS filesystem.
//
// BTRFS setup needs to alternate between the original sudo user and root:
// the image file should be created as the user that invoked sudo, while
// mount/umount/chown require root. This class stores the sudo user's uid/gid
// and exposes the common identity-switching and cleanup operations.
class BtrfsTestEnvironment {
public:
    // Identity of the user that invoked the test through sudo.
    struct SudoIdentity {
        uid_t uid = 0;
        gid_t gid = 0;
    };

    // Validates that the process is running under sudo and records SUDO_UID/SUDO_GID.
    BtrfsTestEnvironment();

    // Switches the process back to the original sudo user for user-owned file creation.
    void dropRoot() const;

    // Switches the process back to root for privileged mount and cleanup operations.
    void gainRoot() const;

    // Returns the uid/gid of the original sudo user.
    uid_t sudoUid() const { return m_sudo.uid; }
    gid_t sudoGid() const { return m_sudo.gid; }

    // Default fixed paths used by the older BTRFS-backed tests.
    static std::string defaultImagePath();
    static std::string defaultMountPath();

    // Unmounts _mountPath if needed, removes _imagePath, and optionally removes _mountPath.
    // Pass _removeMountPath=false when the mount path is owned by another RAII object,
    // such as TransientDirectory.
    static void cleanupArtifacts(
        const std::string& _mountPath, const std::string& _imagePath, bool _removeMountPath );

protected:
    // Reads and validates sudo environment variables.
    static SudoIdentity checkSudo();

    // Low-level identity switch to a specific sudo user.
    static void dropRoot( const SudoIdentity& _sudo );

    // Low-level identity switch to root.
    static void gainRootProcess();

private:
    SudoIdentity m_sudo;
};

// RAII wrapper for a loopback BTRFS mount used by snapshot-related tests.
//
// Construction creates a BTRFS image, mounts it, and leaves the process as root. 
// Destruction unmounts and cleans the image, unless NC is set and 
// respectNoCleanupEnv is true.
class BtrfsTestMount : public BtrfsTestEnvironment {
public:
    // Mount configuration. The remove/create mount path flags make ownership explicit:
    // fixed test paths are owned by this helper, while external paths are owned elsewhere.
    struct Options {
        std::string imagePath = "btrfs.file";
        std::string mountPath = "btrfs";
        bool removeMountPath = true;
        bool createMountPath = true;
        bool respectNoCleanupEnv = true;
        size_t imageSizeMb = 200;
    };

    // Creates a BTRFS mount using the legacy fixed paths: btrfs.file mounted at btrfs.
    BtrfsTestMount();

    // Creates and mounts a BTRFS image according to the supplied options.
    explicit BtrfsTestMount( Options _options );

    // Unmounts and removes owned artifacts, unless cleanup is disabled through NC.
    ~BtrfsTestMount();

    // Mount ownership cannot be copied safely.
    BtrfsTestMount( const BtrfsTestMount& ) = delete;
    BtrfsTestMount& operator=( const BtrfsTestMount& ) = delete;

    // Options for legacy tests that use fixed relative paths and let this helper own them.
    static Options fixedPaths(
        const std::string& _mountPath = "btrfs", const std::string& _imagePath = "btrfs.file" );

    // Options for mounting on a directory owned by another object, such as TransientDirectory.
    static Options externalMountPath(
        const std::string& _mountPath, const std::string& _imagePath = "btrfs.file" );

    // Options for tests that should avoid shared fixed paths and use unique /tmp artifacts.
    static Options uniqueTempPaths( const std::string& _prefix );

    // Paths currently managed by this mount instance.
    const std::string& mountPath() const { return m_options.mountPath; }
    const std::string& imagePath() const { return m_options.imagePath; }

private:
    Options m_options;
    bool m_cleanupArmed = false;
};

}  // namespace test
}  // namespace dev
