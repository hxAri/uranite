
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

#ifndef _URANITE_COMMON_FUNCTIONS_HPP_
#define _URANITE_COMMON_FUNCTIONS_HPP_

#include <memory>
#include <string>
#include <vector>

namespace uranite::common::functions {
	
	namespace cast {
		
		/**
		 * @brief Safely casts a std::shared_ptr from one type to another
		 *
		 * This function performs a dynamic cast on a shared_ptr. It is a wrapper
		 * around std::dynamic_pointer_cast to provide a cleaner syntax for
		 * downcasting or cross-casting in a class hierarchy
		 *
		 * @param from The source shared_ptr to be cast
		 * 
		 * @tparam To The target type to cast to
		 * @tparam From The source type of the provided shared_ptr
		 * 
		 * @return std::shared_ptr<To> A shared_ptr to the target type
		 *	Returns nullptr if the cast fails or if 'from' is null
		 *
		 * @note This function requires Run-Time Type Information (RTTI) to be enabled
		 * 
		 */
		template<typename From, typename To>
		std::shared_ptr<To> shared( std::shared_ptr<From> from ) {
			return std::dynamic_pointer_cast<To>( from );
		}
		
	} // nemaspace uranite::common::functions::cast
	
	
	/**
	 * Check if vector contains element
	 * 
	 * @param vectors
	 *  Vector items
	 * @param value
	 *  Element to be check
	 * 
	 * @return
	 *  Return True if the vector contains an element, False otherwise
	 */
	template <typename T>
	bool vexists( const T &value, const std::vector<T> &vectors ) {
		for( const T &vector : vectors ) {
			if( vector == value ) {
				return true;
			}
		}
		return false;
	}
	
	/** @brief Returns whether string is uppercase */
	bool isupper( const std::string &strings );
	
	/** @brief Prints throwned exeption */
	int printerr( const std::exception &e );
	
	/** @brief Returns removed prefix in strings */
	std::string removeprefix( const std::string &strings, const std::string &character );
	
	/** @brief Returns removed suffix in strings */
	std::string removesuffix( const std::string &strings, const std::string &character );
	
	/** @brief Returns replaced character in strings */
	std::string replacech( const std::string &strings, const std::string &character, const std::string &replace );
	
	/** @brief Returns replaced newline in strings */
	std::string replaceln( const std::string &strings );
	
	/** @copydoc replaceln( const std::string &strings ) */
	std::string replaceln( const std::string &strings, const std::string &replace );
	
	/** @brief Returns replaced tabs in strings */
	std::string replacetb( const std::string &strings );
	
	/** @copydoc replacetb( const std::string &strings ) */
	std::string replacetb( const std::string &strings, const std::string &replace );
	
	/** @brief Returns splited string by delimiter */
	std::vector<std::string> split( const std::string &strings, char delimiter );
	
} // namespace uranite::common::functions

#endif // end _URANITE_COMMON_FUNCTIONS_HPP_
