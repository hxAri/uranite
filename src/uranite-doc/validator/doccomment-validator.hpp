
//
// @author hxAri (hxari)
// @create 2026-06-15 11:00
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

#ifndef _URANITE_DOC_VALIDATOR_DOCCOMMENT_VALIDATOR_HPP_
#define _URANITE_DOC_VALIDATOR_DOCCOMMENT_VALIDATOR_HPP_

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "uranite-doc//extractor/doccomment-parser.hpp"
#include "uranite-doc/model/documentation-model.hpp"
#include "uranite-fmt/comments/comment-extractor.hpp"

namespace uranite::doc {
	
	enum class ValidationSeverity {
		Warning,
		Error
	};
	
	struct ValidationDiagnostic {
		ValidationSeverity severity;
		std::string ruleIdentifier;
		std::string diagnosticMessage;
		std::string sourceFilePath;
		uint32_t lineNumber;
		uint32_t columnNumber;
	};
	
	/**
	 * @class DoccommentValidator
	 * @brief Validates conformance, completeness, and correctness of documentation comments against AST entities.
	 *
	 * This class cross-references parsed documentation models with extracted raw comments to identify
	 * discrepancies such as missing parameter documentations, undocumented return types, structural syntax 
	 * errors within the comments, or references to invalid/unknown error types.
	 */
	class DoccommentValidator {
			
		public:
			
			/**
			 * @brief Registers an error type name that is recognized as valid when referenced inside documentation tags.
			 * * Used by the validator to verify that any documentation declaring thrown exceptions or error 
			 * boundaries (e.g., specific compiler panic or semantic failure models) points to an actual, existing type.
			 * * @param errorTypeName The fully qualified string name of the recognized error or exception type.
			 */
			void registerKnownErrorType( const std::string& errorTypeName );
			
			/**
			 * @brief Batch-validates an entire module's documentation layout against its extracted raw comments.
			 * * This acts as the main entry point for the validator, kicking off recursive validation checks 
			 * for all sub-entities declared inside the target module.
			 * * @param moduleDoc Structured data model containing all documented classes, methods, and functions within the module.
			 * @param extractedComments A list of all raw tokens containing comment contents captured during the lexing phase.
			 * @return std::vector<ValidationDiagnostic> A collection of diagnostic warnings or errors generated during the check.
			 */
			std::vector<ValidationDiagnostic> validateModule( const ModuleDocumentation& moduleDoc, const std::vector<formatter::comments::CommentEntry>& extractedComments );
		
		private:
			
			/**
			 * @brief Lookup utility to bind a specific line of declaration to its preceding raw doccomment blocks.
			 * @param declarationLine The line number where the target code entity (e.g., function, class) starts.
			 * @param extractedComments Reference to the master tokenized vector of comments.
			 * @return std::string The raw block content of the matching doccomment, or an empty string if none is found.
			 */
			std::string findRawDoccommentForLine( uint32_t declarationLine, const std::vector<formatter::comments::CommentEntry>& extractedComments ) const;
			
			/**
			 * @brief Performs deep structural verification on a fully parsed doccomment representation.
			 * * Validates that structural tags (such as `@param`, `@return`, `@throws`) perfectly line up 
			 * with the signatures of the associated compiled entity.
			 * * @param doccomment The structured/tokenized model of the documentation block.
			 * @param entityName Name of the code entity being checked (e.g., function identifier).
			 * @param sourceFilePath Path to the source file where the entity resides (for diagnostic reporting).
			 * @param declarationLine The source line number tracking context.
			 * @param extractedComments Reference to the master tokenized vector of comments.
			 */
			void validateDoccomment( const ParsedDoccomment& doccomment, const std::string& entityName, const std::string& sourceFilePath, uint32_t declarationLine, const std::vector<formatter::comments::CommentEntry>& extractedComments );
			
			/**
			 * @brief Dispatches structural validation checks on an individual entity entry.
			 * @param entity The target documentation metadata structure representing an individual code declaration.
			 * @param sourceFilePath Path to the source file where the entity resides.
			 * @param extractedComments Reference to the master tokenized vector of comments.
			 */
			void validateEntity( const EntityDocumentationEntry& entity, const std::string& sourceFilePath, const std::vector<formatter::comments::CommentEntry>& extractedComments );
			
			/**
			 * @brief Checks the raw text string of a doccomment for fundamental syntax violations before structural parsing.
			 * * Scans for micro-syntactic mistakes like unclosed tags, malformed markdown blocks, or invalid Doxygen command keywords.
			 * * @param rawContent The unparsed, raw comment text string.
			 * @param entityName Name of the code entity being checked.
			 * @param sourceFilePath Path to the source file where the entity resides.
			 * @param lineNumber Starting line position of the comment block in the source file.
			 */
			void validateRawDoccommentContent( const std::string& rawContent, const std::string& entityName, const std::string& sourceFilePath, uint32_t lineNumber );
			
			/** @brief Accumulator for all diagnostic feedback (errors and warnings) gathered during validation passes. */
			std::vector<ValidationDiagnostic> diagnostics_;
			
			/** @brief Whitelist hash set storing all valid error/exception types recognized by the system. */
			std::unordered_set<std::string> knownErrorTypes_;
	};

} // namespace uranite::doc

#endif // _URANITE_DOC_VALIDATOR_DOCCOMMENT_VALIDATOR_HPP_
