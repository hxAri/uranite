
//
// @author hxAri (hxari)
// @create 2026-06-15 12:00
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
// Uranite Licence under GNU General Public Licence v3
//

#ifndef _URANITE_CODEGEN_DEFAULT_RUNTIME_HPP_
#define _URANITE_CODEGEN_DEFAULT_RUNTIME_HPP_

#include "uranite/codegen/runtime-interface.hpp"

namespace uranite::codegen {

	class DefaultRuntime : public RuntimeInterface {

		public:

			RuntimeFunctionSpec getThrowFunction( llvm::LLVMContext& context ) override;
			RuntimeFunctionSpec getPersonalityFunction( llvm::LLVMContext& context ) override;
			RuntimeFunctionSpec getBeginCatchFunction( llvm::LLVMContext& context ) override;
			RuntimeFunctionSpec getEndCatchFunction( llvm::LLVMContext& context ) override;

			RuntimeFunctionSpec getPushFrameFunction( llvm::LLVMContext& context ) override;
			RuntimeFunctionSpec getPopFrameFunction( llvm::LLVMContext& context ) override;
			RuntimeFunctionSpec getGetFrameDepthFunction( llvm::LLVMContext& context ) override;
			RuntimeFunctionSpec getGetFrameAtFunction( llvm::LLVMContext& context ) override;

			RuntimeFunctionSpec getMallocFunction( llvm::LLVMContext& context ) override;
			RuntimeFunctionSpec getFreeFunction( llvm::LLVMContext& context ) override;
			RuntimeFunctionSpec getCallocFunction( llvm::LLVMContext& context ) override;
			RuntimeFunctionSpec getMemcpyFunction( llvm::LLVMContext& context ) override;
			RuntimeFunctionSpec getReallocFunction( llvm::LLVMContext& context ) override;

			RuntimeFunctionSpec getPrintfFunction( llvm::LLVMContext& context ) override;
			RuntimeFunctionSpec getSnprintfFunction( llvm::LLVMContext& context ) override;
			RuntimeFunctionSpec getStrlenFunction( llvm::LLVMContext& context ) override;
			RuntimeFunctionSpec getStrcpyFunction( llvm::LLVMContext& context ) override;
			RuntimeFunctionSpec getStrcatFunction( llvm::LLVMContext& context ) override;
			RuntimeFunctionSpec getStrcmpFunction( llvm::LLVMContext& context ) override;
			RuntimeFunctionSpec getStrstrFunction( llvm::LLVMContext& context ) override;
			RuntimeFunctionSpec getStrncmpFunction( llvm::LLVMContext& context ) override;
			RuntimeFunctionSpec getWriteFunction( llvm::LLVMContext& context ) override;
	};

} // namespace uranite::codegen

#endif // _URANITE_CODEGEN_DEFAULT_RUNTIME_HPP_
