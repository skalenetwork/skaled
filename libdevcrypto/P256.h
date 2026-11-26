/*
    Copyright (C) 2026 SKALE Labs

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
/**
 * @file P256.h
 * ECDSA signature verification over secp256r1 (NIST P-256), as specified in EIP-7951.
 */

#pragma once

#include <libdevcore/Common.h>

namespace dev {
namespace crypto {

/// EIP-7951 P256VERIFY.
/// Input: 160 bytes = hash(32) | r(32) | s(32) | qx(32) | qy(32), all big-endian.
/// Returns { true, 32-byte 0x..01 } if the signature is valid and { true, {} } otherwise.
std::pair< bool, bytes > p256Verify( bytesConstRef _in );

}  // namespace crypto
}  // namespace dev
