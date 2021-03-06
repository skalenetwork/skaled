#if ( !defined __SKUTILS_ETH_UTILS_H )
#define __SKUTILS_ETH_UTILS_H 1

#include <inttypes.h>
#include <string>
#include <vector>

namespace skutils {

namespace eth {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef std::vector< uint8_t > vec_bytes;

extern std::string call_error_message_2_str( const vec_bytes& b );

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace eth


};  // namespace skutils

#endif  /// (!defined __SKUTILS_ETH_UTILS_H)
