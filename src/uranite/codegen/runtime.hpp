
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

#ifndef _URANITE_CODEGEN_RUNTIME_HPP_
#define _URANITE_CODEGEN_RUNTIME_HPP_

#include <string>

#include <llvm/IR/Function.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

namespace uranite::codegen::runtime {
	
	namespace async {
		
		llvm::Function* getOrCreateSchedulerInit( llvm::LLVMContext &context, llvm::Module* module ) {
			llvm::Function* function = module->getFunction( "runtimeInit" );
			if( function != nullptr ) {
				return function;
			}
			std::string functionName( "uraniteSchedulerInit" );
			function = module->getFunction( functionName );
			if( function == nullptr ) {
				return llvm::Function::Create(
					llvm::FunctionType::get(
						llvm::Type::getVoidTy( context ),
						false
					),
					llvm::Function::ExternalLinkage,
					functionName,
					module
				);
			}
			return function;
		}
		
		llvm::Function* getOrCreateRunScheduler( llvm::LLVMContext &context, llvm::Module* module ) {
			llvm::Function* function = module->getFunction( "runtimeRun" );
			if( function != nullptr ) {
				return function;
			}
			std::string functionName( "uraniteRunScheduler" );
			function = module->getFunction( functionName );
			if( function == nullptr ) {
				return llvm::Function::Create(
					llvm::FunctionType::get(
						llvm::Type::getVoidTy( context ),
						false
					),
					llvm::Function::ExternalLinkage,
					functionName,
					module
				);
			}
			return function;
		}
	
	} // namespace uranite::codegen::runtime::async
	
	namespace interface {
		
	} // namespace uranite::codegen::runtime::interface

} // namespace uranite::codegen

#endif // end _URANITE_CODEGEN_RUNTIME_HPP_
