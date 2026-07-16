
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

	void MIRCodegen::setTargetTriple( const std::string& triple ) {
		this->targetTriple_ = triple;
	}

	bool MIRCodegen::generate( MIRModuleDefinition& mirModule ) {
		this->llvmModule = std::make_unique<llvm::Module>( mirModule.moduleName, this->llvmContext );
		std::string resolvedTriple = this->targetTriple_.empty() ? llvm::sys::getDefaultTargetTriple() : this->targetTriple_;
		this->llvmModule->setTargetTriple( resolvedTriple );
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
					llvmFieldType = llvm::PointerType::getUnqual( this->llvmContext );
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

		for( const std::pair<const std::string, MIRGlobalVariable>& globalEntry : mirModule.globalVariables ) {
			const MIRGlobalVariable& mirGlobal = globalEntry.second;
			llvm::Type* globalType = this->toLLVMType( mirGlobal.variableType );
			if( globalType->isVoidTy() ) {
				globalType = llvm::Type::getInt64Ty( this->llvmContext );
			}
			llvm::Constant* initializer = nullptr;
			if( mirGlobal.hasInitializer ) {
				if( mirGlobal.initialValue.kind == MIRModuleConstant::Integer ) {
					initializer = llvm::ConstantInt::get( globalType->isIntegerTy() ? globalType : llvm::Type::getInt64Ty( this->llvmContext ), mirGlobal.initialValue.integerValue );
				}
				else if( mirGlobal.initialValue.kind == MIRModuleConstant::Float ) {
					initializer = llvm::ConstantFP::get( globalType->isFloatingPointTy() ? globalType : llvm::Type::getDoubleTy( this->llvmContext ), mirGlobal.initialValue.floatValue );
				}
				else if( mirGlobal.initialValue.kind == MIRModuleConstant::Boolean ) {
					initializer = llvm::ConstantInt::get( llvm::Type::getInt1Ty( this->llvmContext ), mirGlobal.initialValue.booleanValue ? 1 : 0 );
				}
			}
			if( initializer == nullptr ) {
				initializer = llvm::Constant::getNullValue( globalType );
			}
			new llvm::GlobalVariable(
				*this->llvmModule, globalType, false,
				llvm::GlobalValue::InternalLinkage, initializer, mirGlobal.variableName
			);
		}

		for( std::shared_ptr<MIRFunctionDefinition>& functionDefinition : mirModule.functionDefinitions ) {
			if( functionDefinition == nullptr ) {
				continue;
			}
			std::string llvmFunctionName = functionDefinition->functionName;
			if( functionDefinition->ownerClassQualifiedName.empty() == false ) {
				llvmFunctionName = functionDefinition->ownerClassQualifiedName + "." + functionDefinition->functionName;
			}
			if( this->functionResolutionMap.count( llvmFunctionName ) > 0 ) {
				continue;
			}
			bool isMainFunction = ( llvmFunctionName == "main" &&
				functionDefinition->ownerClassQualifiedName.empty() );

			semantic::TypeSharedPointer returnTypeDesc = functionDefinition->returnTypeDescriptor != nullptr
				? functionDefinition->returnTypeDescriptor
				: std::make_shared<semantic::Type>( semantic::Type::Kind::Void, "Void" );
			llvm::Type* returnType = this->toLLVMType( returnTypeDesc );
			if( isMainFunction ) {
				returnType = llvm::Type::getInt32Ty( this->llvmContext );
			}
			else if( returnType->isPointerTy() == false &&
				( returnTypeDesc->kind == semantic::Type::Kind::Class ||
				  returnTypeDesc->kind == semantic::Type::Kind::Struct ) &&
				this->functionReturnsConstructedObject( *functionDefinition ) ) {
				returnType = llvm::PointerType::getUnqual( this->llvmContext );
			}
			std::vector<llvm::Type*> parameterTypes;
			if( isMainFunction ) {
				parameterTypes.push_back( llvm::Type::getInt32Ty( this->llvmContext ) );
				parameterTypes.push_back( llvm::PointerType::getUnqual( this->llvmContext ) );
			}
			for( MIRVariableIdentifier parameterVariable : functionDefinition->parameterVariableIdentifiers ) {
				llvm::Type* parameterType = llvm::Type::getInt64Ty( this->llvmContext );
				if( functionDefinition->variableDescriptorTable.count( parameterVariable ) > 0 ) {
					MIRVariableDescriptor& descriptor =
						functionDefinition->variableDescriptorTable[parameterVariable];
					if( descriptor.variableType != nullptr ) {
						parameterType = this->toLLVMType( descriptor.variableType );
					}
				}
				if( parameterType->isVoidTy() ) {
					parameterType = llvm::PointerType::getUnqual( this->llvmContext );
				}
				if( functionDefinition->ownerClassQualifiedName.empty() == false && parameterTypes.empty() ) {
					parameterType = llvm::PointerType::getUnqual( this->llvmContext );
				}
				parameterTypes.push_back( parameterType );
			}
			llvm::FunctionType* functionType = llvm::FunctionType::get( returnType, parameterTypes, false );
			llvm::Function* llvmFunction = llvm::Function::Create(
				functionType, llvm::Function::ExternalLinkage,
				llvmFunctionName, this->llvmModule.get()
			);
			this->functionResolutionMap[llvmFunctionName] = llvmFunction;
			if( functionDefinition->ownerClassQualifiedName.empty() ) {
				this->functionResolutionMap[functionDefinition->functionName] = llvmFunction;
			}
		}

		// Generate Object.toString stub — identity function returning self pointer.
		// writeArgsToFd calls args.get(index).toString() on Object; without this
		// stub the linker fails on undefined reference.
		if( this->llvmModule->getFunction( "Object.toString" ) == nullptr ) {
			llvm::Type* pointerType = llvm::PointerType::getUnqual( this->llvmContext );
			llvm::FunctionType* toStringType = llvm::FunctionType::get( pointerType, { pointerType }, false );
			llvm::Function* toStringFunction = llvm::Function::Create(
				toStringType, llvm::Function::ExternalLinkage, "Object.toString", this->llvmModule.get()
			);
			llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(
				this->llvmContext, "entry", toStringFunction
			);
			llvm::IRBuilder<> stubBuilder( this->llvmContext );
			stubBuilder.SetInsertPoint( entryBlock );
			stubBuilder.CreateRet( toStringFunction->getArg( 0 ) );
			this->functionResolutionMap["Object.toString"] = toStringFunction;
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
			std::optional<llvm::Reloc::Model>()
		);
		this->llvmModule->setDataLayout( targetMachine->createDataLayout() );

		std::error_code errorCode;
		llvm::raw_fd_ostream outputStream( filename, errorCode, llvm::sys::fs::OF_None );
		if( errorCode ) {
			return false;
		}

		llvm::legacy::PassManager passManager;
		if( targetMachine->addPassesToEmitFile( passManager, outputStream, nullptr,
			llvm::CodeGenFileType::ObjectFile ) ) {
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
		std::string llvmFunctionName = functionDefinition.functionName;
		if( functionDefinition.ownerClassQualifiedName.empty() == false ) {
			llvmFunctionName = functionDefinition.ownerClassQualifiedName + "." + functionDefinition.functionName;
		}

		llvm::Function* llvmFunction = nullptr;
		if( this->functionResolutionMap.count( llvmFunctionName ) > 0 ) {
			llvmFunction = this->functionResolutionMap[llvmFunctionName];
			if( llvmFunction != nullptr && llvmFunction->isDeclaration() == false ) {
				return;
			}
		}

		this->variableValueMap.clear();
		this->blockMap.clear();
		this->memoryElementTypes.clear();
		this->currentMIRFunction = &functionDefinition;

		if( llvmFunction == nullptr ) {
			bool isMainFunction = ( llvmFunctionName == "main" &&
				functionDefinition.ownerClassQualifiedName.empty() );

			semantic::TypeSharedPointer returnTypeDesc = functionDefinition.returnTypeDescriptor != nullptr
				? functionDefinition.returnTypeDescriptor
				: std::make_shared<semantic::Type>( semantic::Type::Kind::Void, "Void" );
			llvm::Type* returnType = this->toLLVMType( returnTypeDesc );
			if( isMainFunction ) {
				returnType = llvm::Type::getInt32Ty( this->llvmContext );
			}
			else if( returnType->isPointerTy() == false &&
				( returnTypeDesc->kind == semantic::Type::Kind::Class ||
				  returnTypeDesc->kind == semantic::Type::Kind::Struct ) &&
				this->functionReturnsConstructedObject( functionDefinition ) ) {
				returnType = llvm::PointerType::getUnqual( this->llvmContext );
			}
			std::vector<llvm::Type*> parameterTypes;
			if( isMainFunction ) {
				parameterTypes.push_back( llvm::Type::getInt32Ty( this->llvmContext ) );
				parameterTypes.push_back( llvm::PointerType::getUnqual( this->llvmContext ) );
			}
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
					parameterType = llvm::PointerType::getUnqual( this->llvmContext );
				}
				if( functionDefinition.ownerClassQualifiedName.empty() == false && parameterTypes.empty() ) {
					parameterType = llvm::PointerType::getUnqual( this->llvmContext );
				}
				parameterTypes.push_back( parameterType );
			}
			llvm::FunctionType* functionType = llvm::FunctionType::get( returnType, parameterTypes, false );
			llvmFunction = llvm::Function::Create(
				functionType, llvm::Function::ExternalLinkage,
				llvmFunctionName, this->llvmModule.get()
			);
			this->functionResolutionMap[llvmFunctionName] = llvmFunction;
			if( functionDefinition.ownerClassQualifiedName.empty() ) {
				this->functionResolutionMap[functionDefinition.functionName] = llvmFunction;
			}
		}

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

		llvm::Type* functionReturnType = llvmFunction->getReturnType();
		for( llvm::BasicBlock& llvmBlock : *llvmFunction ) {
			if( llvmBlock.getTerminator() == nullptr ) {
				this->irBuilder.SetInsertPoint( &llvmBlock );
				if( functionReturnType->isVoidTy() ) {
					this->irBuilder.CreateRetVoid();
				}
				else {
					this->irBuilder.CreateRet( llvm::Constant::getNullValue( functionReturnType ) );
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
			case MIRInstructionKind::Unreachable:
				this->irBuilder.CreateUnreachable();
				break;
			case MIRInstructionKind::ThrowException: {
				llvm::Function* throwFunction = this->getOrCreateUraniteThrow();
				llvm::Value* thrownObject = nullptr;
				if( instruction.sourceOperands.empty() == false ) {
					thrownObject = this->loadVariableValue( instruction.sourceOperands[0] );
				}
				if( thrownObject == nullptr ) {
					thrownObject = llvm::ConstantPointerNull::get(
						llvm::PointerType::getUnqual( this->llvmContext )
					);
				}
				if( thrownObject->getType()->isPointerTy() == false ) {
					thrownObject = this->irBuilder.CreateIntToPtr(
						thrownObject, llvm::PointerType::getUnqual( this->llvmContext ), "throw.ptr"
					);
				}
				std::string typeName = instruction.calledFunctionQualifiedName.empty()
					? "Error"
					: instruction.calledFunctionQualifiedName;
				llvm::Value* typeNameGlobal = this->irBuilder.CreateGlobalStringPtr( typeName, "throw.typename" );
				this->irBuilder.CreateCall( throwFunction, { thrownObject, typeNameGlobal } );
				this->irBuilder.CreateUnreachable();
				break;
			}
			case MIRInstructionKind::NoOperation:
			case MIRInstructionKind::DropValue:
			case MIRInstructionKind::DeferPush:
			case MIRInstructionKind::DeferEmit:
			case MIRInstructionKind::LandingPad:
			case MIRInstructionKind::InvokeFunction:
			case MIRInstructionKind::CallVirtual:
			case MIRInstructionKind::DestructObject:
			case MIRInstructionKind::LoadVirtualTable:
			case MIRInstructionKind::AddressOf: {
				if( instruction.sourceOperands.empty() == false && instruction.destinationVariable != 0 ) {
					llvm::Value* operandValue = this->getVariableValue( instruction.sourceOperands[0] );
					if( operandValue == nullptr ) {
						operandValue = this->loadVariableValue( instruction.sourceOperands[0] );
					}
					llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
					if( operandValue != nullptr ) {
						if( operandValue->getType()->isPointerTy() ) {
							this->setVariableValue( instruction.destinationVariable,
								this->irBuilder.CreatePtrToInt( operandValue, i64Type, "addrof" ) );
						}
						else if( operandValue->getType()->isIntegerTy() ) {
							llvm::Value* cast = operandValue;
							if( operandValue->getType() != i64Type ) {
								cast = this->irBuilder.CreateSExt( operandValue, i64Type, "addrof.ext" );
							}
							this->setVariableValue( instruction.destinationVariable, cast );
						}
						else {
							this->setVariableValue( instruction.destinationVariable,
								llvm::Constant::getNullValue( i64Type ) );
						}
					}
					else {
						this->setVariableValue( instruction.destinationVariable,
							llvm::Constant::getNullValue( i64Type ) );
					}
				}
				break;
			}
			case MIRInstructionKind::InstanceOfCheck: {
				if( instruction.destinationVariable != 0 ) {
					this->setVariableValue( instruction.destinationVariable,
						llvm::ConstantInt::getFalse( this->llvmContext ) );
				}
				break;
			}
			case MIRInstructionKind::TakeReference: {
				if( instruction.sourceOperands.empty() == false && instruction.destinationVariable != 0 ) {
					llvm::Value* operandValue = this->getVariableValue( instruction.sourceOperands[0] );
					if( operandValue != nullptr ) {
						this->setVariableValue( instruction.destinationVariable, operandValue );
					}
					else {
						this->setVariableValue( instruction.destinationVariable,
							llvm::ConstantPointerNull::get( llvm::PointerType::getUnqual( this->llvmContext ) ) );
					}
				}
				break;
			}
			case MIRInstructionKind::DereferencePointer: {
				if( instruction.sourceOperands.empty() == false && instruction.destinationVariable != 0 ) {
					llvm::Value* ptrValue = this->loadVariableValue( instruction.sourceOperands[0] );
					if( ptrValue != nullptr && ptrValue->getType()->isPointerTy() ) {
						llvm::Type* loadType = llvm::Type::getInt64Ty( this->llvmContext );
						if( instruction.operandType != nullptr ) {
							loadType = this->toLLVMType( instruction.operandType );
						}
						llvm::Value* loaded = this->irBuilder.CreateLoad( loadType, ptrValue, "deref" );
						this->setVariableValue( instruction.destinationVariable, loaded );
					}
					else if( ptrValue != nullptr ) {
						this->setVariableValue( instruction.destinationVariable, ptrValue );
					}
				}
				break;
			}
			case MIRInstructionKind::InlineAssembly: {
				std::string constraintString;
				std::vector<llvm::Type*> outputTypes;
				std::vector<llvm::Value*> outputPointers;
				std::vector<llvm::Value*> inputValues;
				std::vector<llvm::Type*> inputTypes;

				for( size_t outputIndex = 0; outputIndex < instruction.assemblyOutputVariables.size(); outputIndex++ ) {
					if( constraintString.empty() == false ) {
						constraintString += ",";
					}
					constraintString += instruction.assemblyOutputConstraints[outputIndex];
					llvm::Value* outputPointer = this->getVariableValue( instruction.assemblyOutputVariables[outputIndex] );
					outputPointers.push_back( outputPointer );
					if( outputPointer != nullptr ) {
						llvm::Type* outputElementType = nullptr;
						if( llvm::AllocaInst* allocaInst = llvm::dyn_cast<llvm::AllocaInst>( outputPointer ) ) {
							outputElementType = allocaInst->getAllocatedType();
						}
						else if( llvm::GetElementPtrInst* gepInst = llvm::dyn_cast<llvm::GetElementPtrInst>( outputPointer ) ) {
							outputElementType = gepInst->getResultElementType();
						}
						if( outputElementType != nullptr ) {
							outputTypes.push_back( outputElementType );
						}
						else {
							outputTypes.push_back( llvm::Type::getInt64Ty( this->llvmContext ) );
						}
					}
					else {
						outputTypes.push_back( llvm::Type::getInt64Ty( this->llvmContext ) );
					}
				}

				for( size_t inputIndex = 0; inputIndex < instruction.assemblyInputVariables.size(); inputIndex++ ) {
					if( constraintString.empty() == false ) {
						constraintString += ",";
					}
					constraintString += instruction.assemblyInputConstraints[inputIndex];
					llvm::Value* inputValue = this->loadVariableValue( instruction.assemblyInputVariables[inputIndex] );
					if( inputValue == nullptr ) {
						inputValue = llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->llvmContext ), 0 );
					}
					else if( inputValue->getType()->isPointerTy() ) {
						inputValue = this->irBuilder.CreatePtrToInt( inputValue, llvm::Type::getInt64Ty( this->llvmContext ), "asm.input.ptrtoint" );
					}
					inputValues.push_back( inputValue );
					inputTypes.push_back( inputValue->getType() );
				}

				for( const std::string& clobber : instruction.assemblyClobbers ) {
					if( constraintString.empty() == false ) {
						constraintString += ",";
					}
					constraintString += fmt::format( "~{{{}}}", clobber );
				}

				llvm::Type* resultType = nullptr;
				if( outputTypes.empty() ) {
					resultType = llvm::Type::getVoidTy( this->llvmContext );
				}
				else if( outputTypes.size() == 1 ) {
					resultType = outputTypes[0];
				}
				else {
					resultType = llvm::StructType::get( this->llvmContext, outputTypes );
				}

				llvm::FunctionType* asmFunctionType = llvm::FunctionType::get( resultType, inputTypes, false );
				llvm::InlineAsm* inlineAsm = llvm::InlineAsm::get(
					asmFunctionType,
					instruction.assemblyTemplate,
					constraintString,
					instruction.assemblyIsVolatile
				);
				llvm::CallInst* asmResult = this->irBuilder.CreateCall( asmFunctionType, inlineAsm, inputValues );

				if( outputPointers.empty() == false ) {
					for( size_t outputIndex = 0; outputIndex < outputPointers.size(); outputIndex++ ) {
						llvm::Value* outputPointer = outputPointers[outputIndex];
						if( outputPointer != nullptr ) {
							llvm::Value* outputValue = asmResult;
							if( outputPointers.size() > 1 ) {
								outputValue = this->irBuilder.CreateExtractValue( asmResult, outputIndex, "asm.out" );
							}
							llvm::Type* storeType = nullptr;
							if( llvm::AllocaInst* allocaInst = llvm::dyn_cast<llvm::AllocaInst>( outputPointer ) ) {
								storeType = allocaInst->getAllocatedType();
							}
							else if( llvm::GetElementPtrInst* gepInst = llvm::dyn_cast<llvm::GetElementPtrInst>( outputPointer ) ) {
								storeType = gepInst->getResultElementType();
							}
							if( storeType != nullptr && outputValue->getType() != storeType ) {
								if( storeType->isIntegerTy() && outputValue->getType()->isIntegerTy() ) {
									outputValue = this->irBuilder.CreateIntCast( outputValue, storeType, true, "asm.out.cast" );
								}
							}
							this->irBuilder.CreateStore( outputValue, outputPointer );
						}
					}
				}

				if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER && outputPointers.empty() == false ) {
					this->setVariableValue( instruction.destinationVariable, outputPointers[0] );
				}
				break;
			}
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
		if( instruction.calledFunctionQualifiedName.empty() == false &&
			instruction.calledFunctionQualifiedName[0] == '@' ) {
			std::string globalName = instruction.calledFunctionQualifiedName.substr( 1 );
			llvm::GlobalVariable* globalVar = this->llvmModule->getGlobalVariable( globalName, true );
			if( globalVar != nullptr && instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
				llvm::Value* loadedValue = this->irBuilder.CreateLoad( globalVar->getValueType(), globalVar, "global.load" );
				this->setVariableValue( instruction.destinationVariable, loadedValue );
				return;
			}
		}
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
		if( instruction.calledFunctionQualifiedName.empty() == false &&
			instruction.calledFunctionQualifiedName[0] == '@' ) {
			std::string globalName = instruction.calledFunctionQualifiedName.substr( 1 );
			llvm::GlobalVariable* globalVar = this->llvmModule->getGlobalVariable( globalName, true );
			if( globalVar != nullptr && instruction.sourceOperands.empty() == false ) {
				llvm::Value* sourceValue = this->loadVariableValue( instruction.sourceOperands[0] );
				if( sourceValue != nullptr ) {
					if( sourceValue->getType() != globalVar->getValueType() ) {
						if( sourceValue->getType()->isIntegerTy() && globalVar->getValueType()->isIntegerTy() ) {
							sourceValue = this->irBuilder.CreateIntCast( sourceValue, globalVar->getValueType(), true, "global.cast" );
						}
						else if( sourceValue->getType()->isFloatingPointTy() && globalVar->getValueType()->isFloatingPointTy() ) {
							sourceValue = this->irBuilder.CreateFPCast( sourceValue, globalVar->getValueType(), "global.fcast" );
						}
					}
					this->irBuilder.CreateStore( sourceValue, globalVar );
				}
				return;
			}
		}
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
		else if( llvm::GlobalVariable* globalVar = llvm::dyn_cast<llvm::GlobalVariable>( destinationPointer ) ) {
			targetStoreType = globalVar->getValueType();
		}
		else if( destinationPointer->getType()->isPointerTy() ) {
			targetStoreType = sourceValue->getType();
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
			else if( sourceValue->getType()->isPointerTy() &&
					( targetStoreType->isFloatingPointTy() || targetStoreType->isIntegerTy() == false ) ) {
				llvm::AllocaInst* destAlloca = llvm::dyn_cast<llvm::AllocaInst>( destinationPointer );
				if( destAlloca != nullptr ) {
					llvm::IRBuilder<> entryBuilder(
						&destAlloca->getFunction()->getEntryBlock(),
						destAlloca->getFunction()->getEntryBlock().begin()
					);
					llvm::AllocaInst* newAlloca = entryBuilder.CreateAlloca(
						llvm::PointerType::getUnqual( this->llvmContext ), nullptr,
						destAlloca->getName() + ".ptr"
					);
					this->variableValueMap[instruction.destinationVariable] = newAlloca;
					destinationPointer = newAlloca;
					targetStoreType = llvm::PointerType::getUnqual( this->llvmContext );
				}
				else {
					return;
				}
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
			llvm::PointerType::getUnqual( this->llvmContext )
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
			if( operand->getType()->isFloatingPointTy() ) {
				if( llvm::ConstantFP* constFP = llvm::dyn_cast<llvm::ConstantFP>( operand ) ) {
					llvm::APFloat negated = constFP->getValueAPF();
					negated.changeSign();
					result = llvm::ConstantFP::get( this->llvmContext, negated );
				}
				else {
					result = this->irBuilder.CreateFNeg( operand, "neg.f" );
				}
			}
			else if( operand->getType()->isIntegerTy() ) {
				result = this->irBuilder.CreateNeg( operand, "neg.i" );
			}
			else if( operand->getType()->isPointerTy() ) {
				llvm::Value* asInt = this->irBuilder.CreatePtrToInt(
					operand, llvm::Type::getInt64Ty( this->llvmContext ), "neg.ptoi"
				);
				result = this->irBuilder.CreateNeg( asInt, "neg.i" );
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
			if( leftOperand->getType()->isPointerTy() ) {
				leftOperand = this->irBuilder.CreatePtrToInt(
					leftOperand, llvm::Type::getInt64Ty( this->llvmContext ), "coerce.ptoi"
				);
			}
			if( rightOperand->getType()->isPointerTy() ) {
				rightOperand = this->irBuilder.CreatePtrToInt(
					rightOperand, llvm::Type::getInt64Ty( this->llvmContext ), "coerce.ptoi"
				);
			}
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
			if( leftOperand->getType() != rightOperand->getType() ) {
				if( leftOperand->getType()->isIntegerTy() && rightOperand->getType()->isIntegerTy() ) {
					unsigned leftBits = leftOperand->getType()->getIntegerBitWidth();
					unsigned rightBits = rightOperand->getType()->getIntegerBitWidth();
					if( leftBits < rightBits ) {
						leftOperand = this->irBuilder.CreateSExt( leftOperand, rightOperand->getType(), "coerce.sext" );
					}
					else {
						rightOperand = this->irBuilder.CreateSExt( rightOperand, leftOperand->getType(), "coerce.sext" );
					}
				}
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
			llvm::Value* result = nullptr;
			if( operand->getType()->isIntegerTy( 1 ) ) {
				result = this->irBuilder.CreateNot( operand, "not" );
			}
			else if( operand->getType()->isIntegerTy() ) {
				result = this->irBuilder.CreateICmpEQ(
					operand, llvm::ConstantInt::get( operand->getType(), 0 ), "not"
				);
			}
			else if( operand->getType()->isPointerTy() ) {
				result = this->irBuilder.CreateICmpEQ(
					operand,
					llvm::ConstantPointerNull::get( llvm::cast<llvm::PointerType>( operand->getType() ) ),
					"not"
				);
			}
			else {
				result = this->irBuilder.CreateNot( operand, "not" );
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

		if( leftOperand->getType()->isIntegerTy( 1 ) == false && leftOperand->getType()->isIntegerTy() ) {
			leftOperand = this->irBuilder.CreateICmpNE(
				leftOperand, llvm::ConstantInt::get( leftOperand->getType(), 0 ), "lhs.bool"
			);
		}
		if( rightOperand->getType()->isIntegerTy( 1 ) == false && rightOperand->getType()->isIntegerTy() ) {
			rightOperand = this->irBuilder.CreateICmpNE(
				rightOperand, llvm::ConstantInt::get( rightOperand->getType(), 0 ), "rhs.bool"
			);
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
			if( operand->getType()->isPointerTy() ) {
				operand = this->irBuilder.CreatePtrToInt(
					operand, llvm::Type::getInt64Ty( this->llvmContext ), "bnot.ptoi"
				);
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

		llvm::Type* i64Ty = llvm::Type::getInt64Ty( this->llvmContext );
		if( leftOperand->getType()->isPointerTy() ) {
			leftOperand = this->irBuilder.CreatePtrToInt( leftOperand, i64Ty, "bw.ptoi" );
		}
		if( rightOperand->getType()->isPointerTy() ) {
			rightOperand = this->irBuilder.CreatePtrToInt( rightOperand, i64Ty, "bw.ptoi" );
		}
		if( leftOperand->getType() != rightOperand->getType() ) {
			if( leftOperand->getType()->isIntegerTy() && rightOperand->getType()->isIntegerTy() ) {
				unsigned leftBits = leftOperand->getType()->getIntegerBitWidth();
				unsigned rightBits = rightOperand->getType()->getIntegerBitWidth();
				if( leftBits < rightBits ) {
					leftOperand = this->irBuilder.CreateSExt( leftOperand, rightOperand->getType(), "bw.sext" );
				}
				else {
					rightOperand = this->irBuilder.CreateSExt( rightOperand, leftOperand->getType(), "bw.sext" );
				}
			}
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
		else if( sourceType->isFloatingPointTy() && targetType->isPointerTy() ) {
			llvm::Value* asInt = this->irBuilder.CreateFPToSI(
				sourceValue, llvm::Type::getInt64Ty( this->llvmContext ), "fp.toInt"
			);
			result = this->irBuilder.CreateIntToPtr( asInt, targetType, "fp.toPtr" );
		}
		else if( sourceType->isPointerTy() && targetType->isFloatingPointTy() ) {
			llvm::Value* asInt = this->irBuilder.CreatePtrToInt(
				sourceValue, llvm::Type::getInt64Ty( this->llvmContext ), "ptr.toInt"
			);
			result = this->irBuilder.CreateSIToFP( asInt, targetType, "ptr.toFp" );
		}
		else {
			result = sourceValue;
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
		std::string calledName = instruction.calledFunctionQualifiedName;

		if( calledName == "puts" || calledName == "putsln" ||
			calledName == "putserr" || calledName == "putserrln" ) {
			llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
			llvm::Type* i32Type = llvm::Type::getInt32Ty( this->llvmContext );
			llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
			llvm::Function* strlenFunction = this->llvmModule->getFunction( "strlen" );
			if( strlenFunction == nullptr ) {
				llvm::FunctionType* strlenType = llvm::FunctionType::get( i64Type, { ptrType }, false );
				strlenFunction = llvm::Function::Create(
					strlenType, llvm::Function::ExternalLinkage, "strlen", this->llvmModule.get()
				);
			}
			llvm::Function* writeFunction = this->llvmModule->getFunction( "write" );
			if( writeFunction == nullptr ) {
				llvm::FunctionType* writeType = llvm::FunctionType::get(
					i64Type, { i32Type, ptrType, i64Type }, false
				);
				writeFunction = llvm::Function::Create(
					writeType, llvm::Function::ExternalLinkage, "write", this->llvmModule.get()
				);
			}
			llvm::FunctionCallee snprintfCallee = this->llvmModule->getOrInsertFunction( "snprintf",
				llvm::FunctionType::get( i32Type, { ptrType, i64Type, ptrType }, true )
			);
			llvm::FunctionCallee mallocCallee = this->llvmModule->getOrInsertFunction( "malloc",
				llvm::FunctionType::get( ptrType, { i64Type }, false )
			);
			int fileDescriptor = ( calledName == "putserr" || calledName == "putserrln" ) ? 2 : 1;
			llvm::Value* fdValue = llvm::ConstantInt::get( i32Type, fileDescriptor );
			for( size_t operandIndex = 0; operandIndex < instruction.sourceOperands.size(); operandIndex++ ) {
				if( operandIndex > 0 ) {
					llvm::Value* spaceStr = this->irBuilder.CreateGlobalStringPtr( " ", "sep" );
					this->irBuilder.CreateCall( writeFunction, {
						fdValue, spaceStr, llvm::ConstantInt::get( i64Type, 1 )
					} );
				}
				llvm::Value* argValue = this->loadVariableValue( instruction.sourceOperands[operandIndex] );
				if( argValue == nullptr ) {
					continue;
				}
				llvm::Value* strValue = nullptr;
				if( argValue->getType()->isPointerTy() ) {
					strValue = argValue;
				}
				else if( argValue->getType()->isIntegerTy( 1 ) ) {
					llvm::Value* trueStr = this->irBuilder.CreateGlobalStringPtr( "True", "bool.true" );
					llvm::Value* falseStr = this->irBuilder.CreateGlobalStringPtr( "False", "bool.false" );
					strValue = this->irBuilder.CreateSelect( argValue, trueStr, falseStr, "bool.str" );
				}
				else if( argValue->getType()->isIntegerTy() ) {
					llvm::Value* buf = this->irBuilder.CreateCall( mallocCallee, {
						llvm::ConstantInt::get( i64Type, 24 )
					}, "int.buf" );
					llvm::Value* val = argValue;
					if( argValue->getType() != i64Type ) {
						val = this->irBuilder.CreateSExt( argValue, i64Type, "iext" );
					}
					llvm::Value* intFmt = this->irBuilder.CreateGlobalStringPtr( "%ld", "int.fmt" );
					this->irBuilder.CreateCall( snprintfCallee, {
						buf, llvm::ConstantInt::get( i64Type, 24 ), intFmt, val
					} );
					strValue = buf;
				}
				else if( argValue->getType()->isDoubleTy() || argValue->getType()->isFloatTy() ) {
					llvm::Value* buf = this->irBuilder.CreateCall( mallocCallee, {
						llvm::ConstantInt::get( i64Type, 48 )
					}, "flt.buf" );
					llvm::Value* val = argValue;
					if( argValue->getType()->isFloatTy() ) {
						val = this->irBuilder.CreateFPExt( val, llvm::Type::getDoubleTy( this->llvmContext ), "f2d" );
					}
					llvm::Value* fltFmt = this->irBuilder.CreateGlobalStringPtr( "%f", "flt.fmt" );
					this->irBuilder.CreateCall( snprintfCallee, {
						buf, llvm::ConstantInt::get( i64Type, 48 ), fltFmt, val
					} );
					strValue = buf;
				}
				else {
					strValue = this->irBuilder.CreateGlobalStringPtr( "<unknown>", "unk.str" );
				}
				llvm::Value* strLen = this->irBuilder.CreateCall( strlenFunction, { strValue }, "slen" );
				this->irBuilder.CreateCall( writeFunction, { fdValue, strValue, strLen } );
			}
			llvm::Value* nlStr = this->irBuilder.CreateGlobalStringPtr( "\n", "nl" );
			this->irBuilder.CreateCall( writeFunction, { fdValue, nlStr, llvm::ConstantInt::get( i64Type, 1 ) } );
			if( instruction.destinationVariable != 0 ) {
				this->setVariableValue( instruction.destinationVariable,
					llvm::Constant::getNullValue( ptrType ) );
			}
			return;
		}

		// String.format intrinsic — snprintf-based {} substitution
		if( calledName == "String.format" && instruction.sourceOperands.size() >= 2 &&
			instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
			llvm::Value* formatString = this->loadVariableValue( instruction.sourceOperands[0] );
			if( formatString != nullptr && formatString->getType()->isPointerTy() ) {
				llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
				llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
				llvm::Type* i32Type = llvm::Type::getInt32Ty( this->llvmContext );
				llvm::Function* snprintfFunction = this->llvmModule->getFunction( "snprintf" );
				if( snprintfFunction == nullptr ) {
					llvm::FunctionType* snprintfType = llvm::FunctionType::get( i32Type, { ptrType, i64Type, ptrType }, true );
					snprintfFunction = llvm::Function::Create(
						snprintfType, llvm::Function::ExternalLinkage, "snprintf", this->llvmModule.get()
					);
				}
				llvm::Function* strlenFunction = this->llvmModule->getFunction( "strlen" );
				if( strlenFunction == nullptr ) {
					llvm::FunctionType* strlenType = llvm::FunctionType::get( i64Type, { ptrType }, false );
					strlenFunction = llvm::Function::Create(
						strlenType, llvm::Function::ExternalLinkage, "strlen", this->llvmModule.get()
					);
				}
				llvm::Function* mallocFunction = this->getOrCreateMalloc();
				llvm::Value* fmtLen = this->irBuilder.CreateCall( strlenFunction, { formatString }, "fmt.len" );
				llvm::Value* extraSpace = llvm::ConstantInt::get( i64Type, 64 * ( instruction.sourceOperands.size() - 1 ) );
				llvm::Value* bufSize = this->irBuilder.CreateAdd( fmtLen, extraSpace, "buf.size" );
				llvm::Value* bufPtr = this->irBuilder.CreateCall( mallocFunction, { bufSize }, "fmt.buf" );
				llvm::Value* cFormatStr = formatString;
				llvm::GlobalVariable* globalVar = llvm::dyn_cast<llvm::GlobalVariable>( formatString );
				if( globalVar != nullptr && globalVar->hasInitializer() ) {
					llvm::ConstantDataSequential* dataSeq = llvm::dyn_cast<llvm::ConstantDataSequential>( globalVar->getInitializer() );
					if( dataSeq != nullptr ) {
						std::string original = dataSeq->getAsString().str();
						if( original.empty() == false && original.back() == '\0' ) {
							original.pop_back();
						}
						std::string formatted;
						size_t argIdx = 1;
						for( size_t charIndex = 0; charIndex < original.size(); charIndex++ ) {
							if( charIndex + 1 < original.size() && original[charIndex] == '{' && original[charIndex + 1] == '}' ) {
								if( argIdx < instruction.sourceOperands.size() ) {
									llvm::Value* argVal = this->loadVariableValue( instruction.sourceOperands[argIdx] );
									if( argVal != nullptr && argVal->getType()->isFloatingPointTy() ) {
										formatted += "%f";
									}
									else {
										formatted += "%ld";
									}
									argIdx++;
								}
								charIndex++;
							}
							else {
								formatted += original[charIndex];
							}
						}
						llvm::Constant* fmtConstant = llvm::ConstantDataArray::getString( this->llvmContext, formatted, true );
						llvm::GlobalVariable* fmtGlobal = new llvm::GlobalVariable(
							*this->llvmModule, fmtConstant->getType(), true,
							llvm::GlobalValue::PrivateLinkage, fmtConstant, "fmt.str"
						);
						cFormatStr = fmtGlobal;
					}
				}
				std::vector<llvm::Value*> snprintfArgs = { bufPtr, bufSize, cFormatStr };
				for( size_t argIndex = 1; argIndex < instruction.sourceOperands.size(); argIndex++ ) {
					llvm::Value* argValue = this->loadVariableValue( instruction.sourceOperands[argIndex] );
					if( argValue != nullptr ) {
						if( argValue->getType()->isFloatingPointTy() ) {
							argValue = this->irBuilder.CreateFPExt(
								argValue, llvm::Type::getDoubleTy( this->llvmContext ), "fmt.dbl"
							);
						}
						snprintfArgs.push_back( argValue );
					}
				}
				this->irBuilder.CreateCall( snprintfFunction, snprintfArgs );
				this->setVariableValue( instruction.destinationVariable, bufPtr );
			}
			return;
		}

		// Memory<T> intrinsic method calls
		if( calledName == "Memory.get" && instruction.sourceOperands.size() >= 2 ) {
			llvm::Value* selfPointer = this->loadVariableValue( instruction.sourceOperands[0] );
			llvm::Value* indexValue = this->loadVariableValue( instruction.sourceOperands[1] );
			if( selfPointer != nullptr && indexValue != nullptr && instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
				llvm::Type* elementType = llvm::Type::getInt64Ty( this->llvmContext );
				if( this->memoryElementTypes.count( instruction.sourceOperands[0] ) > 0 ) {
					elementType = this->memoryElementTypes[instruction.sourceOperands[0]];
				}
				else if( this->currentMIRFunction != nullptr ) {
					MIRVariableIdentifier memoryVariableIdentifier = instruction.sourceOperands[0];
					auto descriptorIterator = this->currentMIRFunction->variableDescriptorTable.find( memoryVariableIdentifier );
					if( descriptorIterator != this->currentMIRFunction->variableDescriptorTable.end() &&
						descriptorIterator->second.variableType != nullptr ) {
						llvm::Type* resolved = this->resolveMemoryElementType( descriptorIterator->second.variableType );
						if( resolved != nullptr && resolved->isVoidTy() == false ) {
							elementType = resolved;
							this->memoryElementTypes[memoryVariableIdentifier] = elementType;
						}
					}
				}
				if( elementType->isIntegerTy( 64 ) && instruction.operandType != nullptr ) {
					llvm::Type* operandResolved = this->toLLVMType( instruction.operandType );
					if( operandResolved != nullptr && operandResolved->isVoidTy() == false && operandResolved->isPointerTy() == false ) {
						elementType = operandResolved;
					}
				}
				if( indexValue->getType()->isIntegerTy() == false ) {
					indexValue = this->irBuilder.CreateFPToSI(
						indexValue, llvm::Type::getInt64Ty( this->llvmContext ), "mem.idx.int"
					);
				}
				llvm::Value* gepValue = this->irBuilder.CreateGEP( elementType, selfPointer, indexValue, "mem.get.gep" );
				llvm::Value* loadedValue = this->irBuilder.CreateLoad( elementType, gepValue, "mem.get.val" );
				this->setVariableValue( instruction.destinationVariable, loadedValue );
			}
			return;
		}
		if( calledName == "Memory.set" && instruction.sourceOperands.size() >= 3 ) {
			llvm::Value* selfPointer = this->loadVariableValue( instruction.sourceOperands[0] );
			llvm::Value* indexValue = this->loadVariableValue( instruction.sourceOperands[1] );
			llvm::Value* storeValue = this->loadVariableValue( instruction.sourceOperands[2] );
			if( selfPointer != nullptr && indexValue != nullptr && storeValue != nullptr ) {
				llvm::Type* elementType = llvm::Type::getInt64Ty( this->llvmContext );
				if( this->memoryElementTypes.count( instruction.sourceOperands[0] ) > 0 ) {
					elementType = this->memoryElementTypes[instruction.sourceOperands[0]];
				}
				else if( this->currentMIRFunction != nullptr ) {
					MIRVariableIdentifier memoryVariableIdentifier = instruction.sourceOperands[0];
					auto descriptorIterator = this->currentMIRFunction->variableDescriptorTable.find( memoryVariableIdentifier );
					if( descriptorIterator != this->currentMIRFunction->variableDescriptorTable.end() &&
						descriptorIterator->second.variableType != nullptr ) {
						llvm::Type* resolved = this->resolveMemoryElementType( descriptorIterator->second.variableType );
						if( resolved != nullptr && resolved->isVoidTy() == false ) {
							elementType = resolved;
							this->memoryElementTypes[memoryVariableIdentifier] = elementType;
						}
					}
				}
				if( elementType->isIntegerTy( 64 ) && storeValue->getType()->isFloatingPointTy() ) {
					elementType = storeValue->getType();
				}
				if( indexValue->getType()->isIntegerTy() == false ) {
					indexValue = this->irBuilder.CreateFPToSI(
						indexValue, llvm::Type::getInt64Ty( this->llvmContext ), "mem.idx.int"
					);
				}
				if( storeValue->getType() != elementType ) {
					if( elementType->isIntegerTy() && storeValue->getType()->isIntegerTy() ) {
						storeValue = this->irBuilder.CreateIntCast( storeValue, elementType, true, "mem.set.cast" );
					}
					else if( elementType->isFloatingPointTy() && storeValue->getType()->isIntegerTy() ) {
						storeValue = this->irBuilder.CreateSIToFP( storeValue, elementType, "mem.set.itof" );
					}
					else if( elementType->isIntegerTy() && storeValue->getType()->isFloatingPointTy() ) {
						storeValue = this->irBuilder.CreateFPToSI( storeValue, elementType, "mem.set.ftoi" );
					}
				}
				llvm::Value* gepValue = this->irBuilder.CreateGEP( elementType, selfPointer, indexValue, "mem.set.gep" );
				this->irBuilder.CreateStore( storeValue, gepValue );
			}
			return;
		}
		if( calledName == "Memory.free" && instruction.sourceOperands.size() >= 1 ) {
			llvm::Value* selfPointer = this->loadVariableValue( instruction.sourceOperands[0] );
			if( selfPointer != nullptr ) {
				llvm::Function* freeFunction = this->getOrCreateFree();
				llvm::Value* castPointer = this->irBuilder.CreateBitCast(
					selfPointer, llvm::PointerType::getUnqual( this->llvmContext ), "mem.free.cast"
				);
				this->irBuilder.CreateCall( freeFunction, { castPointer } );
			}
			return;
		}
		if( calledName == "Memory.copyTo" && instruction.sourceOperands.size() >= 3 ) {
			llvm::Value* selfPointer = this->loadVariableValue( instruction.sourceOperands[0] );
			llvm::Value* destPointer = this->loadVariableValue( instruction.sourceOperands[1] );
			llvm::Value* lengthValue = this->loadVariableValue( instruction.sourceOperands[2] );
			if( selfPointer != nullptr && destPointer != nullptr && lengthValue != nullptr ) {
				llvm::Type* elementType = llvm::Type::getInt64Ty( this->llvmContext );
				if( this->memoryElementTypes.count( instruction.sourceOperands[0] ) > 0 ) {
					elementType = this->memoryElementTypes[instruction.sourceOperands[0]];
				}
				else if( this->currentMIRFunction != nullptr ) {
					MIRVariableIdentifier memoryVariableIdentifier = instruction.sourceOperands[0];
					auto descriptorIterator = this->currentMIRFunction->variableDescriptorTable.find( memoryVariableIdentifier );
					if( descriptorIterator != this->currentMIRFunction->variableDescriptorTable.end() &&
						descriptorIterator->second.variableType != nullptr ) {
						llvm::Type* resolved = this->resolveMemoryElementType( descriptorIterator->second.variableType );
						if( resolved != nullptr && resolved->isVoidTy() == false ) {
							elementType = resolved;
							this->memoryElementTypes[memoryVariableIdentifier] = elementType;
						}
					}
				}
				llvm::DataLayout dataLayout( this->llvmModule.get() );
				uint64_t elementSize = dataLayout.getTypeAllocSize( elementType );
				llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
				llvm::Value* totalBytes = this->irBuilder.CreateMul(
					lengthValue, llvm::ConstantInt::get( i64Type, elementSize ), "copy.bytes"
				);
				llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
				llvm::Value* srcI8 = this->irBuilder.CreateBitCast( selfPointer, ptrType, "copy.src" );
				llvm::Value* dstI8 = this->irBuilder.CreateBitCast( destPointer, ptrType, "copy.dst" );
				llvm::Function* memcpyFunction = llvm::Intrinsic::getDeclaration(
					this->llvmModule.get(), llvm::Intrinsic::memcpy,
					{ ptrType, ptrType, i64Type }
				);
				this->irBuilder.CreateCall( memcpyFunction, { dstI8, srcI8, totalBytes, this->irBuilder.getInt1( false ) } );
			}
			return;
		}
		if( calledName == "Memory.Memory" ) {
			return;
		}

		// Arena<T> intrinsic method calls
		if( calledName == "Arena.alloc" && instruction.sourceOperands.size() >= 1 ) {
			llvm::Value* arenaPointer = this->loadVariableValue( instruction.sourceOperands[0] );
			if( arenaPointer != nullptr && instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
				llvm::Type* elementType = llvm::Type::getInt64Ty( this->llvmContext );
				if( this->memoryElementTypes.count( instruction.sourceOperands[0] ) > 0 ) {
					elementType = this->memoryElementTypes[instruction.sourceOperands[0]];
				}
				else if( this->currentMIRFunction != nullptr ) {
					MIRVariableIdentifier arenaVariableIdentifier = instruction.sourceOperands[0];
					auto descriptorIterator = this->currentMIRFunction->variableDescriptorTable.find( arenaVariableIdentifier );
					if( descriptorIterator != this->currentMIRFunction->variableDescriptorTable.end() &&
						descriptorIterator->second.variableType != nullptr ) {
						llvm::Type* resolved = this->resolveMemoryElementType( descriptorIterator->second.variableType );
						if( resolved != nullptr && resolved->isVoidTy() == false ) {
							elementType = resolved;
							this->memoryElementTypes[arenaVariableIdentifier] = elementType;
						}
					}
				}
				llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
				llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
				llvm::StructType* arenaStructType = llvm::StructType::get(
					this->llvmContext, { ptrType, i64Type, i64Type }
				);
				llvm::Value* baseGep = this->irBuilder.CreateStructGEP( arenaStructType, arenaPointer, 0, "arena.base.ptr" );
				llvm::Value* basePointer = this->irBuilder.CreateLoad( ptrType, baseGep, "arena.base" );
				llvm::Value* countGep = this->irBuilder.CreateStructGEP( arenaStructType, arenaPointer, 1, "arena.count.ptr" );
				llvm::Value* currentCount = this->irBuilder.CreateLoad( i64Type, countGep, "arena.count" );
				llvm::Value* slotGep = this->irBuilder.CreateGEP( elementType, basePointer, currentCount, "arena.slot" );
				llvm::Value* incrementedCount = this->irBuilder.CreateAdd(
					currentCount, llvm::ConstantInt::get( i64Type, 1 ), "arena.count.inc"
				);
				this->irBuilder.CreateStore( incrementedCount, countGep );
				this->setVariableValue( instruction.destinationVariable, slotGep );
			}
			return;
		}
		if( calledName == "Arena.freeAll" && instruction.sourceOperands.size() >= 1 ) {
			llvm::Value* arenaPointer = this->loadVariableValue( instruction.sourceOperands[0] );
			if( arenaPointer != nullptr ) {
				llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
				llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
				llvm::StructType* arenaStructType = llvm::StructType::get(
					this->llvmContext, { ptrType, i64Type, i64Type }
				);
				llvm::Value* countGep = this->irBuilder.CreateStructGEP( arenaStructType, arenaPointer, 1, "arena.count.ptr" );
				this->irBuilder.CreateStore( llvm::ConstantInt::get( i64Type, 0 ), countGep );
			}
			return;
		}
		if( calledName == "Arena.destroy" && instruction.sourceOperands.size() >= 1 ) {
			llvm::Value* arenaPointer = this->loadVariableValue( instruction.sourceOperands[0] );
			if( arenaPointer != nullptr ) {
				llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
				llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
				llvm::StructType* arenaStructType = llvm::StructType::get(
					this->llvmContext, { ptrType, i64Type, i64Type }
				);
				llvm::Value* baseGep = this->irBuilder.CreateStructGEP( arenaStructType, arenaPointer, 0, "arena.base.ptr" );
				llvm::Value* basePointer = this->irBuilder.CreateLoad( ptrType, baseGep, "arena.base" );
				this->irBuilder.CreateCall( this->getOrCreateFree(), { basePointer } );
			}
			return;
		}
		if( calledName == "Arena.count" && instruction.sourceOperands.size() >= 1 ) {
			llvm::Value* arenaPointer = this->loadVariableValue( instruction.sourceOperands[0] );
			if( arenaPointer != nullptr && instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
				llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
				llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
				llvm::StructType* arenaStructType = llvm::StructType::get(
					this->llvmContext, { ptrType, i64Type, i64Type }
				);
				llvm::Value* countGep = this->irBuilder.CreateStructGEP( arenaStructType, arenaPointer, 1, "arena.count.ptr" );
				llvm::Value* countValue = this->irBuilder.CreateLoad( i64Type, countGep, "arena.count" );
				this->setVariableValue( instruction.destinationVariable, countValue );
			}
			return;
		}
		if( calledName == "Arena.Arena" ) {
			return;
		}

		// OOP wrapper type method intrinsics
		{
			static const std::unordered_map<std::string, int> integerWrapperBitWidths = {
				{"Int", 64}, {"I64", 64}, {"Long", 64}, {"Integer", 64}, {"UInt", 64}, {"U64", 64},
				{"I32", 32}, {"U32", 32}, {"I16", 16}, {"U16", 16},
				{"I8", 8}, {"U8", 8}, {"Byte", 8}, {"Char", 32}
			};
			static const std::unordered_set<std::string> floatWrapperNames = {
				"Float", "Double", "F64", "F32"
			};
			static const std::unordered_set<std::string> unsignedWrapperNames = {
				"UInt", "U64", "U32", "U16", "U8", "Byte"
			};

			size_t dotPosition = calledName.find( '.' );
			if( dotPosition != std::string::npos ) {
				std::string wrapperName = calledName.substr( 0, dotPosition );
				std::string methodName = calledName.substr( dotPosition + 1 );

				bool isIntegerWrapper = integerWrapperBitWidths.count( wrapperName ) > 0;
				bool isFloatWrapper = floatWrapperNames.count( wrapperName ) > 0;
				bool isUnsigned = unsignedWrapperNames.count( wrapperName ) > 0;

				if( wrapperName == "Boolean" && dotPosition != std::string::npos ) {
					llvm::Type* i1Type = llvm::Type::getInt1Ty( this->llvmContext );
					auto loadBoolSelf = [&]() -> llvm::Value* {
						if( instruction.sourceOperands.empty() ) return nullptr;
						llvm::Value* selfValue = this->loadVariableValue( instruction.sourceOperands[0] );
						if( selfValue == nullptr ) return nullptr;
						if( selfValue->getType() != i1Type ) {
							if( selfValue->getType()->isIntegerTy() ) {
								selfValue = this->irBuilder.CreateTrunc( selfValue, i1Type, "bool.trunc" );
							}
						}
						return selfValue;
					};
					auto loadBoolArg = [&]() -> llvm::Value* {
						if( instruction.sourceOperands.size() < 2 ) return nullptr;
						llvm::Value* argValue = this->loadVariableValue( instruction.sourceOperands[1] );
						if( argValue == nullptr ) return nullptr;
						if( argValue->getType() != i1Type ) {
							if( argValue->getType()->isIntegerTy() ) {
								argValue = this->irBuilder.CreateTrunc( argValue, i1Type, "bool.arg.trunc" );
							}
						}
						return argValue;
					};
					auto setBoolResult = [&]( llvm::Value* resultValue ) {
						if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER && resultValue != nullptr ) {
							this->setVariableValue( instruction.destinationVariable, resultValue );
						}
					};
					if( methodName == "getValue" || methodName == "value" ) {
						setBoolResult( loadBoolSelf() );
						return;
					}
					if( methodName == "negate" ) {
						llvm::Value* selfValue = loadBoolSelf();
						if( selfValue != nullptr ) {
							setBoolResult( this->irBuilder.CreateXor( selfValue, llvm::ConstantInt::getTrue( this->llvmContext ), "bool.neg" ) );
						}
						return;
					}
					if( methodName == "logicalAnd" ) {
						llvm::Value* selfValue = loadBoolSelf();
						llvm::Value* argValue = loadBoolArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							setBoolResult( this->irBuilder.CreateAnd( selfValue, argValue, "bool.and" ) );
						}
						return;
					}
					if( methodName == "logicalOr" ) {
						llvm::Value* selfValue = loadBoolSelf();
						llvm::Value* argValue = loadBoolArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							setBoolResult( this->irBuilder.CreateOr( selfValue, argValue, "bool.or" ) );
						}
						return;
					}
					if( methodName == "logicalXor" ) {
						llvm::Value* selfValue = loadBoolSelf();
						llvm::Value* argValue = loadBoolArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							setBoolResult( this->irBuilder.CreateXor( selfValue, argValue, "bool.xor" ) );
						}
						return;
					}
					if( methodName == "equals" ) {
						llvm::Value* selfValue = loadBoolSelf();
						llvm::Value* argValue = loadBoolArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							setBoolResult( this->irBuilder.CreateICmpEQ( selfValue, argValue, "bool.eq" ) );
						}
						return;
					}
					if( methodName == "toString" ) {
						llvm::Value* selfValue = loadBoolSelf();
						if( selfValue != nullptr ) {
							llvm::Value* trueStr = this->irBuilder.CreateGlobalStringPtr( "True", "bool.true" );
							llvm::Value* falseStr = this->irBuilder.CreateGlobalStringPtr( "False", "bool.false" );
							setBoolResult( this->irBuilder.CreateSelect( selfValue, trueStr, falseStr, "bool.str" ) );
						}
						return;
					}
					if( methodName == "Boolean" ) {
						return;
					}
				}
				if( wrapperName == "Char" && dotPosition != std::string::npos ) {
					llvm::Type* i32Type = llvm::Type::getInt32Ty( this->llvmContext );
					auto loadCharSelf = [&]() -> llvm::Value* {
						if( instruction.sourceOperands.empty() ) return nullptr;
						llvm::Value* selfValue = this->loadVariableValue( instruction.sourceOperands[0] );
						if( selfValue == nullptr ) return nullptr;
						if( selfValue->getType() != i32Type ) {
							if( selfValue->getType()->isPointerTy() ) {
								selfValue = this->irBuilder.CreatePtrToInt( selfValue, i32Type, "char.ptoi" );
							}
							else if( selfValue->getType()->isIntegerTy() ) {
								selfValue = this->irBuilder.CreateIntCast( selfValue, i32Type, true, "char.cast" );
							}
						}
						return selfValue;
					};
					auto setCharResult = [&]( llvm::Value* resultValue ) {
						if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER && resultValue != nullptr ) {
							this->setVariableValue( instruction.destinationVariable, resultValue );
						}
					};
					if( methodName == "getValue" || methodName == "value" ) {
						setCharResult( loadCharSelf() );
						return;
					}
					if( methodName == "isAlpha" ) {
						llvm::Value* selfValue = loadCharSelf();
						if( selfValue != nullptr ) {
							llvm::Value* geA = this->irBuilder.CreateICmpSGE( selfValue, llvm::ConstantInt::get( i32Type, 'A' ), "char.geA" );
							llvm::Value* leZ = this->irBuilder.CreateICmpSLE( selfValue, llvm::ConstantInt::get( i32Type, 'Z' ), "char.leZ" );
							llvm::Value* isUpper = this->irBuilder.CreateAnd( geA, leZ, "char.isUpper" );
							llvm::Value* gea = this->irBuilder.CreateICmpSGE( selfValue, llvm::ConstantInt::get( i32Type, 'a' ), "char.gea" );
							llvm::Value* lez = this->irBuilder.CreateICmpSLE( selfValue, llvm::ConstantInt::get( i32Type, 'z' ), "char.lez" );
							llvm::Value* isLower = this->irBuilder.CreateAnd( gea, lez, "char.isLower" );
							setCharResult( this->irBuilder.CreateOr( isUpper, isLower, "char.isAlpha" ) );
						}
						return;
					}
					if( methodName == "isDigit" ) {
						llvm::Value* selfValue = loadCharSelf();
						if( selfValue != nullptr ) {
							llvm::Value* ge0 = this->irBuilder.CreateICmpSGE( selfValue, llvm::ConstantInt::get( i32Type, '0' ), "char.ge0" );
							llvm::Value* le9 = this->irBuilder.CreateICmpSLE( selfValue, llvm::ConstantInt::get( i32Type, '9' ), "char.le9" );
							setCharResult( this->irBuilder.CreateAnd( ge0, le9, "char.isDigit" ) );
						}
						return;
					}
					if( methodName == "isAlphanumeric" ) {
						llvm::Value* selfValue = loadCharSelf();
						if( selfValue != nullptr ) {
							llvm::Value* geA = this->irBuilder.CreateICmpSGE( selfValue, llvm::ConstantInt::get( i32Type, 'A' ), "char.geA" );
							llvm::Value* leZ = this->irBuilder.CreateICmpSLE( selfValue, llvm::ConstantInt::get( i32Type, 'Z' ), "char.leZ" );
							llvm::Value* isUpper = this->irBuilder.CreateAnd( geA, leZ, "char.isUpper" );
							llvm::Value* gea = this->irBuilder.CreateICmpSGE( selfValue, llvm::ConstantInt::get( i32Type, 'a' ), "char.gea" );
							llvm::Value* lez = this->irBuilder.CreateICmpSLE( selfValue, llvm::ConstantInt::get( i32Type, 'z' ), "char.lez" );
							llvm::Value* isLower = this->irBuilder.CreateAnd( gea, lez, "char.isLower" );
							llvm::Value* isAlpha = this->irBuilder.CreateOr( isUpper, isLower, "char.isAlpha" );
							llvm::Value* ge0 = this->irBuilder.CreateICmpSGE( selfValue, llvm::ConstantInt::get( i32Type, '0' ), "char.ge0" );
							llvm::Value* le9 = this->irBuilder.CreateICmpSLE( selfValue, llvm::ConstantInt::get( i32Type, '9' ), "char.le9" );
							llvm::Value* isDigit = this->irBuilder.CreateAnd( ge0, le9, "char.isDigit" );
							setCharResult( this->irBuilder.CreateOr( isAlpha, isDigit, "char.isAlnum" ) );
						}
						return;
					}
					if( methodName == "isWhitespace" ) {
						llvm::Value* selfValue = loadCharSelf();
						if( selfValue != nullptr ) {
							llvm::Value* isSpace = this->irBuilder.CreateICmpEQ( selfValue, llvm::ConstantInt::get( i32Type, ' ' ), "char.isSpace" );
							llvm::Value* isTab = this->irBuilder.CreateICmpEQ( selfValue, llvm::ConstantInt::get( i32Type, '\t' ), "char.isTab" );
							llvm::Value* isNewline = this->irBuilder.CreateICmpEQ( selfValue, llvm::ConstantInt::get( i32Type, '\n' ), "char.isNl" );
							llvm::Value* isReturn = this->irBuilder.CreateICmpEQ( selfValue, llvm::ConstantInt::get( i32Type, '\r' ), "char.isCr" );
							llvm::Value* result = this->irBuilder.CreateOr( isSpace, isTab, "char.ws1" );
							result = this->irBuilder.CreateOr( result, isNewline, "char.ws2" );
							setCharResult( this->irBuilder.CreateOr( result, isReturn, "char.isWs" ) );
						}
						return;
					}
					if( methodName == "toUpper" ) {
						llvm::Value* selfValue = loadCharSelf();
						if( selfValue != nullptr ) {
							llvm::Value* gea = this->irBuilder.CreateICmpSGE( selfValue, llvm::ConstantInt::get( i32Type, 'a' ), "char.gea" );
							llvm::Value* lez = this->irBuilder.CreateICmpSLE( selfValue, llvm::ConstantInt::get( i32Type, 'z' ), "char.lez" );
							llvm::Value* isLower = this->irBuilder.CreateAnd( gea, lez, "char.isLower" );
							llvm::Value* upper = this->irBuilder.CreateSub( selfValue, llvm::ConstantInt::get( i32Type, 32 ), "char.upper" );
							setCharResult( this->irBuilder.CreateSelect( isLower, upper, selfValue, "char.toUpper" ) );
						}
						return;
					}
					if( methodName == "toLower" ) {
						llvm::Value* selfValue = loadCharSelf();
						if( selfValue != nullptr ) {
							llvm::Value* geA = this->irBuilder.CreateICmpSGE( selfValue, llvm::ConstantInt::get( i32Type, 'A' ), "char.geA" );
							llvm::Value* leZ = this->irBuilder.CreateICmpSLE( selfValue, llvm::ConstantInt::get( i32Type, 'Z' ), "char.leZ" );
							llvm::Value* isUpper = this->irBuilder.CreateAnd( geA, leZ, "char.isUpper" );
							llvm::Value* lower = this->irBuilder.CreateAdd( selfValue, llvm::ConstantInt::get( i32Type, 32 ), "char.lower" );
							setCharResult( this->irBuilder.CreateSelect( isUpper, lower, selfValue, "char.toLower" ) );
						}
						return;
					}
					if( methodName == "toString" ) {
						llvm::Value* selfValue = loadCharSelf();
						if( selfValue != nullptr ) {
							llvm::Function* snprintfFunction = this->llvmModule->getFunction( "snprintf" );
							if( snprintfFunction == nullptr ) {
								llvm::FunctionType* snprintfType = llvm::FunctionType::get(
									llvm::Type::getInt32Ty( this->llvmContext ),
									{ llvm::PointerType::getUnqual( this->llvmContext ), llvm::Type::getInt64Ty( this->llvmContext ), llvm::PointerType::getUnqual( this->llvmContext ) },
									true
								);
								snprintfFunction = llvm::Function::Create( snprintfType, llvm::Function::ExternalLinkage, "snprintf", this->llvmModule.get() );
							}
							llvm::Value* buffer = this->irBuilder.CreateAlloca( llvm::Type::getInt8Ty( this->llvmContext ), llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->llvmContext ), 8 ), "char.buf" );
							llvm::Value* fmtStr = this->irBuilder.CreateGlobalStringPtr( "%c", "char.fmt" );
							this->irBuilder.CreateCall( snprintfFunction, { buffer, llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->llvmContext ), 8 ), fmtStr, selfValue } );
							setCharResult( buffer );
						}
						return;
					}
					if( methodName == "Char" ) {
						return;
					}
				}
				if( isIntegerWrapper || isFloatWrapper ) {
					llvm::Type* primitiveType = nullptr;
					if( isIntegerWrapper ) {
						primitiveType = llvm::Type::getInt64Ty( this->llvmContext );
					}
					else if( wrapperName == "F32" ) {
						primitiveType = llvm::Type::getFloatTy( this->llvmContext );
					}
					else {
						primitiveType = llvm::Type::getDoubleTy( this->llvmContext );
					}

					auto loadSelf = [&]() -> llvm::Value* {
						if( instruction.sourceOperands.empty() ) return nullptr;
						llvm::Value* selfValue = this->loadVariableValue( instruction.sourceOperands[0] );
						if( selfValue == nullptr ) return nullptr;
						if( selfValue->getType() != primitiveType ) {
							if( primitiveType->isIntegerTy() && selfValue->getType()->isIntegerTy() ) {
								selfValue = this->irBuilder.CreateIntCast( selfValue, primitiveType, !isUnsigned, "wrap.self" );
							}
							else if( primitiveType->isFloatingPointTy() && selfValue->getType()->isIntegerTy() ) {
								selfValue = this->irBuilder.CreateSIToFP( selfValue, primitiveType, "wrap.self.itof" );
							}
							else if( primitiveType->isIntegerTy() && selfValue->getType()->isFloatingPointTy() ) {
								selfValue = this->irBuilder.CreateFPToSI( selfValue, primitiveType, "wrap.self.ftoi" );
							}
							else if( primitiveType->isFloatingPointTy() && selfValue->getType()->isFloatingPointTy() ) {
								selfValue = this->irBuilder.CreateFPCast( selfValue, primitiveType, "wrap.self.fcast" );
							}
							else if( selfValue->getType()->isPointerTy() && primitiveType->isIntegerTy() ) {
								selfValue = this->irBuilder.CreatePtrToInt( selfValue, primitiveType, "wrap.self.ptoi" );
							}
						}
						return selfValue;
					};
					auto loadArg = [&]() -> llvm::Value* {
						if( instruction.sourceOperands.size() < 2 ) return nullptr;
						llvm::Value* argValue = this->loadVariableValue( instruction.sourceOperands[1] );
						if( argValue == nullptr ) return nullptr;
						if( argValue->getType() != primitiveType ) {
							if( primitiveType->isIntegerTy() && argValue->getType()->isIntegerTy() ) {
								argValue = this->irBuilder.CreateIntCast( argValue, primitiveType, !isUnsigned, "wrap.arg" );
							}
							else if( primitiveType->isFloatingPointTy() && argValue->getType()->isIntegerTy() ) {
								argValue = this->irBuilder.CreateSIToFP( argValue, primitiveType, "wrap.arg.itof" );
							}
							else if( primitiveType->isIntegerTy() && argValue->getType()->isFloatingPointTy() ) {
								argValue = this->irBuilder.CreateFPToSI( argValue, primitiveType, "wrap.arg.ftoi" );
							}
							else if( primitiveType->isFloatingPointTy() && argValue->getType()->isFloatingPointTy() ) {
								argValue = this->irBuilder.CreateFPCast( argValue, primitiveType, "wrap.arg.fcast" );
							}
							else if( argValue->getType()->isPointerTy() && primitiveType->isIntegerTy() ) {
								argValue = this->irBuilder.CreatePtrToInt( argValue, primitiveType, "wrap.arg.ptoi" );
							}
						}
						return argValue;
					};
					auto setResult = [&]( llvm::Value* resultValue ) {
						if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER && resultValue != nullptr ) {
							this->setVariableValue( instruction.destinationVariable, resultValue );
						}
					};

					if( methodName == "getValue" || methodName == "value" ) {
						setResult( loadSelf() );
						return;
					}
					if( methodName == "add" ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							setResult( isFloatWrapper
								? this->irBuilder.CreateFAdd( selfValue, argValue, "wrap.add" )
								: this->irBuilder.CreateAdd( selfValue, argValue, "wrap.add" ) );
						}
						return;
					}
					if( methodName == "subtract" || methodName == "sub" ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							setResult( isFloatWrapper
								? this->irBuilder.CreateFSub( selfValue, argValue, "wrap.sub" )
								: this->irBuilder.CreateSub( selfValue, argValue, "wrap.sub" ) );
						}
						return;
					}
					if( methodName == "multiply" || methodName == "mul" ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							setResult( isFloatWrapper
								? this->irBuilder.CreateFMul( selfValue, argValue, "wrap.mul" )
								: this->irBuilder.CreateMul( selfValue, argValue, "wrap.mul" ) );
						}
						return;
					}
					if( methodName == "divide" || methodName == "div" ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							if( isFloatWrapper ) {
								setResult( this->irBuilder.CreateFDiv( selfValue, argValue, "wrap.div" ) );
							}
							else if( isUnsigned ) {
								setResult( this->irBuilder.CreateUDiv( selfValue, argValue, "wrap.div" ) );
							}
							else {
								setResult( this->irBuilder.CreateSDiv( selfValue, argValue, "wrap.div" ) );
							}
						}
						return;
					}
					if( methodName == "modulo" || methodName == "mod" || methodName == "remainder" ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							if( isFloatWrapper ) {
								setResult( this->irBuilder.CreateFRem( selfValue, argValue, "wrap.rem" ) );
							}
							else if( isUnsigned ) {
								setResult( this->irBuilder.CreateURem( selfValue, argValue, "wrap.rem" ) );
							}
							else {
								setResult( this->irBuilder.CreateSRem( selfValue, argValue, "wrap.rem" ) );
							}
						}
						return;
					}
					if( methodName == "negate" || methodName == "neg" ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							if( isFloatWrapper ) {
								setResult( this->irBuilder.CreateFNeg( selfValue, "wrap.neg" ) );
							}
							else {
								setResult( this->irBuilder.CreateNeg( selfValue, "wrap.neg" ) );
							}
						}
						return;
					}
					if( methodName == "abs" ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							if( isFloatWrapper ) {
								llvm::Value* negValue = this->irBuilder.CreateFNeg( selfValue, "wrap.abs.neg" );
								llvm::Value* isNeg = this->irBuilder.CreateFCmpOLT( selfValue,
									llvm::ConstantFP::get( primitiveType, 0.0 ), "wrap.abs.cmp" );
								setResult( this->irBuilder.CreateSelect( isNeg, negValue, selfValue, "wrap.abs" ) );
							}
							else {
								llvm::Value* negValue = this->irBuilder.CreateNeg( selfValue, "wrap.abs.neg" );
								llvm::Value* isNeg = this->irBuilder.CreateICmpSLT( selfValue,
									llvm::ConstantInt::get( primitiveType, 0, true ), "wrap.abs.cmp" );
								setResult( this->irBuilder.CreateSelect( isNeg, negValue, selfValue, "wrap.abs" ) );
							}
						}
						return;
					}
					if( methodName == "equals" ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							llvm::Value* result = isFloatWrapper
								? this->irBuilder.CreateFCmpOEQ( selfValue, argValue, "wrap.eq" )
								: this->irBuilder.CreateICmpEQ( selfValue, argValue, "wrap.eq" );
							setResult( result );
						}
						return;
					}
					if( methodName == "compareTo" ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
							if( isFloatWrapper ) {
								llvm::Value* isLt = this->irBuilder.CreateFCmpOLT( selfValue, argValue, "wrap.cmp.lt" );
								llvm::Value* isGt = this->irBuilder.CreateFCmpOGT( selfValue, argValue, "wrap.cmp.gt" );
								llvm::Value* ltExt = this->irBuilder.CreateZExt( isLt, i64Type );
								llvm::Value* gtExt = this->irBuilder.CreateZExt( isGt, i64Type );
								setResult( this->irBuilder.CreateSub( gtExt, ltExt, "wrap.cmp" ) );
							}
							else {
								llvm::Value* isLt = isUnsigned
									? this->irBuilder.CreateICmpULT( selfValue, argValue, "wrap.cmp.lt" )
									: this->irBuilder.CreateICmpSLT( selfValue, argValue, "wrap.cmp.lt" );
								llvm::Value* isGt = isUnsigned
									? this->irBuilder.CreateICmpUGT( selfValue, argValue, "wrap.cmp.gt" )
									: this->irBuilder.CreateICmpSGT( selfValue, argValue, "wrap.cmp.gt" );
								llvm::Value* ltExt = this->irBuilder.CreateZExt( isLt, i64Type );
								llvm::Value* gtExt = this->irBuilder.CreateZExt( isGt, i64Type );
								setResult( this->irBuilder.CreateSub( gtExt, ltExt, "wrap.cmp" ) );
							}
						}
						return;
					}
					if( methodName == "greaterThan" || methodName == "gt" ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							llvm::Value* result = isFloatWrapper
								? this->irBuilder.CreateFCmpOGT( selfValue, argValue, "wrap.gt" )
								: ( isUnsigned
									? this->irBuilder.CreateICmpUGT( selfValue, argValue, "wrap.gt" )
									: this->irBuilder.CreateICmpSGT( selfValue, argValue, "wrap.gt" ) );
							setResult( result );
						}
						return;
					}
					if( methodName == "lessThan" || methodName == "lt" ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							llvm::Value* result = isFloatWrapper
								? this->irBuilder.CreateFCmpOLT( selfValue, argValue, "wrap.lt" )
								: ( isUnsigned
									? this->irBuilder.CreateICmpULT( selfValue, argValue, "wrap.lt" )
									: this->irBuilder.CreateICmpSLT( selfValue, argValue, "wrap.lt" ) );
							setResult( result );
						}
						return;
					}
					if( methodName == "greaterThanOrEqual" || methodName == "gte" ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							llvm::Value* result = isFloatWrapper
								? this->irBuilder.CreateFCmpOGE( selfValue, argValue, "wrap.gte" )
								: ( isUnsigned
									? this->irBuilder.CreateICmpUGE( selfValue, argValue, "wrap.gte" )
									: this->irBuilder.CreateICmpSGE( selfValue, argValue, "wrap.gte" ) );
							setResult( result );
						}
						return;
					}
					if( methodName == "lessThanOrEqual" || methodName == "lte" ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							llvm::Value* result = isFloatWrapper
								? this->irBuilder.CreateFCmpOLE( selfValue, argValue, "wrap.lte" )
								: ( isUnsigned
									? this->irBuilder.CreateICmpULE( selfValue, argValue, "wrap.lte" )
									: this->irBuilder.CreateICmpSLE( selfValue, argValue, "wrap.lte" ) );
							setResult( result );
						}
						return;
					}
					if( methodName == "min" ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							llvm::Value* cond = isFloatWrapper
								? this->irBuilder.CreateFCmpOLT( selfValue, argValue, "wrap.min.cmp" )
								: ( isUnsigned
									? this->irBuilder.CreateICmpULT( selfValue, argValue, "wrap.min.cmp" )
									: this->irBuilder.CreateICmpSLT( selfValue, argValue, "wrap.min.cmp" ) );
							setResult( this->irBuilder.CreateSelect( cond, selfValue, argValue, "wrap.min" ) );
						}
						return;
					}
					if( methodName == "max" ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							llvm::Value* cond = isFloatWrapper
								? this->irBuilder.CreateFCmpOGT( selfValue, argValue, "wrap.max.cmp" )
								: ( isUnsigned
									? this->irBuilder.CreateICmpUGT( selfValue, argValue, "wrap.max.cmp" )
									: this->irBuilder.CreateICmpSGT( selfValue, argValue, "wrap.max.cmp" ) );
							setResult( this->irBuilder.CreateSelect( cond, selfValue, argValue, "wrap.max" ) );
						}
						return;
					}
					if( methodName == "bitwiseAnd" && isIntegerWrapper ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							setResult( this->irBuilder.CreateAnd( selfValue, argValue, "wrap.and" ) );
						}
						return;
					}
					if( methodName == "bitwiseOr" && isIntegerWrapper ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							setResult( this->irBuilder.CreateOr( selfValue, argValue, "wrap.or" ) );
						}
						return;
					}
					if( methodName == "bitwiseXor" && isIntegerWrapper ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							setResult( this->irBuilder.CreateXor( selfValue, argValue, "wrap.xor" ) );
						}
						return;
					}
					if( methodName == "bitwiseNot" && isIntegerWrapper ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							setResult( this->irBuilder.CreateNot( selfValue, "wrap.not" ) );
						}
						return;
					}
					if( methodName == "shiftLeft" && isIntegerWrapper ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							setResult( this->irBuilder.CreateShl( selfValue, argValue, "wrap.shl" ) );
						}
						return;
					}
					if( methodName == "shiftRight" && isIntegerWrapper ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							setResult( isUnsigned
								? this->irBuilder.CreateLShr( selfValue, argValue, "wrap.shr" )
								: this->irBuilder.CreateAShr( selfValue, argValue, "wrap.shr" ) );
						}
						return;
					}
					if( methodName == "toString" ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
							llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
							llvm::Type* i32Type = llvm::Type::getInt32Ty( this->llvmContext );
							llvm::Function* snprintfFunction = this->llvmModule->getFunction( "snprintf" );
							if( snprintfFunction == nullptr ) {
								llvm::FunctionType* snprintfType = llvm::FunctionType::get( i32Type, { ptrType, i64Type, ptrType }, true );
								snprintfFunction = llvm::Function::Create( snprintfType, llvm::Function::ExternalLinkage, "snprintf", this->llvmModule.get() );
							}
							llvm::Function* mallocFunction = this->getOrCreateMalloc();
							llvm::Value* bufSize = llvm::ConstantInt::get( i64Type, 48 );
							llvm::Value* bufPtr = this->irBuilder.CreateCall( mallocFunction, { bufSize }, "wrap.str.buf" );
							if( isFloatWrapper ) {
								llvm::Value* fmtStr = this->irBuilder.CreateGlobalStringPtr( "%g", "wrap.str.fmt" );
								if( selfValue->getType()->isFloatTy() ) {
									selfValue = this->irBuilder.CreateFPExt( selfValue, llvm::Type::getDoubleTy( this->llvmContext ), "wrap.str.ext" );
								}
								this->irBuilder.CreateCall( snprintfFunction, { bufPtr, bufSize, fmtStr, selfValue } );
							}
							else {
								llvm::Value* printVal = selfValue;
								if( selfValue->getType() != i64Type ) {
									printVal = isUnsigned
										? this->irBuilder.CreateZExt( selfValue, i64Type, "wrap.str.zext" )
										: this->irBuilder.CreateSExt( selfValue, i64Type, "wrap.str.sext" );
								}
								llvm::Value* fmtStr = this->irBuilder.CreateGlobalStringPtr( "%ld", "wrap.str.fmt" );
								this->irBuilder.CreateCall( snprintfFunction, { bufPtr, bufSize, fmtStr, printVal } );
							}
							setResult( bufPtr );
						}
						return;
					}
					if( methodName == "toI64" || methodName == "toInt" || methodName == "toLong" ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
							if( isFloatWrapper ) {
								setResult( this->irBuilder.CreateFPToSI( selfValue, i64Type, "wrap.toi64" ) );
							}
							else {
								setResult( this->irBuilder.CreateIntCast( selfValue, i64Type, !isUnsigned, "wrap.toi64" ) );
							}
						}
						return;
					}
					if( methodName == "toI32" ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							llvm::Type* i32Type = llvm::Type::getInt32Ty( this->llvmContext );
							if( isFloatWrapper ) {
								setResult( this->irBuilder.CreateFPToSI( selfValue, i32Type, "wrap.toi32" ) );
							}
							else {
								setResult( this->irBuilder.CreateIntCast( selfValue, i32Type, !isUnsigned, "wrap.toi32" ) );
							}
						}
						return;
					}
					if( methodName == "toFloat" || methodName == "toDouble" || methodName == "toF64" ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							llvm::Type* doubleType = llvm::Type::getDoubleTy( this->llvmContext );
							if( isFloatWrapper ) {
								setResult( this->irBuilder.CreateFPCast( selfValue, doubleType, "wrap.tof64" ) );
							}
							else if( isUnsigned ) {
								setResult( this->irBuilder.CreateUIToFP( selfValue, doubleType, "wrap.tof64" ) );
							}
							else {
								setResult( this->irBuilder.CreateSIToFP( selfValue, doubleType, "wrap.tof64" ) );
							}
						}
						return;
					}
					if( methodName == "isInfinite" && isFloatWrapper ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							llvm::Value* absVal = this->irBuilder.CreateUnaryIntrinsic( llvm::Intrinsic::fabs, selfValue, nullptr, "wrap.fabs" );
							llvm::Value* inf = llvm::ConstantFP::getInfinity( primitiveType );
							setResult( this->irBuilder.CreateFCmpOEQ( absVal, inf, "wrap.isinf" ) );
						}
						return;
					}
					if( methodName == "isNaN" && isFloatWrapper ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							setResult( this->irBuilder.CreateFCmpUNO( selfValue, selfValue, "wrap.isnan" ) );
						}
						return;
					}
					if( methodName == "isFinite" && isFloatWrapper ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							llvm::Value* absVal = this->irBuilder.CreateUnaryIntrinsic( llvm::Intrinsic::fabs, selfValue, nullptr, "wrap.fabs" );
							llvm::Value* inf = llvm::ConstantFP::getInfinity( primitiveType );
							llvm::Value* isInf = this->irBuilder.CreateFCmpOEQ( absVal, inf, "wrap.isinf" );
							llvm::Value* isNan = this->irBuilder.CreateFCmpUNO( selfValue, selfValue, "wrap.isnan" );
							llvm::Value* notFinite = this->irBuilder.CreateOr( isInf, isNan, "wrap.notfinite" );
							setResult( this->irBuilder.CreateNot( notFinite, "wrap.isfinite" ) );
						}
						return;
					}
					if( methodName == "floor" && isFloatWrapper ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							setResult( this->irBuilder.CreateUnaryIntrinsic( llvm::Intrinsic::floor, selfValue, nullptr, "wrap.floor" ) );
						}
						return;
					}
					if( methodName == "ceil" && isFloatWrapper ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							setResult( this->irBuilder.CreateUnaryIntrinsic( llvm::Intrinsic::ceil, selfValue, nullptr, "wrap.ceil" ) );
						}
						return;
					}
					if( methodName == "round" && isFloatWrapper ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							setResult( this->irBuilder.CreateUnaryIntrinsic( llvm::Intrinsic::round, selfValue, nullptr, "wrap.round" ) );
						}
						return;
					}
					if( methodName == "sqrt" && isFloatWrapper ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							setResult( this->irBuilder.CreateUnaryIntrinsic( llvm::Intrinsic::sqrt, selfValue, nullptr, "wrap.sqrt" ) );
						}
						return;
					}
					if( methodName == "power" && isFloatWrapper ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							setResult( this->irBuilder.CreateBinaryIntrinsic( llvm::Intrinsic::pow, selfValue, argValue, nullptr, "wrap.pow" ) );
						}
						return;
					}
					if( methodName == "isZero" ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							llvm::Value* result = isFloatWrapper
								? this->irBuilder.CreateFCmpOEQ( selfValue, llvm::ConstantFP::get( primitiveType, 0.0 ), "wrap.iszero" )
								: this->irBuilder.CreateICmpEQ( selfValue, llvm::ConstantInt::get( primitiveType, 0 ), "wrap.iszero" );
							setResult( result );
						}
						return;
					}
					if( methodName == "isPositive" ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							llvm::Value* result = isFloatWrapper
								? this->irBuilder.CreateFCmpOGT( selfValue, llvm::ConstantFP::get( primitiveType, 0.0 ), "wrap.ispos" )
								: this->irBuilder.CreateICmpSGT( selfValue, llvm::ConstantInt::get( primitiveType, 0, true ), "wrap.ispos" );
							setResult( result );
						}
						return;
					}
					if( methodName == "isNegative" ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							llvm::Value* result = isFloatWrapper
								? this->irBuilder.CreateFCmpOLT( selfValue, llvm::ConstantFP::get( primitiveType, 0.0 ), "wrap.isneg" )
								: this->irBuilder.CreateICmpSLT( selfValue, llvm::ConstantInt::get( primitiveType, 0, true ), "wrap.isneg" );
							setResult( result );
						}
						return;
					}
					if( methodName == "Boolean" || methodName == "Int" || methodName == "I64" ||
						methodName == "I32" || methodName == "I16" || methodName == "I8" ||
						methodName == "UInt" || methodName == "U64" || methodName == "U32" ||
						methodName == "U16" || methodName == "U8" || methodName == "Byte" ||
						methodName == "Long" || methodName == "Integer" || methodName == "Char" ||
						methodName == "Float" || methodName == "Double" || methodName == "F32" || methodName == "F64" ) {
						return;
					}
				}
			}
		}

		if( calledName == "concat" && instruction.sourceOperands.size() == 2 ) {
			llvm::Value* leftValue = this->loadVariableValue( instruction.sourceOperands[0] );
			llvm::Value* rightValue = this->loadVariableValue( instruction.sourceOperands[1] );
			if( leftValue != nullptr && rightValue != nullptr &&
				leftValue->getType()->isPointerTy() && rightValue->getType()->isPointerTy() ) {
				llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
				llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
				llvm::Function* strlenFunction = this->llvmModule->getFunction( "strlen" );
				if( strlenFunction == nullptr ) {
					llvm::FunctionType* strlenType = llvm::FunctionType::get( i64Type, { ptrType }, false );
					strlenFunction = llvm::Function::Create( strlenType, llvm::Function::ExternalLinkage, "strlen", this->llvmModule.get() );
				}
				llvm::Function* strcpyFunction = this->llvmModule->getFunction( "strcpy" );
				if( strcpyFunction == nullptr ) {
					llvm::FunctionType* strcpyType = llvm::FunctionType::get( ptrType, { ptrType, ptrType }, false );
					strcpyFunction = llvm::Function::Create( strcpyType, llvm::Function::ExternalLinkage, "strcpy", this->llvmModule.get() );
				}
				llvm::Function* strcatFunction = this->llvmModule->getFunction( "strcat" );
				if( strcatFunction == nullptr ) {
					llvm::FunctionType* strcatType = llvm::FunctionType::get( ptrType, { ptrType, ptrType }, false );
					strcatFunction = llvm::Function::Create( strcatType, llvm::Function::ExternalLinkage, "strcat", this->llvmModule.get() );
				}
				llvm::Function* mallocFunction = this->llvmModule->getFunction( "malloc" );
				if( mallocFunction == nullptr ) {
					llvm::FunctionType* mallocType = llvm::FunctionType::get( ptrType, { i64Type }, false );
					mallocFunction = llvm::Function::Create( mallocType, llvm::Function::ExternalLinkage, "malloc", this->llvmModule.get() );
				}
				llvm::Value* lenA = this->irBuilder.CreateCall( strlenFunction, { leftValue }, "len.a" );
				llvm::Value* lenB = this->irBuilder.CreateCall( strlenFunction, { rightValue }, "len.b" );
				llvm::Value* totalLen = this->irBuilder.CreateAdd( lenA, lenB, "total.len" );
				llvm::Value* allocSize = this->irBuilder.CreateAdd( totalLen, llvm::ConstantInt::get( i64Type, 1 ), "alloc.size" );
				llvm::Value* buffer = this->irBuilder.CreateCall( mallocFunction, { allocSize }, "str.buf" );
				this->irBuilder.CreateCall( strcpyFunction, { buffer, leftValue } );
				this->irBuilder.CreateCall( strcatFunction, { buffer, rightValue } );
				if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
					this->setVariableValue( instruction.destinationVariable, buffer );
				}
				return;
			}
		}

		if( calledName == "equals" && instruction.sourceOperands.size() == 2 ) {
			llvm::Value* leftValue = this->loadVariableValue( instruction.sourceOperands[0] );
			llvm::Value* rightValue = this->loadVariableValue( instruction.sourceOperands[1] );
			if( leftValue != nullptr && rightValue != nullptr &&
				leftValue->getType()->isPointerTy() && rightValue->getType()->isPointerTy() ) {
				llvm::Type* i32Type = llvm::Type::getInt32Ty( this->llvmContext );
				llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
				llvm::Function* strcmpFunction = this->llvmModule->getFunction( "strcmp" );
				if( strcmpFunction == nullptr ) {
					llvm::FunctionType* strcmpType = llvm::FunctionType::get( i32Type, { ptrType, ptrType }, false );
					strcmpFunction = llvm::Function::Create( strcmpType, llvm::Function::ExternalLinkage, "strcmp", this->llvmModule.get() );
				}
				llvm::Value* cmpResult = this->irBuilder.CreateCall( strcmpFunction, { leftValue, rightValue }, "strcmp.res" );
				llvm::Value* isEqual = this->irBuilder.CreateICmpEQ( cmpResult, llvm::ConstantInt::get( i32Type, 0 ), "streq" );
				if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
					this->setVariableValue( instruction.destinationVariable, isEqual );
				}
				return;
			}
		}

		llvm::Function* callee = nullptr;
		if( this->functionResolutionMap.count( calledName ) > 0 ) {
			callee = this->functionResolutionMap[calledName];
		}
		if( callee == nullptr ) {
			callee = this->llvmModule->getFunction( calledName );
		}
		if( callee == nullptr && calledName.find( '.' ) != std::string::npos ) {
			size_t dotPosition = calledName.find( '.' );
			std::string className = calledName.substr( 0, dotPosition );
			std::string methodName = calledName.substr( dotPosition + 1 );
			semantic::TypeSharedPointer classType = this->semanticAnalyzer.types().lookupType( className );
			while( callee == nullptr && classType != nullptr && classType->kind == semantic::Type::Kind::Class ) {
				semantic::ClassType* classTypePtr = static_cast<semantic::ClassType*>( classType.get() );
				if( classTypePtr->baseClass != nullptr ) {
					std::string parentName = classTypePtr->baseClass->name;
					std::string parentMethodName = parentName + "." + methodName;
					if( this->functionResolutionMap.count( parentMethodName ) > 0 ) {
						callee = this->functionResolutionMap[parentMethodName];
					}
					if( callee == nullptr ) {
						callee = this->llvmModule->getFunction( parentMethodName );
					}
					classType = classTypePtr->baseClass;
				}
				else {
					break;
				}
			}
		}
		if( callee == nullptr && calledName.find( '.' ) == std::string::npos ) {
			std::string constructorName = calledName + "." + calledName;
			if( this->functionResolutionMap.count( constructorName ) > 0 ) {
				callee = this->functionResolutionMap[constructorName];
			}
			if( callee == nullptr ) {
				callee = this->llvmModule->getFunction( constructorName );
			}
			if( callee == nullptr && functionDefinition.ownerClassQualifiedName.empty() == false ) {
				std::string qualifiedMethodName = functionDefinition.ownerClassQualifiedName + "." + calledName;
				if( this->functionResolutionMap.count( qualifiedMethodName ) > 0 ) {
					callee = this->functionResolutionMap[qualifiedMethodName];
				}
				if( callee == nullptr ) {
					callee = this->llvmModule->getFunction( qualifiedMethodName );
				}
			}
			if( callee == nullptr && instruction.sourceOperands.empty() == false ) {
				MIRVariableIdentifier receiverVariable = instruction.sourceOperands[0];
				if( functionDefinition.variableDescriptorTable.count( receiverVariable ) > 0 ) {
					MIRVariableDescriptor& receiverDescriptor = functionDefinition.variableDescriptorTable[receiverVariable];
					if( receiverDescriptor.variableType != nullptr ) {
						std::string receiverTypeName = receiverDescriptor.variableType->name;
						size_t genericPos = receiverTypeName.find( '<' );
						if( genericPos != std::string::npos ) {
							receiverTypeName = receiverTypeName.substr( 0, genericPos );
						}
						if( receiverTypeName.empty() == false ) {
							std::string methodName = receiverTypeName + "." + calledName;
							if( this->functionResolutionMap.count( methodName ) > 0 ) {
								callee = this->functionResolutionMap[methodName];
							}
							if( callee == nullptr ) {
								callee = this->llvmModule->getFunction( methodName );
							}
						}
					}
				}
				if( callee == nullptr ) {
					std::string dotCalledName = "." + calledName;
					for( const std::pair<const std::string, llvm::Function*>& mapEntry : this->functionResolutionMap ) {
						if( mapEntry.first.size() > dotCalledName.size() &&
							mapEntry.first.compare( mapEntry.first.size() - dotCalledName.size(), dotCalledName.size(), dotCalledName ) == 0 ) {
							callee = mapEntry.second;
							break;
						}
					}
				}
			}
		}
		if( callee == nullptr ) {
			llvm::Value* calleePointer = nullptr;
			for( std::pair<const MIRVariableIdentifier, MIRVariableDescriptor>& descriptorEntry : functionDefinition.variableDescriptorTable ) {
				if( descriptorEntry.second.variableName == calledName ) {
					calleePointer = this->loadVariableValue( descriptorEntry.first );
					break;
				}
			}
			if( calleePointer != nullptr ) {
				std::vector<llvm::Type*> paramTypes;
				std::vector<llvm::Value*> arguments;
				for( MIRVariableIdentifier sourceOperand : instruction.sourceOperands ) {
					llvm::Value* argumentValue = this->loadVariableValue( sourceOperand );
					if( argumentValue != nullptr ) {
						arguments.push_back( argumentValue );
						paramTypes.push_back( argumentValue->getType() );
					}
				}
				llvm::Type* returnType = llvm::Type::getInt64Ty( this->llvmContext );
				if( instruction.operandType != nullptr ) {
					returnType = this->toLLVMType( instruction.operandType );
				}
				llvm::FunctionType* indirectCallType = llvm::FunctionType::get( returnType, paramTypes, false );
				if( calleePointer->getType()->isPointerTy() == false ) {
					calleePointer = this->irBuilder.CreateIntToPtr(
						calleePointer, llvm::PointerType::getUnqual( this->llvmContext ), "fn.ptr"
					);
				}
				llvm::Value* callResult = this->irBuilder.CreateCall( indirectCallType, calleePointer, arguments );
				if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
					this->setVariableValue( instruction.destinationVariable, callResult );
				}
				return;
			}
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
					else if( argumentValue->getType()->isFloatingPointTy() && expectedType->isPointerTy() ) {
						llvm::Value* asInt = this->irBuilder.CreateBitCast(
							argumentValue, llvm::Type::getInt64Ty( this->llvmContext ), "arg.fptoi"
						);
						argumentValue = this->irBuilder.CreateIntToPtr( asInt, expectedType, "arg.ftoptr" );
					}
					else if( argumentValue->getType()->isPointerTy() && expectedType->isFloatingPointTy() ) {
						llvm::Value* asInt = this->irBuilder.CreatePtrToInt(
							argumentValue, llvm::Type::getInt64Ty( this->llvmContext ), "arg.ptoi"
						);
						argumentValue = this->irBuilder.CreateBitCast( asInt, expectedType, "arg.ptofp" );
					}
					else if( argumentValue->getType()->isFloatingPointTy() && expectedType->isFloatingPointTy() ) {
						argumentValue = this->irBuilder.CreateFPCast( argumentValue, expectedType, "arg.fpcast" );
					}
					else if( argumentValue->getType()->isIntegerTy( 1 ) && expectedType->isIntegerTy() ) {
						argumentValue = this->irBuilder.CreateZExt( argumentValue, expectedType, "arg.bext" );
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
			else if( returnValue->getType()->isIntegerTy() && expectedReturnType->isFloatingPointTy() ) {
				returnValue = this->irBuilder.CreateSIToFP( returnValue, expectedReturnType, "ret.itof" );
			}
			else if( returnValue->getType()->isFloatingPointTy() && expectedReturnType->isIntegerTy() ) {
				returnValue = this->irBuilder.CreateFPToSI( returnValue, expectedReturnType, "ret.ftoi" );
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

		if( conditionValue->getType()->isIntegerTy( 1 ) == false ) {
			if( conditionValue->getType()->isIntegerTy() ) {
				conditionValue = this->irBuilder.CreateICmpNE(
					conditionValue,
					llvm::ConstantInt::get( conditionValue->getType(), 0 ),
					"cond.bool"
				);
			}
			else if( conditionValue->getType()->isPointerTy() ) {
				conditionValue = this->irBuilder.CreateICmpNE(
					conditionValue,
					llvm::ConstantPointerNull::get(
						llvm::cast<llvm::PointerType>( conditionValue->getType() )
					),
					"cond.bool"
				);
			}
			else if( conditionValue->getType()->isFloatingPointTy() ) {
				conditionValue = this->irBuilder.CreateFCmpONE(
					conditionValue,
					llvm::ConstantFP::get( conditionValue->getType(), 0.0 ),
					"cond.bool"
				);
			}
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

		// Strategy 1: extract struct type from alloca/GEP source
		if( llvm::AllocaInst* allocaBase = llvm::dyn_cast<llvm::AllocaInst>( basePointer ) ) {
			llvm::Type* allocatedType = allocaBase->getAllocatedType();
			if( allocatedType->isStructTy() ) {
				pointedType = allocatedType;
			}
		}
		else if( llvm::GetElementPtrInst* gepBase = llvm::dyn_cast<llvm::GetElementPtrInst>( basePointer ) ) {
			llvm::Type* resultType = gepBase->getResultElementType();
			if( resultType->isStructTy() ) {
				pointedType = resultType;
			}
		}

		// Strategy 2: look up from alloca's allocated type (for double-pointer case)
		if( pointedType == nullptr ) {
			llvm::Value* rawPointer = this->getVariableValue( instruction.sourceOperands[0] );
			if( llvm::AllocaInst* allocaInst = llvm::dyn_cast<llvm::AllocaInst>( rawPointer ) ) {
				llvm::Type* allocatedType = allocaInst->getAllocatedType();
				if( allocatedType->isStructTy() ) {
					pointedType = allocatedType;
				}
				else if( allocatedType->isStructTy() ) {
					pointedType = allocatedType;
				}
			}
		}

		// Strategy 3: look up struct type from source variable's semantic type
		if( pointedType == nullptr && this->currentMIRFunction != nullptr ) {
			MIRVariableIdentifier sourceVariable = instruction.sourceOperands[0];
			if( this->currentMIRFunction->variableDescriptorTable.count( sourceVariable ) > 0 ) {
				MIRVariableDescriptor& descriptor =
					this->currentMIRFunction->variableDescriptorTable[sourceVariable];
				if( descriptor.variableType != nullptr ) {
					std::string typeName = descriptor.variableType->name;
					size_t genericPosition = typeName.find( '<' );
					if( genericPosition != std::string::npos ) {
						typeName = typeName.substr( 0, genericPosition );
					}
					if( this->structTypeCache.count( typeName ) > 0 ) {
						pointedType = this->structTypeCache[typeName];
					}
				}
				if( pointedType == nullptr &&
					this->currentMIRFunction->ownerClassQualifiedName.empty() == false ) {
					std::string ownerName = this->currentMIRFunction->ownerClassQualifiedName;
					if( this->structTypeCache.count( ownerName ) > 0 ) {
						pointedType = this->structTypeCache[ownerName];
					}
				}
			}
		}

		// Strategy 4: look up from struct type cache using field access name
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
			if( fieldIndex >= pointedType->getStructNumElements() &&
				instruction.fieldAccessName.empty() == false && this->currentMIRModule != nullptr ) {
				for( const std::pair<const std::string, TypeLayoutDescriptor>& layoutEntry :
					this->currentMIRModule->typeLayoutTable ) {
					llvm::StructType* candidateType = nullptr;
					if( this->structTypeCache.count( layoutEntry.first ) > 0 ) {
						candidateType = this->structTypeCache[layoutEntry.first];
					}
					if( candidateType != pointedType ) {
						continue;
					}
					const TypeLayoutDescriptor& layout = layoutEntry.second;
					for( size_t nameIndex = 0; nameIndex < layout.fieldNames.size(); nameIndex++ ) {
						if( layout.fieldNames[nameIndex] == instruction.fieldAccessName ) {
							fieldIndex = static_cast<unsigned>( nameIndex );
							break;
						}
					}
					break;
				}
			}
			if( fieldIndex < pointedType->getStructNumElements() ) {
				llvm::Value* fieldPointer = this->irBuilder.CreateStructGEP(
					pointedType, basePointer, fieldIndex, "gep.field"
				);
				this->setVariableValue( instruction.destinationVariable, fieldPointer );
			}
			else if( instruction.fieldAccessName.empty() == false ) {
				// Field not found — try property getter (ClassName.fieldName)
				std::string getterName;
				llvm::StructType* structType = llvm::dyn_cast<llvm::StructType>( pointedType );
				if( structType != nullptr && structType->hasName() ) {
					getterName = structType->getName().str() + "." + instruction.fieldAccessName;
				}
				llvm::Function* getterFunction = nullptr;
				if( getterName.empty() == false ) {
					if( this->functionResolutionMap.count( getterName ) > 0 ) {
						getterFunction = this->functionResolutionMap[getterName];
					}
					else {
						getterFunction = this->llvmModule->getFunction( getterName );
					}
				}
				if( getterFunction != nullptr && getterFunction->arg_size() == 1 ) {
					llvm::Value* result = this->irBuilder.CreateCall(
						getterFunction, { basePointer }, "prop." + instruction.fieldAccessName
					);
					this->setVariableValue( instruction.destinationVariable, result );
				}
				else {
					this->setVariableValue( instruction.destinationVariable, basePointer );
				}
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
			pointer, llvm::PointerType::getUnqual( this->llvmContext ), "free.cast"
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

		// Find struct type for the constructed type — strip generic params
		std::string typeName = instruction.calledFunctionQualifiedName;
		size_t genericBracket = typeName.find( '<' );
		if( genericBracket != std::string::npos ) {
			typeName = typeName.substr( 0, genericBracket );
		}

		// Memory<T> is a compiler intrinsic — raw calloc'd array, not a struct
		if( typeName == "Memory" ) {
			llvm::Type* elementType = this->resolveMemoryElementType( instruction.operandType );
			llvm::Value* capacityValue = nullptr;
			if( instruction.sourceOperands.empty() == false ) {
				capacityValue = this->loadVariableValue( instruction.sourceOperands[0] );
			}
			if( capacityValue == nullptr ) {
				capacityValue = llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->llvmContext ), 16 );
			}
			if( capacityValue->getType()->isIntegerTy() == false ) {
				capacityValue = this->irBuilder.CreateFPToSI(
					capacityValue, llvm::Type::getInt64Ty( this->llvmContext ), "mem.cap.int"
				);
			}
			llvm::Function* callocFunction = this->getOrCreateCalloc();
			llvm::DataLayout dataLayout( this->llvmModule.get() );
			uint64_t elementSize = dataLayout.getTypeAllocSize( elementType );
			llvm::Value* elementSizeValue = llvm::ConstantInt::get(
				llvm::Type::getInt64Ty( this->llvmContext ), elementSize
			);
			llvm::Value* rawPointer = this->irBuilder.CreateCall(
				callocFunction, { capacityValue, elementSizeValue }, "mem.raw"
			);
			this->setVariableValue( instruction.destinationVariable, rawPointer );
			this->memoryElementTypes[instruction.destinationVariable] = elementType;
			return;
		}

		// Arena<T> is also a compiler intrinsic — struct { ptr, i64 count, i64 capacity }
		if( typeName == "Arena" ) {
			llvm::Type* elementType = this->resolveMemoryElementType( instruction.operandType );
			llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
			llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
			llvm::StructType* arenaStructType = llvm::StructType::get(
				this->llvmContext, { ptrType, i64Type, i64Type }
			);
			llvm::Function* mallocFunction = this->getOrCreateMalloc();
			llvm::DataLayout dataLayout( this->llvmModule.get() );
			uint64_t arenaSize = dataLayout.getTypeAllocSize( arenaStructType );
			llvm::Value* arenaSizeValue = llvm::ConstantInt::get( i64Type, arenaSize );
			llvm::Value* arenaPointer = this->irBuilder.CreateCall(
				mallocFunction, { arenaSizeValue }, "arena.raw"
			);
			llvm::Value* capacityValue = nullptr;
			if( instruction.sourceOperands.empty() == false ) {
				capacityValue = this->loadVariableValue( instruction.sourceOperands[0] );
			}
			if( capacityValue == nullptr ) {
				capacityValue = llvm::ConstantInt::get( i64Type, 1024 );
			}
			if( capacityValue->getType()->isIntegerTy() == false ) {
				capacityValue = this->irBuilder.CreateFPToSI( capacityValue, i64Type, "arena.cap.int" );
			}
			uint64_t elementSize = dataLayout.getTypeAllocSize( elementType );
			llvm::Value* totalBytes = this->irBuilder.CreateMul(
				capacityValue, llvm::ConstantInt::get( i64Type, elementSize ), "arena.bytes"
			);
			llvm::Function* callocFunction = this->getOrCreateCalloc();
			llvm::Value* elementArray = this->irBuilder.CreateCall(
				callocFunction, { capacityValue, llvm::ConstantInt::get( i64Type, elementSize ) }, "arena.elems"
			);
			llvm::Value* basePtr = this->irBuilder.CreateStructGEP( arenaStructType, arenaPointer, 0, "arena.base.ptr" );
			this->irBuilder.CreateStore( elementArray, basePtr );
			llvm::Value* countPtr = this->irBuilder.CreateStructGEP( arenaStructType, arenaPointer, 1, "arena.count.ptr" );
			this->irBuilder.CreateStore( llvm::ConstantInt::get( i64Type, 0 ), countPtr );
			llvm::Value* capPtr = this->irBuilder.CreateStructGEP( arenaStructType, arenaPointer, 2, "arena.cap.ptr" );
			this->irBuilder.CreateStore( capacityValue, capPtr );
			this->setVariableValue( instruction.destinationVariable, arenaPointer );
			this->memoryElementTypes[instruction.destinationVariable] = elementType;
			return;
		}

		// OOP wrapper types map to primitives — skip struct allocation
		static const std::unordered_set<std::string> oopWrapperNames = {
			"Int", "I64", "I32", "I16", "I8", "UInt", "U64", "U32", "U16", "U8",
			"Float", "Double", "F32", "F64", "Long", "Integer", "Boolean", "Byte", "Char",
			"String", "Void"
		};
		if( oopWrapperNames.count( typeName ) > 0 ) {
			semantic::TypeSharedPointer wrapperType = std::make_shared<semantic::Type>(
				semantic::Type::Kind::Class, typeName
			);
			llvm::Type* llvmTypeCheck = this->toLLVMType( wrapperType );
			if( instruction.sourceOperands.empty() == false ) {
				llvm::Value* argValue = this->loadVariableValue( instruction.sourceOperands[0] );
				if( argValue != nullptr ) {
					if( argValue->getType() != llvmTypeCheck ) {
						if( llvmTypeCheck->isIntegerTy() && argValue->getType()->isIntegerTy() ) {
							argValue = this->irBuilder.CreateIntCast( argValue, llvmTypeCheck, true, "wrap.cast" );
						}
						else if( llvmTypeCheck->isFloatingPointTy() && argValue->getType()->isIntegerTy() ) {
							argValue = this->irBuilder.CreateSIToFP( argValue, llvmTypeCheck, "wrap.itof" );
						}
						else if( llvmTypeCheck->isIntegerTy() && argValue->getType()->isFloatingPointTy() ) {
							argValue = this->irBuilder.CreateFPToSI( argValue, llvmTypeCheck, "wrap.ftoi" );
						}
					}
					this->setVariableValue( instruction.destinationVariable, argValue );
					return;
				}
			}
			llvm::Value* defaultValue = llvm::Constant::getNullValue( llvmTypeCheck );
			this->setVariableValue( instruction.destinationVariable, defaultValue );
			return;
		}

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
			if( typeSize < 64 ) {
				typeSize = 64;
			}
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
			if( constructorFunction != nullptr &&
				constructorFunction->arg_size() == instruction.sourceOperands.size() + 1 ) {
				// Pre-validate arg types before emitting any IR
				bool argsCompatible = true;
				for( size_t i = 0; i < instruction.sourceOperands.size(); i++ ) {
					llvm::Value* argValue = this->loadVariableValue( instruction.sourceOperands[i] );
					if( argValue == nullptr ) { argsCompatible = false; break; }
					llvm::Type* expectedType = constructorFunction->getArg( i + 1 )->getType();
					llvm::Type* actualType = argValue->getType();
					if( actualType == expectedType ) { continue; }
					if( expectedType->isIntegerTy() && actualType->isIntegerTy() ) { continue; }
					if( expectedType->isFloatingPointTy() && actualType->isIntegerTy() ) { continue; }
					if( expectedType->isIntegerTy() && actualType->isFloatingPointTy() ) { continue; }
					if( expectedType->isPointerTy() && actualType->isPointerTy() ) { continue; }
					argsCompatible = false;
					break;
				}
			if( argsCompatible ) {
				std::vector<llvm::Value*> constructorArgs;
				llvm::Type* selfParamType = constructorFunction->getArg( 0 )->getType();
				llvm::Value* selfArg = typedPointer;
				if( selfParamType != typedPointer->getType() ) {
					if( selfParamType->isIntegerTy() ) {
						selfArg = this->irBuilder.CreatePtrToInt( typedPointer, selfParamType, "self.int" );
					}
					else if( selfParamType->isFloatingPointTy() ) {
						llvm::Value* asInt = this->irBuilder.CreatePtrToInt(
							typedPointer, llvm::Type::getInt64Ty( this->llvmContext ), "self.ptoi"
						);
						selfArg = this->irBuilder.CreateSIToFP( asInt, selfParamType, "self.fp" );
					}
					else {
						selfArg = typedPointer;
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
			} // argsCompatible
			} // arity match
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
				return llvm::PointerType::getUnqual( this->llvmContext );
			case semantic::Type::Kind::Void:
				return llvm::Type::getVoidTy( this->llvmContext );
			case semantic::Type::Kind::Pointer:
			case semantic::Type::Kind::Reference:
				return llvm::PointerType::getUnqual( this->llvmContext );
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
				if( qualifiedName == semantic::qname::I16 || className == "I16" ) {
					return llvm::Type::getInt16Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qname::I8 || className == "I8" ) {
					return llvm::Type::getInt8Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qname::BYTE || className == "Byte" ) {
					return llvm::Type::getInt8Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qname::UINT || qualifiedName == semantic::qname::U64 ||
					className == "UInt" || className == "U64" ) {
					return llvm::Type::getInt64Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qname::U32 || className == "U32" ) {
					return llvm::Type::getInt32Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qname::U16 || className == "U16" ) {
					return llvm::Type::getInt16Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qname::U8 || className == "U8" ) {
					return llvm::Type::getInt8Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qname::LONG || className == "Long" ) {
					return llvm::Type::getInt64Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qname::INTEGER || className == "Integer" ) {
					return llvm::Type::getInt64Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qname::F32 || className == "F32" ) {
					return llvm::Type::getFloatTy( this->llvmContext );
				}
				if( qualifiedName == semantic::qname::CHAR || className == "Char" ) {
					return llvm::Type::getInt32Ty( this->llvmContext );
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
					return llvm::PointerType::getUnqual( this->llvmContext );
				}
				if( qualifiedName == semantic::qname::VOID || className == "Void" ) {
					return llvm::Type::getVoidTy( this->llvmContext );
				}
				if( qualifiedName == semantic::qname::OBJECT || className == "Object" ) {
					return llvm::PointerType::getUnqual( this->llvmContext );
				}

				// Class type → struct pointer
				llvm::StructType* structType = llvm::StructType::getTypeByName(
					this->llvmContext, className
				);
				if( structType != nullptr ) {
					return llvm::PointerType::getUnqual( structType );
				}
				return llvm::PointerType::getUnqual( this->llvmContext );
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
				return llvm::PointerType::getUnqual( this->llvmContext );
			}
			case semantic::Type::Kind::Function:
			case semantic::Type::Kind::Callable:
				return llvm::PointerType::getUnqual( this->llvmContext );
			case semantic::Type::Kind::Optional:
				return llvm::PointerType::getUnqual( this->llvmContext );
			case semantic::Type::Kind::Future:
				return llvm::Type::getInt64Ty( this->llvmContext );
			default:
				return llvm::Type::getInt64Ty( this->llvmContext );
		}
	}

	bool MIRCodegen::functionReturnsConstructedObject( MIRFunctionDefinition& functionDefinition ) {
		std::unordered_set<MIRVariableIdentifier> constructedVariables;
		for( std::shared_ptr<MIRBasicBlock>& block : functionDefinition.controlFlowBlocks ) {
			if( block == nullptr ) {
				continue;
			}
			for( const MIRInstruction& instruction : block->blockInstructions ) {
				if( instruction.instructionKind == MIRInstructionKind::ConstructObject &&
					instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
					constructedVariables.insert( instruction.destinationVariable );
				}
				if( instruction.instructionKind == MIRInstructionKind::StoreVariable &&
					instruction.sourceOperands.empty() == false ) {
					if( constructedVariables.count( instruction.sourceOperands[0] ) > 0 ) {
						constructedVariables.insert( instruction.destinationVariable );
					}
				}
				if( instruction.instructionKind == MIRInstructionKind::ReturnValue &&
					instruction.sourceOperands.empty() == false ) {
					MIRVariableIdentifier returnVar = instruction.sourceOperands[0];
					if( constructedVariables.count( returnVar ) > 0 ) {
						return true;
					}
				}
			}
		}
		return false;
	}

	llvm::Type* MIRCodegen::resolveReturnType( const semantic::TypeSharedPointer& returnTypeDescriptor ) {
		if( returnTypeDescriptor == nullptr ) {
			return llvm::Type::getVoidTy( this->llvmContext );
		}
		llvm::Type* returnType = this->toLLVMType( returnTypeDescriptor );
		if( ( returnTypeDescriptor->kind == semantic::Type::Kind::Class ||
			  returnTypeDescriptor->kind == semantic::Type::Kind::Struct ) &&
			returnType->isPointerTy() == false ) {
			std::string typeName = returnTypeDescriptor->name;
			size_t genericPosition = typeName.find( '<' );
			if( genericPosition != std::string::npos ) {
				typeName = typeName.substr( 0, genericPosition );
			}
			if( this->structTypeCache.count( typeName ) > 0 ) {
				returnType = llvm::PointerType::getUnqual( this->llvmContext );
			}
		}
		return returnType;
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
			type = llvm::PointerType::getUnqual( function->getContext() );
		}
		llvm::IRBuilder<> entryBuilder( &function->getEntryBlock(), function->getEntryBlock().begin() );
		return entryBuilder.CreateAlloca( type, nullptr, name );
	}

	llvm::Function* MIRCodegen::getOrCreateMalloc() {
		llvm::Function* mallocFunction = this->llvmModule->getFunction( "malloc" );
		if( mallocFunction == nullptr ) {
			llvm::FunctionType* mallocType = llvm::FunctionType::get(
				llvm::PointerType::getUnqual( this->llvmContext ),
				{ llvm::Type::getInt64Ty( this->llvmContext ) },
				false
			);
			mallocFunction = llvm::Function::Create(
				mallocType, llvm::Function::ExternalLinkage, "malloc", this->llvmModule.get()
			);
		}
		return mallocFunction;
	}

	llvm::Function* MIRCodegen::getOrCreateCalloc() {
		llvm::Function* callocFunction = this->llvmModule->getFunction( "calloc" );
		if( callocFunction == nullptr ) {
			llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
			llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
			llvm::FunctionType* callocType = llvm::FunctionType::get( ptrType, { i64Type, i64Type }, false );
			callocFunction = llvm::Function::Create(
				callocType, llvm::Function::ExternalLinkage, "calloc", this->llvmModule.get()
			);
		}
		return callocFunction;
	}

	llvm::Type* MIRCodegen::resolveMemoryElementType( const semantic::TypeSharedPointer& operandType ) {
		if( operandType == nullptr ) {
			return llvm::Type::getInt64Ty( this->llvmContext );
		}

		std::string elementClassName;
		semantic::TypeSharedPointer elementSemaType = nullptr;

		if( operandType->kind == semantic::Type::Kind::Class ) {
			semantic::ClassType* classType = dynamic_cast<semantic::ClassType*>( operandType.get() );
			if( classType != nullptr && classType->typeSubstitutions.empty() == false ) {
				for( const std::pair<const std::string, semantic::TypeSharedPointer>& substitution : classType->typeSubstitutions ) {
					if( substitution.second != nullptr ) {
						elementSemaType = substitution.second;
						elementClassName = substitution.second->name;
						break;
					}
				}
			}
		}

		if( elementSemaType == nullptr ) {
			std::string typeName = operandType->name;
			size_t openBracket = typeName.find( '<' );
			size_t closeBracket = typeName.rfind( '>' );
			if( openBracket != std::string::npos && closeBracket != std::string::npos && closeBracket > openBracket ) {
				elementClassName = typeName.substr( openBracket + 1, closeBracket - openBracket - 1 );
				elementSemaType = std::make_shared<semantic::Type>(
					semantic::Type::Kind::Class, elementClassName
				);
			}
		}

		if( elementSemaType == nullptr ) {
			return llvm::Type::getInt64Ty( this->llvmContext );
		}

		llvm::Type* resolved = this->toLLVMType( elementSemaType );
		if( resolved == nullptr || resolved->isVoidTy() ) {
			return llvm::Type::getInt64Ty( this->llvmContext );
		}

		if( resolved->isPointerTy() && elementSemaType->kind == semantic::Type::Kind::Class ) {
			if( this->structTypeCache.count( elementClassName ) > 0 ) {
				return this->structTypeCache[elementClassName];
			}
			llvm::StructType* structType = llvm::StructType::getTypeByName( this->llvmContext, elementClassName );
			if( structType != nullptr ) {
				return structType;
			}
		}

		return resolved;
	}

	llvm::Function* MIRCodegen::getOrCreateFree() {
		llvm::Function* freeFunction = this->llvmModule->getFunction( "free" );
		if( freeFunction == nullptr ) {
			llvm::FunctionType* freeType = llvm::FunctionType::get(
				llvm::Type::getVoidTy( this->llvmContext ),
				{ llvm::PointerType::getUnqual( this->llvmContext ) },
				false
			);
			freeFunction = llvm::Function::Create(
				freeType, llvm::Function::ExternalLinkage, "free", this->llvmModule.get()
			);
		}
		return freeFunction;
	}

	llvm::Function* MIRCodegen::getOrCreateUraniteThrow() {
		llvm::Function* throwFunction = this->llvmModule->getFunction( "__uranite_throw" );
		if( throwFunction == nullptr ) {
			llvm::Type* pointerType = llvm::PointerType::getUnqual( this->llvmContext );
			llvm::FunctionType* throwType = llvm::FunctionType::get(
				llvm::Type::getVoidTy( this->llvmContext ),
				{ pointerType, pointerType },
				false
			);
			throwFunction = llvm::Function::Create(
				throwType, llvm::Function::ExternalLinkage, "__uranite_throw", this->llvmModule.get()
			);
			throwFunction->setDoesNotReturn();
		}
		return throwFunction;
	}

} // namespace uranite::ir::mir
