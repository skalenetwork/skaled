#ifndef ROTATINGLEVELDB_H
#define ROTATINGLEVELDB_H

#include "LevelDB.h"

#include <set>

namespace dev{
namespace db{

class ManuallyRotatingLevelDB : public DatabaseFace
{
private:
    DatabaseFace* current_piece;
    mutable std::set<WriteBatchFace*> batch_cache;
public:
    ManuallyRotatingLevelDB(const boost::filesystem::path& _path, int _nPieces);
    void rotate();

    virtual std::string lookup( Slice _key ) const;
    virtual bool exists( Slice _key ) const;
    virtual void insert( Slice _key, Slice _value );
    virtual void kill( Slice _key );

    virtual std::unique_ptr< WriteBatchFace > createWriteBatch() const;
    virtual void commit( std::unique_ptr< WriteBatchFace > _batch );

    virtual void forEach( std::function< bool( Slice, Slice ) > f ) const;
    virtual h256 hashBase() const;
};

}  // namespace db
}  // namespace dev

#endif // ROTATINGLEVELDB_H
