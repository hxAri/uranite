
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

#include "uranite/diagnostic/diagnostic.hpp"

#include "fmt/core.h"
#include "fmt/color.h"

namespace uranite::diagnostic {
	
	Engine::Engine() = default;
	
	Engine::Engine(
		uint32_t errorMaximum,
		uint32_t warningMaximum
	) : errorMaximum( errorMaximum ),
		warningMaximum( warningMaximum ) {
	}
	
	void Engine::emit( Level level, const lookup::SourceSharedPointer& source, const std::string& messages, const std::string& hinting ) {
		this->vdiagnostics.emplace_back( level, source, messages, hinting );
		switch( level ) {
			case Level::Error:
			case Level::Fatal:
				this->errorCounts++;
				break;
			case Level::Warning:
				this->warningCounts++;
				break;
			default:
				break;
		}
		if( this->printable ) {
			this->printDiagnostic( this->vdiagnostics.back() );
		}
		if( this->errorMaximum > 0 && this->errorCounts >= this->errorMaximum ) {
			throw DiagnosticLimitReachedError( "Diagnostic error limit reached" );
		}
	}
	
	void Engine::error( const lookup::SourceSharedPointer& source, const std::string& messages, const std::string& hinting ) {
		this->emit( Level::Error, source, messages, hinting );
	}
	
	void Engine::fatal( const lookup::SourceSharedPointer& source, const std::string& messages ) {
		this->emit( Level::Fatal, source, messages );
	}
	
	void Engine::note( const lookup::SourceSharedPointer& source, const std::string& messages ) {
		this->emit( Level::Note, source, messages );
	}
	
	void Engine::printAll() const {
		for( const diagnostic::Diagnostic& diagnostic : this->vdiagnostics ) {
			this->printDiagnostic( diagnostic );
		}
	}
	
	void Engine::printDiagnostic( const Diagnostic& diagnostic ) const {
		fmt::color color = fmt::color::white;
		fmt::print( stderr, fmt::fg( fmt::color::white ) | fmt::emphasis::bold, "{}:{}:{}: ", diagnostic.source->filename, diagnostic.source->location->line, diagnostic.source->location->column );
		fmt::print( stderr, fmt::fg( color ) | fmt::emphasis::bold, "{}: ", toString( diagnostic.level ) );
		fmt::print( stderr, fmt::fg( fmt::color::white ) | fmt::emphasis::bold, "{}\n", diagnostic.messages );
		if( this->sourceLines && diagnostic.source->location->line > 0 && diagnostic.source->location->line <= this->sourceLines->size() ) {
			const std::string& line = (* this->sourceLines )[( diagnostic.source->location->line - 1 )];
			fmt::print( stderr, " {:>4} | {}\n", diagnostic.source->location->line, line );
			if( diagnostic.source->location->column > 0 ) {
				std::string pointer( diagnostic.source->location->column - 1 + 7, ' ' );
				fmt::print( stderr, fmt::fg( color ), "{}^\n", pointer );
			}
		}
		if( diagnostic.hinting.empty() == false ) {
			fmt::print( stderr, fmt::fg( fmt::color::green ), "  hint: {}\n", diagnostic.hinting );
		}
	}
	
	void Engine::warning( const lookup::SourceSharedPointer& source, const std::string& messages ) {
		this->emit( Level::Warning, source, messages );
	}
	
	fmt::color color( Level level ) {
		switch( level) {
			case Level::Note:
				return fmt::color::cyan;
			case Level::Error:
				return fmt::color::red;
			case Level::Fatal:
				return fmt::color::magenta;
			case Level::Warning:
				return fmt::color::yellow;
			default:
				return fmt::color::violet;
		}
	}
	
	std::string toString( Level level ) {
		switch( level) {
			case Level::Note:
				return "Note";
			case Level::Error:
				return "Error";
			case Level::Fatal:
				return "Fatal";
			case Level::Warning:
				return "Warning";
			default:
				return "Unspecified";
		}
	}
	
}
