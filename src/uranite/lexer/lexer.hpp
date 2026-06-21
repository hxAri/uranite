
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

#ifndef _URANITE_LEXER_LEXER_HPP_
#define _URANITE_LEXER_LEXER_HPP_

#include <filesystem>
#include <stack>
#include <string>
#include <vector>

#include "uranite/diagnostic/diagnostic.hpp"
#include "uranite/lookup/source.hpp"
#include "uranite/token/token.hpp"

namespace uranite::lexer {
	
	/**
	 * @brief Performs lexical analysis to convert source code into a stream of tokens.
	 * 
	 * The Lexer scans the input string, handles whitespace, comments, and indentation 
	 * sensitivity, and produces a vector of tokens used by the parser. It also 
	 * maintains source line information for diagnostic reporting.
	 */
	class Lexer {
		
		public:
			
			/**
			 * @brief Constructs a new Lexer instance.
			 * @param source The raw source code string.
			 * @param filename The name of the file being processed.
			 * @param diagnostic Reference to the diagnostic engine for error reporting.
			 */
			Lexer( const std::string& source, const std::string& filename, diagnostic::Engine& diagnostic );
			
			/**
			 * @brief Retrieves the source code split into individual lines.
			 * @return A constant reference to the vector of source lines.
			 */
			inline const std::vector<std::string>& sourceLines() const {
				return this->lines;
			}
			
			/**
			 * @brief Scans the source string and generates a list of tokens.
			 * @return A vector of processed Token objects.
			 */
			std::vector<token::Token> tokenize();
			
		private:
			
			/**
			 * @brief Consumes the current character and moves the cursor forward.
			 * @return The character that was just consumed.
			 */
			char advance();
			
			/**
			 * @brief Returns the character at the current cursor position.
			 * @return The current character.
			 */
			char current() const;
			
			/**
			 * @brief Generates a source location object based on current line and column.
			 * @return The current Source location.
			 */
			lookup::SourceSharedPointer currentSource() const;
			
			/**
			 * @brief Emits a newline token if one is currently marked as pending.
			 */
			void emitPendingNewline();
			
			/**
			 * @brief Processes indentation levels and emits INDENT/DEDENT tokens.
			 */
			void handleIndentation();
			
			/**
			 * @brief Checks if the cursor has reached the end of the source string.
			 * @return true if no more characters are available.
			 */
			bool isAtEnd() const;
			
			/**
			 * @brief Creates a token with a specific type and lexeme value.
			 * @param kind The type of the token.
			 * @param value The literal string value of the token.
			 * @return The constructed Token.
			 */
			token::Token makeToken( token::Type kind, const std::string& value );
			
			/**
			 * @brief Creates a token with a specific type.
			 * @param kind The type of the token.
			 * @return The constructed Token.
			 */
			token::Token makeToken( token::Type kind );
			
			/**
			 * @brief Consumes the current character if it matches the expected character.
			 * @param expected The character to look for.
			 * @return true if the character matched and was consumed.
			 */
			bool match( char expected );
			
			/**
			 * @brief Looks ahead in the source without consuming characters.
			 * @param offset How many characters to look ahead (default is 1).
			 * @return The character at the offset position.
			 */
			char peek( int offset = 1 ) const;
			
			/**
			 * @brief Scans a character literal.
			 * @return A Token representing the character.
			 */
			token::Token readChar();
			
			/**
			 * @brief Scans an identifier and determines if it is a reserved keyword.
			 * @return A Token representing the identifier or keyword.
			 */
			token::Token readIdentifierOrKeyword();
			
			/**
			 * @brief Scans a numeric literal (integer or floating point).
			 * @return A Token representing the number.
			 */
			token::Token readNumber();
			
			/**
			 * @brief Scans a string literal.
			 * @return A Token representing the string.
			 */
			token::Token readString();
			
			/**
			 * @brief Skips over a multi-line block comment.
			 */
			void skipBlockComment();
			
			/**
			 * @brief Skips over a single-line comment.
			 */
			void skipLineComment();
			
			/**
			 * @brief Skips over a triple-quoted comment block.
			 * @param quote The quote character used (single or double).
			 */
			void skipTripleQuoteComment( char quote );
			
			/** @brief Flag indicating if the cursor is currently at the start of a new line. */
			bool atLineStart = true;
			
			/** @brief The current column number (1-based). */
			uint32_t column = 1;
			
			/** @brief Reference to the diagnostic engine for error handling. */
			diagnostic::Engine& diagnostic;
			
			/** @brief The name of the file being tokenized. */
			std::string filename;
			
			/** @brief The path of the file being tokenized. */
			std::filesystem::path filepath;
			
			/** @brief Stack used to track indentation levels for block-based scoping. */
			std::stack<int> indentStack;
			
			/** @brief The current line number (1-based). */
			uint32_t line = 1;
			
			/** @brief Internal storage for source code lines used for error context. */
			std::vector<std::string> lines;
			
			/** @brief Tracks nested parentheses depth to suppress certain indentation rules. */
			int parenDepth = 0;
			
			/** @brief Indicates that a newline token should be emitted at the next opportunity. */
			bool pendingNewline = false;
			
			/** @brief The current byte position within the source string. */
			size_t position = 0;
			
			/** @brief The raw input source code string. */
			std::string source;
			
			/** @brief The list of tokens generated during analysis. */
			std::vector<token::Token> tokens;
		
	};
	
} // namespace uranite::lexer

#endif // end _URANITE_LEXER_LEXER_HPP_
