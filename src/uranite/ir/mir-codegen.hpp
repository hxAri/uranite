
//
// @author hxAri (hxari)
// @create 13-06-2026
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
// Uranite Licence under GNU General Public Licence v3
//

#ifndef _URANITE_IR_MIR_CODEGEN_HPP_
#define _URANITE_IR_MIR_CODEGEN_HPP_

#include <memory>
#include <string>
#include <unordered_map>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>

#include "uranite/diagnostic/diagnostic.hpp"
#include "uranite/ir/mir.hpp"
#include "uranite/semantic/analyzer.hpp"

namespace uranite::ir::mir {

	/** @brief Generates LLVM IR from the MIR control-flow graph representation. */
	class MIRCodegen {
	public:

		MIRCodegen( semantic::Analyzer& semanticAnalyzer, diagnostic::Engine& diagnosticEngine );

		/** @brief Generates LLVM IR for all functions in the MIR module. */
		bool generate( MIRModuleDefinition& mirModule );

		/** @brief Returns the generated LLVM module. */
		llvm::Module* getModule();

		/** @brief Writes generated IR to a text file. */
		bool writeIR( const std::string& filename );

		/** @brief Writes generated IR to an object file. */
		bool writeObject( const std::string& filename );

	private:

		void generateFunction( MIRFunctionDefinition& functionDefinition );
		void generateBasicBlock( MIRBasicBlock& basicBlock, MIRFunctionDefinition& functionDefinition );
		void generateInstruction( const MIRInstruction& instruction, MIRFunctionDefinition& functionDefinition );

		// Instruction category generators
		void generateAllocateLocal( const MIRInstruction& instruction, MIRFunctionDefinition& functionDefinition );
		void generateLoadVariable( const MIRInstruction& instruction );
		void generateStoreVariable( const MIRInstruction& instruction );
		void generateCopyValue( const MIRInstruction& instruction );
		void generateMoveValue( const MIRInstruction& instruction );
		void generateConstantInteger( const MIRInstruction& instruction );
		void generateConstantFloat( const MIRInstruction& instruction );
		void generateConstantBoolean( const MIRInstruction& instruction );
		void generateConstantString( const MIRInstruction& instruction );
		void generateConstantChar( const MIRInstruction& instruction );
		void generateConstantNone( const MIRInstruction& instruction );
		void generateArithmetic( const MIRInstruction& instruction );
		void generateComparison( const MIRInstruction& instruction );
		void generateLogical( const MIRInstruction& instruction );
		void generateBitwise( const MIRInstruction& instruction );
		void generateCastType( const MIRInstruction& instruction );
		void generateCallFunction( const MIRInstruction& instruction, MIRFunctionDefinition& functionDefinition );
		void generateReturnValue( const MIRInstruction& instruction );
		void generateBranchConditional( const MIRInstruction& instruction );
		void generateJumpUnconditional( const MIRInstruction& instruction );
		void generateSwitchBranch( const MIRInstruction& instruction );
		void generateComputeFieldAddress( const MIRInstruction& instruction );
		void generateComputeIndexAddress( const MIRInstruction& instruction );
		void generateHeapAllocate( const MIRInstruction& instruction, MIRFunctionDefinition& functionDefinition );
		void generateHeapFree( const MIRInstruction& instruction );
		void generatePhiNode( const MIRInstruction& instruction );
		void generateConstructObject( const MIRInstruction& instruction, MIRFunctionDefinition& functionDefinition );

		// Helpers
		llvm::Type* toLLVMType( const semantic::TypeSharedPointer& semanticType );
		llvm::Value* getVariableValue( MIRVariableIdentifier variableIdentifier );
		llvm::Value* loadVariableValue( MIRVariableIdentifier variableIdentifier );
		void setVariableValue( MIRVariableIdentifier variableIdentifier, llvm::Value* value );
		llvm::AllocaInst* createEntryBlockAllocation( llvm::Function* function, const std::string& name, llvm::Type* type );
		llvm::Function* getOrCreateMalloc();
		llvm::Function* getOrCreateFree();

		semantic::Analyzer& semanticAnalyzer;
		diagnostic::Engine& diagnosticEngine;

		llvm::LLVMContext llvmContext;
		std::unique_ptr<llvm::Module> llvmModule;
		llvm::IRBuilder<> irBuilder;

		// Per-function mappings
		std::unordered_map<MIRVariableIdentifier, llvm::Value*> variableValueMap;
		std::unordered_map<MIRBlockIdentifier, llvm::BasicBlock*> blockMap;

		// Struct type cache
		std::unordered_map<std::string, llvm::StructType*> structTypeCache;

		// Function resolution: MIR name → LLVM function
		std::unordered_map<std::string, llvm::Function*> functionResolutionMap;

		// Current module being generated (for type layout lookups)
		MIRModuleDefinition* currentMIRModule = nullptr;

	};

} // namespace uranite::ir::mir

#endif // end _URANITE_IR_MIR_CODEGEN_HPP_
