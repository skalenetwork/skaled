/*
    Copyright (C) 2019-present, SKALE Labs

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
 * @file Exceptions.cpp
 * @author Dima Litvinov
 * @date 2019
 */

#ifndef EXCEPTIONS_CPP
#define EXCEPTIONS_CPP

#include "Exceptions.h"

#include <iostream>

namespace dev {

std::string nested_exception_what( const std::exception& ex ) {
    std::string what = std::string( ex.what() ) + " --- ";
    try {
        std::rethrow_if_nested( ex );
    } catch ( const std::exception& ex2 ) {
        what += nested_exception_what( ex2 );
    } catch ( ... ) {
        what += "unknown exception";
    }
    return what;
}

std::string innermost_exception_what( const std::exception& ex ) {
    std::string what = std::string( ex.what() );
    try {
        std::rethrow_if_nested( ex );
    } catch ( const std::exception& ex2 ) {
        what = innermost_exception_what( ex2 );
    } catch ( ... ) {
        what = "unknown exception";
    }
    return what;
}

void rethrow_most_nested( const std::exception& ex ) {
    //    std::cerr << nested_exception_what(ex) << std::endl;

    const std::nested_exception* nested_ptr = dynamic_cast< const std::nested_exception* >( &ex );
    if ( nested_ptr == nullptr ) {
        throw;  // TODO can we make this func to be called without arguments? what does it really
                // throw?
    }
    try {
        std::rethrow_if_nested( ex );
    } catch ( const std::exception& ex2 ) {
        rethrow_most_nested( ex2 );
    } catch ( ... ) {
        throw;
    }
}

}  // namespace dev
#endif  // EXCEPTIONS_CPP
