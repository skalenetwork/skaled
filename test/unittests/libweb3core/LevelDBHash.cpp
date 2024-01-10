#include <libdevcore/LevelDB.h>
#include <libdevcore/TransientDirectory.h>
#include <boost/test/unit_test.hpp>

#include <string>

BOOST_AUTO_TEST_SUITE( LevelDBHashBase )

BOOST_AUTO_TEST_CASE( hash ) {
    dev::TransientDirectory td;

    dev::h256 hash;
    {
        std::unique_ptr< dev::db::LevelDB > db( new dev::db::LevelDB( td.path() ) );
        BOOST_REQUIRE( db );

        for ( size_t i = 0; i < 123; ++i ) {
            std::string key = std::to_string( 43 + i );
            std::string value = std::to_string( i );
            db->insert( dev::db::Slice(key), dev::db::Slice(value) );
        }

        hash = db->hashBase();
    }

    dev::h256 hash_same;
    {
        std::unique_ptr< dev::db::LevelDB > db_copy( new dev::db::LevelDB( td.path() ) );
        BOOST_REQUIRE( db_copy );

        for ( size_t i = 0; i < 123; ++i ) {
            std::string key = std::to_string( 43 + i );
            std::string value = std::to_string( i );
            db_copy->insert( dev::db::Slice(key), dev::db::Slice(value) );
        }

        hash_same = db_copy->hashBase();
    }

    BOOST_REQUIRE( hash == hash_same );

    dev::h256 hashPartially;
    {
        {
            std::unique_ptr< dev::db::LevelDB > db_copy( new dev::db::LevelDB( td.path() ) );
            BOOST_REQUIRE( db_copy );

            for ( size_t i = 0; i < 123; ++i ) {
                std::string key = std::to_string( 43 + i );
                std::string value = std::to_string( i );
                db_copy->insert( dev::db::Slice(key), dev::db::Slice(value) );
            }
        }

        std::array< std::string, 17 > lexographicKeysSegments = { "0", "1", "2", "3", "4", "5", "6",
                                                                  "7", "8", "9", "a", "b", "c", "d",
                                                                  "e", "f", "g" };

        secp256k1_sha256_t dbCtx;
        secp256k1_sha256_initialize( &dbCtx );

        for (size_t i = 0; i < lexographicKeysSegments.size() - 1; ++i) {
            std::unique_ptr< dev::db::LevelDB > db( new dev::db::LevelDB( td.path(),
                dev::db::LevelDB::defaultSnapshotReadOptions(), dev::db::LevelDB::defaultWriteOptions(),
                dev::db::LevelDB::defaultSnapshotDBOptions() ) );

            db->hashBasePartially( &dbCtx, lexographicKeysSegments[i], lexographicKeysSegments[i + 1] );
        }

        secp256k1_sha256_finalize( &dbCtx, hashPartially.data() );
    }

    BOOST_REQUIRE( hash == hashPartially );

    dev::h256 hash_diff;
    {
        std::unique_ptr< dev::db::LevelDB > db_diff( new dev::db::LevelDB( td.path() ) );
        BOOST_REQUIRE( db_diff );

        for ( size_t i = 0; i < 123; ++i ) {
            std::string key = std::to_string( 42 + i );
            std::string value = std::to_string( i );
            db_diff->insert( dev::db::Slice(key), dev::db::Slice(value) );
        }

        hash_diff = db_diff->hashBase();
    }

    BOOST_REQUIRE( hash != hash_diff );
}

BOOST_AUTO_TEST_SUITE_END()
