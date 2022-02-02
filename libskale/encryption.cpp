/*
    Copyright (C) 2018-present, SKALE Labs

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
 * @file encryption.cpp
 * @author Dmytro Nazarenko
 * @date 2022
 */

#include "encryption.h"
#include <libethereum/Transaction.h>
#include <libethcore/TransactionBase.h>
#include <algorithm>

std::shared_ptr<std::vector<uint8_t>> EncryptedTransactionAnalyzer::getEncryptedData( const std::vector<uint8_t>& _transaction ) {
    dev::eth::Transaction t( _transaction, dev::eth::CheckTransaction::None );
    auto data = t.data();
    auto first = std::search(data.begin(), data.end(), TE_MAGIC_START.begin(), TE_MAGIC_START.end());
    auto last = std::search(data.begin(), data.end(), TE_MAGIC_END.begin(), TE_MAGIC_END.end());
    if (first != data.end() && last != data.end() && std::distance(first, last) > 0) {
        return std::make_shared<std::vector<uint8_t>> (first + TE_MAGIC_START.size(), last);
    }
    return nullptr;
}; 