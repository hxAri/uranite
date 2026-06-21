
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

#include "uranite-fmt/formatter/source-writer.hpp"

namespace uranite::formatter {
	
	SourceWriter::SourceWriter()
		: currentIndentLevel_( 0 ) {
	}
	
	SourceWriter::SourceWriter( const FormattingRules& formattingRules )
		: currentIndentLevel_( 0 ),
		  formattingRules_( formattingRules ) {
	}
	
	void SourceWriter::increaseIndent() {
		this->currentIndentLevel_++;
	}
	
	void SourceWriter::decreaseIndent() {
		if( this->currentIndentLevel_ > 0 ) {
			this->currentIndentLevel_--;
		}
	}
	
	void SourceWriter::writeIndentation() {
		uint32_t totalSpaces = this->currentIndentLevel_ * this->formattingRules_.indentationWidth;
		for( uint32_t spaceIndex = 0; spaceIndex < totalSpaces; spaceIndex++ ) {
			this->outputBuffer_ << ' ';
		}
	}
	
	void SourceWriter::writeIndentedLine( const std::string& lineContent ) {
		this->writeIndentation();
		this->outputBuffer_ << lineContent << '\n';
	}
	
	void SourceWriter::writeRaw( const std::string& rawContent ) {
		this->outputBuffer_ << rawContent;
	}
	
	void SourceWriter::writeNewline() {
		this->outputBuffer_ << '\n';
	}
	
	void SourceWriter::writeBlankLine() {
		this->outputBuffer_ << '\n';
	}
	
	uint32_t SourceWriter::currentIndentation() const {
		return this->currentIndentLevel_;
	}
	
	std::string SourceWriter::finalize() const {
		return this->outputBuffer_.str();
	}

}
