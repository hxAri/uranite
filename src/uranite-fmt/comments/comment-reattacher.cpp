
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

#include "uranite-fmt/comments/comment-reattacher.hpp"

#include <sstream>

namespace uranite::formatter::comments {
	
	CommentReattacher::CommentReattacher( 
		const std::vector<CommentEntry>& extractedComments 
	) : extractedComments_( extractedComments ) {
	}
	
	std::string CommentReattacher::reattach( const std::string& formattedSource, const std::string& originalSource ) {
		
		// For the initial implementation, comments that were at the file header
		// (before any code) are prepended to the formatted output. Doccomments
		// attached to declarations are preserved through the AST since the
		// formatter knows about them from the extraction pass.
		//
		// Full positional reattachment (mapping old source positions to new
		// formatted positions) is a Phase 2 refinement.
		
		std::ostringstream outputStream;
		bool hasEmittedFileHeader = false;
		for( const CommentEntry& commentEntry : this->extractedComments_ ) {
			if( commentEntry.attachmentStrategy == AttachmentStrategy::FloatingBlock ||
				commentEntry.attachmentStrategy == AttachmentStrategy::LeadingDeclaration ) {
				if( commentEntry.commentKind == CommentKind::SingleLine && commentEntry.startLine <= 25 ) {
					if( hasEmittedFileHeader == false ) {
						hasEmittedFileHeader = true;
					}
					outputStream << commentEntry.commentContent << "\n";
				}
			}
		}
		if( hasEmittedFileHeader ) {
			outputStream << "\n";
		}
		outputStream << formattedSource;
		return outputStream.str();
	}

}
