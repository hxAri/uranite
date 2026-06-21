
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

#ifndef _URANITE_IR_MIR_LOWERING_HPP_
#define _URANITE_IR_MIR_LOWERING_HPP_

#include <memory>
#include <stack>
#include <string>

#include "uranite/diagnostic/diagnostic.hpp"
#include "uranite/ir/hir.hpp"
#include "uranite/ir/mir.hpp"

namespace uranite::ir::mir {
	
	/** @brief Lowers HIR tree into MIR control-flow graph with basic blocks. */
	class MIRLowering {
	public:
		
		MIRLowering( diagnostic::Engine& diagnosticEngine );
		
		/** @brief Lowers an entire HIR module into an MIR module. */
		std::shared_ptr<MIRModuleDefinition> lower( hir::HIRModule& hirModule );
	
	private:
		
		// Declaration lowering
		void lowerFunctionDefinition( hir::HIRFunctionDefinition& hirFunction );
		void lowerClassDefinition( hir::HIRClassDefinition& hirClass );
		
		// Statement lowering — emits instructions into current block
		void lowerStatement( const hir::HIRNodeSharedPointer& hirStatement );
		void lowerBlock( hir::HIRBlock& hirBlock );
		void lowerIf( hir::HIRIf& hirIf );
		void lowerLoop( hir::HIRLoop& hirLoop );
		void lowerMatch( hir::HIRMatch& hirMatch );
		void lowerSwitch( hir::HIRSwitch& hirSwitch );
		void lowerTryCatch( hir::HIRTryCatch& hirTryCatch );
		void lowerDefer( hir::HIRDefer& hirDefer );
		
		// Expression lowering — returns variable holding result
		MIRVariableIdentifier lowerExpression( const hir::HIRNodeSharedPointer& hirExpression );
		MIRVariableIdentifier lowerBinaryOperation( hir::HIRBinaryOperation& hirBinaryOp );
		MIRVariableIdentifier lowerUnaryOperation( hir::HIRUnaryOperation& hirUnaryOp );
		MIRVariableIdentifier lowerFunctionCall( hir::HIRFunctionCall& hirCall );
		MIRVariableIdentifier lowerMethodCall( hir::HIRMethodCall& hirMethodCall );
		MIRVariableIdentifier lowerFieldAccess( hir::HIRFieldAccess& hirFieldAccess );
		MIRVariableIdentifier lowerIndexAccess( hir::HIRIndexAccess& hirIndexAccess );
		MIRVariableIdentifier lowerConstruct( hir::HIRConstruct& hirConstruct );
		
		// Instruction emission
		MIRVariableIdentifier emitInstruction( MIRInstruction instruction );
		void emitTerminator( MIRInstruction terminator );
		void switchToBlock( std::shared_ptr<MIRBasicBlock> targetBlock );
		void ensureBlockTerminated();
		
		// Loop context for break/continue
		struct LoopContext {
			MIRBlockIdentifier headerBlockIdentifier;
			MIRBlockIdentifier exitBlockIdentifier;
			MIRBlockIdentifier updateBlockIdentifier;
		};
		
		// State
		std::shared_ptr<MIRModuleDefinition> currentModule;
		std::shared_ptr<MIRFunctionDefinition> currentFunction;
		std::shared_ptr<MIRBasicBlock> currentBlock;
		std::stack<LoopContext> loopContextStack;
		MIRInstructionIdentifier nextInstructionIdentifier = 0;
		std::string currentClassName;
		std::string currentParentClassName;

		// Maps variable names to their MIR variable identifiers within current function
		std::unordered_map<std::string, MIRVariableIdentifier> variableNameMap;

		diagnostic::Engine& diagnosticEngine;
	
	};

} // namespace uranite::ir::mir

#endif // end _URANITE_IR_MIR_LOWERING_HPP_
