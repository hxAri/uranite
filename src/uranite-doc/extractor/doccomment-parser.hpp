
//
// @author hxAri (hxari)
// @create 2026-06-15 03:00
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

#ifndef _URANITE_DOC_EXTRACTOR_DOCCOMMENT_PARSER_HPP_
#define _URANITE_DOC_EXTRACTOR_DOCCOMMENT_PARSER_HPP_

#include <optional>
#include <string>
#include <vector>

namespace uranite::doc {
	
	/**
	 * @struct ParameterDocumentation
	 * @brief Represents the documented metadata for an individual function or method parameter.
	 * * Holds the parsed semantic components originating from an `@param` documentation tag.
	 */
	struct ParameterDocumentation {
		
		/** @brief The exact variable identifier name of the parameter. */
		std::string parameterName;
		
		/** @brief The resolved or declared data type of the parameter (if specified in the metadata). */
		std::string parameterType;
		
		/** @brief The human-readable description explaining the purpose or constraints of the parameter. */
		std::string descriptionText;
		
	};
	
	/**
	 * @struct ReturnsDocumentation
	 * @brief Captures the documented metadata regarding a function's return value.
	 * * Holds the parsed semantic components originating from a `@return` or `@returns` documentation tag.
	 */
	struct ReturnsDocumentation {
		
		/** @brief The data type returned by the function or method upon successful execution. */
		std::string returnType;
		
		/** @brief The description explaining what the returned value signifies or contains. */
		std::string descriptionText;
		
	};
	
	/**
	 * @struct RaisesDocumentation
	 * @brief Documents an exceptional exit path or error boundary for a code entity.
	 * * Holds the parsed semantic components originating from an `@throws` or `@raises` documentation tag.
	 */
	struct RaisesDocumentation {
		
		/** @brief The specific exception, diagnostic panic, or error type that might be thrown. */
		std::string exceptionType;
		
		/** @brief The exact logical condition or circumstance under which this error is triggered. */
		std::string conditionDescription;
		
	};
	
	/**
	 * @struct ComplexityDocumentation
	 * @brief Encapsulates algorithmic performance guarantees using Big-O notation.
	 * * Holds the parsed semantic components originating from custom performance or complexity tracking tags 
	 * (e.g., `@complexity`). Highly beneficial for low-level systems programming in Aether.
	 */
	struct ComplexityDocumentation {
		
		/** @brief The asymptotic time complexity bounds (e.g., "O(1)", "O(N log N)"). */
		std::string timeComplexity;
		
		/** @brief The asymptotic space/memory overhead guarantees (e.g., "O(1)", "O(N)"). */
		std::string spaceComplexity;
		
	};
	
	/**
	 * @struct ParsedDoccomment
	 * @brief The root structured representation of a fully tokenized and parsed documentation block.
	 * * This model serves as the internal Abstract Syntax Tree (AST) counterpart for doccomments, 
	 * aggregating all micro-parsed sections into clean, typed fields ready for verification or code-gen mapping.
	 */
	struct ParsedDoccomment {
		/** @brief The single-line high-level summary paragraph (the introductory brief description). */
		std::string summaryLine;
		
		/** @brief The detailed multiline description block providing in-depth context or usage examples. */
		std::string extendedDescription;
		
		/** @brief Ordered list of all documented parameter fields. */
		std::vector<ParameterDocumentation> parameters;
		
		/** @brief Optional description of the return value block. Null if the entity returns void or is undocumented. */
		std::optional<ReturnsDocumentation> returns;
		
		/** @brief Collection of all potential error criteria or exceptions thrown by this entity. */
		std::vector<RaisesDocumentation> raises;
		
		/** @brief Optional performance metrics documenting algorithmic complexity limits. */
		std::optional<ComplexityDocumentation> complexity;
	};
	
	/**
	 * @class DoccommentParser
	 * @brief A static utility class responsible for parsing raw documentation comment blocks.
	 *
	 * This class extracts structured documentation tokens, descriptions, and metadata fields 
	 * from unparsed text strings by applying string cleansing, delimiter stripping, and line-by-line micro-parsing.
	 */
	class DoccommentParser {
		
		public:
				
			/**
			 * @brief Parses a raw doccomment string into a structured representation.
			 * * This is the main entry point of the parser. It strips language-specific block delimiters,
			 * normalizes indentations, splits text into discrete lines, and populates a ParsedDoccomment object.
			 * * @param rawDoccommentContent The full, raw block of text captured directly from the source file.
			 * @return ParsedDoccomment A structured object containing categorized fields (e.g., brief, params, returns).
			 */
			static ParsedDoccomment parse( const std::string& rawDoccommentContent );
			
		private:
				
			/**
			 * @brief Measures the number of leading spaces or tabs in a given line.
			 * * Used to determine block-level indentation matching for multiline descriptions 
			 * or code block formatting inside the comment.
			 * * @param line The individual text line to examine.
			 * @return size_t The total count of consecutive leading whitespace characters.
			 */
			static size_t measureIndentation( const std::string& line );
			
			/**
			 * @brief Splitting utility that divides a single string payload into a vector of lines.
			 * * Identifies standard line feed (`\n`) and carriage return (`\r\n`) characters as boundaries.
			 * * @param content The continuous text string to slice.
			 * @return std::vector<std::string> A list of individual strings representing each line.
			 */
			static std::vector<std::string> splitIntoLines( const std::string& content );
			
			/**
			 * @brief Checks if a line begins with a specific documentation command tag or section prefix.
			 * * Scans for patterns like `@param`, `@return`, or custom markers after accounting for layout padding.
			 * * @param line The current text line being examined.
			 * @param sectionName The exact tag identifier to check for (e.g., "@param").
			 * @return true If the line strictly starts with the specified command token.
			 * @return false Otherwise.
			 */
			static bool startsWithSection( const std::string& line, const std::string& sectionName );
			
			/**
			 * @brief Cleanses the comment block by removing comment wrappers.
			* * @param rawContent The fully enclosed raw source comment string.
			* @return std::string The pure internal text payload free of structural language delimiters.
			*/
			static std::string stripDelimiters( const std::string& rawContent );
			
			/**
			 * @brief Removes whitespace characters exclusively from the beginning of a text string.
			 * @param text The input string to modify.
			 * @return std::string The modified string containing only trailing or internal spacing.
			 */
			static std::string trimLeadingWhitespace( const std::string& text );
			
			/**
			 * @brief Sanitizes a text string by trimming all leading and trailing whitespace blocks.
			 * @param text The input target string.
			 * @return std::string The tightly bounded text result.
			 */
			static std::string trimWhitespace( const std::string& text );
		
	};

} // namespace uranite::doc

#endif // _URANITE_DOC_EXTRACTOR_DOCCOMMENT_PARSER_HPP_
