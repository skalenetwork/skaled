#include "ManuallyRotatingLevelDB.h"

namespace dev {
namespace db {

ManuallyRotatingLevelDB::ManuallyRotatingLevelDB(const boost::filesystem::path& _path, int _nPieces){

}

std::string ManuallyRotatingLevelDB::lookup( Slice _key ) const {
    return current_piece->lookup( _key );
}

bool ManuallyRotatingLevelDB::exists( Slice _key ) const {
    return current_piece->exists( _key );
}

void ManuallyRotatingLevelDB::insert( Slice _key, Slice _value ) {
    current_piece->insert( _key, _value );
}

void ManuallyRotatingLevelDB::kill( Slice _key ) {
    current_piece->kill( _key );
}

std::unique_ptr< WriteBatchFace > ManuallyRotatingLevelDB::createWriteBatch() const {
    std::unique_ptr<WriteBatchFace>  wbf = current_piece->createWriteBatch();
    batch_cache.insert(wbf.get());
    return wbf;
}
void ManuallyRotatingLevelDB::commit( std::unique_ptr< WriteBatchFace > _batch ) {
    current_piece->commit( std::move(_batch) );
    batch_cache.erase(_batch.get());
}

void ManuallyRotatingLevelDB::forEach( std::function< bool( Slice, Slice ) > f ) const {
    current_piece->forEach(f);
}

h256 ManuallyRotatingLevelDB::hashBase() const {
    return h256();
}

}  // namespace db
}  // namespace dev
