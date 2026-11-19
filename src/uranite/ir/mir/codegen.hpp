
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
#include <unordered_set>

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
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>

#include "uranite/codegen/default-runtime.hpp"
#include "uranite/descriptor/descriptor.hpp"
#include "uranite/diagnostic/diagnostic.hpp"
#include "uranite/ir/mir.hpp"
#include "uranite/semantic/analyzer.hpp"

namespace uranite::ir::mir {
	
	/** @brief Generates LLVM IR from the MIR control-flow graph representation. */
	class MIRCodegen {
	public:
		
		MIRCodegen( semantic::Analyzer& semanticAnalyzer, diagnostic::Engine& diagnosticEngine );
		
		/** @brief Overrides the target triple for cross-compilation. */
		void setTargetTriple( const std::string& triple );
		
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
		void generateStoreVariable( const MIRInstruction& instruction, MIRFunctionDefinition& functionDefinition );
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
		llvm::Value* emitCallOrInvoke( const MIRInstruction& instruction, llvm::Function* callee, std::vector<llvm::Value*>& arguments );
		llvm::Value* reinterpretErasedPrimitiveResult( const MIRInstruction& instruction, llvm::Value* callResult );
		void generateReturnValue( const MIRInstruction& instruction );
		void generateBranchConditional( const MIRInstruction& instruction );
		void generateJumpUnconditional( const MIRInstruction& instruction );
		void generateSwitchBranch( const MIRInstruction& instruction );
		void generateComputeFieldAddress( const MIRInstruction& instruction );
		void generateComputeIndexAddress( const MIRInstruction& instruction );
		void generateHeapAllocate( const MIRInstruction& instruction, MIRFunctionDefinition& functionDefinition );
		void generateHeapFree( const MIRInstruction& instruction );
		void emitDropCallForVariable( MIRVariableIdentifier variableId, llvm::Value* pointer );
		void emitDroperScopeCleanup( MIRVariableIdentifier excludeVariable );
		void registerDroperCleanupEntry( MIRVariableIdentifier variableId, const std::string& typeName );
 		bool isOptionalBoxedInnerType( const semantic::TypeSharedPointer& innerType );
 		bool optionalInnerIsScalarKind( const semantic::TypeSharedPointer& innerType );
 		llvm::Type* optionalScalarLLVMType( const semantic::TypeSharedPointer& innerType );
 		llvm::Value* createOptionalBox( llvm::Value* rawValue );
		llvm::Value* unwrapOptionalOperand( MIRVariableIdentifier operandId, MIRFunctionDefinition& functionDefinition );
		void emitOwnedBoxRelease( MIRVariableIdentifier variableId, MIRFunctionDefinition& functionDefinition );
		void generatePhiNode( const MIRInstruction& instruction );
		void generateConstructObject( const MIRInstruction& instruction, MIRFunctionDefinition& functionDefinition );
		void generateYield( const MIRInstruction& instruction );
		void generateGeneratorFunction( MIRFunctionDefinition& functionDefinition, const std::string& llvmFunctionName, llvm::Function* llvmFunction );
		void generateAsyncFunction( MIRFunctionDefinition& functionDefinition, const std::string& llvmFunctionName, llvm::Function* llvmFunction );
		
		// Helpers
		llvm::Type* toLLVMType( const semantic::TypeSharedPointer& semanticType );
		llvm::Type* resolveReturnType( const semantic::TypeSharedPointer& returnTypeDescriptor );
		bool functionReturnsConstructedObject( MIRFunctionDefinition& functionDefinition );
		llvm::Value* getVariableValue( MIRVariableIdentifier variableIdentifier );
		llvm::Value* loadVariableValue( MIRVariableIdentifier variableIdentifier );
		void setVariableValue( MIRVariableIdentifier variableIdentifier, llvm::Value* value );
		llvm::AllocaInst* createEntryBlockAllocation( llvm::Function* function, const std::string& name, llvm::Type* type );
		llvm::Function* getOrCreateMalloc();
		llvm::Function* getOrCreateCalloc();
		llvm::Function* getOrCreateFree();
		int getVtableFieldIndex( const std::string& typeName );
		
		/**
		 * @brief Generates a runtime throw of a newly constructed arithmetic error (e.g. ZeroDivisionError).
		 * @param errorClassName The qualified name of the error class to construct and throw.
		 * @param message The human-readable error message.
		 * @param source The source location for traceback injection.
		 * @param landingPadTarget The enclosing landing pad block identifier, or INVALID if none.
		 */
		void generateMIRThrowError( const std::string& errorClassName, const std::string& message, const lookup::SourceSharedPointer& source, MIRBlockIdentifier landingPadTarget = INVALID_BLOCK_IDENTIFIER );
		
		/**
		 * @brief Resolves or creates the per-class typeid global used for runtime instanceof checks.
		 * @param typeName Any known spelling of the class name (short, qualified, or generic); the
		 *        name is normalized (generic arguments stripped, package-relative fallback) so that
		 *        construction sites and instanceof checks converge on the same global. Classes with
		 *        an existing master vtable or itable marker reuse that global as their identity.
		 * @return The typeid global variable, or nullptr when the name is empty.
		 */
		llvm::GlobalVariable* getOrCreateClassTypeId( const std::string& typeName );
		
		/**
		 * @brief Lazily creates the zero-filled sentinel global used to null-guard instanceof loads.
		 * @return An 8-byte zero-initialized global whose address can be safely dereferenced as a
		 *         substitute object pointer when the real receiver is null (its first slot reads as
		 *         null, matching no typeid).
		 */
		llvm::GlobalVariable* getOrCreateInstanceofNullSentinel();
		
		/**
		 * @brief Collects the typeid globals of every concrete class that satisfies an instanceof target.
		 * @param targetType The semantic type being tested against (class or interface).
		 * @return Typeid globals for the target class itself plus every transitive subclass (for
		 *         class targets), or every class implementing the interface (for interface targets).
		 */
		std::vector<llvm::GlobalVariable*> collectInstanceofCandidates( const semantic::TypeSharedPointer& targetType );

		/**
		 * @brief Resolves or creates the runtime helper that tests whether a caught exception's
		 *        type name matches any of the supplied candidate names (subtypes included).
		 * @return The external function declaration, or nullptr on failure.
		 */
		llvm::Function* getOrCreateExceptionMatchesAny();

		/**
		 * @brief Resolves or creates the runtime helper that re-raises a caught exception wrapper
		 *        after no handler matches, preserving its original type name.
		 * @return The external function declaration, or nullptr on failure.
		 */
		llvm::Function* getOrCreateUraniteRethrow();

		llvm::Function* getOrCreateMemcpy();
		llvm::Function* getOrCreateStrlen();
		llvm::Function* getOrCreateStrcmp();
		llvm::Function* getOrCreateStringHash();
		llvm::Function* getOrCreateIntToBinStr();
		llvm::Function* getOrCreateStrCenter();
		llvm::Function* getOrCreateStrcpy();
		llvm::Function* getOrCreateStrcat();
		llvm::Function* getOrCreateStrstr();
		llvm::Function* getOrCreateStrncmp();
		llvm::Function* getOrCreateSnprintf();
		llvm::Function* getOrCreateWrite();
		llvm::Function* getOrCreateUraniteThrow();
		llvm::Function* getOrCreatePersonality();
		llvm::Function* getOrCreateBeginCatch();
		llvm::Function* getOrCreateExtern( const std::string& name, llvm::Type* returnType, std::vector<llvm::Type*> paramTypes );
		llvm::Type* resolveMemoryElementType( const semantic::TypeSharedPointer& operandType );
		bool tryBuiltinDescriptor( const std::string& typeName, const std::string& methodName, const MIRInstruction& instruction, MIRFunctionDefinition& functionDefinition );
		llvm::Function* getOrCreatePushFrame();
		llvm::Function* getOrCreatePopFrame();
		llvm::Function* getOrCreateGetFrameDepth();
		llvm::Function* getOrCreateGetFrameAt();
		llvm::Function* getOrCreateBuildTraceback();
		llvm::Function* getOrCreateRestoreFrames();
		llvm::Function* getOrCreateGetPushSerial();
		llvm::AllocaInst* getOrCreateTrySerialSlot( llvm::Function* enclosingFunction, llvm::BasicBlock* unwindDestination );
		bool registerCaughtThrowableBinding( MIRVariableIdentifier variableId, llvm::Value* caughtPointer );
		std::unordered_map<MIRVariableIdentifier, semantic::TypeSharedPointer> iteratorElementTypeByVar;
		semantic::TypeSharedPointer extractIteratorElementType( const std::string& concreteClassName );
		bool tryEmitSyntheticArrayListIteration( const MIRInstruction& instruction, MIRFunctionDefinition& functionDefinition, const std::string& methodName );
		int64_t resolveDroperItableSlot( const semantic::TypeSharedPointer& interfaceType );
		llvm::GlobalVariable* getOrCreateDroperGlobal( const std::string& globalName, llvm::Type* type, llvm::Constant* initialValue );
		void emitPushFrame( const std::string& file, int64_t line, int64_t column, const std::string& functionName );
		void emitPopFrame();
		
		semantic::Analyzer& semanticAnalyzer;
		diagnostic::Engine& diagnosticEngine;
		descriptor::Builtin builtinRegistry;
		std::shared_ptr<codegen::RuntimeInterface> runtimeInterface_;
		
		llvm::LLVMContext llvmContext;
		std::unique_ptr<llvm::Module> llvmModule;
		llvm::IRBuilder<> irBuilder;
		
		// Per-function mappings
		std::unordered_map<MIRVariableIdentifier, llvm::Value*> variableValueMap;
		std::unordered_map<MIRBlockIdentifier, llvm::BasicBlock*> blockMap;
		std::unordered_map<llvm::BasicBlock*, llvm::AllocaInst*> trySerialSlotMap;
		
		// Struct type cache
		std::unordered_map<std::string, llvm::StructType*> structTypeCache;
		
		// Function resolution: MIR name → LLVM function
		std::unordered_map<std::string, llvm::Function*> functionResolutionMap;
		
		// Cross-compilation target triple override (empty = host default)
		std::string targetTriple_;
		
		// Current module being generated (for type layout lookups)
		MIRModuleDefinition* currentMIRModule = nullptr;
		
		// Current function being generated (for variable descriptor lookups)
		MIRFunctionDefinition* currentMIRFunction = nullptr;
		
		// MIR function lookup for variadic parameter detection at call sites
		std::unordered_map<std::string, MIRFunctionDefinition*> mirFunctionDefinitionMap;
		
		// Names registered by extern declarations (take priority over Uranite functions)
		std::unordered_set<std::string> externDeclaredNames;
		
		// Tracks concrete class name for variables assigned via ConstructObject
		std::unordered_map<MIRVariableIdentifier, std::string> concreteClassMap;
		
		// Precomputed: functions that always return a construct of a specific class
		std::unordered_map<std::string, std::string> functionReturnConcreteClass;
		
		// Memory<T> element type per variable (compiler intrinsic tracking)
		std::unordered_map<MIRVariableIdentifier, llvm::Type*> memoryElementTypes;
		
		struct GeneratorContext {
			llvm::AllocaInst* stateVar = nullptr;
			llvm::AllocaInst* valueVar = nullptr;
			llvm::AllocaInst* doneVar = nullptr;
			llvm::BasicBlock* exitBlock = nullptr;
			llvm::StructType* structType = nullptr;
			llvm::Value* structPointer = nullptr;
			llvm::SwitchInst* dispatchSwitch = nullptr;
			int nextStateId = 1;
			std::string prefix;
			std::unordered_set<MIRVariableIdentifier> parameterVariables;
			std::unordered_map<std::string, llvm::GlobalVariable*> persistedLocals;
		};
		
		GeneratorContext* currentGeneratorContext = nullptr;
		bool isGeneratingAsyncWrapper = false;
		bool programHasAsyncFunctions = false;
		llvm::BasicBlock* asyncWrapperCatchBlock = nullptr;
		
		// Interface dispatch: class name → itable global variable
		std::unordered_map<std::string, llvm::GlobalVariable*> interfaceTableMap;
		
		// Runtime type identification: class name (base/short/qualified) → per-class typeid global
		std::unordered_map<std::string, llvm::GlobalVariable*> classTypeIdMap;
		
		// Null-object sentinel used to safely evaluate instanceof on potentially-null receivers
		llvm::GlobalVariable* instanceofNullSentinel = nullptr;
		
		// Interface dispatch: interface qualified name → ordered method names
		std::unordered_map<std::string, std::vector<std::string>> interfaceMethodOrder;
		
		// Master VTable: global interface index → slot offset in master vtable (1-based, 0 = class vtable)
		std::unordered_map<std::string, int> interfaceGlobalIndex;
		
		// Classes that have vtable pointers (implement interfaces)
		std::unordered_set<std::string> classesWithVtable;
		
		// Interfaces that have direct itable implementations (safe for vtable dispatch)
		std::unordered_set<std::string> interfacesWithDirectItable;
		
		// Abstract class dispatch: abstract class name → concrete subclass names
		std::unordered_map<std::string, std::vector<std::string>> abstractClassSubclasses;
		
		// Per-class vtable identifier constant (itable global or unique marker)
		std::unordered_map<std::string, llvm::Constant*> classVtableIdentifier;
		
		struct DroperCleanupEntry {
			MIRVariableIdentifier variableIdentifier;
			std::string qualifiedTypeName;
			llvm::AllocaInst* aliveFlag = nullptr;
			llvm::AllocaInst* pointerSlot = nullptr;
		};
		
		std::vector<DroperCleanupEntry> droperCleanupEntries;
		
		// Variables whose current value is a heap-boxed optional payload owned by this function
		std::unordered_set<MIRVariableIdentifier> optionalOwnedBoxes;
		
	};

} // namespace uranite::ir::mir

#endif // end _URANITE_IR_MIR_CODEGEN_HPP_
