
//
// @author hxAri (hxari)
// @create 2026-06-15 12:00
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
// Uranite Licence under GNU General Public Licence v3
//

#ifndef _URANITE_CODEGEN_RUNTIME_INTERFACE_HPP_
#define _URANITE_CODEGEN_RUNTIME_INTERFACE_HPP_

#include <string>

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Type.h>

namespace uranite::codegen {

	struct RuntimeFunctionSpec {
		std::string functionName;
		llvm::FunctionType* functionSignature = nullptr;
		llvm::Function::LinkageTypes linkageType = llvm::Function::ExternalLinkage;
		bool isNoReturn = false;
		bool isVariadic = false;
	};

	class RuntimeInterface {

		public:

			virtual ~RuntimeInterface() = default;

			virtual RuntimeFunctionSpec getThrowFunction( llvm::LLVMContext& context ) = 0;
			virtual RuntimeFunctionSpec getPersonalityFunction( llvm::LLVMContext& context ) = 0;
			virtual RuntimeFunctionSpec getBeginCatchFunction( llvm::LLVMContext& context ) = 0;
			virtual RuntimeFunctionSpec getEndCatchFunction( llvm::LLVMContext& context ) = 0;

			virtual RuntimeFunctionSpec getPushFrameFunction( llvm::LLVMContext& context ) = 0;
			virtual RuntimeFunctionSpec getPopFrameFunction( llvm::LLVMContext& context ) = 0;

			virtual RuntimeFunctionSpec getMallocFunction( llvm::LLVMContext& context ) = 0;
			virtual RuntimeFunctionSpec getFreeFunction( llvm::LLVMContext& context ) = 0;
			virtual RuntimeFunctionSpec getCallocFunction( llvm::LLVMContext& context ) = 0;
			virtual RuntimeFunctionSpec getMemcpyFunction( llvm::LLVMContext& context ) = 0;
			virtual RuntimeFunctionSpec getReallocFunction( llvm::LLVMContext& context ) = 0;

			virtual RuntimeFunctionSpec getPrintfFunction( llvm::LLVMContext& context ) = 0;
			virtual RuntimeFunctionSpec getSnprintfFunction( llvm::LLVMContext& context ) = 0;
			virtual RuntimeFunctionSpec getStrlenFunction( llvm::LLVMContext& context ) = 0;
			virtual RuntimeFunctionSpec getStrcpyFunction( llvm::LLVMContext& context ) = 0;
			virtual RuntimeFunctionSpec getStrcatFunction( llvm::LLVMContext& context ) = 0;
			virtual RuntimeFunctionSpec getStrcmpFunction( llvm::LLVMContext& context ) = 0;
			virtual RuntimeFunctionSpec getStrstrFunction( llvm::LLVMContext& context ) = 0;
			virtual RuntimeFunctionSpec getStrncmpFunction( llvm::LLVMContext& context ) = 0;
			virtual RuntimeFunctionSpec getWriteFunction( llvm::LLVMContext& context ) = 0;
	};

} // namespace uranite::codegen

#endif // _URANITE_CODEGEN_RUNTIME_INTERFACE_HPP_
