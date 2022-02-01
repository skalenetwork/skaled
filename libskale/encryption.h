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
 * @file State.h
 * @author Dmytro Nazarenko
 * @date 2022
 */

#include <memory>
#include <vector>

class EncryptedTransactionAnalyzer { 

    public:  
        static std::shared_ptr< std::vector< uint8_t > > getEncryptedData( const std::vector<uint8_t>& data ); 

};