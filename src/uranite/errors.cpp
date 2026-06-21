
//
//
// @author hxAri (hxari)
// @create 2025-02-24 15:15
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
//

#include <execinfo.h>
#include <iostream>
#include <sstream>

#include "uranite/errors.hpp"

namespace uranite::errors {
	
	Throwable::Throwable() : causes({}), code( 0 ), message( "" ) {
		this->captureStackTrace();
	}
	
	Throwable::Throwable( const std::string &message ) : causes({}), code( 0 ), message( message ) {
		this->captureStackTrace();
	}
	
	Throwable::Throwable( const std::string &message, int code ) : causes({}), code( code ), message( message ) {
		this->captureStackTrace();
	}
	
	Throwable::Throwable( const std::string& message, std::vector<std::exception_ptr> causes ) : causes( causes ), code( 0 ), message( message ) {
		this->captureStackTrace();
	}
	
	Throwable::Throwable( const std::string& message, int code, std::vector<std::exception_ptr> causes ) : causes( causes ), code( code ), message( message ) {
		this->captureStackTrace();
	}
	
	void Throwable::captureStackTrace() {
		void *array[10];
		size_t size = backtrace( array, 10 );
		char **symbols = backtrace_symbols( array, size );
		if( symbols ) {
			for( size_t i=0; i<size; ++i ) {
				this->stackTrace.emplace_back( symbols[i] );
			}
			free( symbols );
		}
	}
	
	int Throwable::getCode() {
		return this->code;
	}
	
	std::string Throwable::getMessage() {
		return this->message;
	}
	
	std::string Throwable::getMessageAll() {
		std::ostringstream oss;
		oss << "Error " << this->code << ": " << message;
		for( const std::exception_ptr &cause : this->causes ) {
			try {
				std::rethrow_exception( cause );
			}
			catch( const std::exception& e ) {
				oss << " | Cause: " << e.what();
			}
		}
		return oss.str();
	}
	
	std::vector<std::string> Throwable::getStackTrace() {
		return this->stackTrace;
	}
	
	void Throwable::printStackTrace() const {
		std::cerr << "Stack Trace:" << std::endl;
        for( const std::string& frame : stackTrace ) {
            std::cerr << frame << std::endl;
        }
	}
	
	const char* Throwable::what() const noexcept {
		return this->message.c_str();
	}
	
}
