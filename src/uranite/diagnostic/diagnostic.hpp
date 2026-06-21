
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

#ifndef _URANITE_DIAGNOSTIC_DIAGNOSTIC_HPP_
#define _URANITE_DIAGNOSTIC_DIAGNOSTIC_HPP_

#include <string>
#include <vector>

#include "uranite/errors.hpp"
#include "uranite/lookup/source.hpp"

namespace uranite::diagnostic {
	
	/**
	 * @brief Severity levels for diagnostic messages.
	 */
	enum class Level {
		Error,
		Fatal,
		Note,
		Warning
	};
	
	/** @brief Return diagnostic level name */
	std::string toString( Level level );
	
	/**
	 * @brief Encapsulates details of a diagnostic event.
	 */
	struct Diagnostic {
		
		std::string hinting;
		
		Level level;
		
		std::string messages;
		
		lookup::SourceSharedPointer source;
		
		/**
		 * @brief Constructs a new Diagnostic object.
		 * @param level The severity level of the diagnostic.
		 * @param source Shared pointer to the source location related to this diagnostic.
		 * @param messages The primary descriptive message.
		 * @param hinting Optional hint or suggestion for resolving the issue.
		 */
		Diagnostic(
			Level level,
			const lookup::SourceSharedPointer& source,
			const std::string& messages,
			const std::string& hinting=""
		) : hinting( hinting ),
			level( level ),
			messages( messages ),
			source( source ) {
		}
		
	};
	
	class DiagnosticLimitReachedError : public errors::Error {
		using Error::Error;
	};
	
	/**
	 * @brief The diagnostic engine responsible for managing and reporting errors and warnings.
	 */
	class Engine final {
		
		private:
			
			uint32_t errorCounts = 0;
			uint32_t errorMaximum = 1;
			
			std::vector<Diagnostic> vdiagnostics;
			const std::vector<std::string>* sourceLines = nullptr;
			
			bool printable = true;
			
			uint32_t warningCounts = 0;
			uint32_t warningMaximum = 0;
		
		public:
			
			/**
			 * @brief Default constructor for the diagnostic engine.
			 */
			Engine();
			
			/**
			 * @brief Parameterized constructor to set limits for errors and warnings.
			 * @param errorMaximum Maximum number of errors allowed before stopping.
			 * @param warningMaximum Maximum number of warnings allowed.
			 */
			explicit Engine(
				uint32_t errorMaximum,
				uint32_t warningMaximum
			);
			
			/**
			 * @brief Returns the list of captured diagnostics.
			 * @return A constant reference to the vector of Diagnostic objects.
			 */
			const std::vector<Diagnostic>& diagnostics() const {
				return this->vdiagnostics;
			}
			
			/**
			 * @brief Reports a general diagnostic event.
			 * @param level The severity level.
			 * @param source The source location.
			 * @param messages The diagnostic message.
			 * @param hinting Optional hinting string.
			 */
			void emit(
				Level level,
				const lookup::SourceSharedPointer& source,
				const std::string& messages,
				const std::string& hinting=""
			);
			
			/**
			 * @brief Reports an error diagnostic.
			 * @param source The source location.
			 * @param messages The error message.
			 * @param hinting Optional hinting string.
			 */
			void error(
				const lookup::SourceSharedPointer& source,
				const std::string& messages,
				const std::string& hinting=""
			);
			
			/**
			 * @brief Gets the current number of reported errors.
			 * @return The error count.
			 */
			uint32_t errorCount() const {
				return this->errorCounts;
			}
			
			/**
			 * @brief Reports a fatal diagnostic that usually terminates processing.
			 * @param source The source location.
			 * @param messages The fatal error message.
			 */
			void fatal(
				const lookup::SourceSharedPointer& source,
				const std::string& messages
			);
			
			/**
			 * @brief Checks if any errors have been reported.
			 * @return True if errorCount > 0, false otherwise.
			 */
			bool hasErrors() const {
				return this->errorCounts > 0;
			}
			
			/**
			 * @brief Gets the maximum allowed error count.
			 * @return The maximum error limit.
			 */
			uint32_t maxErrors() const {
				return this->errorMaximum;
			}
			
			/**
			 * @brief Reports a note diagnostic for informational purposes.
			 * @param source The source location.
			 * @param messages The informational message.
			 */
			void note(
				const lookup::SourceSharedPointer& source,
				const std::string& messages
			);
			
			/**
			 * @brief Prints all captured diagnostics to the output.
			 */
			void printAll() const;
			
			/**
			 * @brief Prints a specific diagnostic object.
			 * @param diagnostic The diagnostic to print.
			 */
			void printDiagnostic( const Diagnostic& diagnostic ) const;
			
			/**
			 * @brief Enables or disables console printing.
			 * @param enable True to enable, false to disable.
			 */
			void setConsolPrintable( bool enable ) {
				this->printable = enable;
			}
			
			/**
			 * @brief Sets the maximum number of errors before the engine halts.
			 * @param maximum The error limit.
			 */
			void setMaxErrors( uint32_t maximum ) {
				this->errorMaximum = maximum;
			}
			
			/**
			 * @brief Sets the source code lines for contextual reporting.
			 * @param lines A reference to a vector of strings containing source lines.
			 */
			void setSourceLines( const std::vector<std::string>& lines ) {
				this->sourceLines = &lines;
			}
			
			/**
			 * @brief Reports a warning diagnostic.
			 * @param source The source location.
			 * @param messages The warning message.
			 */
			void warning(
				const lookup::SourceSharedPointer& source,
				const std::string& messages
			);
			
			/**
			 * @brief Gets the current number of reported warnings.
			 * @return The warning count.
			 */
			uint32_t warningCount() const {
				return this->warningCounts;
			}
		
	};
	
} // namespace uranite::diagnostic

#endif // end _URANITE_DIAGNOSTIC_DIAGNOSTIC_HPP_
