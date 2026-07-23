
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

#ifndef _URANITE_IR_MIR_ANALYSIS_HPP_
#define _URANITE_IR_MIR_ANALYSIS_HPP_

#include "uranite/ir/mir.hpp"

namespace uranite::ir::mir {
	
	/** @brief Backward dataflow analysis computing live variable sets per basic block. */
	class MIRLivenessAnalyzer {
	public:
		
		/** @brief Runs liveness analysis on all blocks, populating live-in/live-out sets. */
		void analyze( MIRFunctionDefinition& functionDefinition );
	
	private:
		
		void computeLocalSets( MIRBasicBlock& basicBlock );
		void buildPredecessorSuccessorEdges( MIRFunctionDefinition& functionDefinition );
		bool propagateBackward( MIRFunctionDefinition& functionDefinition );
	
	};

} // namespace uranite::ir::mir

#endif // end _URANITE_IR_MIR_ANALYSIS_HPP_
