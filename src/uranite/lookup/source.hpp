
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

#ifndef _URANITE_LOOKUP_SOURCE_HPP_
#define _URANITE_LOOKUP_SOURCE_HPP_

#include <cstdint>
#include <memory>
#include <string>

#include "fmt/format.h"

namespace uranite::lookup {
	
	/**
	 * @brief Represents the source information of a specific piece of code or data.
	 */
	struct Source {
		
		/**
		 * @brief Represents a specific point (line and column) within a source file.
		 */
		struct Location {
			
			uint32_t column;
			uint32_t line;
			
			/**
			 * @brief Default constructor initializing location to 0:0.
			 */
			Location() : column( 0 ), line( 0 ) {
			}
			
			/**
			 * @brief Parameterized constructor for a specific location.
			 * @param column The horizontal position.
			 * @param line The vertical position.
			 */
			Location(
				uint32_t column,
				uint32_t line
			) : column( column ),
				line( line ) {
			}
			
			/**
			 * @brief Converts the location to a string format.
			 * @return A string formatted as "line:column".
			 */
			std::string toString() const {
				return fmt::format("{}:{}", this->line, this->column);
			}
			
		};
		
		using LocationSharedPointer = std::shared_ptr<Location>;
		
		/**
		 * @brief Represents a range between two locations in a source file.
		 */
		struct Range {
			
			LocationSharedPointer begin;
			LocationSharedPointer end;
			
			/**
			 * @brief Default constructor initializing begin and end to default locations.
			 */
			Range() : begin( std::make_shared<Location>() ), end( std::make_shared<Location>() ) {
			}
			
			/**
			 * @brief Parameterized constructor for a specific range.
			 * @param begin Shared pointer to the start location.
			 * @param end Shared pointer to the end location.
			 */
			Range(
				const LocationSharedPointer& begin,
				const LocationSharedPointer& end
			) : begin( begin ),
				end( end ) {
			}
			
			/**
			 * @brief Converts the range to a string format.
			 * @return A string formatted as "begin_line:begin_column:end_line:end_column".
			 */
			std::string toString() const {
				return fmt::format( "{}:{}", this->begin->toString(), this->end->toString() );
			}
			
		};
		
		using RangeSharedPointer = std::shared_ptr<Range>;
		
		/** @brief Diagnostic filename */
		std::string filename;
		
		/** @brief Diagnostic location */
		LocationSharedPointer location;
		
		/** @brief Diagnostic pathname of filename */
		std::string pathname;
		
		/** @brief Diagnostic range */
		RangeSharedPointer range;
		
		/**
		 * @brief Default constructor initializing with "<unknown>" values.
		 */
		Source() :
			filename( "<unknown>" ),
			location( std::make_shared<Location>() ),
			pathname( "<unknown>" ),
			range( std::make_shared<Range>() ) {
		}
		
		/**
		 * @brief Parameterized constructor for full source details.
		 * @param filename The name of the file.
		 * @param pathname The full path to the file.
		 * @param range Shared pointer to the range within the file.
		 */
		Source(
			const std::string& filename,
			const std::string& pathname,
			const LocationSharedPointer& location,
			const RangeSharedPointer& range
		) : filename( filename ),
			location( location ),
			pathname( pathname ),
			range( range ) {
		}
		
		/**
		 * @brief Converts the source information to a string format.
		 * @return A string formatted as "filename:range_details".
		 */
		std::string toString() const {
			return fmt::format( "{}:{}", this->filename, this->range->toString() );
		}
		
	};
	
	using LocationSharedPointer = Source::LocationSharedPointer;
	using RangeSharedPointer = Source::RangeSharedPointer;
	using SourceSharedPointer = std::shared_ptr<Source>;
	
} // namespace uranite::lookup

#endif // end _URANITE_LOOKUP_SOURCE_HPP_
