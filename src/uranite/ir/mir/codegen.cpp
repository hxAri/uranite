
//
// @author hxAri (hxari)
// @create 13-06-2026
// @github https://github.com/uranite-lang/uranite
//
// Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
// Uranite Licence under GNU General Public Licence v3
//

#include <fmt/format.h>
#include <set>
#include <unordered_set>

#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetOptions.h>

#include "uranite/ir/mir/codegen.hpp"
#include "uranite/semantic/qualnames.hpp"

namespace uranite::ir::mir {
	
	MIRCodegen::MIRCodegen( semantic::Analyzer& semanticAnalyzer, diagnostic::Engine& diagnosticEngine )
		: semanticAnalyzer( semanticAnalyzer ),
		  diagnosticEngine( diagnosticEngine ),
		  runtimeInterface_( std::make_shared<codegen::DefaultRuntime>() ),
		  irBuilder( this->llvmContext ) {
		this->builtinRegistry.registerAllTypes();
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
		this->interfaceTableMap.clear();
		this->interfaceMethodOrder.clear();
		this->classesWithVtable.clear();
		this->interfacesWithDirectItable.clear();
		this->currentMIRModule = &mirModule;
		this->mirFunctionDefinitionMap.clear();
		this->functionReturnConcreteClass.clear();
		for( std::shared_ptr<MIRFunctionDefinition>& funcDef : mirModule.functionDefinitions ) {
			if( funcDef == nullptr ) continue;
			std::string mapKey = funcDef->functionName;
			if( funcDef->ownerClassQualifiedName.empty() == false ) {
				mapKey = funcDef->ownerClassQualifiedName + "." + funcDef->functionName;
			}
			else if( funcDef->mangledFunctionName.empty() == false ) {
				mapKey = funcDef->mangledFunctionName;
			}
			this->mirFunctionDefinitionMap[mapKey] = funcDef.get();
			if( funcDef->ownerClassQualifiedName.empty() && funcDef->mangledFunctionName.empty() == false ) {
				this->mirFunctionDefinitionMap[funcDef->functionName] = funcDef.get();
			}
			std::unordered_map<MIRVariableIdentifier, std::string> varConcreteClass;
			for( const std::shared_ptr<MIRBasicBlock>& block : funcDef->controlFlowBlocks ) {
				if( block == nullptr ) continue;
				for( const MIRInstruction& instr : block->blockInstructions ) {
					if( instr.instructionKind == MIRInstructionKind::ConstructObject &&
						instr.destinationVariable != INVALID_VARIABLE_IDENTIFIER &&
						instr.calledFunctionQualifiedName.empty() == false ) {
						varConcreteClass[instr.destinationVariable] = instr.calledFunctionQualifiedName;
					}
				}
			}
			bool propagationChanged = true;
			int propagationLimit = 10;
			while( propagationChanged && propagationLimit-- > 0 ) {
				propagationChanged = false;
				for( const std::shared_ptr<MIRBasicBlock>& block : funcDef->controlFlowBlocks ) {
					if( block == nullptr ) continue;
					for( const MIRInstruction& instr : block->blockInstructions ) {
						if( ( instr.instructionKind == MIRInstructionKind::StoreVariable ||
							  instr.instructionKind == MIRInstructionKind::LoadVariable ) &&
							instr.destinationVariable != INVALID_VARIABLE_IDENTIFIER &&
							instr.sourceOperands.empty() == false ) {
							MIRVariableIdentifier source = instr.sourceOperands[0];
							MIRVariableIdentifier dest = instr.destinationVariable;
							if( varConcreteClass.count( source ) > 0 &&
								varConcreteClass[source].empty() == false ) {
								bool skipPropagation = false;
								if( funcDef->variableDescriptorTable.count( dest ) > 0 ) {
									semantic::TypeSharedPointer destType =
										funcDef->variableDescriptorTable[dest].variableType;
									if( destType != nullptr && ( destType->kind == semantic::Type::Kind::Interface ||
										destType->kind == semantic::Type::Kind::GenericParameter ) ) {
										skipPropagation = true;
									}
								}
								if( skipPropagation == false ) {
									std::string sourceClass = varConcreteClass[source];
									if( varConcreteClass.count( dest ) == 0 ) {
										varConcreteClass[dest] = sourceClass;
										propagationChanged = true;
									}
									else if( varConcreteClass[dest].empty() == false &&
											 varConcreteClass[dest] != sourceClass ) {
										varConcreteClass[dest] = "";
										propagationChanged = true;
									}
								}
							}
						}
					}
				}
			}
			std::string returnClassName;
			bool consistent = true;
			for( const std::shared_ptr<MIRBasicBlock>& block : funcDef->controlFlowBlocks ) {
				if( block == nullptr ) continue;
				for( const MIRInstruction& instr : block->blockInstructions ) {
					if( instr.instructionKind == MIRInstructionKind::ReturnValue &&
						instr.sourceOperands.empty() == false ) {
						MIRVariableIdentifier retVar = instr.sourceOperands[0];
						std::string foundClass;
						if( varConcreteClass.count( retVar ) > 0 ) {
							foundClass = varConcreteClass[retVar];
						}
						if( foundClass.empty() ) {
							consistent = false;
							break;
						}
						if( returnClassName.empty() ) {
							returnClassName = foundClass;
						}
						else if( returnClassName != foundClass ) {
							consistent = false;
							break;
						}
					}
				}
				if( consistent == false ) break;
			}
			if( consistent && returnClassName.empty() == false ) {
				this->functionReturnConcreteClass[mapKey] = returnClassName;
			}
		}
		std::set<std::string> processedLayouts;
		for( const std::pair<const std::string, TypeLayoutDescriptor>& typeEntry : mirModule.typeLayoutTable ) {
			const std::string& typeName = typeEntry.first;
			const TypeLayoutDescriptor& typeLayout = typeEntry.second;
			std::string canonicalName = typeLayout.typeQualifiedName.empty() == false
				? typeLayout.typeQualifiedName : typeName;
			if( processedLayouts.count( canonicalName ) > 0 ) {
				if( this->structTypeCache.count( canonicalName ) > 0 ) {
					this->structTypeCache[typeName] = this->structTypeCache[canonicalName];
				}
				continue;
			}
			processedLayouts.insert( canonicalName );
			llvm::StructType* existingType = llvm::StructType::getTypeByName( this->llvmContext, canonicalName );
			if( existingType != nullptr ) {
				this->structTypeCache[canonicalName] = existingType;
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
				this->llvmContext, fieldLLVMTypes, canonicalName
			);
			this->structTypeCache[canonicalName] = structType;
			if( typeName != canonicalName ) {
				this->structTypeCache[typeName] = structType;
			}
		}
		for( const std::pair<const std::string, TypeLayoutDescriptor>& typeEntry : mirModule.typeLayoutTable ) {
			if( typeEntry.second.hasVirtualTable ) {
				this->classesWithVtable.insert( typeEntry.first );
				this->classesWithVtable.insert( typeEntry.second.typeQualifiedName );
			}
		}
		for( const std::pair<const std::string, semantic::TypeSharedPointer>& entry : this->semanticAnalyzer.types().getUserTypes() ) {
			if( entry.second != nullptr && entry.second->kind == semantic::Type::Kind::Interface ) {
				semantic::InterfaceType* interfaceType = static_cast<semantic::InterfaceType*>( entry.second.get() );
				std::string baseRegistryKey = entry.first;
				size_t regBracket = baseRegistryKey.find( '<' );
				if( regBracket != std::string::npos ) {
					baseRegistryKey = baseRegistryKey.substr( 0, regBracket );
				}
				std::string baseInterfaceName = interfaceType->name;
				size_t nameBracket = baseInterfaceName.find( '<' );
				if( nameBracket != std::string::npos ) {
					baseInterfaceName = baseInterfaceName.substr( 0, nameBracket );
				}
				std::vector<std::string> resolvedMethodOrder;
				if( interfaceType->methodOrder.empty() == false ) {
					resolvedMethodOrder = interfaceType->methodOrder;
				}
				else {
					for( const semantic::MethodInfo& method : interfaceType->methods ) {
						resolvedMethodOrder.push_back( method.name );
					}
				}
				if( resolvedMethodOrder.empty() == false ) {
					bool hasToStringSlot = false;
					for( const std::string& existingMethod : resolvedMethodOrder ) {
						if( existingMethod == semantic::qualname::classes::object::methods::ToString ) {
							hasToStringSlot = true;
							break;
						}
					}
					if( hasToStringSlot == false ) {
						resolvedMethodOrder.insert( resolvedMethodOrder.begin(),
							semantic::qualname::classes::object::methods::ToString );
					}
					this->interfaceMethodOrder[baseRegistryKey] = resolvedMethodOrder;
					this->interfaceMethodOrder[baseInterfaceName] = resolvedMethodOrder;
				}
			}
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
		this->externDeclaredNames.clear();
		for( const MIRExternFunction& externFunc : mirModule.externFunctions ) {
			std::string externName = externFunc.linkageName;
			if( externName.empty() || this->functionResolutionMap.count( externName ) > 0 ) {
				continue;
			}
			std::vector<llvm::Type*> paramTypes;
			for( const semantic::TypeSharedPointer& paramType : externFunc.parameterTypes ) {
				if( paramType == nullptr ) {
					paramTypes.push_back( llvm::PointerType::getUnqual( this->llvmContext ) );
					continue;
				}
				llvm::Type* llvmParamType = this->toLLVMType( paramType );
				paramTypes.push_back( llvmParamType );
			}
			llvm::Type* returnType = llvm::Type::getVoidTy( this->llvmContext );
			if( externFunc.returnType != nullptr ) {
				returnType = this->toLLVMType( externFunc.returnType );
			}
			llvm::FunctionType* externType = llvm::FunctionType::get( returnType, paramTypes, externFunc.isVariadic );
			llvm::Function* externFunction = llvm::Function::Create(
				externType, llvm::Function::ExternalLinkage, externName, this->llvmModule.get()
			);
			this->functionResolutionMap[externName] = externFunction;
			this->externDeclaredNames.insert( externName );
			if( externFunc.functionName.empty() == false && externFunc.functionName != externName ) {
				this->functionResolutionMap[externFunc.functionName] = externFunction;
				this->externDeclaredNames.insert( externFunc.functionName );
			}
		}
		for( std::shared_ptr<MIRFunctionDefinition>& functionDefinition : mirModule.functionDefinitions ) {
			if( functionDefinition == nullptr ) {
				continue;
			}
			std::string llvmFunctionName = functionDefinition->functionName;
			if( functionDefinition->ownerClassQualifiedName.empty() == false ) {
				llvmFunctionName = functionDefinition->ownerClassQualifiedName + "." + functionDefinition->functionName;
			}
			else if( functionDefinition->mangledFunctionName.empty() == false ) {
				llvmFunctionName = functionDefinition->mangledFunctionName;
			}
			if( this->functionResolutionMap.count( llvmFunctionName ) > 0 ) {
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
			bool isMainFunction = ( llvmFunctionName == semantic::qualname::functions::main::Name &&
				functionDefinition->ownerClassQualifiedName.empty() );
			semantic::TypeSharedPointer returnTypeDesc = functionDefinition->returnTypeDescriptor != nullptr
				? functionDefinition->returnTypeDescriptor
				: std::make_shared<semantic::Type>( semantic::Type::Kind::Void, semantic::qualname::classes::Void::Name );
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
				else if( functionDefinition->keywordParameterIndex >= 0 &&
					paramIdx == static_cast<unsigned>( functionDefinition->keywordParameterIndex ) ) {
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
				if( functionDefinition->ownerClassQualifiedName.empty() == false &&
					functionDefinition->isStaticMethod == false && parameterTypes.empty() ) {
					parameterType = llvm::PointerType::getUnqual( this->llvmContext );
				}
				parameterTypes.push_back( parameterType );
			}
			llvm::FunctionType* functionType = llvm::FunctionType::get( returnType, parameterTypes, false );
			llvm::Function* llvmFunction = llvm::Function::Create(
				functionType, llvm::Function::ExternalLinkage,
				llvmFunctionName, this->llvmModule.get()
			);
			llvmFunction->setPersonalityFn( this->getOrCreatePersonality() );
			this->functionResolutionMap[llvmFunctionName] = llvmFunction;
			if( functionDefinition->ownerClassQualifiedName.empty() ) {
				if( this->functionResolutionMap.count( functionDefinition->functionName ) == 0 ) {
					this->functionResolutionMap[functionDefinition->functionName] = llvmFunction;
				}
			}
		}
		
		std::string objectToStringQualified = semantic::qualname::classes::object::Qualified + "." + semantic::qualname::classes::object::methods::ToString;
		if( this->llvmModule->getFunction( objectToStringQualified ) == nullptr ) {
			llvm::Type* pointerType = llvm::PointerType::getUnqual( this->llvmContext );
			llvm::FunctionType* toStringType = llvm::FunctionType::get( pointerType, { pointerType }, false );
			llvm::Function* toStringFunction = llvm::Function::Create(
				toStringType, llvm::Function::ExternalLinkage, objectToStringQualified, this->llvmModule.get()
			);
			llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(
				this->llvmContext, "entry", toStringFunction
			);
			llvm::IRBuilder<> stubBuilder( this->llvmContext );
			stubBuilder.SetInsertPoint( entryBlock );
			llvm::Value* selfArg = toStringFunction->getArg( 0 );
			llvm::Value* isNull = stubBuilder.CreateICmpEQ(
				selfArg,
				llvm::ConstantPointerNull::get( llvm::cast<llvm::PointerType>( pointerType ) ),
				"tostr.is.none"
			);
			llvm::Value* noneStr = stubBuilder.CreateGlobalStringPtr( "None", "str.none" );
			llvm::Value* result = stubBuilder.CreateSelect( isNull, noneStr, selfArg, "tostr.result" );
			stubBuilder.CreateRet( result );
			this->functionResolutionMap[objectToStringQualified] = toStringFunction;
		}
		for( const std::pair<const std::string, semantic::TypeSharedPointer>& entry : this->semanticAnalyzer.types().getUserTypes() ) {
			if( entry.second == nullptr || entry.second->kind != semantic::Type::Kind::Class ) {
				continue;
			}
			semantic::ClassType* toStringClassType = static_cast<semantic::ClassType*>( entry.second.get() );
			bool classHasToString = false;
			semantic::ClassType* toStringWalk = toStringClassType;
			while( toStringWalk != nullptr ) {
				if( toStringWalk->findMethod( semantic::qualname::classes::object::methods::ToString ) != nullptr ) {
					classHasToString = true;
					break;
				}
				if( toStringWalk->baseClass == nullptr || toStringWalk->baseClass->kind != semantic::Type::Kind::Class ) {
					break;
				}
				semantic::ClassType* toStringParent = static_cast<semantic::ClassType*>( toStringWalk->baseClass.get() );
				if( toStringParent->qualified == semantic::qualname::classes::object::Qualified ) {
					break;
				}
				toStringWalk = toStringParent;
			}
			if( classHasToString ) {
				continue;
			}
			std::string toStringClassQualified = toStringClassType->qualified.empty() == false
				? toStringClassType->qualified : toStringClassType->name;
			std::string toStringClassBase = toStringClassQualified;
			{
				size_t bracketPos = toStringClassBase.find( '<' );
				if( bracketPos != std::string::npos ) {
					toStringClassBase = toStringClassBase.substr( 0, bracketPos );
				}
			}
			std::string perClassToStringName = toStringClassBase + "." + semantic::qualname::classes::object::methods::ToString;
			if( this->functionResolutionMap.count( perClassToStringName ) > 0 ||
				this->llvmModule->getFunction( perClassToStringName ) != nullptr ) {
				continue;
			}
			llvm::Type* toStringPointerType = llvm::PointerType::getUnqual( this->llvmContext );
			llvm::Type* toStringI64Type = llvm::Type::getInt64Ty( this->llvmContext );
			llvm::FunctionType* perClassToStringType = llvm::FunctionType::get( toStringPointerType, { toStringPointerType }, false );
			llvm::Function* perClassToStringFunc = llvm::Function::Create(
				perClassToStringType, llvm::Function::ExternalLinkage, perClassToStringName, this->llvmModule.get()
			);
			llvm::BasicBlock* toStringEntryBlock = llvm::BasicBlock::Create( this->llvmContext, "entry", perClassToStringFunc );
			llvm::BasicBlock* toStringNullBlock = llvm::BasicBlock::Create( this->llvmContext, "is.null", perClassToStringFunc );
			llvm::BasicBlock* toStringReprBlock = llvm::BasicBlock::Create( this->llvmContext, "repr", perClassToStringFunc );
			llvm::IRBuilder<> toStringBuilder( this->llvmContext );
			toStringBuilder.SetInsertPoint( toStringEntryBlock );
			llvm::Value* toStringSelfArg = perClassToStringFunc->getArg( 0 );
			llvm::Value* toStringIsNull = toStringBuilder.CreateICmpEQ(
				toStringSelfArg,
				llvm::ConstantPointerNull::get( llvm::cast<llvm::PointerType>( toStringPointerType ) ),
				"tostr.is.none"
			);
			toStringBuilder.CreateCondBr( toStringIsNull, toStringNullBlock, toStringReprBlock );
			toStringBuilder.SetInsertPoint( toStringNullBlock );
			llvm::Value* toStringNoneStr = toStringBuilder.CreateGlobalStringPtr( "None", "str.none" );
			toStringBuilder.CreateRet( toStringNoneStr );
			toStringBuilder.SetInsertPoint( toStringReprBlock );
			std::string reprFormatString = fmt::format( "<{} at 0x%lx>", toStringClassQualified );
			size_t reprBufferSize = reprFormatString.size() + 20;
			llvm::FunctionCallee toStringSnprintf = this->llvmModule->getOrInsertFunction(
				"snprintf",
				llvm::FunctionType::get( llvm::Type::getInt32Ty( this->llvmContext ), { toStringPointerType, toStringI64Type, toStringPointerType }, true )
			);
			llvm::FunctionCallee toStringMalloc = this->llvmModule->getOrInsertFunction(
				"malloc",
				llvm::FunctionType::get( toStringPointerType, { toStringI64Type }, false )
			);
			llvm::Value* reprBuffer = toStringBuilder.CreateCall( toStringMalloc, { llvm::ConstantInt::get( toStringI64Type, reprBufferSize ) }, "repr.buf" );
			llvm::Value* reprPtrInt = toStringBuilder.CreatePtrToInt( toStringSelfArg, toStringI64Type, "ptr.int" );
			llvm::Value* reprFmtVal = toStringBuilder.CreateGlobalStringPtr( reprFormatString, "repr.fmt" );
			toStringBuilder.CreateCall( toStringSnprintf, { reprBuffer, llvm::ConstantInt::get( toStringI64Type, reprBufferSize ), reprFmtVal, reprPtrInt } );
			toStringBuilder.CreateRet( reprBuffer );
			this->functionResolutionMap[perClassToStringName] = perClassToStringFunc;
		}
		for( const std::pair<const std::string, semantic::TypeSharedPointer>& entry : this->semanticAnalyzer.types().getUserTypes() ) {
			if( entry.second == nullptr || entry.second->kind != semantic::Type::Kind::Class ) {
				continue;
			}
			semantic::ClassType* classType = static_cast<semantic::ClassType*>( entry.second.get() );
			if( classType->interfaces.empty() ) {
				continue;
			}
			std::string className = classType->name;
			std::string classQualified = entry.first;
			if( this->interfaceTableMap.count( className ) > 0 || this->interfaceTableMap.count( classQualified ) > 0 ) {
				continue;
			}
			std::string classNameBase = className;
			{
				size_t bracketPos = classNameBase.find( '<' );
				if( bracketPos != std::string::npos ) {
					classNameBase = classNameBase.substr( 0, bracketPos );
				}
			}
			std::string classQualifiedBase = classQualified;
			{
				size_t bracketPos = classQualifiedBase.find( '<' );
				if( bracketPos != std::string::npos ) {
					classQualifiedBase = classQualifiedBase.substr( 0, bracketPos );
				}
			}
			std::string classFullQualified = classType->qualified;
			{
				size_t bracketPos = classFullQualified.find( '<' );
				if( bracketPos != std::string::npos ) {
					classFullQualified = classFullQualified.substr( 0, bracketPos );
				}
			}
			for( const semantic::TypeSharedPointer& interfaceRef : classType->interfaces ) {
				if( interfaceRef == nullptr || interfaceRef->kind != semantic::Type::Kind::Interface ) {
					continue;
				}
				semantic::InterfaceType* interfaceType = static_cast<semantic::InterfaceType*>( interfaceRef.get() );
				std::vector<std::string> methodNames;
				if( interfaceType->methodOrder.empty() == false ) {
					methodNames = interfaceType->methodOrder;
				}
				else {
					for( const semantic::MethodInfo& method : interfaceType->methods ) {
						methodNames.push_back( method.name );
					}
				}
				if( methodNames.empty() ) {
					continue;
				}
				{
					bool hasToStringInItable = false;
					for( const std::string& existingMethodName : methodNames ) {
						if( existingMethodName == semantic::qualname::classes::object::methods::ToString ) {
							hasToStringInItable = true;
							break;
						}
					}
					if( hasToStringInItable == false ) {
						methodNames.insert( methodNames.begin(),
							semantic::qualname::classes::object::methods::ToString );
					}
				}
				llvm::FunctionType* voidFuncType = llvm::FunctionType::get( llvm::Type::getVoidTy( this->llvmContext ), false );
				llvm::PointerType* funcPtrType = llvm::PointerType::getUnqual( voidFuncType );
				std::vector<llvm::Constant*> itableEntries;
				for( const std::string& methodName : methodNames ) {
					llvm::Function* implFunc = nullptr;
					if( classFullQualified.empty() == false ) {
						std::string fullQualifiedMethodName = classFullQualified + "." + methodName;
						if( this->functionResolutionMap.count( fullQualifiedMethodName ) > 0 ) {
							implFunc = this->functionResolutionMap[fullQualifiedMethodName];
						}
					}
					if( implFunc == nullptr ) {
						std::string qualifiedMethodName = classQualifiedBase + "." + methodName;
						if( this->functionResolutionMap.count( qualifiedMethodName ) > 0 ) {
							implFunc = this->functionResolutionMap[qualifiedMethodName];
						}
					}
					if( implFunc == nullptr ) {
						std::string shortMethodName = classNameBase + "." + methodName;
						if( this->functionResolutionMap.count( shortMethodName ) > 0 ) {
							implFunc = this->functionResolutionMap[shortMethodName];
						}
					}
					if( implFunc != nullptr ) {
						itableEntries.push_back( llvm::ConstantExpr::getBitCast( implFunc, funcPtrType ) );
					}
					else {
						itableEntries.push_back( llvm::ConstantPointerNull::get( funcPtrType ) );
					}
				}
				llvm::ArrayType* itableArrayType = llvm::ArrayType::get( funcPtrType, itableEntries.size() );
				std::string itableGlobalName = fmt::format( "_MIR_itable_{}_{}", className, interfaceType->name );
				llvm::GlobalVariable* itableGlobal = new llvm::GlobalVariable(
					*this->llvmModule, itableArrayType, true,
					llvm::GlobalValue::InternalLinkage,
					llvm::ConstantArray::get( itableArrayType, itableEntries ),
					itableGlobalName
				);
				this->interfaceTableMap[classQualified] = itableGlobal;
				this->interfaceTableMap[className] = itableGlobal;
				if( classNameBase != className ) {
					this->interfaceTableMap[classNameBase] = itableGlobal;
				}
				if( classQualifiedBase != classQualified ) {
					this->interfaceTableMap[classQualifiedBase] = itableGlobal;
				}
				std::string directIfaceName = interfaceType->name;
				size_t directIfaceBracket = directIfaceName.find( '<' );
				if( directIfaceBracket != std::string::npos ) {
					directIfaceName = directIfaceName.substr( 0, directIfaceBracket );
				}
				this->interfacesWithDirectItable.insert( directIfaceName );
				this->interfaceTableMap[classNameBase + ":" + directIfaceName] = itableGlobal;
				if( interfaceType->qualified.empty() == false ) {
					std::string directIfaceQualified = interfaceType->qualified;
					size_t qualBracketPos = directIfaceQualified.find( '<' );
					if( qualBracketPos != std::string::npos ) {
						directIfaceQualified = directIfaceQualified.substr( 0, qualBracketPos );
					}
					this->interfacesWithDirectItable.insert( directIfaceQualified );
				}
				std::vector<semantic::InterfaceType*> ancestorStack;
				for( const semantic::TypeSharedPointer& parentInterface : interfaceType->parentInterfaces ) {
					if( parentInterface != nullptr && parentInterface->kind == semantic::Type::Kind::Interface ) {
						ancestorStack.push_back( static_cast<semantic::InterfaceType*>( parentInterface.get() ) );
					}
				}
				std::set<std::string> visitedAncestors;
				while( ancestorStack.empty() == false ) {
					semantic::InterfaceType* ancestorIface = ancestorStack.back();
					ancestorStack.pop_back();
					std::string ancestorBaseName = ancestorIface->name;
					size_t ancestorBracket = ancestorBaseName.find( '<' );
					if( ancestorBracket != std::string::npos ) {
						ancestorBaseName = ancestorBaseName.substr( 0, ancestorBracket );
					}
					if( visitedAncestors.count( ancestorBaseName ) > 0 ) {
						continue;
					}
					visitedAncestors.insert( ancestorBaseName );
					this->interfacesWithDirectItable.insert( ancestorBaseName );
					std::string ancestorBaseQualified = ancestorIface->qualified;
					if( ancestorBaseQualified.empty() == false ) {
						size_t qualBracket = ancestorBaseQualified.find( '<' );
						if( qualBracket != std::string::npos ) {
							ancestorBaseQualified = ancestorBaseQualified.substr( 0, qualBracket );
						}
						this->interfacesWithDirectItable.insert( ancestorBaseQualified );
					}
					semantic::TypeSharedPointer originalType = this->semanticAnalyzer.types().lookupType( ancestorBaseName );
					if( originalType != nullptr && originalType->kind == semantic::Type::Kind::Interface ) {
						semantic::InterfaceType* originalIface = static_cast<semantic::InterfaceType*>( originalType.get() );
						for( const semantic::TypeSharedPointer& grandparent : originalIface->parentInterfaces ) {
							if( grandparent != nullptr && grandparent->kind == semantic::Type::Kind::Interface ) {
								ancestorStack.push_back( static_cast<semantic::InterfaceType*>( grandparent.get() ) );
							}
						}
					}
				}
			}
		}
		this->abstractClassSubclasses.clear();
		this->classVtableIdentifier.clear();
		for( const std::pair<const std::string, semantic::TypeSharedPointer>& entry : this->semanticAnalyzer.types().getUserTypes() ) {
			if( entry.second == nullptr || entry.second->kind != semantic::Type::Kind::Class ) {
				continue;
			}
			semantic::ClassType* classType = static_cast<semantic::ClassType*>( entry.second.get() );
			bool isClassAbstract = classType->isAbstract ||
				( classType->astDeclaration != nullptr && classType->astDeclaration->isAbstract );
			if( isClassAbstract ) {
				continue;
			}
			semantic::TypeSharedPointer walkBase = classType->baseClass;
			while( walkBase != nullptr && walkBase->kind == semantic::Type::Kind::Class ) {
				semantic::ClassType* basePtr = static_cast<semantic::ClassType*>( walkBase.get() );
				bool isBaseAbstract = basePtr->isAbstract ||
					( basePtr->astDeclaration != nullptr && basePtr->astDeclaration->isAbstract );
				if( isBaseAbstract ) {
					std::string concreteName = classType->name;
					this->abstractClassSubclasses[basePtr->name].push_back( concreteName );
					if( basePtr->qualified.empty() == false ) {
						this->abstractClassSubclasses[basePtr->qualified].push_back( concreteName );
					}
					if( this->interfaceTableMap.count( concreteName ) > 0 ) {
						this->classVtableIdentifier[concreteName] = this->interfaceTableMap[concreteName];
					}
					else if( this->interfaceTableMap.count( entry.first ) > 0 ) {
						this->classVtableIdentifier[concreteName] = this->interfaceTableMap[entry.first];
					}
					else {
						llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
						std::string markerName = fmt::format( "_MIR_vtable_marker_{}", concreteName );
						llvm::GlobalVariable* markerGlobal = new llvm::GlobalVariable(
							*this->llvmModule, ptrType, true,
							llvm::GlobalValue::InternalLinkage,
							llvm::ConstantPointerNull::get( llvm::cast<llvm::PointerType>( ptrType ) ),
							markerName
						);
						this->classVtableIdentifier[concreteName] = markerGlobal;
						this->interfaceTableMap[concreteName] = markerGlobal;
						this->classesWithVtable.insert( concreteName );
					}
					break;
				}
				walkBase = basePtr->baseClass;
			}
		}
		for( std::shared_ptr<MIRFunctionDefinition>& functionDefinition : mirModule.functionDefinitions ) {
			if( functionDefinition != nullptr && functionDefinition->isAsyncFunction ) {
				this->programHasAsyncFunctions = true;
				break;
			}
		}
		for( std::shared_ptr<MIRFunctionDefinition>& functionDefinition : mirModule.functionDefinitions ) {
			if( functionDefinition != nullptr && functionDefinition->isGeneratorFunction ) {
				this->generateFunction( *functionDefinition );
			}
		}
		for( std::shared_ptr<MIRFunctionDefinition>& functionDefinition : mirModule.functionDefinitions ) {
			if( functionDefinition != nullptr && functionDefinition->isGeneratorFunction == false ) {
				this->generateFunction( *functionDefinition );
			}
		}
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
							for( const semantic::TypeSharedPointer& parentExtends : parentIfacePtr->parentInterfaces ) {
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
		size_t functionCount = 0;
		for( llvm::Function& function : *this->llvmModule ) {
			functionCount++;
			if( function.isDeclaration() && function.hasPersonalityFn() ) {
				function.setPersonalityFn( nullptr );
			}
		}
		if( functionCount <= 500 ) {
			std::string verifyErrors;
			llvm::raw_string_ostream verifyStream( verifyErrors );
			if( llvm::verifyModule( *this->llvmModule, &verifyStream ) ) {
				fmt::print( stderr, "mir codegen: llvm module verification failed: {}\n", verifyStream.str() );
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
				fmt::format( "mir codegen: cannot open file \"{}\": {}", filename, errorCode.message() )
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
			this->diagnosticEngine.error( nullptr, fmt::format( "mir codegen: target lookup failed: {}", targetError ) );
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
	
	void MIRCodegen::generateFunction( MIRFunctionDefinition& functionDefinition ) {
		this->concreteClassMap.clear();
		std::string llvmFunctionName = functionDefinition.functionName;
		if( functionDefinition.ownerClassQualifiedName.empty() == false ) {
			llvmFunctionName = functionDefinition.ownerClassQualifiedName + "." + functionDefinition.functionName;
		}
		else if( functionDefinition.mangledFunctionName.empty() == false ) {
			llvmFunctionName = functionDefinition.mangledFunctionName;
		}
		llvm::Function* llvmFunction = nullptr;
		if( this->functionResolutionMap.count( llvmFunctionName ) > 0 ) {
			llvmFunction = this->functionResolutionMap[llvmFunctionName];
			if( llvmFunction != nullptr && functionDefinition.ownerClassQualifiedName.empty() &&
				this->externDeclaredNames.count( llvmFunctionName ) > 0 ) {
				return;
			}
			bool isMainFunc = ( llvmFunctionName == semantic::qualname::functions::main::Name && functionDefinition.ownerClassQualifiedName.empty() );
			if( llvmFunction != nullptr && isMainFunc == false &&
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
		bool isMainFunction = ( llvmFunctionName == semantic::qualname::functions::main::Name && functionDefinition.ownerClassQualifiedName.empty() );
		if( llvmFunction == nullptr ) {
			semantic::TypeSharedPointer returnTypeDesc = functionDefinition.returnTypeDescriptor != nullptr
				? functionDefinition.returnTypeDescriptor
				: std::make_shared<semantic::Type>( semantic::Type::Kind::Void, semantic::qualname::classes::Void::Name );
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
				else if( functionDefinition.keywordParameterIndex >= 0 &&
					paramIdx == static_cast<unsigned>( functionDefinition.keywordParameterIndex ) ) {
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
				if( functionDefinition.ownerClassQualifiedName.empty() == false &&
					functionDefinition.isStaticMethod == false && parameterTypes.empty() ) {
					parameterType = llvm::PointerType::getUnqual( this->llvmContext );
				}
				parameterTypes.push_back( parameterType );
			}
			llvm::FunctionType* functionType = llvm::FunctionType::get( returnType, parameterTypes, false );
			llvmFunction = llvm::Function::Create(
				functionType, llvm::Function::ExternalLinkage,
				llvmFunctionName, this->llvmModule.get()
			);
			llvmFunction->setPersonalityFn( this->getOrCreatePersonality() );
			this->functionResolutionMap[llvmFunctionName] = llvmFunction;
			if( functionDefinition.ownerClassQualifiedName.empty() ) {
				if( this->functionResolutionMap.count( functionDefinition.functionName ) == 0 ) {
					this->functionResolutionMap[functionDefinition.functionName] = llvmFunction;
				}
			}
		}
		if( functionDefinition.isGeneratorFunction ) {
			this->generateGeneratorFunction( functionDefinition, llvmFunctionName, llvmFunction );
			return;
		}
		if( functionDefinition.isAsyncFunction && functionDefinition.functionName != semantic::qualname::functions::main::Name ) {
			this->generateAsyncFunction( functionDefinition, llvmFunctionName, llvmFunction );
			return;
		}
		for( std::shared_ptr<MIRBasicBlock>& basicBlock : functionDefinition.controlFlowBlocks ) {
			if( basicBlock != nullptr ) {
				llvm::BasicBlock* llvmBlock = llvm::BasicBlock::Create(
					this->llvmContext, basicBlock->blockLabel, llvmFunction
				);
				this->blockMap[basicBlock->blockIdentifier] = llvmBlock;
			}
		}
		llvm::BasicBlock* entryBlock = this->blockMap[functionDefinition.entryBlockIdentifier];
		if( entryBlock == nullptr ) {
			return;
		}
		this->irBuilder.SetInsertPoint( entryBlock );
		unsigned parameterIndex = isMainFunction ? 2 : 0;
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
		if( isMainFunction && llvmFunction->arg_size() >= 2 ) {
			llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
			llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
			llvm::Value* argcValue = this->irBuilder.CreateSExt( llvmFunction->getArg( 0 ), i64Type, "argc.ext" );
			llvm::Value* argvRaw = llvmFunction->getArg( 1 );
			llvm::GlobalVariable* argcGlobal = this->llvmModule->getGlobalVariable( "argc", true );
			if( argcGlobal != nullptr ) {
				llvm::Value* storedArgc = argcValue;
				if( argcGlobal->getValueType() != i64Type ) {
					storedArgc = this->irBuilder.CreateIntCast( argcValue, argcGlobal->getValueType(), true, "argc.cast" );
				}
				this->irBuilder.CreateStore( storedArgc, argcGlobal );
			}
			llvm::GlobalVariable* argvGlobal = this->llvmModule->getGlobalVariable( "argv", true );
			llvm::StructType* listStructType = nullptr;
			if( argvGlobal != nullptr ) {
				llvm::Type* argvValueType = argvGlobal->getValueType();
				if( argvValueType->isStructTy() ) {
					listStructType = llvm::cast<llvm::StructType>( argvValueType );
				}
				else if( argvValueType->isPointerTy() ) {
					std::unordered_map<std::string, llvm::StructType*>::iterator structIt =
						this->structTypeCache.find( semantic::qualname::classes::arraylist::Name );
					if( structIt != this->structTypeCache.end() ) {
						listStructType = structIt->second;
					}
				}
			}
			if( listStructType == nullptr ) {
				std::unordered_map<std::string, llvm::StructType*>::iterator structIt =
					this->structTypeCache.find( semantic::qualname::classes::arraylist::Name );
				if( structIt != this->structTypeCache.end() ) {
					listStructType = structIt->second;
				}
			}
			llvm::Value* builtArgvPtr = nullptr;
			if( listStructType != nullptr && listStructType->getNumElements() >= 3 ) {
				llvm::Value* structSize = llvm::ConstantInt::get( i64Type,
					this->llvmModule->getDataLayout().getTypeAllocSize( listStructType ) );
				llvm::Value* allocatedList = this->irBuilder.CreateCall(
					this->getOrCreateCalloc(),
					{ llvm::ConstantInt::get( i64Type, 1 ), structSize },
					"argv.alloc" );
				builtArgvPtr = allocatedList;
				if( argvGlobal != nullptr ) {
					this->irBuilder.CreateStore( allocatedList, argvGlobal );
				}
				llvm::Value* ptrSize = llvm::ConstantInt::get( i64Type, 8 );
				llvm::Value* totalBytes = this->irBuilder.CreateMul( argcValue, ptrSize, "argv.bytes" );
				llvm::Value* rawBuf = this->irBuilder.CreateCall( this->getOrCreateMalloc(), { totalBytes }, "argv.buf" );
				this->irBuilder.CreateCall( this->getOrCreateMemcpy(),
					{ rawBuf, argvRaw, totalBytes, this->irBuilder.getInt1( false ) } );
				unsigned argvFieldOff = 0;
				if( this->classesWithVtable.count( semantic::qualname::classes::arraylist::Name ) ||
					this->classesWithVtable.count( semantic::qualname::classes::arraylist::Qualified ) ) {
					if( listStructType->getNumElements() >= 4 ) {
						argvFieldOff = 1;
					}
				}
				if( argvFieldOff > 0 ) {
					llvm::Value* vtableGep = this->irBuilder.CreateStructGEP( listStructType, builtArgvPtr, 0, "argv.vtable" );
					llvm::Value* argvItablePtr = llvm::ConstantPointerNull::get(
						llvm::PointerType::getUnqual( this->llvmContext ) );
					std::string argvItableKey = semantic::qualname::classes::arraylist::Name;
					if( this->interfaceTableMap.count( argvItableKey ) == 0 ) {
						argvItableKey = semantic::qualname::classes::arraylist::Qualified;
					}
					if( this->interfaceTableMap.count( argvItableKey ) > 0 ) {
						argvItablePtr = this->irBuilder.CreateBitCast(
							this->interfaceTableMap[argvItableKey],
							llvm::PointerType::getUnqual( this->llvmContext ), "argv.itable" );
					}
					this->irBuilder.CreateStore( argvItablePtr, vtableGep );
				}
				llvm::Value* dataGep = this->irBuilder.CreateStructGEP( listStructType, builtArgvPtr, argvFieldOff + 0, "argv.data.ptr" );
				llvm::Value* countGep = this->irBuilder.CreateStructGEP( listStructType, builtArgvPtr, argvFieldOff + 1, "argv.count.ptr" );
				llvm::Value* capGep = this->irBuilder.CreateStructGEP( listStructType, builtArgvPtr, argvFieldOff + 2, "argv.cap.ptr" );
				llvm::Type* dataFieldType = listStructType->getElementType( argvFieldOff + 0 );
				llvm::Value* dataCast = rawBuf;
				if( rawBuf->getType() != dataFieldType ) {
					if( dataFieldType->isIntegerTy() ) {
						dataCast = this->irBuilder.CreatePtrToInt( rawBuf, dataFieldType, "argv.data.cast" );
					}
				}
				this->irBuilder.CreateStore( dataCast, dataGep );
				this->irBuilder.CreateStore( argcValue, countGep );
				this->irBuilder.CreateStore( argcValue, capGep );
			}
			for( size_t paramIdx = 0; paramIdx < functionDefinition.parameterVariableIdentifiers.size(); paramIdx++ ) {
				MIRVariableIdentifier paramVariable = functionDefinition.parameterVariableIdentifiers[paramIdx];
				if( this->variableValueMap.count( paramVariable ) == 0 ) {
					continue;
				}
				if( functionDefinition.variableDescriptorTable.count( paramVariable ) == 0 ) {
					continue;
				}
				std::string paramName = functionDefinition.variableDescriptorTable[paramVariable].variableName;
				if( paramName == semantic::qualname::functions::main::params::Argc ) {
					this->irBuilder.CreateStore( argcValue, this->variableValueMap[paramVariable] );
				}
				else if( paramName == semantic::qualname::functions::main::params::Argv && builtArgvPtr != nullptr ) {
					this->irBuilder.CreateStore( builtArgvPtr, this->variableValueMap[paramVariable] );
				}
			}
		}
		std::string frameDisplayName = functionDefinition.functionName;
		if( functionDefinition.ownerClassQualifiedName.empty() == false ) {
			std::string ownerShort = functionDefinition.ownerClassQualifiedName;
			size_t lastDot = ownerShort.rfind( '.' );
			if( lastDot != std::string::npos ) {
				ownerShort = ownerShort.substr( lastDot + 1 );
			}
			frameDisplayName = ownerShort + "." + functionDefinition.functionName;
		}
		std::string frameFilename = "unknown";
		int64_t frameLine = 0;
		int64_t frameColumn = 0;
		if( functionDefinition.sourceLocation != nullptr ) {
			frameFilename = functionDefinition.sourceLocation->filename;
			if( functionDefinition.sourceLocation->location != nullptr ) {
				frameLine = functionDefinition.sourceLocation->location->line;
				frameColumn = functionDefinition.sourceLocation->location->column;
			}
		}
		this->emitPushFrame( frameFilename, frameLine, frameColumn, frameDisplayName );
		if( isMainFunction && this->programHasAsyncFunctions ) {
			llvm::Function* schedulerInitFunc = this->llvmModule->getFunction( "runtimeInit" );
			if( schedulerInitFunc == nullptr ) {
				schedulerInitFunc = this->llvmModule->getFunction( "uraniteSchedulerInit" );
				if( schedulerInitFunc == nullptr ) {
					llvm::FunctionType* initType = llvm::FunctionType::get( llvm::Type::getVoidTy( this->llvmContext ), false );
					schedulerInitFunc = llvm::Function::Create( initType, llvm::Function::ExternalLinkage, "uraniteSchedulerInit", this->llvmModule.get() );
				}
			}
			this->irBuilder.CreateCall( schedulerInitFunc );
		}
		for( std::shared_ptr<MIRBasicBlock>& basicBlock : functionDefinition.controlFlowBlocks ) {
			if( basicBlock != nullptr ) {
				this->generateBasicBlock( *basicBlock, functionDefinition );
			}
		}
		llvm::Type* functionReturnType = llvmFunction->getReturnType();
		for( llvm::BasicBlock& llvmBlock : *llvmFunction ) {
			if( llvmBlock.getTerminator() == nullptr ) {
				this->irBuilder.SetInsertPoint( &llvmBlock );
				this->emitPopFrame();
				if( functionReturnType->isVoidTy() ) {
					this->irBuilder.CreateRetVoid();
				}
				else {
					this->irBuilder.CreateRet( llvm::Constant::getNullValue( functionReturnType ) );
				}
			}
		}
	}
	
	void MIRCodegen::generateBasicBlock( MIRBasicBlock& basicBlock, MIRFunctionDefinition& functionDefinition ) {
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
	
	void MIRCodegen::generateInstruction( const MIRInstruction& instruction, MIRFunctionDefinition& functionDefinition ) {
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
				if( instruction.sourceLocation != nullptr ) {
					llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
					llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
					llvm::StructType* throwableBaseType = llvm::StructType::get(
						this->llvmContext, { ptrType, i64Type, ptrType, i64Type, ptrType }
					);
					llvm::Value* fileGEP = this->irBuilder.CreateStructGEP(
						throwableBaseType, thrownObject, 2, "tb.file.ptr"
					);
					llvm::Constant* fileStr = this->irBuilder.CreateGlobalStringPtr(
						instruction.sourceLocation->filename, "tb.file"
					);
					this->irBuilder.CreateStore( fileStr, fileGEP );
					uint32_t lineNumber = instruction.sourceLocation->location
						? instruction.sourceLocation->location->line : 0;
					llvm::Value* lineGEP = this->irBuilder.CreateStructGEP(
						throwableBaseType, thrownObject, 3, "tb.line.ptr"
					);
					this->irBuilder.CreateStore(
						llvm::ConstantInt::get( i64Type, lineNumber ), lineGEP
					);
				}
				std::string typeName = instruction.calledFunctionQualifiedName.empty()
					? semantic::qualname::classes::error::Name
					: instruction.calledFunctionQualifiedName;
				llvm::Value* typeNameGlobal = this->irBuilder.CreateGlobalStringPtr( typeName, "throw.typename" );
				if( instruction.landingPadTarget != INVALID_BLOCK_IDENTIFIER &&
					this->blockMap.count( instruction.landingPadTarget ) > 0 ) {
					llvm::Function* enclosingFunction = this->irBuilder.GetInsertBlock()->getParent();
					if( enclosingFunction->hasPersonalityFn() == false ) {
						enclosingFunction->setPersonalityFn( this->getOrCreatePersonality() );
					}
					llvm::BasicBlock* unwindDest = this->blockMap[instruction.landingPadTarget];
					llvm::BasicBlock* unreachableBlock = llvm::BasicBlock::Create(
						this->llvmContext, "throw.unreachable", enclosingFunction
					);
					this->irBuilder.CreateInvoke(
						throwFunction, unreachableBlock, unwindDest, { thrownObject, typeNameGlobal }
					);
					this->irBuilder.SetInsertPoint( unreachableBlock );
					this->irBuilder.CreateUnreachable();
				}
				else if( this->asyncWrapperCatchBlock != nullptr ) {
					llvm::Function* enclosingFunction = this->irBuilder.GetInsertBlock()->getParent();
					if( enclosingFunction->hasPersonalityFn() == false ) {
						enclosingFunction->setPersonalityFn( this->getOrCreatePersonality() );
					}
					llvm::BasicBlock* unreachableBlock = llvm::BasicBlock::Create(
						this->llvmContext, "throw.unreachable", enclosingFunction
					);
					this->irBuilder.CreateInvoke(
						throwFunction, unreachableBlock, this->asyncWrapperCatchBlock, { thrownObject, typeNameGlobal }
					);
					this->irBuilder.SetInsertPoint( unreachableBlock );
					this->irBuilder.CreateUnreachable();
				}
				else {
					this->irBuilder.CreateCall( throwFunction, { thrownObject, typeNameGlobal } );
					this->irBuilder.CreateUnreachable();
				}
				break;
			}
			case MIRInstructionKind::Yield:
				this->generateYield( instruction );
				break;
			case MIRInstructionKind::NoOperation:
			case MIRInstructionKind::DropValue:
			case MIRInstructionKind::DeferPush:
			case MIRInstructionKind::DeferEmit:
				break;
			case MIRInstructionKind::InvokeFunction:
				this->generateCallFunction( instruction, functionDefinition );
				break;
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
				if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
					llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
					if( instruction.calledFunctionQualifiedName.empty() == false ) {
						llvm::Function* targetFunction = nullptr;
						if( this->functionResolutionMap.count( instruction.calledFunctionQualifiedName ) > 0 ) {
							targetFunction = this->functionResolutionMap[instruction.calledFunctionQualifiedName];
						}
						if( targetFunction == nullptr ) {
							targetFunction = this->llvmModule->getFunction( instruction.calledFunctionQualifiedName );
						}
						if( targetFunction != nullptr ) {
							this->setVariableValue( instruction.destinationVariable, targetFunction );
						}
						else {
							this->setVariableValue( instruction.destinationVariable,
								llvm::Constant::getNullValue( llvm::PointerType::getUnqual( this->llvmContext ) ) );
						}
					}
					else if( instruction.sourceOperands.empty() == false ) {
						llvm::Value* operandValue = this->getVariableValue( instruction.sourceOperands[0] );
						if( operandValue == nullptr ) {
							operandValue = this->loadVariableValue( instruction.sourceOperands[0] );
						}
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
								if( isMatch == false && ( sourceTypeName == semantic::qualname::classes::object::Name || sourceTypeName == targetTypeName ) ) {
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
	
	void MIRCodegen::generateAllocateLocal( const MIRInstruction& instruction, MIRFunctionDefinition& functionDefinition ) {
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
		if( instruction.destinationVariable == INVALID_VARIABLE_IDENTIFIER ) {
			return;
		}
		if( instruction.sourceOperands.empty() ) {
			if( instruction.calledFunctionQualifiedName.empty() == false ) {
				std::string functionLookupName = instruction.calledFunctionQualifiedName;
				llvm::Function* referencedFunction = nullptr;
				if( this->functionResolutionMap.count( functionLookupName ) > 0 ) {
					referencedFunction = this->functionResolutionMap[functionLookupName];
				}
				if( referencedFunction == nullptr ) {
					size_t lastDotPosition = functionLookupName.rfind( '.' );
					if( lastDotPosition != std::string::npos ) {
						std::string shortName = functionLookupName.substr( lastDotPosition + 1 );
						if( this->functionResolutionMap.count( shortName ) > 0 ) {
							referencedFunction = this->functionResolutionMap[shortName];
						}
					}
				}
				if( referencedFunction != nullptr ) {
					llvm::Value* functionPointer = this->irBuilder.CreatePtrToInt(
						referencedFunction, llvm::Type::getInt64Ty( this->llvmContext ), "fn.addr"
					);
					this->setVariableValue( instruction.destinationVariable, functionPointer );
				}
			}
			return;
		}
		if( this->concreteClassMap.count( instruction.sourceOperands[0] ) > 0 ) {
			bool skipPropagation = false;
			if( this->currentMIRFunction != nullptr &&
				this->currentMIRFunction->variableDescriptorTable.count( instruction.destinationVariable ) > 0 ) {
				semantic::TypeSharedPointer destType =
					this->currentMIRFunction->variableDescriptorTable[instruction.destinationVariable].variableType;
				if( destType != nullptr && ( destType->kind == semantic::Type::Kind::Interface ||
					destType->kind == semantic::Type::Kind::GenericParameter ) ) {
					skipPropagation = true;
				}
			}
			if( skipPropagation == false ) {
				this->concreteClassMap[instruction.destinationVariable] =
					this->concreteClassMap[instruction.sourceOperands[0]];
			}
		}
		llvm::Value* sourcePointer = this->getVariableValue( instruction.sourceOperands[0] );
		if( sourcePointer == nullptr ) {
			return;
		}
		if( llvm::AllocaInst* allocaInst = llvm::dyn_cast<llvm::AllocaInst>( sourcePointer ) ) {
			llvm::Type* allocatedType = allocaInst->getAllocatedType();
			if( allocatedType->isFirstClassType() && allocatedType->isVoidTy() == false ) {
				llvm::Value* loadedValue = this->irBuilder.CreateLoad( allocatedType, sourcePointer, "load" );
				if( this->currentGeneratorContext != nullptr ) {
					llvm::Function* parentFunction = this->irBuilder.GetInsertBlock()->getParent();
					llvm::AllocaInst* destAlloca = this->createEntryBlockAllocation(
						parentFunction,
						fmt::format( "gen.v{}", instruction.destinationVariable ),
						allocatedType
					);
					this->irBuilder.CreateStore( loadedValue, destAlloca );
					this->setVariableValue( instruction.destinationVariable, destAlloca );
				}
				else {
					this->setVariableValue( instruction.destinationVariable, loadedValue );
				}
				return;
			}
		}
		if( llvm::GetElementPtrInst* gepInst = llvm::dyn_cast<llvm::GetElementPtrInst>( sourcePointer ) ) {
			llvm::Type* elementType = gepInst->getResultElementType();
			if( elementType->isFirstClassType() && elementType->isVoidTy() == false ) {
				llvm::Value* loadedValue = this->irBuilder.CreateLoad( elementType, sourcePointer, "load.field" );
				if( this->currentGeneratorContext != nullptr ) {
					llvm::Function* parentFunction = this->irBuilder.GetInsertBlock()->getParent();
					llvm::AllocaInst* destAlloca = this->createEntryBlockAllocation(
						parentFunction,
						fmt::format( "gen.v{}", instruction.destinationVariable ),
						elementType
					);
					this->irBuilder.CreateStore( loadedValue, destAlloca );
					this->setVariableValue( instruction.destinationVariable, destAlloca );
				}
				else {
					this->setVariableValue( instruction.destinationVariable, loadedValue );
				}
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
			else if( sourceValue->getType()->isStructTy() ) {
				llvm::StructType* sourceStruct = llvm::cast<llvm::StructType>( sourceValue->getType() );
				if( sourceStruct->getNumElements() == 1 ) {
					llvm::Type* innerType = sourceStruct->getElementType( 0 );
					if( innerType == targetStoreType ) {
						sourceValue = this->irBuilder.CreateExtractValue( sourceValue, 0, "unwrap.oop" );
					}
					else if( innerType->isPointerTy() && targetStoreType->isPointerTy() ) {
						sourceValue = this->irBuilder.CreateExtractValue( sourceValue, 0, "unwrap.oop" );
					}
					else if( innerType->isIntegerTy() && targetStoreType->isIntegerTy() ) {
						sourceValue = this->irBuilder.CreateExtractValue( sourceValue, 0, "unwrap.oop" );
						sourceValue = this->irBuilder.CreateIntCast( sourceValue, targetStoreType, true, "unwrap.cast" );
					}
					else if( innerType->isPointerTy() && targetStoreType->isIntegerTy() ) {
						sourceValue = this->irBuilder.CreateExtractValue( sourceValue, 0, "unwrap.oop" );
						sourceValue = this->irBuilder.CreatePtrToInt( sourceValue, targetStoreType, "unwrap.ptoi" );
					}
					else if( innerType->isIntegerTy() && targetStoreType->isPointerTy() ) {
						sourceValue = this->irBuilder.CreateExtractValue( sourceValue, 0, "unwrap.oop" );
						sourceValue = this->irBuilder.CreateIntToPtr( sourceValue, targetStoreType, "unwrap.itop" );
					}
					else {
						return;
					}
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
		if( instruction.sourceOperands.empty() == false &&
			this->concreteClassMap.count( instruction.sourceOperands[0] ) > 0 ) {
			bool skipPropagation = false;
			if( this->currentMIRFunction != nullptr &&
				this->currentMIRFunction->variableDescriptorTable.count( instruction.destinationVariable ) > 0 ) {
				semantic::TypeSharedPointer destType =
					this->currentMIRFunction->variableDescriptorTable[instruction.destinationVariable].variableType;
				if( destType != nullptr && ( destType->kind == semantic::Type::Kind::Interface ||
					destType->kind == semantic::Type::Kind::GenericParameter ) ) {
					skipPropagation = true;
				}
			}
			if( skipPropagation == false ) {
				this->concreteClassMap[instruction.destinationVariable] =
					this->concreteClassMap[instruction.sourceOperands[0]];
			}
		}
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
		this->generateCopyValue( instruction );
	}
	
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
	
	void MIRCodegen::generateArithmetic( const MIRInstruction& instruction ) {
		if( instruction.destinationVariable == INVALID_VARIABLE_IDENTIFIER ) {
			return;
		}
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
		bool mayBeStringComparison = false;
		if( this->currentMIRFunction != nullptr &&
			instruction.sourceOperands.size() >= 2 &&
			( instruction.instructionKind == MIRInstructionKind::CompareEqual ||
			  instruction.instructionKind == MIRInstructionKind::CompareNotEqual ) ) {
			int stringLikeCount = 0;
			bool hasSmallConstant = false;
			for( MIRVariableIdentifier operandVariable : instruction.sourceOperands ) {
				llvm::Value* operandValue = this->getVariableValue( operandVariable );
				if( operandValue != nullptr ) {
					llvm::ConstantInt* constInt = llvm::dyn_cast<llvm::ConstantInt>( operandValue );
					if( constInt != nullptr && constInt->getSExtValue() >= -1 && constInt->getSExtValue() <= 255 ) {
						hasSmallConstant = true;
					}
				}
				std::unordered_map<MIRVariableIdentifier, MIRVariableDescriptor>::iterator descriptorIterator =
					this->currentMIRFunction->variableDescriptorTable.find( operandVariable );
				if( descriptorIterator != this->currentMIRFunction->variableDescriptorTable.end() &&
					descriptorIterator->second.variableType != nullptr ) {
					semantic::Type::Kind operandKind = descriptorIterator->second.variableType->kind;
					if( operandKind == semantic::Type::Kind::GenericParameter ||
						operandKind == semantic::Type::Kind::String ||
						( operandKind == semantic::Type::Kind::Class &&
						  descriptorIterator->second.variableType->name == semantic::qualname::classes::string::Name ) ) {
						stringLikeCount++;
					}
				}
			}
			mayBeStringComparison = ( stringLikeCount >= 1 ) && ( hasSmallConstant == false );
		}
		llvm::Value* result = nullptr;
		bool isFloat = leftOperand->getType()->isFloatingPointTy();
		switch( instruction.instructionKind ) {
			case MIRInstructionKind::CompareEqual:
				if( isFloat ) {
					result = this->irBuilder.CreateFCmpOEQ( leftOperand, rightOperand, "eq" );
				}
				else if( mayBeStringComparison && leftOperand->getType()->isIntegerTy( 64 ) ) {
					llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
					llvm::Value* pageThreshold = llvm::ConstantInt::get( i64Type, 4096 );
					llvm::Value* leftAbove = this->irBuilder.CreateICmpUGE( leftOperand, pageThreshold, "eq.labove" );
					llvm::Value* rightAbove = this->irBuilder.CreateICmpUGE( rightOperand, pageThreshold, "eq.rabove" );
					llvm::Value* bothAbove = this->irBuilder.CreateAnd( leftAbove, rightAbove, "eq.bothptr" );
					llvm::Function* currentFunction = this->irBuilder.GetInsertBlock()->getParent();
					llvm::BasicBlock* strcmpBlock = llvm::BasicBlock::Create( this->llvmContext, "eq.strcmp.call", currentFunction );
					llvm::BasicBlock* fallbackBlock = llvm::BasicBlock::Create( this->llvmContext, "eq.int.cmp", currentFunction );
					llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create( this->llvmContext, "eq.merge", currentFunction );
					this->irBuilder.CreateCondBr( bothAbove, strcmpBlock, fallbackBlock );
					this->irBuilder.SetInsertPoint( strcmpBlock );
					llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
					llvm::Value* leftPtr = this->irBuilder.CreateIntToPtr( leftOperand, ptrType, "eq.lptr" );
					llvm::Value* rightPtr = this->irBuilder.CreateIntToPtr( rightOperand, ptrType, "eq.rptr" );
					llvm::Function* strcmpFunction = this->getOrCreateStrcmp();
					llvm::Value* strcmpResult = this->irBuilder.CreateCall( strcmpFunction, { leftPtr, rightPtr }, "eq.strcmp" );
					llvm::Value* strcmpEq = this->irBuilder.CreateICmpEQ( strcmpResult,
						llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->llvmContext ), 0 ), "eq.streq" );
					this->irBuilder.CreateBr( mergeBlock );
					llvm::BasicBlock* strcmpExitBlock = this->irBuilder.GetInsertBlock();
					this->irBuilder.SetInsertPoint( fallbackBlock );
					llvm::Value* intEq = this->irBuilder.CreateICmpEQ( leftOperand, rightOperand, "eq.inteq" );
					this->irBuilder.CreateBr( mergeBlock );
					llvm::BasicBlock* fallbackExitBlock = this->irBuilder.GetInsertBlock();
					this->irBuilder.SetInsertPoint( mergeBlock );
					llvm::PHINode* phi = this->irBuilder.CreatePHI( llvm::Type::getInt1Ty( this->llvmContext ), 2, "eq" );
					phi->addIncoming( strcmpEq, strcmpExitBlock );
					phi->addIncoming( intEq, fallbackExitBlock );
					result = phi;
				}
				else if( mayBeStringComparison && leftOperand->getType()->isPointerTy() && rightOperand->getType()->isPointerTy() ) {
					llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
					llvm::Value* leftAsInt = this->irBuilder.CreatePtrToInt( leftOperand, i64Type, "eq.ptr.lint" );
					llvm::Value* rightAsInt = this->irBuilder.CreatePtrToInt( rightOperand, i64Type, "eq.ptr.rint" );
					llvm::Value* pageThreshold = llvm::ConstantInt::get( i64Type, 4096 );
					llvm::Value* leftAbove = this->irBuilder.CreateICmpUGE( leftAsInt, pageThreshold, "eq.ptr.labove" );
					llvm::Value* rightAbove = this->irBuilder.CreateICmpUGE( rightAsInt, pageThreshold, "eq.ptr.rabove" );
					llvm::Value* bothAbove = this->irBuilder.CreateAnd( leftAbove, rightAbove, "eq.ptr.bothptr" );
					llvm::Function* currentFunction = this->irBuilder.GetInsertBlock()->getParent();
					llvm::BasicBlock* strcmpBlock = llvm::BasicBlock::Create( this->llvmContext, "eq.ptr.strcmp", currentFunction );
					llvm::BasicBlock* fallbackBlock = llvm::BasicBlock::Create( this->llvmContext, "eq.ptr.fallback", currentFunction );
					llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create( this->llvmContext, "eq.ptr.merge", currentFunction );
					this->irBuilder.CreateCondBr( bothAbove, strcmpBlock, fallbackBlock );
					this->irBuilder.SetInsertPoint( strcmpBlock );
					llvm::Function* strcmpFunction = this->getOrCreateStrcmp();
					llvm::Value* strcmpResult = this->irBuilder.CreateCall( strcmpFunction, { leftOperand, rightOperand }, "eq.ptr.strcmp.r" );
					llvm::Value* strcmpEq = this->irBuilder.CreateICmpEQ( strcmpResult,
						llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->llvmContext ), 0 ), "eq.ptr.streq" );
					this->irBuilder.CreateBr( mergeBlock );
					llvm::BasicBlock* strcmpExitBlock = this->irBuilder.GetInsertBlock();
					this->irBuilder.SetInsertPoint( fallbackBlock );
					llvm::Value* intEq = this->irBuilder.CreateICmpEQ( leftAsInt, rightAsInt, "eq.ptr.inteq" );
					this->irBuilder.CreateBr( mergeBlock );
					llvm::BasicBlock* fallbackExitBlock = this->irBuilder.GetInsertBlock();
					this->irBuilder.SetInsertPoint( mergeBlock );
					llvm::PHINode* phi = this->irBuilder.CreatePHI( llvm::Type::getInt1Ty( this->llvmContext ), 2, "eq.ptr" );
					phi->addIncoming( strcmpEq, strcmpExitBlock );
					phi->addIncoming( intEq, fallbackExitBlock );
					result = phi;
				}
				else {
					result = this->irBuilder.CreateICmpEQ( leftOperand, rightOperand, "eq" );
				}
				break;
			case MIRInstructionKind::CompareNotEqual:
				if( isFloat ) {
					result = this->irBuilder.CreateFCmpONE( leftOperand, rightOperand, "ne" );
				}
				else if( mayBeStringComparison && leftOperand->getType()->isIntegerTy( 64 ) ) {
					llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
					llvm::Value* pageThreshold = llvm::ConstantInt::get( i64Type, 4096 );
					llvm::Value* leftAbove = this->irBuilder.CreateICmpUGE( leftOperand, pageThreshold, "ne.labove" );
					llvm::Value* rightAbove = this->irBuilder.CreateICmpUGE( rightOperand, pageThreshold, "ne.rabove" );
					llvm::Value* bothAbove = this->irBuilder.CreateAnd( leftAbove, rightAbove, "ne.bothptr" );
					llvm::Function* currentFunction = this->irBuilder.GetInsertBlock()->getParent();
					llvm::BasicBlock* strcmpBlock = llvm::BasicBlock::Create( this->llvmContext, "ne.strcmp.call", currentFunction );
					llvm::BasicBlock* fallbackBlock = llvm::BasicBlock::Create( this->llvmContext, "ne.int.cmp", currentFunction );
					llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create( this->llvmContext, "ne.merge", currentFunction );
					this->irBuilder.CreateCondBr( bothAbove, strcmpBlock, fallbackBlock );
					this->irBuilder.SetInsertPoint( strcmpBlock );
					llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
					llvm::Value* leftPtr = this->irBuilder.CreateIntToPtr( leftOperand, ptrType, "ne.lptr" );
					llvm::Value* rightPtr = this->irBuilder.CreateIntToPtr( rightOperand, ptrType, "ne.rptr" );
					llvm::Function* strcmpFunction = this->getOrCreateStrcmp();
					llvm::Value* strcmpResult = this->irBuilder.CreateCall( strcmpFunction, { leftPtr, rightPtr }, "ne.strcmp" );
					llvm::Value* strcmpNe = this->irBuilder.CreateICmpNE( strcmpResult,
						llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->llvmContext ), 0 ), "ne.strne" );
					this->irBuilder.CreateBr( mergeBlock );
					llvm::BasicBlock* strcmpExitBlock = this->irBuilder.GetInsertBlock();
					this->irBuilder.SetInsertPoint( fallbackBlock );
					llvm::Value* intNe = this->irBuilder.CreateICmpNE( leftOperand, rightOperand, "ne.intne" );
					this->irBuilder.CreateBr( mergeBlock );
					llvm::BasicBlock* fallbackExitBlock = this->irBuilder.GetInsertBlock();
					this->irBuilder.SetInsertPoint( mergeBlock );
					llvm::PHINode* phi = this->irBuilder.CreatePHI( llvm::Type::getInt1Ty( this->llvmContext ), 2, "ne" );
					phi->addIncoming( strcmpNe, strcmpExitBlock );
					phi->addIncoming( intNe, fallbackExitBlock );
					result = phi;
				}
				else if( mayBeStringComparison && leftOperand->getType()->isPointerTy() && rightOperand->getType()->isPointerTy() ) {
					llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
					llvm::Value* leftAsInt = this->irBuilder.CreatePtrToInt( leftOperand, i64Type, "ne.ptr.lint" );
					llvm::Value* rightAsInt = this->irBuilder.CreatePtrToInt( rightOperand, i64Type, "ne.ptr.rint" );
					llvm::Value* pageThreshold = llvm::ConstantInt::get( i64Type, 4096 );
					llvm::Value* leftAbove = this->irBuilder.CreateICmpUGE( leftAsInt, pageThreshold, "ne.ptr.labove" );
					llvm::Value* rightAbove = this->irBuilder.CreateICmpUGE( rightAsInt, pageThreshold, "ne.ptr.rabove" );
					llvm::Value* bothAbove = this->irBuilder.CreateAnd( leftAbove, rightAbove, "ne.ptr.bothptr" );
					llvm::Function* currentFunction = this->irBuilder.GetInsertBlock()->getParent();
					llvm::BasicBlock* strcmpBlock = llvm::BasicBlock::Create( this->llvmContext, "ne.ptr.strcmp", currentFunction );
					llvm::BasicBlock* fallbackBlock = llvm::BasicBlock::Create( this->llvmContext, "ne.ptr.fallback", currentFunction );
					llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create( this->llvmContext, "ne.ptr.merge", currentFunction );
					this->irBuilder.CreateCondBr( bothAbove, strcmpBlock, fallbackBlock );
					this->irBuilder.SetInsertPoint( strcmpBlock );
					llvm::Function* strcmpFunction = this->getOrCreateStrcmp();
					llvm::Value* strcmpResult = this->irBuilder.CreateCall( strcmpFunction, { leftOperand, rightOperand }, "ne.ptr.strcmp.r" );
					llvm::Value* strcmpNe = this->irBuilder.CreateICmpNE( strcmpResult,
						llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->llvmContext ), 0 ), "ne.ptr.strne" );
					this->irBuilder.CreateBr( mergeBlock );
					llvm::BasicBlock* strcmpExitBlock = this->irBuilder.GetInsertBlock();
					this->irBuilder.SetInsertPoint( fallbackBlock );
					llvm::Value* intNe = this->irBuilder.CreateICmpNE( leftAsInt, rightAsInt, "ne.ptr.intne" );
					this->irBuilder.CreateBr( mergeBlock );
					llvm::BasicBlock* fallbackExitBlock = this->irBuilder.GetInsertBlock();
					this->irBuilder.SetInsertPoint( mergeBlock );
					llvm::PHINode* phi = this->irBuilder.CreatePHI( llvm::Type::getInt1Ty( this->llvmContext ), 2, "ne.ptr" );
					phi->addIncoming( strcmpNe, strcmpExitBlock );
					phi->addIncoming( intNe, fallbackExitBlock );
					result = phi;
				}
				else {
					result = this->irBuilder.CreateICmpNE( leftOperand, rightOperand, "ne" );
				}
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
		if( leftOperand->getType()->isPointerTy() ) {
			leftOperand = this->irBuilder.CreateICmpNE(
				leftOperand, llvm::ConstantPointerNull::get( llvm::cast<llvm::PointerType>( leftOperand->getType() ) ), "lhs.bool"
			);
		}
		else if( leftOperand->getType()->isIntegerTy( 1 ) == false && leftOperand->getType()->isIntegerTy() ) {
			leftOperand = this->irBuilder.CreateICmpNE(
				leftOperand, llvm::ConstantInt::get( leftOperand->getType(), 0 ), "lhs.bool"
			);
		}
		if( rightOperand->getType()->isPointerTy() ) {
			rightOperand = this->irBuilder.CreateICmpNE(
				rightOperand, llvm::ConstantPointerNull::get( llvm::cast<llvm::PointerType>( rightOperand->getType() ) ), "rhs.bool"
			);
		}
		else if( rightOperand->getType()->isIntegerTy( 1 ) == false && rightOperand->getType()->isIntegerTy() ) {
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
	
	void MIRCodegen::generateCallFunction( const MIRInstruction& instruction, MIRFunctionDefinition& functionDefinition ) {
		std::string calledName = instruction.calledFunctionQualifiedName;
		std::string shortCalledName = calledName;
		std::string bareMethodName = calledName;
		bool isBareOrObjectCall = ( calledName.find( '.' ) == std::string::npos );
		{
			size_t lastDot = calledName.rfind( '.' );
			if( lastDot != std::string::npos && lastDot > 0 ) {
				bareMethodName = calledName.substr( lastDot + 1 );
				size_t secondLastDot = calledName.rfind( '.', lastDot - 1 );
				if( secondLastDot != std::string::npos ) {
					shortCalledName = calledName.substr( secondLastDot + 1 );
					std::string shortClassPrefix = calledName.substr( secondLastDot + 1, lastDot - secondLastDot - 1 );
					if( shortClassPrefix == semantic::qualname::classes::object::Name ) {
						isBareOrObjectCall = true;
					}
				}
				else {
					std::string classPrefix = calledName.substr( 0, lastDot );
					if( classPrefix == semantic::qualname::classes::object::Name ) {
						isBareOrObjectCall = true;
					}
				}
			}
		}
		{
			size_t genHasPos = calledName.find( ".__gen_has" );
			if( genHasPos != std::string::npos ) {
				std::string baseName = calledName.substr( 0, genHasPos );
				std::string genStructName = fmt::format( "__gen_{}", baseName );
				llvm::StructType* genStruct = nullptr;
				if( this->structTypeCache.count( genStructName ) > 0 ) {
					genStruct = this->structTypeCache[genStructName];
				}
				if( genStruct == nullptr ) {
					for( const std::pair<const std::string, llvm::StructType*>& entry : this->structTypeCache ) {
						if( entry.first.find( "__gen_" ) == 0 && entry.first.find( baseName ) != std::string::npos ) {
							genStruct = entry.second;
							break;
						}
					}
				}
				if( genStruct != nullptr && instruction.sourceOperands.empty() == false ) {
					llvm::Value* genPtr = this->loadVariableValue( instruction.sourceOperands[0] );
					if( genPtr != nullptr ) {
						llvm::Value* doneFieldPtr = this->irBuilder.CreateStructGEP(
							genStruct, genPtr, 2, "gen.done.ptr"
						);
						llvm::Value* doneValue = this->irBuilder.CreateLoad(
							llvm::Type::getInt1Ty( this->llvmContext ), doneFieldPtr, "gen.done"
						);
						llvm::Value* hasValue = this->irBuilder.CreateNot( doneValue, "gen.has" );
						if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
							this->setVariableValue( instruction.destinationVariable, hasValue );
						}
					}
				}
				return;
			}
			size_t genNextPos = calledName.find( ".__gen_next" );
			if( genNextPos != std::string::npos ) {
				std::string baseName = calledName.substr( 0, genNextPos );
				std::string genStructName = fmt::format( "__gen_{}", baseName );
				llvm::StructType* genStruct = nullptr;
				if( this->structTypeCache.count( genStructName ) > 0 ) {
					genStruct = this->structTypeCache[genStructName];
				}
				if( genStruct == nullptr ) {
					for( const std::pair<const std::string, llvm::StructType*>& entry : this->structTypeCache ) {
						if( entry.first.find( "__gen_" ) == 0 && entry.first.find( baseName ) != std::string::npos ) {
							std::string suffix = entry.first.substr( 6 );
							size_t dotPos = suffix.rfind( '.' );
							if( dotPos != std::string::npos && suffix.substr( dotPos + 1 ) == baseName ) {
								genStruct = entry.second;
								break;
							}
						}
					}
				}
				if( genStruct != nullptr && instruction.sourceOperands.empty() == false ) {
					llvm::Value* genPtr = this->loadVariableValue( instruction.sourceOperands[0] );
					if( genPtr != nullptr ) {
						llvm::Type* valueType = genStruct->getElementType( 1 );
						llvm::Value* valueFieldPtr = this->irBuilder.CreateStructGEP(
							genStruct, genPtr, 1, "gen.value.ptr"
						);
						llvm::Value* valueLoaded = this->irBuilder.CreateLoad(
							valueType, valueFieldPtr, "gen.value"
						);
						std::string nextFuncName = fmt::format( "{}.next", baseName );
						llvm::Function* nextFunc = nullptr;
						if( this->functionResolutionMap.count( nextFuncName ) > 0 ) {
							nextFunc = this->functionResolutionMap[nextFuncName];
						}
						if( nextFunc != nullptr ) {
							this->irBuilder.CreateCall( nextFunc, { genPtr } );
						}
						if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
							this->setVariableValue( instruction.destinationVariable, valueLoaded );
						}
					}
				}
				return;
			}
		}
		if( calledName == "__runtime_await" ) {
			if( instruction.sourceOperands.empty() ) {
				return;
			}
			llvm::Value* taskIdValue = this->loadVariableValue( instruction.sourceOperands[0] );
			if( taskIdValue == nullptr ) {
				return;
			}
			llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
			if( taskIdValue->getType() != i64Type ) {
				if( taskIdValue->getType()->isPointerTy() ) {
					taskIdValue = this->irBuilder.CreatePtrToInt( taskIdValue, i64Type, "await.tid" );
				}
				else if( taskIdValue->getType()->isIntegerTy() ) {
					taskIdValue = this->irBuilder.CreateIntCast( taskIdValue, i64Type, true, "await.tid" );
				}
			}
			llvm::Function* currentFunc = this->irBuilder.GetInsertBlock()->getParent();
			llvm::AllocaInst* taskIdAlloca = this->createEntryBlockAllocation( currentFunc, "await.taskid", i64Type );
			this->irBuilder.CreateStore( taskIdValue, taskIdAlloca );
			llvm::Function* awaitFunc = this->llvmModule->getFunction( "runtimeAwait" );
			if( awaitFunc == nullptr ) {
				awaitFunc = this->llvmModule->getFunction( "uraniteAwaitTask" );
				if( awaitFunc == nullptr ) {
					llvm::FunctionType* awaitSig = llvm::FunctionType::get( i64Type, { i64Type }, false );
					awaitFunc = llvm::Function::Create( awaitSig, llvm::Function::ExternalLinkage, "uraniteAwaitTask", this->llvmModule.get() );
				}
			}
			llvm::Value* awaitResult = this->irBuilder.CreateCall( awaitFunc, { taskIdValue }, "await.result" );
			bool hasErrorReturnsI1 = false;
			llvm::Function* hasErrorFunc = this->llvmModule->getFunction( "runtimeTaskHasError" );
			if( hasErrorFunc != nullptr ) {
				hasErrorReturnsI1 = true;
			}
			else {
				hasErrorFunc = this->llvmModule->getFunction( "uraniteTaskHasError" );
				if( hasErrorFunc == nullptr ) {
					llvm::FunctionType* hasErrorSig = llvm::FunctionType::get(
						llvm::Type::getInt32Ty( this->llvmContext ), { i64Type }, false
					);
					hasErrorFunc = llvm::Function::Create( hasErrorSig, llvm::Function::ExternalLinkage, "uraniteTaskHasError", this->llvmModule.get() );
				}
			}
			llvm::Value* taskIdReload = this->irBuilder.CreateLoad( i64Type, taskIdAlloca, "await.tid.reload" );
			llvm::Value* hasErrorResult = this->irBuilder.CreateCall( hasErrorFunc, { taskIdReload }, "await.haserr" );
			llvm::Value* isError;
			if( hasErrorReturnsI1 ) {
				isError = hasErrorResult;
			}
			else {
				isError = this->irBuilder.CreateICmpNE(
					hasErrorResult, llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->llvmContext ), 0 ), "await.iserr"
				);
			}
			llvm::BasicBlock* awaitOkBlock = llvm::BasicBlock::Create( this->llvmContext, "await.ok", currentFunc );
			llvm::BasicBlock* awaitErrBlock = llvm::BasicBlock::Create( this->llvmContext, "await.err", currentFunc );
			this->irBuilder.CreateCondBr( isError, awaitErrBlock, awaitOkBlock );
			this->irBuilder.SetInsertPoint( awaitErrBlock );
			{
				llvm::Function* getErrorFunc = this->llvmModule->getFunction( "runtimeTaskGetError" );
				if( getErrorFunc == nullptr ) {
					getErrorFunc = this->llvmModule->getFunction( "uraniteTaskGetError" );
					if( getErrorFunc == nullptr ) {
						llvm::FunctionType* getErrorSig = llvm::FunctionType::get(
							llvm::PointerType::getUnqual( this->llvmContext ), { i64Type }, false
						);
						getErrorFunc = llvm::Function::Create( getErrorSig, llvm::Function::ExternalLinkage, "uraniteTaskGetError", this->llvmModule.get() );
					}
				}
				llvm::Value* taskIdReload2 = this->irBuilder.CreateLoad( i64Type, taskIdAlloca, "await.tid.reload2" );
				llvm::Value* errorMsg = this->irBuilder.CreateCall( getErrorFunc, { taskIdReload2 }, "await.errmsg" );
				llvm::StructType* exceptionStruct = nullptr;
				if( this->structTypeCache.count( semantic::qualname::classes::exception::Name ) > 0 ) {
					exceptionStruct = this->structTypeCache[semantic::qualname::classes::exception::Name];
				}
				if( exceptionStruct != nullptr ) {
					llvm::DataLayout dataLayout = this->llvmModule->getDataLayout();
					uint64_t exceptionSize = dataLayout.getTypeAllocSize( exceptionStruct );
					llvm::Function* mallocFunc = this->getOrCreateMalloc();
					llvm::Value* rawMem = this->irBuilder.CreateCall( mallocFunc, {
						llvm::ConstantInt::get( i64Type, exceptionSize )
					}, "await.exc.raw" );
					std::string ctorName = std::string( semantic::qualname::classes::exception::Name ) + "." + semantic::qualname::classes::exception::Name;
					llvm::Function* ctorFunc = nullptr;
					if( this->functionResolutionMap.count( ctorName ) > 0 ) {
						ctorFunc = this->functionResolutionMap[ctorName];
					}
					if( ctorFunc == nullptr ) {
						ctorFunc = this->llvmModule->getFunction( ctorName );
					}
					if( ctorFunc != nullptr ) {
						std::vector<llvm::Value*> ctorArgs;
						ctorArgs.push_back( rawMem );
						ctorArgs.push_back( errorMsg );
						ctorArgs.push_back( llvm::ConstantInt::get( i64Type, 0 ) );
						for( size_t argIdx = ctorArgs.size(); argIdx < ctorFunc->arg_size(); argIdx++ ) {
							ctorArgs.push_back( llvm::Constant::getNullValue( ctorFunc->getArg( argIdx )->getType() ) );
						}
						this->irBuilder.CreateCall( ctorFunc, ctorArgs );
					}
					llvm::Value* excTypeName = this->irBuilder.CreateGlobalStringPtr( semantic::qualname::classes::exception::Name, "await.exc.typename" );
					llvm::Function* throwFunc = this->getOrCreateUraniteThrow();
					std::vector<llvm::Value*> throwArgs = { rawMem, excTypeName };
					llvm::BasicBlock* unwindTarget = nullptr;
					if( instruction.landingPadTarget != INVALID_BLOCK_IDENTIFIER && this->blockMap.count( instruction.landingPadTarget ) > 0 ) {
						unwindTarget = this->blockMap[instruction.landingPadTarget];
					}
					else if( this->asyncWrapperCatchBlock != nullptr ) {
						unwindTarget = this->asyncWrapperCatchBlock;
					}
					if( unwindTarget != nullptr ) {
						llvm::Function* personalityFunc = this->getOrCreatePersonality();
						currentFunc->setPersonalityFn( personalityFunc );
						llvm::BasicBlock* throwUnreachBlock = llvm::BasicBlock::Create( this->llvmContext, "throw.unreachable", currentFunc );
						this->irBuilder.CreateInvoke( throwFunc, throwUnreachBlock, unwindTarget, throwArgs );
						this->irBuilder.SetInsertPoint( throwUnreachBlock );
					}
					else {
						this->irBuilder.CreateCall( throwFunc, throwArgs );
					}
				}
				this->irBuilder.CreateUnreachable();
			}
			this->irBuilder.SetInsertPoint( awaitOkBlock );
			if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
				this->setVariableValue( instruction.destinationVariable, awaitResult );
			}
			return;
		}
		if( calledName == "__builtin_strlen" ) {
			llvm::Value* strValue = this->loadVariableValue( instruction.sourceOperands[0] );
			llvm::Function* strlenFunction = this->llvmModule->getFunction( "strlen" );
			bool userShadowed = ( strlenFunction != nullptr && strlenFunction->empty() == false );
			if( userShadowed ) {
				llvm::Function* currentFunc = this->irBuilder.GetInsertBlock()->getParent();
				llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
				llvm::Type* i8Type = llvm::Type::getInt8Ty( this->llvmContext );
				llvm::BasicBlock* loopHeader = llvm::BasicBlock::Create( this->llvmContext, "slen.hdr", currentFunc );
				llvm::BasicBlock* loopBody = llvm::BasicBlock::Create( this->llvmContext, "slen.body", currentFunc );
				llvm::BasicBlock* loopExit = llvm::BasicBlock::Create( this->llvmContext, "slen.exit", currentFunc );
				llvm::Value* idxAlloca = this->irBuilder.CreateAlloca( i64Type, nullptr, "slen.idx" );
				this->irBuilder.CreateStore( llvm::ConstantInt::get( i64Type, 0 ), idxAlloca );
				this->irBuilder.CreateBr( loopHeader );
				this->irBuilder.SetInsertPoint( loopHeader );
				llvm::Value* idx = this->irBuilder.CreateLoad( i64Type, idxAlloca, "slen.i" );
				llvm::Value* charPtr = this->irBuilder.CreateGEP( i8Type, strValue, idx, "slen.ptr" );
				llvm::Value* charVal = this->irBuilder.CreateLoad( i8Type, charPtr, "slen.ch" );
				llvm::Value* isNull = this->irBuilder.CreateICmpEQ(
					charVal, llvm::ConstantInt::get( i8Type, 0 ), "slen.null"
				);
				this->irBuilder.CreateCondBr( isNull, loopExit, loopBody );
				this->irBuilder.SetInsertPoint( loopBody );
				llvm::Value* nextIdx = this->irBuilder.CreateAdd( idx, llvm::ConstantInt::get( i64Type, 1 ), "slen.next" );
				this->irBuilder.CreateStore( nextIdx, idxAlloca );
				this->irBuilder.CreateBr( loopHeader );
				this->irBuilder.SetInsertPoint( loopExit );
				llvm::Value* lenResult = this->irBuilder.CreateLoad( i64Type, idxAlloca, "str.len" );
				if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
					this->setVariableValue( instruction.destinationVariable, lenResult );
				}
			}
			else {
				if( strlenFunction == nullptr ) {
					strlenFunction = this->getOrCreateStrlen();
				}
				llvm::Value* lenResult = this->irBuilder.CreateCall( strlenFunction, { strValue }, "str.len" );
				if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
					this->setVariableValue( instruction.destinationVariable, lenResult );
				}
			}
			return;
		}
		
		// Memory<T> intrinsic method calls
		if( shortCalledName == std::string( semantic::qualname::classes::memory::Name ) + "." + semantic::qualname::classes::memory::methods::Get && instruction.sourceOperands.size() >= 2 ) {
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
		if( shortCalledName == std::string( semantic::qualname::classes::memory::Name ) + "." + semantic::qualname::classes::memory::methods::Set && instruction.sourceOperands.size() >= 3 ) {
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
		if( shortCalledName == std::string( semantic::qualname::classes::memory::Name ) + "." + semantic::qualname::classes::memory::methods::Free && instruction.sourceOperands.size() >= 1 ) {
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
		if( shortCalledName == std::string( semantic::qualname::classes::memory::Name ) + "." + semantic::qualname::classes::memory::methods::CopyTo && instruction.sourceOperands.size() >= 3 ) {
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
		if( shortCalledName == std::string( semantic::qualname::classes::memory::Name ) + "." + semantic::qualname::classes::memory::Name ) {
			return;
		}
		
		// Arena<T> intrinsic method calls
		if( shortCalledName == std::string( semantic::qualname::classes::arena::Name ) + "." + semantic::qualname::classes::arena::methods::Alloc && instruction.sourceOperands.size() >= 1 ) {
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
		if( shortCalledName == std::string( semantic::qualname::classes::arena::Name ) + "." + semantic::qualname::classes::arena::methods::FreeAll && instruction.sourceOperands.size() >= 1 ) {
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
		if( shortCalledName == std::string( semantic::qualname::classes::arena::Name ) + "." + semantic::qualname::classes::arena::methods::Destroy && instruction.sourceOperands.size() >= 1 ) {
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
		if( shortCalledName == std::string( semantic::qualname::classes::arena::Name ) + "." + semantic::qualname::classes::arena::methods::Count && instruction.sourceOperands.size() >= 1 ) {
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
		if( shortCalledName == std::string( semantic::qualname::classes::arena::Name ) + "." + semantic::qualname::classes::arena::methods::Capacity && instruction.sourceOperands.size() >= 1 ) {
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
		if( shortCalledName == std::string( semantic::qualname::classes::arena::Name ) + "." + semantic::qualname::classes::arena::Name ) {
			return;
		}
		if( isBareOrObjectCall && bareMethodName == semantic::qualname::classes::object::methods::HashCode && instruction.sourceOperands.empty() == false ) {
			llvm::Value* selfValue = this->loadVariableValue( instruction.sourceOperands[0] );
			if( selfValue != nullptr ) {
				llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
				llvm::Value* hashResult = nullptr;
				bool isMaybeStringKey = false;
				if( selfValue->getType()->isIntegerTy() && this->currentMIRFunction != nullptr ) {
					std::unordered_map<MIRVariableIdentifier, MIRVariableDescriptor>::iterator descriptorIterator =
						this->currentMIRFunction->variableDescriptorTable.find( instruction.sourceOperands[0] );
					if( descriptorIterator != this->currentMIRFunction->variableDescriptorTable.end() &&
						descriptorIterator->second.variableType != nullptr ) {
						semantic::Type::Kind operandKind = descriptorIterator->second.variableType->kind;
						if( operandKind == semantic::Type::Kind::GenericParameter ||
							operandKind == semantic::Type::Kind::String ||
							( operandKind == semantic::Type::Kind::Class &&
							  descriptorIterator->second.variableType->name == semantic::qualname::classes::string::Name ) ) {
							isMaybeStringKey = true;
						}
					}
				}
				if( isMaybeStringKey ) {
					llvm::Value* pageThreshold = llvm::ConstantInt::get( i64Type, 4096 );
					llvm::Value* aboveThreshold = this->irBuilder.CreateICmpUGE( selfValue, pageThreshold, "hash.isptr" );
					llvm::Function* currentFunction = this->irBuilder.GetInsertBlock()->getParent();
					llvm::BasicBlock* strHashBlock = llvm::BasicBlock::Create( this->llvmContext, "hash.str.call", currentFunction );
					llvm::BasicBlock* intHashBlock = llvm::BasicBlock::Create( this->llvmContext, "hash.int.use", currentFunction );
					llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create( this->llvmContext, "hash.merge", currentFunction );
					this->irBuilder.CreateCondBr( aboveThreshold, strHashBlock, intHashBlock );
					this->irBuilder.SetInsertPoint( strHashBlock );
					llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
					llvm::Value* ptrValue = this->irBuilder.CreateIntToPtr( selfValue, ptrType, "hash.key.ptr" );
					llvm::Function* stringHashFunction = this->getOrCreateStringHash();
					llvm::Value* strHash = this->irBuilder.CreateCall( stringHashFunction, { ptrValue }, "hash.str" );
					this->irBuilder.CreateBr( mergeBlock );
					llvm::BasicBlock* strHashExit = this->irBuilder.GetInsertBlock();
					this->irBuilder.SetInsertPoint( intHashBlock );
					llvm::Value* intHash = this->irBuilder.CreateIntCast( selfValue, i64Type, true, "hash.int" );
					this->irBuilder.CreateBr( mergeBlock );
					llvm::BasicBlock* intHashExit = this->irBuilder.GetInsertBlock();
					this->irBuilder.SetInsertPoint( mergeBlock );
					llvm::PHINode* phi = this->irBuilder.CreatePHI( i64Type, 2, "hash.result" );
					phi->addIncoming( strHash, strHashExit );
					phi->addIncoming( intHash, intHashExit );
					hashResult = phi;
				}
				else if( selfValue->getType()->isIntegerTy() ) {
					hashResult = this->irBuilder.CreateIntCast( selfValue, i64Type, true, "hash.int" );
				}
				else if( selfValue->getType()->isPointerTy() ) {
					bool isGenericParam = false;
					if( this->currentMIRFunction != nullptr ) {
						std::unordered_map<MIRVariableIdentifier, MIRVariableDescriptor>::iterator descriptorIterator =
							this->currentMIRFunction->variableDescriptorTable.find( instruction.sourceOperands[0] );
						if( descriptorIterator != this->currentMIRFunction->variableDescriptorTable.end() &&
							descriptorIterator->second.variableType != nullptr ) {
							isGenericParam = ( descriptorIterator->second.variableType->kind == semantic::Type::Kind::GenericParameter );
						}
					}
					if( isGenericParam ) {
						llvm::Value* ptrAsInt = this->irBuilder.CreatePtrToInt( selfValue, i64Type, "hash.ptr.int" );
						llvm::Value* pageThreshold = llvm::ConstantInt::get( i64Type, 4096 );
						llvm::Value* aboveThreshold = this->irBuilder.CreateICmpUGE( ptrAsInt, pageThreshold, "hash.isptr" );
						llvm::Function* currentFunction = this->irBuilder.GetInsertBlock()->getParent();
						llvm::BasicBlock* strHashBlock = llvm::BasicBlock::Create( this->llvmContext, "hash.str.call", currentFunction );
						llvm::BasicBlock* intHashBlock = llvm::BasicBlock::Create( this->llvmContext, "hash.int.use", currentFunction );
						llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create( this->llvmContext, "hash.merge", currentFunction );
						this->irBuilder.CreateCondBr( aboveThreshold, strHashBlock, intHashBlock );
						this->irBuilder.SetInsertPoint( strHashBlock );
						llvm::Function* stringHashFunction = this->getOrCreateStringHash();
						llvm::Value* strHash = this->irBuilder.CreateCall( stringHashFunction, { selfValue }, "hash.str" );
						this->irBuilder.CreateBr( mergeBlock );
						llvm::BasicBlock* strHashExit = this->irBuilder.GetInsertBlock();
						this->irBuilder.SetInsertPoint( intHashBlock );
						llvm::Value* intHash = ptrAsInt;
						this->irBuilder.CreateBr( mergeBlock );
						llvm::BasicBlock* intHashExit = this->irBuilder.GetInsertBlock();
						this->irBuilder.SetInsertPoint( mergeBlock );
						llvm::PHINode* phi = this->irBuilder.CreatePHI( i64Type, 2, "hash.result" );
						phi->addIncoming( strHash, strHashExit );
						phi->addIncoming( intHash, intHashExit );
						hashResult = phi;
					}
					else {
						llvm::Function* stringHashFunction = this->getOrCreateStringHash();
						hashResult = this->irBuilder.CreateCall( stringHashFunction, { selfValue }, "hash.str" );
					}
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
		if( isBareOrObjectCall && bareMethodName == semantic::qualname::classes::object::methods::ToString && instruction.sourceOperands.empty() == false ) {
			llvm::Value* selfValue = this->loadVariableValue( instruction.sourceOperands[0] );
			if( selfValue != nullptr && selfValue->getType()->isPointerTy() ) {
				llvm::Value* isNull = this->irBuilder.CreateICmpEQ(
					selfValue,
					llvm::ConstantPointerNull::get( llvm::PointerType::getUnqual( this->llvmContext ) ),
					"tostr.is.none"
				);
				llvm::Value* noneStr = this->irBuilder.CreateGlobalStringPtr( "None", "str.none" );
				llvm::Value* toStringResult = this->irBuilder.CreateSelect( isNull, noneStr, selfValue, "tostr.result" );
				this->setVariableValue( instruction.destinationVariable, toStringResult );
				return;
			}
		}
		if( isBareOrObjectCall && bareMethodName == semantic::qualname::classes::string::methods::CharCodeAt && instruction.sourceOperands.size() >= 2 ) {
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
			const std::unordered_map<std::string, int>& integerWrapperBitWidths = descriptor::Builtin::integerBitWidths;
			const std::set<std::string>& floatWrapperNames = descriptor::Builtin::floatOopNames;
			const std::unordered_set<std::string>& unsignedWrapperNames = descriptor::Builtin::unsignedOopNames;
			size_t dotPosition = shortCalledName.find( '.' );
			if( dotPosition != std::string::npos ) {
				std::string wrapperName = shortCalledName.substr( 0, dotPosition );
				std::string methodName = shortCalledName.substr( dotPosition + 1 );
				if( this->tryBuiltinDescriptor( wrapperName, methodName, instruction, functionDefinition ) ) {
					return;
				}
				bool isIntegerWrapper = integerWrapperBitWidths.count( wrapperName ) > 0;
				bool isFloatWrapper = floatWrapperNames.count( wrapperName ) > 0;
				bool isUnsigned = unsignedWrapperNames.count( wrapperName ) > 0;
				if( methodName == semantic::qualname::classes::object::methods::HashCode && instruction.sourceOperands.empty() == false ) {
					llvm::Value* selfValue = this->loadVariableValue( instruction.sourceOperands[0] );
					if( selfValue != nullptr ) {
						llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
						llvm::Value* hashResult = nullptr;
						bool isMaybeStringKey = false;
						if( selfValue->getType()->isIntegerTy() && this->currentMIRFunction != nullptr ) {
							std::unordered_map<MIRVariableIdentifier, MIRVariableDescriptor>::iterator descriptorIterator =
								this->currentMIRFunction->variableDescriptorTable.find( instruction.sourceOperands[0] );
							if( descriptorIterator != this->currentMIRFunction->variableDescriptorTable.end() &&
								descriptorIterator->second.variableType != nullptr ) {
								semantic::Type::Kind operandKind = descriptorIterator->second.variableType->kind;
								if( operandKind == semantic::Type::Kind::GenericParameter ||
									operandKind == semantic::Type::Kind::String ||
									( operandKind == semantic::Type::Kind::Class &&
									  descriptorIterator->second.variableType->name == semantic::qualname::classes::string::Name ) ) {
									isMaybeStringKey = true;
								}
							}
						}
						if( wrapperName == semantic::qualname::classes::string::Name && selfValue->getType()->isIntegerTy( 64 ) ) {
							llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
							llvm::Value* ptrValue = this->irBuilder.CreateIntToPtr( selfValue, ptrType, "hash.key.ptr" );
							llvm::Function* stringHashFunction = this->getOrCreateStringHash();
							hashResult = this->irBuilder.CreateCall( stringHashFunction, { ptrValue }, "hash.str" );
						}
						else if( isMaybeStringKey ) {
							llvm::Value* pageThreshold = llvm::ConstantInt::get( i64Type, 4096 );
							llvm::Value* aboveThreshold = this->irBuilder.CreateICmpUGE( selfValue, pageThreshold, "hash.isptr" );
							llvm::Function* currentFunction = this->irBuilder.GetInsertBlock()->getParent();
							llvm::BasicBlock* strHashBlock = llvm::BasicBlock::Create( this->llvmContext, "hash.str.call", currentFunction );
							llvm::BasicBlock* intHashBlock = llvm::BasicBlock::Create( this->llvmContext, "hash.int.use", currentFunction );
							llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create( this->llvmContext, "hash.merge", currentFunction );
							this->irBuilder.CreateCondBr( aboveThreshold, strHashBlock, intHashBlock );
							this->irBuilder.SetInsertPoint( strHashBlock );
							llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
							llvm::Value* ptrValue = this->irBuilder.CreateIntToPtr( selfValue, ptrType, "hash.key.ptr" );
							llvm::Function* stringHashFunction = this->getOrCreateStringHash();
							llvm::Value* strHash = this->irBuilder.CreateCall( stringHashFunction, { ptrValue }, "hash.str" );
							this->irBuilder.CreateBr( mergeBlock );
							llvm::BasicBlock* strHashExit = this->irBuilder.GetInsertBlock();
							this->irBuilder.SetInsertPoint( intHashBlock );
							llvm::Value* intHash = this->irBuilder.CreateIntCast( selfValue, i64Type, true, "hash.int" );
							this->irBuilder.CreateBr( mergeBlock );
							llvm::BasicBlock* intHashExit = this->irBuilder.GetInsertBlock();
							this->irBuilder.SetInsertPoint( mergeBlock );
							llvm::PHINode* phi = this->irBuilder.CreatePHI( i64Type, 2, "hash.result" );
							phi->addIncoming( strHash, strHashExit );
							phi->addIncoming( intHash, intHashExit );
							hashResult = phi;
						}
						else if( selfValue->getType()->isIntegerTy() ) {
							hashResult = this->irBuilder.CreateIntCast( selfValue, i64Type, true, "hash.int" );
						}
						else if( selfValue->getType()->isPointerTy() ) {
							bool isGenericParam = false;
							if( this->currentMIRFunction != nullptr ) {
								std::unordered_map<MIRVariableIdentifier, MIRVariableDescriptor>::iterator descriptorIterator =
									this->currentMIRFunction->variableDescriptorTable.find( instruction.sourceOperands[0] );
								if( descriptorIterator != this->currentMIRFunction->variableDescriptorTable.end() &&
									descriptorIterator->second.variableType != nullptr ) {
									isGenericParam = ( descriptorIterator->second.variableType->kind == semantic::Type::Kind::GenericParameter );
								}
							}
							if( isGenericParam ) {
								llvm::Value* ptrAsInt = this->irBuilder.CreatePtrToInt( selfValue, i64Type, "hash.ptr.int" );
								llvm::Value* pageThreshold = llvm::ConstantInt::get( i64Type, 4096 );
								llvm::Value* aboveThreshold = this->irBuilder.CreateICmpUGE( ptrAsInt, pageThreshold, "hash.isptr" );
								llvm::Function* currentFunction = this->irBuilder.GetInsertBlock()->getParent();
								llvm::BasicBlock* strHashBlock = llvm::BasicBlock::Create( this->llvmContext, "hash.str.call", currentFunction );
								llvm::BasicBlock* intHashBlock = llvm::BasicBlock::Create( this->llvmContext, "hash.int.use", currentFunction );
								llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create( this->llvmContext, "hash.merge", currentFunction );
								this->irBuilder.CreateCondBr( aboveThreshold, strHashBlock, intHashBlock );
								this->irBuilder.SetInsertPoint( strHashBlock );
								llvm::Function* stringHashFunction = this->getOrCreateStringHash();
								llvm::Value* strHash = this->irBuilder.CreateCall( stringHashFunction, { selfValue }, "hash.str" );
								this->irBuilder.CreateBr( mergeBlock );
								llvm::BasicBlock* strHashExit = this->irBuilder.GetInsertBlock();
								this->irBuilder.SetInsertPoint( intHashBlock );
								llvm::Value* intHash = ptrAsInt;
								this->irBuilder.CreateBr( mergeBlock );
								llvm::BasicBlock* intHashExit = this->irBuilder.GetInsertBlock();
								this->irBuilder.SetInsertPoint( mergeBlock );
								llvm::PHINode* phi = this->irBuilder.CreatePHI( i64Type, 2, "hash.result" );
								phi->addIncoming( strHash, strHashExit );
								phi->addIncoming( intHash, intHashExit );
								hashResult = phi;
							}
							else {
								llvm::Function* stringHashFunction = this->getOrCreateStringHash();
								hashResult = this->irBuilder.CreateCall( stringHashFunction, { selfValue }, "hash.str" );
							}
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
				if( wrapperName == semantic::qualname::classes::string::Name ) {
					llvm::Value* selfValue = this->loadVariableValue( instruction.sourceOperands[0] );
					if( selfValue != nullptr ) {
						if( methodName == semantic::qualname::classes::object::methods::ToString ) {
							this->setVariableValue( instruction.destinationVariable, selfValue );
							return;
						}
						if( methodName == semantic::qualname::classes::string::methods::Substring && instruction.sourceOperands.size() >= 3 ) {
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
						if( methodName == semantic::qualname::interfaces::equatable::methods::Equals && instruction.sourceOperands.size() >= 2 ) {
							llvm::Value* otherValue = this->loadVariableValue( instruction.sourceOperands[1] );
							llvm::Function* strcmpFunction = this->getOrCreateStrcmp();
							llvm::Value* cmpResult = this->irBuilder.CreateCall( strcmpFunction, { selfValue, otherValue }, "str.cmp" );
							llvm::Value* isEqual = this->irBuilder.CreateICmpEQ(
								cmpResult, llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->llvmContext ), 0 ), "str.eq"
							);
							this->setVariableValue( instruction.destinationVariable, isEqual );
							return;
						}
						if( methodName == semantic::qualname::classes::string::methods::Length ) {
							llvm::Function* strlenFunction = this->getOrCreateStrlen();
							llvm::Value* lenResult = this->irBuilder.CreateCall( strlenFunction, { selfValue }, "str.len" );
							this->setVariableValue( instruction.destinationVariable, lenResult );
							return;
						}
						if( methodName == semantic::qualname::classes::string::methods::CharCodeAt && instruction.sourceOperands.size() >= 2 ) {
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
						if( methodName == semantic::qualname::classes::object::methods::GetValue || methodName == semantic::qualname::classes::object::methods::Value ) {
							this->setVariableValue( instruction.destinationVariable, selfValue );
							return;
						}
						if( methodName == semantic::qualname::classes::string::methods::IsEmpty ) {
							llvm::Value* lenResult = this->irBuilder.CreateCall( this->getOrCreateStrlen(), { selfValue }, "str.len" );
							llvm::Value* isZero = this->irBuilder.CreateICmpEQ(
								lenResult, llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->llvmContext ), 0 ), "str.empty" );
							this->setVariableValue( instruction.destinationVariable, isZero );
							return;
						}
						if( methodName == semantic::qualname::classes::string::methods::Format && instruction.sourceOperands.size() >= 2 ) {
							llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
							llvm::Type* i8Type = llvm::Type::getInt8Ty( this->llvmContext );
							llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
							llvm::Function* strlenFunc = this->getOrCreateStrlen();
							llvm::Function* mallocFunc = this->getOrCreateMalloc();
							llvm::Function* memcpyFunc = this->getOrCreateMemcpy();
							llvm::Function* strstrFunc = this->getOrCreateStrstr();
							llvm::Function* snprintfFunc = this->getOrCreateSnprintf();
							llvm::Value* placeholderStr = this->irBuilder.CreateGlobalStringPtr( "{}", "fmt.ph" );
							llvm::Value* currentResult = selfValue;
							for( size_t formatArgIndex = 1; formatArgIndex < instruction.sourceOperands.size(); ++formatArgIndex ) {
								llvm::Value* argValue = this->loadVariableValue( instruction.sourceOperands[formatArgIndex] );
								if( argValue == nullptr ) {
									continue;
								}
								llvm::Value* argStr = argValue;
								if( argValue->getType()->isPointerTy() ) {
									semantic::TypeSharedPointer formatArgType = nullptr;
									if( this->currentMIRFunction != nullptr ) {
										std::unordered_map<MIRVariableIdentifier, MIRVariableDescriptor>::iterator descriptorIt =
											this->currentMIRFunction->variableDescriptorTable.find( instruction.sourceOperands[formatArgIndex] );
										if( descriptorIt != this->currentMIRFunction->variableDescriptorTable.end() &&
											descriptorIt->second.variableType != nullptr ) {
											formatArgType = descriptorIt->second.variableType;
										}
									}
									if( formatArgType != nullptr &&
										formatArgType->kind == semantic::Type::Kind::Class &&
										formatArgType->name != semantic::qualname::classes::string::Name ) {
										std::string fmtArgTypeName = formatArgType->name;
										size_t fmtArgGenericPos = fmtArgTypeName.find( '<' );
										if( fmtArgGenericPos != std::string::npos ) {
											fmtArgTypeName = fmtArgTypeName.substr( 0, fmtArgGenericPos );
										}
										bool isPrimitiveOopWrapper =
											descriptor::Builtin::integerBitWidths.count( fmtArgTypeName ) > 0 ||
											descriptor::Builtin::floatOopNames.count( fmtArgTypeName ) > 0 ||
											fmtArgTypeName == semantic::qualname::classes::boolean::Name;
										if( isPrimitiveOopWrapper ) {
											llvm::Value* primitiveIntVal = this->irBuilder.CreatePtrToInt(
												argValue, i64Type, "fmt.prim.ptoi"
											);
											if( fmtArgTypeName == semantic::qualname::classes::boolean::Name ) {
												llvm::Value* boolVal = this->irBuilder.CreateTrunc(
													primitiveIntVal, llvm::Type::getInt1Ty( this->llvmContext ), "fmt.prim.bool"
												);
												llvm::Value* trueStr = this->irBuilder.CreateGlobalStringPtr( "True", "fmt.prim.true" );
												llvm::Value* falseStr = this->irBuilder.CreateGlobalStringPtr( "False", "fmt.prim.false" );
												argStr = this->irBuilder.CreateSelect( boolVal, trueStr, falseStr, "fmt.prim.boolstr" );
											}
											else if( descriptor::Builtin::floatOopNames.count( fmtArgTypeName ) > 0 ) {
												llvm::Value* floatVal = this->irBuilder.CreateBitCast(
													primitiveIntVal, llvm::Type::getDoubleTy( this->llvmContext ), "fmt.prim.fp"
												);
												llvm::Value* buf = this->irBuilder.CreateCall( mallocFunc, {
													llvm::ConstantInt::get( i64Type, 48 )
												}, "fmt.prim.flt.buf" );
												llvm::Value* fltFmt = this->irBuilder.CreateGlobalStringPtr( "%g", "fmt.prim.flt.fmt" );
												this->irBuilder.CreateCall( snprintfFunc, {
													buf, llvm::ConstantInt::get( i64Type, 48 ), fltFmt, floatVal
												} );
												argStr = buf;
											}
											else if( fmtArgTypeName == semantic::qualname::classes::Char::Name ) {
												llvm::Value* buf = this->irBuilder.CreateCall( mallocFunc, {
													llvm::ConstantInt::get( i64Type, 24 )
												}, "fmt.prim.char.buf" );
												llvm::Value* charFmt = this->irBuilder.CreateGlobalStringPtr( "%c", "fmt.prim.char.fmt" );
												this->irBuilder.CreateCall( snprintfFunc, {
													buf, llvm::ConstantInt::get( i64Type, 24 ), charFmt, primitiveIntVal
												} );
												argStr = buf;
											}
											else {
												bool isUnsigned = descriptor::Builtin::unsignedOopNames.count( fmtArgTypeName ) > 0;
												llvm::Value* buf = this->irBuilder.CreateCall( mallocFunc, {
													llvm::ConstantInt::get( i64Type, 24 )
												}, "fmt.prim.int.buf" );
												llvm::Value* intFmt = this->irBuilder.CreateGlobalStringPtr(
													isUnsigned ? "%lu" : "%ld", "fmt.prim.int.fmt"
												);
												this->irBuilder.CreateCall( snprintfFunc, {
													buf, llvm::ConstantInt::get( i64Type, 24 ), intFmt, primitiveIntVal
												} );
												argStr = buf;
											}
										}
										else {
											std::string fmtArgQualName = formatArgType->qualified.empty() == false
												? formatArgType->qualified : formatArgType->name;
											size_t fmtArgQualGenPos = fmtArgQualName.find( '<' );
											if( fmtArgQualGenPos != std::string::npos ) {
												fmtArgQualName = fmtArgQualName.substr( 0, fmtArgQualGenPos );
											}
											std::string fmtArgToStrName = fmtArgQualName + "." +
												semantic::qualname::classes::object::methods::ToString;
											std::unordered_map<std::string, llvm::Function*>::iterator fmtToStrIt =
												this->functionResolutionMap.find( fmtArgToStrName );
											if( fmtToStrIt != this->functionResolutionMap.end() ) {
												argStr = this->irBuilder.CreateCall(
													fmtToStrIt->second, { argValue }, "fmt.cls.tostr"
												);
											}
										}
									}
									else if( formatArgType != nullptr &&
										formatArgType->kind == semantic::Type::Kind::Interface ) {
										llvm::Type* vtablePtrType = llvm::PointerType::getUnqual( this->llvmContext );
										std::vector<llvm::Type*> vtableWrapperFields = { vtablePtrType };
										llvm::Value* vtableSlotPtr = this->irBuilder.CreateStructGEP(
											llvm::StructType::get( this->llvmContext, vtableWrapperFields ),
											argValue, 0, "fmt.iface.vtable.slot"
										);
										llvm::Value* vtablePtr = this->irBuilder.CreateLoad(
											vtablePtrType, vtableSlotPtr, "fmt.iface.vtable.ptr"
										);
										llvm::FunctionType* toStringVoidType = llvm::FunctionType::get(
											llvm::Type::getVoidTy( this->llvmContext ), false
										);
										llvm::PointerType* toStringFuncPtrType = llvm::PointerType::getUnqual( toStringVoidType );
										llvm::Value* toStringSlotPtr = this->irBuilder.CreateGEP(
											toStringFuncPtrType, vtablePtr,
											llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->llvmContext ), 0 ),
											"fmt.iface.tostr.slot"
										);
										llvm::Value* toStringFuncPtr = this->irBuilder.CreateLoad(
											toStringFuncPtrType, toStringSlotPtr, "fmt.iface.tostr.ptr"
										);
										llvm::FunctionType* toStringCallType = llvm::FunctionType::get(
											llvm::PointerType::getUnqual( this->llvmContext ),
											{ llvm::PointerType::getUnqual( this->llvmContext ) }, false
										);
										llvm::Value* toStringCastedFunc = this->irBuilder.CreateBitCast(
											toStringFuncPtr, llvm::PointerType::getUnqual( toStringCallType ), "fmt.iface.tostr.cast"
										);
										argStr = this->irBuilder.CreateCall(
											toStringCallType, toStringCastedFunc, { argValue }, "fmt.iface.tostr.result"
										);
									}
									else if( formatArgType != nullptr &&
										formatArgType->kind == semantic::Type::Kind::GenericParameter ) {
										llvm::Value* ptrAsInt = this->irBuilder.CreatePtrToInt( argValue, i64Type, "fmt.gen.ptoi" );
										llvm::Value* threshold = llvm::ConstantInt::get( i64Type, 4096 );
										llvm::Value* isRealPtr = this->irBuilder.CreateICmpUGE( ptrAsInt, threshold, "fmt.gen.isptr" );
										llvm::Function* currentFunc = this->irBuilder.GetInsertBlock()->getParent();
										llvm::BasicBlock* strBlock = llvm::BasicBlock::Create( this->llvmContext, "fmt.gen.str", currentFunc );
										llvm::BasicBlock* intBlock = llvm::BasicBlock::Create( this->llvmContext, "fmt.gen.int", currentFunc );
										llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create( this->llvmContext, "fmt.gen.merge", currentFunc );
										this->irBuilder.CreateCondBr( isRealPtr, strBlock, intBlock );
										this->irBuilder.SetInsertPoint( strBlock );
										this->irBuilder.CreateBr( mergeBlock );
										llvm::BasicBlock* strExit = this->irBuilder.GetInsertBlock();
										this->irBuilder.SetInsertPoint( intBlock );
										llvm::Value* intBuf = this->irBuilder.CreateCall( mallocFunc, {
											llvm::ConstantInt::get( i64Type, 24 )
										}, "fmt.gen.int.buf" );
										llvm::Value* intFmt = this->irBuilder.CreateGlobalStringPtr( "%ld", "fmt.gen.int.fmt" );
										this->irBuilder.CreateCall( snprintfFunc, {
											intBuf, llvm::ConstantInt::get( i64Type, 24 ), intFmt, ptrAsInt
										} );
										this->irBuilder.CreateBr( mergeBlock );
										llvm::BasicBlock* intExit = this->irBuilder.GetInsertBlock();
										this->irBuilder.SetInsertPoint( mergeBlock );
										llvm::PHINode* phi = this->irBuilder.CreatePHI( ptrType, 2, "fmt.gen.other" );
										phi->addIncoming( argValue, strExit );
										phi->addIncoming( intBuf, intExit );
										argStr = phi;
									}
								}
								else if( argValue->getType()->isDoubleTy() || argValue->getType()->isFloatTy() ) {
									llvm::Value* dblVal = argValue;
									if( argValue->getType()->isFloatTy() ) {
										dblVal = this->irBuilder.CreateFPExt( argValue, llvm::Type::getDoubleTy( this->llvmContext ), "fmt.f2d" );
									}
									llvm::Value* buf = this->irBuilder.CreateCall( mallocFunc, {
										llvm::ConstantInt::get( i64Type, 48 )
									}, "fmt.flt.buf" );
									llvm::Value* fltFmt = this->irBuilder.CreateGlobalStringPtr( "%.6g", "fmt.flt.fmt" );
									this->irBuilder.CreateCall( snprintfFunc, {
										buf, llvm::ConstantInt::get( i64Type, 48 ), fltFmt, dblVal
									} );
									argStr = buf;
								}
								else if( argValue->getType()->isIntegerTy() ) {
									if( argValue->getType()->isIntegerTy( 1 ) ) {
										llvm::Value* trueStr = this->irBuilder.CreateGlobalStringPtr( "True", "fmt.bool.t" );
										llvm::Value* falseStr = this->irBuilder.CreateGlobalStringPtr( "False", "fmt.bool.f" );
										argStr = this->irBuilder.CreateSelect( argValue, trueStr, falseStr, "fmt.bool" );
									}
									else {
										llvm::Value* intVal = argValue;
										if( argValue->getType()->getIntegerBitWidth() < 64 ) {
											intVal = this->irBuilder.CreateSExt( argValue, i64Type, "fmt.sext" );
										}
										llvm::Value* buf = this->irBuilder.CreateCall( mallocFunc, {
											llvm::ConstantInt::get( i64Type, 24 )
										}, "fmt.int.buf" );
										llvm::Value* intFmt = this->irBuilder.CreateGlobalStringPtr( "%ld", "fmt.int.fmt" );
										this->irBuilder.CreateCall( snprintfFunc, {
											buf, llvm::ConstantInt::get( i64Type, 24 ), intFmt, intVal
										} );
										argStr = buf;
									}
								}
								llvm::Value* foundPos = this->irBuilder.CreateCall( strstrFunc, { currentResult, placeholderStr }, "fmt.find" );
								llvm::Value* notFound = this->irBuilder.CreateICmpEQ( foundPos, llvm::ConstantPointerNull::get(
									llvm::PointerType::getUnqual( this->llvmContext ) ), "fmt.nf" );
								llvm::Function* currentFunc = this->irBuilder.GetInsertBlock()->getParent();
								llvm::BasicBlock* replaceBlock = llvm::BasicBlock::Create( this->llvmContext, "fmt.replace", currentFunc );
								llvm::BasicBlock* skipBlock = llvm::BasicBlock::Create( this->llvmContext, "fmt.skip", currentFunc );
								llvm::BasicBlock* continueBlock = llvm::BasicBlock::Create( this->llvmContext, "fmt.cont", currentFunc );
								this->irBuilder.CreateCondBr( notFound, skipBlock, replaceBlock );
								this->irBuilder.SetInsertPoint( replaceBlock );
								llvm::Value* prefixLen = this->irBuilder.CreatePtrDiff( i8Type, foundPos, currentResult, "fmt.prefix.len" );
								llvm::Value* suffixPtr = this->irBuilder.CreateGEP( i8Type, foundPos,
									llvm::ConstantInt::get( i64Type, 2 ), "fmt.suffix.ptr" );
								llvm::Value* argLen = this->irBuilder.CreateCall( strlenFunc, { argStr }, "fmt.arg.len" );
								llvm::Value* suffixLen = this->irBuilder.CreateCall( strlenFunc, { suffixPtr }, "fmt.suffix.len" );
								llvm::Value* totalLen = this->irBuilder.CreateAdd( prefixLen, argLen, "fmt.total.1" );
								totalLen = this->irBuilder.CreateAdd( totalLen, suffixLen, "fmt.total.2" );
								llvm::Value* allocSize = this->irBuilder.CreateAdd( totalLen,
									llvm::ConstantInt::get( i64Type, 1 ), "fmt.alloc" );
								llvm::Value* resultBuf = this->irBuilder.CreateCall( mallocFunc, { allocSize }, "fmt.buf" );
								this->irBuilder.CreateCall( memcpyFunc, {
									resultBuf, currentResult, prefixLen, this->irBuilder.getFalse()
								} );
								llvm::Value* afterPrefix = this->irBuilder.CreateGEP( i8Type, resultBuf, prefixLen, "fmt.ap" );
								this->irBuilder.CreateCall( memcpyFunc, {
									afterPrefix, argStr, argLen, this->irBuilder.getFalse()
								} );
								llvm::Value* afterArg = this->irBuilder.CreateGEP( i8Type, afterPrefix, argLen, "fmt.aa" );
								llvm::Value* suffixCopyLen = this->irBuilder.CreateAdd( suffixLen,
									llvm::ConstantInt::get( i64Type, 1 ), "fmt.suffix.copy" );
								this->irBuilder.CreateCall( memcpyFunc, {
									afterArg, suffixPtr, suffixCopyLen, this->irBuilder.getFalse()
								} );
								this->irBuilder.CreateBr( continueBlock );
								llvm::BasicBlock* replaceExit = this->irBuilder.GetInsertBlock();
								this->irBuilder.SetInsertPoint( skipBlock );
								this->irBuilder.CreateBr( continueBlock );
								this->irBuilder.SetInsertPoint( continueBlock );
								llvm::PHINode* resultPhi = this->irBuilder.CreatePHI( ptrType, 2, "fmt.result" );
								resultPhi->addIncoming( resultBuf, replaceExit );
								resultPhi->addIncoming( currentResult, skipBlock );
								currentResult = resultPhi;
							}
							this->setVariableValue( instruction.destinationVariable, currentResult );
							return;
						}
						if( methodName == semantic::qualname::classes::string::methods::Concat && instruction.sourceOperands.size() >= 2 ) {
							llvm::Value* otherValue = this->loadVariableValue( instruction.sourceOperands[1] );
							if( otherValue != nullptr ) {
								llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
								llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
								llvm::Function* strlenFunc = this->getOrCreateStrlen();
								llvm::Function* mallocFunc = this->getOrCreateMalloc();
								llvm::Function* strcpyFunc = this->getOrCreateStrcpy();
								llvm::Function* strcatFunc = this->getOrCreateStrcat();
								llvm::Function* snprintfFunc = this->getOrCreateSnprintf();
								llvm::Value* otherStr = otherValue;
								if( otherValue->getType()->isPointerTy() ) {
									semantic::TypeSharedPointer concatArgSemaType = nullptr;
									if( this->currentMIRFunction != nullptr ) {
										std::unordered_map<MIRVariableIdentifier, MIRVariableDescriptor>::iterator descriptorIt =
											this->currentMIRFunction->variableDescriptorTable.find( instruction.sourceOperands[1] );
										if( descriptorIt != this->currentMIRFunction->variableDescriptorTable.end() &&
											descriptorIt->second.variableType != nullptr ) {
											concatArgSemaType = descriptorIt->second.variableType;
										}
									}
									if( concatArgSemaType != nullptr &&
										concatArgSemaType->kind == semantic::Type::Kind::GenericParameter ) {
										// Generic element ptr may be inttoptr'd integer — runtime branch
										llvm::Value* ptrAsInt = this->irBuilder.CreatePtrToInt( otherValue, i64Type, "cat.ptoi" );
										llvm::Value* threshold = llvm::ConstantInt::get( i64Type, 4096 );
										llvm::Value* isRealPtr = this->irBuilder.CreateICmpUGE( ptrAsInt, threshold, "cat.isptr" );
										llvm::Function* currentFunc = this->irBuilder.GetInsertBlock()->getParent();
										llvm::BasicBlock* strBlock = llvm::BasicBlock::Create( this->llvmContext, "cat.str.use", currentFunc );
										llvm::BasicBlock* intBlock = llvm::BasicBlock::Create( this->llvmContext, "cat.int.conv", currentFunc );
										llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create( this->llvmContext, "cat.conv.merge", currentFunc );
										this->irBuilder.CreateCondBr( isRealPtr, strBlock, intBlock );
										this->irBuilder.SetInsertPoint( strBlock );
										this->irBuilder.CreateBr( mergeBlock );
										llvm::BasicBlock* strExit = this->irBuilder.GetInsertBlock();
										this->irBuilder.SetInsertPoint( intBlock );
										llvm::Value* intBuf = this->irBuilder.CreateCall( mallocFunc, {
											llvm::ConstantInt::get( i64Type, 24 )
										}, "cat.int.buf" );
										llvm::Value* intFmt = this->irBuilder.CreateGlobalStringPtr( "%ld", "cat.int.fmt" );
										this->irBuilder.CreateCall( snprintfFunc, {
											intBuf, llvm::ConstantInt::get( i64Type, 24 ), intFmt, ptrAsInt
										} );
										this->irBuilder.CreateBr( mergeBlock );
										llvm::BasicBlock* intExit = this->irBuilder.GetInsertBlock();
										this->irBuilder.SetInsertPoint( mergeBlock );
										llvm::PHINode* phi = this->irBuilder.CreatePHI( ptrType, 2, "cat.other" );
										phi->addIncoming( otherValue, strExit );
										phi->addIncoming( intBuf, intExit );
										otherStr = phi;
									}
									else if( concatArgSemaType != nullptr &&
										concatArgSemaType->kind == semantic::Type::Kind::Class &&
										concatArgSemaType->name != semantic::qualname::classes::string::Name ) {
										std::string concatArgTypeName = concatArgSemaType->name;
										size_t concatArgGenericPos = concatArgTypeName.find( '<' );
										if( concatArgGenericPos != std::string::npos ) {
											concatArgTypeName = concatArgTypeName.substr( 0, concatArgGenericPos );
										}
										bool isPrimitiveOopWrapper =
											descriptor::Builtin::integerBitWidths.count( concatArgTypeName ) > 0 ||
											descriptor::Builtin::floatOopNames.count( concatArgTypeName ) > 0 ||
											concatArgTypeName == semantic::qualname::classes::boolean::Name;
										if( isPrimitiveOopWrapper ) {
											llvm::Value* primitiveIntVal = this->irBuilder.CreatePtrToInt(
												otherValue, i64Type, "cat.prim.ptoi"
											);
											if( concatArgTypeName == semantic::qualname::classes::boolean::Name ) {
												llvm::Value* boolVal = this->irBuilder.CreateTrunc(
													primitiveIntVal, llvm::Type::getInt1Ty( this->llvmContext ), "cat.prim.bool"
												);
												llvm::Value* trueStr = this->irBuilder.CreateGlobalStringPtr( "True", "cat.prim.true" );
												llvm::Value* falseStr = this->irBuilder.CreateGlobalStringPtr( "False", "cat.prim.false" );
												otherStr = this->irBuilder.CreateSelect( boolVal, trueStr, falseStr, "cat.prim.boolstr" );
											}
											else if( descriptor::Builtin::floatOopNames.count( concatArgTypeName ) > 0 ) {
												llvm::Value* floatVal = this->irBuilder.CreateBitCast(
													primitiveIntVal, llvm::Type::getDoubleTy( this->llvmContext ), "cat.prim.fp"
												);
												llvm::Value* primitiveBuf = this->irBuilder.CreateCall( mallocFunc, {
													llvm::ConstantInt::get( i64Type, 48 )
												}, "cat.prim.flt.buf" );
												llvm::Value* fltFmt = this->irBuilder.CreateGlobalStringPtr( "%g", "cat.prim.flt.fmt" );
												this->irBuilder.CreateCall( snprintfFunc, {
													primitiveBuf, llvm::ConstantInt::get( i64Type, 48 ), fltFmt, floatVal
												} );
												otherStr = primitiveBuf;
											}
											else if( concatArgTypeName == semantic::qualname::classes::Char::Name ) {
												llvm::Value* primitiveBuf = this->irBuilder.CreateCall( mallocFunc, {
													llvm::ConstantInt::get( i64Type, 24 )
												}, "cat.prim.char.buf" );
												llvm::Value* charFmt = this->irBuilder.CreateGlobalStringPtr( "%c", "cat.prim.char.fmt" );
												this->irBuilder.CreateCall( snprintfFunc, {
													primitiveBuf, llvm::ConstantInt::get( i64Type, 24 ), charFmt, primitiveIntVal
												} );
												otherStr = primitiveBuf;
											}
											else {
												bool isUnsignedWrapper = descriptor::Builtin::unsignedOopNames.count( concatArgTypeName ) > 0;
												llvm::Value* primitiveBuf = this->irBuilder.CreateCall( mallocFunc, {
													llvm::ConstantInt::get( i64Type, 24 )
												}, "cat.prim.int.buf" );
												llvm::Value* intFmt = this->irBuilder.CreateGlobalStringPtr(
													isUnsignedWrapper ? "%lu" : "%ld", "cat.prim.int.fmt"
												);
												this->irBuilder.CreateCall( snprintfFunc, {
													primitiveBuf, llvm::ConstantInt::get( i64Type, 24 ), intFmt, primitiveIntVal
												} );
												otherStr = primitiveBuf;
											}
										}
										else {
											std::string concatArgQualifiedName = concatArgSemaType->qualified.empty() == false
												? concatArgSemaType->qualified : concatArgSemaType->name;
											size_t concatArgQualGenericPos = concatArgQualifiedName.find( '<' );
											if( concatArgQualGenericPos != std::string::npos ) {
												concatArgQualifiedName = concatArgQualifiedName.substr( 0, concatArgQualGenericPos );
											}
											std::string concatArgToStringName = concatArgQualifiedName + "." +
												semantic::qualname::classes::object::methods::ToString;
											std::unordered_map<std::string, llvm::Function*>::iterator concatToStringIt =
												this->functionResolutionMap.find( concatArgToStringName );
											if( concatToStringIt != this->functionResolutionMap.end() ) {
												otherStr = this->irBuilder.CreateCall(
													concatToStringIt->second, { otherValue }, "cat.cls.tostr"
												);
											}
										}
									}
									else if( concatArgSemaType != nullptr &&
										concatArgSemaType->kind == semantic::Type::Kind::Interface ) {
										llvm::Type* vtablePtrType = llvm::PointerType::getUnqual( this->llvmContext );
										std::vector<llvm::Type*> vtableWrapperFields = { vtablePtrType };
										llvm::Value* vtableSlotPtr = this->irBuilder.CreateStructGEP(
											llvm::StructType::get( this->llvmContext, vtableWrapperFields ),
											otherValue, 0, "cat.iface.vtable.slot"
										);
										llvm::Value* vtablePtr = this->irBuilder.CreateLoad(
											vtablePtrType, vtableSlotPtr, "cat.iface.vtable.ptr"
										);
										llvm::FunctionType* toStringVoidType = llvm::FunctionType::get(
											llvm::Type::getVoidTy( this->llvmContext ), false
										);
										llvm::PointerType* toStringFuncPtrType = llvm::PointerType::getUnqual( toStringVoidType );
										llvm::Value* toStringSlotPtr = this->irBuilder.CreateGEP(
											toStringFuncPtrType, vtablePtr,
											llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->llvmContext ), 0 ),
											"cat.iface.tostr.slot"
										);
										llvm::Value* toStringFuncPtr = this->irBuilder.CreateLoad(
											toStringFuncPtrType, toStringSlotPtr, "cat.iface.tostr.ptr"
										);
										llvm::FunctionType* toStringCallType = llvm::FunctionType::get(
											llvm::PointerType::getUnqual( this->llvmContext ),
											{ llvm::PointerType::getUnqual( this->llvmContext ) }, false
										);
										llvm::Value* toStringCastedFunc = this->irBuilder.CreateBitCast(
											toStringFuncPtr, llvm::PointerType::getUnqual( toStringCallType ), "cat.iface.tostr.cast"
										);
										otherStr = this->irBuilder.CreateCall(
											toStringCallType, toStringCastedFunc, { otherValue }, "cat.iface.tostr.result"
										);
									}
								}
								else if( otherValue->getType()->isDoubleTy() || otherValue->getType()->isFloatTy() ) {
									llvm::Value* dblVal = otherValue;
									if( otherValue->getType()->isFloatTy() ) {
										dblVal = this->irBuilder.CreateFPExt( otherValue, llvm::Type::getDoubleTy( this->llvmContext ), "cat.f2d" );
									}
									llvm::Value* fltBuf = this->irBuilder.CreateCall( mallocFunc, {
										llvm::ConstantInt::get( i64Type, 48 )
									}, "cat.flt.buf" );
									llvm::Value* fltFmt = this->irBuilder.CreateGlobalStringPtr( "%.6g", "cat.flt.fmt" );
									this->irBuilder.CreateCall( snprintfFunc, {
										fltBuf, llvm::ConstantInt::get( i64Type, 48 ), fltFmt, dblVal
									} );
									otherStr = fltBuf;
								}
								else if( otherValue->getType()->isIntegerTy() ) {
									if( otherValue->getType()->isIntegerTy( 1 ) ) {
										llvm::Value* trueStr = this->irBuilder.CreateGlobalStringPtr( "True", "cat.bool.t" );
										llvm::Value* falseStr = this->irBuilder.CreateGlobalStringPtr( "False", "cat.bool.f" );
										otherStr = this->irBuilder.CreateSelect( otherValue, trueStr, falseStr, "cat.bool" );
									}
									else {
										llvm::Value* intVal = otherValue;
										if( otherValue->getType()->getIntegerBitWidth() < 64 ) {
											intVal = this->irBuilder.CreateSExt( otherValue, i64Type, "cat.sext" );
										}
										llvm::Value* intBuf = this->irBuilder.CreateCall( mallocFunc, {
											llvm::ConstantInt::get( i64Type, 24 )
										}, "cat.int.buf" );
										llvm::Value* intFmt = this->irBuilder.CreateGlobalStringPtr( "%ld", "cat.int.fmt" );
										this->irBuilder.CreateCall( snprintfFunc, {
											intBuf, llvm::ConstantInt::get( i64Type, 24 ), intFmt, intVal
										} );
										otherStr = intBuf;
									}
								}
								llvm::Value* lenA = this->irBuilder.CreateCall( strlenFunc, { selfValue }, "cat.lenA" );
								llvm::Value* lenB = this->irBuilder.CreateCall( strlenFunc, { otherStr }, "cat.lenB" );
								llvm::Value* totalLen = this->irBuilder.CreateAdd( lenA, lenB, "cat.total" );
								llvm::Value* allocSize = this->irBuilder.CreateAdd(
									totalLen, llvm::ConstantInt::get( i64Type, 1 ), "cat.alloc" );
								llvm::Value* buf = this->irBuilder.CreateCall( mallocFunc, { allocSize }, "cat.buf" );
								this->irBuilder.CreateCall( strcpyFunc, { buf, selfValue } );
								this->irBuilder.CreateCall( strcatFunc, { buf, otherStr } );
								this->setVariableValue( instruction.destinationVariable, buf );
							}
							return;
						}
						if( methodName == semantic::qualname::classes::string::methods::StartsWith && instruction.sourceOperands.size() >= 2 ) {
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
						if( methodName == semantic::qualname::classes::string::methods::EndsWith && instruction.sourceOperands.size() >= 2 ) {
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
						if( methodName == semantic::qualname::classes::string::methods::Contains && instruction.sourceOperands.size() >= 2 ) {
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
						if( methodName == semantic::qualname::classes::string::methods::IndexOf && instruction.sourceOperands.size() >= 2 ) {
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
						if( methodName == semantic::qualname::classes::string::methods::CharAt && instruction.sourceOperands.size() >= 2 ) {
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
						if( methodName == semantic::qualname::classes::string::methods::ToUpper ) {
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
						if( methodName == semantic::qualname::classes::string::methods::ToLower ) {
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
						if( methodName == semantic::qualname::classes::string::methods::Trim ) {
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
						if( methodName == semantic::qualname::classes::string::methods::Replace && instruction.sourceOperands.size() >= 3 ) {
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
						if( methodName == semantic::qualname::classes::string::Name ) {
							return;
						}
					}
				}
				if( isIntegerWrapper || isFloatWrapper ) {
					llvm::Type* primitiveType = nullptr;
					if( isIntegerWrapper ) {
						primitiveType = llvm::Type::getInt64Ty( this->llvmContext );
					}
					else if( wrapperName == semantic::qualname::classes::f32::Name ) {
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
					if( methodName == semantic::qualname::classes::object::methods::GetValue || methodName == semantic::qualname::classes::object::methods::Value ) {
						setResult( loadSelf() );
						return;
					}
					if( methodName == semantic::qualname::interfaces::addable::methods::Add ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							setResult( isFloatWrapper
								? this->irBuilder.CreateFAdd( selfValue, argValue, "wrap.add" )
								: this->irBuilder.CreateAdd( selfValue, argValue, "wrap.add" ) );
						}
						return;
					}
					if( methodName == semantic::qualname::interfaces::subtractable::methods::Subtract || methodName == semantic::qualname::interfaces::subtractable::methods::Sub ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							setResult( isFloatWrapper
								? this->irBuilder.CreateFSub( selfValue, argValue, "wrap.sub" )
								: this->irBuilder.CreateSub( selfValue, argValue, "wrap.sub" ) );
						}
						return;
					}
					if( methodName == semantic::qualname::interfaces::multipliable::methods::Multiply || methodName == semantic::qualname::interfaces::multipliable::methods::Mul ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							setResult( isFloatWrapper
								? this->irBuilder.CreateFMul( selfValue, argValue, "wrap.mul" )
								: this->irBuilder.CreateMul( selfValue, argValue, "wrap.mul" ) );
						}
						return;
					}
					if( methodName == semantic::qualname::interfaces::dividable::methods::Divide || methodName == semantic::qualname::interfaces::dividable::methods::Div ) {
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
					if( methodName == semantic::qualname::interfaces::modulable::methods::Modulo || methodName == semantic::qualname::interfaces::modulable::methods::Mod || methodName == semantic::qualname::interfaces::modulable::methods::Remainder ) {
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
					if( methodName == semantic::qualname::interfaces::negatable::methods::Negate || methodName == semantic::qualname::interfaces::negatable::methods::Neg ) {
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
					if( methodName == semantic::qualname::classes::object::methods::Abs ) {
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
					if( methodName == semantic::qualname::interfaces::equatable::methods::Equals ) {
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
					if( methodName == semantic::qualname::classes::object::methods::CompareTo ) {
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
					if( methodName == semantic::qualname::interfaces::comparable::methods::GreaterThan || methodName == semantic::qualname::interfaces::comparable::methods::Gt ) {
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
					if( methodName == semantic::qualname::interfaces::comparable::methods::LessThan || methodName == semantic::qualname::interfaces::comparable::methods::Lt ) {
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
					if( methodName == semantic::qualname::classes::object::methods::GreaterThanOrEqual || methodName == semantic::qualname::classes::object::methods::Gte ) {
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
					if( methodName == semantic::qualname::classes::object::methods::LessThanOrEqual || methodName == semantic::qualname::classes::object::methods::Lte ) {
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
					if( methodName == semantic::qualname::classes::object::methods::Min ) {
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
					if( methodName == semantic::qualname::classes::object::methods::Max ) {
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
					if( methodName == semantic::qualname::classes::object::methods::BitwiseAnd && isIntegerWrapper ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							setResult( this->irBuilder.CreateAnd( selfValue, argValue, "wrap.and" ) );
						}
						return;
					}
					if( methodName == semantic::qualname::classes::object::methods::BitwiseOr && isIntegerWrapper ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							setResult( this->irBuilder.CreateOr( selfValue, argValue, "wrap.or" ) );
						}
						return;
					}
					if( methodName == semantic::qualname::classes::object::methods::BitwiseXor && isIntegerWrapper ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							setResult( this->irBuilder.CreateXor( selfValue, argValue, "wrap.xor" ) );
						}
						return;
					}
					if( methodName == semantic::qualname::classes::object::methods::BitwiseNot && isIntegerWrapper ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							setResult( this->irBuilder.CreateNot( selfValue, "wrap.not" ) );
						}
						return;
					}
					if( methodName == semantic::qualname::classes::object::methods::ShiftLeft && isIntegerWrapper ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							setResult( this->irBuilder.CreateShl( selfValue, argValue, "wrap.shl" ) );
						}
						return;
					}
					if( methodName == semantic::qualname::classes::object::methods::ShiftRight && isIntegerWrapper ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							setResult( isUnsigned
								? this->irBuilder.CreateLShr( selfValue, argValue, "wrap.shr" )
								: this->irBuilder.CreateAShr( selfValue, argValue, "wrap.shr" ) );
						}
						return;
					}
					if( methodName == semantic::qualname::classes::object::methods::ToString ) {
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
					if( methodName == semantic::qualname::classes::object::methods::ToI64 || methodName == semantic::qualname::classes::object::methods::ToInt || methodName == semantic::qualname::classes::object::methods::ToLong ) {
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
					if( methodName == semantic::qualname::classes::object::methods::ToI32 ) {
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
					if( methodName == semantic::qualname::classes::object::methods::ToFloat || methodName == semantic::qualname::classes::object::methods::ToDouble || methodName == semantic::qualname::classes::object::methods::ToF64 ) {
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
					if( methodName == semantic::qualname::classes::object::methods::IsInfinite && isFloatWrapper ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							llvm::Value* absVal = this->irBuilder.CreateUnaryIntrinsic( llvm::Intrinsic::fabs, selfValue, nullptr, "wrap.fabs" );
							llvm::Value* inf = llvm::ConstantFP::getInfinity( primitiveType );
							setResult( this->irBuilder.CreateFCmpOEQ( absVal, inf, "wrap.isinf" ) );
						}
						return;
					}
					if( methodName == semantic::qualname::classes::object::methods::IsNan && isFloatWrapper ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							setResult( this->irBuilder.CreateFCmpUNO( selfValue, selfValue, "wrap.isnan" ) );
						}
						return;
					}
					if( methodName == semantic::qualname::classes::object::methods::IsFinite && isFloatWrapper ) {
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
					if( methodName == semantic::qualname::classes::object::methods::Floor && isFloatWrapper ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							setResult( this->irBuilder.CreateUnaryIntrinsic( llvm::Intrinsic::floor, selfValue, nullptr, "wrap.floor" ) );
						}
						return;
					}
					if( methodName == semantic::qualname::classes::object::methods::Ceil && isFloatWrapper ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							setResult( this->irBuilder.CreateUnaryIntrinsic( llvm::Intrinsic::ceil, selfValue, nullptr, "wrap.ceil" ) );
						}
						return;
					}
					if( methodName == semantic::qualname::classes::object::methods::Round && isFloatWrapper ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							setResult( this->irBuilder.CreateUnaryIntrinsic( llvm::Intrinsic::round, selfValue, nullptr, "wrap.round" ) );
						}
						return;
					}
					if( methodName == semantic::qualname::classes::object::methods::Sqrt && isFloatWrapper ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							setResult( this->irBuilder.CreateUnaryIntrinsic( llvm::Intrinsic::sqrt, selfValue, nullptr, "wrap.sqrt" ) );
						}
						return;
					}
					if( methodName == semantic::qualname::classes::object::methods::Power && isFloatWrapper ) {
						llvm::Value* selfValue = loadSelf();
						llvm::Value* argValue = loadArg();
						if( selfValue != nullptr && argValue != nullptr ) {
							setResult( this->irBuilder.CreateBinaryIntrinsic( llvm::Intrinsic::pow, selfValue, argValue, nullptr, "wrap.pow" ) );
						}
						return;
					}
					if( methodName == semantic::qualname::classes::object::methods::IsZero ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							llvm::Value* result = isFloatWrapper
								? this->irBuilder.CreateFCmpOEQ( selfValue, llvm::ConstantFP::get( primitiveType, 0.0 ), "wrap.iszero" )
								: this->irBuilder.CreateICmpEQ( selfValue, llvm::ConstantInt::get( primitiveType, 0 ), "wrap.iszero" );
							setResult( result );
						}
						return;
					}
					if( methodName == semantic::qualname::classes::object::methods::IsPositive ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							llvm::Value* result = isFloatWrapper
								? this->irBuilder.CreateFCmpOGT( selfValue, llvm::ConstantFP::get( primitiveType, 0.0 ), "wrap.ispos" )
								: this->irBuilder.CreateICmpSGT( selfValue, llvm::ConstantInt::get( primitiveType, 0, true ), "wrap.ispos" );
							setResult( result );
						}
						return;
					}
					if( methodName == semantic::qualname::classes::object::methods::IsNegative ) {
						llvm::Value* selfValue = loadSelf();
						if( selfValue != nullptr ) {
							llvm::Value* result = isFloatWrapper
								? this->irBuilder.CreateFCmpOLT( selfValue, llvm::ConstantFP::get( primitiveType, 0.0 ), "wrap.isneg" )
								: this->irBuilder.CreateICmpSLT( selfValue, llvm::ConstantInt::get( primitiveType, 0, true ), "wrap.isneg" );
							setResult( result );
						}
						return;
					}
					if( methodName == semantic::qualname::classes::boolean::Name || methodName == semantic::qualname::classes::Int::Name || methodName == semantic::qualname::classes::i64::Name ||
						methodName == semantic::qualname::classes::i32::Name || methodName == semantic::qualname::classes::i16::Name || methodName == semantic::qualname::classes::i8::Name ||
						methodName == semantic::qualname::classes::uint::Name || methodName == semantic::qualname::classes::u64::Name || methodName == semantic::qualname::classes::u32::Name ||
						methodName == semantic::qualname::classes::u16::Name || methodName == semantic::qualname::classes::u8::Name || methodName == semantic::qualname::classes::byte::Name ||
						methodName == semantic::qualname::classes::Long::Name || methodName == semantic::qualname::classes::integer::Name || methodName == semantic::qualname::classes::Char::Name ||
						methodName == semantic::qualname::classes::Float::Name || methodName == semantic::qualname::classes::Double::Name || methodName == semantic::qualname::classes::f32::Name || methodName == semantic::qualname::classes::f64::Name ) {
						return;
					}
				}
			}
		}
		if( isBareOrObjectCall && bareMethodName == semantic::qualname::classes::string::methods::Concat && instruction.sourceOperands.size() == 2 ) {
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
				std::function<llvm::Value*( llvm::Value*, MIRVariableIdentifier )> ensureString = [&]( llvm::Value* value, MIRVariableIdentifier varId ) -> llvm::Value* {
					if( value->getType()->isPointerTy() ) {
						bool isGenericParam = false;
						if( this->currentMIRFunction != nullptr ) {
							std::unordered_map<MIRVariableIdentifier, MIRVariableDescriptor>::iterator descIt =
								this->currentMIRFunction->variableDescriptorTable.find( varId );
							if( descIt != this->currentMIRFunction->variableDescriptorTable.end() &&
								descIt->second.variableType != nullptr &&
								descIt->second.variableType->kind == semantic::Type::Kind::GenericParameter ) {
								isGenericParam = true;
							}
						}
						if( isGenericParam ) {
							llvm::Value* ptrAsInt = this->irBuilder.CreatePtrToInt( value, i64Type, "es.ptoi" );
							llvm::Value* threshold = llvm::ConstantInt::get( i64Type, 4096 );
							llvm::Value* isRealPtr = this->irBuilder.CreateICmpUGE( ptrAsInt, threshold, "es.isptr" );
							llvm::Function* currentFunc = this->irBuilder.GetInsertBlock()->getParent();
							llvm::BasicBlock* strBlock = llvm::BasicBlock::Create( this->llvmContext, "es.str", currentFunc );
							llvm::BasicBlock* intBlock = llvm::BasicBlock::Create( this->llvmContext, "es.int", currentFunc );
							llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create( this->llvmContext, "es.merge", currentFunc );
							this->irBuilder.CreateCondBr( isRealPtr, strBlock, intBlock );
							this->irBuilder.SetInsertPoint( strBlock );
							this->irBuilder.CreateBr( mergeBlock );
							llvm::BasicBlock* strExit = this->irBuilder.GetInsertBlock();
							this->irBuilder.SetInsertPoint( intBlock );
							llvm::Value* intBuf = this->irBuilder.CreateCall( mallocFunction, {
								llvm::ConstantInt::get( i64Type, 24 )
							}, "es.int.buf" );
							llvm::Value* intFmt = this->irBuilder.CreateGlobalStringPtr( "%ld", "es.int.fmt" );
							this->irBuilder.CreateCall( snprintfFunction, {
								intBuf, llvm::ConstantInt::get( i64Type, 24 ), intFmt, ptrAsInt
							} );
							this->irBuilder.CreateBr( mergeBlock );
							llvm::BasicBlock* intExit = this->irBuilder.GetInsertBlock();
							this->irBuilder.SetInsertPoint( mergeBlock );
							llvm::PHINode* phi = this->irBuilder.CreatePHI( ptrType, 2, "es.val" );
							phi->addIncoming( value, strExit );
							phi->addIncoming( intBuf, intExit );
							return phi;
						}
						return value;
					}
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
				llvm::Value* leftStr = ensureString( leftValue, instruction.sourceOperands[0] );
				llvm::Value* rightStr = ensureString( rightValue, instruction.sourceOperands[1] );
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
		if( isBareOrObjectCall && bareMethodName == semantic::qualname::interfaces::equatable::methods::Equals && instruction.sourceOperands.size() == 2 ) {
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
		if( callee != nullptr && callee->isVarArg() == false &&
			callee->arg_size() != instruction.sourceOperands.size() ) {
			std::string arityName = fmt::format( "{}#{}", calledName, instruction.sourceOperands.size() );
			if( this->functionResolutionMap.count( arityName ) > 0 ) {
				callee = this->functionResolutionMap[arityName];
			}
			else {
				llvm::Function* arityFunction = this->llvmModule->getFunction( arityName );
				if( arityFunction != nullptr ) {
					callee = arityFunction;
				}
			}
		}
		if( callee == nullptr && calledName.find( '.' ) != std::string::npos ) {
			size_t dotPosition = calledName.rfind( '.' );
			std::string className = calledName.substr( 0, dotPosition );
			std::string methodName = calledName.substr( dotPosition + 1 );
			semantic::TypeSharedPointer classType = this->semanticAnalyzer.types().lookupType( className );
			while( callee == nullptr && classType != nullptr && classType->kind == semantic::Type::Kind::Class ) {
				semantic::ClassType* classTypePtr = static_cast<semantic::ClassType*>( classType.get() );
				if( classTypePtr->baseClass != nullptr ) {
					std::string parentQualified = classTypePtr->baseClass->qualified.empty() == false
						? classTypePtr->baseClass->qualified
						: classTypePtr->baseClass->name;
					std::string parentMethodName = parentQualified + "." + methodName;
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
			size_t lastDotPos = calledName.rfind( '.' );
			std::string interfaceQualified = calledName.substr( 0, lastDotPos );
			std::string methodName = calledName.substr( lastDotPos + 1 );
			std::string interfaceShortName = interfaceQualified;
			{
				size_t shortDot = interfaceQualified.rfind( '.' );
				if( shortDot != std::string::npos ) {
					interfaceShortName = interfaceQualified.substr( shortDot + 1 );
				}
			}
			if( instruction.sourceOperands.empty() == false ) {
				MIRVariableIdentifier receiverVariable = instruction.sourceOperands[0];
				if( this->concreteClassMap.count( receiverVariable ) > 0 ) {
					std::string concreteClassName = this->concreteClassMap[receiverVariable];
					std::string concreteMethodFullName = concreteClassName + "." + methodName;
					if( this->functionResolutionMap.count( concreteMethodFullName ) > 0 ) {
						callee = this->functionResolutionMap[concreteMethodFullName];
					}
					if( callee == nullptr ) {
						std::string concreteBaseClassName = concreteClassName;
						size_t concreteBracket = concreteBaseClassName.find( '<' );
						if( concreteBracket != std::string::npos ) {
							concreteBaseClassName = concreteBaseClassName.substr( 0, concreteBracket );
						}
						semantic::TypeSharedPointer concreteType = this->semanticAnalyzer.types().lookupType( concreteBaseClassName );
						if( concreteType != nullptr && concreteType->kind == semantic::Type::Kind::Class ) {
							std::string qualifiedClassName = concreteType->qualified;
							size_t qualBracket = qualifiedClassName.find( '<' );
							if( qualBracket != std::string::npos ) {
								qualifiedClassName = qualifiedClassName.substr( 0, qualBracket );
							}
							if( qualifiedClassName.empty() == false ) {
								std::string qualifiedMethodName2 = qualifiedClassName + "." + methodName;
								if( this->functionResolutionMap.count( qualifiedMethodName2 ) > 0 ) {
									callee = this->functionResolutionMap[qualifiedMethodName2];
								}
							}
						}
					}
					if( callee == nullptr ) {
						size_t concreteLastDot = concreteClassName.rfind( '.' );
						if( concreteLastDot != std::string::npos ) {
							std::string concreteShortName = concreteClassName.substr( concreteLastDot + 1 );
							std::string shortMethodName = concreteShortName + "." + methodName;
							if( this->functionResolutionMap.count( shortMethodName ) > 0 ) {
								callee = this->functionResolutionMap[shortMethodName];
							}
						}
					}
				}
			}
			if( callee == nullptr && instruction.sourceOperands.empty() == false ) {
				std::vector<std::string> interfaceLookupKeys = { interfaceQualified, interfaceShortName };
				for( const std::string& ifaceLookupKey : interfaceLookupKeys ) {
					if( this->interfaceMethodOrder.count( ifaceLookupKey ) == 0 ) {
						continue;
					}
					if( this->interfacesWithDirectItable.count( ifaceLookupKey ) == 0 ) {
						continue;
					}
					const std::vector<std::string>& methodOrder = this->interfaceMethodOrder[ifaceLookupKey];
					int methodSlotIndex = -1;
					for( size_t slotIdx = 0; slotIdx < methodOrder.size(); slotIdx++ ) {
						if( methodOrder[slotIdx] == methodName ) {
							methodSlotIndex = static_cast<int>( slotIdx );
							break;
						}
					}
					if( methodSlotIndex < 0 ) {
						continue;
					}
					MIRVariableIdentifier receiverVariable = instruction.sourceOperands[0];
					llvm::Value* receiverPtr = this->loadVariableValue( receiverVariable );
					if( receiverPtr == nullptr || receiverPtr->getType()->isPointerTy() == false ) {
						break;
					}
					llvm::Type* vtablePtrType = llvm::PointerType::getUnqual( this->llvmContext );
					std::vector<llvm::Type*> vtableWrapperFields = { vtablePtrType };
					llvm::Value* vtableSlotPtr = this->irBuilder.CreateStructGEP(
						llvm::StructType::get( this->llvmContext, vtableWrapperFields ), receiverPtr, 0, "vtable.slot.ptr"
					);
					llvm::Value* vtablePtr = this->irBuilder.CreateLoad( vtablePtrType, vtableSlotPtr, "vtable.ptr" );
					llvm::FunctionType* voidFuncType = llvm::FunctionType::get( llvm::Type::getVoidTy( this->llvmContext ), false );
					llvm::PointerType* funcPtrType = llvm::PointerType::getUnqual( voidFuncType );
					llvm::Value* funcSlotPtr = this->irBuilder.CreateGEP(
						funcPtrType, vtablePtr,
						llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->llvmContext ), methodSlotIndex ),
						"vfunc.slot.ptr"
					);
					llvm::Value* funcPtr = this->irBuilder.CreateLoad( funcPtrType, funcSlotPtr, "vfunc.ptr" );
					std::vector<llvm::Value*> dispatchArgs;
					dispatchArgs.push_back( receiverPtr );
					for( size_t argIdx = 1; argIdx < instruction.sourceOperands.size(); argIdx++ ) {
						llvm::Value* argVal = this->loadVariableValue( instruction.sourceOperands[argIdx] );
						if( argVal != nullptr ) {
							dispatchArgs.push_back( argVal );
						}
					}
					std::vector<llvm::Type*> argTypes;
					for( llvm::Value* arg : dispatchArgs ) {
						argTypes.push_back( arg->getType() );
					}
					llvm::Type* returnType = llvm::Type::getVoidTy( this->llvmContext );
					if( instruction.operandType != nullptr ) {
						returnType = this->toLLVMType( instruction.operandType );
					}
					if( methodName == semantic::qualname::classes::object::methods::ToString ) {
						returnType = llvm::PointerType::getUnqual( this->llvmContext );
					}
					llvm::FunctionType* callType = llvm::FunctionType::get( returnType, argTypes, false );
					llvm::Value* castedFunc = this->irBuilder.CreateBitCast(
						funcPtr, llvm::PointerType::getUnqual( callType ), "vfunc.cast"
					);
					llvm::Value* callResult = nullptr;
					if( returnType->isVoidTy() ) {
						this->irBuilder.CreateCall( callType, castedFunc, dispatchArgs );
					}
					else {
						callResult = this->irBuilder.CreateCall( callType, castedFunc, dispatchArgs, "vcall.result" );
					}
					if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER && callResult != nullptr ) {
						this->setVariableValue( instruction.destinationVariable, callResult );
					}
					return;
				}
			}
			if( callee == nullptr )
			for( const std::pair<const std::string, llvm::Function*>& entry : this->functionResolutionMap ) {
				size_t entryLastDot = entry.first.rfind( '.' );
				if( entryLastDot != std::string::npos &&
					entry.first.substr( entryLastDot + 1 ) == methodName &&
					entry.first != calledName ) {
					std::string candidateQualified = entry.first.substr( 0, entryLastDot );
					semantic::TypeSharedPointer candidateType = this->semanticAnalyzer.types().lookupType( candidateQualified );
					if( candidateType == nullptr ) {
						size_t candidateShortDot = candidateQualified.rfind( '.' );
						if( candidateShortDot != std::string::npos ) {
							candidateType = this->semanticAnalyzer.types().lookupType(
								candidateQualified.substr( candidateShortDot + 1 )
							);
						}
					}
					if( candidateType != nullptr && candidateType->kind == semantic::Type::Kind::Class ) {
						semantic::ClassType* classPtr = static_cast<semantic::ClassType*>( candidateType.get() );
						for( const semantic::TypeSharedPointer& implementedInterface : classPtr->interfaces ) {
							if( implementedInterface == nullptr ) continue;
							std::string ifaceName = implementedInterface->name;
							size_t bracketPos = ifaceName.find( '<' );
							if( bracketPos != std::string::npos ) {
								ifaceName = ifaceName.substr( 0, bracketPos );
							}
							if( ifaceName == interfaceShortName || ifaceName == interfaceQualified ) {
								callee = entry.second;
								break;
							}
						}
						if( callee == nullptr ) {
							semantic::TypeSharedPointer walkType = candidateType;
							while( walkType != nullptr && walkType->kind == semantic::Type::Kind::Class ) {
								semantic::ClassType* walkClass = static_cast<semantic::ClassType*>( walkType.get() );
								for( const semantic::TypeSharedPointer& parentIface : walkClass->interfaces ) {
									if( parentIface == nullptr ) continue;
									std::string parentIfaceName = parentIface->name;
									size_t parentBracketPos = parentIfaceName.find( '<' );
									if( parentBracketPos != std::string::npos ) {
										parentIfaceName = parentIfaceName.substr( 0, parentBracketPos );
									}
									if( parentIfaceName == interfaceShortName || parentIfaceName == interfaceQualified ) {
										callee = entry.second;
										break;
									}
									semantic::TypeSharedPointer ifaceType = this->semanticAnalyzer.types().lookupType( parentIfaceName );
									if( ifaceType != nullptr && ifaceType->kind == semantic::Type::Kind::Interface ) {
										semantic::ClassType* ifaceClass = static_cast<semantic::ClassType*>( ifaceType.get() );
										for( const semantic::TypeSharedPointer& grandparentIface : ifaceClass->interfaces ) {
											if( grandparentIface == nullptr ) continue;
											std::string gpIfaceName = grandparentIface->name;
											size_t gpBracketPos = gpIfaceName.find( '<' );
											if( gpBracketPos != std::string::npos ) {
												gpIfaceName = gpIfaceName.substr( 0, gpBracketPos );
											}
											if( gpIfaceName == interfaceShortName || gpIfaceName == interfaceQualified ) {
												callee = entry.second;
												break;
											}
										}
										if( callee != nullptr ) break;
									}
								}
								if( callee != nullptr ) break;
								if( walkClass->baseClass != nullptr ) {
									walkType = walkClass->baseClass;
								}
								else {
									break;
								}
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
		if( callee == nullptr && calledName.find( '.' ) != std::string::npos ) {
			size_t lastDotPosition = calledName.rfind( '.' );
			std::string shortName = calledName.substr( lastDotPosition + 1 );
			if( this->externDeclaredNames.count( shortName ) > 0 &&
				this->functionResolutionMap.count( shortName ) > 0 ) {
				callee = this->functionResolutionMap[shortName];
			}
		}
		if( callee == nullptr ) {
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
		if( callee != nullptr && calledName.find( '.' ) != std::string::npos &&
			instruction.sourceOperands.empty() == false ) {
			size_t abstractLastDot = calledName.rfind( '.' );
			std::string abstractOwnerName = calledName.substr( 0, abstractLastDot );
			std::string abstractMethodName = calledName.substr( abstractLastDot + 1 );
			std::string abstractShortName = abstractOwnerName;
			{
				size_t shortDot = abstractOwnerName.rfind( '.' );
				if( shortDot != std::string::npos ) {
					abstractShortName = abstractOwnerName.substr( shortDot + 1 );
				}
			}
			std::vector<std::string>* concreteSubclasses = nullptr;
			if( this->abstractClassSubclasses.count( abstractShortName ) > 0 ) {
				concreteSubclasses = &this->abstractClassSubclasses[abstractShortName];
			}
			else if( this->abstractClassSubclasses.count( abstractOwnerName ) > 0 ) {
				concreteSubclasses = &this->abstractClassSubclasses[abstractOwnerName];
			}
			if( concreteSubclasses != nullptr && concreteSubclasses->empty() == false ) {
				semantic::TypeSharedPointer abstractOwnerType = this->semanticAnalyzer.types().lookupType( abstractShortName );
				if( abstractOwnerType == nullptr ) {
					abstractOwnerType = this->semanticAnalyzer.types().lookupType( abstractOwnerName );
				}
				if( abstractOwnerType != nullptr && abstractOwnerType->kind == semantic::Type::Kind::Class ) {
					semantic::ClassType* abstractClassType = static_cast<semantic::ClassType*>( abstractOwnerType.get() );
					semantic::MethodInfo* abstractMethodInfo = abstractClassType->findMethod( abstractMethodName );
					if( abstractClassType->isAbstract ||
						( abstractClassType->astDeclaration != nullptr && abstractClassType->astDeclaration->isAbstract ) ) {
						if( abstractMethodInfo != nullptr && abstractMethodInfo->isVirtual ) {
							struct AbstractDispatchTarget {
								llvm::Function* concreteMethod;
								llvm::Constant* vtableIdentifier;
							};
							std::vector<AbstractDispatchTarget> dispatchTargets;
							for( const std::string& subclassName : *concreteSubclasses ) {
								llvm::Function* concreteMethod = nullptr;
								semantic::TypeSharedPointer subType = this->semanticAnalyzer.types().lookupType( subclassName );
								if( subType != nullptr && subType->qualified.empty() == false ) {
									std::string qualifiedMethodName = subType->qualified + "." + abstractMethodName;
									if( this->functionResolutionMap.count( qualifiedMethodName ) > 0 ) {
										concreteMethod = this->functionResolutionMap[qualifiedMethodName];
									}
								}
								if( concreteMethod == nullptr ) {
									std::string shortMethodName = subclassName + "." + abstractMethodName;
									if( this->functionResolutionMap.count( shortMethodName ) > 0 ) {
										concreteMethod = this->functionResolutionMap[shortMethodName];
									}
								}
								if( concreteMethod == nullptr ) {
									continue;
								}
								llvm::Constant* vtableId = nullptr;
								if( this->classVtableIdentifier.count( subclassName ) > 0 ) {
									vtableId = this->classVtableIdentifier[subclassName];
								}
								if( vtableId == nullptr ) {
									continue;
								}
								dispatchTargets.push_back( { concreteMethod, vtableId } );
							}
							if( dispatchTargets.empty() == false ) {
								MIRVariableIdentifier receiverVar = instruction.sourceOperands[0];
								llvm::Value* receiverPtr = this->loadVariableValue( receiverVar );
								if( receiverPtr != nullptr && receiverPtr->getType()->isPointerTy() ) {
									llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
									llvm::StructType* receiverStructType = nullptr;
									if( this->structTypeCache.count( abstractShortName ) > 0 ) {
										receiverStructType = this->structTypeCache[abstractShortName];
									}
									else if( this->structTypeCache.count( abstractOwnerName ) > 0 ) {
										receiverStructType = this->structTypeCache[abstractOwnerName];
									}
									if( receiverStructType == nullptr || receiverStructType->getNumElements() == 0 ) {
										receiverStructType = llvm::StructType::get( this->llvmContext, { ptrType } );
									}
									llvm::Value* vtableSlotPtr = this->irBuilder.CreateStructGEP(
										receiverStructType,
										receiverPtr, 0, "abstract.vtable.slot"
									);
									llvm::Value* vtablePtr = this->irBuilder.CreateLoad( ptrType, vtableSlotPtr, "abstract.vtable.ptr" );
									llvm::Type* dispatchReturnType = llvm::Type::getVoidTy( this->llvmContext );
									if( instruction.operandType != nullptr ) {
										dispatchReturnType = this->toLLVMType( instruction.operandType );
									}
									bool hasReturnValue = ( dispatchReturnType->isVoidTy() == false );
									std::vector<llvm::Value*> dispatchArgs;
									for( size_t argIdx = 0; argIdx < instruction.sourceOperands.size(); argIdx++ ) {
										llvm::Value* argVal = this->loadVariableValue( instruction.sourceOperands[argIdx] );
										if( argVal != nullptr ) {
											dispatchArgs.push_back( argVal );
										}
									}
									llvm::Function* parentFunction = this->irBuilder.GetInsertBlock()->getParent();
									llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create(
										this->llvmContext, "abstract.merge", parentFunction
									);
									llvm::BasicBlock* fallbackBlock = llvm::BasicBlock::Create(
										this->llvmContext, "abstract.fallback", parentFunction
									);
									std::vector<std::pair<llvm::BasicBlock*, llvm::Value*>> phiIncoming;
									llvm::BasicBlock* currentBlock = this->irBuilder.GetInsertBlock();
									for( size_t targetIdx = 0; targetIdx < dispatchTargets.size(); targetIdx++ ) {
										AbstractDispatchTarget& target = dispatchTargets[targetIdx];
										llvm::BasicBlock* callBlock = llvm::BasicBlock::Create(
											this->llvmContext, "abstract.call", parentFunction
										);
										llvm::BasicBlock* nextBlock = ( targetIdx + 1 < dispatchTargets.size() )
											? llvm::BasicBlock::Create( this->llvmContext, "abstract.check", parentFunction )
											: fallbackBlock;
										this->irBuilder.SetInsertPoint( currentBlock );
										llvm::Value* castedId = this->irBuilder.CreateBitCast(
											target.vtableIdentifier, ptrType, "vtable.id"
										);
										llvm::Value* isMatch = this->irBuilder.CreateICmpEQ( vtablePtr, castedId, "type.match" );
										this->irBuilder.CreateCondBr( isMatch, callBlock, nextBlock );
										this->irBuilder.SetInsertPoint( callBlock );
										std::vector<llvm::Value*> callArgs;
										llvm::FunctionType* concreteType = target.concreteMethod->getFunctionType();
										for( size_t argI = 0; argI < dispatchArgs.size() && argI < concreteType->getNumParams(); argI++ ) {
											llvm::Value* arg = dispatchArgs[argI];
											llvm::Type* expectedType = concreteType->getParamType( static_cast<unsigned>( argI ) );
											if( arg->getType() != expectedType ) {
												if( arg->getType()->isIntegerTy() && expectedType->isIntegerTy() ) {
													arg = this->irBuilder.CreateSExtOrTrunc( arg, expectedType );
												}
											}
											callArgs.push_back( arg );
										}
										llvm::Value* callResult = this->irBuilder.CreateCall( target.concreteMethod, callArgs );
										phiIncoming.push_back( { callBlock, hasReturnValue ? callResult : nullptr } );
										this->irBuilder.CreateBr( mergeBlock );
										currentBlock = nextBlock;
									}
									this->irBuilder.SetInsertPoint( fallbackBlock );
									llvm::Value* fallbackResult = nullptr;
									if( hasReturnValue ) {
										std::vector<llvm::Value*> fallbackArgs;
										llvm::FunctionType* fallbackType = callee->getFunctionType();
										for( size_t argI = 0; argI < dispatchArgs.size() && argI < fallbackType->getNumParams(); argI++ ) {
											llvm::Value* arg = dispatchArgs[argI];
											llvm::Type* expectedType = fallbackType->getParamType( static_cast<unsigned>( argI ) );
											if( arg->getType() != expectedType ) {
												if( arg->getType()->isIntegerTy() && expectedType->isIntegerTy() ) {
													arg = this->irBuilder.CreateSExtOrTrunc( arg, expectedType );
												}
											}
											fallbackArgs.push_back( arg );
										}
										fallbackResult = this->irBuilder.CreateCall( callee, fallbackArgs );
									}
									else {
										std::vector<llvm::Value*> fallbackArgs;
										llvm::FunctionType* fallbackType = callee->getFunctionType();
										for( size_t argI = 0; argI < dispatchArgs.size() && argI < fallbackType->getNumParams(); argI++ ) {
											fallbackArgs.push_back( dispatchArgs[argI] );
										}
										this->irBuilder.CreateCall( callee, fallbackArgs );
									}
									phiIncoming.push_back( { fallbackBlock, fallbackResult } );
									this->irBuilder.CreateBr( mergeBlock );
									this->irBuilder.SetInsertPoint( mergeBlock );
									if( hasReturnValue && instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
										llvm::PHINode* phi = this->irBuilder.CreatePHI(
											dispatchReturnType, static_cast<unsigned>( phiIncoming.size() ), "abstract.result"
										);
										for( std::pair<llvm::BasicBlock*, llvm::Value*>& entry : phiIncoming ) {
											phi->addIncoming( entry.second, entry.first );
										}
										this->setVariableValue( instruction.destinationVariable, phi );
									}
									return;
								}
							}
						}
					}
				}
			}
		}
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
		if( calleeVariadicIndex >= 0 &&
			static_cast<unsigned>( calleeVariadicIndex ) < expectedParamCount &&
			instruction.sourceOperands.size() >= static_cast<size_t>( calleeVariadicIndex ) ) {
			std::vector<llvm::Value*> arguments;
			llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
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
			llvm::Type* elemType = i64Type;
			if( calleeVariadicElemType != nullptr ) {
				llvm::Type* resolved = this->toLLVMType( calleeVariadicElemType );
				if( resolved->isVoidTy() == false ) {
					elemType = resolved;
				}
			}
			MIRFunctionDefinition* calleeDefLocal = nullptr;
			if( this->mirFunctionDefinitionMap.count( instruction.calledFunctionQualifiedName ) > 0 ) {
				calleeDefLocal = this->mirFunctionDefinitionMap[instruction.calledFunctionQualifiedName];
			}
			unsigned keywordOnlyCount = ( expectedParamCount > static_cast<unsigned>( calleeVariadicIndex ) + 1 )
				? expectedParamCount - calleeVariadicIndex - 1
				: 0;
			bool calleeHasKwargsParam = ( calleeDefLocal != nullptr && calleeDefLocal->keywordParameterIndex >= 0 );
			if( calleeHasKwargsParam && keywordOnlyCount > 0 ) {
				keywordOnlyCount--;
			}
			size_t variadicArgStart = static_cast<size_t>( calleeVariadicIndex );
			size_t totalRemainingArgs = ( instruction.sourceOperands.size() > variadicArgStart )
				? instruction.sourceOperands.size() - variadicArgStart
				: 0;
			bool isVariadicForward = false;
			MIRVariableIdentifier forwardVar = 0;
			std::string variadicParamName = "";
			if( calleeDefLocal != nullptr && calleeVariadicIndex < calleeDefLocal->parameterVariableIdentifiers.size() ) {
				MIRVariableIdentifier vId = calleeDefLocal->parameterVariableIdentifiers[calleeVariadicIndex];
				if( calleeDefLocal->variableDescriptorTable.count( vId ) > 0 ) {
					variadicParamName = calleeDefLocal->variableDescriptorTable[vId].variableName;
				}
			}
			if( variadicParamName.empty() == false ) {
				for( size_t kki = 0; kki < instruction.keywordArgumentKeys.size(); kki++ ) {
					if( instruction.keywordArgumentKeys[kki] == variadicParamName ) {
						forwardVar = instruction.keywordArgumentValues[kki];
						isVariadicForward = true;
						break;
					}
				}
			}
			
			if( isVariadicForward == false && totalRemainingArgs >= 1 ) {
				forwardVar = instruction.sourceOperands[variadicArgStart];
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
						forwardDesc.variableType->name.find( semantic::qualname::classes::args::Prefix ) == 0 ) {
						isVariadicForward = true;
					}
				}
			}
			size_t variadicArgCount = isVariadicForward
				? 1
				: totalRemainingArgs;
			if( isVariadicForward ) {
				llvm::Value* forwardedArgs = this->loadVariableValue( forwardVar );
				if( forwardedArgs != nullptr && forwardedArgs->getType()->isPointerTy() == false ) {
					forwardedArgs = this->irBuilder.CreateIntToPtr(
						forwardedArgs, llvm::PointerType::getUnqual( this->llvmContext ), "vfwd.ptr"
					);
				}
				arguments.push_back( forwardedArgs );
			}
			else {
				llvm::Type* ptrTypeVA = llvm::PointerType::getUnqual( this->llvmContext );
				llvm::StructType* argsStructType = llvm::StructType::get( this->llvmContext, {
					ptrTypeVA,
					ptrTypeVA,
					i64Type,
					i64Type
				});
				llvm::Function* currentFunc = this->irBuilder.GetInsertBlock()->getParent();
				llvm::AllocaInst* argsAlloca = this->createEntryBlockAllocation(
					currentFunc, "pack.args", argsStructType
				);
				{
					llvm::Value* vtableFieldPtr = this->irBuilder.CreateStructGEP(
						argsStructType, argsAlloca, 0, "pack.args.vtable"
					);
					llvm::Value* itablePtr = llvm::ConstantPointerNull::get( llvm::cast<llvm::PointerType>( ptrTypeVA ) );
					std::string argsItableKey = std::string( semantic::qualname::classes::args::Name ) + ":ImmutableSequence";
					if( this->interfaceTableMap.count( argsItableKey ) == 0 ) {
						argsItableKey = semantic::qualname::classes::args::Name;
					}
					if( this->interfaceTableMap.count( argsItableKey ) > 0 ) {
						itablePtr = this->irBuilder.CreateBitCast(
							this->interfaceTableMap[argsItableKey], ptrTypeVA, "pack.args.itable"
						);
					}
					this->irBuilder.CreateStore( itablePtr, vtableFieldPtr );
				}
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
									llvm::Function* mallocFn = this->getOrCreateMalloc();
									llvm::Function* snprintfFn = this->getOrCreateSnprintf();
									if( argValue->getType()->isIntegerTy( 1 ) ) {
										llvm::Value* trueStr = this->irBuilder.CreateGlobalStringPtr( "True", "varg.bool.true" );
										llvm::Value* falseStr = this->irBuilder.CreateGlobalStringPtr( "False", "varg.bool.false" );
										argValue = this->irBuilder.CreateSelect( argValue, trueStr, falseStr, "varg.bool.str" );
									}
									else {
										MIRVariableIdentifier argVarId = instruction.sourceOperands[variadicArgStart + i];
										bool isCharType = false;
										if( functionDefinition.variableDescriptorTable.count( argVarId ) ) {
											semantic::TypeSharedPointer argSemaType = functionDefinition.variableDescriptorTable[argVarId].variableType;
											isCharType = argSemaType != nullptr && argSemaType->name == semantic::qualname::classes::Char::Name;
										}
										llvm::Value* buf = this->irBuilder.CreateCall( mallocFn, {
											llvm::ConstantInt::get( i64Type, 24 )
										}, "varg.int.buf" );
										if( isCharType ) {
											llvm::Value* val = argValue;
											llvm::Value* charFmt = this->irBuilder.CreateGlobalStringPtr( "%c", "varg.char.fmt" );
											this->irBuilder.CreateCall( snprintfFn, {
												buf, llvm::ConstantInt::get( i64Type, 24 ), charFmt, val
											} );
										}
										else {
											llvm::Value* val = argValue;
											if( argValue->getType() != i64Type ) {
												val = this->irBuilder.CreateSExt( argValue, i64Type, "varg.iext" );
											}
											llvm::Value* intFmt = this->irBuilder.CreateGlobalStringPtr( "%ld", "varg.int.fmt" );
											this->irBuilder.CreateCall( snprintfFn, {
												buf, llvm::ConstantInt::get( i64Type, 24 ), intFmt, val
											} );
										}
										argValue = buf;
									}
								}
								else if( argValue->getType()->isFloatingPointTy() && elemType->isPointerTy() ) {
									llvm::Function* mallocFn = this->getOrCreateMalloc();
									llvm::Function* snprintfFn = this->getOrCreateSnprintf();
									llvm::Value* buf = this->irBuilder.CreateCall( mallocFn, {
										llvm::ConstantInt::get( i64Type, 48 )
									}, "varg.flt.buf" );
									llvm::Value* val = argValue;
									if( argValue->getType()->isFloatTy() ) {
										val = this->irBuilder.CreateFPExt( val, llvm::Type::getDoubleTy( this->llvmContext ), "varg.f2d" );
									}
									llvm::Value* fltFmt = this->irBuilder.CreateGlobalStringPtr( "%g", "varg.flt.fmt" );
									this->irBuilder.CreateCall( snprintfFn, {
										buf, llvm::ConstantInt::get( i64Type, 48 ), fltFmt, val
									} );
									argValue = buf;
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
							else if( argValue->getType()->isPointerTy() && elemType->isPointerTy() &&
								calleeVariadicElemType != nullptr &&
								calleeVariadicElemType->name == semantic::qualname::classes::object::Name ) {
								MIRVariableIdentifier variadicArgVarId = instruction.sourceOperands[variadicArgStart + i];
								if( functionDefinition.variableDescriptorTable.count( variadicArgVarId ) > 0 ) {
									semantic::TypeSharedPointer variadicArgSemaType =
										functionDefinition.variableDescriptorTable[variadicArgVarId].variableType;
									if( variadicArgSemaType != nullptr ) {
										if( variadicArgSemaType->kind == semantic::Type::Kind::None ) {
											argValue = this->irBuilder.CreateGlobalStringPtr( "None", "varg.none.str" );
										}
										else if( variadicArgSemaType->kind == semantic::Type::Kind::Class &&
											variadicArgSemaType->name != semantic::qualname::classes::string::Name ) {
											std::string variadicArgTypeName = variadicArgSemaType->name;
											size_t variadicArgTypeNameGenericPos = variadicArgTypeName.find( '<' );
											if( variadicArgTypeNameGenericPos != std::string::npos ) {
												variadicArgTypeName = variadicArgTypeName.substr( 0, variadicArgTypeNameGenericPos );
											}
											bool isPrimitiveOopWrapper =
												descriptor::Builtin::integerBitWidths.count( variadicArgTypeName ) > 0 ||
												descriptor::Builtin::floatOopNames.count( variadicArgTypeName ) > 0 ||
												variadicArgTypeName == semantic::qualname::classes::boolean::Name;
											if( isPrimitiveOopWrapper ) {
												llvm::Function* mallocFn = this->getOrCreateMalloc();
												llvm::Function* snprintfFn = this->getOrCreateSnprintf();
												llvm::Value* primitiveIntVal = this->irBuilder.CreatePtrToInt(
													argValue, i64Type, "varg.prim.ptoi"
												);
												if( variadicArgTypeName == semantic::qualname::classes::boolean::Name ) {
													llvm::Value* boolVal = this->irBuilder.CreateTrunc(
														primitiveIntVal, llvm::Type::getInt1Ty( this->llvmContext ), "varg.prim.bool"
													);
													llvm::Value* trueStr = this->irBuilder.CreateGlobalStringPtr( "True", "varg.prim.true" );
													llvm::Value* falseStr = this->irBuilder.CreateGlobalStringPtr( "False", "varg.prim.false" );
													argValue = this->irBuilder.CreateSelect( boolVal, trueStr, falseStr, "varg.prim.boolstr" );
												}
												else if( descriptor::Builtin::floatOopNames.count( variadicArgTypeName ) > 0 ) {
													llvm::Value* floatVal = this->irBuilder.CreateBitCast(
														primitiveIntVal, llvm::Type::getDoubleTy( this->llvmContext ), "varg.prim.fp"
													);
													llvm::Value* primitiveBuf = this->irBuilder.CreateCall( mallocFn, {
														llvm::ConstantInt::get( i64Type, 48 )
													}, "varg.prim.flt.buf" );
													llvm::Value* fltFmt = this->irBuilder.CreateGlobalStringPtr( "%g", "varg.prim.flt.fmt" );
													this->irBuilder.CreateCall( snprintfFn, {
														primitiveBuf, llvm::ConstantInt::get( i64Type, 48 ), fltFmt, floatVal
													} );
													argValue = primitiveBuf;
												}
												else if( variadicArgTypeName == semantic::qualname::classes::Char::Name ) {
													llvm::Value* primitiveBuf = this->irBuilder.CreateCall( mallocFn, {
														llvm::ConstantInt::get( i64Type, 24 )
													}, "varg.prim.char.buf" );
													llvm::Value* charFmt = this->irBuilder.CreateGlobalStringPtr( "%c", "varg.prim.char.fmt" );
													this->irBuilder.CreateCall( snprintfFn, {
														primitiveBuf, llvm::ConstantInt::get( i64Type, 24 ), charFmt, primitiveIntVal
													} );
													argValue = primitiveBuf;
												}
												else {
													bool isUnsignedWrapper = descriptor::Builtin::unsignedOopNames.count( variadicArgTypeName ) > 0;
													llvm::Value* primitiveBuf = this->irBuilder.CreateCall( mallocFn, {
														llvm::ConstantInt::get( i64Type, 24 )
													}, "varg.prim.int.buf" );
													llvm::Value* intFmt = this->irBuilder.CreateGlobalStringPtr(
														isUnsignedWrapper ? "%lu" : "%ld", "varg.prim.int.fmt"
													);
													this->irBuilder.CreateCall( snprintfFn, {
														primitiveBuf, llvm::ConstantInt::get( i64Type, 24 ), intFmt, primitiveIntVal
													} );
													argValue = primitiveBuf;
												}
											}
											else {
												std::string variadicArgQualifiedName = variadicArgSemaType->qualified.empty() == false
													? variadicArgSemaType->qualified : variadicArgSemaType->name;
												size_t variadicArgGenericPos = variadicArgQualifiedName.find( '<' );
												if( variadicArgGenericPos != std::string::npos ) {
													variadicArgQualifiedName = variadicArgQualifiedName.substr( 0, variadicArgGenericPos );
												}
												std::string variadicArgToStringName = variadicArgQualifiedName + "." +
													semantic::qualname::classes::object::methods::ToString;
												std::unordered_map<std::string, llvm::Function*>::iterator variadicToStringIt =
													this->functionResolutionMap.find( variadicArgToStringName );
												if( variadicToStringIt != this->functionResolutionMap.end() ) {
													argValue = this->irBuilder.CreateCall(
														variadicToStringIt->second, { argValue }, "varg.tostr"
													);
												}
											}
										}
										else if( variadicArgSemaType->kind == semantic::Type::Kind::Interface ) {
											llvm::Type* vtablePtrType = llvm::PointerType::getUnqual( this->llvmContext );
											std::vector<llvm::Type*> vtableWrapperFields = { vtablePtrType };
											llvm::Value* vtableSlotPtr = this->irBuilder.CreateStructGEP(
												llvm::StructType::get( this->llvmContext, vtableWrapperFields ),
												argValue, 0, "varg.iface.vtable.slot"
											);
											llvm::Value* vtablePtr = this->irBuilder.CreateLoad(
												vtablePtrType, vtableSlotPtr, "varg.iface.vtable.ptr"
											);
											llvm::FunctionType* toStringVoidType = llvm::FunctionType::get(
												llvm::Type::getVoidTy( this->llvmContext ), false
											);
											llvm::PointerType* toStringFuncPtrType = llvm::PointerType::getUnqual( toStringVoidType );
											llvm::Value* toStringSlotPtr = this->irBuilder.CreateGEP(
												toStringFuncPtrType, vtablePtr,
												llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->llvmContext ), 0 ),
												"varg.iface.tostr.slot"
											);
											llvm::Value* toStringFuncPtr = this->irBuilder.CreateLoad(
												toStringFuncPtrType, toStringSlotPtr, "varg.iface.tostr.ptr"
											);
											llvm::FunctionType* toStringCallType = llvm::FunctionType::get(
												llvm::PointerType::getUnqual( this->llvmContext ),
												{ llvm::PointerType::getUnqual( this->llvmContext ) }, false
											);
											llvm::Value* toStringCastedFunc = this->irBuilder.CreateBitCast(
												toStringFuncPtr, llvm::PointerType::getUnqual( toStringCallType ), "varg.iface.tostr.cast"
											);
											argValue = this->irBuilder.CreateCall(
												toStringCallType, toStringCastedFunc, { argValue }, "varg.iface.tostr.result"
											);
										}
									}
								}
							}
							llvm::Value* elemGep = this->irBuilder.CreateConstGEP2_32(
								dataArrayType, dataAlloca, 0, static_cast<unsigned>( i ), "pack.args.gep"
							);
							this->irBuilder.CreateStore( argValue, elemGep );
						}
					}
					llvm::Value* dataFieldPtr = this->irBuilder.CreateStructGEP(
						argsStructType, argsAlloca, 1, "pack.args.data.field"
					);
					this->irBuilder.CreateStore( dataAlloca, dataFieldPtr );
				}
				else {
					llvm::Value* dataFieldPtr = this->irBuilder.CreateStructGEP(
						argsStructType, argsAlloca, 1, "pack.args.data.field"
					);
					this->irBuilder.CreateStore(
						llvm::ConstantPointerNull::get( llvm::PointerType::getUnqual( this->llvmContext ) ),
						dataFieldPtr
					);
				}
				llvm::Value* countFieldPtr = this->irBuilder.CreateStructGEP(
					argsStructType, argsAlloca, 2, "pack.args.count.field"
				);
				this->irBuilder.CreateStore( llvm::ConstantInt::get( i64Type, variadicArgCount ), countFieldPtr );
				llvm::Value* posFieldPtr = this->irBuilder.CreateStructGEP(
					argsStructType, argsAlloca, 3, "pack.args.pos.field"
				);
				this->irBuilder.CreateStore( llvm::ConstantInt::get( i64Type, 0 ), posFieldPtr );
				arguments.push_back( argsAlloca );
			}
			size_t kwArgStart = variadicArgStart + variadicArgCount;
			for( unsigned kwIdx = 0; kwIdx < keywordOnlyCount; kwIdx++ ) {
				unsigned paramIdx = static_cast<unsigned>( calleeVariadicIndex ) + 1 + kwIdx;
				
				bool foundExplicit = false;
				llvm::Value* argValue = nullptr;
				std::string paramName = "";
				MIRFunctionDefinition* calleeDef = nullptr;
				if( this->mirFunctionDefinitionMap.count( instruction.calledFunctionQualifiedName ) > 0 ) {
					calleeDef = this->mirFunctionDefinitionMap[instruction.calledFunctionQualifiedName];
				}
				if( calleeDef != nullptr && paramIdx < calleeDef->parameterVariableIdentifiers.size() ) {
					MIRVariableIdentifier paramVarId = calleeDef->parameterVariableIdentifiers[paramIdx];
					if( calleeDef->variableDescriptorTable.count( paramVarId ) > 0 ) {
						paramName = calleeDef->variableDescriptorTable[paramVarId].variableName;
					}
				}
				
				if( paramName.empty() == false ) {
					for( size_t kki = 0; kki < instruction.keywordArgumentKeys.size(); kki++ ) {
						if( instruction.keywordArgumentKeys[kki] == paramName ) {
							argValue = this->loadVariableValue( instruction.keywordArgumentValues[kki] );
							foundExplicit = true;
							break;
						}
					}
				}
				
				if( foundExplicit ) {
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
					llvm::Type* expectedType = calleeType->getParamType( paramIdx );
					bool usedDefault = false;
					if( calleeDef != nullptr ) {
						std::unordered_map<int, MIRModuleConstant>::iterator defaultIter =
							calleeDef->parameterDefaultValues.find( static_cast<int>( paramIdx ) );
						if( defaultIter != calleeDef->parameterDefaultValues.end() ) {
							MIRModuleConstant& defaultVal = defaultIter->second;
							switch( defaultVal.kind ) {
								case MIRModuleConstant::String: {
									llvm::Value* strPtr = this->irBuilder.CreateGlobalStringPtr(
										defaultVal.stringValue, "kw.default.str" );
									arguments.push_back( strPtr );
									usedDefault = true;
									break;
								}
								case MIRModuleConstant::Integer: {
									arguments.push_back( llvm::ConstantInt::get( expectedType, defaultVal.integerValue ) );
									usedDefault = true;
									break;
								}
								case MIRModuleConstant::Float: {
									arguments.push_back( llvm::ConstantFP::get( expectedType, defaultVal.floatValue ) );
									usedDefault = true;
									break;
								}
								case MIRModuleConstant::Boolean: {
									if( expectedType->isIntegerTy() ) {
										arguments.push_back( llvm::ConstantInt::get(
											expectedType, defaultVal.booleanValue ? 1 : 0 ) );
									}
									else {
										arguments.push_back( llvm::Constant::getNullValue( expectedType ) );
									}
									usedDefault = true;
									break;
								}
								case MIRModuleConstant::Null: {
									arguments.push_back( llvm::Constant::getNullValue( expectedType ) );
									usedDefault = true;
									break;
								}
							}
						}
					}
					if( usedDefault == false ) {
						arguments.push_back( llvm::Constant::getNullValue( expectedType ) );
					}
				}
			}
			if( calleeHasKwargsParam && instruction.keywordArgumentKeys.empty() == false &&
				arguments.size() < expectedParamCount ) {
				size_t kwargCount = instruction.keywordArgumentKeys.size();
				llvm::Type* i64TypeKw = llvm::Type::getInt64Ty( this->llvmContext );
				llvm::Type* ptrTypeKw = llvm::PointerType::getUnqual( this->llvmContext );
				llvm::StructType* kwargsStructTypeKw = nullptr;
				unsigned kwFieldOffset = 0;
				std::unordered_map<std::string, llvm::StructType*>::iterator kwCacheIt = this->structTypeCache.find( semantic::qualname::classes::kwargs::Name );
				if( kwCacheIt == this->structTypeCache.end() ) {
					kwCacheIt = this->structTypeCache.find( semantic::qualname::classes::kwargs::Qualified );
				}
				if( kwCacheIt != this->structTypeCache.end() && kwCacheIt->second->getNumElements() >= 3 ) {
					kwargsStructTypeKw = kwCacheIt->second;
					unsigned kwargsNumElements = kwargsStructTypeKw->getNumElements();
					if( kwargsNumElements > 3 ) {
						kwFieldOffset = kwargsNumElements - 3;
					}
				}
				if( kwargsStructTypeKw == nullptr ) {
					kwargsStructTypeKw = llvm::StructType::get( this->llvmContext, {
						ptrTypeKw, ptrTypeKw, i64TypeKw, i64TypeKw
					});
				}
				llvm::Function* currentFuncKw = this->irBuilder.GetInsertBlock()->getParent();
				llvm::AllocaInst* kwargsAllocaKw = this->createEntryBlockAllocation(
					currentFuncKw, "pack.kwargs", kwargsStructTypeKw
				);
				if( kwFieldOffset > 0 ) {
					llvm::Value* vtableFieldPtr = this->irBuilder.CreateStructGEP(
						kwargsStructTypeKw, kwargsAllocaKw, 0, "pack.kwargs.vtable"
					);
					this->irBuilder.CreateStore( llvm::ConstantPointerNull::get(
						llvm::cast<llvm::PointerType>( ptrTypeKw ) ), vtableFieldPtr );
				}
				if( kwargCount > 0 ) {
					llvm::ArrayType* keysArrayTypeKw = llvm::ArrayType::get( ptrTypeKw, kwargCount );
					llvm::ArrayType* valsArrayTypeKw = llvm::ArrayType::get( ptrTypeKw, kwargCount );
					llvm::AllocaInst* keysAllocaKw = this->createEntryBlockAllocation(
						currentFuncKw, "pack.kwargs.keys", keysArrayTypeKw
					);
					llvm::AllocaInst* valsAllocaKw = this->createEntryBlockAllocation(
						currentFuncKw, "pack.kwargs.vals", valsArrayTypeKw
					);
					for( size_t ki = 0; ki < kwargCount; ki++ ) {
						llvm::Value* keyStr = this->irBuilder.CreateGlobalStringPtr(
							instruction.keywordArgumentKeys[ki], "kwarg.key"
						);
						llvm::Value* keyGep = this->irBuilder.CreateConstGEP2_32(
							keysArrayTypeKw, keysAllocaKw, 0, static_cast<unsigned>( ki ), "pack.kwargs.key.gep"
						);
						this->irBuilder.CreateStore( keyStr, keyGep );
						llvm::Value* valExpr = this->loadVariableValue( instruction.keywordArgumentValues[ki] );
						if( valExpr != nullptr ) {
							if( valExpr->getType()->isPointerTy() == false ) {
								valExpr = this->irBuilder.CreateIntToPtr( valExpr, ptrTypeKw, "kwval.itop" );
							}
							llvm::Value* valGep = this->irBuilder.CreateConstGEP2_32(
								valsArrayTypeKw, valsAllocaKw, 0, static_cast<unsigned>( ki ), "pack.kwargs.val.gep"
							);
							this->irBuilder.CreateStore( valExpr, valGep );
						}
					}
					llvm::Value* keysFieldPtrKw = this->irBuilder.CreateStructGEP(
						kwargsStructTypeKw, kwargsAllocaKw, kwFieldOffset + 0, "pack.kwargs.keys.field"
					);
					this->irBuilder.CreateStore( keysAllocaKw, keysFieldPtrKw );
					llvm::Value* valsFieldPtrKw = this->irBuilder.CreateStructGEP(
						kwargsStructTypeKw, kwargsAllocaKw, kwFieldOffset + 1, "pack.kwargs.vals.field"
					);
					this->irBuilder.CreateStore( valsAllocaKw, valsFieldPtrKw );
				}
				else {
					llvm::Value* keysFieldPtrKw = this->irBuilder.CreateStructGEP(
						kwargsStructTypeKw, kwargsAllocaKw, kwFieldOffset + 0, "pack.kwargs.keys.field"
					);
					this->irBuilder.CreateStore( llvm::ConstantPointerNull::get(
						llvm::cast<llvm::PointerType>( ptrTypeKw ) ), keysFieldPtrKw );
					llvm::Value* valsFieldPtrKw = this->irBuilder.CreateStructGEP(
						kwargsStructTypeKw, kwargsAllocaKw, kwFieldOffset + 1, "pack.kwargs.vals.field"
					);
					this->irBuilder.CreateStore( llvm::ConstantPointerNull::get(
						llvm::cast<llvm::PointerType>( ptrTypeKw ) ), valsFieldPtrKw );
				}
				llvm::Value* countFieldPtrKw = this->irBuilder.CreateStructGEP(
					kwargsStructTypeKw, kwargsAllocaKw, kwFieldOffset + 2, "pack.kwargs.count.field"
				);
				this->irBuilder.CreateStore( llvm::ConstantInt::get( i64TypeKw, kwargCount ), countFieldPtrKw );
				if( kwFieldOffset + 3 < kwargsStructTypeKw->getNumElements() ) {
					llvm::Value* posFieldPtrKw = this->irBuilder.CreateStructGEP(
						kwargsStructTypeKw, kwargsAllocaKw, kwFieldOffset + 3, "pack.kwargs.pos.field"
					);
					this->irBuilder.CreateStore( llvm::ConstantInt::get( i64TypeKw, 0 ), posFieldPtrKw );
				}
				arguments.push_back( kwargsAllocaKw );
			}
			{
				MIRFunctionDefinition* varPaddingCalleeDef = nullptr;
				if( this->mirFunctionDefinitionMap.count( instruction.calledFunctionQualifiedName ) > 0 ) {
					varPaddingCalleeDef = this->mirFunctionDefinitionMap[instruction.calledFunctionQualifiedName];
				}
				while( arguments.size() < expectedParamCount ) {
					unsigned varPaddingIdx = static_cast<unsigned>( arguments.size() );
					llvm::Type* paramType = calleeType->getParamType( varPaddingIdx );
					bool usedDefault = false;
					if( varPaddingCalleeDef != nullptr ) {
						std::unordered_map<int, MIRModuleConstant>::iterator defaultIter =
							varPaddingCalleeDef->parameterDefaultValues.find( static_cast<int>( varPaddingIdx ) );
						if( defaultIter != varPaddingCalleeDef->parameterDefaultValues.end() ) {
							MIRModuleConstant& defaultVal = defaultIter->second;
							switch( defaultVal.kind ) {
								case MIRModuleConstant::String: {
									llvm::Value* strPtr = this->irBuilder.CreateGlobalStringPtr(
										defaultVal.stringValue, "vpad.default.str" );
									arguments.push_back( strPtr );
									usedDefault = true;
									break;
								}
								case MIRModuleConstant::Integer: {
									arguments.push_back( llvm::ConstantInt::get( paramType, defaultVal.integerValue, true ) );
									usedDefault = true;
									break;
								}
								case MIRModuleConstant::Float: {
									arguments.push_back( llvm::ConstantFP::get( paramType, defaultVal.floatValue ) );
									usedDefault = true;
									break;
								}
								case MIRModuleConstant::Boolean: {
									if( paramType->isIntegerTy() ) {
										arguments.push_back( llvm::ConstantInt::get(
											paramType, defaultVal.booleanValue ? 1 : 0 ) );
									}
									else {
										arguments.push_back( llvm::Constant::getNullValue( paramType ) );
									}
									usedDefault = true;
									break;
								}
								case MIRModuleConstant::Null: {
									arguments.push_back( llvm::Constant::getNullValue( paramType ) );
									usedDefault = true;
									break;
								}
							}
						}
					}
					if( usedDefault == false ) {
						arguments.push_back( llvm::Constant::getNullValue( paramType ) );
					}
				}
			}
			llvm::Value* result = this->emitCallOrInvoke( instruction, callee, arguments );
			if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER &&
				callee->getReturnType()->isVoidTy() == false ) {
				if( result != nullptr && result->getType()->isPointerTy() && instruction.operandType != nullptr ) {
					llvm::Type* expectedReturnType = this->toLLVMType( instruction.operandType );
					if( expectedReturnType->isIntegerTy() ) {
						result = this->irBuilder.CreatePtrToInt( result, expectedReturnType, "generic.ret.unbox" );
					}
					else if( expectedReturnType->isFloatingPointTy() ) {
						llvm::Value* asInt = this->irBuilder.CreatePtrToInt(
							result, llvm::Type::getInt64Ty( this->llvmContext ), "generic.ret.ptoi"
						);
						result = this->irBuilder.CreateBitCast( asInt, expectedReturnType, "generic.ret.fpunbox" );
					}
				}
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
			if( argumentValue == nullptr ) {
				if( argumentIndex < expectedParamCount ) {
					arguments.push_back( llvm::Constant::getNullValue( calleeType->getParamType( argumentIndex ) ) );
				}
				continue;
			}
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
		if( instruction.keywordArgumentKeys.empty() == false &&
			arguments.size() < expectedParamCount ) {
			bool calleeHasKwargsParam = false;
			MIRFunctionDefinition* calleeMIRDef = nullptr;
			if( this->mirFunctionDefinitionMap.count( calledName ) > 0 ) {
				calleeMIRDef = this->mirFunctionDefinitionMap[calledName];
			}
			if( calleeMIRDef == nullptr && callee != nullptr ) {
				std::string resolvedName = callee->getName().str();
				if( this->mirFunctionDefinitionMap.count( resolvedName ) > 0 ) {
					calleeMIRDef = this->mirFunctionDefinitionMap[resolvedName];
				}
			}
			if( calleeMIRDef != nullptr && calleeMIRDef->keywordParameterIndex >= 0 ) {
				calleeHasKwargsParam = true;
			}
			if( calleeHasKwargsParam == false && calleeMIRDef != nullptr ) {
				for( size_t ki = 0; ki < instruction.keywordArgumentKeys.size(); ki++ ) {
					std::string keyName = instruction.keywordArgumentKeys[ki];
					for( size_t pi = 0; pi < calleeMIRDef->parameterVariableIdentifiers.size(); pi++ ) {
						MIRVariableIdentifier paramVarId = calleeMIRDef->parameterVariableIdentifiers[pi];
						if( calleeMIRDef->variableDescriptorTable.count( paramVarId ) > 0 ) {
							MIRVariableDescriptor& paramDesc = calleeMIRDef->variableDescriptorTable[paramVarId];
							if( paramDesc.variableName == keyName ) {
								while( arguments.size() < pi ) {
									llvm::Type* padType = calleeType->getParamType( arguments.size() );
									arguments.push_back( llvm::Constant::getNullValue( padType ) );
								}
								llvm::Value* argValue = this->loadVariableValue( instruction.keywordArgumentValues[ki] );
								if( argValue != nullptr && pi < expectedParamCount ) {
									llvm::Type* expectedType = calleeType->getParamType( pi );
									if( argValue->getType() != expectedType ) {
										if( argValue->getType()->isIntegerTy() && expectedType->isIntegerTy() ) {
											argValue = this->irBuilder.CreateIntCast( argValue, expectedType, true, "kw.cast" );
										}
										else if( argValue->getType()->isIntegerTy() && expectedType->isPointerTy() ) {
											argValue = this->irBuilder.CreateIntToPtr( argValue, expectedType, "kw.itop" );
										}
										else if( argValue->getType()->isPointerTy() && expectedType->isIntegerTy() ) {
											argValue = this->irBuilder.CreatePtrToInt( argValue, expectedType, "kw.ptoi" );
										}
										else if( argValue->getType()->isIntegerTy() && expectedType->isFloatingPointTy() ) {
											argValue = this->irBuilder.CreateSIToFP( argValue, expectedType, "kw.itofp" );
										}
									}
								}
								if( pi < arguments.size() ) {
									arguments[pi] = argValue;
								}
								else {
									arguments.push_back( argValue );
								}
								break;
							}
						}
					}
				}
			}
			if( calleeHasKwargsParam && arguments.size() < expectedParamCount ) {
			size_t kwargCount = instruction.keywordArgumentKeys.size();
			llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
			llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
			llvm::Type* valType = ptrType;
			unsigned kwargsParamIdx = arguments.size();
			llvm::Type* expectedKwargsType = calleeType->getParamType( kwargsParamIdx );
			if( expectedKwargsType->isPointerTy() ) {
				valType = ptrType;
			}
			llvm::StructType* kwargsStructType = nullptr;
			unsigned kwFieldOff = 0;
			std::unordered_map<std::string, llvm::StructType*>::iterator kwCacheIt2 = this->structTypeCache.find( semantic::qualname::classes::kwargs::Name );
			if( kwCacheIt2 == this->structTypeCache.end() ) {
				kwCacheIt2 = this->structTypeCache.find( semantic::qualname::classes::kwargs::Qualified );
			}
			if( kwCacheIt2 != this->structTypeCache.end() && kwCacheIt2->second->getNumElements() >= 3 ) {
				kwargsStructType = kwCacheIt2->second;
				unsigned kwargsNumElements2 = kwargsStructType->getNumElements();
				if( kwargsNumElements2 > 3 ) {
					kwFieldOff = kwargsNumElements2 - 3;
				}
			}
			if( kwargsStructType == nullptr ) {
				kwargsStructType = llvm::StructType::get( this->llvmContext, {
					ptrType, ptrType, i64Type, i64Type
				});
			}
			llvm::Function* currentFunc = this->irBuilder.GetInsertBlock()->getParent();
			llvm::AllocaInst* kwargsAlloca = this->createEntryBlockAllocation(
				currentFunc, "pack.kwargs", kwargsStructType
			);
			if( kwFieldOff > 0 ) {
				llvm::Value* vtableFieldPtr = this->irBuilder.CreateStructGEP(
					kwargsStructType, kwargsAlloca, 0, "pack.kwargs.vtable"
				);
				this->irBuilder.CreateStore( llvm::ConstantPointerNull::get(
					llvm::cast<llvm::PointerType>( ptrType ) ), vtableFieldPtr );
			}
			if( kwargCount > 0 ) {
				llvm::ArrayType* keysArrayType = llvm::ArrayType::get( ptrType, kwargCount );
				llvm::ArrayType* valsArrayType = llvm::ArrayType::get( valType, kwargCount );
				llvm::AllocaInst* keysAlloca = this->createEntryBlockAllocation(
					currentFunc, "pack.kwargs.keys", keysArrayType
				);
				llvm::AllocaInst* valsAlloca = this->createEntryBlockAllocation(
					currentFunc, "pack.kwargs.vals", valsArrayType
				);
				for( size_t i = 0; i < kwargCount; i++ ) {
					llvm::Value* keyStr = this->irBuilder.CreateGlobalStringPtr(
						instruction.keywordArgumentKeys[i], "kwarg.key"
					);
					llvm::Value* keyGep = this->irBuilder.CreateConstGEP2_32(
						keysArrayType, keysAlloca, 0, static_cast<unsigned>( i ), "pack.kwargs.key.gep"
					);
					this->irBuilder.CreateStore( keyStr, keyGep );
					llvm::Value* valExpr = this->loadVariableValue( instruction.keywordArgumentValues[i] );
					if( valExpr != nullptr ) {
						if( valExpr->getType() != valType ) {
							if( valExpr->getType()->isPointerTy() && valType->isPointerTy() ) {
								// opaque pointers compatible
							}
							else if( valExpr->getType()->isIntegerTy() && valType->isPointerTy() ) {
								valExpr = this->irBuilder.CreateIntToPtr( valExpr, valType, "kwval.itop" );
							}
							else if( valExpr->getType()->isPointerTy() && valType->isIntegerTy() ) {
								valExpr = this->irBuilder.CreatePtrToInt( valExpr, valType, "kwval.ptoi" );
							}
						}
						llvm::Value* valGep = this->irBuilder.CreateConstGEP2_32(
							valsArrayType, valsAlloca, 0, static_cast<unsigned>( i ), "pack.kwargs.val.gep"
						);
						this->irBuilder.CreateStore( valExpr, valGep );
					}
				}
				llvm::Value* keysFieldPtr = this->irBuilder.CreateStructGEP(
					kwargsStructType, kwargsAlloca, kwFieldOff + 0, "pack.kwargs.keys.field"
				);
				this->irBuilder.CreateStore( keysAlloca, keysFieldPtr );
				llvm::Value* valsFieldPtr = this->irBuilder.CreateStructGEP(
					kwargsStructType, kwargsAlloca, kwFieldOff + 1, "pack.kwargs.vals.field"
				);
				this->irBuilder.CreateStore( valsAlloca, valsFieldPtr );
			}
			else {
				llvm::Value* keysFieldPtr = this->irBuilder.CreateStructGEP(
					kwargsStructType, kwargsAlloca, kwFieldOff + 0, "pack.kwargs.keys.field"
				);
				this->irBuilder.CreateStore( llvm::ConstantPointerNull::get(
					llvm::cast<llvm::PointerType>( ptrType ) ), keysFieldPtr );
				llvm::Value* valsFieldPtr = this->irBuilder.CreateStructGEP(
					kwargsStructType, kwargsAlloca, kwFieldOff + 1, "pack.kwargs.vals.field"
				);
				this->irBuilder.CreateStore( llvm::ConstantPointerNull::get(
					llvm::cast<llvm::PointerType>( ptrType ) ), valsFieldPtr );
			}
			llvm::Value* countFieldPtr = this->irBuilder.CreateStructGEP(
				kwargsStructType, kwargsAlloca, kwFieldOff + 2, "pack.kwargs.count.field"
			);
			this->irBuilder.CreateStore( llvm::ConstantInt::get( i64Type, kwargCount ), countFieldPtr );
			if( kwFieldOff + 3 < kwargsStructType->getNumElements() ) {
				llvm::Value* posFieldPtr = this->irBuilder.CreateStructGEP(
					kwargsStructType, kwargsAlloca, kwFieldOff + 3, "pack.kwargs.pos.field"
				);
				this->irBuilder.CreateStore( llvm::ConstantInt::get( i64Type, 0 ), posFieldPtr );
			}
			arguments.push_back( kwargsAlloca );
		}
		}
		{
			MIRFunctionDefinition* paddingCalleeDef = nullptr;
			if( this->mirFunctionDefinitionMap.count( instruction.calledFunctionQualifiedName ) > 0 ) {
				paddingCalleeDef = this->mirFunctionDefinitionMap[instruction.calledFunctionQualifiedName];
			}
			while( arguments.size() < expectedParamCount ) {
				unsigned paddingParamIdx = static_cast<unsigned>( arguments.size() );
				llvm::Type* paramType = calleeType->getParamType( paddingParamIdx );
				bool usedDefault = false;
				if( paddingCalleeDef != nullptr ) {
					std::unordered_map<int, MIRModuleConstant>::iterator defaultIter =
						paddingCalleeDef->parameterDefaultValues.find( static_cast<int>( paddingParamIdx ) );
					if( defaultIter != paddingCalleeDef->parameterDefaultValues.end() ) {
						MIRModuleConstant& defaultVal = defaultIter->second;
						switch( defaultVal.kind ) {
							case MIRModuleConstant::String: {
								llvm::Value* strPtr = this->irBuilder.CreateGlobalStringPtr(
									defaultVal.stringValue, "pad.default.str" );
								arguments.push_back( strPtr );
								usedDefault = true;
								break;
							}
							case MIRModuleConstant::Integer: {
								arguments.push_back( llvm::ConstantInt::get( paramType, defaultVal.integerValue, true ) );
								usedDefault = true;
								break;
							}
							case MIRModuleConstant::Float: {
								arguments.push_back( llvm::ConstantFP::get( paramType, defaultVal.floatValue ) );
								usedDefault = true;
								break;
							}
							case MIRModuleConstant::Boolean: {
								if( paramType->isIntegerTy() ) {
									arguments.push_back( llvm::ConstantInt::get(
										paramType, defaultVal.booleanValue ? 1 : 0 ) );
								}
								else {
									arguments.push_back( llvm::Constant::getNullValue( paramType ) );
								}
								usedDefault = true;
								break;
							}
							case MIRModuleConstant::Null: {
								arguments.push_back( llvm::Constant::getNullValue( paramType ) );
								usedDefault = true;
								break;
							}
						}
					}
				}
				if( usedDefault == false ) {
					arguments.push_back( llvm::Constant::getNullValue( paramType ) );
				}
			}
		}
		if( arguments.size() > expectedParamCount && callee->isVarArg() == false ) {
			arguments.resize( expectedParamCount );
		}
		llvm::Value* result = this->emitCallOrInvoke( instruction, callee, arguments );
		if( instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER &&
			callee->getReturnType()->isVoidTy() == false ) {
			if( result != nullptr && result->getType()->isPointerTy() && instruction.operandType != nullptr ) {
				llvm::Type* expectedReturnType = this->toLLVMType( instruction.operandType );
				if( expectedReturnType->isIntegerTy() ) {
					result = this->irBuilder.CreatePtrToInt( result, expectedReturnType, "generic.ret.unbox" );
				}
				else if( expectedReturnType->isFloatingPointTy() ) {
					llvm::Value* asInt = this->irBuilder.CreatePtrToInt(
						result, llvm::Type::getInt64Ty( this->llvmContext ), "generic.ret.ptoi"
					);
					result = this->irBuilder.CreateBitCast( asInt, expectedReturnType, "generic.ret.fpunbox" );
				}
			}
			this->setVariableValue( instruction.destinationVariable, result );
			bool skipCallPropagation = false;
			if( this->currentMIRFunction != nullptr &&
				this->currentMIRFunction->variableDescriptorTable.count( instruction.destinationVariable ) > 0 ) {
				semantic::TypeSharedPointer destType =
					this->currentMIRFunction->variableDescriptorTable[instruction.destinationVariable].variableType;
				if( destType != nullptr && ( destType->kind == semantic::Type::Kind::Interface ||
					destType->kind == semantic::Type::Kind::GenericParameter ) ) {
					skipCallPropagation = true;
				}
			}
			if( skipCallPropagation == false ) {
				if( this->functionReturnConcreteClass.count( calledName ) > 0 ) {
					this->concreteClassMap[instruction.destinationVariable] =
						this->functionReturnConcreteClass[calledName];
				}
				else if( callee != nullptr ) {
					std::string resolvedCalleeName = callee->getName().str();
					if( this->functionReturnConcreteClass.count( resolvedCalleeName ) > 0 ) {
						this->concreteClassMap[instruction.destinationVariable] =
							this->functionReturnConcreteClass[resolvedCalleeName];
					}
				}
			}
		}
	}
	
	void MIRCodegen::generateReturnValue( const MIRInstruction& instruction ) {
		llvm::BasicBlock* insertBlock = this->irBuilder.GetInsertBlock();
		if( insertBlock == nullptr ) {
			return;
		}
		if( this->currentGeneratorContext != nullptr ) {
			this->irBuilder.CreateStore(
				llvm::ConstantInt::getTrue( this->llvmContext ),
				this->currentGeneratorContext->doneVar
			);
			this->irBuilder.CreateBr( this->currentGeneratorContext->exitBlock );
			return;
		}
		if( this->isGeneratingAsyncWrapper ) {
			llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
			llvm::Value* completionValue = llvm::ConstantInt::get( i64Type, 0 );
			if( instruction.sourceOperands.empty() == false ) {
				llvm::Value* retVal = this->loadVariableValue( instruction.sourceOperands[0] );
				if( retVal != nullptr ) {
					if( retVal->getType() == i64Type ) {
						completionValue = retVal;
					}
					else if( retVal->getType()->isIntegerTy() ) {
						completionValue = this->irBuilder.CreateIntCast( retVal, i64Type, true, "async.ret" );
					}
					else if( retVal->getType()->isPointerTy() ) {
						completionValue = this->irBuilder.CreatePtrToInt( retVal, i64Type, "async.ret" );
					}
					else if( retVal->getType()->isFloatingPointTy() ) {
						completionValue = this->irBuilder.CreateFPToSI( retVal, i64Type, "async.ret" );
					}
				}
			}
			llvm::Function* completeFunc = this->llvmModule->getFunction( "runtimeComplete" );
			if( completeFunc != nullptr ) {
				this->irBuilder.CreateCall( completeFunc, { completionValue } );
			}
			else {
				llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
				completeFunc = this->llvmModule->getFunction( "uraniteTaskComplete" );
				if( completeFunc == nullptr ) {
					llvm::FunctionType* completeType = llvm::FunctionType::get(
						llvm::Type::getVoidTy( this->llvmContext ), { ptrType, i64Type }, false
					);
					completeFunc = llvm::Function::Create(
						completeType, llvm::Function::ExternalLinkage, "uraniteTaskComplete", this->llvmModule.get()
					);
				}
				llvm::Function* wrapperFunc = insertBlock->getParent();
				llvm::Value* taskArg = wrapperFunc->getArg( 0 );
				this->irBuilder.CreateCall( completeFunc, { taskArg, completionValue } );
			}
			this->emitPopFrame();
			this->irBuilder.CreateRetVoid();
			return;
		}
		llvm::Function* currentFunction = insertBlock->getParent();
		llvm::Type* expectedReturnType = currentFunction->getReturnType();
		if( this->programHasAsyncFunctions && currentFunction->getName() == semantic::qualname::functions::main::Name ) {
			llvm::Function* runSchedulerFunc = this->llvmModule->getFunction( "runtimeRun" );
			if( runSchedulerFunc == nullptr ) {
				runSchedulerFunc = this->llvmModule->getFunction( "uraniteRunScheduler" );
				if( runSchedulerFunc == nullptr ) {
					llvm::FunctionType* runType = llvm::FunctionType::get( llvm::Type::getVoidTy( this->llvmContext ), false );
					runSchedulerFunc = llvm::Function::Create( runType, llvm::Function::ExternalLinkage, "uraniteRunScheduler", this->llvmModule.get() );
				}
			}
			this->irBuilder.CreateCall( runSchedulerFunc );
		}
		if( expectedReturnType->isVoidTy() ) {
			this->emitPopFrame();
			this->irBuilder.CreateRetVoid();
			return;
		}
		if( instruction.sourceOperands.empty() ) {
			this->emitPopFrame();
			this->irBuilder.CreateRet( llvm::Constant::getNullValue( expectedReturnType ) );
			return;
		}
		llvm::Value* returnValue = this->loadVariableValue( instruction.sourceOperands[0] );
		if( returnValue == nullptr ) {
			this->emitPopFrame();
			this->irBuilder.CreateRet( llvm::Constant::getNullValue( expectedReturnType ) );
			return;
		}
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
				this->emitPopFrame();
				this->irBuilder.CreateRet( llvm::Constant::getNullValue( expectedReturnType ) );
				return;
			}
		}
		this->emitPopFrame();
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
		if( instruction.fieldAccessName == semantic::qualname::fields::Name || instruction.fieldAccessName == semantic::qualname::fields::Value ) {
			MIRVariableIdentifier sourceVar = instruction.sourceOperands[0];
			if( this->currentMIRFunction != nullptr &&
				this->currentMIRFunction->variableDescriptorTable.count( sourceVar ) > 0 ) {
				MIRVariableDescriptor& descriptor =
					this->currentMIRFunction->variableDescriptorTable[sourceVar];
				if( descriptor.variableType != nullptr &&
					descriptor.variableType->kind == semantic::Type::Kind::Enum ) {
					semantic::EnumTypeSharedPointer enumType =
						std::dynamic_pointer_cast<semantic::EnumType>(
							this->semanticAnalyzer.types().lookupType( descriptor.variableType->name ) );
					if( instruction.fieldAccessName == semantic::qualname::fields::Value && enumType != nullptr &&
						enumType->astDeclaration != nullptr && enumType->variants.empty() == false ) {
						bool hasBacked = enumType->backedType != nullptr;
						if( hasBacked ) {
							llvm::Value* result = nullptr;
							for( size_t variantIndex = 0; variantIndex < enumType->variants.size(); variantIndex++ ) {
								const semantic::EnumVariantInfo& variant = enumType->variants[variantIndex];
								llvm::Value* backedVal = nullptr;
								int64_t compareValue = variant.discriminant;
								for( const ast::nodes::EnumVariantSharedPointer& astVariant : enumType->astDeclaration->variants ) {
									if( astVariant->name == variant.name && astVariant->backedValue != nullptr ) {
										if( astVariant->backedValue->kind == ast::Node::Kind::FloatLiteral ) {
											ast::nodes::FloatLiteralExpression& floatLit = static_cast<ast::nodes::FloatLiteralExpression&>( *astVariant->backedValue );
											backedVal = llvm::ConstantFP::get( llvm::Type::getDoubleTy( this->llvmContext ), floatLit.value );
										}
										else if( astVariant->backedValue->kind == ast::Node::Kind::IntegerLiteral ) {
											ast::nodes::IntegerLiteralExpression& intLit = static_cast<ast::nodes::IntegerLiteralExpression&>( *astVariant->backedValue );
											backedVal = llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->llvmContext ), intLit.value, true );
											compareValue = intLit.value;
										}
										else if( astVariant->backedValue->kind == ast::Node::Kind::StringLiteral ) {
											ast::nodes::StringLiteralExpression& strLit = static_cast<ast::nodes::StringLiteralExpression&>( *astVariant->backedValue );
											backedVal = this->irBuilder.CreateGlobalStringPtr( strLit.value, fmt::format( "enum.val.{}", variant.name ) );
										}
										break;
									}
								}
								if( backedVal == nullptr ) {
									continue;
								}
								llvm::Value* isMatch = this->irBuilder.CreateICmpEQ(
									basePointer,
									llvm::ConstantInt::get( basePointer->getType(), compareValue ),
									fmt::format( "cmp.{}", variant.name ) );
								if( result == nullptr ) {
									result = backedVal;
								}
								result = this->irBuilder.CreateSelect( isMatch, backedVal, result,
									fmt::format( "sel.{}", variant.name ) );
							}
							if( result != nullptr ) {
								this->setVariableValue( instruction.destinationVariable, result );
								return;
							}
						}
						this->setVariableValue( instruction.destinationVariable, basePointer );
						return;
					}
					else if( instruction.fieldAccessName == semantic::qualname::fields::Value ) {
						this->setVariableValue( instruction.destinationVariable, basePointer );
						return;
					}
					if( enumType != nullptr && enumType->variants.empty() == false ) {
						bool hasBackedName = enumType->backedType != nullptr;
						llvm::Value* result = this->irBuilder.CreateGlobalStringPtr( "?", "enum.unknown" );
						for( const semantic::EnumVariantInfo& variant : enumType->variants ) {
							llvm::Value* variantName = this->irBuilder.CreateGlobalStringPtr(
								variant.name, fmt::format( "enum.{}", variant.name ) );
							int64_t compareValue = variant.discriminant;
							if( hasBackedName && enumType->astDeclaration != nullptr ) {
								for( const ast::nodes::EnumVariantSharedPointer& astVariant : enumType->astDeclaration->variants ) {
									if( astVariant->name == variant.name && astVariant->backedValue != nullptr ) {
										if( astVariant->backedValue->kind == ast::Node::Kind::IntegerLiteral ) {
											ast::nodes::IntegerLiteralExpression& intLit =
												static_cast<ast::nodes::IntegerLiteralExpression&>( *astVariant->backedValue );
											compareValue = intLit.value;
										}
										break;
									}
								}
							}
							llvm::Value* isMatch = this->irBuilder.CreateICmpEQ(
								basePointer,
								llvm::ConstantInt::get( basePointer->getType(), compareValue ),
								fmt::format( "cmp.{}", variant.name ) );
							result = this->irBuilder.CreateSelect( isMatch, variantName, result,
								fmt::format( "sel.{}", variant.name ) );
						}
						this->setVariableValue( instruction.destinationVariable, result );
						return;
					}
				}
			}
		}
		if( basePointer->getType()->isPointerTy() == false ) {
			this->setVariableValue( instruction.destinationVariable, basePointer );
			return;
		}
		llvm::Type* pointedType = nullptr;
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
		if( pointedType == nullptr ) {
			llvm::Value* rawPointer = this->getVariableValue( instruction.sourceOperands[0] );
			if( llvm::AllocaInst* allocaInst = llvm::dyn_cast<llvm::AllocaInst>( rawPointer ) ) {
				llvm::Type* allocatedType = allocaInst->getAllocatedType();
				if( allocatedType->isStructTy() ) {
					pointedType = allocatedType;
				}
			}
		}
		if( pointedType == nullptr && this->currentMIRFunction != nullptr ) {
			MIRVariableIdentifier sourceVariable = instruction.sourceOperands[0];
			if( this->currentMIRFunction->variableDescriptorTable.count( sourceVariable ) > 0 ) {
				MIRVariableDescriptor& descriptor =
					this->currentMIRFunction->variableDescriptorTable[sourceVariable];
				if( descriptor.variableType != nullptr ) {
					std::string typeName = descriptor.variableType->qualified.empty() == false
						? descriptor.variableType->qualified
						: descriptor.variableType->name;
					if( this->structTypeCache.count( typeName ) > 0 ) {
						pointedType = this->structTypeCache[typeName];
					}
					else if( this->structTypeCache.count( descriptor.variableType->name ) > 0 ) {
						pointedType = this->structTypeCache[descriptor.variableType->name];
					}
					else {
						size_t genericPosition = typeName.find( '<' );
						if( genericPosition != std::string::npos ) {
							typeName = typeName.substr( 0, genericPosition );
						}
						if( this->structTypeCache.count( typeName ) > 0 ) {
							pointedType = this->structTypeCache[typeName];
						}
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
		if( pointedType == nullptr && instruction.sourceOperands.empty() == false ) {
			MIRVariableIdentifier sourceVar = instruction.sourceOperands[0];
			if( this->concreteClassMap.count( sourceVar ) > 0 ) {
				std::string concreteName = this->concreteClassMap[sourceVar];
				if( this->structTypeCache.count( concreteName ) > 0 ) {
					pointedType = this->structTypeCache[concreteName];
				}
			}
		}
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
					if( getterName.empty() == false &&
						this->functionReturnConcreteClass.count( getterName ) > 0 ) {
						bool skipPropagation = false;
						if( this->currentMIRFunction != nullptr &&
							this->currentMIRFunction->variableDescriptorTable.count( instruction.destinationVariable ) > 0 ) {
							semantic::TypeSharedPointer destType =
								this->currentMIRFunction->variableDescriptorTable[instruction.destinationVariable].variableType;
							if( destType != nullptr && ( destType->kind == semantic::Type::Kind::Interface ||
								destType->kind == semantic::Type::Kind::GenericParameter ) ) {
								skipPropagation = true;
							}
						}
						if( skipPropagation == false ) {
							this->concreteClassMap[instruction.destinationVariable] =
								this->functionReturnConcreteClass[getterName];
						}
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
		else if( instruction.fieldAccessName.empty() == false ) {
			llvm::Function* getterFunction = nullptr;
			if( this->currentMIRFunction != nullptr ) {
				MIRVariableIdentifier sourceVariable = instruction.sourceOperands[0];
				if( this->currentMIRFunction->variableDescriptorTable.count( sourceVariable ) > 0 ) {
					MIRVariableDescriptor& descriptor =
						this->currentMIRFunction->variableDescriptorTable[sourceVariable];
					if( descriptor.variableType != nullptr ) {
						std::string typeName = descriptor.variableType->name;
						size_t genericPos = typeName.find( '<' );
						if( genericPos != std::string::npos ) {
							typeName = typeName.substr( 0, genericPos );
						}
						std::string getterName = typeName + "." + instruction.fieldAccessName;
						if( this->functionResolutionMap.count( getterName ) > 0 ) {
							getterFunction = this->functionResolutionMap[getterName];
						}
						if( getterFunction == nullptr ) {
							MIRVariableIdentifier sourceVariable = instruction.sourceOperands[0];
							if( this->concreteClassMap.count( sourceVariable ) > 0 ) {
								std::string concreteGetterName =
									this->concreteClassMap[sourceVariable] + "." + instruction.fieldAccessName;
								if( this->functionResolutionMap.count( concreteGetterName ) > 0 ) {
									getterFunction = this->functionResolutionMap[concreteGetterName];
								}
							}
						}
						if( getterFunction == nullptr &&
							descriptor.variableType->kind == semantic::Type::Kind::Interface ) {
							for( const std::pair<const std::string, llvm::StructType*>& cacheEntry :
								this->structTypeCache ) {
								std::string candidateName =
									cacheEntry.first + "." + instruction.fieldAccessName;
								if( this->functionResolutionMap.count( candidateName ) > 0 ) {
									llvm::Function* candidate = this->functionResolutionMap[candidateName];
									if( candidate != nullptr && candidate->arg_size() == 1 ) {
										getterFunction = candidate;
										break;
									}
								}
							}
						}
					}
				}
			}
			if( getterFunction != nullptr && getterFunction->arg_size() == 1 ) {
				llvm::Value* result = this->irBuilder.CreateCall(
					getterFunction, { basePointer }, "prop." + instruction.fieldAccessName
				);
				this->setVariableValue( instruction.destinationVariable, result );
				std::string resolvedGetterName = getterFunction->getName().str();
				if( this->functionReturnConcreteClass.count( resolvedGetterName ) > 0 ) {
					bool skipPropagation = false;
					if( this->currentMIRFunction != nullptr &&
						this->currentMIRFunction->variableDescriptorTable.count( instruction.destinationVariable ) > 0 ) {
						semantic::TypeSharedPointer destType =
							this->currentMIRFunction->variableDescriptorTable[instruction.destinationVariable].variableType;
						if( destType != nullptr && ( destType->kind == semantic::Type::Kind::Interface ||
							destType->kind == semantic::Type::Kind::GenericParameter ) ) {
							skipPropagation = true;
						}
					}
					if( skipPropagation == false ) {
						this->concreteClassMap[instruction.destinationVariable] =
							this->functionReturnConcreteClass[resolvedGetterName];
					}
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
			llvm::Type* elementType = llvm::Type::getInt8Ty( this->llvmContext );
			if( instruction.operandType != nullptr ) {
				elementType = this->toLLVMType( instruction.operandType );
			}
			llvm::Value* elementPointer = this->irBuilder.CreateGEP(
				elementType,
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
		std::string typeName = instruction.calledFunctionQualifiedName;
		size_t genericBracket = typeName.find( '<' );
		if( genericBracket != std::string::npos ) {
			typeName = typeName.substr( 0, genericBracket );
		}
		std::string shortTypeName = typeName;
		{
			size_t lastDot = typeName.rfind( '.' );
			if( lastDot != std::string::npos ) {
				shortTypeName = typeName.substr( lastDot + 1 );
			}
		}
		
		// Memory<T> is a compiler intrinsic — raw calloc'd array, not a struct
		if( shortTypeName == semantic::qualname::classes::memory::Name ) {
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
		if( shortTypeName == semantic::qualname::classes::arena::Name ) {
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
		if( descriptor::Builtin::oopWrapperNames.count( shortTypeName ) > 0 ) {
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
		if( structType == nullptr && typeName.empty() == false ) {
			for( const std::pair<const std::string, llvm::StructType*>& cacheEntry : this->structTypeCache ) {
				size_t lastDot = cacheEntry.first.rfind( '.' );
				if( lastDot != std::string::npos && cacheEntry.first.substr( lastDot + 1 ) == shortTypeName ) {
					structType = cacheEntry.second;
					typeName = cacheEntry.first;
					break;
				}
			}
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
			this->concreteClassMap[instruction.destinationVariable] = typeName;
			if( this->classesWithVtable.count( typeName ) > 0 || this->classesWithVtable.count( shortTypeName ) > 0 ) {
				std::string itableKey = typeName;
				if( this->interfaceTableMap.count( itableKey ) == 0 ) {
					itableKey = shortTypeName;
				}
				if( this->interfaceTableMap.count( itableKey ) > 0 ) {
					llvm::GlobalVariable* itableGlobal = this->interfaceTableMap[itableKey];
					llvm::Value* vtableSlot = this->irBuilder.CreateStructGEP( structType, typedPointer, 0, "vtable.slot" );
					llvm::Value* itablePtr = this->irBuilder.CreateBitCast(
						itableGlobal, llvm::PointerType::getUnqual( this->llvmContext ), "itable.ptr"
					);
					this->irBuilder.CreateStore( itablePtr, vtableSlot );
				}
			}
			std::string constructorName = typeName + "." + shortTypeName;
			llvm::Function* constructorFunction = nullptr;
			if( this->functionResolutionMap.count( constructorName ) > 0 ) {
				constructorFunction = this->functionResolutionMap[constructorName];
			}
			else {
				constructorFunction = this->llvmModule->getFunction( constructorName );
			}
			if( constructorFunction == nullptr && instruction.operandType != nullptr ) {
				std::string qualifiedTypeName = instruction.operandType->qualified.empty() == false
					? instruction.operandType->qualified
					: instruction.operandType->name;
				size_t qualGenericBracket = qualifiedTypeName.find( '<' );
				if( qualGenericBracket != std::string::npos ) {
					qualifiedTypeName = qualifiedTypeName.substr( 0, qualGenericBracket );
				}
				if( qualifiedTypeName != typeName ) {
					std::string qualConstructorName = qualifiedTypeName + "." + shortTypeName;
					if( this->functionResolutionMap.count( qualConstructorName ) > 0 ) {
						constructorFunction = this->functionResolutionMap[qualConstructorName];
						constructorName = qualConstructorName;
					}
					else {
						constructorFunction = this->llvmModule->getFunction( qualConstructorName );
						if( constructorFunction != nullptr ) {
							constructorName = qualConstructorName;
						}
					}
				}
			}
			if( constructorFunction == nullptr ) {
				std::string dotShort = "." + shortTypeName;
				for( const std::pair<const std::string, llvm::Function*>& entry : this->functionResolutionMap ) {
					if( entry.first.size() > dotShort.size() &&
						entry.first.compare( entry.first.size() - dotShort.size(), dotShort.size(), dotShort ) == 0 ) {
						size_t dotBefore = entry.first.rfind( '.', entry.first.size() - dotShort.size() - 1 );
						if( dotBefore != std::string::npos ) {
							std::string entryShortClass = entry.first.substr( dotBefore + 1,
								entry.first.size() - dotShort.size() - dotBefore - 1 );
							if( entryShortClass == shortTypeName ) {
								constructorFunction = entry.second;
								constructorName = entry.first;
								break;
							}
						}
					}
				}
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
				constructorFunction->arg_size() >= instruction.sourceOperands.size() + 1 ) {
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
					if( expectedType->isIntegerTy() && actualType->isPointerTy() ) { continue; }
					if( expectedType->isPointerTy() && actualType->isIntegerTy() ) { continue; }
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
								else if( expectedType->isIntegerTy() && argValue->getType()->isPointerTy() ) {
									argValue = this->irBuilder.CreatePtrToInt( argValue, expectedType, "arg.ptoi" );
								}
								else if( expectedType->isPointerTy() && argValue->getType()->isIntegerTy() ) {
									argValue = this->irBuilder.CreateIntToPtr( argValue, expectedType, "arg.itop" );
								}
							}
						}
						constructorArgs.push_back( argValue );
					}
				}
				while( constructorArgs.size() < constructorFunction->arg_size() ) {
					unsigned argIndex = constructorArgs.size();
					llvm::Type* expectedType = constructorFunction->getArg( argIndex )->getType();
					constructorArgs.push_back( llvm::Constant::getNullValue( expectedType ) );
				}
				this->irBuilder.CreateCall( constructorFunction, constructorArgs );
			} // argsCompatible
			} // arity match
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
					if( this->classesWithVtable.count( typeName ) > 0 || this->classesWithVtable.count( shortTypeName ) > 0 ) {
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
			case semantic::Type::Kind::None:
				return llvm::PointerType::getUnqual( this->llvmContext );
			case semantic::Type::Kind::Pointer:
			case semantic::Type::Kind::Reference:
				return llvm::PointerType::getUnqual( this->llvmContext );
			case semantic::Type::Kind::Class: {
				std::string className = semanticType->name;
				const std::string& qualifiedName = semanticType->qualified;
				if( qualifiedName == semantic::qualname::Int || qualifiedName == semantic::qualname::I64 ||
					className == semantic::qualname::classes::Int::Name || className == semantic::qualname::classes::i64::Name ) {
					return llvm::Type::getInt64Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qualname::I32 || className == semantic::qualname::classes::i32::Name ) {
					return llvm::Type::getInt32Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qualname::I16 || className == semantic::qualname::classes::i16::Name ) {
					return llvm::Type::getInt16Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qualname::I8 || className == semantic::qualname::classes::i8::Name ) {
					return llvm::Type::getInt8Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qualname::Byte || className == semantic::qualname::classes::byte::Name ) {
					return llvm::Type::getInt8Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qualname::UInt || qualifiedName == semantic::qualname::U64 ||
					className == semantic::qualname::classes::uint::Name || className == semantic::qualname::classes::u64::Name ) {
					return llvm::Type::getInt64Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qualname::U32 || className == semantic::qualname::classes::u32::Name ) {
					return llvm::Type::getInt32Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qualname::U16 || className == semantic::qualname::classes::u16::Name ) {
					return llvm::Type::getInt16Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qualname::U8 || className == semantic::qualname::classes::u8::Name ) {
					return llvm::Type::getInt8Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qualname::Long || className == semantic::qualname::classes::Long::Name ) {
					return llvm::Type::getInt64Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qualname::Integer || className == semantic::qualname::classes::integer::Name ) {
					return llvm::Type::getInt64Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qualname::F32 || className == semantic::qualname::classes::f32::Name ) {
					return llvm::Type::getFloatTy( this->llvmContext );
				}
				if( qualifiedName == semantic::qualname::Char || className == semantic::qualname::classes::Char::Name ) {
					return llvm::Type::getInt32Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qualname::F64 || qualifiedName == semantic::qualname::Double ||
					qualifiedName == semantic::qualname::Float ||
					className == semantic::qualname::classes::Float::Name || className == semantic::qualname::classes::Double::Name || className == semantic::qualname::classes::f64::Name ) {
					return llvm::Type::getDoubleTy( this->llvmContext );
				}
				if( qualifiedName == semantic::qualname::Boolean || className == semantic::qualname::classes::boolean::Name ) {
					return llvm::Type::getInt1Ty( this->llvmContext );
				}
				if( qualifiedName == semantic::qualname::String || className == semantic::qualname::classes::string::Name ) {
					return llvm::PointerType::getUnqual( this->llvmContext );
				}
				if( qualifiedName == semantic::qualname::Void || className == semantic::qualname::classes::Void::Name ) {
					return llvm::Type::getVoidTy( this->llvmContext );
				}
				if( qualifiedName == semantic::qualname::Object || className == semantic::qualname::classes::object::Name ) {
					return llvm::PointerType::getUnqual( this->llvmContext );
				}
				llvm::StructType* structType = llvm::StructType::getTypeByName(
					this->llvmContext, className
				);
				if( structType != nullptr ) {
					return llvm::PointerType::getUnqual( structType );
				}
				return llvm::PointerType::getUnqual( this->llvmContext );
			}
			case semantic::Type::Kind::Interface:
				return llvm::PointerType::getUnqual( this->llvmContext );
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
			case semantic::Type::Kind::Generator:
				return llvm::PointerType::getUnqual( this->llvmContext );
			case semantic::Type::Kind::GenericParameter:
				return llvm::PointerType::getUnqual( this->llvmContext );
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
			std::string shortElementName = elementClassName;
			size_t lastDotPos = shortElementName.rfind( '.' );
			if( lastDotPos != std::string::npos ) {
				shortElementName = shortElementName.substr( lastDotPos + 1 );
			}
			if( descriptor::Builtin::oopWrapperNames.count( shortElementName ) == 0 ) {
				if( this->structTypeCache.count( elementClassName ) > 0 ) {
					return this->structTypeCache[elementClassName];
				}
				llvm::StructType* structType = llvm::StructType::getTypeByName( this->llvmContext, elementClassName );
				if( structType != nullptr ) {
					return structType;
				}
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
	
	llvm::Function* MIRCodegen::getOrCreateStringHash() {
		llvm::Function* function = this->llvmModule->getFunction( "__uranite_string_hash" );
		if( function != nullptr ) {
			return function;
		}
		llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
		llvm::Type* i8Type = llvm::Type::getInt8Ty( this->llvmContext );
		llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
		llvm::FunctionType* hashFuncType = llvm::FunctionType::get( i64Type, { ptrType }, false );
		function = llvm::Function::Create(
			hashFuncType, llvm::Function::InternalLinkage,
			"__uranite_string_hash", this->llvmModule.get()
		);
		llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create( this->llvmContext, "entry", function );
		llvm::BasicBlock* loopHeader = llvm::BasicBlock::Create( this->llvmContext, "loop", function );
		llvm::BasicBlock* loopBody = llvm::BasicBlock::Create( this->llvmContext, "body", function );
		llvm::BasicBlock* exitBlock = llvm::BasicBlock::Create( this->llvmContext, "exit", function );
		llvm::IRBuilder<> hashBuilder( this->llvmContext );
		hashBuilder.SetInsertPoint( entryBlock );
		llvm::Value* strArg = function->getArg( 0 );
		llvm::Value* isNull = hashBuilder.CreateICmpEQ( strArg,
			llvm::ConstantPointerNull::get( llvm::cast<llvm::PointerType>( ptrType ) ), "null.chk" );
		llvm::BasicBlock* nonNullBlock = llvm::BasicBlock::Create( this->llvmContext, "nonnull", function, loopHeader );
		hashBuilder.CreateCondBr( isNull, exitBlock, nonNullBlock );
		hashBuilder.SetInsertPoint( nonNullBlock );
		hashBuilder.CreateBr( loopHeader );
		hashBuilder.SetInsertPoint( loopHeader );
		llvm::PHINode* hashPhi = hashBuilder.CreatePHI( i64Type, 2, "hash" );
		llvm::PHINode* idxPhi = hashBuilder.CreatePHI( i64Type, 2, "idx" );
		hashPhi->addIncoming( llvm::ConstantInt::get( i64Type, 5381 ), nonNullBlock );
		idxPhi->addIncoming( llvm::ConstantInt::get( i64Type, 0 ), nonNullBlock );
		llvm::Value* charPtr = hashBuilder.CreateGEP( i8Type, strArg, idxPhi, "ch.ptr" );
		llvm::Value* charVal = hashBuilder.CreateLoad( i8Type, charPtr, "ch" );
		llvm::Value* isZero = hashBuilder.CreateICmpEQ( charVal, llvm::ConstantInt::get( i8Type, 0 ), "is.zero" );
		hashBuilder.CreateCondBr( isZero, exitBlock, loopBody );
		hashBuilder.SetInsertPoint( loopBody );
		llvm::Value* charExt = hashBuilder.CreateZExt( charVal, i64Type, "ch.ext" );
		llvm::Value* shifted = hashBuilder.CreateShl( hashPhi, llvm::ConstantInt::get( i64Type, 5 ), "shl" );
		llvm::Value* combined = hashBuilder.CreateAdd( shifted, hashPhi, "combined" );
		llvm::Value* newHash = hashBuilder.CreateAdd( combined, charExt, "new.hash" );
		llvm::Value* newIdx = hashBuilder.CreateAdd( idxPhi, llvm::ConstantInt::get( i64Type, 1 ), "new.idx" );
		hashPhi->addIncoming( newHash, loopBody );
		idxPhi->addIncoming( newIdx, loopBody );
		hashBuilder.CreateBr( loopHeader );
		hashBuilder.SetInsertPoint( exitBlock );
		llvm::PHINode* resultPhi = hashBuilder.CreatePHI( i64Type, 2, "result" );
		resultPhi->addIncoming( llvm::ConstantInt::get( i64Type, 0 ), entryBlock );
		resultPhi->addIncoming( hashPhi, loopHeader );
		hashBuilder.CreateRet( resultPhi );
		return function;
	}
	
	llvm::Function* MIRCodegen::getOrCreateIntToBinStr() {
		llvm::Function* function = this->llvmModule->getFunction( "__uranite_int_to_bin" );
		if( function != nullptr ) {
			return function;
		}
		llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
		llvm::Type* i8Type = llvm::Type::getInt8Ty( this->llvmContext );
		llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
		llvm::FunctionType* funcType = llvm::FunctionType::get( ptrType, { i64Type }, false );
		function = llvm::Function::Create(
			funcType, llvm::Function::InternalLinkage,
			"__uranite_int_to_bin", this->llvmModule.get()
		);
		llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create( this->llvmContext, "entry", function );
		llvm::BasicBlock* zeroBlock = llvm::BasicBlock::Create( this->llvmContext, "zero", function );
		llvm::BasicBlock* loopBlock = llvm::BasicBlock::Create( this->llvmContext, "loop", function );
		llvm::BasicBlock* loopBody = llvm::BasicBlock::Create( this->llvmContext, "body", function );
		llvm::BasicBlock* reverseInit = llvm::BasicBlock::Create( this->llvmContext, "rev.init", function );
		llvm::BasicBlock* reverseLoop = llvm::BasicBlock::Create( this->llvmContext, "rev.loop", function );
		llvm::BasicBlock* reverseBody = llvm::BasicBlock::Create( this->llvmContext, "rev.body", function );
		llvm::BasicBlock* doneBlock = llvm::BasicBlock::Create( this->llvmContext, "done", function );
		llvm::IRBuilder<> binBuilder( this->llvmContext );
		llvm::Function* mallocFunction = this->getOrCreateMalloc();
		binBuilder.SetInsertPoint( entryBlock );
		llvm::Value* inputValue = function->getArg( 0 );
		llvm::Value* tempBuf = binBuilder.CreateCall( mallocFunction, { llvm::ConstantInt::get( i64Type, 65 ) }, "tmp" );
		llvm::Value* resultBuf = binBuilder.CreateCall( mallocFunction, { llvm::ConstantInt::get( i64Type, 65 ) }, "res" );
		llvm::Value* isZero = binBuilder.CreateICmpEQ( inputValue, llvm::ConstantInt::get( i64Type, 0 ), "is.zero" );
		binBuilder.CreateCondBr( isZero, zeroBlock, loopBlock );
		binBuilder.SetInsertPoint( zeroBlock );
		binBuilder.CreateStore( llvm::ConstantInt::get( i8Type, '0' ), resultBuf );
		llvm::Value* zeroTermPos = binBuilder.CreateGEP( i8Type, resultBuf, llvm::ConstantInt::get( i64Type, 1 ), "zt" );
		binBuilder.CreateStore( llvm::ConstantInt::get( i8Type, 0 ), zeroTermPos );
		binBuilder.CreateRet( resultBuf );
		binBuilder.SetInsertPoint( loopBlock );
		llvm::PHINode* valPhi = binBuilder.CreatePHI( i64Type, 2, "val" );
		llvm::PHINode* posPhi = binBuilder.CreatePHI( i64Type, 2, "pos" );
		valPhi->addIncoming( inputValue, entryBlock );
		posPhi->addIncoming( llvm::ConstantInt::get( i64Type, 0 ), entryBlock );
		llvm::Value* isDone = binBuilder.CreateICmpEQ( valPhi, llvm::ConstantInt::get( i64Type, 0 ), "done" );
		binBuilder.CreateCondBr( isDone, reverseInit, loopBody );
		binBuilder.SetInsertPoint( loopBody );
		llvm::Value* remainder = binBuilder.CreateURem( valPhi, llvm::ConstantInt::get( i64Type, 2 ), "rem" );
		llvm::Value* digit = binBuilder.CreateAdd( remainder, llvm::ConstantInt::get( i64Type, '0' ), "dig" );
		llvm::Value* digitByte = binBuilder.CreateTrunc( digit, i8Type, "dig.b" );
		llvm::Value* writePtr = binBuilder.CreateGEP( i8Type, tempBuf, posPhi, "wr" );
		binBuilder.CreateStore( digitByte, writePtr );
		llvm::Value* nextVal = binBuilder.CreateUDiv( valPhi, llvm::ConstantInt::get( i64Type, 2 ), "next" );
		llvm::Value* nextPos = binBuilder.CreateAdd( posPhi, llvm::ConstantInt::get( i64Type, 1 ), "npos" );
		valPhi->addIncoming( nextVal, loopBody );
		posPhi->addIncoming( nextPos, loopBody );
		binBuilder.CreateBr( loopBlock );
		binBuilder.SetInsertPoint( reverseInit );
		llvm::Value* totalLen = posPhi;
		binBuilder.CreateBr( reverseLoop );
		binBuilder.SetInsertPoint( reverseLoop );
		llvm::PHINode* revIdx = binBuilder.CreatePHI( i64Type, 2, "ri" );
		revIdx->addIncoming( llvm::ConstantInt::get( i64Type, 0 ), reverseInit );
		llvm::Value* revDone = binBuilder.CreateICmpUGE( revIdx, totalLen, "rev.done" );
		binBuilder.CreateCondBr( revDone, doneBlock, reverseBody );
		binBuilder.SetInsertPoint( reverseBody );
		llvm::Value* srcIdx = binBuilder.CreateSub( totalLen, binBuilder.CreateAdd( revIdx, llvm::ConstantInt::get( i64Type, 1 ) ), "si" );
		llvm::Value* srcPtr = binBuilder.CreateGEP( i8Type, tempBuf, srcIdx, "sp" );
		llvm::Value* srcByte = binBuilder.CreateLoad( i8Type, srcPtr, "sb" );
		llvm::Value* dstPtr = binBuilder.CreateGEP( i8Type, resultBuf, revIdx, "dp" );
		binBuilder.CreateStore( srcByte, dstPtr );
		llvm::Value* nextRevIdx = binBuilder.CreateAdd( revIdx, llvm::ConstantInt::get( i64Type, 1 ), "nri" );
		revIdx->addIncoming( nextRevIdx, reverseBody );
		binBuilder.CreateBr( reverseLoop );
		binBuilder.SetInsertPoint( doneBlock );
		llvm::Value* termPtr = binBuilder.CreateGEP( i8Type, resultBuf, totalLen, "term" );
		binBuilder.CreateStore( llvm::ConstantInt::get( i8Type, 0 ), termPtr );
		binBuilder.CreateRet( resultBuf );
		return function;
	}
	
	llvm::Function* MIRCodegen::getOrCreateStrCenter() {
		llvm::Function* function = this->llvmModule->getFunction( "__uranite_str_center" );
		if( function != nullptr ) {
			return function;
		}
		llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
		llvm::Type* i8Type = llvm::Type::getInt8Ty( this->llvmContext );
		llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
		llvm::FunctionType* funcType = llvm::FunctionType::get( ptrType, { ptrType, i64Type, i8Type }, false );
		function = llvm::Function::Create(
			funcType, llvm::Function::InternalLinkage,
			"__uranite_str_center", this->llvmModule.get()
		);
		llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create( this->llvmContext, "entry", function );
		llvm::BasicBlock* shortBlock = llvm::BasicBlock::Create( this->llvmContext, "short", function );
		llvm::BasicBlock* padBlock = llvm::BasicBlock::Create( this->llvmContext, "pad", function );
		llvm::BasicBlock* fillLoop = llvm::BasicBlock::Create( this->llvmContext, "fill", function );
		llvm::BasicBlock* fillBody = llvm::BasicBlock::Create( this->llvmContext, "fill.body", function );
		llvm::BasicBlock* copyLoop = llvm::BasicBlock::Create( this->llvmContext, "copy", function );
		llvm::BasicBlock* copyBody = llvm::BasicBlock::Create( this->llvmContext, "copy.body", function );
		llvm::BasicBlock* doneBlock = llvm::BasicBlock::Create( this->llvmContext, "done", function );
		llvm::IRBuilder<> centerBuilder( this->llvmContext );
		llvm::Function* mallocFunction = this->getOrCreateMalloc();
		llvm::Function* strlenFunction = this->getOrCreateStrlen();
		centerBuilder.SetInsertPoint( entryBlock );
		llvm::Value* inputStr = function->getArg( 0 );
		llvm::Value* targetWidth = function->getArg( 1 );
		llvm::Value* fillChar = function->getArg( 2 );
		llvm::Value* strLen = centerBuilder.CreateCall( strlenFunction, { inputStr }, "len" );
		llvm::Value* needsPad = centerBuilder.CreateICmpULT( strLen, targetWidth, "needs.pad" );
		centerBuilder.CreateCondBr( needsPad, padBlock, shortBlock );
		centerBuilder.SetInsertPoint( shortBlock );
		centerBuilder.CreateRet( inputStr );
		centerBuilder.SetInsertPoint( padBlock );
		llvm::Value* bufSize = centerBuilder.CreateAdd( targetWidth, llvm::ConstantInt::get( i64Type, 1 ), "buf.sz" );
		llvm::Value* resultBuf = centerBuilder.CreateCall( mallocFunction, { bufSize }, "res" );
		llvm::Value* padTotal = centerBuilder.CreateSub( targetWidth, strLen, "pad.total" );
		llvm::Value* leftPad = centerBuilder.CreateUDiv( padTotal, llvm::ConstantInt::get( i64Type, 2 ), "left.pad" );
		centerBuilder.CreateBr( fillLoop );
		centerBuilder.SetInsertPoint( fillLoop );
		llvm::PHINode* fillIdx = centerBuilder.CreatePHI( i64Type, 2, "fi" );
		fillIdx->addIncoming( llvm::ConstantInt::get( i64Type, 0 ), padBlock );
		llvm::Value* fillDone = centerBuilder.CreateICmpUGE( fillIdx, targetWidth, "fill.done" );
		centerBuilder.CreateCondBr( fillDone, copyLoop, fillBody );
		centerBuilder.SetInsertPoint( fillBody );
		llvm::Value* fillPtr = centerBuilder.CreateGEP( i8Type, resultBuf, fillIdx, "fp" );
		centerBuilder.CreateStore( fillChar, fillPtr );
		llvm::Value* nextFillIdx = centerBuilder.CreateAdd( fillIdx, llvm::ConstantInt::get( i64Type, 1 ), "nfi" );
		fillIdx->addIncoming( nextFillIdx, fillBody );
		centerBuilder.CreateBr( fillLoop );
		centerBuilder.SetInsertPoint( copyLoop );
		llvm::PHINode* copyIdx = centerBuilder.CreatePHI( i64Type, 2, "ci" );
		copyIdx->addIncoming( llvm::ConstantInt::get( i64Type, 0 ), fillLoop );
		llvm::Value* copyDone = centerBuilder.CreateICmpUGE( copyIdx, strLen, "copy.done" );
		centerBuilder.CreateCondBr( copyDone, doneBlock, copyBody );
		centerBuilder.SetInsertPoint( copyBody );
		llvm::Value* srcPtr = centerBuilder.CreateGEP( i8Type, inputStr, copyIdx, "sp" );
		llvm::Value* srcByte = centerBuilder.CreateLoad( i8Type, srcPtr, "sb" );
		llvm::Value* dstOffset = centerBuilder.CreateAdd( leftPad, copyIdx, "do" );
		llvm::Value* dstPtr = centerBuilder.CreateGEP( i8Type, resultBuf, dstOffset, "dp" );
		centerBuilder.CreateStore( srcByte, dstPtr );
		llvm::Value* nextCopyIdx = centerBuilder.CreateAdd( copyIdx, llvm::ConstantInt::get( i64Type, 1 ), "nci" );
		copyIdx->addIncoming( nextCopyIdx, copyBody );
		centerBuilder.CreateBr( copyLoop );
		centerBuilder.SetInsertPoint( doneBlock );
		llvm::Value* termPtr = centerBuilder.CreateGEP( i8Type, resultBuf, targetWidth, "term" );
		centerBuilder.CreateStore( llvm::ConstantInt::get( i8Type, 0 ), termPtr );
		centerBuilder.CreateRet( resultBuf );
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
	
	llvm::Function* MIRCodegen::getOrCreatePushFrame() {
		codegen::RuntimeFunctionSpec spec = this->runtimeInterface_->getPushFrameFunction( this->llvmContext );
		llvm::Function* function = this->llvmModule->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType, spec.functionName, this->llvmModule.get()
			);
		}
		return function;
	}
	
	llvm::Function* MIRCodegen::getOrCreatePopFrame() {
		codegen::RuntimeFunctionSpec spec = this->runtimeInterface_->getPopFrameFunction( this->llvmContext );
		llvm::Function* function = this->llvmModule->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType, spec.functionName, this->llvmModule.get()
			);
		}
		return function;
	}
	
	void MIRCodegen::emitPushFrame( const std::string& file, int64_t line, int64_t column, const std::string& functionName ) {
		llvm::Function* pushFrame = this->getOrCreatePushFrame();
		llvm::Value* fileStr = this->irBuilder.CreateGlobalStringPtr( file, "frame.file" );
		llvm::Value* lineVal = llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->llvmContext ), line );
		llvm::Value* colVal = llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->llvmContext ), column );
		llvm::Value* funcStr = this->irBuilder.CreateGlobalStringPtr( functionName, "frame.func" );
		this->irBuilder.CreateCall( pushFrame, { fileStr, lineVal, colVal, funcStr } );
	}
	
	void MIRCodegen::emitPopFrame() {
		llvm::Function* popFrame = this->getOrCreatePopFrame();
		this->irBuilder.CreateCall( popFrame, {} );
	}
	
	llvm::Value* MIRCodegen::emitCallOrInvoke( const MIRInstruction& instruction, llvm::Function* callee, std::vector<llvm::Value*>& arguments ) {
		if( instruction.instructionKind == MIRInstructionKind::InvokeFunction &&
			this->blockMap.count( instruction.trueBranchTarget ) > 0 &&
			this->blockMap.count( instruction.landingPadTarget ) > 0 ) {
			llvm::Function* enclosingFunction = this->irBuilder.GetInsertBlock()->getParent();
			if( enclosingFunction->hasPersonalityFn() == false ) {
				enclosingFunction->setPersonalityFn( this->getOrCreatePersonality() );
			}
			llvm::BasicBlock* normalDest = this->blockMap[instruction.trueBranchTarget];
			llvm::BasicBlock* unwindDest = this->blockMap[instruction.landingPadTarget];
			llvm::InvokeInst* invokeResult = this->irBuilder.CreateInvoke( callee, normalDest, unwindDest, arguments );
			this->irBuilder.SetInsertPoint( normalDest );
			return invokeResult;
		}
		if( this->asyncWrapperCatchBlock != nullptr ) {
			llvm::Function* enclosingFunction = this->irBuilder.GetInsertBlock()->getParent();
			if( enclosingFunction->hasPersonalityFn() == false ) {
				enclosingFunction->setPersonalityFn( this->getOrCreatePersonality() );
			}
			llvm::BasicBlock* contBlock = llvm::BasicBlock::Create(
				this->llvmContext, "async.cont", enclosingFunction
			);
			llvm::InvokeInst* invokeResult = this->irBuilder.CreateInvoke(
				callee, contBlock, this->asyncWrapperCatchBlock, arguments
			);
			this->irBuilder.SetInsertPoint( contBlock );
			return invokeResult;
		}
		return this->irBuilder.CreateCall( callee, arguments );
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
	
	bool MIRCodegen::tryBuiltinDescriptor( const std::string& typeName, const std::string& methodName, const MIRInstruction& instruction, MIRFunctionDefinition& functionDefinition ) {
		if( instruction.sourceOperands.empty() ) {
			return false;
		}
		MIRVariableIdentifier receiverIdentifier = instruction.sourceOperands[0];
		if( this->currentMIRFunction != nullptr ) {
			std::unordered_map<MIRVariableIdentifier, MIRVariableDescriptor>::iterator descriptorIterator =
				this->currentMIRFunction->variableDescriptorTable.find( receiverIdentifier );
			if( descriptorIterator != this->currentMIRFunction->variableDescriptorTable.end() ) {
				MIRVariableDescriptor& variableDescriptor = descriptorIterator->second;
				if( variableDescriptor.variableType != nullptr ) {
					std::string qualifiedName = variableDescriptor.variableType->qualified;
					if( qualifiedName.empty() == false ) {
						if( semantic::qualname::isOopWrapper( qualifiedName ) == false ) {
							return false;
						}
					}
				}
			}
		}
		const descriptor::BuiltinMethodDescriptor* methodDescriptor =
			this->builtinRegistry.lookupMethod( typeName, methodName );
		if( methodDescriptor == nullptr ) {
			return false;
		}
		llvm::Type* expectedSelfType = this->builtinRegistry.getLLVMType( typeName, this->llvmContext );
		llvm::Value* selfValue = this->loadVariableValue( instruction.sourceOperands[0] );
		if( selfValue == nullptr ) {
			return false;
		}
		if( expectedSelfType != nullptr && selfValue->getType() != expectedSelfType ) {
			if( expectedSelfType->isIntegerTy() && selfValue->getType()->isPointerTy() ) {
				selfValue = this->irBuilder.CreatePtrToInt( selfValue, expectedSelfType, "self.ptoi" );
			}
			else if( expectedSelfType->isIntegerTy() && selfValue->getType()->isIntegerTy() ) {
				selfValue = this->irBuilder.CreateIntCast( selfValue, expectedSelfType, true, "self.icast" );
			}
			else if( expectedSelfType->isDoubleTy() && selfValue->getType()->isIntegerTy() ) {
				selfValue = this->irBuilder.CreateSIToFP( selfValue, expectedSelfType, "self.itof" );
			}
			else if( expectedSelfType->isPointerTy() && selfValue->getType()->isIntegerTy() ) {
				selfValue = this->irBuilder.CreateIntToPtr( selfValue, expectedSelfType, "self.itop" );
			}
		}
		std::vector<llvm::Value*> arguments;
		bool isFormatMethod = ( methodName == semantic::qualname::classes::string::methods::Format );
		for( size_t operandIndex = 1; operandIndex < instruction.sourceOperands.size(); operandIndex++ ) {
			llvm::Value* argumentValue = this->loadVariableValue( instruction.sourceOperands[operandIndex] );
			if( argumentValue != nullptr ) {
				if( isFormatMethod && argumentValue->getType()->isPointerTy() && this->currentMIRFunction != nullptr ) {
					std::unordered_map<MIRVariableIdentifier, MIRVariableDescriptor>::iterator argDescriptorIterator =
						this->currentMIRFunction->variableDescriptorTable.find( instruction.sourceOperands[operandIndex] );
					if( argDescriptorIterator != this->currentMIRFunction->variableDescriptorTable.end() &&
						argDescriptorIterator->second.variableType != nullptr &&
						argDescriptorIterator->second.variableType->kind == semantic::Type::Kind::Class &&
						argDescriptorIterator->second.variableType->name != semantic::qualname::classes::string::Name ) {
						std::string argQualifiedName = argDescriptorIterator->second.variableType->qualified;
						if( semantic::qualname::isOopWrapper( argQualifiedName ) ) {
							llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
							std::string argShortName = argDescriptorIterator->second.variableType->name;
							if( argShortName == semantic::qualname::classes::boolean::Name ) {
								llvm::Value* argIntValue = this->irBuilder.CreatePtrToInt( argumentValue, i64Type, "fmt.bool.ptoi" );
								argumentValue = this->irBuilder.CreateTrunc( argIntValue, llvm::Type::getInt1Ty( this->llvmContext ), "fmt.bool.trunc" );
							}
							else if( descriptor::Builtin::floatOopNames.count( argShortName ) > 0 ) {
								llvm::Value* argIntValue = this->irBuilder.CreatePtrToInt( argumentValue, i64Type, "fmt.flt.ptoi" );
								argumentValue = this->irBuilder.CreateBitCast( argIntValue, llvm::Type::getDoubleTy( this->llvmContext ), "fmt.flt.cast" );
							}
							else {
								argumentValue = this->irBuilder.CreatePtrToInt( argumentValue, i64Type, "fmt.int.ptoi" );
							}
						}
						else {
							std::string argClassQualified = argQualifiedName;
							if( argClassQualified.empty() ) {
								argClassQualified = argDescriptorIterator->second.variableType->name;
							}
							size_t argGenericPosition = argClassQualified.find( '<' );
							if( argGenericPosition != std::string::npos ) {
								argClassQualified = argClassQualified.substr( 0, argGenericPosition );
							}
							std::string argToStringName = argClassQualified + "." + semantic::qualname::classes::object::methods::ToString;
							llvm::Function* argToStringFunction = nullptr;
							std::unordered_map<std::string, llvm::Function*>::iterator toStringResolutionIterator = this->functionResolutionMap.find( argToStringName );
							if( toStringResolutionIterator != this->functionResolutionMap.end() ) {
								argToStringFunction = toStringResolutionIterator->second;
							}
							else {
								argToStringFunction = this->llvmModule->getFunction( argToStringName );
							}
							if( argToStringFunction != nullptr ) {
								argumentValue = this->irBuilder.CreateCall( argToStringFunction, { argumentValue }, "arg.tostr" );
							}
						}
					}
				}
				arguments.push_back( argumentValue );
			}
		}
		std::vector<std::pair<std::string, llvm::Value*>> keywordArguments;
		for( size_t keywordIndex = 0; keywordIndex < instruction.keywordArgumentKeys.size(); keywordIndex++ ) {
			llvm::Value* keywordValue = this->loadVariableValue( instruction.keywordArgumentValues[keywordIndex] );
			if( keywordValue != nullptr ) {
				if( isFormatMethod && keywordValue->getType()->isPointerTy() && this->currentMIRFunction != nullptr ) {
					std::unordered_map<MIRVariableIdentifier, MIRVariableDescriptor>::iterator kwDescriptorIterator =
						this->currentMIRFunction->variableDescriptorTable.find( instruction.keywordArgumentValues[keywordIndex] );
					if( kwDescriptorIterator != this->currentMIRFunction->variableDescriptorTable.end() &&
						kwDescriptorIterator->second.variableType != nullptr &&
						kwDescriptorIterator->second.variableType->kind == semantic::Type::Kind::Class &&
						kwDescriptorIterator->second.variableType->name != semantic::qualname::classes::string::Name ) {
						std::string kwQualifiedName = kwDescriptorIterator->second.variableType->qualified;
						if( semantic::qualname::isOopWrapper( kwQualifiedName ) ) {
							llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
							std::string kwShortName = kwDescriptorIterator->second.variableType->name;
							if( kwShortName == semantic::qualname::classes::boolean::Name ) {
								llvm::Value* kwIntValue = this->irBuilder.CreatePtrToInt( keywordValue, i64Type, "fmt.kw.bool.ptoi" );
								keywordValue = this->irBuilder.CreateTrunc( kwIntValue, llvm::Type::getInt1Ty( this->llvmContext ), "fmt.kw.bool.trunc" );
							}
							else if( descriptor::Builtin::floatOopNames.count( kwShortName ) > 0 ) {
								llvm::Value* kwIntValue = this->irBuilder.CreatePtrToInt( keywordValue, i64Type, "fmt.kw.flt.ptoi" );
								keywordValue = this->irBuilder.CreateBitCast( kwIntValue, llvm::Type::getDoubleTy( this->llvmContext ), "fmt.kw.flt.cast" );
							}
							else {
								keywordValue = this->irBuilder.CreatePtrToInt( keywordValue, i64Type, "fmt.kw.int.ptoi" );
							}
						}
						else {
							std::string kwClassQualified = kwQualifiedName;
							if( kwClassQualified.empty() ) {
								kwClassQualified = kwDescriptorIterator->second.variableType->name;
							}
							size_t kwGenericPosition = kwClassQualified.find( '<' );
							if( kwGenericPosition != std::string::npos ) {
								kwClassQualified = kwClassQualified.substr( 0, kwGenericPosition );
							}
							std::string kwToStringName = kwClassQualified + "." + semantic::qualname::classes::object::methods::ToString;
							llvm::Function* kwToStringFunction = nullptr;
							std::unordered_map<std::string, llvm::Function*>::iterator kwToStringResolutionIterator = this->functionResolutionMap.find( kwToStringName );
							if( kwToStringResolutionIterator != this->functionResolutionMap.end() ) {
								kwToStringFunction = kwToStringResolutionIterator->second;
							}
							else {
								kwToStringFunction = this->llvmModule->getFunction( kwToStringName );
							}
							if( kwToStringFunction != nullptr ) {
								keywordValue = this->irBuilder.CreateCall( kwToStringFunction, { keywordValue }, "kw.tostr" );
							}
						}
					}
				}
				keywordArguments.push_back( { instruction.keywordArgumentKeys[keywordIndex], keywordValue } );
			}
		}
		llvm::Value* result = methodDescriptor->irGenerator(
			this->irBuilder, this->llvmContext, selfValue, arguments, keywordArguments
		);
		if( result != nullptr && instruction.destinationVariable != INVALID_VARIABLE_IDENTIFIER ) {
			llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
			if( result->getType()->isIntegerTy() && result->getType() != i64Type &&
				result->getType()->isIntegerTy( 1 ) == false ) {
				bool useZeroExtend = descriptor::Builtin::unsignedOopNames.count( typeName ) > 0 &&
					expectedSelfType != nullptr && result->getType() == expectedSelfType;
				if( useZeroExtend ) {
					result = this->irBuilder.CreateZExt( result, i64Type, "desc.zext" );
				}
				else {
					result = this->irBuilder.CreateSExt( result, i64Type, "desc.sext" );
				}
			}
			this->setVariableValue( instruction.destinationVariable, result );
		}
		return true;
	}
	
	void MIRCodegen::generateYield( const MIRInstruction& instruction ) {
		if( this->currentGeneratorContext == nullptr ) {
			return;
		}
		GeneratorContext& genCtx = *this->currentGeneratorContext;
		llvm::LLVMContext& ctx = this->llvmContext;
		llvm::Value* yieldedValue = nullptr;
		if( instruction.sourceOperands.empty() == false ) {
			yieldedValue = this->loadVariableValue( instruction.sourceOperands[0] );
		}
		if( yieldedValue != nullptr && genCtx.valueVar != nullptr ) {
			llvm::Type* valueType = genCtx.valueVar->getAllocatedType();
			if( yieldedValue->getType() != valueType ) {
				if( valueType->isIntegerTy() && yieldedValue->getType()->isIntegerTy() ) {
					yieldedValue = this->irBuilder.CreateIntCast( yieldedValue, valueType, true, "yield.cast" );
				}
				else if( valueType->isDoubleTy() && yieldedValue->getType()->isIntegerTy() ) {
					yieldedValue = this->irBuilder.CreateSIToFP( yieldedValue, valueType, "yield.cast" );
				}
			}
			this->irBuilder.CreateStore( yieldedValue, genCtx.valueVar );
		}
		this->irBuilder.CreateStore( llvm::ConstantInt::getFalse( ctx ), genCtx.doneVar );
		int stateId = genCtx.nextStateId++;
		this->irBuilder.CreateStore(
			llvm::ConstantInt::get( llvm::Type::getInt32Ty( ctx ), stateId ),
			genCtx.stateVar
		);
		this->irBuilder.CreateBr( genCtx.exitBlock );
		llvm::Function* currentFunction = this->irBuilder.GetInsertBlock()->getParent();
		llvm::BasicBlock* resumeBlock = llvm::BasicBlock::Create(
			ctx, fmt::format( "resume.{}", stateId ), currentFunction
		);
		this->irBuilder.SetInsertPoint( resumeBlock );
	}
	
	void MIRCodegen::generateGeneratorFunction( MIRFunctionDefinition& functionDefinition, const std::string& llvmFunctionName, llvm::Function* llvmFunction ) {
		llvm::LLVMContext& ctx = this->llvmContext;
		llvm::Type* i32Type = llvm::Type::getInt32Ty( ctx );
		llvm::Type* i1Type = llvm::Type::getInt1Ty( ctx );
		llvm::Type* i64Type = llvm::Type::getInt64Ty( ctx );
		llvm::Type* ptrType = llvm::PointerType::getUnqual( ctx );
		llvm::Type* yieldType = i64Type;
		if( functionDefinition.generatorYieldType != nullptr ) {
			yieldType = this->toLLVMType( functionDefinition.generatorYieldType );
		}
		if( yieldType->isVoidTy() ) {
			yieldType = i64Type;
		}
		std::vector<llvm::Type*> paramLLVMTypes;
		std::vector<std::string> paramNames;
		std::vector<MIRVariableIdentifier> paramVarIds;
		for( MIRVariableIdentifier paramVar : functionDefinition.parameterVariableIdentifiers ) {
			llvm::Type* paramType = i64Type;
			std::string paramName = "param";
			if( functionDefinition.variableDescriptorTable.count( paramVar ) > 0 ) {
				MIRVariableDescriptor& descriptor = functionDefinition.variableDescriptorTable[paramVar];
				paramName = descriptor.variableName;
				if( descriptor.variableType != nullptr ) {
					paramType = this->toLLVMType( descriptor.variableType );
				}
			}
			if( paramType->isVoidTy() ) {
				paramType = i64Type;
			}
			paramLLVMTypes.push_back( paramType );
			paramNames.push_back( paramName );
			paramVarIds.push_back( paramVar );
		}
		std::vector<llvm::Type*> structFields = { i32Type, yieldType, i1Type };
		for( llvm::Type* paramType : paramLLVMTypes ) {
			structFields.push_back( paramType );
		}
		std::string structName = fmt::format( "__gen_{}", llvmFunctionName );
		llvm::StructType* genStructType = llvm::StructType::create( ctx, structFields, structName );
		this->structTypeCache[structName] = genStructType;
		if( functionDefinition.functionName != llvmFunctionName ) {
			std::string shortStructName = fmt::format( "__gen_{}", functionDefinition.functionName );
			this->structTypeCache[shortStructName] = genStructType;
		}
		std::string nextFuncName = fmt::format( "{}.next", llvmFunctionName );
		llvm::FunctionType* nextFuncType = llvm::FunctionType::get( llvm::Type::getVoidTy( ctx ), { ptrType }, false );
		llvm::Function* nextFunction = llvm::Function::Create(
			nextFuncType, llvm::Function::InternalLinkage,
			nextFuncName, this->llvmModule.get()
		);
		nextFunction->setPersonalityFn( this->getOrCreatePersonality() );
		this->functionResolutionMap[nextFuncName] = nextFunction;
		if( functionDefinition.functionName != llvmFunctionName ) {
			std::string shortNextFuncName = fmt::format( "{}.next", functionDefinition.functionName );
			this->functionResolutionMap[shortNextFuncName] = nextFunction;
		}
		std::unordered_map<MIRVariableIdentifier, llvm::Value*> savedVariableMap = this->variableValueMap;
		std::unordered_map<MIRBlockIdentifier, llvm::BasicBlock*> savedBlockMap = this->blockMap;
		this->variableValueMap.clear();
		this->blockMap.clear();
		llvm::BasicBlock* nextEntry = llvm::BasicBlock::Create( ctx, "entry", nextFunction );
		llvm::BasicBlock* doneBlock = llvm::BasicBlock::Create( ctx, "done", nextFunction );
		llvm::BasicBlock* exitBlock = llvm::BasicBlock::Create( ctx, "exit", nextFunction );
		this->irBuilder.SetInsertPoint( nextEntry );
		llvm::Argument* genArgument = nextFunction->arg_begin();
		genArgument->setName( "gen" );
		llvm::Value* statePtr = this->irBuilder.CreateStructGEP( genStructType, genArgument, 0, "state.ptr" );
		llvm::Value* valuePtr = this->irBuilder.CreateStructGEP( genStructType, genArgument, 1, "value.ptr" );
		llvm::Value* donePtr = this->irBuilder.CreateStructGEP( genStructType, genArgument, 2, "done.ptr" );
		GeneratorContext genCtx;
		genCtx.stateVar = this->createEntryBlockAllocation( nextFunction, "state.local", i32Type );
		genCtx.valueVar = this->createEntryBlockAllocation( nextFunction, "value.local", yieldType );
		genCtx.doneVar = this->createEntryBlockAllocation( nextFunction, "done.local", i1Type );
		genCtx.exitBlock = exitBlock;
		genCtx.structType = genStructType;
		genCtx.structPointer = genArgument;
		genCtx.nextStateId = 1;
		genCtx.prefix = llvmFunctionName;
		llvm::Value* loadedState = this->irBuilder.CreateLoad( i32Type, statePtr, "state" );
		this->irBuilder.CreateStore( loadedState, genCtx.stateVar );
		this->irBuilder.CreateStore( llvm::ConstantInt::getFalse( ctx ), genCtx.doneVar );
		for( size_t paramIndex = 0; paramIndex < paramVarIds.size(); paramIndex++ ) {
			std::string paramPtrName = fmt::format( "{}.ptr", paramNames[paramIndex] );
			llvm::Value* paramFieldPtr = this->irBuilder.CreateStructGEP(
				genStructType, genArgument, 3 + paramIndex, paramPtrName
			);
			llvm::AllocaInst* paramAlloca = this->createEntryBlockAllocation(
				nextFunction, paramNames[paramIndex], paramLLVMTypes[paramIndex]
			);
			llvm::Value* paramValue = this->irBuilder.CreateLoad(
				paramLLVMTypes[paramIndex], paramFieldPtr, paramNames[paramIndex]
			);
			this->irBuilder.CreateStore( paramValue, paramAlloca );
			this->variableValueMap[paramVarIds[paramIndex]] = paramAlloca;
			genCtx.parameterVariables.insert( paramVarIds[paramIndex] );
		}
		for( std::shared_ptr<MIRBasicBlock>& mirBlock : functionDefinition.controlFlowBlocks ) {
			if( mirBlock != nullptr ) {
				llvm::BasicBlock* llvmBlock = llvm::BasicBlock::Create(
					ctx, mirBlock->blockLabel, nextFunction
				);
				this->blockMap[mirBlock->blockIdentifier] = llvmBlock;
			}
		}
		llvm::BasicBlock* bodyStart = this->blockMap[functionDefinition.entryBlockIdentifier];
		llvm::SwitchInst* dispatchSwitch = this->irBuilder.CreateSwitch( loadedState, doneBlock, 16 );
		dispatchSwitch->addCase( llvm::ConstantInt::get( llvm::cast<llvm::IntegerType>( i32Type ), 0 ), bodyStart );
		genCtx.dispatchSwitch = dispatchSwitch;
		this->irBuilder.SetInsertPoint( doneBlock );
		this->irBuilder.CreateStore( llvm::ConstantInt::getTrue( ctx ), genCtx.doneVar );
		this->irBuilder.CreateBr( exitBlock );
		GeneratorContext* savedGenCtx = this->currentGeneratorContext;
		this->currentGeneratorContext = &genCtx;
		this->irBuilder.SetInsertPoint( bodyStart );
		for( std::shared_ptr<MIRBasicBlock>& mirBlock : functionDefinition.controlFlowBlocks ) {
			if( mirBlock != nullptr ) {
				this->generateBasicBlock( *mirBlock, functionDefinition );
			}
		}
		for( llvm::BasicBlock& llvmBlock : *nextFunction ) {
			if( llvmBlock.getTerminator() == nullptr &&
				&llvmBlock != exitBlock ) {
				this->irBuilder.SetInsertPoint( &llvmBlock );
				this->irBuilder.CreateStore( llvm::ConstantInt::getTrue( ctx ), genCtx.doneVar );
				this->irBuilder.CreateBr( exitBlock );
			}
		}
		std::vector<std::pair<MIRVariableIdentifier, llvm::AllocaInst*>> localAllocas;
		for( std::pair<const MIRVariableIdentifier, llvm::Value*>& entry : this->variableValueMap ) {
			if( genCtx.parameterVariables.count( entry.first ) > 0 ) {
				continue;
			}
			llvm::AllocaInst* alloca = llvm::dyn_cast<llvm::AllocaInst>( entry.second );
			if( alloca != nullptr && alloca != genCtx.stateVar &&
				alloca != genCtx.valueVar && alloca != genCtx.doneVar ) {
				localAllocas.push_back( { entry.first, alloca } );
			}
		}
		for( std::pair<MIRVariableIdentifier, llvm::AllocaInst*>& localEntry : localAllocas ) {
			std::string globalName = fmt::format( "{}.local.v{}", genCtx.prefix, localEntry.first );
			llvm::AllocaInst* alloca = localEntry.second;
			llvm::GlobalVariable* globalVar = new llvm::GlobalVariable(
				*this->llvmModule, alloca->getAllocatedType(), false,
				llvm::GlobalValue::InternalLinkage,
				llvm::Constant::getNullValue( alloca->getAllocatedType() ),
				globalName
			);
			genCtx.persistedLocals[globalName] = globalVar;
		}
		for( llvm::BasicBlock& llvmBlock : *nextFunction ) {
			llvm::StringRef blockName = llvmBlock.getName();
			if( blockName.starts_with( "resume." ) ) {
				std::string stateIdStr = blockName.substr( 7 ).str();
				int stateId = std::stoi( stateIdStr );
				std::string restoreName = fmt::format( "restore.{}", stateId );
				llvm::BasicBlock* restoreBlock = llvm::BasicBlock::Create(
					ctx, restoreName, nextFunction
				);
				dispatchSwitch->addCase(
					llvm::ConstantInt::get( llvm::cast<llvm::IntegerType>( i32Type ), stateId ), restoreBlock
				);
				this->irBuilder.SetInsertPoint( restoreBlock );
				for( std::pair<MIRVariableIdentifier, llvm::AllocaInst*>& localEntry : localAllocas ) {
					std::string globalName = fmt::format( "{}.local.v{}", genCtx.prefix, localEntry.first );
					llvm::GlobalVariable* globalVar = genCtx.persistedLocals[globalName];
					llvm::AllocaInst* alloca = localEntry.second;
					llvm::Value* restored = this->irBuilder.CreateLoad(
						alloca->getAllocatedType(), globalVar,
						fmt::format( "restore.v{}", localEntry.first )
					);
					this->irBuilder.CreateStore( restored, alloca );
				}
				for( size_t paramIndex = 0; paramIndex < paramVarIds.size(); paramIndex++ ) {
					llvm::Value* paramFieldPtr = this->irBuilder.CreateStructGEP(
						genStructType, genArgument, 3 + paramIndex,
						fmt::format( "restore.{}", paramNames[paramIndex] )
					);
					llvm::Value* paramValue = this->irBuilder.CreateLoad(
						paramLLVMTypes[paramIndex], paramFieldPtr,
						fmt::format( "reload.{}", paramNames[paramIndex] )
					);
					llvm::AllocaInst* paramAlloca = llvm::dyn_cast<llvm::AllocaInst>(
						this->variableValueMap[paramVarIds[paramIndex]]
					);
					if( paramAlloca != nullptr ) {
						this->irBuilder.CreateStore( paramValue, paramAlloca );
					}
				}
				this->irBuilder.CreateBr( &llvmBlock );
			}
		}
		this->irBuilder.SetInsertPoint( exitBlock );
		for( std::pair<MIRVariableIdentifier, llvm::AllocaInst*>& localEntry : localAllocas ) {
			std::string globalName = fmt::format( "{}.local.v{}", genCtx.prefix, localEntry.first );
			llvm::GlobalVariable* globalVar = genCtx.persistedLocals[globalName];
			llvm::AllocaInst* alloca = localEntry.second;
			llvm::Value* savedValue = this->irBuilder.CreateLoad(
				alloca->getAllocatedType(), alloca,
				fmt::format( "save.v{}", localEntry.first )
			);
			this->irBuilder.CreateStore( savedValue, globalVar );
		}
		llvm::Value* finalState = this->irBuilder.CreateLoad( i32Type, genCtx.stateVar, "final.state" );
		this->irBuilder.CreateStore( finalState, statePtr );
		llvm::Value* finalValue = this->irBuilder.CreateLoad( yieldType, genCtx.valueVar, "final.value" );
		this->irBuilder.CreateStore( finalValue, valuePtr );
		llvm::Value* finalDone = this->irBuilder.CreateLoad( i1Type, genCtx.doneVar, "final.done" );
		this->irBuilder.CreateStore( finalDone, donePtr );
		this->irBuilder.CreateRetVoid();
		this->currentGeneratorContext = savedGenCtx;
		this->variableValueMap = savedVariableMap;
		this->blockMap = savedBlockMap;
		llvm::BasicBlock* initEntry = llvm::BasicBlock::Create( ctx, "entry", llvmFunction );
		this->irBuilder.SetInsertPoint( initEntry );
		llvm::Value* structSize = llvm::ConstantInt::get( i64Type,
			this->llvmModule->getDataLayout().getTypeAllocSize( genStructType ) );
		llvm::Value* genPtr = this->irBuilder.CreateCall(
			this->getOrCreateCalloc(),
			{ llvm::ConstantInt::get( i64Type, 1 ), structSize },
			"gen.ptr"
		);
		llvm::Value* initStatePtr = this->irBuilder.CreateStructGEP( genStructType, genPtr, 0, "init.state" );
		this->irBuilder.CreateStore( llvm::ConstantInt::get( i32Type, 0 ), initStatePtr );
		llvm::Value* initDonePtr = this->irBuilder.CreateStructGEP( genStructType, genPtr, 2, "init.done" );
		this->irBuilder.CreateStore( llvm::ConstantInt::getFalse( ctx ), initDonePtr );
		unsigned argIndex = 0;
		for( llvm::Argument& arg : llvmFunction->args() ) {
			if( argIndex < paramNames.size() ) {
				arg.setName( paramNames[argIndex] );
				llvm::Value* paramStorePtr = this->irBuilder.CreateStructGEP(
					genStructType, genPtr, 3 + argIndex,
					fmt::format( "init.{}", paramNames[argIndex] )
				);
				llvm::Value* argValue = &arg;
				if( argValue->getType() != paramLLVMTypes[argIndex] ) {
					if( paramLLVMTypes[argIndex]->isIntegerTy() && argValue->getType()->isIntegerTy() ) {
						argValue = this->irBuilder.CreateIntCast(
							argValue, paramLLVMTypes[argIndex], true, "init.cast"
						);
					}
				}
				this->irBuilder.CreateStore( argValue, paramStorePtr );
			}
			argIndex++;
		}
		this->irBuilder.CreateCall( nextFunction, { genPtr } );
		this->irBuilder.CreateRet( genPtr );
	}
	
	void MIRCodegen::generateAsyncFunction( MIRFunctionDefinition& functionDefinition, const std::string& llvmFunctionName, llvm::Function* llvmFunction ) {
		std::unordered_map<MIRVariableIdentifier, llvm::Value*> savedVariableValueMap = this->variableValueMap;
		std::unordered_map<MIRBlockIdentifier, llvm::BasicBlock*> savedBlockMap = this->blockMap;
		MIRFunctionDefinition* savedMIRFunction = this->currentMIRFunction;
		llvm::Type* i64Type = llvm::Type::getInt64Ty( this->llvmContext );
		llvm::Type* ptrType = llvm::PointerType::getUnqual( this->llvmContext );
		std::vector<llvm::Type*> paramLLVMTypes;
		std::vector<std::string> paramNames;
		std::vector<MIRVariableIdentifier> paramVarIds;
		for( MIRVariableIdentifier paramVar : functionDefinition.parameterVariableIdentifiers ) {
			llvm::Type* paramType = i64Type;
			std::string paramName = "param";
			if( functionDefinition.variableDescriptorTable.count( paramVar ) > 0 ) {
				MIRVariableDescriptor& descriptor = functionDefinition.variableDescriptorTable[paramVar];
				paramName = descriptor.variableName;
				if( descriptor.variableType != nullptr ) {
					paramType = this->toLLVMType( descriptor.variableType );
				}
			}
			if( paramType->isVoidTy() ) {
				paramType = i64Type;
			}
			if( functionDefinition.ownerClassQualifiedName.empty() == false && paramLLVMTypes.empty() ) {
				paramType = ptrType;
			}
			paramLLVMTypes.push_back( paramType );
			paramNames.push_back( paramName );
			paramVarIds.push_back( paramVar );
		}
		std::vector<llvm::GlobalVariable*> argGlobals;
		for( size_t i = 0; i < paramNames.size(); i++ ) {
			std::string globalName = fmt::format( "{}.arg.{}", llvmFunctionName, paramNames[i] );
			llvm::GlobalVariable* globalVar = new llvm::GlobalVariable(
				*this->llvmModule, paramLLVMTypes[i], false,
				llvm::GlobalValue::InternalLinkage,
				llvm::Constant::getNullValue( paramLLVMTypes[i] ),
				globalName
			);
			argGlobals.push_back( globalVar );
		}
		std::string wrapperName = fmt::format( "{}.async", llvmFunctionName );
		llvm::FunctionType* wrapperType = llvm::FunctionType::get(
			llvm::Type::getVoidTy( this->llvmContext ), { ptrType }, false
		);
		llvm::Function* wrapperFunction = llvm::Function::Create(
			wrapperType, llvm::Function::InternalLinkage, wrapperName, this->llvmModule.get()
		);
		wrapperFunction->setPersonalityFn( this->getOrCreatePersonality() );
		wrapperFunction->getArg( 0 )->setName( "task" );
		this->variableValueMap.clear();
		this->blockMap.clear();
		this->currentMIRFunction = &functionDefinition;
		for( std::shared_ptr<MIRBasicBlock>& basicBlock : functionDefinition.controlFlowBlocks ) {
			if( basicBlock != nullptr ) {
				llvm::BasicBlock* llvmBlock = llvm::BasicBlock::Create(
					this->llvmContext, basicBlock->blockLabel, wrapperFunction
				);
				this->blockMap[basicBlock->blockIdentifier] = llvmBlock;
			}
		}
		llvm::BasicBlock* entryBlock = this->blockMap[functionDefinition.entryBlockIdentifier];
		if( entryBlock == nullptr ) {
			this->variableValueMap = savedVariableValueMap;
			this->blockMap = savedBlockMap;
			this->currentMIRFunction = savedMIRFunction;
			return;
		}
		this->irBuilder.SetInsertPoint( entryBlock );
		for( size_t i = 0; i < paramNames.size(); i++ ) {
			llvm::Value* loadedValue = this->irBuilder.CreateLoad(
				paramLLVMTypes[i], argGlobals[i], paramNames[i]
			);
			llvm::AllocaInst* paramAlloca = this->createEntryBlockAllocation(
				wrapperFunction, paramNames[i], paramLLVMTypes[i]
			);
			this->irBuilder.CreateStore( loadedValue, paramAlloca );
			this->variableValueMap[paramVarIds[i]] = paramAlloca;
		}
		std::string sourceFile = "unknown";
		int64_t sourceLine = 0;
		int64_t sourceColumn = 0;
		if( functionDefinition.sourceLocation != nullptr ) {
			sourceFile = functionDefinition.sourceLocation->filename;
			if( functionDefinition.sourceLocation->location != nullptr ) {
				sourceLine = functionDefinition.sourceLocation->location->line;
				sourceColumn = functionDefinition.sourceLocation->location->column;
			}
		}
		this->emitPushFrame( sourceFile, sourceLine, sourceColumn, wrapperName );
		llvm::BasicBlock* asyncCatchBlock = llvm::BasicBlock::Create(
			this->llvmContext, "async.catch", wrapperFunction
		);
		this->isGeneratingAsyncWrapper = true;
		this->asyncWrapperCatchBlock = asyncCatchBlock;
		for( std::shared_ptr<MIRBasicBlock>& basicBlock : functionDefinition.controlFlowBlocks ) {
			if( basicBlock != nullptr ) {
				this->generateBasicBlock( *basicBlock, functionDefinition );
			}
		}
		this->asyncWrapperCatchBlock = nullptr;
		this->isGeneratingAsyncWrapper = false;
		for( llvm::BasicBlock& llvmBlock : *wrapperFunction ) {
			if( &llvmBlock == asyncCatchBlock ) {
				continue;
			}
			if( llvmBlock.getTerminator() == nullptr ) {
				this->irBuilder.SetInsertPoint( &llvmBlock );
				llvm::Value* zeroValue = llvm::ConstantInt::get( i64Type, 0 );
				llvm::Function* completeFunc = this->llvmModule->getFunction( "runtimeComplete" );
				if( completeFunc != nullptr ) {
					this->irBuilder.CreateCall( completeFunc, { zeroValue } );
				}
				else {
					completeFunc = this->llvmModule->getFunction( "uraniteTaskComplete" );
					if( completeFunc == nullptr ) {
						llvm::FunctionType* completeType = llvm::FunctionType::get(
							llvm::Type::getVoidTy( this->llvmContext ), { ptrType, i64Type }, false
						);
						completeFunc = llvm::Function::Create(
							completeType, llvm::Function::ExternalLinkage, "uraniteTaskComplete", this->llvmModule.get()
						);
					}
					this->irBuilder.CreateCall( completeFunc, { wrapperFunction->getArg( 0 ), zeroValue } );
				}
				this->emitPopFrame();
				this->irBuilder.CreateRetVoid();
			}
		}
		this->irBuilder.SetInsertPoint( asyncCatchBlock );
		{
			llvm::LandingPadInst* landingPad = this->irBuilder.CreateLandingPad(
				llvm::StructType::get( this->llvmContext, {
					llvm::PointerType::getUnqual( this->llvmContext ),
					llvm::Type::getInt32Ty( this->llvmContext )
				} ),
				1, "async.lp"
			);
			landingPad->addClause( llvm::ConstantPointerNull::get(
				llvm::PointerType::getUnqual( this->llvmContext )
			) );
			llvm::Value* exceptionPtr = this->irBuilder.CreateExtractValue( landingPad, 0, "async.exc.ptr" );
			llvm::Value* uraniteObj = this->irBuilder.CreateCall(
				this->getOrCreateBeginCatch(), { exceptionPtr }, "async.exc.obj"
			);
			llvm::Function* errorFunc = this->llvmModule->getFunction( "runtimeError" );
			if( errorFunc != nullptr ) {
				this->irBuilder.CreateCall( errorFunc, { uraniteObj } );
			}
			else {
				errorFunc = this->llvmModule->getFunction( "uraniteTaskError" );
				if( errorFunc == nullptr ) {
					llvm::FunctionType* errorType = llvm::FunctionType::get(
						llvm::Type::getVoidTy( this->llvmContext ),
						{ ptrType, ptrType }, false
					);
					errorFunc = llvm::Function::Create(
						errorType, llvm::Function::ExternalLinkage,
						"uraniteTaskError", this->llvmModule.get()
					);
				}
				this->irBuilder.CreateCall( errorFunc, { wrapperFunction->getArg( 0 ), uraniteObj } );
			}
			this->emitPopFrame();
			this->irBuilder.CreateRetVoid();
		}
		this->variableValueMap = savedVariableValueMap;
		this->blockMap = savedBlockMap;
		this->currentMIRFunction = savedMIRFunction;
		if( llvmFunction->empty() == false ) {
			return;
		}
		this->variableValueMap.clear();
		this->blockMap.clear();
		llvm::BasicBlock* spawnerEntry = llvm::BasicBlock::Create(
			this->llvmContext, "entry", llvmFunction
		);
		this->irBuilder.SetInsertPoint( spawnerEntry );
		for( size_t i = 0; i < paramVarIds.size(); i++ ) {
			if( i >= llvmFunction->arg_size() ) {
				break;
			}
			llvm::Argument* arg = llvmFunction->getArg( static_cast<unsigned>( i ) );
			arg->setName( paramNames[i] );
			llvm::AllocaInst* paramAlloca = this->createEntryBlockAllocation(
				llvmFunction, paramNames[i], arg->getType()
			);
			this->irBuilder.CreateStore( arg, paramAlloca );
			this->variableValueMap[paramVarIds[i]] = paramAlloca;
		}
		for( size_t i = 0; i < paramNames.size(); i++ ) {
			if( i >= llvmFunction->arg_size() ) {
				break;
			}
			llvm::Value* loadedArg = this->irBuilder.CreateLoad(
				paramLLVMTypes[i], this->variableValueMap[paramVarIds[i]],
				fmt::format( "{}.val", paramNames[i] )
			);
			this->irBuilder.CreateStore( loadedArg, argGlobals[i] );
		}
		llvm::Function* spawnFunc = this->llvmModule->getFunction( "runtimeSpawn" );
		if( spawnFunc != nullptr ) {
			llvm::Value* wrapperPtr = this->irBuilder.CreatePtrToInt( wrapperFunction, i64Type, "wrapper.i64" );
			llvm::Value* userData = llvm::ConstantInt::get( i64Type, 0 );
			llvm::CallInst* taskId = this->irBuilder.CreateCall( spawnFunc, { wrapperPtr, userData }, "taskId" );
			this->irBuilder.CreateRet( taskId );
		}
		else {
			spawnFunc = this->llvmModule->getFunction( "uraniteSpawnTask" );
			if( spawnFunc == nullptr ) {
				llvm::FunctionType* spawnType = llvm::FunctionType::get(
					i64Type, { ptrType, ptrType }, false
				);
				spawnFunc = llvm::Function::Create(
					spawnType, llvm::Function::ExternalLinkage, "uraniteSpawnTask", this->llvmModule.get()
				);
			}
			llvm::Value* spawnArg0;
			llvm::Value* spawnArg1;
			if( spawnFunc->getArg( 0 )->getType()->isIntegerTy() ) {
				spawnArg0 = this->irBuilder.CreatePtrToInt( wrapperFunction, spawnFunc->getArg( 0 )->getType(), "wrapper.i64" );
				spawnArg1 = llvm::ConstantInt::get( spawnFunc->getArg( 1 )->getType(), 0 );
			}
			else {
				spawnArg0 = wrapperFunction;
				spawnArg1 = llvm::ConstantPointerNull::get( llvm::PointerType::getUnqual( this->llvmContext ) );
			}
			llvm::CallInst* taskId = this->irBuilder.CreateCall( spawnFunc, { spawnArg0, spawnArg1 }, "taskId" );
			this->irBuilder.CreateRet( taskId );
		}
		this->variableValueMap = savedVariableValueMap;
		this->blockMap = savedBlockMap;
	}
	
} // namespace uranite::ir::mir
