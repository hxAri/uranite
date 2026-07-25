
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

#ifndef _URANITE_IR_MIR_BORROW_CHECKER_HPP_
#define _URANITE_IR_MIR_BORROW_CHECKER_HPP_

#include <unordered_map>
#include <vector>

#include "uranite/diagnostic/diagnostic.hpp"
#include "uranite/ir/mir.hpp"

namespace uranite::ir::mir {
	
	/** @brief Ownership state for a single variable at a given program point. */
	enum class BorrowOwnershipState {
		Owned,
		Moved,
		Borrowed,
		MutablyBorrowed,
		Uninitialized,
	};
	
	/** @brief Per-variable ownership state at a block boundary. */
	using OwnershipStateMap = std::unordered_map<MIRVariableIdentifier, BorrowOwnershipState>;
	
	/** @brief Path-sensitive ownership verifier operating on MIR control-flow graph. */
	class MIRBorrowChecker {
	public:
		
		MIRBorrowChecker( diagnostic::Engine& diagnosticEngine );
		
		/** @brief Checks ownership rules for a single function; reports errors via diagnostic engine. */
		void check( MIRFunctionDefinition& functionDefinition );
		
		/** @brief Returns count of ownership violations found. */
		int violationCount() const;
	
	private:
		
		void propagateOwnershipStates( MIRFunctionDefinition& functionDefinition );
		void checkInstruction( const MIRInstruction& instruction, OwnershipStateMap& currentStates,
			const MIRFunctionDefinition& functionDefinition );
		OwnershipStateMap mergeOwnershipMaps( const std::vector<OwnershipStateMap>& predecessorStates );
		void reportViolation( const std::string& message, const lookup::SourceSharedPointer& sourceLocation );
		
		diagnostic::Engine& diagnosticEngine;
		int detectedViolations = 0;
	
	};

} // namespace uranite::ir::mir

#endif // end _URANITE_IR_MIR_BORROW_CHECKER_HPP_
