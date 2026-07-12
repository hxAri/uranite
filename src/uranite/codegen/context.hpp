
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

#ifndef _URANITE_CODEGEN_CONTEXT_HPP_
#define _URANITE_CODEGEN_CONTEXT_HPP_

#include <string>
#include <unordered_map>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instruction.h>

namespace uranite::codegen {
	
	/**
	 * @brief Contextual information for generator support during LLVM IR generation.
	 * 
	 * This structure manages state persistence and control flow blocks required
	 * for implementing generator functions.
	 */
	struct GeneratorContext {
		
		/** @brief LLVM variable tracking the completion status of the generator. */
		llvm::AllocaInst* doneVar = nullptr;
		
		/** @brief The basic block used as the exit point for the generator logic. */
		llvm::BasicBlock* exitBB = nullptr;
		
		/** @brief The next state identifier to be assigned for state transitions. */
		int nextStateId = 1;
		
		/** @brief Mapping of local variable names to their corresponding global storage for persistence. */
		std::unordered_map<std::string, llvm::GlobalVariable*> persistedLocals;
		
		/** @brief A prefix string used for naming symbols within the generator scope. */
		std::string prefix;
		
		/** @brief LLVM variable storing the current state index of the generator. */
		llvm::AllocaInst* stateVar = nullptr;
		
		/** @brief LLVM variable holding the current value being yielded. */
		llvm::AllocaInst* valueVar = nullptr;
		
	};
	
} // namespace uranite::codegen

#endif // end _URANITE_CODEGEN_CONTEXT_HPP_
