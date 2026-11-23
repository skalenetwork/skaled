/*
    Copyright (C) 2026-present, SKALE Labs

    This file is part of skaled.

    skaled is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skaled is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skaled.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef BITE

#include "DecryptedTxFieldsPatch.h"

#include <libethereum/SchainPatch.h>

#include <libdevcore/Log.h>

#include <ctime>
#include <string>

namespace dev {
namespace eth {
namespace patches {

std::shared_ptr< const DecryptedRegularTxsMap > decryptedFields(
    const std::shared_ptr< const DecryptedRegularTxsMap >& _map ) {
    return _map;
}

std::shared_ptr< const DecryptedRegularTxsMap > legacyDecryptedFields(
    const DecryptedRegularTxsMap& _map, size_t _firstTxIndex, size_t _endTxIndex ) {
    auto result = std::make_shared< DecryptedRegularTxsMap >();
    auto cursor = _map.begin();
    for ( size_t i = _firstTxIndex; i < _endTxIndex; ++i ) {
        if ( cursor != _map.end() && cursor->first == i ) {
            result->emplace( *cursor );
        }
        if ( cursor != _map.end() ) {
            ++cursor;
        }
    }
    return result;
}

bool useLegacyDecryptedFields( time_t _parentBlockTimestamp ) {
    return DecryptedTxFieldsLegacyStartPatch::isReachedWhen( _parentBlockTimestamp ) &&
           !DecryptedTxFieldsPatch::isEnabledWhen( _parentBlockTimestamp );
}

std::shared_ptr< const DecryptedRegularTxsMap > selectDecryptedFields(
    const std::shared_ptr< const DecryptedRegularTxsMap >& _map, size_t _firstTxIndex,
    size_t _endTxIndex, time_t _parentBlockTimestamp ) {
    if ( useLegacyDecryptedFields( _parentBlockTimestamp ) ) {
        return legacyDecryptedFields( *_map, _firstTxIndex, _endTxIndex );
    }
    return decryptedFields( _map );
}

void logDecryptedFieldsWindow( const ChainOperationParams& _chainParams ) {
    const time_t legacyStart =
        _chainParams.getPatchTimestamp( SchainPatchEnum::DecryptedTxFieldsLegacyStartPatch );
    const time_t fix = _chainParams.getPatchTimestamp( SchainPatchEnum::DecryptedTxFieldsPatch );

    if ( legacyStart != 0 && fix != 0 && legacyStart >= fix ) {
        cnote << "Decrypted fields: legacy selection is never used (start " << legacyStart
              << " is not before end " << fix << ")";
    } else {
        cnote << "Decrypted fields: legacy selection from "
              << ( legacyStart == 0 ? "the first block" :
                                      "timestamp " + std::to_string( legacyStart ) )
              << ( fix == 0 ? ", no end set" : " until timestamp " + std::to_string( fix ) );
    }

    if ( legacyStart > time( nullptr ) ) {
        cwarn << "WARNING: DecryptedTxFieldsLegacyStartPatch is set to " << legacyStart
              << ", which is in the future (is it in seconds?). It must be a past timestamp, "
                 "the same on every node of the chain. Check the config.";
    }
}

}  // namespace patches
}  // namespace eth
}  // namespace dev

#endif  // BITE
