
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

#ifndef _URANITE_IR_MIR_OPTIMIZER_HPP_
#define _URANITE_IR_MIR_OPTIMIZER_HPP_

#include "uranite/ir/mir.hpp"
#include "uranite/ir/mir-analysis.hpp"

namespace uranite::ir::mir {
	
	/** @brief MIR-level optimization passes operating on the control-flow graph. */
	class MIROptimizer {
	public:
		
		/** @brief Runs all optimization passes on every function in the module. */
		void optimize( MIRModuleDefinition& moduleDefinition );
		
		/** @brief Runs all optimization passes on a single function. */
		void optimizeFunction( MIRFunctionDefinition& functionDefinition );
		
		/** @brief Returns total number of instructions removed across all passes. */
		int removedInstructionCount() const;
		
		/** @brief Returns total number of blocks removed across all passes. */
		int removedBlockCount() const;
	
	private:
		
		/** @brief Removes stores whose destination is never read before next store or block exit. */
		bool eliminateDeadStores( MIRFunctionDefinition& functionDefinition );
		
		/** @brief Replaces uses of copied variables with their source when safe. */
		bool propagateCopies( MIRFunctionDefinition& functionDefinition );
		
		/** @brief Evaluates constant arithmetic and comparisons at compile time. */
		bool foldConstants( MIRFunctionDefinition& functionDefinition );
		
		/** @brief Merges linear block chains where predecessor has one successor and successor has one predecessor. */
		bool mergeLinearBlocks( MIRFunctionDefinition& functionDefinition );
		
		/** @brief Removes blocks with no predecessors (except entry block). */
		bool eliminateUnreachableBlocks( MIRFunctionDefinition& functionDefinition );
		
		int totalRemovedInstructions = 0;
		int totalRemovedBlocks = 0;
	
	};

} // namespace uranite::ir::mir

#endif // end _URANITE_IR_MIR_OPTIMIZER_HPP_
