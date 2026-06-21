
//
// @author hxAri (hxari)
// @create 2026-06-15 05:00
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

#ifndef _URANITE_FMT_FORMATTER_SOURCE_WRITER_HPP_
#define _URANITE_FMT_FORMATTER_SOURCE_WRITER_HPP_

#include <cstdint>
#include <sstream>
#include <string>

#include "uranite-fmt/formatter/formatting-rules.hpp"

namespace uranite::formatter {
	
	/**
	 * @class SourceWriter
	 * @brief Stateful stream writer optimizing structured code emission with automated indentation management.
	 *
	 * This utility wraps around an in-memory string stream to streamline the text generation pass of the Formatter.
	 * It manages current nesting indentation depths, implements semantic rule-guided space padding, and handles 
	 * line control elements (newlines, blank spaces, and raw block text insertions).
	 */
	class SourceWriter {
			
		public:
			
			/**
			 * @brief Constructs a SourceWriter instance initialized with default formatting parameters.
			 */
			SourceWriter();
			
			/**
			 * @brief Constructs a SourceWriter bound to specific configuration style constraints.
			 * @param formattingRules Configuration parameters containing tab stop rules and width limits.
			 */
			explicit SourceWriter( const FormattingRules& formattingRules );
			
			SourceWriter( const SourceWriter& ) = delete;
			SourceWriter( SourceWriter&& other ) = default;
			SourceWriter& operator=( const SourceWriter& ) = delete;
			SourceWriter& operator=( SourceWriter&& other ) = default;
			
			/**
			 * @brief Inspects the active total spacing character width corresponding to the current indentation level.
			 * @return uint32_t Total space offset characters actively used per line prefix.
			 */
			uint32_t currentIndentation() const;
			
			/**
			 * @brief Decreases the active nesting tab depth level by one unit stop.
			 * * Automatically guards against underflow conditions, capping the layout boundary at level 0.
			 */
			void decreaseIndent();
			
			/**
			 * @brief Unwinds and flushes the underlying string stream buffer to produce the final product string.
			 * @return std::string The complete compiled source text output.
			 */
			std::string finalize() const;
			
			/** @brief Increases the active nesting tab depth level by one unit stop. */
			void increaseIndent();
				
			/** @brief Emits an isolated empty vertical break line into the active buffer. */
			void writeBlankLine();
			
			/** @brief Writes the specific number of padding spaces matching the current indentation level into the current row prefix. */
			void writeIndentation();
			
			/**
			 * @brief Convenience method to inject an entire indented line block into the stream.
			 * * Automatically prefixes the layout with the required indentation spacing and appends 
			 * an implicit trailing newline feed.
			 * * @param lineContent The text content payload string to render.
			 */
			void writeIndentedLine( const std::string& lineContent );
			
			/**
			 * @brief Injects an isolated standard carriage return newline feed (`\n`) into the stream buffer.
			 */
			void writeNewline();
			
			/**
			 * @brief Streams an unformatted string fragment directly into the stream without shifting coordinates or modifying layout rules.
			 * @param rawContent The target text segment.
			 */
			void writeRaw( const std::string& rawContent );
			
		private:
			
			/** @brief Internal scalar depth tracker counting the current active nesting levels. */
			uint32_t currentIndentLevel_ = 0;
			
			/** @brief Copy of the global active style configuration layout rules. */
			FormattingRules formattingRules_;
			
			/** @brief Native memory stream aggregator accumulating the formatted output character arrays. */
			std::ostringstream outputBuffer_;
		
	};
	
} // namespace uranite::formatter

#endif // _URANITE_FMT_FORMATTER_SOURCE_WRITER_HPP_
