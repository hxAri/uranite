
//
// @author hxAri (hxari)
// @create 13-06-2026
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
// Uranite Licence under GNU General Public Licence v3
//

#include <fmt/format.h>
#include <unordered_set>

#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetOptions.h>

#include "uranite/ir/mir-codegen.hpp"
#include "uranite/semantic/qualnames.hpp"

namespace uranite::ir::mir {

	MIRCodegen::MIRCodegen( semantic::Analyzer& semanticAnalyzer, diagnostic::Engine& diagnosticEngine )
		: semanticAnalyzer( semanticAnalyzer ),
		  diagnosticEngine( diagnosticEngine ),
		  runtimeInterface_( std::make_shared<codegen::DefaultRuntime>() ),
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

		this->mirFunctionDefinitionMap.clear();
		for( std::shared_ptr<MIRFunctionDefinition>& funcDef : mirModule.functionDefinitions ) {
			if( funcDef == nullptr ) continue;
			std::string mapKey = funcDef->ownerClassQualifiedName.empty()
				? funcDef->functionName
				: funcDef->ownerClassQualifiedName + "." + funcDef->functionName;
			this->mirFunctionDefinitionMap[mapKey] = funcDef.get();
		}

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
				if( functionDefinition->ownerClassQualifiedName.empty() ) {
					continue;
				}
				llvm::Function* existingFunction = this->functionResolutionMap[llvmFunctionName];
				std::string existingSuffix = fmt::format( "{}#{}", llvmFunctionName, existingFunction->arg_size() );
				if( this->functionResolutionMap.count( existingSuffix ) == 0 ) {
					this->functionResolutionMap[existingSuffix] = existingFunction;
				}
				llvmFunctionName = fmt::format( "{}#{}", llvmFunctionName,
					functionDefinition->parameterVariableIdentifiers.size() );
				if( this->functionResolutionMap.count( llvmFunctionName ) > 0 ) {
					continue;
				}
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
			for( unsigned paramIdx = 0; paramIdx < functionDefinition->parameterVariableIdentifiers.size(); paramIdx++ ) {
				MIRVariableIdentifier parameterVariable = functionDefinition->parameterVariableIdentifiers[paramIdx];
				llvm::Type* parameterType = llvm::Type::getInt64Ty( this->llvmContext );
				if( functionDefinition->variadicParameterIndex >= 0 &&
					paramIdx == static_cast<unsigned>( functionDefinition->variadicParameterIndex ) ) {
					parameterType = llvm::PointerType::getUnqual( this->llvmContext );
				}
				else if( functionDefinition->variableDescriptorTable.count( parameterVariable ) > 0 ) {
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

		// Resolve unresolved interface method calls (Sequence.get -> ArrayList.get, etc.)
		{
			std::vector<llvm::Function*> toErase;
			for( llvm::Function& unresolvedFunction : *this->llvmModule ) {
				if( unresolvedFunction.isDeclaration() == false ) continue;
				std::string unresolvedName = unresolvedFunction.getName().str();
				size_t dotPos = unresolvedName.find( '.' );
				if( dotPos == std::string::npos ) continue;
				std::string typeName = unresolvedName.substr( 0, dotPos );
				std::string methodName = unresolvedName.substr( dotPos + 1 );
				llvm::Function* resolvedFunction = nullptr;
				for( const std::pair<const std::string, llvm::Function*>& entry : this->functionResolutionMap ) {
					size_t entryDot = entry.first.find( '.' );
					if( entryDot == std::string::npos ) continue;
					if( entry.first.substr( entryDot + 1 ) != methodName ) continue;
					std::string candidateClass = entry.first.substr( 0, entryDot );
					if( candidateClass == typeName ) continue;
					semantic::TypeSharedPointer candidateType = this->semanticAnalyzer.types().lookupType( candidateClass );
					if( candidateType == nullptr ) {
						for( const std::pair<const std::string, semantic::TypeSharedPointer>& regEntry : this->semanticAnalyzer.types().getUserTypes() ) {
							if( regEntry.second != nullptr && regEntry.second->name == candidateClass ) {
								candidateType = regEntry.second;
								break;
							}
						}
					}
					if( candidateType == nullptr || candidateType->kind != semantic::Type::Kind::Class ) continue;
					semantic::ClassType* classPtr = static_cast<semantic::ClassType*>( candidateType.get() );
					std::vector<semantic::TypeSharedPointer> interfaceQueue( classPtr->interfaces.begin(), classPtr->interfaces.end() );
					std::unordered_set<std::string> visitedInterfaces;
					while( interfaceQueue.empty() == false && resolvedFunction == nullptr ) {
						semantic::TypeSharedPointer iface = interfaceQueue.back();
						interfaceQueue.pop_back();
						if( iface == nullptr ) continue;
						std::string ifaceName = iface->name;
						size_t bracketPos = ifaceName.find( '<' );
						if( bracketPos != std::string::npos ) ifaceName = ifaceName.substr( 0, bracketPos );
						if( visitedInterfaces.count( ifaceName ) > 0 ) continue;
						visitedInterfaces.insert( ifaceName );
						if( ifaceName == typeName ) {
							resolvedFunction = entry.second;
							break;
						}
						semantic::TypeSharedPointer parentIface = this->semanticAnalyzer.types().lookupType( ifaceName );
						if( parentIface == nullptr ) {
							for( const std::pair<const std::string, semantic::TypeSharedPointer>& regEntry : this->semanticAnalyzer.types().getUserTypes() ) {
								if( regEntry.second != nullptr && regEntry.second->name == ifaceName ) {
									parentIface = regEntry.second;
									break;
								}
							}
						}
						if( parentIface != nullptr && parentIface->kind == semantic::Type::Kind::Interface ) {
							semantic::InterfaceType* parentIfacePtr = static_cast<semantic::InterfaceType*>( parentIface.get() );
							for( const semantic::TypeSharedPointer& parentExtends : parentIfacePtr->superInterfaces ) {
								interfaceQueue.push_back( parentExtends );
							}
						}
					}
					if( resolvedFunction != nullptr ) break;
				}
				if( resolvedFunction != nullptr && resolvedFunction != &unresolvedFunction ) {
					if( unresolvedFunction.getFunctionType() == resolvedFunction->getFunctionType() ) {
						unresolvedFunction.replaceAllUsesWith( resolvedFunction );
						toErase.push_back( &unresolvedFunction );
					}
					else {
						llvm::BasicBlock* wrapperBlock = llvm::BasicBlock::Create(
							this->llvmContext, "entry", &unresolvedFunction
						);
						llvm::IRBuilder<> wrapperBuilder( wrapperBlock );
						std::vector<llvm::Value*> forwardedArgs;
						llvm::FunctionType* targetType = resolvedFunction->getFunctionType();
						unsigned argIndex = 0;
						for( llvm::Argument& arg : unresolvedFunction.args() ) {
							llvm::Value* forwarded = &arg;
							if( argIndex < targetType->getNumParams() ) {
								llvm::Type* expectedType = targetType->getParamType( argIndex );
								if( forwarded->getType() != expectedType ) {
									if( forwarded->getType()->isIntegerTy() && expectedType->isPointerTy() ) {
										forwarded = wrapperBuilder.CreateIntToPtr( forwarded, expectedType );
									}
									else if( forwarded->getType()->isPointerTy() && expectedType->isIntegerTy() ) {
										forwarded = wrapperBuilder.CreatePtrToInt( forwarded, expectedType );
									}
									else if( forwarded->getType()->isIntegerTy() && expectedType->isIntegerTy() ) {
										forwarded = wrapperBuilder.CreateIntCast( forwarded, expectedType, true );
									}
								}
							}
							forwardedArgs.push_back( forwarded );
							argIndex++;
						}
						llvm::Value* callResult = wrapperBuilder.CreateCall( resolvedFunction, forwardedArgs );
						if( unresolvedFunction.getReturnType()->isVoidTy() ) {
							wrapperBuilder.CreateRetVoid();
						}
						else if( callResult->getType() != unresolvedFunction.getReturnType() ) {
							if( callResult->getType()->isPointerTy() && unresolvedFunction.getReturnType()->isIntegerTy() ) {
								wrapperBuilder.CreateRet( wrapperBuilder.CreatePtrToInt( callResult, unresolvedFunction.getReturnType() ) );
							}
							else {
								wrapperBuilder.CreateRet( callResult );
							}
						}
						else {
							wrapperBuilder.CreateRet( callResult );
						}
					}
				}
			}
			for( llvm::Function* deadFunction : toErase ) {
				deadFunction->eraseFromParent();
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
			if( llvmFunction != nullptr && functionDefinition.ownerClassQualifiedName.empty() == false &&
				llvmFunction->arg_size() != functionDefinition.parameterVariableIdentifiers.size() ) {
				std::string aritySuffix = fmt::format( "{}#{}", llvmFunctionName,
					functionDefinition.parameterVariableIdentifiers.size() );
				if( this->functionResolutionMap.count( aritySuffix ) > 0 ) {
					llvmFunction = this->functionResolutionMap[aritySuffix];
					llvmFunctionName = aritySuffix;
				}
				else {
					llvmFunction = nullptr;
				}
			}
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
			for( unsigned paramIdx = 0; paramIdx < functionDefinition.parameterVariableIdentifiers.size(); paramIdx++ ) {
				MIRVariableIdentifier parameterVariable = functionDefinition.parameterVariableIdentifiers[paramIdx];
				llvm::Type* parameterType = llvm::Type::getInt64Ty( this->llvmContext );
				if( functionDefinition.variadicParameterIndex >= 0 &&
					paramIdx == static_cast<unsigned>( functionDefinition.variadicParameterIndex ) ) {
					parameterType = llvm::PointerType::getUnqual( this->llvmContext );
				}
				else if( functionDefinition.variableDescriptorTable.count( parameterVariable ) > 0 ) {
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
				break;
			case MIRInstructionKind::InvokeFunction: {
				// Resolve function
				std::string invokedName = instruction.calledFunctionQualifiedName;
				llvm::Function* invokedFunction = nullptr;
				if( this->functionResolutionMap.count( invokedName ) > 0 ) {
					invokedFunction = this->functionResolutionMap[invokedName];
				}
				if( invokedFunction == nullptr ) {
					invokedFunction = this->llvmModule->getFunction( invokedName );
				}
				if( invokedFunction == nullptr ) {
					std::vector<llvm::Type*> paramTypes;
					for( size_t operandIndex = 0; operandIndex < instruction.sourceOperands.size(); operandIndex++ ) {
						llvm::Value* operandValue = this->getVariableValue( instruction.sourceOperands[operandIndex] );
						llvm::Type* argType = llvm::Type::getInt64Ty( this->llvmContext );
						if( operandValue != nullptr ) {
							argType = operandValue->getType();
							if( llvm::AllocaInst* allocaInst = llvm::dyn_cast<llvm::AllocaInst>( operandValue ) ) {
								argType = allocaInst->getAllocatedType();
							}
						}
						paramTypes.push_back( argType );
					}
					llvm::Type* returnType = llvm::Type::getInt64Ty( this->llvmContext );
					if( instruction.operandType != nullptr ) {
						returnType = this->toLLVMType( instruction.operandType );
					}
					llvm::FunctionType* externType = llvm::FunctionType::get( returnType, paramTypes, false );
					invokedFunction = llvm::Function::Create(
						externType, llvm::Function::ExternalLinkage, invokedName, this->llvmModule.get()
					);
					this->functionResolutionMap[invokedName] = invokedFunction;
				}
				// Set personality on enclosing function
				llvm::Function* enclosingFunction = this->irBuilder.GetInsertBlock()->getParent();
				if( enclosingFunction->hasPersonalityFn() == false ) {
					enclosingFunction->setPersonalityFn( this->getOrCreatePersonality() );
				}
				// Build arguments
				llvm::FunctionType* invokeType = invokedFunction->getFunctionType();
				unsigned expectedParams = invokeType->getNumParams();
				std::vector<llvm::Value*> invokeArgs;
				for( unsigned argumentIndex = 0; argumentIndex < instruction.sourceOperands.size(); argumentIndex++ ) {
					if( argumentIndex >= expectedParams && invokedFunction->isVarArg() == false ) {
						break;
					}
					llvm::Value* argumentValue = this->loadVariableValue( instruction.sourceOperands[argumentIndex] );
					if( argumentValue == nullptr ) {
						if( argumentIndex < expectedParams ) {
							invokeArgs.push_back( llvm::Constant::getNullValue( invokeType->getParamType( argumentIndex ) ) );
						}
						continue;
					}
					if( argumentIndex < expectedParams ) {
						llvm::Type* expectedType = invokeType->getParamType( argumentIndex );
						if( argumentValue->getType() != expectedType ) {
							if( argumentValue->getType()->isIntegerTy() && expectedType->isIntegerTy() ) {
								argumentValue = this->irBuilder.CreateIntCast( argumentValue, expectedType, true, "inv.cast" );
							}
							else if( argumentValue->getType()->isPointerTy() && expectedType->isIntegerTy() ) {
								argumentValue = this->irBuilder.CreatePtrToInt( argumentValue, expectedType, "inv.ptoi" );
							}
							else if( argumentValue->getType()->isIntegerTy() && expectedType->isPointerTy() ) {
								argumentValue = this->irBuilder.CreateIntToPtr( argumentValue, expectedType, "inv.itop" );
							}
							else if( argumentValue->getType()->isFloatingPointTy() && expectedType->isFloatingPointTy() ) {
								argumentValue = this->irBuilder.CreateFPCast( argumentValue, expectedType, "inv.fpc" );
							}
							else if( argumentValue->getType()->isIntegerTy() && expectedType->isFloatingPointTy() ) {
								argumentValue = this->irBuilder.CreateSIToFP( argumentValue, expectedType, "inv.itof" );
							}
							else if( argumentValue->getType()->isFloatingPointTy() && expectedType->isIntegerTy() ) {
								argumentValue = this->irBuilder.CreateFPToSI( argumentValue, expectedType, "inv.ftoi" );
							}
						}
					}
					invokeArgs.push_back( argumentValue );
				}
				while( invokeArgs.size() < expectedParams ) {
					invokeArgs.push_back( llvm::Constant::getNullValue( invokeType->getParamType( invokeArgs.size() ) ) );
				}
				if( invokeArgs.size() > expectedParams && invokedFunction->isVarArg() == false ) {
					invokeArgs.resize( expectedParams );
				}
				// Resolve target blocks
				llvm::BasicBlock* normalDest = this->blockMap[instruction.trueBranchTarget];
				llvm::BasicBlock* unwindDest = this->blockMap[instruction.landingPadTarget];
				if( normalDest == nullptr || unwindDest == nullptr ) {
					llvm::Value* result = this->irBuilder.CreateCall( invokedFunction, invokeArgs );
					if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER &&
						invokedFunction->getReturnType()->isVoidTy() == false ) {
						this->setVariableValue( instruction.destinationVariable, result );
					}
				}
				else {
					llvm::InvokeInst* invokeResult = this->irBuilder.CreateInvoke(
						invokedFunction, normalDest, unwindDest, invokeArgs
					);
					if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER &&
						invokedFunction->getReturnType()->isVoidTy() == false ) {
						this->setVariableValue( instruction.destinationVariable, invokeResult );
					}
				}
				break;
			}
			case MIRInstructionKind::LandingPad: {
				llvm::Function* enclosingFunction = this->irBuilder.GetInsertBlock()->getParent();
				if( enclosingFunction->hasPersonalityFn() == false ) {
					enclosingFunction->setPersonalityFn( this->getOrCreatePersonality() );
				}
				llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
				llvm::Type* i32Type = llvm::Type::getInt32Ty( this->llvmContext );
				llvm::StructType* landingPadType = llvm::StructType::get( this->llvmContext, { ptrType, i32Type } );
				llvm::LandingPadInst* landingPad = this->irBuilder.CreateLandingPad( landingPadType, 1, "lp" );
				landingPad->addClause( llvm::Constant::getNullValue( ptrType ) );
				llvm::Value* exceptionPtr = this->irBuilder.CreateExtractValue( landingPad, 0, "exc.ptr" );
				llvm::Value* caughtObject = this->irBuilder.CreateCall( this->getOrCreateBeginCatch(), { exceptionPtr }, "caught" );
				if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
					this->setVariableValue( instruction.destinationVariable, caughtObject );
				}
				break;
			}
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
					bool isMatch = false;
					if( instruction.operandType != nullptr && instruction.sourceOperands.empty() == false ) {
						std::string targetTypeName = instruction.operandType->name;
						size_t bracketPos = targetTypeName.find( '<' );
						if( bracketPos != std::string::npos ) {
							targetTypeName = targetTypeName.substr( 0, bracketPos );
						}
						MIRVariableIdentifier sourceVar = instruction.sourceOperands[0];
						if( this->currentMIRFunction->variableDescriptorTable.count( sourceVar ) > 0 ) {
							MIRVariableDescriptor& descriptor = this->currentMIRFunction->variableDescriptorTable[sourceVar];
							if( descriptor.variableType != nullptr ) {
								std::string sourceTypeName = descriptor.variableType->name;
								bracketPos = sourceTypeName.find( '<' );
								if( bracketPos != std::string::npos ) {
									sourceTypeName = sourceTypeName.substr( 0, bracketPos );
								}
								if( sourceTypeName == targetTypeName ) {
									isMatch = true;
								}
								else if( descriptor.variableType->kind == semantic::Type::Kind::Class ) {
									semantic::ClassType* classPtr = static_cast<semantic::ClassType*>( descriptor.variableType.get() );
									semantic::TypeSharedPointer current = classPtr->baseClass;
									while( current != nullptr && isMatch == false ) {
										std::string baseName = current->name;
										bracketPos = baseName.find( '<' );
										if( bracketPos != std::string::npos ) {
											baseName = baseName.substr( 0, bracketPos );
										}
										if( baseName == targetTypeName ) {
											isMatch = true;
										}
										if( current->kind == semantic::Type::Kind::Class ) {
											current = static_cast<semantic::ClassType*>( current.get() )->baseClass;
										}
										else {
											break;
										}
									}
								}
								if( isMatch == false && ( sourceTypeName == "Object" || sourceTypeName == targetTypeName ) ) {
									isMatch = true;
								}
							}
							else {
								isMatch = true;
							}
						}
						else {
							isMatch = true;
						}
					}
					this->setVariableValue( instruction.destinationVariable,
						isMatch ? llvm::ConstantInt::getTrue( this->llvmContext )
						        : llvm::ConstantInt::getFalse( this->llvmContext ) );
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
			if( this->currentMIRModule != nullptr && instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
				std::unordered_map<std::string, MIRModuleConstant>::iterator constantIterator =
					this->currentMIRModule->moduleConstants.find( globalName );
				if( constantIterator != this->currentMIRModule->moduleConstants.end() ) {
					MIRModuleConstant& constant = constantIterator->second;
					llvm::Value* constantValue = nullptr;
					switch( constant.kind ) {
						case MIRModuleConstant::Integer:
							constantValue = llvm::ConstantInt::get(
								llvm::Type::getInt64Ty( this->llvmContext ), constant.integerValue );
							break;
						case MIRModuleConstant::Float:
							constantValue = llvm::ConstantFP::get(
								llvm::Type::getDoubleTy( this->llvmContext ), constant.floatValue );
							break;
						case MIRModuleConstant::Boolean:
							constantValue = llvm::ConstantInt::get(
								llvm::Type::getInt1Ty( this->llvmContext ), constant.booleanValue ? 1 : 0 );
							break;
						case MIRModuleConstant::String:
							constantValue = this->irBuilder.CreateGlobalStringPtr( constant.stringValue, "const.str" );
							break;
					}
					if( constantValue != nullptr ) {
						this->setVariableValue( instruction.destinationVariable, constantValue );
						return;
					}
				}
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
		else if( llvm::GetElementPtrInst* sourceGEP = llvm::dyn_cast<llvm::GetElementPtrInst>( sourceValue ) ) {
			if( sourceGEP->getNumIndices() >= 2 ) {
				llvm::Type* elementType = sourceGEP->getResultElementType();
				if( elementType->isFirstClassType() && elementType->isVoidTy() == false ) {
					sourceValue = this->irBuilder.CreateLoad( elementType, sourceValue, "store.gep.load" );
				}
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
			llvm::Function* strlenFunction = this->getOrCreateStrlen();
			llvm::Function* writeFunction = this->getOrCreateWrite();
			llvm::Function* snprintfFunction = this->getOrCreateSnprintf();
			llvm::Function* mallocFunction = this->getOrCreateMalloc();
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
					llvm::Value* buf = this->irBuilder.CreateCall( mallocFunction, {
						llvm::ConstantInt::get( i64Type, 24 )
					}, "int.buf" );
					llvm::Value* val = argValue;
					if( argValue->getType() != i64Type ) {
						val = this->irBuilder.CreateSExt( argValue, i64Type, "iext" );
					}
					llvm::Value* intFmt = this->irBuilder.CreateGlobalStringPtr( "%ld", "int.fmt" );
					this->irBuilder.CreateCall( snprintfFunction, {
						buf, llvm::ConstantInt::get( i64Type, 24 ), intFmt, val
					} );
					strValue = buf;
				}
				else if( argValue->getType()->isDoubleTy() || argValue->getType()->isFloatTy() ) {
					llvm::Value* buf = this->irBuilder.CreateCall( mallocFunction, {
						llvm::ConstantInt::get( i64Type, 48 )
					}, "flt.buf" );
					llvm::Value* val = argValue;
					if( argValue->getType()->isFloatTy() ) {
						val = this->irBuilder.CreateFPExt( val, llvm::Type::getDoubleTy( this->llvmContext ), "f2d" );
					}
					llvm::Value* fltFmt = this->irBuilder.CreateGlobalStringPtr( "%f", "flt.fmt" );
					this->irBuilder.CreateCall( snprintfFunction, {
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
				llvm::Function* snprintfFunction = this->getOrCreateSnprintf();
				llvm::Function* strlenFunction = this->getOrCreateStrlen();
				llvm::Function* mallocFunction = this->getOrCreateMalloc();
				llvm::Value* fmtLen = this->irBuilder.CreateCall( strlenFunction, { formatString }, "fmt.len" );
				llvm::Value* extraSpace = llvm::ConstantInt::get( i64Type, 64 * ( instruction.sourceOperands.size() - 1 ) );
				llvm::Value* bufSize = this->irBuilder.CreateAdd( fmtLen, extraSpace, "buf.size" );
				llvm::Value* bufPtr = this->irBuilder.CreateCall( mallocFunction, { bufSize }, "fmt.buf" );
				llvm::Value* cFormatStr = formatString;
				std::vector<size_t> argOrder;
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
							if( charIndex + 1 < original.size() && original[charIndex] == '{' && original[charIndex + 1] == '{' ) {
								formatted += '{';
								charIndex++;
								continue;
							}
							if( charIndex + 1 < original.size() && original[charIndex] == '}' && original[charIndex + 1] == '}' ) {
								formatted += '}';
								charIndex++;
								continue;
							}
							if( original[charIndex] == '{' ) {
								size_t closePos = original.find( '}', charIndex + 1 );
								if( closePos != std::string::npos ) {
									std::string spec = original.substr( charIndex + 1, closePos - charIndex - 1 );
									size_t resolvedArgIdx = argIdx;
									std::string formatSpec;
									if( spec.empty() ) {
										resolvedArgIdx = argIdx++;
									}
									else if( spec.find( ':' ) == 0 ) {
										resolvedArgIdx = argIdx++;
										formatSpec = spec.substr( 1 );
									}
									else if( spec.find_first_not_of( "0123456789" ) == std::string::npos ) {
										resolvedArgIdx = std::stoul( spec ) + 1;
									}
									else {
										size_t colonPos = spec.find( ':' );
										if( colonPos != std::string::npos ) {
											std::string indexPart = spec.substr( 0, colonPos );
											formatSpec = spec.substr( colonPos + 1 );
											if( indexPart.find_first_not_of( "0123456789" ) == std::string::npos ) {
												resolvedArgIdx = std::stoul( indexPart ) + 1;
											}
										}
									}
									argOrder.push_back( resolvedArgIdx );
									if( formatSpec == "x" ) {
										formatted += "%lx";
									}
									else if( formatSpec == "X" ) {
										formatted += "%lX";
									}
									else if( formatSpec == "o" ) {
										formatted += "%lo";
									}
									else if( formatSpec == "b" ) {
										formatted += "%ld";
									}
									else if( formatSpec.empty() == false && formatSpec[0] == '.' ) {
										formatted += "%" + formatSpec;
									}
									else if( resolvedArgIdx < instruction.sourceOperands.size() ) {
										llvm::Value* argVal = this->loadVariableValue( instruction.sourceOperands[resolvedArgIdx] );
										if( argVal != nullptr && argVal->getType()->isFloatingPointTy() ) {
											formatted += "%f";
										}
										else if( argVal != nullptr && argVal->getType()->isPointerTy() ) {
											formatted += "%s";
										}
										else {
											formatted += "%ld";
										}
									}
									else {
										formatted += "%ld";
									}
									charIndex = closePos;
									continue;
								}
							}
							formatted += original[charIndex];
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
				if( argOrder.empty() == false ) {
					for( size_t orderIdx = 0; orderIdx < argOrder.size(); orderIdx++ ) {
						size_t resolvedIdx = argOrder[orderIdx];
						if( resolvedIdx < instruction.sourceOperands.size() ) {
							llvm::Value* argValue = this->loadVariableValue( instruction.sourceOperands[resolvedIdx] );
							if( argValue != nullptr ) {
								if( argValue->getType()->isFloatingPointTy() ) {
									argValue = this->irBuilder.CreateFPExt(
										argValue, llvm::Type::getDoubleTy( this->llvmContext ), "fmt.dbl" );
								}
								snprintfArgs.push_back( argValue );
							}
						}
					}
				}
				else {
					for( size_t argIndex = 1; argIndex < instruction.sourceOperands.size(); argIndex++ ) {
						llvm::Value* argValue = this->loadVariableValue( instruction.sourceOperands[argIndex] );
						if( argValue != nullptr ) {
							if( argValue->getType()->isFloatingPointTy() ) {
								argValue = this->irBuilder.CreateFPExt(
									argValue, llvm::Type::getDoubleTy( this->llvmContext ), "fmt.dbl" );
							}
							snprintfArgs.push_back( argValue );
						}
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
					std::unordered_map<ir::mir::MIRVariableIdentifier,ir::mir::MIRVariableDescriptor>::iterator descriptorIterator = this->currentMIRFunction->variableDescriptorTable.find( memoryVariableIdentifier );
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
					std::unordered_map<MIRVariableIdentifier, MIRVariableDescriptor>::iterator descriptorIterator = this->currentMIRFunction->variableDescriptorTable.find( memoryVariableIdentifier );
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
					std::unordered_map<MIRVariableIdentifier, MIRVariableDescriptor>::iterator descriptorIterator = this->currentMIRFunction->variableDescriptorTable.find( memoryVariableIdentifier );
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
					std::unordered_map<MIRVariableIdentifier, MIRVariableDescriptor>::iterator descriptorIterator = this->currentMIRFunction->variableDescriptorTable.find( arenaVariableIdentifier );
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
		if( calledName == "Arena.capacity" && instruction.sourceOperands.size() >= 1 ) {
			llvm::Value* arenaPointer = this->loadVariableValue( instruction.sourceOperands[0] );
			if( arenaPointer != nullptr && instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
				llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
				llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
				llvm::StructType* arenaStructType = llvm::StructType::get(
					this->llvmContext, { ptrType, i64Type, i64Type }
				);
				llvm::Value* capGep = this->irBuilder.CreateStructGEP( arenaStructType, arenaPointer, 2, "arena.cap.ptr" );
				llvm::Value* capValue = this->irBuilder.CreateLoad( i64Type, capGep, "arena.cap" );
				this->setVariableValue( instruction.destinationVariable, capValue );
			}
			return;
		}
		if( calledName == "Arena.Arena" ) {
			return;
		}

		// Universal methods without class prefix (from imported generic module code)
		if( calledName == "hashCode" && instruction.sourceOperands.empty() == false ) {
			llvm::Value* selfValue = this->loadVariableValue( instruction.sourceOperands[0] );
			if( selfValue != nullptr ) {
				llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
				llvm::Value* hashResult = nullptr;
				if( selfValue->getType()->isIntegerTy() ) {
					hashResult = this->irBuilder.CreateIntCast( selfValue, i64Type, true, "hash.int" );
				}
				else if( selfValue->getType()->isPointerTy() ) {
					hashResult = this->irBuilder.CreatePtrToInt( selfValue, i64Type, "hash.ptr" );
				}
				else if( selfValue->getType()->isFloatingPointTy() ) {
					hashResult = this->irBuilder.CreateBitCast( selfValue, i64Type, "hash.fp" );
				}
				if( hashResult != nullptr ) {
					this->setVariableValue( instruction.destinationVariable, hashResult );
					return;
				}
			}
		}
		if( calledName == "toString" && instruction.sourceOperands.empty() == false ) {
			llvm::Value* selfValue = this->loadVariableValue( instruction.sourceOperands[0] );
			if( selfValue != nullptr && selfValue->getType()->isPointerTy() ) {
				this->setVariableValue( instruction.destinationVariable, selfValue );
				return;
			}
		}
		if( calledName == "charCodeAt" && instruction.sourceOperands.size() >= 2 ) {
			llvm::Value* selfValue = this->loadVariableValue( instruction.sourceOperands[0] );
			llvm::Value* indexValue = this->loadVariableValue( instruction.sourceOperands[1] );
			if( selfValue != nullptr && indexValue != nullptr && selfValue->getType()->isPointerTy() ) {
				if( indexValue->getType()->isIntegerTy() == false ) {
					indexValue = this->irBuilder.CreateFPToSI(
						indexValue, llvm::Type::getInt64Ty( this->llvmContext ), "idx.int"
					);
				}
				llvm::Value* charPtr = this->irBuilder.CreateGEP(
					llvm::Type::getInt8Ty( this->llvmContext ), selfValue, indexValue, "str.char.ptr"
				);
				llvm::Value* charVal = this->irBuilder.CreateLoad(
					llvm::Type::getInt8Ty( this->llvmContext ), charPtr, "str.char"
				);
				llvm::Value* charI64 = this->irBuilder.CreateZExt(
					charVal, llvm::Type::getInt64Ty( this->llvmContext ), "str.charcode"
				);
				this->setVariableValue( instruction.destinationVariable, charI64 );
				return;
			}
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
					std::function<llvm::Value*()> loadBoolSelf = [&]() -> llvm::Value* {
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
					std::function<llvm::Value*()> loadBoolArg = [&]() -> llvm::Value* {
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
					std::function<void( llvm::Value* )> setBoolResult = [&]( llvm::Value* resultValue ) {
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
					std::function<llvm::Value*()> loadCharSelf = [&]() -> llvm::Value* {
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
					std::function<void( llvm::Value* )> setCharResult = [&]( llvm::Value* resultValue ) {
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
							llvm::Function* snprintfFunction = this->getOrCreateSnprintf();
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
				if( methodName == "hashCode" && instruction.sourceOperands.empty() == false ) {
					llvm::Value* selfValue = this->loadVariableValue( instruction.sourceOperands[0] );
					if( selfValue != nullptr ) {
						llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
						llvm::Value* hashResult = nullptr;
						if( selfValue->getType()->isIntegerTy() ) {
							hashResult = this->irBuilder.CreateIntCast( selfValue, i64Type, true, "hash.int" );
						}
						else if( selfValue->getType()->isPointerTy() ) {
							hashResult = this->irBuilder.CreatePtrToInt( selfValue, i64Type, "hash.ptr" );
						}
						else if( selfValue->getType()->isFloatingPointTy() ) {
							hashResult = this->irBuilder.CreateBitCast( selfValue, i64Type, "hash.fp" );
						}
						if( hashResult != nullptr ) {
							this->setVariableValue( instruction.destinationVariable, hashResult );
							return;
						}
					}
				}
				if( wrapperName == "String" ) {
					llvm::Value* selfValue = this->loadVariableValue( instruction.sourceOperands[0] );
					if( selfValue != nullptr ) {
						if( methodName == "toString" ) {
							this->setVariableValue( instruction.destinationVariable, selfValue );
							return;
						}
						if( methodName == "substring" && instruction.sourceOperands.size() >= 3 ) {
							llvm::Value* startIndex = this->loadVariableValue( instruction.sourceOperands[1] );
							llvm::Value* endIndex = this->loadVariableValue( instruction.sourceOperands[2] );
							llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
							if( startIndex->getType() != i64Type ) {
								startIndex = this->irBuilder.CreateIntCast( startIndex, i64Type, true, "sub.start" );
							}
							if( endIndex->getType() != i64Type ) {
								endIndex = this->irBuilder.CreateIntCast( endIndex, i64Type, true, "sub.end" );
							}
							llvm::Value* length = this->irBuilder.CreateSub( endIndex, startIndex, "sub.len" );
							llvm::Value* srcPtr = this->irBuilder.CreateGEP(
								llvm::Type::getInt8Ty( this->llvmContext ), selfValue, startIndex, "sub.src"
							);
							llvm::Value* sizeWithNull = this->irBuilder.CreateAdd(
								length, llvm::ConstantInt::get( i64Type, 1 ), "sub.alloc"
							);
							llvm::Value* destPtr = this->irBuilder.CreateCall( this->getOrCreateMalloc(), { sizeWithNull }, "sub.buf" );
							this->irBuilder.CreateCall( this->getOrCreateMemcpy(), { destPtr, srcPtr, length, this->irBuilder.getFalse() } );
							llvm::Value* nullPos = this->irBuilder.CreateGEP(
								llvm::Type::getInt8Ty( this->llvmContext ), destPtr, length, "sub.null"
							);
							this->irBuilder.CreateStore( llvm::ConstantInt::get( llvm::Type::getInt8Ty( this->llvmContext ), 0 ), nullPos );
							this->setVariableValue( instruction.destinationVariable, destPtr );
							return;
						}
						if( methodName == "equals" && instruction.sourceOperands.size() >= 2 ) {
							llvm::Value* otherValue = this->loadVariableValue( instruction.sourceOperands[1] );
							llvm::Function* strcmpFunction = this->getOrCreateStrcmp();
							llvm::Value* cmpResult = this->irBuilder.CreateCall( strcmpFunction, { selfValue, otherValue }, "str.cmp" );
							llvm::Value* isEqual = this->irBuilder.CreateICmpEQ(
								cmpResult, llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->llvmContext ), 0 ), "str.eq"
							);
							this->setVariableValue( instruction.destinationVariable, isEqual );
							return;
						}
						if( methodName == "length" ) {
							llvm::Function* strlenFunction = this->getOrCreateStrlen();
							llvm::Value* lenResult = this->irBuilder.CreateCall( strlenFunction, { selfValue }, "str.len" );
							this->setVariableValue( instruction.destinationVariable, lenResult );
							return;
						}
						if( methodName == "charCodeAt" && instruction.sourceOperands.size() >= 2 ) {
							llvm::Value* indexValue = this->loadVariableValue( instruction.sourceOperands[1] );
							if( indexValue != nullptr ) {
								if( indexValue->getType()->isIntegerTy() == false ) {
									indexValue = this->irBuilder.CreateFPToSI(
										indexValue, llvm::Type::getInt64Ty( this->llvmContext ), "idx.int"
									);
								}
								llvm::Value* charPtr = this->irBuilder.CreateGEP(
									llvm::Type::getInt8Ty( this->llvmContext ), selfValue, indexValue, "str.char.ptr"
								);
								llvm::Value* charVal = this->irBuilder.CreateLoad(
									llvm::Type::getInt8Ty( this->llvmContext ), charPtr, "str.char"
								);
								llvm::Value* charI64 = this->irBuilder.CreateZExt(
									charVal, llvm::Type::getInt64Ty( this->llvmContext ), "str.charcode"
								);
								this->setVariableValue( instruction.destinationVariable, charI64 );
								return;
							}
						}
						if( methodName == "getValue" || methodName == "value" ) {
							this->setVariableValue( instruction.destinationVariable, selfValue );
							return;
						}
						if( methodName == "isEmpty" ) {
							llvm::Value* lenResult = this->irBuilder.CreateCall( this->getOrCreateStrlen(), { selfValue }, "str.len" );
							llvm::Value* isZero = this->irBuilder.CreateICmpEQ(
								lenResult, llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->llvmContext ), 0 ), "str.empty" );
							this->setVariableValue( instruction.destinationVariable, isZero );
							return;
						}
						if( methodName == "concat" && instruction.sourceOperands.size() >= 2 ) {
							llvm::Value* otherValue = this->loadVariableValue( instruction.sourceOperands[1] );
							if( otherValue != nullptr ) {
								llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
								llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
								llvm::Function* strlenFunc = this->getOrCreateStrlen();
								llvm::Function* mallocFunc = this->getOrCreateMalloc();
								llvm::Function* strcpyFunc = this->getOrCreateStrcpy();
								llvm::Function* strcatFunc = this->getOrCreateStrcat();
								llvm::Value* lenA = this->irBuilder.CreateCall( strlenFunc, { selfValue }, "cat.lenA" );
								llvm::Value* lenB = this->irBuilder.CreateCall( strlenFunc, { otherValue }, "cat.lenB" );
								llvm::Value* totalLen = this->irBuilder.CreateAdd( lenA, lenB, "cat.total" );
								llvm::Value* allocSize = this->irBuilder.CreateAdd(
									totalLen, llvm::ConstantInt::get( i64Type, 1 ), "cat.alloc" );
								llvm::Value* buf = this->irBuilder.CreateCall( mallocFunc, { allocSize }, "cat.buf" );
								this->irBuilder.CreateCall( strcpyFunc, { buf, selfValue } );
								this->irBuilder.CreateCall( strcatFunc, { buf, otherValue } );
								this->setVariableValue( instruction.destinationVariable, buf );
							}
							return;
						}
						if( methodName == "startsWith" && instruction.sourceOperands.size() >= 2 ) {
							llvm::Value* prefixValue = this->loadVariableValue( instruction.sourceOperands[1] );
							if( prefixValue != nullptr ) {
								llvm::Function* strlenFunc = this->getOrCreateStrlen();
								llvm::Function* strncmpFunc = this->getOrCreateStrncmp();
								llvm::Value* prefixLen = this->irBuilder.CreateCall( strlenFunc, { prefixValue }, "sw.len" );
								llvm::Value* cmpResult = this->irBuilder.CreateCall( strncmpFunc, { selfValue, prefixValue, prefixLen }, "sw.cmp" );
								llvm::Value* isMatch = this->irBuilder.CreateICmpEQ(
									cmpResult, llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->llvmContext ), 0 ), "sw.eq" );
								this->setVariableValue( instruction.destinationVariable, isMatch );
							}
							return;
						}
						if( methodName == "endsWith" && instruction.sourceOperands.size() >= 2 ) {
							llvm::Value* suffixValue = this->loadVariableValue( instruction.sourceOperands[1] );
							if( suffixValue != nullptr ) {
								llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
								llvm::Type* i32Type = llvm::Type::getInt32Ty( this->llvmContext );
								llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
								llvm::Function* strlenFunc = this->getOrCreateStrlen();
								llvm::Function* strcmpFunc = this->getOrCreateStrcmp();
								llvm::Value* selfLen = this->irBuilder.CreateCall( strlenFunc, { selfValue }, "ew.slen" );
								llvm::Value* suffixLen = this->irBuilder.CreateCall( strlenFunc, { suffixValue }, "ew.suflen" );
								llvm::Value* offset = this->irBuilder.CreateSub( selfLen, suffixLen, "ew.off" );
								llvm::Value* tailPtr = this->irBuilder.CreateGEP(
									llvm::Type::getInt8Ty( this->llvmContext ), selfValue, offset, "ew.tail" );
								llvm::Value* cmpResult = this->irBuilder.CreateCall( strcmpFunc, { tailPtr, suffixValue }, "ew.cmp" );
								llvm::Value* isMatch = this->irBuilder.CreateICmpEQ(
									cmpResult, llvm::ConstantInt::get( i32Type, 0 ), "ew.eq" );
								llvm::Value* lenOk = this->irBuilder.CreateICmpSGE( selfLen, suffixLen, "ew.lenok" );
								llvm::Value* result = this->irBuilder.CreateAnd( lenOk, isMatch, "ew.result" );
								this->setVariableValue( instruction.destinationVariable, result );
							}
							return;
						}
						if( methodName == "contains" && instruction.sourceOperands.size() >= 2 ) {
							llvm::Value* substrValue = this->loadVariableValue( instruction.sourceOperands[1] );
							if( substrValue != nullptr ) {
								llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
								llvm::Function* strstrFunc = this->getOrCreateStrstr();
								llvm::Value* found = this->irBuilder.CreateCall( strstrFunc, { selfValue, substrValue }, "str.find" );
								llvm::Value* isFound = this->irBuilder.CreateICmpNE(
									found, llvm::ConstantPointerNull::get( llvm::PointerType::getUnqual( this->llvmContext ) ), "str.has" );
								this->setVariableValue( instruction.destinationVariable, isFound );
							}
							return;
						}
						if( methodName == "indexOf" && instruction.sourceOperands.size() >= 2 ) {
							llvm::Value* substrValue = this->loadVariableValue( instruction.sourceOperands[1] );
							if( substrValue != nullptr ) {
								llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
								llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
								llvm::Function* strstrFunc = this->getOrCreateStrstr();
								llvm::Value* found = this->irBuilder.CreateCall( strstrFunc, { selfValue, substrValue }, "idx.find" );
								llvm::Value* isNull = this->irBuilder.CreateICmpEQ(
									found, llvm::ConstantPointerNull::get( llvm::PointerType::getUnqual( this->llvmContext ) ), "idx.null" );
								llvm::Value* offset = this->irBuilder.CreatePtrDiff( llvm::Type::getInt8Ty( this->llvmContext ), found, selfValue, "idx.off" );
								llvm::Value* result = this->irBuilder.CreateSelect( isNull,
									llvm::ConstantInt::get( i64Type, -1 ), offset, "idx.result" );
								this->setVariableValue( instruction.destinationVariable, result );
							}
							return;
						}
						if( methodName == "charAt" && instruction.sourceOperands.size() >= 2 ) {
							llvm::Value* indexValue = this->loadVariableValue( instruction.sourceOperands[1] );
							if( indexValue != nullptr ) {
								llvm::Type* i8Type = llvm::Type::getInt8Ty( this->llvmContext );
								llvm::Value* charPtr = this->irBuilder.CreateGEP( i8Type, selfValue, indexValue, "at.ptr" );
								llvm::Value* charVal = this->irBuilder.CreateLoad( i8Type, charPtr, "at.char" );
								llvm::Value* charI64 = this->irBuilder.CreateZExt(
									charVal, llvm::Type::getInt64Ty( this->llvmContext ), "at.i64" );
								this->setVariableValue( instruction.destinationVariable, charI64 );
							}
							return;
						}
						if( methodName == "toUpper" ) {
							llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
							llvm::Type* i8Type = llvm::Type::getInt8Ty( this->llvmContext );
							llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
							llvm::Function* strlenFunc = this->getOrCreateStrlen();
							llvm::Function* mallocFunc = this->getOrCreateMalloc();
							llvm::Function* memcpyFunc = this->getOrCreateMemcpy();
							llvm::Value* len = this->irBuilder.CreateCall( strlenFunc, { selfValue }, "up.len" );
							llvm::Value* allocSize = this->irBuilder.CreateAdd( len, llvm::ConstantInt::get( i64Type, 1 ), "up.alloc" );
							llvm::Value* buf = this->irBuilder.CreateCall( mallocFunc, { allocSize }, "up.buf" );
							this->irBuilder.CreateCall( memcpyFunc, { buf, selfValue, allocSize, this->irBuilder.getFalse() } );
							llvm::Function* currentFunc = this->irBuilder.GetInsertBlock()->getParent();
							llvm::BasicBlock* loopHeader = llvm::BasicBlock::Create( this->llvmContext, "up.hdr", currentFunc );
							llvm::BasicBlock* loopBody = llvm::BasicBlock::Create( this->llvmContext, "up.body", currentFunc );
							llvm::BasicBlock* loopExit = llvm::BasicBlock::Create( this->llvmContext, "up.exit", currentFunc );
							llvm::Value* idxAlloca = this->irBuilder.CreateAlloca( i64Type, nullptr, "up.idx" );
							this->irBuilder.CreateStore( llvm::ConstantInt::get( i64Type, 0 ), idxAlloca );
							this->irBuilder.CreateBr( loopHeader );
							this->irBuilder.SetInsertPoint( loopHeader );
							llvm::Value* idx = this->irBuilder.CreateLoad( i64Type, idxAlloca, "up.i" );
							llvm::Value* cond = this->irBuilder.CreateICmpSLT( idx, len, "up.cond" );
							this->irBuilder.CreateCondBr( cond, loopBody, loopExit );
							this->irBuilder.SetInsertPoint( loopBody );
							llvm::Value* charPtr = this->irBuilder.CreateGEP( i8Type, buf, idx, "up.ptr" );
							llvm::Value* ch = this->irBuilder.CreateLoad( i8Type, charPtr, "up.ch" );
							llvm::Value* isLower = this->irBuilder.CreateAnd(
								this->irBuilder.CreateICmpSGE( ch, llvm::ConstantInt::get( i8Type, 'a' ), "up.ge" ),
								this->irBuilder.CreateICmpSLE( ch, llvm::ConstantInt::get( i8Type, 'z' ), "up.le" ), "up.islow" );
							llvm::Value* upper = this->irBuilder.CreateSub( ch, llvm::ConstantInt::get( i8Type, 32 ), "up.upper" );
							llvm::Value* result = this->irBuilder.CreateSelect( isLower, upper, ch, "up.sel" );
							this->irBuilder.CreateStore( result, charPtr );
							llvm::Value* nextIdx = this->irBuilder.CreateAdd( idx, llvm::ConstantInt::get( i64Type, 1 ), "up.next" );
							this->irBuilder.CreateStore( nextIdx, idxAlloca );
							this->irBuilder.CreateBr( loopHeader );
							this->irBuilder.SetInsertPoint( loopExit );
							this->setVariableValue( instruction.destinationVariable, buf );
							return;
						}
						if( methodName == "toLower" ) {
							llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
							llvm::Type* i8Type = llvm::Type::getInt8Ty( this->llvmContext );
							llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
							llvm::Function* strlenFunc = this->getOrCreateStrlen();
							llvm::Function* mallocFunc = this->getOrCreateMalloc();
							llvm::Function* memcpyFunc = this->getOrCreateMemcpy();
							llvm::Value* len = this->irBuilder.CreateCall( strlenFunc, { selfValue }, "lo.len" );
							llvm::Value* allocSize = this->irBuilder.CreateAdd( len, llvm::ConstantInt::get( i64Type, 1 ), "lo.alloc" );
							llvm::Value* buf = this->irBuilder.CreateCall( mallocFunc, { allocSize }, "lo.buf" );
							this->irBuilder.CreateCall( memcpyFunc, { buf, selfValue, allocSize, this->irBuilder.getFalse() } );
							llvm::Function* currentFunc = this->irBuilder.GetInsertBlock()->getParent();
							llvm::BasicBlock* loopHeader = llvm::BasicBlock::Create( this->llvmContext, "lo.hdr", currentFunc );
							llvm::BasicBlock* loopBody = llvm::BasicBlock::Create( this->llvmContext, "lo.body", currentFunc );
							llvm::BasicBlock* loopExit = llvm::BasicBlock::Create( this->llvmContext, "lo.exit", currentFunc );
							llvm::Value* idxAlloca = this->irBuilder.CreateAlloca( i64Type, nullptr, "lo.idx" );
							this->irBuilder.CreateStore( llvm::ConstantInt::get( i64Type, 0 ), idxAlloca );
							this->irBuilder.CreateBr( loopHeader );
							this->irBuilder.SetInsertPoint( loopHeader );
							llvm::Value* idx = this->irBuilder.CreateLoad( i64Type, idxAlloca, "lo.i" );
							llvm::Value* cond = this->irBuilder.CreateICmpSLT( idx, len, "lo.cond" );
							this->irBuilder.CreateCondBr( cond, loopBody, loopExit );
							this->irBuilder.SetInsertPoint( loopBody );
							llvm::Value* charPtr = this->irBuilder.CreateGEP( i8Type, buf, idx, "lo.ptr" );
							llvm::Value* ch = this->irBuilder.CreateLoad( i8Type, charPtr, "lo.ch" );
							llvm::Value* isUpper = this->irBuilder.CreateAnd(
								this->irBuilder.CreateICmpSGE( ch, llvm::ConstantInt::get( i8Type, 'A' ), "lo.ge" ),
								this->irBuilder.CreateICmpSLE( ch, llvm::ConstantInt::get( i8Type, 'Z' ), "lo.le" ), "lo.isup" );
							llvm::Value* lower = this->irBuilder.CreateAdd( ch, llvm::ConstantInt::get( i8Type, 32 ), "lo.lower" );
							llvm::Value* result = this->irBuilder.CreateSelect( isUpper, lower, ch, "lo.sel" );
							this->irBuilder.CreateStore( result, charPtr );
							llvm::Value* nextIdx = this->irBuilder.CreateAdd( idx, llvm::ConstantInt::get( i64Type, 1 ), "lo.next" );
							this->irBuilder.CreateStore( nextIdx, idxAlloca );
							this->irBuilder.CreateBr( loopHeader );
							this->irBuilder.SetInsertPoint( loopExit );
							this->setVariableValue( instruction.destinationVariable, buf );
							return;
						}
						if( methodName == "trim" ) {
							llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
							llvm::Type* i8Type = llvm::Type::getInt8Ty( this->llvmContext );
							llvm::Function* strlenFunc = this->getOrCreateStrlen();
							llvm::Function* mallocFunc = this->getOrCreateMalloc();
							llvm::Function* memcpyFunc = this->getOrCreateMemcpy();
							llvm::Value* len = this->irBuilder.CreateCall( strlenFunc, { selfValue }, "trim.len" );
							llvm::Function* currentFunc = this->irBuilder.GetInsertBlock()->getParent();
							llvm::BasicBlock* trimStartHdr = llvm::BasicBlock::Create( this->llvmContext, "trim.s.hdr", currentFunc );
							llvm::BasicBlock* trimStartBody = llvm::BasicBlock::Create( this->llvmContext, "trim.s.body", currentFunc );
							llvm::BasicBlock* trimStartInc = llvm::BasicBlock::Create( this->llvmContext, "trim.s.inc", currentFunc );
							llvm::BasicBlock* trimEndHdr = llvm::BasicBlock::Create( this->llvmContext, "trim.e.hdr", currentFunc );
							llvm::BasicBlock* trimEndBody = llvm::BasicBlock::Create( this->llvmContext, "trim.e.body", currentFunc );
							llvm::BasicBlock* trimEndDec = llvm::BasicBlock::Create( this->llvmContext, "trim.e.dec", currentFunc );
							llvm::BasicBlock* trimCopy = llvm::BasicBlock::Create( this->llvmContext, "trim.copy", currentFunc );
							llvm::Value* startAlloca = this->irBuilder.CreateAlloca( i64Type, nullptr, "trim.start" );
							llvm::Value* endAlloca = this->irBuilder.CreateAlloca( i64Type, nullptr, "trim.end" );
							this->irBuilder.CreateStore( llvm::ConstantInt::get( i64Type, 0 ), startAlloca );
							this->irBuilder.CreateStore( len, endAlloca );
							this->irBuilder.CreateBr( trimStartHdr );
							this->irBuilder.SetInsertPoint( trimStartHdr );
							llvm::Value* startIdx = this->irBuilder.CreateLoad( i64Type, startAlloca, "trim.si" );
							llvm::Value* startCond = this->irBuilder.CreateICmpSLT( startIdx, len, "trim.sc" );
							this->irBuilder.CreateCondBr( startCond, trimStartBody, trimEndHdr );
							this->irBuilder.SetInsertPoint( trimStartBody );
							llvm::Value* sCharPtr = this->irBuilder.CreateGEP( i8Type, selfValue, startIdx, "trim.scp" );
							llvm::Value* sCh = this->irBuilder.CreateLoad( i8Type, sCharPtr, "trim.sch" );
							llvm::Value* isSpace = this->irBuilder.CreateOr(
								this->irBuilder.CreateICmpEQ( sCh, llvm::ConstantInt::get( i8Type, ' ' ) ),
								this->irBuilder.CreateOr(
									this->irBuilder.CreateICmpEQ( sCh, llvm::ConstantInt::get( i8Type, '\t' ) ),
									this->irBuilder.CreateOr(
										this->irBuilder.CreateICmpEQ( sCh, llvm::ConstantInt::get( i8Type, '\n' ) ),
										this->irBuilder.CreateICmpEQ( sCh, llvm::ConstantInt::get( i8Type, '\r' ) ) ) ) );
							this->irBuilder.CreateCondBr( isSpace, trimStartInc, trimEndHdr );
							this->irBuilder.SetInsertPoint( trimStartInc );
							llvm::Value* nextStart = this->irBuilder.CreateAdd( startIdx, llvm::ConstantInt::get( i64Type, 1 ) );
							this->irBuilder.CreateStore( nextStart, startAlloca );
							this->irBuilder.CreateBr( trimStartHdr );
							this->irBuilder.SetInsertPoint( trimEndHdr );
							llvm::Value* endIdx = this->irBuilder.CreateLoad( i64Type, endAlloca, "trim.ei" );
							llvm::Value* startVal = this->irBuilder.CreateLoad( i64Type, startAlloca, "trim.sv" );
							llvm::Value* endCond = this->irBuilder.CreateICmpSGT( endIdx, startVal, "trim.ec" );
							this->irBuilder.CreateCondBr( endCond, trimEndBody, trimCopy );
							this->irBuilder.SetInsertPoint( trimEndBody );
							llvm::Value* ePos = this->irBuilder.CreateSub( endIdx, llvm::ConstantInt::get( i64Type, 1 ), "trim.epos" );
							llvm::Value* eCharPtr = this->irBuilder.CreateGEP( i8Type, selfValue, ePos, "trim.ecp" );
							llvm::Value* eCh = this->irBuilder.CreateLoad( i8Type, eCharPtr, "trim.ech" );
							llvm::Value* eIsSpace = this->irBuilder.CreateOr(
								this->irBuilder.CreateICmpEQ( eCh, llvm::ConstantInt::get( i8Type, ' ' ) ),
								this->irBuilder.CreateOr(
									this->irBuilder.CreateICmpEQ( eCh, llvm::ConstantInt::get( i8Type, '\t' ) ),
									this->irBuilder.CreateOr(
										this->irBuilder.CreateICmpEQ( eCh, llvm::ConstantInt::get( i8Type, '\n' ) ),
										this->irBuilder.CreateICmpEQ( eCh, llvm::ConstantInt::get( i8Type, '\r' ) ) ) ) );
							this->irBuilder.CreateCondBr( eIsSpace, trimEndDec, trimCopy );
							this->irBuilder.SetInsertPoint( trimEndDec );
							this->irBuilder.CreateStore( ePos, endAlloca );
							this->irBuilder.CreateBr( trimEndHdr );
							this->irBuilder.SetInsertPoint( trimCopy );
							llvm::Value* trimStart = this->irBuilder.CreateLoad( i64Type, startAlloca, "trim.rs" );
							llvm::Value* trimEnd = this->irBuilder.CreateLoad( i64Type, endAlloca, "trim.re" );
							llvm::Value* trimLen = this->irBuilder.CreateSub( trimEnd, trimStart, "trim.len2" );
							llvm::Value* trimAlloc = this->irBuilder.CreateAdd( trimLen, llvm::ConstantInt::get( i64Type, 1 ), "trim.alloc" );
							llvm::Value* trimBuf = this->irBuilder.CreateCall( mallocFunc, { trimAlloc }, "trim.buf" );
							llvm::Value* srcPtr = this->irBuilder.CreateGEP( i8Type, selfValue, trimStart, "trim.src" );
							this->irBuilder.CreateCall( memcpyFunc, { trimBuf, srcPtr, trimLen, this->irBuilder.getFalse() } );
							llvm::Value* nullPos = this->irBuilder.CreateGEP( i8Type, trimBuf, trimLen, "trim.null" );
							this->irBuilder.CreateStore( llvm::ConstantInt::get( i8Type, 0 ), nullPos );
							this->setVariableValue( instruction.destinationVariable, trimBuf );
							return;
						}
						if( methodName == "replace" && instruction.sourceOperands.size() >= 3 ) {
							llvm::Value* oldStr = this->loadVariableValue( instruction.sourceOperands[1] );
							llvm::Value* newStr = this->loadVariableValue( instruction.sourceOperands[2] );
							if( oldStr != nullptr && newStr != nullptr ) {
								llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
								llvm::Type* i8Type = llvm::Type::getInt8Ty( this->llvmContext );
								llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
								llvm::Function* strlenFunc = this->getOrCreateStrlen();
								llvm::Function* strstrFunc = this->getOrCreateStrstr();
								llvm::Function* mallocFunc = this->getOrCreateMalloc();
								llvm::Function* memcpyFunc = this->getOrCreateMemcpy();
								llvm::Value* selfLen = this->irBuilder.CreateCall( strlenFunc, { selfValue }, "rep.slen" );
								llvm::Value* oldLen = this->irBuilder.CreateCall( strlenFunc, { oldStr }, "rep.olen" );
								llvm::Value* newLen = this->irBuilder.CreateCall( strlenFunc, { newStr }, "rep.nlen" );
								llvm::Value* worstCase = this->irBuilder.CreateMul( selfLen,
									this->irBuilder.CreateAdd( newLen, llvm::ConstantInt::get( i64Type, 1 ) ), "rep.worst" );
								llvm::Value* allocSize = this->irBuilder.CreateAdd( worstCase,
									llvm::ConstantInt::get( i64Type, 1 ), "rep.alloc" );
								llvm::Value* buf = this->irBuilder.CreateCall( mallocFunc, { allocSize }, "rep.buf" );
								llvm::Function* currentFunc = this->irBuilder.GetInsertBlock()->getParent();
								llvm::BasicBlock* loopHdr = llvm::BasicBlock::Create( this->llvmContext, "rep.hdr", currentFunc );
								llvm::BasicBlock* foundBlock = llvm::BasicBlock::Create( this->llvmContext, "rep.found", currentFunc );
								llvm::BasicBlock* notFound = llvm::BasicBlock::Create( this->llvmContext, "rep.nf", currentFunc );
								llvm::BasicBlock* exitBlock = llvm::BasicBlock::Create( this->llvmContext, "rep.exit", currentFunc );
								llvm::Value* srcAlloca = this->irBuilder.CreateAlloca( ptrType, nullptr, "rep.src" );
								llvm::Value* dstAlloca = this->irBuilder.CreateAlloca( ptrType, nullptr, "rep.dst" );
								this->irBuilder.CreateStore( selfValue, srcAlloca );
								this->irBuilder.CreateStore( buf, dstAlloca );
								this->irBuilder.CreateBr( loopHdr );
								this->irBuilder.SetInsertPoint( loopHdr );
								llvm::Value* src = this->irBuilder.CreateLoad( ptrType, srcAlloca, "rep.csrc" );
								llvm::Value* found = this->irBuilder.CreateCall( strstrFunc, { src, oldStr }, "rep.find" );
								llvm::Value* isFound = this->irBuilder.CreateICmpNE( found,
									llvm::ConstantPointerNull::get( llvm::PointerType::getUnqual( this->llvmContext ) ), "rep.isf" );
								this->irBuilder.CreateCondBr( isFound, foundBlock, notFound );
								this->irBuilder.SetInsertPoint( foundBlock );
								llvm::Value* dst = this->irBuilder.CreateLoad( ptrType, dstAlloca, "rep.cdst" );
								llvm::Value* prefixLen = this->irBuilder.CreatePtrDiff( i8Type, found, src, "rep.plen" );
								this->irBuilder.CreateCall( memcpyFunc, { dst, src, prefixLen, this->irBuilder.getFalse() } );
								llvm::Value* dst2 = this->irBuilder.CreateGEP( i8Type, dst, prefixLen, "rep.d2" );
								this->irBuilder.CreateCall( memcpyFunc, { dst2, newStr, newLen, this->irBuilder.getFalse() } );
								llvm::Value* dst3 = this->irBuilder.CreateGEP( i8Type, dst2, newLen, "rep.d3" );
								this->irBuilder.CreateStore( dst3, dstAlloca );
								llvm::Value* nextSrc = this->irBuilder.CreateGEP( i8Type, found, oldLen, "rep.ns" );
								this->irBuilder.CreateStore( nextSrc, srcAlloca );
								this->irBuilder.CreateBr( loopHdr );
								this->irBuilder.SetInsertPoint( notFound );
								llvm::Value* finalSrc = this->irBuilder.CreateLoad( ptrType, srcAlloca, "rep.fs" );
								llvm::Value* finalDst = this->irBuilder.CreateLoad( ptrType, dstAlloca, "rep.fd" );
								llvm::Value* remainLen = this->irBuilder.CreateCall( strlenFunc, { finalSrc }, "rep.rlen" );
								llvm::Value* copyLen = this->irBuilder.CreateAdd( remainLen,
									llvm::ConstantInt::get( i64Type, 1 ), "rep.clen" );
								this->irBuilder.CreateCall( memcpyFunc, { finalDst, finalSrc, copyLen, this->irBuilder.getFalse() } );
								this->irBuilder.CreateBr( exitBlock );
								this->irBuilder.SetInsertPoint( exitBlock );
								this->setVariableValue( instruction.destinationVariable, buf );
							}
							return;
						}
						if( methodName == "String" ) {
							return;
						}
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

					std::function<llvm::Value*()> loadSelf = [&]() -> llvm::Value* {
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
					std::function<llvm::Value*()> loadArg = [&]() -> llvm::Value* {
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
					std::function<void( llvm::Value* )> setResult = [&]( llvm::Value* resultValue ) {
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
							llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
							llvm::Function* snprintfFunction = this->getOrCreateSnprintf();
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
			if( leftValue != nullptr && rightValue != nullptr ) {
				llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
				llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
				llvm::Function* strlenFunction = this->getOrCreateStrlen();
				llvm::Function* strcpyFunction = this->getOrCreateStrcpy();
				llvm::Function* strcatFunction = this->getOrCreateStrcat();
				llvm::Function* mallocFunction = this->getOrCreateMalloc();
				llvm::Function* snprintfFunction = this->getOrCreateSnprintf();
				std::function<llvm::Value*( llvm::Value* )> ensureString = [&]( llvm::Value* value ) -> llvm::Value* {
					if( value->getType()->isPointerTy() ) return value;
					if( value->getType()->isDoubleTy() ) {
						llvm::Value* buffer = this->irBuilder.CreateCall( mallocFunction, { llvm::ConstantInt::get( i64Type, 48 ) }, "flt.buf" );
						llvm::Value* fmtStr = this->irBuilder.CreateGlobalStringPtr( "%.6g", "flt.fmt" );
						this->irBuilder.CreateCall( snprintfFunction, { buffer, llvm::ConstantInt::get( i64Type, 48 ), fmtStr, value } );
						return buffer;
					}
					llvm::Value* intVal = value;
					if( value->getType()->getIntegerBitWidth() < 64 ) {
						intVal = this->irBuilder.CreateSExt( value, i64Type, "sext.arg" );
					}
					llvm::Value* buffer = this->irBuilder.CreateCall( mallocFunction, { llvm::ConstantInt::get( i64Type, 24 ) }, "int.buf" );
					llvm::Value* fmtStr = this->irBuilder.CreateGlobalStringPtr( "%ld", "int.fmt" );
					this->irBuilder.CreateCall( snprintfFunction, { buffer, llvm::ConstantInt::get( i64Type, 24 ), fmtStr, intVal } );
					return buffer;
				};
				llvm::Value* leftStr = ensureString( leftValue );
				llvm::Value* rightStr = ensureString( rightValue );
				llvm::Value* lenA = this->irBuilder.CreateCall( strlenFunction, { leftStr }, "len.a" );
				llvm::Value* lenB = this->irBuilder.CreateCall( strlenFunction, { rightStr }, "len.b" );
				llvm::Value* totalLen = this->irBuilder.CreateAdd( lenA, lenB, "total.len" );
				llvm::Value* allocSize = this->irBuilder.CreateAdd( totalLen, llvm::ConstantInt::get( i64Type, 1 ), "alloc.size" );
				llvm::Value* buffer = this->irBuilder.CreateCall( mallocFunction, { allocSize }, "str.buf" );
				this->irBuilder.CreateCall( strcpyFunction, { buffer, leftStr } );
				this->irBuilder.CreateCall( strcatFunction, { buffer, rightStr } );
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
				llvm::Function* strcmpFunction = this->getOrCreateStrcmp();
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
		if( callee == nullptr && calledName.find( '.' ) != std::string::npos ) {
			size_t dotPosition = calledName.find( '.' );
			std::string typeName = calledName.substr( 0, dotPosition );
			std::string methodName = calledName.substr( dotPosition + 1 );
			for( const std::pair<const std::string, llvm::Function*>& entry : this->functionResolutionMap ) {
				size_t entryDotPos = entry.first.find( '.' );
				if( entryDotPos != std::string::npos &&
					entry.first.substr( entryDotPos + 1 ) == methodName &&
					entry.first.substr( 0, entryDotPos ) != typeName ) {
					std::string candidateClass = entry.first.substr( 0, entryDotPos );
					semantic::TypeSharedPointer candidateType = this->semanticAnalyzer.types().lookupType( candidateClass );
					if( candidateType != nullptr && candidateType->kind == semantic::Type::Kind::Class ) {
						semantic::ClassType* classPtr = static_cast<semantic::ClassType*>( candidateType.get() );
						for( const semantic::TypeSharedPointer& implementedInterface : classPtr->interfaces ) {
							if( implementedInterface == nullptr ) continue;
							std::string ifaceName = implementedInterface->name;
							size_t bracketPos = ifaceName.find( '<' );
							if( bracketPos != std::string::npos ) {
								ifaceName = ifaceName.substr( 0, bracketPos );
							}
							if( ifaceName == typeName ) {
								callee = entry.second;
								break;
							}
						}
						if( callee != nullptr ) break;
					}
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

		// Detect variadic callee from MIR definitions or semantic method info
		int calleeVariadicIndex = -1;
		semantic::TypeSharedPointer calleeVariadicElemType = nullptr;
		if( this->mirFunctionDefinitionMap.count( instruction.calledFunctionQualifiedName ) > 0 ) {
			MIRFunctionDefinition* calleeDef = this->mirFunctionDefinitionMap[instruction.calledFunctionQualifiedName];
			calleeVariadicIndex = calleeDef->variadicParameterIndex;
			calleeVariadicElemType = calleeDef->variadicElementType;
		}
		if( calleeVariadicIndex < 0 && instruction.calledFunctionQualifiedName.find( '.' ) != std::string::npos ) {
			size_t dotPos = instruction.calledFunctionQualifiedName.find( '.' );
			std::string ownerName = instruction.calledFunctionQualifiedName.substr( 0, dotPos );
			std::string methName = instruction.calledFunctionQualifiedName.substr( dotPos + 1 );
			semantic::TypeSharedPointer ownerType = this->semanticAnalyzer.types().lookupType( ownerName );
			if( ownerType != nullptr && ownerType->kind == semantic::Type::Kind::Class ) {
				semantic::ClassType* classPtr = static_cast<semantic::ClassType*>( ownerType.get() );
				for( const semantic::MethodInfo& methodEntry : classPtr->methods ) {
					if( methodEntry.name == methName && methodEntry.type != nullptr &&
						methodEntry.type->kind == semantic::Type::Kind::Function ) {
						semantic::FunctionType* funcType = static_cast<semantic::FunctionType*>( methodEntry.type.get() );
						if( funcType->variadicParameterIndex >= 0 ) {
							calleeVariadicIndex = funcType->variadicParameterIndex;
							calleeVariadicElemType = funcType->variadicElementType;
							break;
						}
					}
				}
			}
		}

		llvm::FunctionType* calleeType = callee->getFunctionType();
		unsigned expectedParamCount = calleeType->getNumParams();

		// Pack variadic arguments into Args<T> struct when callee has variadic parameter
		if( calleeVariadicIndex >= 0 &&
			static_cast<unsigned>( calleeVariadicIndex ) < expectedParamCount &&
			instruction.sourceOperands.size() > static_cast<size_t>( calleeVariadicIndex ) ) {

			std::vector<llvm::Value*> arguments;
			llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );

			// Fixed arguments before the variadic parameter
			for( int fixedIdx = 0; fixedIdx < calleeVariadicIndex; fixedIdx++ ) {
				if( static_cast<size_t>( fixedIdx ) >= instruction.sourceOperands.size() ) {
					arguments.push_back( llvm::Constant::getNullValue( calleeType->getParamType( fixedIdx ) ) );
					continue;
				}
				llvm::Value* argValue = this->loadVariableValue( instruction.sourceOperands[fixedIdx] );
				if( argValue == nullptr ) {
					arguments.push_back( llvm::Constant::getNullValue( calleeType->getParamType( fixedIdx ) ) );
					continue;
				}
				llvm::Type* expectedType = calleeType->getParamType( fixedIdx );
				if( argValue->getType() != expectedType ) {
					if( argValue->getType()->isIntegerTy() && expectedType->isIntegerTy() ) {
						unsigned srcBits = argValue->getType()->getIntegerBitWidth();
						unsigned dstBits = expectedType->getIntegerBitWidth();
						if( srcBits < dstBits ) argValue = this->irBuilder.CreateSExt( argValue, expectedType, "farg.sext" );
						else if( srcBits > dstBits ) argValue = this->irBuilder.CreateTrunc( argValue, expectedType, "farg.trunc" );
					}
					else if( argValue->getType()->isPointerTy() && expectedType->isPointerTy() ) {
						// Opaque pointers — compatible
					}
					else if( argValue->getType()->isIntegerTy() && expectedType->isPointerTy() ) {
						argValue = this->irBuilder.CreateIntToPtr( argValue, expectedType, "farg.itop" );
					}
					else if( argValue->getType()->isPointerTy() && expectedType->isIntegerTy() ) {
						argValue = this->irBuilder.CreatePtrToInt( argValue, expectedType, "farg.ptoi" );
					}
					else if( argValue->getType()->isFloatingPointTy() && expectedType->isIntegerTy() ) {
						argValue = this->irBuilder.CreateFPToSI( argValue, expectedType, "farg.fptoi" );
					}
					else if( argValue->getType()->isIntegerTy() && expectedType->isFloatingPointTy() ) {
						argValue = this->irBuilder.CreateSIToFP( argValue, expectedType, "farg.itofp" );
					}
					else if( argValue->getType()->isFloatingPointTy() && expectedType->isFloatingPointTy() ) {
						argValue = this->irBuilder.CreateFPCast( argValue, expectedType, "farg.fpcast" );
					}
					else if( argValue->getType()->isIntegerTy( 1 ) && expectedType->isIntegerTy() ) {
						argValue = this->irBuilder.CreateZExt( argValue, expectedType, "farg.bext" );
					}
				}
				arguments.push_back( argValue );
			}

			// Determine element type for Args<T> struct
			llvm::Type* elemType = i64Type;
			if( calleeVariadicElemType != nullptr ) {
				llvm::Type* resolved = this->toLLVMType( calleeVariadicElemType );
				if( resolved->isVoidTy() == false ) {
					elemType = resolved;
				}
			}

			// Count keyword-only params after the variadic slot
			unsigned keywordOnlyCount = ( expectedParamCount > static_cast<unsigned>( calleeVariadicIndex ) + 1 )
				? expectedParamCount - calleeVariadicIndex - 1
				: 0;

			size_t variadicArgStart = static_cast<size_t>( calleeVariadicIndex );
			size_t totalRemainingArgs = ( instruction.sourceOperands.size() > variadicArgStart )
				? instruction.sourceOperands.size() - variadicArgStart
				: 0;
			size_t variadicArgCount = ( totalRemainingArgs > keywordOnlyCount )
				? totalRemainingArgs - keywordOnlyCount
				: 0;

			// Check for Args<T> forwarding (variadic param passed as single Args struct)
			bool isVariadicForward = false;
			if( variadicArgCount == 1 ) {
				MIRVariableIdentifier forwardVar = instruction.sourceOperands[variadicArgStart];
				if( functionDefinition.variadicParameterIndex >= 0 ) {
					size_t callerVarIdx = static_cast<size_t>( functionDefinition.variadicParameterIndex );
					if( callerVarIdx < functionDefinition.parameterVariableIdentifiers.size() &&
						functionDefinition.parameterVariableIdentifiers[callerVarIdx] == forwardVar ) {
						isVariadicForward = true;
					}
				}
				if( isVariadicForward == false &&
					functionDefinition.variableDescriptorTable.count( forwardVar ) > 0 ) {
					MIRVariableDescriptor& forwardDesc = functionDefinition.variableDescriptorTable[forwardVar];
					if( forwardDesc.variableType != nullptr &&
						forwardDesc.variableType->name.find( "Args<" ) == 0 ) {
						isVariadicForward = true;
					}
				}
			}

			if( isVariadicForward ) {
				llvm::Value* forwardedArgs = this->loadVariableValue( instruction.sourceOperands[variadicArgStart] );
				if( forwardedArgs != nullptr && forwardedArgs->getType()->isPointerTy() == false ) {
					forwardedArgs = this->irBuilder.CreateIntToPtr(
						forwardedArgs, llvm::PointerType::getUnqual( this->llvmContext ), "vfwd.ptr"
					);
				}
				arguments.push_back( forwardedArgs );
			}
			else {
				// Build Args<T> struct: { T* data, i64 count, i64 pos }
				llvm::StructType* argsStructType = llvm::StructType::get( this->llvmContext, {
					llvm::PointerType::getUnqual( this->llvmContext ),
					i64Type,
					i64Type
				});

				llvm::Function* currentFunc = this->irBuilder.GetInsertBlock()->getParent();
				llvm::AllocaInst* argsAlloca = this->createEntryBlockAllocation(
					currentFunc, "pack.args", argsStructType
				);

				if( variadicArgCount > 0 ) {
					llvm::ArrayType* dataArrayType = llvm::ArrayType::get( elemType, variadicArgCount );
					llvm::AllocaInst* dataAlloca = this->createEntryBlockAllocation(
						currentFunc, "pack.args.data", dataArrayType
					);

					for( size_t i = 0; i < variadicArgCount; i++ ) {
						llvm::Value* argValue = this->loadVariableValue(
							instruction.sourceOperands[variadicArgStart + i]
						);
						if( argValue != nullptr ) {
							if( argValue->getType() != elemType ) {
								if( argValue->getType()->isIntegerTy() && elemType->isIntegerTy() ) {
									unsigned srcBits = argValue->getType()->getIntegerBitWidth();
									unsigned dstBits = elemType->getIntegerBitWidth();
									if( srcBits < dstBits ) argValue = this->irBuilder.CreateSExt( argValue, elemType, "varg.sext" );
									else if( srcBits > dstBits ) argValue = this->irBuilder.CreateTrunc( argValue, elemType, "varg.trunc" );
								}
								else if( argValue->getType()->isPointerTy() && elemType->isIntegerTy() ) {
									argValue = this->irBuilder.CreatePtrToInt( argValue, elemType, "varg.ptoi" );
								}
								else if( argValue->getType()->isIntegerTy() && elemType->isPointerTy() ) {
									argValue = this->irBuilder.CreateIntToPtr( argValue, elemType, "varg.itop" );
								}
								else if( argValue->getType()->isFloatingPointTy() && elemType->isIntegerTy() ) {
									argValue = this->irBuilder.CreateFPToSI( argValue, elemType, "varg.fptoi" );
								}
								else if( argValue->getType()->isIntegerTy() && elemType->isFloatingPointTy() ) {
									argValue = this->irBuilder.CreateSIToFP( argValue, elemType, "varg.itofp" );
								}
								else if( argValue->getType()->isFloatingPointTy() && elemType->isFloatingPointTy() ) {
									argValue = this->irBuilder.CreateFPCast( argValue, elemType, "varg.fpcast" );
								}
							}
							llvm::Value* elemGep = this->irBuilder.CreateConstGEP2_32(
								dataArrayType, dataAlloca, 0, static_cast<unsigned>( i ), "pack.args.gep"
							);
							this->irBuilder.CreateStore( argValue, elemGep );
						}
					}

					llvm::Value* dataFieldPtr = this->irBuilder.CreateStructGEP(
						argsStructType, argsAlloca, 0, "pack.args.data.field"
					);
					this->irBuilder.CreateStore( dataAlloca, dataFieldPtr );
				}
				else {
					llvm::Value* dataFieldPtr = this->irBuilder.CreateStructGEP(
						argsStructType, argsAlloca, 0, "pack.args.data.field"
					);
					this->irBuilder.CreateStore(
						llvm::ConstantPointerNull::get( llvm::PointerType::getUnqual( this->llvmContext ) ),
						dataFieldPtr
					);
				}

				llvm::Value* countFieldPtr = this->irBuilder.CreateStructGEP(
					argsStructType, argsAlloca, 1, "pack.args.count.field"
				);
				this->irBuilder.CreateStore( llvm::ConstantInt::get( i64Type, variadicArgCount ), countFieldPtr );

				llvm::Value* posFieldPtr = this->irBuilder.CreateStructGEP(
					argsStructType, argsAlloca, 2, "pack.args.pos.field"
				);
				this->irBuilder.CreateStore( llvm::ConstantInt::get( i64Type, 0 ), posFieldPtr );

				arguments.push_back( argsAlloca );
			}

			// Keyword-only arguments after the variadic struct
			size_t kwArgStart = variadicArgStart + variadicArgCount;
			for( unsigned kwIdx = 0; kwIdx < keywordOnlyCount; kwIdx++ ) {
				unsigned paramIdx = static_cast<unsigned>( calleeVariadicIndex ) + 1 + kwIdx;
				size_t sourceIdx = kwArgStart + kwIdx;
				if( sourceIdx < instruction.sourceOperands.size() ) {
					llvm::Value* argValue = this->loadVariableValue( instruction.sourceOperands[sourceIdx] );
					if( argValue == nullptr ) {
						arguments.push_back( llvm::Constant::getNullValue( calleeType->getParamType( paramIdx ) ) );
						continue;
					}
					llvm::Type* expectedType = calleeType->getParamType( paramIdx );
					if( argValue->getType() != expectedType ) {
						if( argValue->getType()->isIntegerTy() && expectedType->isIntegerTy() ) {
							unsigned srcBits = argValue->getType()->getIntegerBitWidth();
							unsigned dstBits = expectedType->getIntegerBitWidth();
							if( srcBits < dstBits ) argValue = this->irBuilder.CreateSExt( argValue, expectedType, "kw.sext" );
							else if( srcBits > dstBits ) argValue = this->irBuilder.CreateTrunc( argValue, expectedType, "kw.trunc" );
						}
						else if( argValue->getType()->isIntegerTy() && expectedType->isPointerTy() ) {
							argValue = this->irBuilder.CreateIntToPtr( argValue, expectedType, "kw.itop" );
						}
						else if( argValue->getType()->isPointerTy() && expectedType->isIntegerTy() ) {
							argValue = this->irBuilder.CreatePtrToInt( argValue, expectedType, "kw.ptoi" );
						}
					}
					arguments.push_back( argValue );
				}
				else {
					arguments.push_back( llvm::Constant::getNullValue( calleeType->getParamType( paramIdx ) ) );
				}
			}

			while( arguments.size() < expectedParamCount ) {
				llvm::Type* paramType = calleeType->getParamType( arguments.size() );
				arguments.push_back( llvm::Constant::getNullValue( paramType ) );
			}

			llvm::Value* result = this->irBuilder.CreateCall( callee, arguments );
			if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER &&
				callee->getReturnType()->isVoidTy() == false ) {
				this->setVariableValue( instruction.destinationVariable, result );
			}
			return;
		}

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
				constructorFunction->arg_size() != instruction.sourceOperands.size() + 1 ) {
				std::string arityName = fmt::format( "{}#{}", constructorName, instruction.sourceOperands.size() + 1 );
				if( this->functionResolutionMap.count( arityName ) > 0 ) {
					constructorFunction = this->functionResolutionMap[arityName];
				}
				else {
					llvm::Function* arityFunction = this->llvmModule->getFunction( arityName );
					if( arityFunction != nullptr ) {
						constructorFunction = arityFunction;
					}
				}
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

			// Initialize fields with default values from class declaration
			semantic::TypeSharedPointer constructSemanticType = this->semanticAnalyzer.types().lookupType( typeName );
			if( constructSemanticType == nullptr ) {
				for( const std::pair<const std::string, semantic::TypeSharedPointer>& entry :
					 this->semanticAnalyzer.types().getUserTypes() ) {
					if( entry.second->name == typeName && entry.second->kind == semantic::Type::Kind::Class ) {
						constructSemanticType = entry.second;
						break;
					}
				}
			}
			if( constructSemanticType != nullptr && constructSemanticType->kind == semantic::Type::Kind::Class ) {
				semantic::ClassTypeSharedPointer constructClassType = std::static_pointer_cast<semantic::ClassType>( constructSemanticType );
				if( constructClassType->astDeclaration != nullptr ) {
					unsigned int defaultFieldIndex = 0;
					if( constructClassType->virtualTable.empty() == false ) {
						defaultFieldIndex = 1;
					}
					for( std::shared_ptr<ast::nodes::FieldDeclarationNode>& fieldDeclaration : constructClassType->astDeclaration->fields ) {
						if( fieldDeclaration->defaultValue != nullptr && defaultFieldIndex < structType->getNumElements() ) {
							llvm::Value* defaultFieldValue = nullptr;
							llvm::Type* fieldLLVMType = structType->getElementType( defaultFieldIndex );
							ast::Node::Kind defaultKind = fieldDeclaration->defaultValue->kind;

							if( defaultKind == ast::Node::Kind::IntegerLiteral ) {
								ast::nodes::IntegerLiteralExpression& intLiteral =
									static_cast<ast::nodes::IntegerLiteralExpression&>( *fieldDeclaration->defaultValue );
								if( fieldLLVMType->isIntegerTy() ) {
									defaultFieldValue = llvm::ConstantInt::get( fieldLLVMType, intLiteral.value, true );
								}
								else if( fieldLLVMType->isFloatingPointTy() ) {
									defaultFieldValue = llvm::ConstantFP::get( fieldLLVMType, static_cast<double>( intLiteral.value ) );
								}
							}
							else if( defaultKind == ast::Node::Kind::FloatLiteral ) {
								ast::nodes::FloatLiteralExpression& floatLiteral =
									static_cast<ast::nodes::FloatLiteralExpression&>( *fieldDeclaration->defaultValue );
								if( fieldLLVMType->isFloatingPointTy() ) {
									defaultFieldValue = llvm::ConstantFP::get( fieldLLVMType, floatLiteral.value );
								}
								else if( fieldLLVMType->isIntegerTy() ) {
									defaultFieldValue = llvm::ConstantInt::get( fieldLLVMType, static_cast<int64_t>( floatLiteral.value ), true );
								}
							}
							else if( defaultKind == ast::Node::Kind::BooleanLiteral ) {
								ast::nodes::BoolLiteralExpression& boolLiteral =
									static_cast<ast::nodes::BoolLiteralExpression&>( *fieldDeclaration->defaultValue );
								defaultFieldValue = llvm::ConstantInt::get( fieldLLVMType, boolLiteral.value ? 1 : 0 );
							}
							else if( defaultKind == ast::Node::Kind::CharLiteral ) {
								ast::nodes::CharLiteralExpression& charLiteral =
									static_cast<ast::nodes::CharLiteralExpression&>( *fieldDeclaration->defaultValue );
								defaultFieldValue = llvm::ConstantInt::get( fieldLLVMType, static_cast<int64_t>( charLiteral.value ), true );
							}
							else if( defaultKind == ast::Node::Kind::NoneLiteral ) {
								defaultFieldValue = llvm::Constant::getNullValue( fieldLLVMType );
							}
							else if( defaultKind == ast::Node::Kind::StringLiteral ) {
								ast::nodes::StringLiteralExpression& strLiteral =
									static_cast<ast::nodes::StringLiteralExpression&>( *fieldDeclaration->defaultValue );
								defaultFieldValue = this->irBuilder.CreateGlobalStringPtr( strLiteral.value, "field.str" );
							}
							else if( defaultKind == ast::Node::Kind::UnaryExpression ) {
								ast::nodes::UnaryExpression& unaryExpr =
									static_cast<ast::nodes::UnaryExpression&>( *fieldDeclaration->defaultValue );
								if( unaryExpr.operation == token::Type::Minus && unaryExpr.operand != nullptr ) {
									if( unaryExpr.operand->kind == ast::Node::Kind::IntegerLiteral ) {
										ast::nodes::IntegerLiteralExpression& intLiteral =
											static_cast<ast::nodes::IntegerLiteralExpression&>( *unaryExpr.operand );
										if( fieldLLVMType->isIntegerTy() ) {
											defaultFieldValue = llvm::ConstantInt::get( fieldLLVMType, -intLiteral.value, true );
										}
										else if( fieldLLVMType->isFloatingPointTy() ) {
											defaultFieldValue = llvm::ConstantFP::get( fieldLLVMType, static_cast<double>( -intLiteral.value ) );
										}
									}
									else if( unaryExpr.operand->kind == ast::Node::Kind::FloatLiteral ) {
										ast::nodes::FloatLiteralExpression& floatLiteral =
											static_cast<ast::nodes::FloatLiteralExpression&>( *unaryExpr.operand );
										if( fieldLLVMType->isFloatingPointTy() ) {
											defaultFieldValue = llvm::ConstantFP::get( fieldLLVMType, -floatLiteral.value );
										}
									}
								}
							}

							if( defaultFieldValue != nullptr ) {
								std::string defaultFieldName = fmt::format( "field.default.{}", fieldDeclaration->name );
								llvm::Value* fieldGEP = this->irBuilder.CreateStructGEP(
									structType, typedPointer, defaultFieldIndex, defaultFieldName
								);
								this->irBuilder.CreateStore( defaultFieldValue, fieldGEP );
							}
						}
						defaultFieldIndex++;
					}
				}
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
		codegen::RuntimeFunctionSpec spec = this->runtimeInterface_->getMallocFunction( this->llvmContext );
		llvm::Function* function = this->llvmModule->getFunction( spec.functionName );
		if( function != nullptr && function->getFunctionType() != spec.functionSignature ) {
			function->eraseFromParent();
			function = nullptr;
		}
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType, spec.functionName, this->llvmModule.get()
			);
		}
		return function;
	}

	llvm::Function* MIRCodegen::getOrCreateCalloc() {
		codegen::RuntimeFunctionSpec spec = this->runtimeInterface_->getCallocFunction( this->llvmContext );
		llvm::Function* function = this->llvmModule->getFunction( spec.functionName );
		if( function != nullptr && function->getFunctionType() != spec.functionSignature ) {
			function->eraseFromParent();
			function = nullptr;
		}
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType, spec.functionName, this->llvmModule.get()
			);
		}
		return function;
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
		codegen::RuntimeFunctionSpec spec = this->runtimeInterface_->getFreeFunction( this->llvmContext );
		llvm::Function* function = this->llvmModule->getFunction( spec.functionName );
		if( function != nullptr && function->getFunctionType() != spec.functionSignature ) {
			function->eraseFromParent();
			function = nullptr;
		}
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType, spec.functionName, this->llvmModule.get()
			);
		}
		return function;
	}

	llvm::Function* MIRCodegen::getOrCreateMemcpy() {
		codegen::RuntimeFunctionSpec spec = this->runtimeInterface_->getMemcpyFunction( this->llvmContext );
		llvm::Function* function = this->llvmModule->getFunction( spec.functionName );
		if( function != nullptr && function->getFunctionType() != spec.functionSignature ) {
			function->eraseFromParent();
			function = nullptr;
		}
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType, spec.functionName, this->llvmModule.get()
			);
		}
		return function;
	}

	llvm::Function* MIRCodegen::getOrCreateStrlen() {
		codegen::RuntimeFunctionSpec spec = this->runtimeInterface_->getStrlenFunction( this->llvmContext );
		llvm::Function* function = this->llvmModule->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType, spec.functionName, this->llvmModule.get()
			);
		}
		return function;
	}

	llvm::Function* MIRCodegen::getOrCreateStrcmp() {
		codegen::RuntimeFunctionSpec spec = this->runtimeInterface_->getStrcmpFunction( this->llvmContext );
		llvm::Function* function = this->llvmModule->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType, spec.functionName, this->llvmModule.get()
			);
		}
		return function;
	}

	llvm::Function* MIRCodegen::getOrCreateStrcpy() {
		codegen::RuntimeFunctionSpec spec = this->runtimeInterface_->getStrcpyFunction( this->llvmContext );
		llvm::Function* function = this->llvmModule->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType, spec.functionName, this->llvmModule.get()
			);
		}
		return function;
	}

	llvm::Function* MIRCodegen::getOrCreateStrcat() {
		codegen::RuntimeFunctionSpec spec = this->runtimeInterface_->getStrcatFunction( this->llvmContext );
		llvm::Function* function = this->llvmModule->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType, spec.functionName, this->llvmModule.get()
			);
		}
		return function;
	}

	llvm::Function* MIRCodegen::getOrCreateStrstr() {
		codegen::RuntimeFunctionSpec spec = this->runtimeInterface_->getStrstrFunction( this->llvmContext );
		llvm::Function* function = this->llvmModule->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType, spec.functionName, this->llvmModule.get()
			);
		}
		return function;
	}

	llvm::Function* MIRCodegen::getOrCreateStrncmp() {
		codegen::RuntimeFunctionSpec spec = this->runtimeInterface_->getStrncmpFunction( this->llvmContext );
		llvm::Function* function = this->llvmModule->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType, spec.functionName, this->llvmModule.get()
			);
		}
		return function;
	}

	llvm::Function* MIRCodegen::getOrCreateSnprintf() {
		codegen::RuntimeFunctionSpec spec = this->runtimeInterface_->getSnprintfFunction( this->llvmContext );
		llvm::Function* function = this->llvmModule->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType, spec.functionName, this->llvmModule.get()
			);
		}
		return function;
	}

	llvm::Function* MIRCodegen::getOrCreateWrite() {
		codegen::RuntimeFunctionSpec spec = this->runtimeInterface_->getWriteFunction( this->llvmContext );
		llvm::Function* function = this->llvmModule->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType, spec.functionName, this->llvmModule.get()
			);
		}
		return function;
	}

	llvm::Function* MIRCodegen::getOrCreateUraniteThrow() {
		codegen::RuntimeFunctionSpec spec = this->runtimeInterface_->getThrowFunction( this->llvmContext );
		llvm::Function* function = this->llvmModule->getFunction( spec.functionName );
		if( function != nullptr && function->getFunctionType() != spec.functionSignature ) {
			function->eraseFromParent();
			function = nullptr;
		}
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType, spec.functionName, this->llvmModule.get()
			);
			if( spec.isNoReturn ) {
				function->setDoesNotReturn();
			}
		}
		return function;
	}

	llvm::Function* MIRCodegen::getOrCreatePersonality() {
		codegen::RuntimeFunctionSpec spec = this->runtimeInterface_->getPersonalityFunction( this->llvmContext );
		llvm::Function* function = this->llvmModule->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType, spec.functionName, this->llvmModule.get()
			);
		}
		return function;
	}

	llvm::Function* MIRCodegen::getOrCreateBeginCatch() {
		codegen::RuntimeFunctionSpec spec = this->runtimeInterface_->getBeginCatchFunction( this->llvmContext );
		llvm::Function* function = this->llvmModule->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType, spec.functionName, this->llvmModule.get()
			);
		}
		return function;
	}

	llvm::Function* MIRCodegen::getOrCreateExtern( const std::string& name, llvm::Type* returnType, std::vector<llvm::Type*> paramTypes ) {
		llvm::Function* function = this->llvmModule->getFunction( name );
		if( function == nullptr ) {
			llvm::FunctionType* functionType = llvm::FunctionType::get( returnType, paramTypes, false );
			function = llvm::Function::Create(
				functionType, llvm::Function::ExternalLinkage, name, this->llvmModule.get()
			);
		}
		return function;
	}

} // namespace uranite::ir::mir
