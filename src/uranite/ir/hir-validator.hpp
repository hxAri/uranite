
//
// @author hxAri (hxari)
// @create 13-06-2026
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

#ifndef _URANITE_IR_HIR_VALIDATOR_HPP_
#define _URANITE_IR_HIR_VALIDATOR_HPP_

#include <string>
#include <vector>

#include "uranite/diagnostic/diagnostic.hpp"
#include "uranite/ir/hir.hpp"

namespace uranite::ir::hir {
	
	/** @brief Validates structural invariants of an HIR module after lowering. */
	class HIRValidator {
	public:
		
		HIRValidator( diagnostic::Engine& diagnosticEngine );
		
		/** @brief Validates the entire HIR module; returns true if no errors found. */
		bool validate( const HIRModule& hirModule );
		
		/** @brief Returns collected validation error messages. */
		const std::vector<std::string>& validationErrors() const;
	
	private:
		
		void validateFunctionDefinition( const HIRFunctionDefinition& functionDefinition );
		void validateClassDefinition( const HIRClassDefinition& classDefinition );
		void validateStructDefinition( const HIRStructDefinition& structDefinition );
		void validateEnumDefinition( const HIREnumDefinition& enumDefinition );
		void validateInterfaceDefinition( const HIRInterfaceDefinition& interfaceDefinition );
		void validateBlock( const HIRBlock& block );
		void validateNode( const HIRNodeSharedPointer& node );
		void validateExpression( const HIRNodeSharedPointer& expression );
		
		void addError( const std::string& errorMessage, const lookup::SourceSharedPointer& sourceLocation );
		
		diagnostic::Engine& diagnosticEngine;
		std::vector<std::string> collectedErrors;
	
	};

} // namespace uranite::ir::hir

#endif // end _URANITE_IR_HIR_VALIDATOR_HPP_
