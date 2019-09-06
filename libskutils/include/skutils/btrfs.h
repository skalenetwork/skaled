typedef struct {
    const char* ( *strerror )();
    struct {
        int ( *create )( const char* path );
        int ( *_delete )( const char* path );
        int ( *snapshot )( const char* from, const char* to );
        int ( *snapshot_r )( const char* from, const char* to );
    } subvolume;
    int ( *receive )( const char* file, const char* path );
    int ( *send )( const char* parent, const char* file, const char* vol );
} btrfs_t;

extern btrfs_t btrfs;
