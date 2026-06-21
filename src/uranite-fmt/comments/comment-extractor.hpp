
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

#ifndef _URANITE_FMT_COMMENTS_COMMENT_EXTRACTOR_HPP_
#define _URANITE_FMT_COMMENTS_COMMENT_EXTRACTOR_HPP_

#include <cstdint>
#include <string>
#include <vector>

namespace uranite::formatter::comments {
	
	/**
	 * @enum CommentKind
	 * @brief Classifies the structural type of a captured comment token.
	 * * Distinguishes between normal code descriptions and structured api documentation metadata within Aether.
	 */
	enum class CommentKind {
		SingleLine, ///< A standard C++ style single-line comment (e.g., `// ...`).
		DocComment  ///< A structured documentation block comment (e.g., `/** ... */`).
	};
	
	/**
	 * @enum AttachmentStrategy
	 * @brief Defines how a comment block binds or relates to surrounding AST nodes.
	 * * Used by the formatter and linter engines to position code nodes safely around comments 
	 * without causing positional displacement during rewriting.
	 */
	enum class AttachmentStrategy {
		LeadingDeclaration, ///< Comment precedes and documents an upcoming entity declaration (e.g., functions, classes).
		TrailingStatement,  ///< Comment is appended at the end of a statements' line (e.g., `x = 1; // reset counter`).
		FloatingBlock       ///< Comment acts as an isolated section separator or multi-line general text block.
	};
	
	/**
	 * @struct CommentEntry
	 * @brief Encapsulates a tokenized comment block accompanied by its geometric coordinate metadata.
	 * * Preserves text footprints and location parameters, which are crucial for the `CommentReattacher` 
	 * to re-inject text chunks back into formatted outputs.
	 */
	struct CommentEntry {
		/** @brief The structural classification tier of the captured comment. */
		CommentKind commentKind;
		
		/** @brief The calculated binding strategy determining how this comment anchors to code layout streams. */
		AttachmentStrategy attachmentStrategy;
		
		/** @brief The raw textual payload string extracted from inside the comment bounds. */
		std::string commentContent;
		
		/** @brief The physical line index coordinate where the comment starts in the original file. */
		uint32_t startLine;
		
		/** @brief The physical column character index coordinate where the comment starts. */
		uint32_t startColumn;
		
		/** @brief The physical line index coordinate where the comment block terminates. */
		uint32_t endLine;
		
		/** @brief The physical column character index coordinate where the comment block terminates. */
		uint32_t endColumn;
		
	};
	
	/**
	 * @class CommentExtractor
	 * @brief An isolated lexing engine dedicated to parsing and localizing comment blocks out of source streams.
	 *
	 * This class runs a micro-tokenization pass over the source code, independent of the main compiler lexer, 
	 * capturing text contents and positional scopes for both single-line comments and multi-line doccomments.
	 */
	class CommentExtractor {
		
		public:
			
			/**
			 * @brief Constructs a CommentExtractor bound to an in-memory source string buffer.
			 * @param sourceText Raw source code contents to scan.
			 * @param sourceFilePath Target filesystem filepath string used for logging context tracking.
			 */
			CommentExtractor( const std::string& sourceText, const std::string& sourceFilePath );
			
			/**
			 * @brief Scans the text payload to harvest an ordered collection of all embedded comments.
			 * * Iterates individual text frames from the current cursor position until it reaches the end of the file.
			 * @return std::vector<CommentEntry> A vector listing all successfully extracted comment instances.
			 */
			std::vector<CommentEntry> extract();
			
		private:
			
			/**
			 * @brief Increments the linear text reader cursor position and shifts the geometric column index.
			 */
			void advanceCursor();
			
			/**
			 * @brief Shuts down character positions on the current line and wraps tracking coordinates to the next line boundary.
			 */
			void advanceLine();
			
			/**
			 * @brief Returns the character token resting directly underneath the reader cursor tracking layout.
			 * @return char The character token byte, or `\0` if the index outrides file limits.
			 */
			char currentCharacter() const;
			
			/**
			 * @brief Heuristically deduces the structural attachment strategy for a comment segment based on line coordinates.
			 * * Looks ahead to check if the upcoming non-blank line contains a structural entity declaration or a regular statement block.
			 * * @param commentStartLine The line index where the comment token sequence began.
			 * @param commentEndLine The line index where the comment token sequence concluded.
			 * @return AttachmentStrategy The calculated strategy label.
			 */
			AttachmentStrategy determineAttachment( uint32_t commentStartLine, uint32_t commentEndLine ) const;
			
			/**
			 * @brief Gathers text frames within multi-line block comments (`/** ... *\/`) to extract documentation text.
			 * @return CommentEntry Compiled comment metadata payload record.
			 */
			CommentEntry extractDocComment();
			
			/**
			 * @brief Gathers text frames starting from single-line forward slashes (`//`) up to the immediate line feed terminal.
			 * @return CommentEntry Compiled comment metadata payload record.
			 */
			CommentEntry extractSingleLineComment();
			
			/**
			 * @brief Verifies if the reader cursor position has completely reached or bypassed the end boundary limits of the source text.
			 * @return true If the scanner cannot advance further.
			 * @return false Otherwise.
			 */
			bool isAtEnd() const;
			
			/**
			 * @brief Lookahead helper to verify if the next valid line of code introduces a formal declaration block.
			 * * Evaluates lines past whitespace ranges to identify language declaration keywords (e.g., `fn`, `class`, `struct`).
			 * * @param afterLine Target line index base where the validation query initiates.
			 * @return true If a declaration node is found on the immediate following operational line.
			 * @return false If it points to an execution statement, block scope close, or empty space.
			 */
			bool isNextNonBlankLineDeclaration( uint32_t afterLine ) const;
			
			/**
			 * @brief Observers the character byte resting immediately ahead of the current operational cursor coordinate without moving it.
			 * @return char The previewed character byte payload.
			 */
			char peekNextCharacter() const;
			
			/**
			 * @brief Bypasses all standard blanks or tab stops resting within the bounds of the current line.
			 */
			void skipWhitespaceOnLine();
				
			/** @brief Geometric coordinate tracking the reader cursor's horizontal column position. */
			uint32_t currentColumn_ = 1;
			
			/** @brief Geometric coordinate tracking the reader cursor's vertical line location index. */
			uint32_t currentLine_ = 1;
			
			/** @brief Linear scale value mapping the absolute byte offset of the tracker within the master string payload. */
			size_t cursorPosition_ = 0;
			
			/** @brief System tracker storing the file target file destination address. */
			std::string sourceFilePath_;
			
			/** @brief In-memory reference block containing the original full code payload text string. */
			std::string sourceText_;
		
	};

} // namespace uranite::formatter::comments

#endif // _URANITE_FMT_COMMENTS_COMMENT_EXTRACTOR_HPP_
