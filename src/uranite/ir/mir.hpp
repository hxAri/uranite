
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

#ifndef _URANITE_IR_MIR_HPP_
#define _URANITE_IR_MIR_HPP_

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "uranite/lookup/source.hpp"
#include "uranite/semantic/typeref.hpp"

namespace uranite::ir::mir {
	
	using MIRBlockIdentifier = uint32_t;
	using MIRVariableIdentifier = uint32_t;
	using MIRInstructionIdentifier = uint32_t;
	
	static constexpr MIRBlockIdentifier INVALID_BLOCK_IDENTIFIER = std::numeric_limits<MIRBlockIdentifier>::max();
	static constexpr MIRVariableIdentifier INVALID_VARIABLE_IDENTIFIER = std::numeric_limits<MIRVariableIdentifier>::max();
	
	/** @brief Categorizes each MIR instruction into exactly one operational role. */
	enum class MIRInstructionKind {
		
		// Variable lifecycle
		AllocateLocal,
		LoadVariable,
		StoreVariable,
		CopyValue,
		MoveValue,
		DropValue,
		
		// Arithmetic (integer)
		AddInteger,
		SubtractInteger,
		MultiplyInteger,
		DivideInteger,
		ModuloInteger,
		NegateInteger,
		PowerInteger,
		
		// Arithmetic (float)
		AddFloat,
		SubtractFloat,
		MultiplyFloat,
		DivideFloat,
		NegateFloat,
		PowerFloat,
		
		// Bitwise
		BitwiseAnd,
		BitwiseOr,
		BitwiseXor,
		BitwiseNot,
		ShiftLeft,
		ShiftRight,
		
		// Comparison
		CompareEqual,
		CompareNotEqual,
		CompareLessThan,
		CompareGreaterThan,
		CompareLessEqual,
		CompareGreaterEqual,
		
		// Logical
		LogicalAnd,
		LogicalOr,
		LogicalNot,
		
		// Memory
		TakeReference,
		DereferencePointer,
		ComputeFieldAddress,
		ComputeIndexAddress,
		HeapAllocate,
		HeapFree,
		AddressOf,
		
		// Control flow (terminators)
		CallFunction,
		CallVirtual,
		ReturnValue,
		BranchConditional,
		JumpUnconditional,
		SwitchBranch,
		InvokeFunction,
		Unreachable,
		
		// Object operations
		ConstructObject,
		DestructObject,
		LoadVirtualTable,
		
		// Constants
		ConstantInteger,
		ConstantFloat,
		ConstantBoolean,
		ConstantString,
		ConstantChar,
		ConstantNone,
		
		// Type operations
		CastType,
		InstanceOfCheck,
		
		// SSA
		PhiNode,
		
		// Exception handling
		ThrowException,
		LandingPad,
		
		// Defer
		DeferPush,
		DeferEmit,
		
		// Inline assembly
		InlineAssembly,
		
		// Generator
		Yield,
		
		// No-op
		NoOperation
		
	};
	
	/** @brief Single MIR instruction within a basic block. */
	struct MIRInstruction {
		
		MIRInstructionIdentifier instructionIdentifier = 0;
		MIRInstructionKind instructionKind;
		MIRVariableIdentifier destinationVariable = INVALID_VARIABLE_IDENTIFIER;
		std::vector<MIRVariableIdentifier> sourceOperands;
		semantic::TypeSharedPointer operandType;
		lookup::SourceSharedPointer sourceLocation;
		
		// Constant payloads
		int64_t integerConstantValue = 0;
		double floatConstantValue = 0.0;
		bool booleanConstantValue = false;
		std::string stringConstantValue;
		char charConstantValue = '\0';
		
		// Call payloads
		std::string calledFunctionQualifiedName;
		int virtualTableEntryIndex = -1;
		std::vector<std::string> keywordArgumentKeys;
		std::vector<MIRVariableIdentifier> keywordArgumentValues;
		
		// Branch payloads
		MIRBlockIdentifier trueBranchTarget = INVALID_BLOCK_IDENTIFIER;
		MIRBlockIdentifier falseBranchTarget = INVALID_BLOCK_IDENTIFIER;
		std::vector<std::pair<int64_t, MIRBlockIdentifier>> switchBranchTargets;
		MIRBlockIdentifier defaultSwitchTarget = INVALID_BLOCK_IDENTIFIER;
		
		// Field access
		int fieldLayoutIndex = 0;
		std::string fieldAccessName;
		
		// Landing pad
		MIRBlockIdentifier landingPadTarget = INVALID_BLOCK_IDENTIFIER;
		
		// Phi node
		std::vector<std::pair<MIRBlockIdentifier, MIRVariableIdentifier>> phiIncomingValues;
		
		// Inline assembly
		std::string assemblyTemplate;
		std::vector<std::string> assemblyClobbers;
		bool assemblyIsVolatile = false;
		std::vector<MIRVariableIdentifier> assemblyOutputVariables;
		std::vector<MIRVariableIdentifier> assemblyInputVariables;
		std::vector<std::string> assemblyOutputConstraints;
		std::vector<std::string> assemblyInputConstraints;
		
		// Type cast
		semantic::TypeSharedPointer castTargetType;
		
		// Instance-of check
		semantic::TypeSharedPointer instanceCheckType;
		
		MIRInstruction( MIRInstructionKind instructionKind )
			: instructionKind( instructionKind ) {
		}
	
	};
	
	/** @brief Linear sequence of instructions terminated by a single control flow op. */
	struct MIRBasicBlock {
		
		MIRBlockIdentifier blockIdentifier;
		std::string blockLabel;
		std::vector<MIRInstruction> blockInstructions;
		std::vector<MIRBlockIdentifier> predecessorBlocks;
		std::vector<MIRBlockIdentifier> successorBlocks;
		bool isTerminated = false;
		
		// Dataflow annotations (populated by analysis passes)
		std::unordered_set<MIRVariableIdentifier> liveVariablesAtEntry;
		std::unordered_set<MIRVariableIdentifier> liveVariablesAtExit;
		std::unordered_set<MIRVariableIdentifier> definedVariables;
		std::unordered_set<MIRVariableIdentifier> usedVariables;
		
		MIRBasicBlock( MIRBlockIdentifier blockIdentifier )
			: blockIdentifier( blockIdentifier ) {
		}
	
	};
	
	/** @brief Tracks type, mutability, and ownership state for a single MIR variable. */
	struct MIRVariableDescriptor {
		
		MIRVariableIdentifier variableIdentifier = INVALID_VARIABLE_IDENTIFIER;
		std::string variableName;
		semantic::TypeSharedPointer variableType;
		bool isMutableVariable = false;
		bool isHeapAllocatedVariable = false;
		bool isParameterVariable = false;
		
		int ownershipScopeDepth = 0;
	
	};
	
	struct MIRModuleConstant {
		enum ConstantKind { Integer, Float, Boolean, String, Null };
		ConstantKind kind = Integer;
		int64_t integerValue = 0;
		double floatValue = 0.0;
		bool booleanValue = false;
		std::string stringValue;
	};
	
	/** @brief MIR representation of a single function/method with its CFG. */
	struct MIRFunctionDefinition {
		
		std::string functionName;
		std::string mangledFunctionName;
		std::string ownerClassQualifiedName;
		semantic::TypeSharedPointer returnTypeDescriptor;
		lookup::SourceSharedPointer sourceLocation;
		std::vector<MIRVariableIdentifier> parameterVariableIdentifiers;
		std::vector<std::shared_ptr<MIRBasicBlock>> controlFlowBlocks;
		std::unordered_map<MIRVariableIdentifier, MIRVariableDescriptor> variableDescriptorTable;
		MIRVariableIdentifier nextAvailableVariableIdentifier = 0;
		MIRBlockIdentifier entryBlockIdentifier = 0;
		int variadicParameterIndex = -1;
		semantic::TypeSharedPointer variadicElementType;
		int keywordParameterIndex = -1;
		semantic::TypeSharedPointer keywordValueType;
		std::unordered_map<int, MIRModuleConstant> parameterDefaultValues;
		bool isStaticMethod = false;
		bool isGeneratorFunction = false;
		semantic::TypeSharedPointer generatorYieldType;
		bool isAsyncFunction = false;
		semantic::TypeSharedPointer asyncInnerReturnType;
		
		/** @brief Allocates a new variable and registers it in the descriptor table. */
		MIRVariableIdentifier allocateVariable(
			const std::string& debugName,
			semantic::TypeSharedPointer variableType,
			bool isMutable
		) {
			MIRVariableIdentifier variableIdentifier = this->nextAvailableVariableIdentifier++;
			MIRVariableDescriptor descriptor;
			descriptor.variableIdentifier = variableIdentifier;
			descriptor.variableName = debugName;
			descriptor.variableType = std::move( variableType );
			descriptor.isMutableVariable = isMutable;
			this->variableDescriptorTable[variableIdentifier] = std::move( descriptor );
			return variableIdentifier;
		}
		
		/** @brief Creates a new basic block and appends it to the control flow graph. */
		std::shared_ptr<MIRBasicBlock> createBasicBlock( const std::string& labelPrefix ) {
			MIRBlockIdentifier blockIdentifier = static_cast<MIRBlockIdentifier>( this->controlFlowBlocks.size() );
			std::shared_ptr<MIRBasicBlock> basicBlock = std::make_shared<MIRBasicBlock>( blockIdentifier );
			basicBlock->blockLabel = labelPrefix + "." + std::to_string( blockIdentifier );
			this->controlFlowBlocks.push_back( basicBlock );
			return basicBlock;
		}
	
	};
	
	/** @brief Describes the memory layout of a user-defined type. */
	struct TypeLayoutDescriptor {
		std::string typeQualifiedName;
		int typeSizeInBytes = 0;
		int typeAlignmentInBytes = 0;
		std::vector<int> fieldByteOffsets;
		std::vector<std::string> fieldNames;
		std::vector<semantic::TypeSharedPointer> fieldTypes;
		bool hasVirtualTable = false;
		int virtualTableEntryCount = 0;
	};
	
	struct MIRGlobalVariable {
		std::string variableName;
		semantic::TypeSharedPointer variableType;
		MIRModuleConstant initialValue;
		bool hasInitializer = false;
	};
	
	struct MIRExternFunction {
		std::string functionName;
		std::string linkageName;
		semantic::TypeSharedPointer returnType;
		std::vector<semantic::TypeSharedPointer> parameterTypes;
		bool isVariadic = false;
	};
	
	/** @brief Top-level MIR container holding all functions and type layouts for a module. */
	struct MIRModuleDefinition {
		
		std::string moduleName;
		std::vector<std::shared_ptr<MIRFunctionDefinition>> functionDefinitions;
		std::unordered_map<std::string, TypeLayoutDescriptor> typeLayoutTable;
		std::unordered_map<std::string, MIRModuleConstant> moduleConstants;
		std::unordered_map<std::string, MIRGlobalVariable> globalVariables;
		std::vector<MIRExternFunction> externFunctions;
	
	};

} // namespace uranite::ir::mir

#endif // end _URANITE_IR_MIR_HPP_
