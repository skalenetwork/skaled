/*
    Modifications Copyright (C) 2018 SKALE Labs

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

#include "Common.h"
#include "db.h"

#include <boost/filesystem.hpp>
#include <boost/program_options/options_description.hpp>

namespace dev::db {

enum class DatabaseKind { LevelDB, RocksDB };

/// Provide a set of program options related to databases
///
/// @param _lineLength  The line length for description text wrapping, the same as in
///                     boost::program_options::options_description::options_description().
boost::program_options::options_description databaseProgramOptions(
    unsigned _lineLength = boost::program_options::options_description::m_default_line_length );

bool isDiskDatabase();
DatabaseKind databaseKind();
void setDatabaseKindByName( std::string const& _name );
void setDatabaseKind( DatabaseKind _kind );
boost::filesystem::path databasePath();

class DBFactory {
public:
    DBFactory() = delete;
    ~DBFactory() = delete;

    static std::unique_ptr< DatabaseFace > create();
    static std::unique_ptr< DatabaseFace > create( boost::filesystem::path const& _path );
    static std::unique_ptr< DatabaseFace > create( DatabaseKind _kind );
    static std::unique_ptr< DatabaseFace > create(
        DatabaseKind _kind, boost::filesystem::path const& _path );
    static std::unique_ptr< DatabaseFace > createHistoric(
        DatabaseKind _kind, boost::filesystem::path const& _path );

    static void setReopenPeriodMs( int64_t _reopenPeriodMs ) { s_reopenPeriodMs = _reopenPeriodMs; }

private:
    static std::atomic< int64_t > s_reopenPeriodMs;
};
}  // namespace dev::db
