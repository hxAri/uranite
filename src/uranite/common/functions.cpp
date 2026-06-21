
//
// @author hxAri (hxari)
// @create 2025-02-24 15:15
// @update 2026-06-17 20:03
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
// Uranite Licence under GNU General Public Licence v3
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include <cxxabi.h>
#include <regex>

#include "uranite/common/functions.hpp"

#include "spdlog/spdlog.h"

namespace uranite::common::functions {
	
	bool isupper( const std::string &strings ) {
		if( strings.empty() == false ) {
			for( size_t i=0; i<strings.length(); i++ ) {
				if( std::islower( strings[i] ) ) {
					return false;
				}
			}
		}
		return true;
	}
	
	int printerr( const std::exception &e ) {
		int status( 0 );
		std::string namedtype( typeid( e ).name() );
		std::string message( e.what() );
		message = replaceln( message, "\x0a\x20\x2b\x20" );
		message = replacetb( message, "\x2e\x2e\x2e\x2e" );
		char *demangledtype( abi::__cxa_demangle( namedtype.c_str(), nullptr, nullptr, &status ) );
		if( status ) {
			spdlog::error( "printerr: {}: {}", namedtype, message );
			return status;
		}
		else {
			spdlog::error( "printerr: {}: {}", demangledtype, message );
		}
		return 1;
	}
	
	std::string removeprefix( const std::string &strings, const std::string &character ) {
		if( strings.size() >= character.size() &&
			strings.compare( 0, character.size(), character ) == 0 ) {
			return strings.substr( character.size() );
		}
		return strings;
	}
	
	std::string removesuffix( const std::string &strings, const std::string &character ) {
		if( strings.size() >= character.size() &&
			strings.compare( strings.size() - character.size(), character.size(), character ) == 0 ) {
			strings.substr( 0, strings.size() - character.size() );
		}
		return strings;
	}
	
	std::string replacech( const std::string &strings, const std::string &character, const std::string &replace ) {
		return std::regex_replace( strings, std::regex( character ), replace );
	}
	
	std::string replaceln( const std::string &strings ) {
		return replaceln( strings, "\x0a\x09" );
	}
	
	std::string replaceln( const std::string &strings, const std::string &replace ) {
		return replacech( strings, "\x0a", replace );
	}
	
	std::string replacetb( const std::string &strings ) {
		return replaceln( strings, "\x20\x20\x20\x20" );
	}
	
	std::string replacetb( const std::string &strings, const std::string &replace ) {
		return replacech( strings, "\x09", replace );
	}
	
	std::vector<std::string> split( const std::string &strings, char delimiter ) {
		std::vector<std::string> tokens;
		std::istringstream stream( strings );
		std::string token;
		while( std::getline( stream, token, delimiter ) ) {
			tokens.push_back( token );
		}
		return tokens;
	}
	
}
