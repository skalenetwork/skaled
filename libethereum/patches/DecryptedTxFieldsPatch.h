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

#pragma once

#ifdef BITE

#include <libconsensus/node/ConsensusTypes.h>
#include <libethcore/ChainOperationParams.h>

#include <cstddef>
#include <ctime>
#include <memory>

namespace dev {
namespace eth {
namespace patches {

/// All functions below return the decrypted fields to attach to the regular transactions
/// of a block, keyed by block index, in the map type consensus provides. The caller looks
/// each transaction up by its own block index and attaches the value if it has one.
/// _firstTxIndex and _endTxIndex are the block indexes of the regular transactions:
/// [_firstTxIndex, _endTxIndex).

/// Current behaviour: the map from consensus as it is.
std::shared_ptr< const DecryptedRegularTxsMap > decryptedFields(
    const std::shared_ptr< const DecryptedRegularTxsMap >& _map );

/// Legacy behaviour. Kept ONLY to re-execute blocks that were executed with it; do not
/// change it.
std::shared_ptr< const DecryptedRegularTxsMap > legacyDecryptedFields(
    const DecryptedRegularTxsMap& _map, size_t _firstTxIndex, size_t _endTxIndex );

/// True when a block whose parent has timestamp _parentBlockTimestamp must use the legacy
/// behaviour: from DecryptedTxFieldsLegacyStartPatch (not set = from the first block) until
/// DecryptedTxFieldsPatch is enabled (not set = never ends).
bool useLegacyDecryptedFields( time_t _parentBlockTimestamp );

/// Selects the behaviour for the block and returns its result.
std::shared_ptr< const DecryptedRegularTxsMap > selectDecryptedFields(
    const std::shared_ptr< const DecryptedRegularTxsMap >& _map, size_t _firstTxIndex,
    size_t _endTxIndex, time_t _parentBlockTimestamp );

/// Logs at startup which blocks use the legacy behaviour, and warns when the start marker
/// is in the future: it describes the past, so a wrong value does not fail by itself, the
/// node would only disagree with the other nodes at some later block.
void logDecryptedFieldsWindow( const ChainOperationParams& _chainParams );

}  // namespace patches
}  // namespace eth
}  // namespace dev

#endif  // BITE
