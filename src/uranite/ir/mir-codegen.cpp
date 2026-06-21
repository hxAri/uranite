
//
// @author hxAri (hxari)
// @create 13-06-2026
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
// Uranite Licence under GNU General Public Licence v3
//

#include "uranite/ir/mir-codegen.hpp"

#include <fmt/format.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetOptions.h>

#include "uranite/semantic/qualnames.hpp"

namespace uranite::ir::mir {

	MIRCodegen::MIRCodegen( semantic::Analyzer& semanticAnalyzer, diagnostic::Engine& diagnosticEngine )
		: semanticAnalyzer( semanticAnalyzer ),
		  diagnosticEngine( diagnosticEngine ),
		  irBuilder( this->llvmContext ) {
	}

	bool MIRCodegen::generate( MIRModuleDefinition& mirModule ) {
		this->llvmModule = std::make_unique<llvm::Module>( mirModule.moduleName, this->llvmContext );
		this->llvmModule->setTargetTriple( llvm::sys::getDefaultTargetTriple() );
		this->functionResolutionMap.clear();
		this->structTypeCache.clear();
		this->currentMIRModule = &mirModule;

		for( const std::pair<const std::string, TypeLayoutDescriptor>& typeEntry : mirModule.typeLayoutTable ) {
			const std::string& typeName = typeEntry.first;
			const TypeLayoutDescriptor& typeLayout = typeEntry.second;

			llvm::StructType* existingType = llvm::StructType::getTypeByName( this->llvmContext, typeName );
			if( existingType != nullptr ) {
				this->structTypeCache[typeName] = existingType;
				continue;
			}

			std::vector<llvm::Type*> fieldLLVMTypes;
			for( const semantic::TypeSharedPointer& fieldType : typeLayout.fieldTypes ) {
				llvm::Type* llvmFieldType = this->toLLVMType( fieldType );
				if( llvmFieldType->isVoidTy() ) {
					llvmFieldType = llvm::Type::getInt8PtrTy( this->llvmContext );
				}
				fieldLLVMTypes.push_back( llvmFieldType );
			}

			if( fieldLLVMTypes.empty() ) {
				fieldLLVMTypes.push_back( llvm::Type::getInt8Ty( this->llvmContext ) );
			}

			llvm::StructType* structType = llvm::StructType::create(
				this->llvmContext, fieldLLVMTypes, typeName
			);
			this->structTypeCache[typeName] = structType;
		}

		for( std::shared_ptr<MIRFunctionDefinition>& functionDefinition : mirModule.functionDefinitions ) {
			if( functionDefinition != nullptr ) {
				this->generateFunction( *functionDefinition );
			}
		}

		// Remove dead basic blocks (no predecessors and not entry)
		for( llvm::Function& function : *this->llvmModule ) {
			if( function.isDeclaration() ) {
				continue;
			}
			bool hasChanges = true;
			while( hasChanges ) {
				hasChanges = false;
				std::vector<llvm::BasicBlock*> deadBlocks;
				for( llvm::BasicBlock& block : function ) {
					if( &block == &function.getEntryBlock() ) {
						continue;
					}
					if( block.hasNPredecessors( 0 ) ) {
						deadBlocks.push_back( &block );
					}
				}
				for( llvm::BasicBlock* deadBlock : deadBlocks ) {
					deadBlock->dropAllReferences();
				}
				for( llvm::BasicBlock* deadBlock : deadBlocks ) {
					if( deadBlock->use_empty() ) {
						deadBlock->eraseFromParent();
						hasChanges = true;
					}
				}
			}
		}

		// Verify module — skip if module has too many functions (LLVM 14 verifier
		// can crash on large modules with many auto-suffixed duplicate definitions)
		size_t functionCount = 0;
		for( llvm::Function& function : *this->llvmModule ) {
			functionCount++;
		}
		if( functionCount <= 500 ) {
			std::string verifyErrors;
			llvm::raw_string_ostream verifyStream( verifyErrors );
			if( llvm::verifyModule( *this->llvmModule, &verifyStream ) ) {
				fmt::print( stderr, "MIR codegen: LLVM module verification failed: {}\n", verifyStream.str() );
				return false;
			}
		}
		return true;
	}

	llvm::Module* MIRCodegen::getModule() {
		return this->llvmModule.get();
	}

	bool MIRCodegen::writeIR( const std::string& filename ) {
		std::error_code errorCode;
		llvm::raw_fd_ostream outputStream( filename, errorCode, llvm::sys::fs::OF_None );
		if( errorCode ) {
			this->diagnosticEngine.error(
				nullptr,
				fmt::format( "MIR codegen: cannot open file '{}': {}", filename, errorCode.message() )
			);
			return false;
		}
		this->llvmModule->print( outputStream, nullptr );
		return true;
	}

	bool MIRCodegen::writeObject( const std::string& filename ) {
		llvm::InitializeAllTargetInfos();
		llvm::InitializeAllTargets();
		llvm::InitializeAllTargetMCs();
		llvm::InitializeAllAsmParsers();
		llvm::InitializeAllAsmPrinters();

		std::string targetError;
		const llvm::Target* target = llvm::TargetRegistry::lookupTarget(
			this->llvmModule->getTargetTriple(), targetError
		);
		if( target == nullptr ) {
			this->diagnosticEngine.error(
				nullptr,
				fmt::format( "MIR codegen: target lookup failed: {}", targetError )
			);
			return false;
		}

		llvm::TargetOptions targetOptions;
		llvm::TargetMachine* targetMachine = target->createTargetMachine(
			this->llvmModule->getTargetTriple(), "generic", "", targetOptions,
			llvm::Optional<llvm::Reloc::Model>()
		);
		this->llvmModule->setDataLayout( targetMachine->createDataLayout() );

		std::error_code errorCode;
		llvm::raw_fd_ostream outputStream( filename, errorCode, llvm::sys::fs::OF_None );
		if( errorCode ) {
			return false;
		}

		llvm::legacy::PassManager passManager;
		if( targetMachine->addPassesToEmitFile( passManager, outputStream, nullptr,
			llvm::CGFT_ObjectFile ) ) {
			return false;
		}
		passManager.run( *this->llvmModule );
		outputStream.flush();
		return true;
	}

	// ==========================================================================
	// Function generation
	// ==========================================================================

	void MIRCodegen::generateFunction( MIRFunctionDefinition& functionDefinition ) {
		// Build unique LLVM function name: prefix with owner class to reduce collisions
		std::string llvmFunctionName = functionDefinition.functionName;
		if( functionDefinition.ownerClassQualifiedName.empty() == false ) {
			llvmFunctionName = functionDefinition.ownerClassQualifiedName + "." + functionDefinition.functionName;
		}

		// Fast dedup: skip if qualified name already generated
		if( this->functionResolutionMap.count( llvmFunctionName ) > 0 ) {
			return;
		}

		this->variableValueMap.clear();
		this->blockMap.clear();

		// Resolve return type
		llvm::Type* returnType = llvm::Type::getVoidTy( this->llvmContext );
		if( functionDefinition.returnTypeDescriptor != nullptr ) {
			returnType = this->toLLVMType( functionDefinition.returnTypeDescriptor );
		}

		// Resolve parameter types
		std::vector<llvm::Type*> parameterTypes;
		for( MIRVariableIdentifier parameterVariable : functionDefinition.parameterVariableIdentifiers ) {
			llvm::Type* parameterType = llvm::Type::getInt64Ty( this->llvmContext );
			if( functionDefinition.variableDescriptorTable.count( parameterVariable ) > 0 ) {
				MIRVariableDescriptor& descriptor =
					functionDefinition.variableDescriptorTable[parameterVariable];
				if( descriptor.variableType != nullptr ) {
					parameterType = this->toLLVMType( descriptor.variableType );
				}
			}
			if( parameterType->isVoidTy() ) {
				parameterType = llvm::Type::getInt8PtrTy( this->llvmContext );
			}
			parameterTypes.push_back( parameterType );
		}

		llvm::FunctionType* functionType = llvm::FunctionType::get( returnType, parameterTypes, false );
		llvm::Function* llvmFunction = llvm::Function::Create(
			functionType, llvm::Function::ExternalLinkage,
			llvmFunctionName, this->llvmModule.get()
		);

		// Register with both qualified and unqualified names for call resolution
		this->functionResolutionMap[llvmFunctionName] = llvmFunction;
		this->functionResolutionMap[functionDefinition.functionName] = llvmFunction;

		// Create all basic blocks
		for( std::shared_ptr<MIRBasicBlock>& basicBlock : functionDefinition.controlFlowBlocks ) {
			if( basicBlock != nullptr ) {
				llvm::BasicBlock* llvmBlock = llvm::BasicBlock::Create(
					this->llvmContext, basicBlock->blockLabel, llvmFunction
				);
				this->blockMap[basicBlock->blockIdentifier] = llvmBlock;
			}
		}

		// Set up entry block
		llvm::BasicBlock* entryBlock = this->blockMap[functionDefinition.entryBlockIdentifier];
		if( entryBlock == nullptr ) {
			return;
		}
		this->irBuilder.SetInsertPoint( entryBlock );

		// Allocate parameter variables
		unsigned parameterIndex = 0;
		for( MIRVariableIdentifier parameterVariable : functionDefinition.parameterVariableIdentifiers ) {
			llvm::Argument* argument = llvmFunction->getArg( parameterIndex );
			std::string parameterName = "param";
			if( functionDefinition.variableDescriptorTable.count( parameterVariable ) > 0 ) {
				parameterName = functionDefinition.variableDescriptorTable[parameterVariable].variableName;
			}
			argument->setName( parameterName );

			llvm::AllocaInst* parameterAlloca = this->createEntryBlockAllocation(
				llvmFunction, parameterName, argument->getType()
			);
			this->irBuilder.CreateStore( argument, parameterAlloca );
			this->variableValueMap[parameterVariable] = parameterAlloca;
			parameterIndex++;
		}

		// Generate each basic block
		for( std::shared_ptr<MIRBasicBlock>& basicBlock : functionDefinition.controlFlowBlocks ) {
			if( basicBlock != nullptr ) {
				this->generateBasicBlock( *basicBlock, functionDefinition );
			}
		}

		// Add missing terminators for unterminated blocks
		for( llvm::BasicBlock& llvmBlock : *llvmFunction ) {
			if( llvmBlock.getTerminator() == nullptr ) {
				this->irBuilder.SetInsertPoint( &llvmBlock );
				if( returnType->isVoidTy() ) {
					this->irBuilder.CreateRetVoid();
				}
				else {
					this->irBuilder.CreateRet( llvm::Constant::getNullValue( returnType ) );
				}
			}
		}
	}

	void MIRCodegen::generateBasicBlock(
		MIRBasicBlock& basicBlock,
		MIRFunctionDefinition& functionDefinition
	) {
		llvm::BasicBlock* llvmBlock = this->blockMap[basicBlock.blockIdentifier];
		if( llvmBlock == nullptr ) {
			return;
		}
		this->irBuilder.SetInsertPoint( llvmBlock );

		for( const MIRInstruction& instruction : basicBlock.blockInstructions ) {
			if( this->irBuilder.GetInsertBlock()->getTerminator() != nullptr ) {
				break;
			}
			this->generateInstruction( instruction, functionDefinition );
		}
	}

	// ==========================================================================
	// Instruction dispatch
	// ==========================================================================

	void MIRCodegen::generateInstruction(
		const MIRInstruction& instruction,
		MIRFunctionDefinition& functionDefinition
	) {
		switch( instruction.instructionKind ) {
			case MIRInstructionKind::AllocateLocal:
				this->generateAllocateLocal( instruction, functionDefinition );
				break;
			case MIRInstructionKind::LoadVariable:
				this->generateLoadVariable( instruction );
				break;
			case MIRInstructionKind::StoreVariable:
				this->generateStoreVariable( instruction );
				break;
			case MIRInstructionKind::CopyValue:
				this->generateCopyValue( instruction );
				break;
			case MIRInstructionKind::MoveValue:
				this->generateMoveValue( instruction );
				break;
			case MIRInstructionKind::ConstantInteger:
				this->generateConstantInteger( instruction );
				break;
			case MIRInstructionKind::ConstantFloat:
				this->generateConstantFloat( instruction );
				break;
			case MIRInstructionKind::ConstantBoolean:
				this->generateConstantBoolean( instruction );
				break;
			case MIRInstructionKind::ConstantString:
				this->generateConstantString( instruction );
				break;
			case MIRInstructionKind::ConstantChar:
				this->generateConstantChar( instruction );
				break;
			case MIRInstructionKind::ConstantNone:
				this->generateConstantNone( instruction );
				break;
			case MIRInstructionKind::AddInteger:
			case MIRInstructionKind::SubtractInteger:
			case MIRInstructionKind::MultiplyInteger:
			case MIRInstructionKind::DivideInteger:
			case MIRInstructionKind::ModuloInteger:
			case MIRInstructionKind::NegateInteger:
			case MIRInstructionKind::PowerInteger:
			case MIRInstructionKind::AddFloat:
			case MIRInstructionKind::SubtractFloat:
			case MIRInstructionKind::MultiplyFloat:
			case MIRInstructionKind::DivideFloat:
			case MIRInstructionKind::NegateFloat:
			case MIRInstructionKind::PowerFloat:
				this->generateArithmetic( instruction );
				break;
			case MIRInstructionKind::CompareEqual:
			case MIRInstructionKind::CompareNotEqual:
			case MIRInstructionKind::CompareLessThan:
			case MIRInstructionKind::CompareGreaterThan:
			case MIRInstructionKind::CompareLessEqual:
			case MIRInstructionKind::CompareGreaterEqual:
				this->generateComparison( instruction );
				break;
			case MIRInstructionKind::LogicalAnd:
			case MIRInstructionKind::LogicalOr:
			case MIRInstructionKind::LogicalNot:
				this->generateLogical( instruction );
				break;
			case MIRInstructionKind::BitwiseAnd:
			case MIRInstructionKind::BitwiseOr:
			case MIRInstructionKind::BitwiseXor:
			case MIRInstructionKind::BitwiseNot:
			case MIRInstructionKind::ShiftLeft:
			case MIRInstructionKind::ShiftRight:
				this->generateBitwise( instruction );
				break;
			case MIRInstructionKind::CastType:
				this->generateCastType( instruction );
				break;
			case MIRInstructionKind::CallFunction:
				this->generateCallFunction( instruction, functionDefinition );
				break;
			case MIRInstructionKind::ReturnValue:
				this->generateReturnValue( instruction );
				break;
			case MIRInstructionKind::BranchConditional:
				this->generateBranchConditional( instruction );
				break;
			case MIRInstructionKind::JumpUnconditional:
				this->generateJumpUnconditional( instruction );
				break;
			case MIRInstructionKind::SwitchBranch:
				this->generateSwitchBranch( instruction );
				break;
			case MIRInstructionKind::ComputeFieldAddress:
				this->generateComputeFieldAddress( instruction );
				break;
			case MIRInstructionKind::ComputeIndexAddress:
				this->generateComputeIndexAddress( instruction );
				break;
			case MIRInstructionKind::HeapAllocate:
				this->generateHeapAllocate( instruction, functionDefinition );
				break;
			case MIRInstructionKind::HeapFree:
				this->generateHeapFree( instruction );
				break;
			case MIRInstructionKind::PhiNode:
				this->generatePhiNode( instruction );
				break;
			case MIRInstructionKind::ConstructObject:
				this->generateConstructObject( instruction, functionDefinition );
				break;
			case MIRInstructionKind::NoOperation:
			case MIRInstructionKind::DropValue:
			case MIRInstructionKind::DeferPush:
			case MIRInstructionKind::DeferEmit:
			case MIRInstructionKind::Unreachable:
			case MIRInstructionKind::ThrowException:
			case MIRInstructionKind::LandingPad:
			case MIRInstructionKind::InvokeFunction:
			case MIRInstructionKind::CallVirtual:
			case MIRInstructionKind::DestructObject:
			case MIRInstructionKind::LoadVirtualTable:
			case MIRInstructionKind::TakeReference:
			case MIRInstructionKind::DereferencePointer:
			case MIRInstructionKind::AddressOf:
			case MIRInstructionKind::InstanceOfCheck:
			case MIRInstructionKind::InlineAssembly:
				break;
		}
	}

	// ==========================================================================
	// Variable lifecycle
	// ==========================================================================

	void MIRCodegen::generateAllocateLocal(
		const MIRInstruction& instruction,
		MIRFunctionDefinition& functionDefinition
	) {
		if( instruction.destinationVariable == INVALID_VARIABLE_IDENTIFIER ) {
			return;
		}

		llvm::Type* variableType = llvm::Type::getInt64Ty( this->llvmContext );
		std::string variableName = "local";

		if( functionDefinition.variableDescriptorTable.count( instruction.destinationVariable ) > 0 ) {
			MIRVariableDescriptor& descriptor =
				functionDefinition.variableDescriptorTable[instruction.destinationVariable];
			variableName = descriptor.variableName;
			if( descriptor.variableType != nullptr ) {
				variableType = this->toLLVMType( descriptor.variableType );
			}
		}

		if( variableType->isVoidTy() ) {
			variableType = llvm::Type::getInt64Ty( this->llvmContext );
		}

		llvm::BasicBlock* insertBlock = this->irBuilder.GetInsertBlock();
		if( insertBlock == nullptr ) {
			return;
		}
		llvm::Function* currentFunction = insertBlock->getParent();
		llvm::AllocaInst* alloca = this->createEntryBlockAllocation(
			currentFunction, variableName, variableType
		);
		this->setVariableValue( instruction.destinationVariable, alloca );
	}

	void MIRCodegen::generateLoadVariable( const MIRInstruction& instruction ) {
		if( instruction.destinationVariable == INVALID_VARIABLE_IDENTIFIER ||
			instruction.sourceOperands.empty() ) {
			return;
		}

		llvm::Value* sourcePointer = this->getVariableValue( instruction.sourceOperands[0] );
		if( sourcePointer == nullptr ) {
			return;
		}

		if( llvm::AllocaInst* allocaInst = llvm::dyn_cast<llvm::AllocaInst>( sourcePointer ) ) {
			llvm::Type* allocatedType = allocaInst->getAllocatedType();
			if( allocatedType->isFirstClassType() && allocatedType->isVoidTy() == false ) {
				llvm::Value* loadedValue = this->irBuilder.CreateLoad( allocatedType, sourcePointer, "load" );
				this->setVariableValue( instruction.destinationVariable, loadedValue );
				return;
			}
		}

		if( llvm::GetElementPtrInst* gepInst = llvm::dyn_cast<llvm::GetElementPtrInst>( sourcePointer ) ) {
			llvm::Type* elementType = gepInst->getResultElementType();
			if( elementType->isFirstClassType() && elementType->isVoidTy() == false ) {
				llvm::Value* loadedValue = this->irBuilder.CreateLoad( elementType, sourcePointer, "load.field" );
				this->setVariableValue( instruction.destinationVariable, loadedValue );
				return;
			}
		}

		this->setVariableValue( instruction.destinationVariable, sourcePointer );
	}

	void MIRCodegen::generateStoreVariable( const MIRInstruction& instruction ) {
		if( instruction.destinationVariable == INVALID_VARIABLE_IDENTIFIER ||
			instruction.sourceOperands.empty() ) {
			return;
		}

		llvm::Value* destinationPointer = this->getVariableValue( instruction.destinationVariable );
		llvm::Value* sourceValue = this->getVariableValue( instruction.sourceOperands[0] );
		if( destinationPointer == nullptr || sourceValue == nullptr ) {
			return;
		}

		// Load from source if it's an alloca
		if( llvm::AllocaInst* sourceAlloca = llvm::dyn_cast<llvm::AllocaInst>( sourceValue ) ) {
			llvm::Type* sourceAllocType = sourceAlloca->getAllocatedType();
			if( sourceAllocType->isFirstClassType() && sourceAllocType->isVoidTy() == false ) {
				sourceValue = this->irBuilder.CreateLoad( sourceAllocType, sourceValue, "store.load" );
			}
		}

		// Determine target type for type coercion
		llvm::Type* targetStoreType = nullptr;
		if( llvm::AllocaInst* allocaInst = llvm::dyn_cast<llvm::AllocaInst>( destinationPointer ) ) {
			targetStoreType = allocaInst->getAllocatedType();
		}
		else if( llvm::GetElementPtrInst* gepInst = llvm::dyn_cast<llvm::GetElementPtrInst>( destinationPointer ) ) {
			targetStoreType = gepInst->getResultElementType();
		}
		else if( destinationPointer->getType()->isPointerTy() ) {
			targetStoreType = destinationPointer->getType()->getPointerElementType();
		}

		if( targetStoreType == nullptr ) {
			return;
		}

		// Type coercion for source value
		if( sourceValue->getType() != targetStoreType ) {
			if( sourceValue->getType()->isIntegerTy() && targetStoreType->isIntegerTy() ) {
				unsigned sourceBits = sourceValue->getType()->getIntegerBitWidth();
				unsigned destBits = targetStoreType->getIntegerBitWidth();
				if( sourceBits < destBits ) {
					sourceValue = this->irBuilder.CreateSExt( sourceValue, targetStoreType, "sext.store" );
				}
				else if( sourceBits > destBits ) {
					sourceValue = this->irBuilder.CreateTrunc( sourceValue, targetStoreType, "trunc.store" );
				}
			}
			else if( sourceValue->getType()->isFloatingPointTy() && targetStoreType->isFloatingPointTy() ) {
				sourceValue = this->irBuilder.CreateFPCast( sourceValue, targetStoreType, "fpcast.store" );
			}
			else if( sourceValue->getType()->isIntegerTy() && targetStoreType->isFloatingPointTy() ) {
				sourceValue = this->irBuilder.CreateSIToFP( sourceValue, targetStoreType, "itof.store" );
			}
			else if( sourceValue->getType()->isFloatingPointTy() && targetStoreType->isIntegerTy() ) {
				sourceValue = this->irBuilder.CreateFPToSI( sourceValue, targetStoreType, "ftoi.store" );
			}
			else if( sourceValue->getType()->isPointerTy() && targetStoreType->isPointerTy() ) {
				sourceValue = this->irBuilder.CreateBitCast( sourceValue, targetStoreType, "pcast.store" );
			}
			else if( sourceValue->getType()->isPointerTy() && targetStoreType->isIntegerTy() ) {
				sourceValue = this->irBuilder.CreatePtrToInt( sourceValue, targetStoreType, "ptoi.store" );
			}
			else if( sourceValue->getType()->isIntegerTy() && targetStoreType->isPointerTy() ) {
				sourceValue = this->irBuilder.CreateIntToPtr( sourceValue, targetStoreType, "itop.store" );
			}
			else {
				return;
			}
		}

		this->irBuilder.CreateStore( sourceValue, destinationPointer );
	}

	void MIRCodegen::generateCopyValue( const MIRInstruction& instruction ) {
		if( instruction.destinationVariable == INVALID_VARIABLE_IDENTIFIER ||
			instruction.sourceOperands.empty() ) {
			return;
		}
		llvm::Value* sourceValue = this->getVariableValue( instruction.sourceOperands[0] );
		if( sourceValue != nullptr ) {
			this->setVariableValue( instruction.destinationVariable, sourceValue );
		}
	}

	void MIRCodegen::generateMoveValue( const MIRInstruction& instruction ) {
		// Move is semantically identical to copy at LLVM level
		this->generateCopyValue( instruction );
	}

	// ==========================================================================
	// Constants
	// ==========================================================================

	void MIRCodegen::generateConstantInteger( const MIRInstruction& instruction ) {
		if( instruction.destinationVariable == INVALID_VARIABLE_IDENTIFIER ) {
			return;
		}
		llvm::Value* constValue = llvm::ConstantInt::get(
			llvm::Type::getInt64Ty( this->llvmContext ), instruction.integerConstantValue, true
		);
		this->setVariableValue( instruction.destinationVariable, constValue );
	}

	void MIRCodegen::generateConstantFloat( const MIRInstruction& instruction ) {
		if( instruction.destinationVariable == INVALID_VARIABLE_IDENTIFIER ) {
			return;
		}
		llvm::Value* constValue = llvm::ConstantFP::get(
			llvm::Type::getDoubleTy( this->llvmContext ), instruction.floatConstantValue
		);
		this->setVariableValue( instruction.destinationVariable, constValue );
	}

	void MIRCodegen::generateConstantBoolean( const MIRInstruction& instruction ) {
		if( instruction.destinationVariable == INVALID_VARIABLE_IDENTIFIER ) {
			return;
		}
		llvm::Value* constValue = llvm::ConstantInt::get(
			llvm::Type::getInt1Ty( this->llvmContext ),
			instruction.booleanConstantValue ? 1 : 0
		);
		this->setVariableValue( instruction.destinationVariable, constValue );
	}

	void MIRCodegen::generateConstantString( const MIRInstruction& instruction ) {
		if( instruction.destinationVariable == INVALID_VARIABLE_IDENTIFIER ) {
			return;
		}
		llvm::Value* constValue = this->irBuilder.CreateGlobalStringPtr(
			instruction.stringConstantValue, "str"
		);
		this->setVariableValue( instruction.destinationVariable, constValue );
	}

	void MIRCodegen::generateConstantChar( const MIRInstruction& instruction ) {
		if( instruction.destinationVariable == INVALID_VARIABLE_IDENTIFIER ) {
			return;
		}
		llvm::Value* constValue = llvm::ConstantInt::get(
			llvm::Type::getInt32Ty( this->llvmContext ),
			static_cast<uint32_t>( instruction.charConstantValue )
		);
		this->setVariableValue( instruction.destinationVariable, constValue );
	}

	void MIRCodegen::generateConstantNone( const MIRInstruction& instruction ) {
		if( instruction.destinationVariable == INVALID_VARIABLE_IDENTIFIER ) {
			return;
		}
		llvm::Value* constValue = llvm::ConstantPointerNull::get(
			llvm::Type::getInt8PtrTy( this->llvmContext )
		);
		this->setVariableValue( instruction.destinationVariable, constValue );
	}

	// ==========================================================================
	// Arithmetic
	// ==========================================================================

	void MIRCodegen::generateArithmetic( const MIRInstruction& instruction ) {
		if( instruction.destinationVariable == INVALID_VARIABLE_IDENTIFIER ) {
			return;
		}

		// Unary negate
		if( instruction.instructionKind == MIRInstructionKind::NegateInteger ||
			instruction.instructionKind == MIRInstructionKind::NegateFloat ) {
			if( instruction.sourceOperands.empty() ) {
				return;
			}
			llvm::Value* operand = this->loadVariableValue( instruction.sourceOperands[0] );
			if( operand == nullptr ) {
				return;
			}
			llvm::Value* result = nullptr;
			if( instruction.instructionKind == MIRInstructionKind::NegateInteger ) {
				result = this->irBuilder.CreateNeg( operand, "neg.i" );
			}
			else {
				if( llvm::ConstantFP* constFP = llvm::dyn_cast<llvm::ConstantFP>( operand ) ) {
					llvm::APFloat negated = constFP->getValueAPF();
					negated.changeSign();
					result = llvm::ConstantFP::get( this->llvmContext, negated );
				}
				else {
					result = this->irBuilder.CreateFNeg( operand, "neg.f" );
				}
			}
			this->setVariableValue( instruction.destinationVariable, result );
			return;
		}

		if( instruction.sourceOperands.size() < 2 ) {
			return;
		}

		llvm::Value* leftOperand = this->loadVariableValue( instruction.sourceOperands[0] );
		llvm::Value* rightOperand = this->loadVariableValue( instruction.sourceOperands[1] );
		if( leftOperand == nullptr || rightOperand == nullptr ) {
			return;
		}

		// Coerce mismatched operand types for binary arithmetic
		bool isFloatInstruction =
			instruction.instructionKind == MIRInstructionKind::AddFloat ||
			instruction.instructionKind == MIRInstructionKind::SubtractFloat ||
			instruction.instructionKind == MIRInstructionKind::MultiplyFloat ||
			instruction.instructionKind == MIRInstructionKind::DivideFloat;
		bool isIntegerInstruction =
			instruction.instructionKind == MIRInstructionKind::AddInteger ||
			instruction.instructionKind == MIRInstructionKind::SubtractInteger ||
			instruction.instructionKind == MIRInstructionKind::MultiplyInteger ||
			instruction.instructionKind == MIRInstructionKind::DivideInteger ||
			instruction.instructionKind == MIRInstructionKind::ModuloInteger;

		if( isFloatInstruction ) {
			llvm::Type* doubleTy = llvm::Type::getDoubleTy( this->llvmContext );
			if( leftOperand->getType()->isIntegerTy() ) {
				leftOperand = this->irBuilder.CreateSIToFP( leftOperand, doubleTy, "coerce.itof" );
			}
			if( rightOperand->getType()->isIntegerTy() ) {
				rightOperand = this->irBuilder.CreateSIToFP( rightOperand, doubleTy, "coerce.itof" );
			}
		}
		else if( isIntegerInstruction ) {
			llvm::Type* i64Ty = llvm::Type::getInt64Ty( this->llvmContext );
			if( leftOperand->getType()->isFloatingPointTy() ) {
				leftOperand = this->irBuilder.CreateFPToSI( leftOperand, i64Ty, "coerce.ftoi" );
			}
			if( rightOperand->getType()->isFloatingPointTy() ) {
				rightOperand = this->irBuilder.CreateFPToSI( rightOperand, i64Ty, "coerce.ftoi" );
			}
			if( leftOperand->getType()->isPointerTy() ) {
				leftOperand = this->irBuilder.CreatePtrToInt( leftOperand, i64Ty, "coerce.ptoi" );
			}
			if( rightOperand->getType()->isPointerTy() ) {
				rightOperand = this->irBuilder.CreatePtrToInt( rightOperand, i64Ty, "coerce.ptoi" );
			}
		}

		llvm::Value* result = nullptr;

		switch( instruction.instructionKind ) {
			case MIRInstructionKind::AddInteger:
				result = this->irBuilder.CreateAdd( leftOperand, rightOperand, "add.i" );
				break;
			case MIRInstructionKind::SubtractInteger:
				result = this->irBuilder.CreateSub( leftOperand, rightOperand, "sub.i" );
				break;
			case MIRInstructionKind::MultiplyInteger:
				result = this->irBuilder.CreateMul( leftOperand, rightOperand, "mul.i" );
				break;
			case MIRInstructionKind::DivideInteger:
				result = this->irBuilder.CreateSDiv( leftOperand, rightOperand, "div.i" );
				break;
			case MIRInstructionKind::ModuloInteger:
				result = this->irBuilder.CreateSRem( leftOperand, rightOperand, "mod.i" );
				break;
			case MIRInstructionKind::AddFloat: {
				llvm::ConstantFP* leftConst = llvm::dyn_cast<llvm::ConstantFP>( leftOperand );
				llvm::ConstantFP* rightConst = llvm::dyn_cast<llvm::ConstantFP>( rightOperand );
				if( leftConst && rightConst ) {
					llvm::APFloat res = leftConst->getValueAPF();
					res.add( rightConst->getValueAPF(), llvm::APFloat::rmNearestTiesToEven );
					result = llvm::ConstantFP::get( this->llvmContext, res );
				}
				else {
					result = this->irBuilder.CreateFAdd( leftOperand, rightOperand, "add.f" );
				}
				break;
			}
			case MIRInstructionKind::SubtractFloat: {
				llvm::ConstantFP* leftConst = llvm::dyn_cast<llvm::ConstantFP>( leftOperand );
				llvm::ConstantFP* rightConst = llvm::dyn_cast<llvm::ConstantFP>( rightOperand );
				if( leftConst && rightConst ) {
					llvm::APFloat res = leftConst->getValueAPF();
					res.subtract( rightConst->getValueAPF(), llvm::APFloat::rmNearestTiesToEven );
					result = llvm::ConstantFP::get( this->llvmContext, res );
				}
				else {
					result = this->irBuilder.CreateFSub( leftOperand, rightOperand, "sub.f" );
				}
				break;
			}
			case MIRInstructionKind::MultiplyFloat: {
				llvm::ConstantFP* leftConst = llvm::dyn_cast<llvm::ConstantFP>( leftOperand );
				llvm::ConstantFP* rightConst = llvm::dyn_cast<llvm::ConstantFP>( rightOperand );
				if( leftConst && rightConst ) {
					llvm::APFloat res = leftConst->getValueAPF();
					res.multiply( rightConst->getValueAPF(), llvm::APFloat::rmNearestTiesToEven );
					result = llvm::ConstantFP::get( this->llvmContext, res );
				}
				else {
					result = this->irBuilder.CreateFMul( leftOperand, rightOperand, "mul.f" );
				}
				break;
			}
			case MIRInstructionKind::DivideFloat: {
				llvm::ConstantFP* leftConst = llvm::dyn_cast<llvm::ConstantFP>( leftOperand );
				llvm::ConstantFP* rightConst = llvm::dyn_cast<llvm::ConstantFP>( rightOperand );
				if( leftConst && rightConst ) {
					llvm::APFloat res = leftConst->getValueAPF();
					res.divide( rightConst->getValueAPF(), llvm::APFloat::rmNearestTiesToEven );
					result = llvm::ConstantFP::get( this->llvmContext, res );
				}
				else {
					result = this->irBuilder.CreateFDiv( leftOperand, rightOperand, "div.f" );
				}
				break;
			}
			default:
				return;
		}

		if( result != nullptr ) {
			this->setVariableValue( instruction.destinationVariable, result );
		}
	}

	// ==========================================================================
	// Comparison
	// ==========================================================================

	void MIRCodegen::generateComparison( const MIRInstruction& instruction ) {
		if( instruction.destinationVariable == INVALID_VARIABLE_IDENTIFIER ||
			instruction.sourceOperands.size() < 2 ) {
			return;
		}

		llvm::Value* leftOperand = this->loadVariableValue( instruction.sourceOperands[0] );
		llvm::Value* rightOperand = this->loadVariableValue( instruction.sourceOperands[1] );
		if( leftOperand == nullptr || rightOperand == nullptr ) {
			return;
		}

		// Coerce mismatched types for comparison
		if( leftOperand->getType() != rightOperand->getType() ) {
			if( leftOperand->getType()->isFloatingPointTy() || rightOperand->getType()->isFloatingPointTy() ) {
				llvm::Type* doubleTy = llvm::Type::getDoubleTy( this->llvmContext );
				if( leftOperand->getType()->isIntegerTy() ) {
					leftOperand = this->irBuilder.CreateSIToFP( leftOperand, doubleTy, "cmp.itof" );
				}
				if( rightOperand->getType()->isIntegerTy() ) {
					rightOperand = this->irBuilder.CreateSIToFP( rightOperand, doubleTy, "cmp.itof" );
				}
			}
			else if( leftOperand->getType()->isIntegerTy() && rightOperand->getType()->isIntegerTy() ) {
				unsigned leftBits = leftOperand->getType()->getIntegerBitWidth();
				unsigned rightBits = rightOperand->getType()->getIntegerBitWidth();
				if( leftBits < rightBits ) {
					leftOperand = this->irBuilder.CreateSExt( leftOperand, rightOperand->getType(), "cmp.sext" );
				}
				else {
					rightOperand = this->irBuilder.CreateSExt( rightOperand, leftOperand->getType(), "cmp.sext" );
				}
			}
			else if( leftOperand->getType()->isPointerTy() && rightOperand->getType()->isIntegerTy() ) {
				rightOperand = this->irBuilder.CreateIntToPtr( rightOperand, leftOperand->getType(), "cmp.itop" );
			}
			else if( leftOperand->getType()->isIntegerTy() && rightOperand->getType()->isPointerTy() ) {
				leftOperand = this->irBuilder.CreateIntToPtr( leftOperand, rightOperand->getType(), "cmp.itop" );
			}
		}

		llvm::Value* result = nullptr;
		bool isFloat = leftOperand->getType()->isFloatingPointTy();

		switch( instruction.instructionKind ) {
			case MIRInstructionKind::CompareEqual:
				result = isFloat
					? this->irBuilder.CreateFCmpOEQ( leftOperand, rightOperand, "eq" )
					: this->irBuilder.CreateICmpEQ( leftOperand, rightOperand, "eq" );
				break;
			case MIRInstructionKind::CompareNotEqual:
				result = isFloat
					? this->irBuilder.CreateFCmpONE( leftOperand, rightOperand, "ne" )
					: this->irBuilder.CreateICmpNE( leftOperand, rightOperand, "ne" );
				break;
			case MIRInstructionKind::CompareLessThan:
				result = isFloat
					? this->irBuilder.CreateFCmpOLT( leftOperand, rightOperand, "lt" )
					: this->irBuilder.CreateICmpSLT( leftOperand, rightOperand, "lt" );
				break;
			case MIRInstructionKind::CompareGreaterThan:
				result = isFloat
					? this->irBuilder.CreateFCmpOGT( leftOperand, rightOperand, "gt" )
					: this->irBuilder.CreateICmpSGT( leftOperand, rightOperand, "gt" );
				break;
			case MIRInstructionKind::CompareLessEqual:
				result = isFloat
					? this->irBuilder.CreateFCmpOLE( leftOperand, rightOperand, "le" )
					: this->irBuilder.CreateICmpSLE( leftOperand, rightOperand, "le" );
				break;
			case MIRInstructionKind::CompareGreaterEqual:
				result = isFloat
					? this->irBuilder.CreateFCmpOGE( leftOperand, rightOperand, "ge" )
					: this->irBuilder.CreateICmpSGE( leftOperand, rightOperand, "ge" );
				break;
			default:
				return;
		}

		if( result != nullptr ) {
			this->setVariableValue( instruction.destinationVariable, result );
		}
	}

	// ==========================================================================
	// Logical
	// ==========================================================================

	void MIRCodegen::generateLogical( const MIRInstruction& instruction ) {
		if( instruction.destinationVariable == INVALID_VARIABLE_IDENTIFIER ) {
			return;
		}

		if( instruction.instructionKind == MIRInstructionKind::LogicalNot ) {
			if( instruction.sourceOperands.empty() ) {
				return;
			}
			llvm::Value* operand = this->loadVariableValue( instruction.sourceOperands[0] );
			if( operand == nullptr ) {
				return;
			}
			llvm::Value* result = this->irBuilder.CreateNot( operand, "not" );
			this->setVariableValue( instruction.destinationVariable, result );
			return;
		}

		if( instruction.sourceOperands.size() < 2 ) {
			return;
		}

		llvm::Value* leftOperand = this->loadVariableValue( instruction.sourceOperands[0] );
		llvm::Value* rightOperand = this->loadVariableValue( instruction.sourceOperands[1] );
		if( leftOperand == nullptr || rightOperand == nullptr ) {
			return;
		}

		llvm::Value* result = nullptr;
		if( instruction.instructionKind == MIRInstructionKind::LogicalAnd ) {
			result = this->irBuilder.CreateAnd( leftOperand, rightOperand, "and" );
		}
		else {
			result = this->irBuilder.CreateOr( leftOperand, rightOperand, "or" );
		}

		if( result != nullptr ) {
			this->setVariableValue( instruction.destinationVariable, result );
		}
	}

	// ==========================================================================
	// Bitwise
	// ==========================================================================

	void MIRCodegen::generateBitwise( const MIRInstruction& instruction ) {
		if( instruction.destinationVariable == INVALID_VARIABLE_IDENTIFIER ) {
			return;
		}

		if( instruction.instructionKind == MIRInstructionKind::BitwiseNot ) {
			if( instruction.sourceOperands.empty() ) {
				return;
			}
			llvm::Value* operand = this->loadVariableValue( instruction.sourceOperands[0] );
			if( operand == nullptr ) {
				return;
			}
			llvm::Value* result = this->irBuilder.CreateNot( operand, "bnot" );
			this->setVariableValue( instruction.destinationVariable, result );
			return;
		}

		if( instruction.sourceOperands.size() < 2 ) {
			return;
		}

		llvm::Value* leftOperand = this->loadVariableValue( instruction.sourceOperands[0] );
		llvm::Value* rightOperand = this->loadVariableValue( instruction.sourceOperands[1] );
		if( leftOperand == nullptr || rightOperand == nullptr ) {
			return;
		}

		llvm::Value* result = nullptr;

		switch( instruction.instructionKind ) {
			case MIRInstructionKind::BitwiseAnd:
				result = this->irBuilder.CreateAnd( leftOperand, rightOperand, "band" );
				break;
			case MIRInstructionKind::BitwiseOr:
				result = this->irBuilder.CreateOr( leftOperand, rightOperand, "bor" );
				break;
			case MIRInstructionKind::BitwiseXor:
				result = this->irBuilder.CreateXor( leftOperand, rightOperand, "bxor" );
				break;
			case MIRInstructionKind::ShiftLeft:
				result = this->irBuilder.CreateShl( leftOperand, rightOperand, "shl" );
				break;
			case MIRInstructionKind::ShiftRight:
				result = this->irBuilder.CreateAShr( leftOperand, rightOperand, "shr" );
				break;
			default:
				return;
		}

		if( result != nullptr ) {
			this->setVariableValue( instruction.destinationVariable, result );
		}
	}

	// ==========================================================================
	// Cast
	// ==========================================================================

	void MIRCodegen::generateCastType( const MIRInstruction& instruction ) {
		if( instruction.destinationVariable == INVALID_VARIABLE_IDENTIFIER ||
			instruction.sourceOperands.empty() ||
			instruction.castTargetType == nullptr ) {
			return;
		}

		llvm::Value* sourceValue = this->loadVariableValue( instruction.sourceOperands[0] );
		if( sourceValue == nullptr ) {
			return;
		}

		llvm::Type* targetType = this->toLLVMType( instruction.castTargetType );
		llvm::Type* sourceType = sourceValue->getType();

		llvm::Value* result = nullptr;

		if( sourceType == targetType ) {
			result = sourceValue;
		}
		else if( sourceType->isIntegerTy() && targetType->isIntegerTy() ) {
			unsigned sourceBits = sourceType->getIntegerBitWidth();
			unsigned targetBits = targetType->getIntegerBitWidth();
			if( sourceBits < targetBits ) {
				result = this->irBuilder.CreateSExt( sourceValue, targetType, "sext" );
			}
			else {
				result = this->irBuilder.CreateTrunc( sourceValue, targetType, "trunc" );
			}
		}
		else if( sourceType->isIntegerTy() && targetType->isFloatingPointTy() ) {
			if( llvm::ConstantInt* constInt = llvm::dyn_cast<llvm::ConstantInt>( sourceValue ) ) {
				double doubleValue = static_cast<double>( constInt->getSExtValue() );
				result = llvm::ConstantFP::get( targetType, doubleValue );
			}
			else {
				result = this->irBuilder.CreateSIToFP( sourceValue, targetType, "sitofp" );
			}
		}
		else if( sourceType->isFloatingPointTy() && targetType->isIntegerTy() ) {
			if( llvm::ConstantFP* constFP = llvm::dyn_cast<llvm::ConstantFP>( sourceValue ) ) {
				int64_t intValue = static_cast<int64_t>( constFP->getValueAPF().convertToDouble() );
				result = llvm::ConstantInt::get( targetType, intValue, true );
			}
			else {
				result = this->irBuilder.CreateFPToSI( sourceValue, targetType, "fptosi" );
			}
		}
		else if( sourceType->isFloatingPointTy() && targetType->isFloatingPointTy() ) {
			if( llvm::ConstantFP* constFP = llvm::dyn_cast<llvm::ConstantFP>( sourceValue ) ) {
				result = llvm::ConstantFP::get( targetType, constFP->getValueAPF().convertToDouble() );
			}
			else if( sourceType->getPrimitiveSizeInBits() < targetType->getPrimitiveSizeInBits() ) {
				result = this->irBuilder.CreateFPExt( sourceValue, targetType, "fpext" );
			}
			else {
				result = this->irBuilder.CreateFPTrunc( sourceValue, targetType, "fptrunc" );
			}
		}
		else if( sourceType->isPointerTy() && targetType->isPointerTy() ) {
			result = this->irBuilder.CreateBitCast( sourceValue, targetType, "bitcast" );
		}
		else if( sourceType->isPointerTy() && targetType->isIntegerTy() ) {
			result = this->irBuilder.CreatePtrToInt( sourceValue, targetType, "ptrtoint" );
		}
		else if( sourceType->isIntegerTy() && targetType->isPointerTy() ) {
			result = this->irBuilder.CreateIntToPtr( sourceValue, targetType, "inttoptr" );
		}
		else {
			result = this->irBuilder.CreateBitCast( sourceValue, targetType, "cast" );
		}

		if( result != nullptr ) {
			this->setVariableValue( instruction.destinationVariable, result );
		}
	}

	// ==========================================================================
	// Control flow
	// ==========================================================================

	void MIRCodegen::generateCallFunction(
		const MIRInstruction& instruction,
		MIRFunctionDefinition& functionDefinition
	) {
		// Resolve callee: try resolution map first, then module lookup
		llvm::Function* callee = nullptr;
		if( this->functionResolutionMap.count( instruction.calledFunctionQualifiedName ) > 0 ) {
			callee = this->functionResolutionMap[instruction.calledFunctionQualifiedName];
		}
		if( callee == nullptr ) {
			callee = this->llvmModule->getFunction( instruction.calledFunctionQualifiedName );
		}
		if( callee == nullptr ) {
			// Auto-declare extern function based on call arguments
			std::vector<llvm::Type*> paramTypes;
			for( MIRVariableIdentifier sourceOperand : instruction.sourceOperands ) {
				llvm::Value* argumentValue = this->getVariableValue( sourceOperand );
				if( argumentValue != nullptr ) {
					llvm::Type* argType = argumentValue->getType();
					if( llvm::AllocaInst* allocaInst = llvm::dyn_cast<llvm::AllocaInst>( argumentValue ) ) {
						argType = allocaInst->getAllocatedType();
					}
					paramTypes.push_back( argType );
				}
			}

			llvm::Type* returnType = llvm::Type::getInt64Ty( this->llvmContext );
			if( instruction.operandType != nullptr ) {
				returnType = this->toLLVMType( instruction.operandType );
			}

			llvm::FunctionType* externType = llvm::FunctionType::get( returnType, paramTypes, false );
			callee = llvm::Function::Create(
				externType, llvm::Function::ExternalLinkage,
				instruction.calledFunctionQualifiedName, this->llvmModule.get()
			);
			this->functionResolutionMap[instruction.calledFunctionQualifiedName] = callee;
		}

		llvm::FunctionType* calleeType = callee->getFunctionType();
		unsigned expectedParamCount = calleeType->getNumParams();

		std::vector<llvm::Value*> arguments;
		for( unsigned argumentIndex = 0; argumentIndex < instruction.sourceOperands.size(); argumentIndex++ ) {
			if( argumentIndex >= expectedParamCount && callee->isVarArg() == false ) {
				break;
			}

			llvm::Value* argumentValue = this->loadVariableValue( instruction.sourceOperands[argumentIndex] );

			// Fill nulls with zero/null for the expected type
			if( argumentValue == nullptr ) {
				if( argumentIndex < expectedParamCount ) {
					arguments.push_back( llvm::Constant::getNullValue( calleeType->getParamType( argumentIndex ) ) );
				}
				continue;
			}

			// Cast to expected parameter type if needed
			if( argumentIndex < expectedParamCount ) {
				llvm::Type* expectedType = calleeType->getParamType( argumentIndex );
				if( argumentValue->getType() != expectedType ) {
					if( argumentValue->getType()->isIntegerTy() && expectedType->isIntegerTy() ) {
						unsigned sourceBits = argumentValue->getType()->getIntegerBitWidth();
						unsigned destBits = expectedType->getIntegerBitWidth();
						if( sourceBits < destBits ) {
							argumentValue = this->irBuilder.CreateSExt( argumentValue, expectedType, "arg.sext" );
						}
						else if( sourceBits > destBits ) {
							argumentValue = this->irBuilder.CreateTrunc( argumentValue, expectedType, "arg.trunc" );
						}
					}
					else if( argumentValue->getType()->isPointerTy() && expectedType->isPointerTy() ) {
						argumentValue = this->irBuilder.CreateBitCast( argumentValue, expectedType, "arg.cast" );
					}
					else if( argumentValue->getType()->isIntegerTy() && expectedType->isPointerTy() ) {
						argumentValue = this->irBuilder.CreateIntToPtr( argumentValue, expectedType, "arg.itop" );
					}
					else if( argumentValue->getType()->isPointerTy() && expectedType->isIntegerTy() ) {
						argumentValue = this->irBuilder.CreatePtrToInt( argumentValue, expectedType, "arg.ptoi" );
					}
					else if( argumentValue->getType()->isFloatingPointTy() && expectedType->isIntegerTy() ) {
						if( llvm::ConstantFP* constFP = llvm::dyn_cast<llvm::ConstantFP>( argumentValue ) ) {
							int64_t intValue = static_cast<int64_t>( constFP->getValueAPF().convertToDouble() );
							argumentValue = llvm::ConstantInt::get( expectedType, intValue, true );
						}
						else {
							argumentValue = this->irBuilder.CreateFPToSI( argumentValue, expectedType, "arg.fptoi" );
						}
					}
					else if( argumentValue->getType()->isIntegerTy() && expectedType->isFloatingPointTy() ) {
						if( llvm::ConstantInt* constInt = llvm::dyn_cast<llvm::ConstantInt>( argumentValue ) ) {
							double doubleValue = static_cast<double>( constInt->getSExtValue() );
							argumentValue = llvm::ConstantFP::get( expectedType, doubleValue );
						}
						else {
							argumentValue = this->irBuilder.CreateSIToFP( argumentValue, expectedType, "arg.itofp" );
						}
					}
					else {
						argumentValue = llvm::Constant::getNullValue( expectedType );
					}
				}
			}

			arguments.push_back( argumentValue );
		}

		// Pad missing arguments with null/zero
		while( arguments.size() < expectedParamCount ) {
			llvm::Type* paramType = calleeType->getParamType( arguments.size() );
			arguments.push_back( llvm::Constant::getNullValue( paramType ) );
		}

		// Trim excess arguments
		if( arguments.size() > expectedParamCount && callee->isVarArg() == false ) {
			arguments.resize( expectedParamCount );
		}

		llvm::Value* result = this->irBuilder.CreateCall( callee, arguments );

		if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER &&
			callee->getReturnType()->isVoidTy() == false ) {
			this->setVariableValue( instruction.destinationVariable, result );
		}
	}

	void MIRCodegen::generateReturnValue( const MIRInstruction& instruction ) {
		llvm::BasicBlock* insertBlock = this->irBuilder.GetInsertBlock();
		if( insertBlock == nullptr ) {
			return;
		}
		llvm::Function* currentFunction = insertBlock->getParent();
		llvm::Type* expectedReturnType = currentFunction->getReturnType();

		if( expectedReturnType->isVoidTy() ) {
			this->irBuilder.CreateRetVoid();
			return;
		}

		if( instruction.sourceOperands.empty() ) {
			this->irBuilder.CreateRet( llvm::Constant::getNullValue( expectedReturnType ) );
			return;
		}

		llvm::Value* returnValue = this->loadVariableValue( instruction.sourceOperands[0] );
		if( returnValue == nullptr ) {
			this->irBuilder.CreateRet( llvm::Constant::getNullValue( expectedReturnType ) );
			return;
		}

		// Type mismatch handling
		if( returnValue->getType() != expectedReturnType ) {
			if( returnValue->getType()->isIntegerTy() && expectedReturnType->isIntegerTy() ) {
				unsigned sourceBits = returnValue->getType()->getIntegerBitWidth();
				unsigned destBits = expectedReturnType->getIntegerBitWidth();
				if( sourceBits < destBits ) {
					returnValue = this->irBuilder.CreateSExt( returnValue, expectedReturnType, "ret.sext" );
				}
				else if( sourceBits > destBits ) {
					returnValue = this->irBuilder.CreateTrunc( returnValue, expectedReturnType, "ret.trunc" );
				}
			}
			else if( returnValue->getType()->isPointerTy() && expectedReturnType->isPointerTy() ) {
				returnValue = this->irBuilder.CreateBitCast( returnValue, expectedReturnType, "ret.cast" );
			}
			else if( returnValue->getType()->isPointerTy() && expectedReturnType->isIntegerTy() ) {
				returnValue = this->irBuilder.CreatePtrToInt( returnValue, expectedReturnType, "ret.ptoi" );
			}
			else if( returnValue->getType()->isIntegerTy() && expectedReturnType->isPointerTy() ) {
				returnValue = this->irBuilder.CreateIntToPtr( returnValue, expectedReturnType, "ret.itop" );
			}
			else {
				this->irBuilder.CreateRet( llvm::Constant::getNullValue( expectedReturnType ) );
				return;
			}
		}

		this->irBuilder.CreateRet( returnValue );
	}

	void MIRCodegen::generateBranchConditional( const MIRInstruction& instruction ) {
		if( instruction.sourceOperands.empty() ) {
			return;
		}

		llvm::Value* conditionValue = this->loadVariableValue( instruction.sourceOperands[0] );
		if( conditionValue == nullptr ) {
			return;
		}

		llvm::BasicBlock* trueBlock = nullptr;
		llvm::BasicBlock* falseBlock = nullptr;

		if( this->blockMap.count( instruction.trueBranchTarget ) > 0 ) {
			trueBlock = this->blockMap[instruction.trueBranchTarget];
		}
		if( this->blockMap.count( instruction.falseBranchTarget ) > 0 ) {
			falseBlock = this->blockMap[instruction.falseBranchTarget];
		}

		if( trueBlock != nullptr && falseBlock != nullptr ) {
			this->irBuilder.CreateCondBr( conditionValue, trueBlock, falseBlock );
		}
		else if( trueBlock != nullptr ) {
			this->irBuilder.CreateBr( trueBlock );
		}
	}

	void MIRCodegen::generateJumpUnconditional( const MIRInstruction& instruction ) {
		if( this->blockMap.count( instruction.trueBranchTarget ) > 0 ) {
			this->irBuilder.CreateBr( this->blockMap[instruction.trueBranchTarget] );
		}
	}

	void MIRCodegen::generateSwitchBranch( const MIRInstruction& instruction ) {
		if( instruction.sourceOperands.empty() ) {
			return;
		}

		llvm::Value* switchValue = this->loadVariableValue( instruction.sourceOperands[0] );
		if( switchValue == nullptr ) {
			return;
		}

		llvm::BasicBlock* defaultBlock = nullptr;
		if( this->blockMap.count( instruction.defaultSwitchTarget ) > 0 ) {
			defaultBlock = this->blockMap[instruction.defaultSwitchTarget];
		}
		else {
			defaultBlock = this->irBuilder.GetInsertBlock();
		}

		llvm::SwitchInst* switchInst = this->irBuilder.CreateSwitch(
			switchValue, defaultBlock, static_cast<unsigned>( instruction.switchBranchTargets.size() )
		);

		for( const std::pair<int64_t, MIRBlockIdentifier>& switchTarget : instruction.switchBranchTargets ) {
			if( this->blockMap.count( switchTarget.second ) > 0 ) {
				llvm::ConstantInt* caseValue = llvm::cast<llvm::ConstantInt>(
					llvm::ConstantInt::get( switchValue->getType(), switchTarget.first )
				);
				switchInst->addCase( caseValue, this->blockMap[switchTarget.second] );
			}
		}
	}

	// ==========================================================================
	// Memory
	// ==========================================================================

	void MIRCodegen::generateComputeFieldAddress( const MIRInstruction& instruction ) {
		if( instruction.destinationVariable == INVALID_VARIABLE_IDENTIFIER ||
			instruction.sourceOperands.empty() ) {
			return;
		}

		if( this->irBuilder.GetInsertBlock() == nullptr ) {
			return;
		}

		llvm::Value* basePointer = this->loadVariableValue( instruction.sourceOperands[0] );
		if( basePointer == nullptr ) {
			return;
		}

		if( basePointer->getType()->isPointerTy() == false ) {
			this->setVariableValue( instruction.destinationVariable, basePointer );
			return;
		}

		llvm::Type* pointedType = nullptr;

		// Strategy 1: extract struct type from the pointer type (LLVM 14 typed pointers)
		if( basePointer->getType()->isPointerTy() ) {
			llvm::Type* elementType = basePointer->getType()->getPointerElementType();
			if( elementType->isStructTy() ) {
				pointedType = elementType;
			}
		}

		// Strategy 2: look up from alloca's allocated type
		if( pointedType == nullptr ) {
			llvm::Value* rawPointer = this->getVariableValue( instruction.sourceOperands[0] );
			if( llvm::AllocaInst* allocaInst = llvm::dyn_cast<llvm::AllocaInst>( rawPointer ) ) {
				llvm::Type* allocatedType = allocaInst->getAllocatedType();
				if( allocatedType->isPointerTy() ) {
					llvm::Type* innerType = allocatedType->getPointerElementType();
					if( innerType->isStructTy() ) {
						pointedType = innerType;
					}
				}
				else if( allocatedType->isStructTy() ) {
					pointedType = allocatedType;
				}
			}
		}

		// Strategy 3: look up from struct type cache using field access name
		if( pointedType == nullptr && instruction.fieldAccessName.empty() == false ) {
			for( const std::pair<const std::string, llvm::StructType*>& cacheEntry : this->structTypeCache ) {
				if( this->currentMIRModule != nullptr ) {
					const std::unordered_map<std::string, TypeLayoutDescriptor>& layoutTable =
						this->currentMIRModule->typeLayoutTable;
					if( layoutTable.count( cacheEntry.first ) > 0 ) {
						const TypeLayoutDescriptor& layout = layoutTable.at( cacheEntry.first );
						for( const std::string& fieldName : layout.fieldNames ) {
							if( fieldName == instruction.fieldAccessName ) {
								pointedType = cacheEntry.second;
								break;
							}
						}
						if( pointedType != nullptr ) {
							break;
						}
					}
				}
			}
		}

		if( pointedType != nullptr && pointedType->isStructTy() ) {
			unsigned fieldIndex = static_cast<unsigned>( instruction.fieldLayoutIndex );
			if( fieldIndex < pointedType->getStructNumElements() ) {
				llvm::Value* fieldPointer = this->irBuilder.CreateStructGEP(
					pointedType, basePointer, fieldIndex, "gep.field"
				);
				this->setVariableValue( instruction.destinationVariable, fieldPointer );
			}
			else {
				this->setVariableValue( instruction.destinationVariable, basePointer );
			}
		}
		else {
			this->setVariableValue( instruction.destinationVariable, basePointer );
		}
	}

	void MIRCodegen::generateComputeIndexAddress( const MIRInstruction& instruction ) {
		if( instruction.destinationVariable == INVALID_VARIABLE_IDENTIFIER ||
			instruction.sourceOperands.size() < 2 ) {
			return;
		}

		llvm::Value* basePointer = this->loadVariableValue( instruction.sourceOperands[0] );
		llvm::Value* indexValue = this->loadVariableValue( instruction.sourceOperands[1] );
		if( basePointer == nullptr || indexValue == nullptr ) {
			return;
		}

		if( basePointer->getType()->isPointerTy() ) {
			llvm::Value* elementPointer = this->irBuilder.CreateGEP(
				llvm::Type::getInt8Ty( this->llvmContext ),
				basePointer,
				indexValue,
				"gep.idx"
			);
			this->setVariableValue( instruction.destinationVariable, elementPointer );
		}
	}

	void MIRCodegen::generateHeapAllocate(
		const MIRInstruction& instruction,
		MIRFunctionDefinition& functionDefinition
	) {
		if( instruction.destinationVariable == INVALID_VARIABLE_IDENTIFIER ) {
			return;
		}

		llvm::Function* mallocFunction = this->getOrCreateMalloc();
		int64_t allocationSize = instruction.integerConstantValue > 0
			? instruction.integerConstantValue : 8;

		llvm::Value* sizeValue = llvm::ConstantInt::get(
			llvm::Type::getInt64Ty( this->llvmContext ), allocationSize
		);
		llvm::Value* mallocResult = this->irBuilder.CreateCall( mallocFunction, { sizeValue }, "heap" );
		this->setVariableValue( instruction.destinationVariable, mallocResult );
	}

	void MIRCodegen::generateHeapFree( const MIRInstruction& instruction ) {
		if( instruction.sourceOperands.empty() ) {
			return;
		}

		llvm::Value* pointer = this->loadVariableValue( instruction.sourceOperands[0] );
		if( pointer == nullptr ) {
			return;
		}

		llvm::Function* freeFunction = this->getOrCreateFree();
		llvm::Value* castPointer = this->irBuilder.CreateBitCast(
			pointer, llvm::Type::getInt8PtrTy( this->llvmContext ), "free.cast"
		);
		this->irBuilder.CreateCall( freeFunction, { castPointer } );
	}

	void MIRCodegen::generatePhiNode( const MIRInstruction& instruction ) {
		if( instruction.destinationVariable == INVALID_VARIABLE_IDENTIFIER ||
			instruction.phiIncomingValues.empty() ) {
			return;
		}

		llvm::Value* firstIncomingValue = nullptr;
		for( const std::pair<MIRBlockIdentifier, MIRVariableIdentifier>& phiEntry : instruction.phiIncomingValues ) {
			firstIncomingValue = this->getVariableValue( phiEntry.second );
			if( firstIncomingValue != nullptr ) {
				break;
			}
		}

		if( firstIncomingValue == nullptr ) {
			return;
		}

		llvm::PHINode* phiNode = this->irBuilder.CreatePHI(
			firstIncomingValue->getType(),
			static_cast<unsigned>( instruction.phiIncomingValues.size() ),
			"phi"
		);

		for( const std::pair<MIRBlockIdentifier, MIRVariableIdentifier>& phiEntry : instruction.phiIncomingValues ) {
			llvm::Value* incomingValue = this->getVariableValue( phiEntry.second );
			llvm::BasicBlock* incomingBlock = nullptr;

			if( this->blockMap.count( phiEntry.first ) > 0 ) {
				incomingBlock = this->blockMap[phiEntry.first];
			}

			if( incomingValue != nullptr && incomingBlock != nullptr ) {
				phiNode->addIncoming( incomingValue, incomingBlock );
			}
		}

		this->setVariableValue( instruction.destinationVariable, phiNode );
	}

	void MIRCodegen::generateConstructObject(
		const MIRInstruction& instruction,
		MIRFunctionDefinition& functionDefinition
	) {
		if( instruction.destinationVariable == INVALID_VARIABLE_IDENTIFIER ) {
			return;
		}

		// Find struct type for the constructed type
		std::string typeName = instruction.calledFunctionQualifiedName;
		llvm::StructType* structType = nullptr;

		if( this->structTypeCache.count( typeName ) > 0 ) {
			structType = this->structTypeCache[typeName];
		}
		else {
			structType = llvm::StructType::getTypeByName( this->llvmContext, typeName );
		}

		if( structType != nullptr ) {
			llvm::Function* mallocFunction = this->getOrCreateMalloc();
			llvm::DataLayout dataLayout( this->llvmModule.get() );
			uint64_t typeSize = dataLayout.getTypeAllocSize( structType );
			llvm::Value* sizeValue = llvm::ConstantInt::get(
				llvm::Type::getInt64Ty( this->llvmContext ), typeSize
			);
			llvm::Value* rawPointer = this->irBuilder.CreateCall( mallocFunction, { sizeValue }, "obj.raw" );
			llvm::Value* typedPointer = this->irBuilder.CreateBitCast(
				rawPointer, llvm::PointerType::getUnqual( structType ), "obj"
			);
			this->setVariableValue( instruction.destinationVariable, typedPointer );

			// Call constructor: ClassName.ClassName(self, args...)
			std::string constructorName = typeName + "." + typeName;
			llvm::Function* constructorFunction = nullptr;
			if( this->functionResolutionMap.count( constructorName ) > 0 ) {
				constructorFunction = this->functionResolutionMap[constructorName];
			}
			else {
				constructorFunction = this->llvmModule->getFunction( constructorName );
			}
			if( constructorFunction != nullptr ) {
				std::vector<llvm::Value*> constructorArgs;
				llvm::Type* selfParamType = constructorFunction->getArg( 0 )->getType();
				llvm::Value* selfArg = typedPointer;
				if( selfParamType != typedPointer->getType() ) {
					if( selfParamType->isIntegerTy() ) {
						selfArg = this->irBuilder.CreatePtrToInt( typedPointer, selfParamType, "self.int" );
					}
					else {
						selfArg = this->irBuilder.CreateBitCast( typedPointer, selfParamType, "self.cast" );
					}
				}
				constructorArgs.push_back( selfArg );

				for( MIRVariableIdentifier sourceOperand : instruction.sourceOperands ) {
					llvm::Value* argValue = this->loadVariableValue( sourceOperand );
					if( argValue != nullptr ) {
						unsigned argIndex = constructorArgs.size();
						if( argIndex < constructorFunction->arg_size() ) {
							llvm::Type* expectedType = constructorFunction->getArg( argIndex )->getType();
							if( argValue->getType() != expectedType ) {
								if( expectedType->isIntegerTy() && argValue->getType()->isIntegerTy() ) {
									argValue = this->irBuilder.CreateIntCast(
										argValue, expectedType, true, "arg.cast"
									);
								}
								else if( expectedType->isFloatingPointTy() && argValue->getType()->isIntegerTy() ) {
									argValue = this->irBuilder.CreateSIToFP( argValue, expectedType, "arg.itof" );
								}
								else if( expectedType->isIntegerTy() && argValue->getType()->isFloatingPointTy() ) {
									argValue = this->irBuilder.CreateFPToSI( argValue, expectedType, "arg.ftoi" );
								}
								else if( expectedType->isPointerTy() && argValue->getType()->isPointerTy() ) {
									argValue = this->irBuilder.CreateBitCast( argValue, expectedType, "arg.pcast" );
								}
							}
						}
						constructorArgs.push_back( argValue );
					}
				}
				this->irBuilder.CreateCall( constructorFunction, constructorArgs );
			}
		}
		else {
			llvm::Function* mallocFunction = this->getOrCreateMalloc();
			llvm::Value* sizeValue = llvm::ConstantInt::get(
				llvm::Type::getInt64Ty( this->llvmContext ), 64
			);
			llvm::Value* rawPointer = this->irBuilder.CreateCall( mallocFunction, { sizeValue }, "obj.raw" );
			this->setVariableValue( instruction.destinationVariable, rawPointer );
		}
	}

	// ==========================================================================
	// Helpers
	// ==========================================================================

	llvm::Type* MIRCodegen::toLLVMType( const semantic::TypeSharedPointer& semanticType ) {
		if( semanticType == nullptr ) {
			return llvm::Type::getVoidTy( this->llvmContext );
		}

		switch( semanticType->kind ) {
			case semantic::Type::Kind::Bool:
				return llvm::Type::getInt1Ty( this->llvmContext );
			case semantic::Type::Kind::Char:
				return llvm::Type::getInt32Ty( this->llvmContext );
			case semantic::Type::Kind::Float: {
				semantic::FloatTypeSharedPointer floatType =
					std::static_pointer_cast<semantic::FloatType>( semanticType );
				if( floatType->bitWidth == 32 ) {
					return llvm::Type::getFloatTy( this->llvmContext );
				}
				return llvm::Type::getDoubleTy( this->llvmContext );
			}
			case semantic::Type::Kind::Integer: {
				semantic::IntegerTypeSharedPointer integerType =
					std::static_pointer_cast<semantic::IntegerType>( semanticType );
				switch( integerType->bitWidth ) {
					case 1: return llvm::Type::getInt1Ty( this->llvmContext );
					case 8: return llvm::Type::getInt8Ty( this->llvmContext );
					case 16: return llvm::Type::getInt16Ty( this->llvmContext );
					case 32: return llvm::Type::getInt32Ty( this->llvmContext );
					default: return llvm::Type::getInt64Ty( this->llvmContext );
				}
			}
			case semantic::Type::Kind::String:
				return llvm::Type::getInt8PtrTy( this->llvmContext );
			case semantic::Type::Kind::Void:
				return llvm::Type::getVoidTy( this->llvmContext );
			case semantic::Type::Kind::Pointer:
			case semantic::Type::Kind::Reference:
				return llvm::Type::getInt8PtrTy( this->llvmContext );
			case semantic::Type::Kind::Class: {
				std::string className = semanticType->name;
				const std::string& qualifiedName = semanticType->qualified;

				// OOP wrappers → primitive types
				if( qualifiedName == semantic::qname::INT || qualifiedName == semantic::qname::I64 ||
					className == "Int" || className == "I64" ) {
					return llvm::Type::getInt64Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qname::I32 || className == "I32" ) {
					return llvm::Type::getInt32Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qname::I8 || className == "I8" ) {
					return llvm::Type::getInt8Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qname::UINT || qualifiedName == semantic::qname::U64 ||
					className == "UInt" || className == "U64" ) {
					return llvm::Type::getInt64Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qname::F64 || qualifiedName == semantic::qname::DOUBLE ||
					qualifiedName == semantic::qname::FLOAT ||
					className == "Float" || className == "Double" || className == "F64" ) {
					return llvm::Type::getDoubleTy( this->llvmContext );
				}
				if( qualifiedName == semantic::qname::BOOLEAN || className == "Boolean" ) {
					return llvm::Type::getInt1Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qname::STRING || className == "String" ) {
					return llvm::Type::getInt8PtrTy( this->llvmContext );
				}
				if( qualifiedName == semantic::qname::VOID || className == "Void" ) {
					return llvm::Type::getVoidTy( this->llvmContext );
				}
				if( qualifiedName == semantic::qname::OBJECT || className == "Object" ) {
					return llvm::Type::getInt8PtrTy( this->llvmContext );
				}

				// Class type → struct pointer
				llvm::StructType* structType = llvm::StructType::getTypeByName(
					this->llvmContext, className
				);
				if( structType != nullptr ) {
					return llvm::PointerType::getUnqual( structType );
				}
				return llvm::Type::getInt8PtrTy( this->llvmContext );
			}
			case semantic::Type::Kind::Enum:
				return llvm::Type::getInt32Ty( this->llvmContext );
			case semantic::Type::Kind::Struct: {
				llvm::StructType* structType = llvm::StructType::getTypeByName(
					this->llvmContext, semanticType->name
				);
				if( structType != nullptr ) {
					return llvm::PointerType::getUnqual( structType );
				}
				return llvm::Type::getInt8PtrTy( this->llvmContext );
			}
			case semantic::Type::Kind::Function:
			case semantic::Type::Kind::Callable:
				return llvm::Type::getInt8PtrTy( this->llvmContext );
			case semantic::Type::Kind::Optional:
				return llvm::Type::getInt8PtrTy( this->llvmContext );
			case semantic::Type::Kind::Future:
				return llvm::Type::getInt64Ty( this->llvmContext );
			default:
				return llvm::Type::getInt64Ty( this->llvmContext );
		}
	}

	llvm::Value* MIRCodegen::getVariableValue( MIRVariableIdentifier variableIdentifier ) {
		if( variableIdentifier == INVALID_VARIABLE_IDENTIFIER ) {
			return nullptr;
		}
		if( this->variableValueMap.count( variableIdentifier ) > 0 ) {
			return this->variableValueMap[variableIdentifier];
		}
		return nullptr;
	}

	llvm::Value* MIRCodegen::loadVariableValue( MIRVariableIdentifier variableIdentifier ) {
		llvm::Value* value = this->getVariableValue( variableIdentifier );
		if( value == nullptr ) {
			return nullptr;
		}
		if( llvm::AllocaInst* allocaInst = llvm::dyn_cast<llvm::AllocaInst>( value ) ) {
			llvm::Type* allocatedType = allocaInst->getAllocatedType();
			if( allocatedType->isFirstClassType() && allocatedType->isVoidTy() == false ) {
				return this->irBuilder.CreateLoad( allocatedType, value, "val" );
			}
		}
		if( llvm::GetElementPtrInst* gepInst = llvm::dyn_cast<llvm::GetElementPtrInst>( value ) ) {
			llvm::Type* elementType = gepInst->getResultElementType();
			if( elementType->isFirstClassType() && elementType->isVoidTy() == false ) {
				return this->irBuilder.CreateLoad( elementType, value, "val.field" );
			}
		}
		return value;
	}

	void MIRCodegen::setVariableValue( MIRVariableIdentifier variableIdentifier, llvm::Value* value ) {
		if( variableIdentifier != INVALID_VARIABLE_IDENTIFIER && value != nullptr ) {
			this->variableValueMap[variableIdentifier] = value;
		}
	}

	llvm::AllocaInst* MIRCodegen::createEntryBlockAllocation(
		llvm::Function* function,
		const std::string& name,
		llvm::Type* type
	) {
		if( type->isVoidTy() ) {
			type = llvm::Type::getInt8PtrTy( function->getContext() );
		}
		llvm::IRBuilder<> entryBuilder( &function->getEntryBlock(), function->getEntryBlock().begin() );
		return entryBuilder.CreateAlloca( type, nullptr, name );
	}

	llvm::Function* MIRCodegen::getOrCreateMalloc() {
		llvm::Function* mallocFunction = this->llvmModule->getFunction( "malloc" );
		if( mallocFunction == nullptr ) {
			llvm::FunctionType* mallocType = llvm::FunctionType::get(
				llvm::Type::getInt8PtrTy( this->llvmContext ),
				{ llvm::Type::getInt64Ty( this->llvmContext ) },
				false
			);
			mallocFunction = llvm::Function::Create(
				mallocType, llvm::Function::ExternalLinkage, "malloc", this->llvmModule.get()
			);
		}
		return mallocFunction;
	}

	llvm::Function* MIRCodegen::getOrCreateFree() {
		llvm::Function* freeFunction = this->llvmModule->getFunction( "free" );
		if( freeFunction == nullptr ) {
			llvm::FunctionType* freeType = llvm::FunctionType::get(
				llvm::Type::getVoidTy( this->llvmContext ),
				{ llvm::Type::getInt8PtrTy( this->llvmContext ) },
				false
			);
			freeFunction = llvm::Function::Create(
				freeType, llvm::Function::ExternalLinkage, "free", this->llvmModule.get()
			);
		}
		return freeFunction;
	}

} // namespace uranite::ir::mir
