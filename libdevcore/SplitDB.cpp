#include "SplitDB.h"

#include <memory>

namespace dev{
namespace db{


SplitDB::SplitDB(std::shared_ptr<DatabaseFace> _backend):
    backend(_backend){

}

DatabaseFace* SplitDB::newInterface(){
    assert(this->interfaces.size()<256);

    unsigned char prefix = this->interfaces.size();

    mutexes.push_back(std::make_unique<std::shared_mutex>());
    PrefixedDB* pdb = new PrefixedDB(prefix, backend.get(), *mutexes.back());
    interfaces.emplace_back(pdb);

    return pdb;
}

SplitDB::PrefixedWriteBatchFace::PrefixedWriteBatchFace(WriteBatchFace& _backend, char _prefix):
    backend(_backend), prefix(_prefix){

}

void SplitDB::PrefixedWriteBatchFace::insert( Slice _key, Slice _value ){
    std::vector<char> key2 = _key.toVector();
    key2.insert(key2.begin(), prefix);

    backend.insert(ref(key2), _value);
}

void SplitDB::PrefixedWriteBatchFace::kill( Slice _key ){
    std::vector<char> key2 = _key.toVector();
    key2.insert(key2.begin(), prefix);

    backend.kill(ref(key2));
}

SplitDB::PrefixedDB::PrefixedDB(char _prefix, DatabaseFace* _backend, std::shared_mutex& _mutex):
    prefix(_prefix), backend(_backend), backend_mutex(_mutex){

}

std::string SplitDB::PrefixedDB::lookup( Slice _key ) const {
    assert(_key.size() >= 1);

    std::vector<char> key2 = _key.toVector();
    key2.insert(key2.begin(), prefix);
    return backend->lookup(ref(key2));
}

bool SplitDB::PrefixedDB::exists( Slice _key ) const {
    std::vector<char> key2 = _key.toVector();
    key2.insert(key2.begin(), prefix);

    return backend->exists(ref(key2));
}

void SplitDB::PrefixedDB::insert( Slice _key, Slice _value ) {
    std::vector<char> key2 = _key.toVector();
    key2.insert(key2.begin(), prefix);

    backend->insert(ref(key2), _value);
}

void SplitDB::PrefixedDB::kill( Slice _key ) {
    std::vector<char> key2 = _key.toVector();
    key2.insert(key2.begin(), prefix);

    backend->kill(ref(key2));
}

std::unique_ptr< WriteBatchFace > SplitDB::PrefixedDB::createWriteBatch() const {
    return backend->createWriteBatch();
}
void SplitDB::PrefixedDB::commit( std::unique_ptr< WriteBatchFace > _batch ) {

}

void SplitDB::PrefixedDB::forEach( std::function< bool( Slice, Slice ) > f ) const {

}

h256 SplitDB::PrefixedDB::hashBase() const {
    return h256();
}

} // namespace db
} // namespace dev
