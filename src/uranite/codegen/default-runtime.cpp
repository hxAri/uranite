
//
// @author hxAri (hxari)
// @create 2026-06-15 12:00
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
// Uranite Licence under GNU General Public Licence v3
//

#include "uranite/codegen/default-runtime.hpp"

namespace uranite::codegen {

	RuntimeFunctionSpec DefaultRuntime::getThrowFunction( llvm::LLVMContext& context ) {
		RuntimeFunctionSpec spec;
		spec.functionName = "__uranite_throw";
		spec.functionSignature = llvm::FunctionType::get(
			llvm::Type::getVoidTy( context ),
			{ llvm::PointerType::getUnqual( context ), llvm::PointerType::getUnqual( context ) },
			false
		);
		spec.isNoReturn = true;
		return spec;
	}

	RuntimeFunctionSpec DefaultRuntime::getPersonalityFunction( llvm::LLVMContext& context ) {
		RuntimeFunctionSpec spec;
		spec.functionName = "__uranite_personality_v0";
		spec.functionSignature = llvm::FunctionType::get(
			llvm::Type::getInt32Ty( context ),
			true
		);
		spec.isVariadic = true;
		return spec;
	}

	RuntimeFunctionSpec DefaultRuntime::getBeginCatchFunction( llvm::LLVMContext& context ) {
		RuntimeFunctionSpec spec;
		spec.functionName = "__uranite_begin_catch";
		spec.functionSignature = llvm::FunctionType::get(
			llvm::PointerType::getUnqual( context ),
			{ llvm::PointerType::getUnqual( context ) },
			false
		);
		return spec;
	}

	RuntimeFunctionSpec DefaultRuntime::getEndCatchFunction( llvm::LLVMContext& context ) {
		RuntimeFunctionSpec spec;
		spec.functionName = "__uranite_end_catch";
		spec.functionSignature = llvm::FunctionType::get(
			llvm::Type::getVoidTy( context ),
			{ llvm::PointerType::getUnqual( context ) },
			false
		);
		return spec;
	}

	RuntimeFunctionSpec DefaultRuntime::getPushFrameFunction( llvm::LLVMContext& context ) {
		RuntimeFunctionSpec spec;
		spec.functionName = "__uranite_push_frame";
		spec.functionSignature = llvm::FunctionType::get(
			llvm::Type::getVoidTy( context ),
			{
				llvm::PointerType::getUnqual( context ),
				llvm::Type::getInt64Ty( context ),
				llvm::Type::getInt64Ty( context ),
				llvm::PointerType::getUnqual( context )
			},
			false
		);
		return spec;
	}

	RuntimeFunctionSpec DefaultRuntime::getPopFrameFunction( llvm::LLVMContext& context ) {
		RuntimeFunctionSpec spec;
		spec.functionName = "__uranite_pop_frame";
		spec.functionSignature = llvm::FunctionType::get(
			llvm::Type::getVoidTy( context ),
			false
		);
		return spec;
	}

	RuntimeFunctionSpec DefaultRuntime::getGetFrameDepthFunction( llvm::LLVMContext& context ) {
		RuntimeFunctionSpec spec;
		spec.functionName = "__uranite_get_frame_depth";
		spec.functionSignature = llvm::FunctionType::get(
			llvm::Type::getInt64Ty( context ),
			false
		);
		return spec;
	}

	RuntimeFunctionSpec DefaultRuntime::getGetFrameAtFunction( llvm::LLVMContext& context ) {
		RuntimeFunctionSpec spec;
		spec.functionName = "__uranite_get_frame_at";
		spec.functionSignature = llvm::FunctionType::get(
			llvm::PointerType::getUnqual( context ),
			{ llvm::Type::getInt64Ty( context ) },
			false
		);
		return spec;
	}

	RuntimeFunctionSpec DefaultRuntime::getMallocFunction( llvm::LLVMContext& context ) {
		RuntimeFunctionSpec spec;
		spec.functionName = "malloc";
		spec.functionSignature = llvm::FunctionType::get(
			llvm::PointerType::getUnqual( context ),
			{ llvm::Type::getInt64Ty( context ) },
			false
		);
		return spec;
	}

	RuntimeFunctionSpec DefaultRuntime::getFreeFunction( llvm::LLVMContext& context ) {
		RuntimeFunctionSpec spec;
		spec.functionName = "free";
		spec.functionSignature = llvm::FunctionType::get(
			llvm::Type::getVoidTy( context ),
			{ llvm::PointerType::getUnqual( context ) },
			false
		);
		return spec;
	}

	RuntimeFunctionSpec DefaultRuntime::getCallocFunction( llvm::LLVMContext& context ) {
		RuntimeFunctionSpec spec;
		spec.functionName = "calloc";
		spec.functionSignature = llvm::FunctionType::get(
			llvm::PointerType::getUnqual( context ),
			{
				llvm::Type::getInt64Ty( context ),
				llvm::Type::getInt64Ty( context )
			},
			false
		);
		return spec;
	}

	RuntimeFunctionSpec DefaultRuntime::getMemcpyFunction( llvm::LLVMContext& context ) {
		RuntimeFunctionSpec spec;
		spec.functionName = "llvm.memcpy.p0.p0.i64";
		spec.functionSignature = llvm::FunctionType::get(
			llvm::Type::getVoidTy( context ),
			{
				llvm::PointerType::getUnqual( context ),
				llvm::PointerType::getUnqual( context ),
				llvm::Type::getInt64Ty( context ),
				llvm::Type::getInt1Ty( context )
			},
			false
		);
		return spec;
	}

	RuntimeFunctionSpec DefaultRuntime::getReallocFunction( llvm::LLVMContext& context ) {
		RuntimeFunctionSpec spec;
		spec.functionName = "realloc";
		spec.functionSignature = llvm::FunctionType::get(
			llvm::PointerType::getUnqual( context ),
			{
				llvm::PointerType::getUnqual( context ),
				llvm::Type::getInt64Ty( context )
			},
			false
		);
		return spec;
	}

	RuntimeFunctionSpec DefaultRuntime::getPrintfFunction( llvm::LLVMContext& context ) {
		RuntimeFunctionSpec spec;
		spec.functionName = "printf";
		spec.functionSignature = llvm::FunctionType::get(
			llvm::Type::getInt32Ty( context ),
			{ llvm::PointerType::getUnqual( context ) },
			true
		);
		spec.isVariadic = true;
		return spec;
	}

	RuntimeFunctionSpec DefaultRuntime::getSnprintfFunction( llvm::LLVMContext& context ) {
		RuntimeFunctionSpec spec;
		spec.functionName = "snprintf";
		spec.functionSignature = llvm::FunctionType::get(
			llvm::Type::getInt32Ty( context ),
			{
				llvm::PointerType::getUnqual( context ),
				llvm::Type::getInt64Ty( context ),
				llvm::PointerType::getUnqual( context )
			},
			true
		);
		spec.isVariadic = true;
		return spec;
	}

	RuntimeFunctionSpec DefaultRuntime::getStrlenFunction( llvm::LLVMContext& context ) {
		RuntimeFunctionSpec spec;
		spec.functionName = "strlen";
		spec.functionSignature = llvm::FunctionType::get(
			llvm::Type::getInt64Ty( context ),
			{ llvm::PointerType::getUnqual( context ) },
			false
		);
		return spec;
	}

	RuntimeFunctionSpec DefaultRuntime::getStrcpyFunction( llvm::LLVMContext& context ) {
		RuntimeFunctionSpec spec;
		spec.functionName = "strcpy";
		spec.functionSignature = llvm::FunctionType::get(
			llvm::PointerType::getUnqual( context ),
			{
				llvm::PointerType::getUnqual( context ),
				llvm::PointerType::getUnqual( context )
			},
			false
		);
		return spec;
	}

	RuntimeFunctionSpec DefaultRuntime::getStrcatFunction( llvm::LLVMContext& context ) {
		RuntimeFunctionSpec spec;
		spec.functionName = "strcat";
		spec.functionSignature = llvm::FunctionType::get(
			llvm::PointerType::getUnqual( context ),
			{
				llvm::PointerType::getUnqual( context ),
				llvm::PointerType::getUnqual( context )
			},
			false
		);
		return spec;
	}

	RuntimeFunctionSpec DefaultRuntime::getStrcmpFunction( llvm::LLVMContext& context ) {
		RuntimeFunctionSpec spec;
		spec.functionName = "strcmp";
		spec.functionSignature = llvm::FunctionType::get(
			llvm::Type::getInt32Ty( context ),
			{
				llvm::PointerType::getUnqual( context ),
				llvm::PointerType::getUnqual( context )
			},
			false
		);
		return spec;
	}

	RuntimeFunctionSpec DefaultRuntime::getStrstrFunction( llvm::LLVMContext& context ) {
		RuntimeFunctionSpec spec;
		spec.functionName = "strstr";
		spec.functionSignature = llvm::FunctionType::get(
			llvm::PointerType::getUnqual( context ),
			{
				llvm::PointerType::getUnqual( context ),
				llvm::PointerType::getUnqual( context )
			},
			false
		);
		return spec;
	}

	RuntimeFunctionSpec DefaultRuntime::getStrncmpFunction( llvm::LLVMContext& context ) {
		RuntimeFunctionSpec spec;
		spec.functionName = "strncmp";
		spec.functionSignature = llvm::FunctionType::get(
			llvm::Type::getInt32Ty( context ),
			{
				llvm::PointerType::getUnqual( context ),
				llvm::PointerType::getUnqual( context ),
				llvm::Type::getInt64Ty( context )
			},
			false
		);
		return spec;
	}

	RuntimeFunctionSpec DefaultRuntime::getWriteFunction( llvm::LLVMContext& context ) {
		RuntimeFunctionSpec spec;
		spec.functionName = "write";
		spec.functionSignature = llvm::FunctionType::get(
			llvm::Type::getInt64Ty( context ),
			{
				llvm::Type::getInt32Ty( context ),
				llvm::PointerType::getUnqual( context ),
				llvm::Type::getInt64Ty( context )
			},
			false
		);
		return spec;
	}

} // namespace uranite::codegen
