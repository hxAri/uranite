
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

#include "uranite-fmt/comments/comment-extractor.hpp"

namespace uranite::formatter::comments {
	
	CommentExtractor::CommentExtractor( const std::string& sourceText, const std::string& sourceFilePath )
		: sourceText_( sourceText ), sourceFilePath_( sourceFilePath ) {
	}
	
	bool CommentExtractor::isAtEnd() const {
		return this->cursorPosition_ >= this->sourceText_.size();
	}
	
	char CommentExtractor::currentCharacter() const {
		if( this->isAtEnd() ) {
			return '\0';
		}
		return this->sourceText_[this->cursorPosition_];
	}
	
	char CommentExtractor::peekNextCharacter() const {
		size_t nextPosition = this->cursorPosition_ + 1;
		if( nextPosition >= this->sourceText_.size() ) {
			return '\0';
		}
		return this->sourceText_[nextPosition];
	}
	
	void CommentExtractor::advanceCursor() {
		if( this->isAtEnd() == false ) {
			if( this->sourceText_[this->cursorPosition_] == '\n' ) {
				this->currentLine_++;
				this->currentColumn_ = 1;
			}
			else {
				this->currentColumn_++;
			}
			this->cursorPosition_++;
		}
	}
	
	void CommentExtractor::advanceLine() {
		while( this->isAtEnd() == false && this->sourceText_[this->cursorPosition_] != '\n' ) {
			this->cursorPosition_++;
			this->currentColumn_++;
		}
		if( this->isAtEnd() == false ) {
			this->cursorPosition_++;
			this->currentLine_++;
			this->currentColumn_ = 1;
		}
	}
	
	void CommentExtractor::skipWhitespaceOnLine() {
		while( this->isAtEnd() == false ) {
			char character = this->sourceText_[this->cursorPosition_];
			if( character == ' ' || character == '\t' ) {
				this->cursorPosition_++;
				this->currentColumn_++;
			}
			else {
				break;
			}
		}
	}
	
	CommentEntry CommentExtractor::extractSingleLineComment() {
		CommentEntry entry;
		entry.commentKind = CommentKind::SingleLine;
		entry.startLine = this->currentLine_;
		entry.startColumn = this->currentColumn_;
		size_t lineStartPosition = this->cursorPosition_;
		this->advanceLine();
		size_t lineEndPosition = this->cursorPosition_;
		if( lineEndPosition > 0 && this->sourceText_[lineEndPosition - 1] == '\n' ) {
			lineEndPosition--;
		}
		entry.commentContent = this->sourceText_.substr( lineStartPosition, lineEndPosition - lineStartPosition );
		entry.endLine = this->currentLine_ > entry.startLine ? this->currentLine_ - 1 : entry.startLine;
		entry.endColumn = static_cast<uint32_t>( lineEndPosition - lineStartPosition ) + entry.startColumn;
		entry.attachmentStrategy = this->determineAttachment( entry.startLine, entry.endLine );
		return entry;
	}
	
	CommentEntry CommentExtractor::extractDocComment() {
		CommentEntry entry;
		entry.commentKind = CommentKind::DocComment;
		entry.startLine = this->currentLine_;
		entry.startColumn = this->currentColumn_;
		size_t docStartPosition = this->cursorPosition_;
		this->cursorPosition_+= 3;
		this->currentColumn_+= 3;
		bool foundClosingDelimiter = false;
		while( this->isAtEnd() == false ) {
			if( this->sourceText_[this->cursorPosition_] == '"' &&
				this->cursorPosition_ + 2 < this->sourceText_.size() &&
				this->sourceText_[this->cursorPosition_ + 1] == '"' &&
				this->sourceText_[this->cursorPosition_ + 2] == '"' ) {
				this->cursorPosition_+= 3;
				this->currentColumn_+= 3;
				foundClosingDelimiter = true;
				break;
			}
			if( this->sourceText_[this->cursorPosition_] == '\n' ) {
				this->currentLine_++;
				this->currentColumn_ = 1;
			}
			else {
				this->currentColumn_++;
			}
			this->cursorPosition_++;
		}
		entry.commentContent = this->sourceText_.substr( docStartPosition, this->cursorPosition_ - docStartPosition );
		entry.endLine = this->currentLine_;
		entry.endColumn = this->currentColumn_;
		entry.attachmentStrategy = AttachmentStrategy::LeadingDeclaration;
		return entry;
	}
	
	AttachmentStrategy CommentExtractor::determineAttachment( uint32_t commentStartLine, uint32_t commentEndLine ) const {
		if( this->isNextNonBlankLineDeclaration( commentEndLine ) ) {
			return AttachmentStrategy::LeadingDeclaration;
		}
		size_t searchPosition = 0;
		uint32_t lineCounter = 1;
		while( searchPosition < this->sourceText_.size() && lineCounter < commentStartLine ) {
			if( this->sourceText_[searchPosition] == '\n' ) {
				lineCounter++;
			}
			searchPosition++;
		}
		bool hasCodeBeforeComment = false;
		size_t lineStartSearch = searchPosition;
		while( lineStartSearch > 0 && this->sourceText_[lineStartSearch - 1] != '\n' ) {
			lineStartSearch--;
		}
		for( size_t checkPosition = lineStartSearch; checkPosition < searchPosition; checkPosition++ ) {
			char character = this->sourceText_[checkPosition];
			if( character != ' ' && character != '\t' ) {
				hasCodeBeforeComment = true;
				break;
			}
		}
		if( hasCodeBeforeComment ) {
			return AttachmentStrategy::TrailingStatement;
		}
		return AttachmentStrategy::FloatingBlock;
	}
	
	bool CommentExtractor::isNextNonBlankLineDeclaration( uint32_t afterLine ) const {
		size_t searchPosition = 0;
		uint32_t lineCounter = 1;
		while( searchPosition < this->sourceText_.size() && lineCounter <= afterLine ) {
			if( this->sourceText_[searchPosition] == '\n' ) {
				lineCounter++;
			}
			searchPosition++;
		}
		while( searchPosition < this->sourceText_.size() ) {
			char character = this->sourceText_[searchPosition];
			if( character == '\n' ) {
				searchPosition++;
				continue;
			}
			if( character == ' ' || character == '\t' ) {
				searchPosition++;
				continue;
			}
			std::string remainingText = this->sourceText_.substr( searchPosition, 120 );
			if( remainingText.find( "public " ) == 0 ||
				remainingText.find( "protect " ) == 0 ||
				remainingText.find( "private " ) == 0 ||
				remainingText.find( "function " ) == 0 ||
				remainingText.find( "class " ) == 0 ||
				remainingText.find( "interface " ) == 0 ||
				remainingText.find( "struct " ) == 0 ||
				remainingText.find( "enum " ) == 0 ||
				remainingText.find( "extern " ) == 0 ||
				remainingText.find( "const " ) == 0 ||
				remainingText.find( "property " ) == 0 ||
				remainingText.find( "async " ) == 0 ) {
				return true;
			}
			return false;
		}
		return false;
	}
	
	std::vector<CommentEntry> CommentExtractor::extract() {
		std::vector<CommentEntry> extractedComments;
		this->cursorPosition_ = 0;
		this->currentLine_ = 1;
		this->currentColumn_ = 1;
		while( this->isAtEnd() == false ) {
			char character = this->currentCharacter();
			if( character == '#' ) {
				extractedComments.push_back( this->extractSingleLineComment() );
				continue;
			}
			if( character == '"' &&
				this->cursorPosition_ + 2 < this->sourceText_.size() &&
				this->sourceText_[this->cursorPosition_ + 1] == '"' &&
				this->sourceText_[this->cursorPosition_ + 2] == '"' ) {
				extractedComments.push_back( this->extractDocComment() );
				continue;
			}
			if( character == '\'' &&
				this->cursorPosition_ + 2 < this->sourceText_.size() &&
				this->sourceText_[this->cursorPosition_ + 1] == '\'' &&
				this->sourceText_[this->cursorPosition_ + 2] == '\'' ) {
				extractedComments.push_back( this->extractDocComment() );
				continue;
			}
			if( character == '"' ) {
				this->advanceCursor();
				while( this->isAtEnd() == false && this->currentCharacter() != '"' ) {
					if( this->currentCharacter() == '\\' ) {
						this->advanceCursor();
					}
					this->advanceCursor();
				}
				if( this->isAtEnd() == false ) {
					this->advanceCursor();
				}
				continue;
			}
			if( character == '\'' ) {
				this->advanceCursor();
				while( this->isAtEnd() == false && this->currentCharacter() != '\'' ) {
					if( this->currentCharacter() == '\\' ) {
						this->advanceCursor();
					}
					this->advanceCursor();
				}
				if( this->isAtEnd() == false ) {
					this->advanceCursor();
				}
				continue;
			}
			this->advanceCursor();
		}
		return extractedComments;
	}

}
