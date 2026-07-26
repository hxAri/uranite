
//
// @author hxAri (hxari)
// @create 13-06-2026
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

#include <fmt/format.h>
#include <functional>

#include "uranite/ir/mir/lowering.hpp"
#include "uranite/semantic/qualnames.hpp"

namespace uranite::ir::mir {
	
	MIRLowering::MIRLowering( diagnostic::Engine& diagnosticEngine, const semantic::Registry* typeRegistry )
		: typeRegistry( typeRegistry ), diagnosticEngine( diagnosticEngine ) {
	}
	
	std::shared_ptr<MIRModuleDefinition> MIRLowering::lower( hir::HIRModule& hirModule ) {
		this->currentModule = std::make_shared<MIRModuleDefinition>();
		this->currentModule->moduleName = hirModule.moduleName;
		if( this->typeRegistry != nullptr ) {
			for( const std::pair<const std::string, semantic::TypeSharedPointer>& registryEntry : this->typeRegistry->getUserTypes() ) {
				if( registryEntry.second == nullptr || registryEntry.second->kind != semantic::Type::Kind::Class ) {
					continue;
				}
				semantic::ClassType* classType = static_cast<semantic::ClassType*>( registryEntry.second.get() );
				if( classType->typeSubstitutions.empty() ) {
					continue;
				}
				std::string baseName = registryEntry.first;
				size_t bracketPosition = baseName.find( '<' );
				if( bracketPosition == std::string::npos ) {
					continue;
				}
				baseName = baseName.substr( 0, bracketPosition );
				std::unordered_map<std::string, std::string>& substitutions = this->genericClassSubstitutions[baseName];
				for( const std::pair<const std::string, semantic::TypeSharedPointer>& substitution : classType->typeSubstitutions ) {
					std::string concreteTypeName = substitution.second->name;
					size_t genericBracket = concreteTypeName.find( '<' );
					if( genericBracket != std::string::npos ) {
						concreteTypeName = concreteTypeName.substr( 0, genericBracket );
					}
					bool isConcreteType = ( substitution.second->kind != semantic::Type::Kind::GenericParameter );
					if( substitutions.count( substitution.first ) == 0 || isConcreteType ) {
						substitutions[substitution.first] = concreteTypeName;
					}
				}
			}
			
			// Transitively resolve substitutions: E -> K -> String becomes E -> String
			bool changed = true;
			int maxIterations = 10;
			while( changed && maxIterations-- > 0 ) {
				changed = false;
				for( std::pair<const std::string, std::unordered_map<std::string, std::string>>& classEntry : this->genericClassSubstitutions ) {
					for( std::pair<const std::string, std::string>& paramEntry : classEntry.second ) {
						for( const std::pair<const std::string, std::unordered_map<std::string, std::string>>& otherClass : this->genericClassSubstitutions ) {
							std::unordered_map<std::string, std::string>::const_iterator resolvedIt = otherClass.second.find( paramEntry.second );
							if( resolvedIt != otherClass.second.end() && resolvedIt->second != paramEntry.second ) {
								paramEntry.second = resolvedIt->second;
								changed = true;
							}
						}
					}
				}
			}
		}
		for( std::shared_ptr<hir::HIRClassDefinition>& classDefinition : hirModule.classDefinitions ) {
			if( classDefinition == nullptr ) {
				continue;
			}
			TypeLayoutDescriptor typeLayout;
			std::string layoutKeyName = classDefinition->classQualifiedName.empty() == false
				? classDefinition->classQualifiedName : classDefinition->className;
			typeLayout.typeQualifiedName = layoutKeyName;
			typeLayout.hasVirtualTable = ( classDefinition->implementedInterfaceQualifiedNames.empty() == false );
			if( typeLayout.hasVirtualTable ) {
				std::string shortName = classDefinition->className;
				if( shortName == "Args" || shortName.find( "Args<" ) == 0 ) {
					typeLayout.hasVirtualTable = false;
				}
				if( typeLayout.hasVirtualTable && this->typeRegistry != nullptr ) {
					semantic::TypeSharedPointer semaType = this->typeRegistry->lookupType( shortName );
					if( semaType != nullptr && semaType->kind == semantic::Type::Kind::Class ) {
						semantic::ClassType* classTypePtr = static_cast<semantic::ClassType*>( semaType.get() );
						semantic::TypeSharedPointer walkType = classTypePtr->baseClass;
						while( walkType != nullptr ) {
							if( walkType->name == "Error" || walkType->name == "Exception" ||
								walkType->name == "Warning" || walkType->name == "Throwable" ) {
								typeLayout.hasVirtualTable = false;
								break;
							}
							if( walkType->kind == semantic::Type::Kind::Class ) {
								walkType = static_cast<semantic::ClassType*>( walkType.get() )->baseClass;
							}
							else {
								break;
							}
						}
					}
				}
			}
			int fieldIndex = 0;
			if( typeLayout.hasVirtualTable ) {
				typeLayout.fieldByteOffsets.push_back( 0 );
				typeLayout.fieldNames.push_back( "__vtable" );
				typeLayout.fieldTypes.push_back( std::make_shared<semantic::Type>( semantic::Type::Kind::Pointer, "ptr" ) );
				fieldIndex++;
			}
			for( const hir::HIRFieldDescriptor& fieldDescriptor : classDefinition->fieldDescriptors ) {
				typeLayout.fieldByteOffsets.push_back( fieldIndex * 8 );
				typeLayout.fieldNames.push_back( fieldDescriptor.fieldName );
				typeLayout.fieldTypes.push_back( fieldDescriptor.fieldType );
				fieldIndex++;
			}
			typeLayout.typeSizeInBytes = fieldIndex * 8;
			typeLayout.typeAlignmentInBytes = 8;
			this->currentModule->typeLayoutTable[layoutKeyName] = typeLayout;
			if( layoutKeyName != classDefinition->className ) {
				this->currentModule->typeLayoutTable[classDefinition->className] = typeLayout;
			}
		}
		for( std::shared_ptr<hir::HIRStructDefinition>& structDefinition : hirModule.structDefinitions ) {
			if( structDefinition != nullptr ) {
				this->lowerStructDefinition( *structDefinition );
			}
		}
		for( std::shared_ptr<hir::HIRClassDefinition>& classDefinition : hirModule.classDefinitions ) {
			if( classDefinition == nullptr || classDefinition->parentClassQualifiedName.empty() ) {
				continue;
			}
			std::string parentName = classDefinition->parentClassQualifiedName;
			if( this->currentModule->typeLayoutTable.count( parentName ) == 0 ) {
				continue;
			}
			std::string childLayoutKey = classDefinition->classQualifiedName.empty() == false
				? classDefinition->classQualifiedName : classDefinition->className;
			TypeLayoutDescriptor& childLayout = this->currentModule->typeLayoutTable[childLayoutKey];
			const TypeLayoutDescriptor& parentLayout = this->currentModule->typeLayoutTable[parentName];
			std::vector<int> mergedOffsets;
			std::vector<std::string> mergedNames;
			std::vector<semantic::TypeSharedPointer> mergedTypes;
			int fieldIndex = 0;
			for( size_t parentFieldIndex = 0; parentFieldIndex < parentLayout.fieldNames.size(); parentFieldIndex++ ) {
				mergedOffsets.push_back( fieldIndex * 8 );
				mergedNames.push_back( parentLayout.fieldNames[parentFieldIndex] );
				mergedTypes.push_back( parentLayout.fieldTypes[parentFieldIndex] );
				fieldIndex++;
			}
			for( size_t ownFieldIndex = 0; ownFieldIndex < childLayout.fieldNames.size(); ownFieldIndex++ ) {
				mergedOffsets.push_back( fieldIndex * 8 );
				mergedNames.push_back( childLayout.fieldNames[ownFieldIndex] );
				mergedTypes.push_back( childLayout.fieldTypes[ownFieldIndex] );
				fieldIndex++;
			}
			childLayout.fieldByteOffsets = std::move( mergedOffsets );
			childLayout.fieldNames = std::move( mergedNames );
			childLayout.fieldTypes = std::move( mergedTypes );
			childLayout.typeSizeInBytes = fieldIndex * 8;
			if( childLayoutKey != classDefinition->className ) {
				this->currentModule->typeLayoutTable[classDefinition->className] = childLayout;
			}
		}
		for( std::shared_ptr<hir::HIRConstantDefinition>& constantDefinition : hirModule.constantDefinitions ) {
			if( constantDefinition != nullptr && constantDefinition->initializerExpression != nullptr ) {
				MIRModuleConstant moduleConstant;
				hir::HIRNodeSharedPointer& initExpr = constantDefinition->initializerExpression;
				if( initExpr->nodeKind == hir::HIRNodeKind::IntegerLiteral ) {
					hir::HIRIntegerLiteral& intLit = static_cast<hir::HIRIntegerLiteral&>( *initExpr );
					moduleConstant.kind = MIRModuleConstant::Integer;
					moduleConstant.integerValue = intLit.integerValue;
				}
				else if( initExpr->nodeKind == hir::HIRNodeKind::FloatLiteral ) {
					hir::HIRFloatLiteral& floatLit = static_cast<hir::HIRFloatLiteral&>( *initExpr );
					moduleConstant.kind = MIRModuleConstant::Float;
					moduleConstant.floatValue = floatLit.floatValue;
				}
				else if( initExpr->nodeKind == hir::HIRNodeKind::BooleanLiteral ) {
					hir::HIRBooleanLiteral& boolLit = static_cast<hir::HIRBooleanLiteral&>( *initExpr );
					moduleConstant.kind = MIRModuleConstant::Boolean;
					moduleConstant.booleanValue = boolLit.booleanValue;
				}
				else if( initExpr->nodeKind == hir::HIRNodeKind::StringLiteral ) {
					hir::HIRStringLiteral& strLit = static_cast<hir::HIRStringLiteral&>( *initExpr );
					moduleConstant.kind = MIRModuleConstant::String;
					moduleConstant.stringValue = strLit.stringValue;
				}
				else {
					continue;
				}
				this->currentModule->moduleConstants[constantDefinition->constantName] = moduleConstant;
			}
		}
		for( std::shared_ptr<hir::HIREnumDefinition>& enumDefinition : hirModule.enumDefinitions ) {
			if( enumDefinition == nullptr ) {
				continue;
			}
			for( size_t variantIndex = 0; variantIndex < enumDefinition->variantDescriptors.size(); variantIndex++ ) {
				hir::HIREnumVariantDescriptor& variant = enumDefinition->variantDescriptors[variantIndex];
				std::string constantKey = fmt::format( "{}.{}", enumDefinition->enumName, variant.variantName );
				MIRModuleConstant moduleConstant;
				moduleConstant.kind = MIRModuleConstant::Integer;
				if( variant.backedValueExpression != nullptr &&
					variant.backedValueExpression->nodeKind == hir::HIRNodeKind::IntegerLiteral ) {
					hir::HIRIntegerLiteral& intLit = static_cast<hir::HIRIntegerLiteral&>( *variant.backedValueExpression );
					moduleConstant.integerValue = intLit.integerValue;
				}
				else {
					moduleConstant.integerValue = static_cast<int64_t>( variantIndex );
				}
				this->currentModule->moduleConstants[constantKey] = moduleConstant;
			}
		}
		for( std::shared_ptr<hir::HIRGlobalVariableDefinition>& globalVarDefinition : hirModule.globalVariableDefinitions ) {
			if( globalVarDefinition == nullptr ) {
				continue;
			}
			MIRGlobalVariable mirGlobal;
			mirGlobal.variableName = globalVarDefinition->variableName;
			mirGlobal.variableType = globalVarDefinition->variableType;
			if( globalVarDefinition->initializerExpression != nullptr ) {
				hir::HIRNodeSharedPointer& initExpr = globalVarDefinition->initializerExpression;
				if( initExpr->nodeKind == hir::HIRNodeKind::IntegerLiteral ) {
					mirGlobal.hasInitializer = true;
					hir::HIRIntegerLiteral& intLit = static_cast<hir::HIRIntegerLiteral&>( *initExpr );
					mirGlobal.initialValue.kind = MIRModuleConstant::Integer;
					mirGlobal.initialValue.integerValue = intLit.integerValue;
				}
				else if( initExpr->nodeKind == hir::HIRNodeKind::FloatLiteral ) {
					mirGlobal.hasInitializer = true;
					hir::HIRFloatLiteral& floatLit = static_cast<hir::HIRFloatLiteral&>( *initExpr );
					mirGlobal.initialValue.kind = MIRModuleConstant::Float;
					mirGlobal.initialValue.floatValue = floatLit.floatValue;
				}
				else if( initExpr->nodeKind == hir::HIRNodeKind::BooleanLiteral ) {
					mirGlobal.hasInitializer = true;
					hir::HIRBooleanLiteral& boolLit = static_cast<hir::HIRBooleanLiteral&>( *initExpr );
					mirGlobal.initialValue.kind = MIRModuleConstant::Boolean;
					mirGlobal.initialValue.booleanValue = boolLit.booleanValue;
				}
				else if( initExpr->nodeKind == hir::HIRNodeKind::StringLiteral ) {
					mirGlobal.hasInitializer = true;
					hir::HIRStringLiteral& strLit = static_cast<hir::HIRStringLiteral&>( *initExpr );
					mirGlobal.initialValue.kind = MIRModuleConstant::String;
					mirGlobal.initialValue.stringValue = strLit.stringValue;
				}
			}
			this->currentModule->globalVariables[globalVarDefinition->variableName] = mirGlobal;
		}
		for( std::shared_ptr<hir::HIRExternFunctionDeclaration>& externDeclaration : hirModule.externFunctionDeclarations ) {
			if( externDeclaration == nullptr ) {
				continue;
			}
			MIRExternFunction mirExtern;
			mirExtern.functionName = externDeclaration->functionName;
			mirExtern.linkageName = externDeclaration->linkageName.empty() ?
				externDeclaration->functionName : externDeclaration->linkageName;
			mirExtern.returnType = externDeclaration->returnTypeDescriptor;
			mirExtern.isVariadic = externDeclaration->isVariadicFunction;
			for( hir::HIRParameterDescriptor& param : externDeclaration->parameterDescriptors ) {
				mirExtern.parameterTypes.push_back( param.parameterType );
			}
			this->currentModule->externFunctions.push_back( std::move( mirExtern ) );
		}
		for( std::shared_ptr<hir::HIRClassDefinition>& classDefinition : hirModule.classDefinitions ) {
			if( classDefinition != nullptr ) {
				this->lowerClassDefinition( *classDefinition );
			}
		}
		for( std::shared_ptr<hir::HIREnumDefinition>& enumDefinition : hirModule.enumDefinitions ) {
			if( enumDefinition == nullptr ) {
				continue;
			}
			this->lowerEnumMethodDefinitions( *enumDefinition );
		}
		for( std::shared_ptr<hir::HIRFunctionDefinition>& functionDefinition : hirModule.functionDefinitions ) {
			if( functionDefinition != nullptr ) {
				this->lowerFunctionDefinition( *functionDefinition );
			}
		}
		return this->currentModule;
	}
	
	void MIRLowering::lowerFunctionDefinition( hir::HIRFunctionDefinition& hirFunction ) {
		std::shared_ptr<MIRFunctionDefinition> mirFunction = std::make_shared<MIRFunctionDefinition>();
		mirFunction->functionName = hirFunction.functionName;
		mirFunction->mangledFunctionName = hirFunction.mangledName;
		mirFunction->ownerClassQualifiedName = hirFunction.ownerClassName;
		mirFunction->returnTypeDescriptor = hirFunction.returnTypeDescriptor;
		mirFunction->sourceLocation = hirFunction.sourceLocation;
		mirFunction->isGeneratorFunction = hirFunction.isGeneratorFunction;
		mirFunction->isAsyncFunction = hirFunction.isAsyncFunction;
		if( hirFunction.isAsyncFunction && hirFunction.returnTypeDescriptor != nullptr ) {
			if( hirFunction.returnTypeDescriptor->kind == semantic::Type::Kind::Future ) {
				semantic::FutureType* futureType = static_cast<semantic::FutureType*>( hirFunction.returnTypeDescriptor.get() );
				mirFunction->asyncInnerReturnType = futureType->innerType;
			}
			else if( hirFunction.returnTypeDescriptor->kind == semantic::Type::Kind::Class ) {
				std::string typeName = hirFunction.returnTypeDescriptor->name;
				size_t openBracket = typeName.find( '<' );
				size_t closeBracket = typeName.rfind( '>' );
				if( openBracket != std::string::npos && closeBracket != std::string::npos ) {
					std::string elementName = typeName.substr( openBracket + 1, closeBracket - openBracket - 1 );
					mirFunction->asyncInnerReturnType = std::make_shared<semantic::Type>(
						semantic::Type::Kind::Class, elementName
					);
				}
			}
		}
		if( hirFunction.isGeneratorFunction && hirFunction.returnTypeDescriptor != nullptr ) {
			if( hirFunction.returnTypeDescriptor->kind == semantic::Type::Kind::Generator ) {
				semantic::GeneratorType* genType = static_cast<semantic::GeneratorType*>( hirFunction.returnTypeDescriptor.get() );
				mirFunction->generatorYieldType = genType->yieldType;
			}
			else if( hirFunction.returnTypeDescriptor->kind == semantic::Type::Kind::Class ) {
				std::string typeName = hirFunction.returnTypeDescriptor->name;
				size_t openBracket = typeName.find( '<' );
				size_t closeBracket = typeName.rfind( '>' );
				if( openBracket != std::string::npos && closeBracket != std::string::npos ) {
					std::string elementName = typeName.substr( openBracket + 1, closeBracket - openBracket - 1 );
					mirFunction->generatorYieldType = std::make_shared<semantic::Type>(
						semantic::Type::Kind::Class, elementName
					);
				}
			}
		}
		this->currentFunction = mirFunction;
		this->nextInstructionIdentifier = 0;
		this->variableNameMap.clear();
		this->deferredStatements.clear();
		
		std::shared_ptr<MIRBasicBlock> entryBlock = mirFunction->createBasicBlock( "entry" );
		this->switchToBlock( entryBlock );
		mirFunction->entryBlockIdentifier = entryBlock->blockIdentifier;
		
		for( size_t paramIndex = 0; paramIndex < hirFunction.parameterDescriptors.size(); paramIndex++ ) {
			const hir::HIRParameterDescriptor& parameterDescriptor = hirFunction.parameterDescriptors[paramIndex];
			MIRVariableIdentifier parameterVariable = mirFunction->allocateVariable(
				parameterDescriptor.parameterName,
				parameterDescriptor.parameterType,
				parameterDescriptor.isMutableParameter
			);
			mirFunction->variableDescriptorTable[parameterVariable].isParameterVariable = true;
			mirFunction->parameterVariableIdentifiers.push_back( parameterVariable );
			this->variableNameMap[parameterDescriptor.parameterName] = parameterVariable;
			if( parameterDescriptor.isVariadicParameter ) {
				mirFunction->variadicParameterIndex = static_cast<int>( paramIndex );
				if( parameterDescriptor.parameterType != nullptr ) {
					semantic::TypeSharedPointer elemType = parameterDescriptor.parameterType;
					if( elemType->kind == semantic::Type::Kind::Class ) {
						semantic::ClassType* classType = dynamic_cast<semantic::ClassType*>( elemType.get() );
						if( classType != nullptr && classType->typeSubstitutions.empty() == false ) {
							for( const std::pair<const std::string, semantic::TypeSharedPointer>& sub : classType->typeSubstitutions ) {
								if( sub.second != nullptr ) {
									mirFunction->variadicElementType = sub.second;
									break;
								}
							}
						}
					}
					if( mirFunction->variadicElementType == nullptr ) {
						if( elemType->kind == semantic::Type::Kind::Array ) {
							semantic::ArrayType* arrayType = dynamic_cast<semantic::ArrayType*>( elemType.get() );
							if( arrayType != nullptr && arrayType->elementType != nullptr ) {
								mirFunction->variadicElementType = arrayType->elementType;
							}
						}
					}
					if( mirFunction->variadicElementType == nullptr ) {
						std::string typeName = elemType->name;
						size_t openBracket = typeName.find( '<' );
						size_t closeBracket = typeName.rfind( '>' );
						if( openBracket != std::string::npos && closeBracket != std::string::npos ) {
							std::string elementName = typeName.substr( openBracket + 1, closeBracket - openBracket - 1 );
							mirFunction->variadicElementType = std::make_shared<semantic::Type>(
								semantic::Type::Kind::Class, elementName
							);
						}
						else if( elemType->kind != semantic::Type::Kind::Array ) {
							mirFunction->variadicElementType = elemType;
						}
					}
				}
			}
			if( parameterDescriptor.isKeywordParameter ) {
				mirFunction->keywordParameterIndex = static_cast<int>( paramIndex );
				if( parameterDescriptor.parameterType != nullptr ) {
					semantic::TypeSharedPointer kwType = parameterDescriptor.parameterType;
					if( kwType->kind == semantic::Type::Kind::Class ) {
						semantic::ClassType* classType = dynamic_cast<semantic::ClassType*>( kwType.get() );
						if( classType != nullptr && classType->typeSubstitutions.empty() == false ) {
							for( const std::pair<const std::string, semantic::TypeSharedPointer>& sub : classType->typeSubstitutions ) {
								if( sub.second != nullptr ) {
									mirFunction->keywordValueType = sub.second;
									break;
								}
							}
						}
					}
				}
			}
			if( parameterDescriptor.defaultValueExpression != nullptr ) {
				MIRModuleConstant defaultConstant;
				bool captured = false;
				switch( parameterDescriptor.defaultValueExpression->nodeKind ) {
					case hir::HIRNodeKind::StringLiteral: {
						hir::HIRStringLiteral& strLit = static_cast<hir::HIRStringLiteral&>(
							*parameterDescriptor.defaultValueExpression );
						defaultConstant.kind = MIRModuleConstant::String;
						defaultConstant.stringValue = strLit.stringValue;
						captured = true;
						break;
					}
					case hir::HIRNodeKind::IntegerLiteral: {
						hir::HIRIntegerLiteral& intLit = static_cast<hir::HIRIntegerLiteral&>(
							*parameterDescriptor.defaultValueExpression );
						defaultConstant.kind = MIRModuleConstant::Integer;
						defaultConstant.integerValue = intLit.integerValue;
						captured = true;
						break;
					}
					case hir::HIRNodeKind::FloatLiteral: {
						hir::HIRFloatLiteral& floatLit = static_cast<hir::HIRFloatLiteral&>(
							*parameterDescriptor.defaultValueExpression );
						defaultConstant.kind = MIRModuleConstant::Float;
						defaultConstant.floatValue = floatLit.floatValue;
						captured = true;
						break;
					}
					case hir::HIRNodeKind::BooleanLiteral: {
						hir::HIRBooleanLiteral& boolLit = static_cast<hir::HIRBooleanLiteral&>(
							*parameterDescriptor.defaultValueExpression );
						defaultConstant.kind = MIRModuleConstant::Boolean;
						defaultConstant.booleanValue = boolLit.booleanValue;
						captured = true;
						break;
					}
					case hir::HIRNodeKind::NoneLiteral: {
						defaultConstant.kind = MIRModuleConstant::Null;
						captured = true;
						break;
					}
					default:
						break;
				}
				if( captured ) {
					mirFunction->parameterDefaultValues[static_cast<int>( paramIndex )] = defaultConstant;
				}
			}
		}
		if( hirFunction.functionBody != nullptr ) {
			this->lowerBlock( *hirFunction.functionBody );
		}
		this->ensureBlockTerminated();
		this->currentModule->functionDefinitions.push_back( std::move( mirFunction ) );
		this->currentFunction = nullptr;
		this->currentBlock = nullptr;
	}
	
	void MIRLowering::lowerClassDefinition( hir::HIRClassDefinition& hirClass ) {
		std::string savedClassName = this->currentClassName;
		std::string savedParentClassName = this->currentParentClassName;
		this->currentClassName = hirClass.classQualifiedName.empty() == false
			? hirClass.classQualifiedName : hirClass.className;
		this->currentParentClassName = hirClass.parentClassQualifiedName;
		for( std::shared_ptr<hir::HIRFunctionDefinition>& methodDefinition : hirClass.methodDefinitions ) {
			if( methodDefinition != nullptr ) {
				this->lowerFunctionDefinition( *methodDefinition );
			}
		}
		this->currentClassName = savedClassName;
		this->currentParentClassName = savedParentClassName;
	}
	
	void MIRLowering::lowerStructDefinition( hir::HIRStructDefinition& hirStruct ) {
		std::string structLayoutKey = hirStruct.structQualifiedName.empty() == false
			? hirStruct.structQualifiedName : hirStruct.structName;
		TypeLayoutDescriptor typeLayout;
		typeLayout.typeQualifiedName = structLayoutKey;
		typeLayout.hasVirtualTable = false;
		int fieldIndex = 0;
		for( const hir::HIRFieldDescriptor& fieldDescriptor : hirStruct.fieldDescriptors ) {
			typeLayout.fieldByteOffsets.push_back( fieldIndex * 8 );
			typeLayout.fieldNames.push_back( fieldDescriptor.fieldName );
			typeLayout.fieldTypes.push_back( fieldDescriptor.fieldType );
			fieldIndex++;
		}
		typeLayout.typeSizeInBytes = fieldIndex * 8;
		typeLayout.typeAlignmentInBytes = 8;
		this->currentModule->typeLayoutTable[structLayoutKey] = typeLayout;
		if( structLayoutKey != hirStruct.structName ) {
			this->currentModule->typeLayoutTable[hirStruct.structName] = typeLayout;
		}
		std::string savedClassName = this->currentClassName;
		std::string savedParentClassName = this->currentParentClassName;
		this->currentClassName = hirStruct.structQualifiedName.empty() == false
			? hirStruct.structQualifiedName : hirStruct.structName;
		this->currentParentClassName = "";
		for( std::shared_ptr<hir::HIRFunctionDefinition>& methodDefinition : hirStruct.methodDefinitions ) {
			if( methodDefinition != nullptr ) {
				this->lowerFunctionDefinition( *methodDefinition );
			}
		}
		this->currentClassName = savedClassName;
		this->currentParentClassName = savedParentClassName;
	}
	
	void MIRLowering::lowerEnumMethodDefinitions( hir::HIREnumDefinition& hirEnum ) {
		std::string enumQualifiedName = hirEnum.enumQualifiedName.empty() == false
			? hirEnum.enumQualifiedName : hirEnum.enumName;
		std::unordered_map<std::string, std::vector<std::pair<int64_t, std::shared_ptr<hir::HIRFunctionDefinition>>>> variantOverrides;
		for( size_t variantIndex = 0; variantIndex < hirEnum.variantDescriptors.size(); variantIndex++ ) {
			hir::HIREnumVariantDescriptor& variant = hirEnum.variantDescriptors[variantIndex];
			int64_t discriminant = static_cast<int64_t>( variantIndex );
			if( variant.backedValueExpression != nullptr &&
				variant.backedValueExpression->nodeKind == hir::HIRNodeKind::IntegerLiteral ) {
				hir::HIRIntegerLiteral& intLit = static_cast<hir::HIRIntegerLiteral&>( *variant.backedValueExpression );
				discriminant = intLit.integerValue;
			}
			for( std::shared_ptr<hir::HIRFunctionDefinition>& variantMethod : variant.variantMethods ) {
				if( variantMethod != nullptr ) {
					variantOverrides[variantMethod->functionName].push_back( { discriminant, variantMethod } );
				}
			}
		}
		std::string savedClassName = this->currentClassName;
		std::string savedParentClassName = this->currentParentClassName;
		this->currentParentClassName = "";
		for( std::shared_ptr<hir::HIRFunctionDefinition>& baseMethod : hirEnum.methodDefinitions ) {
			if( baseMethod == nullptr ) {
				continue;
			}
			std::string methodName = baseMethod->functionName;
			std::string ownerName = baseMethod->ownerClassName.empty() == false
				? baseMethod->ownerClassName : enumQualifiedName;
			std::unordered_map<std::string, std::vector<std::pair<int64_t, std::shared_ptr<hir::HIRFunctionDefinition>>>>::iterator overrideIt =
				variantOverrides.find( methodName );
			if( overrideIt == variantOverrides.end() || overrideIt->second.empty() ) {
				this->lowerFunctionDefinition( *baseMethod );
				continue;
			}
			this->currentClassName = ownerName;
			std::string fullDispatchName = fmt::format( "{}.{}", ownerName, methodName );
			std::shared_ptr<MIRFunctionDefinition> dispatchFunction = std::make_shared<MIRFunctionDefinition>();
			dispatchFunction->functionName = fullDispatchName;
			dispatchFunction->returnTypeDescriptor = baseMethod->returnTypeDescriptor;
			std::shared_ptr<MIRFunctionDefinition> previousFunction = this->currentFunction;
			std::unordered_map<std::string, MIRVariableIdentifier> savedVariableNames = this->variableNameMap;
			this->variableNameMap.clear();
			this->currentFunction = dispatchFunction;
			std::shared_ptr<MIRBasicBlock> entryBlock = this->currentFunction->createBasicBlock( "entry" );
			this->switchToBlock( entryBlock );
			semantic::TypeSharedPointer enumBackingType = std::make_shared<semantic::Type>( semantic::Type::Kind::Integer, "I32" );
			MIRVariableIdentifier selfParam = this->currentFunction->allocateVariable( "self", enumBackingType, false );
			dispatchFunction->parameterVariableIdentifiers.push_back( selfParam );
			for( size_t paramIdx = 1; paramIdx < baseMethod->parameterDescriptors.size(); paramIdx++ ) {
				hir::HIRParameterDescriptor& param = baseMethod->parameterDescriptors[paramIdx];
				MIRVariableIdentifier paramVar = this->currentFunction->allocateVariable(
					param.parameterName, param.parameterType, false
				);
				dispatchFunction->parameterVariableIdentifiers.push_back( paramVar );
			}
			this->variableNameMap["self"] = selfParam;
			std::shared_ptr<MIRBasicBlock> defaultBlock = this->currentFunction->createBasicBlock( "default" );
			MIRInstruction loadSelf( MIRInstructionKind::LoadVariable );
			loadSelf.sourceOperands.push_back( selfParam );
			loadSelf.operandType = enumBackingType;
			MIRVariableIdentifier selfValue = this->currentFunction->allocateVariable( "_self_val", enumBackingType, false );
			loadSelf.destinationVariable = selfValue;
			this->emitInstruction( loadSelf );
			MIRInstruction switchInstr( MIRInstructionKind::SwitchBranch );
			switchInstr.sourceOperands.push_back( selfValue );
			switchInstr.defaultSwitchTarget = defaultBlock->blockIdentifier;
			for( std::pair<int64_t, std::shared_ptr<hir::HIRFunctionDefinition>>& overridePair : overrideIt->second ) {
				int64_t discriminant = overridePair.first;
				std::shared_ptr<hir::HIRFunctionDefinition>& overrideMethod = overridePair.second;
				std::shared_ptr<MIRBasicBlock> caseBlock = this->currentFunction->createBasicBlock( fmt::format( "case.{}", discriminant ) );
				switchInstr.switchBranchTargets.push_back( { discriminant, caseBlock->blockIdentifier } );
				this->switchToBlock( caseBlock );
				if( overrideMethod->functionBody != nullptr ) {
					this->lowerBlock( *overrideMethod->functionBody );
				}
				if( this->currentBlock != nullptr &&
					( this->currentBlock->blockInstructions.empty() ||
					  this->currentBlock->blockInstructions.back().instructionKind != MIRInstructionKind::ReturnValue ) ) {
					MIRInstruction retVoid( MIRInstructionKind::ReturnValue );
					retVoid.sourceOperands.clear();
					this->emitTerminator( retVoid );
				}
			}
			this->switchToBlock( entryBlock );
			this->emitTerminator( switchInstr );
			this->switchToBlock( defaultBlock );
			if( baseMethod->functionBody != nullptr ) {
				this->lowerBlock( *baseMethod->functionBody );
			}
			if( this->currentBlock != nullptr &&
				( this->currentBlock->blockInstructions.empty() ||
				  this->currentBlock->blockInstructions.back().instructionKind != MIRInstructionKind::ReturnValue ) ) {
				MIRInstruction retVoid( MIRInstructionKind::ReturnValue );
				retVoid.sourceOperands.clear();
				this->emitTerminator( retVoid );
			}
			this->currentFunction = previousFunction;
			this->variableNameMap = savedVariableNames;
			this->currentModule->functionDefinitions.push_back( std::move( dispatchFunction ) );
		}
		this->currentClassName = savedClassName;
		this->currentParentClassName = savedParentClassName;
	}
	
	void MIRLowering::lowerBlock( hir::HIRBlock& hirBlock ) {
		for( const hir::HIRNodeSharedPointer& statement : hirBlock.blockStatements ) {
			if( statement != nullptr ) {
				this->lowerStatement( statement );
			}
		}
	}
	
	void MIRLowering::lowerStatement( const hir::HIRNodeSharedPointer& hirStatement ) {
		if( hirStatement == nullptr || this->currentBlock == nullptr ) {
			return;
		}
		if( this->currentBlock->isTerminated ) {
			return;
		}
		switch( hirStatement->nodeKind ) {
			case hir::HIRNodeKind::VariableBinding: {
				hir::HIRVariableBinding& binding = static_cast<hir::HIRVariableBinding&>( *hirStatement );
				MIRVariableIdentifier variableIdentifier = this->currentFunction->allocateVariable(
					binding.variableName,
					binding.variableType,
					binding.isMutableBinding
				);
				this->variableNameMap[binding.variableName] = variableIdentifier;
				MIRInstruction allocateInstruction( MIRInstructionKind::AllocateLocal );
				allocateInstruction.destinationVariable = variableIdentifier;
				allocateInstruction.operandType = binding.variableType;
				allocateInstruction.sourceLocation = binding.sourceLocation;
				this->emitInstruction( allocateInstruction );
				if( binding.initializerExpression != nullptr ) {
					MIRVariableIdentifier initializerValue = this->lowerExpression( binding.initializerExpression );
					MIRInstruction storeInstruction( MIRInstructionKind::StoreVariable );
					storeInstruction.destinationVariable = variableIdentifier;
					storeInstruction.sourceOperands.push_back( initializerValue );
					storeInstruction.sourceLocation = binding.sourceLocation;
					this->emitInstruction( storeInstruction );
				}
				break;
			}
			case hir::HIRNodeKind::Assignment: {
				hir::HIRAssignment& assignment = static_cast<hir::HIRAssignment&>( *hirStatement );
				MIRVariableIdentifier valueVariable = this->lowerExpression( assignment.valueExpression );
				MIRVariableIdentifier targetVariable = INVALID_VARIABLE_IDENTIFIER;
				std::string globalTargetName;
				if( assignment.targetExpression != nullptr &&
					assignment.targetExpression->nodeKind == hir::HIRNodeKind::Identifier ) {
					hir::HIRIdentifier& targetIdentifier =
						static_cast<hir::HIRIdentifier&>( *assignment.targetExpression );
					std::unordered_map<std::string, MIRVariableIdentifier>::iterator targetLookup =
						this->variableNameMap.find( targetIdentifier.identifierName );
					if( targetLookup != this->variableNameMap.end() ) {
						targetVariable = targetLookup->second;
					}
					else if( this->currentModule->globalVariables.find( targetIdentifier.identifierName ) !=
							 this->currentModule->globalVariables.end() ) {
						globalTargetName = targetIdentifier.identifierName;
					}
				}
				else {
					targetVariable = this->lowerExpression( assignment.targetExpression );
				}
				if( assignment.assignmentOperator != token::Type::Assignment &&
					( targetVariable != INVALID_VARIABLE_IDENTIFIER || globalTargetName.empty() == false ) ) {
					MIRInstruction loadInstruction( MIRInstructionKind::LoadVariable );
					loadInstruction.destinationVariable = this->currentFunction->allocateVariable(
						"_compound_lhs", assignment.targetExpression->resolvedType, false
					);
					if( globalTargetName.empty() == false ) {
						loadInstruction.calledFunctionQualifiedName = fmt::format( "@{}", globalTargetName );
					}
					else {
						loadInstruction.sourceOperands.push_back( targetVariable );
					}
					loadInstruction.sourceLocation = assignment.sourceLocation;
					MIRVariableIdentifier loadedTarget = this->emitInstruction( loadInstruction );
					MIRInstructionKind arithmeticKind = MIRInstructionKind::AddInteger;
					auto isFloatTypeCheck = []( const semantic::TypeSharedPointer& type ) -> bool {
						return type != nullptr &&
							( type->kind == semantic::Type::Kind::Float ||
							  ( type->kind == semantic::Type::Kind::Class &&
							    ( type->name == "Float" || type->name == "Double" ||
							      type->name == "F32" || type->name == "F64" ) ) );
					};
					bool isFloatOp = ( assignment.valueExpression != nullptr &&
						isFloatTypeCheck( assignment.valueExpression->resolvedType ) ) ||
						( assignment.targetExpression != nullptr &&
						isFloatTypeCheck( assignment.targetExpression->resolvedType ) );
					switch( assignment.assignmentOperator ) {
						case token::Type::PlusAssignment:
							arithmeticKind = isFloatOp ? MIRInstructionKind::AddFloat : MIRInstructionKind::AddInteger;
							break;
						case token::Type::MinusAssignment:
							arithmeticKind = isFloatOp ? MIRInstructionKind::SubtractFloat : MIRInstructionKind::SubtractInteger;
							break;
						case token::Type::StarAssignment:
							arithmeticKind = isFloatOp ? MIRInstructionKind::MultiplyFloat : MIRInstructionKind::MultiplyInteger;
							break;
						case token::Type::SlashAssignment:
							arithmeticKind = isFloatOp ? MIRInstructionKind::DivideFloat : MIRInstructionKind::DivideInteger;
							break;
						default:
							break;
					}
					MIRInstruction arithmeticInstruction( arithmeticKind );
					arithmeticInstruction.sourceOperands.push_back( loadedTarget );
					arithmeticInstruction.sourceOperands.push_back( valueVariable );
					arithmeticInstruction.operandType = assignment.valueExpression != nullptr
						? assignment.valueExpression->resolvedType : nullptr;
					arithmeticInstruction.sourceLocation = assignment.sourceLocation;
					arithmeticInstruction.destinationVariable = this->currentFunction->allocateVariable(
						"_compound_result", assignment.valueExpression != nullptr
							? assignment.valueExpression->resolvedType : nullptr, false
					);
					valueVariable = this->emitInstruction( arithmeticInstruction );
				}
				MIRInstruction storeInstruction( MIRInstructionKind::StoreVariable );
				if( globalTargetName.empty() == false ) {
					storeInstruction.calledFunctionQualifiedName = fmt::format( "@{}", globalTargetName );
				}
				else {
					storeInstruction.destinationVariable = targetVariable;
				}
				storeInstruction.sourceOperands.push_back( valueVariable );
				storeInstruction.sourceLocation = assignment.sourceLocation;
				this->emitInstruction( storeInstruction );
				break;
			}
			case hir::HIRNodeKind::Return: {
				hir::HIRReturn& returnNode = static_cast<hir::HIRReturn&>( *hirStatement );
				MIRInstruction returnInstruction( MIRInstructionKind::ReturnValue );
				returnInstruction.sourceLocation = returnNode.sourceLocation;
				if( returnNode.returnValueExpression != nullptr ) {
					MIRVariableIdentifier returnValue = this->lowerExpression( returnNode.returnValueExpression );
					returnInstruction.sourceOperands.push_back( returnValue );
				}
				this->emitDeferredStatements();
				this->emitTerminator( returnInstruction );
				break;
			}
			case hir::HIRNodeKind::If: {
				hir::HIRIf& ifNode = static_cast<hir::HIRIf&>( *hirStatement );
				this->lowerIf( ifNode );
				break;
			}
			case hir::HIRNodeKind::Loop: {
				hir::HIRLoop& loopNode = static_cast<hir::HIRLoop&>( *hirStatement );
				this->lowerLoop( loopNode );
				break;
			}
			case hir::HIRNodeKind::Match: {
				hir::HIRMatch& matchNode = static_cast<hir::HIRMatch&>( *hirStatement );
				this->lowerMatch( matchNode );
				break;
			}
			case hir::HIRNodeKind::Switch: {
				hir::HIRSwitch& switchNode = static_cast<hir::HIRSwitch&>( *hirStatement );
				this->lowerSwitch( switchNode );
				break;
			}
			case hir::HIRNodeKind::TryCatch: {
				hir::HIRTryCatch& tryCatchNode = static_cast<hir::HIRTryCatch&>( *hirStatement );
				this->lowerTryCatch( tryCatchNode );
				break;
			}
			case hir::HIRNodeKind::Defer: {
				hir::HIRDefer& deferNode = static_cast<hir::HIRDefer&>( *hirStatement );
				this->lowerDefer( deferNode );
				break;
			}
			case hir::HIRNodeKind::Throw: {
				hir::HIRThrow& throwNode = static_cast<hir::HIRThrow&>( *hirStatement );
				MIRVariableIdentifier thrownValue = this->lowerExpression( throwNode.thrownExpression );
				MIRInstruction throwInstruction( MIRInstructionKind::ThrowException );
				throwInstruction.sourceOperands.push_back( thrownValue );
				throwInstruction.sourceLocation = throwNode.sourceLocation;
				if( throwNode.thrownExpression != nullptr && throwNode.thrownExpression->resolvedType != nullptr ) {
					throwInstruction.calledFunctionQualifiedName =
						throwNode.thrownExpression->resolvedType->qualified.empty() == false
						? throwNode.thrownExpression->resolvedType->qualified
						: throwNode.thrownExpression->resolvedType->name;
				}
				if( this->activeLandingPad != INVALID_BLOCK_IDENTIFIER ) {
					throwInstruction.landingPadTarget = this->activeLandingPad;
				}
				this->emitTerminator( throwInstruction );
				break;
			}
			case hir::HIRNodeKind::Delete: {
				hir::HIRDelete& deleteNode = static_cast<hir::HIRDelete&>( *hirStatement );
				MIRVariableIdentifier targetVariable = this->lowerExpression( deleteNode.targetExpression );
				MIRInstruction freeInstruction( MIRInstructionKind::HeapFree );
				freeInstruction.sourceOperands.push_back( targetVariable );
				freeInstruction.sourceLocation = deleteNode.sourceLocation;
				this->emitInstruction( freeInstruction );
				break;
			}
			case hir::HIRNodeKind::Break: {
				if( this->loopContextStack.empty() == false ) {
					LoopContext& loopContext = this->loopContextStack.top();
					for( size_t deferIndex = this->deferredStatements.size(); deferIndex > loopContext.deferCountAtEntry; --deferIndex ) {
						this->lowerStatement( this->deferredStatements[deferIndex - 1] );
					}
					MIRInstruction jumpInstruction( MIRInstructionKind::JumpUnconditional );
					jumpInstruction.trueBranchTarget = loopContext.exitBlockIdentifier;
					jumpInstruction.sourceLocation = hirStatement->sourceLocation;
					this->emitTerminator( jumpInstruction );
				}
				break;
			}
			case hir::HIRNodeKind::Continue: {
				if( this->loopContextStack.empty() == false ) {
					LoopContext& loopContext = this->loopContextStack.top();
					for( size_t deferIndex = this->deferredStatements.size(); deferIndex > loopContext.deferCountAtEntry; --deferIndex ) {
						this->lowerStatement( this->deferredStatements[deferIndex - 1] );
					}
					MIRBlockIdentifier continueTarget = loopContext.updateBlockIdentifier;
					if( continueTarget == INVALID_BLOCK_IDENTIFIER ) {
						continueTarget = loopContext.headerBlockIdentifier;
					}
					MIRInstruction jumpInstruction( MIRInstructionKind::JumpUnconditional );
					jumpInstruction.trueBranchTarget = continueTarget;
					jumpInstruction.sourceLocation = hirStatement->sourceLocation;
					this->emitTerminator( jumpInstruction );
				}
				break;
			}
			case hir::HIRNodeKind::ExpressionStatement: {
				hir::HIRExpressionStatement& exprStatement = static_cast<hir::HIRExpressionStatement&>( *hirStatement );
				if( exprStatement.expression != nullptr ) {
					this->lowerExpression( exprStatement.expression );
				}
				break;
			}
			case hir::HIRNodeKind::InlineAssembly: {
				hir::HIRInlineAssembly& asmNode = static_cast<hir::HIRInlineAssembly&>( *hirStatement );
				MIRInstruction asmInstruction( MIRInstructionKind::InlineAssembly );
				asmInstruction.assemblyTemplate = asmNode.assemblyTemplate;
				asmInstruction.assemblyIsVolatile = asmNode.isVolatile;
				asmInstruction.assemblyClobbers = asmNode.clobberRegisters;
				for( const hir::HIRAsmOperand& outputOperand : asmNode.outputOperands ) {
					asmInstruction.assemblyOutputConstraints.push_back( outputOperand.constraintString );
					if( outputOperand.boundExpression != nullptr ) {
						MIRVariableIdentifier outputVariable = INVALID_VARIABLE_IDENTIFIER;
						if( outputOperand.boundExpression->nodeKind == hir::HIRNodeKind::Identifier ) {
							hir::HIRIdentifier& outputIdentifier =
								static_cast<hir::HIRIdentifier&>( *outputOperand.boundExpression );
							std::unordered_map<std::string, MIRVariableIdentifier>::iterator outputLookup =
								this->variableNameMap.find( outputIdentifier.identifierName );
							if( outputLookup != this->variableNameMap.end() ) {
								outputVariable = outputLookup->second;
							}
						}
						else if( outputOperand.boundExpression->nodeKind == hir::HIRNodeKind::FieldAccess ) {
							outputVariable = this->lowerExpression( outputOperand.boundExpression );
						}
						if( outputVariable == INVALID_VARIABLE_IDENTIFIER ) {
							outputVariable = this->lowerExpression( outputOperand.boundExpression );
						}
						asmInstruction.assemblyOutputVariables.push_back( outputVariable );
					}
				}
				for( const hir::HIRAsmOperand& inputOperand : asmNode.inputOperands ) {
					asmInstruction.assemblyInputConstraints.push_back( inputOperand.constraintString );
					if( inputOperand.boundExpression != nullptr ) {
						MIRVariableIdentifier inputVariable = this->lowerExpression( inputOperand.boundExpression );
						asmInstruction.assemblyInputVariables.push_back( inputVariable );
					}
				}
				asmInstruction.sourceLocation = asmNode.sourceLocation;
				this->emitInstruction( asmInstruction );
				break;
			}
			case hir::HIRNodeKind::UnsafeBlock: {
				hir::HIRUnsafeBlock& unsafeBlock = static_cast<hir::HIRUnsafeBlock&>( *hirStatement );
				if( unsafeBlock.unsafeBody != nullptr ) {
					this->lowerBlock( *unsafeBlock.unsafeBody );
				}
				break;
			}
			case hir::HIRNodeKind::Block: {
				hir::HIRBlock& innerBlock = static_cast<hir::HIRBlock&>( *hirStatement );
				this->lowerBlock( innerBlock );
				break;
			}
			case hir::HIRNodeKind::Yield: {
				hir::HIRYield& yieldNode = static_cast<hir::HIRYield&>( *hirStatement );
				MIRInstruction yieldInstruction( MIRInstructionKind::Yield );
				if( yieldNode.yieldedExpression != nullptr ) {
					MIRVariableIdentifier yieldedValue = this->lowerExpression( yieldNode.yieldedExpression );
					yieldInstruction.sourceOperands.push_back( yieldedValue );
				}
				this->emitInstruction( yieldInstruction );
				break;
			}
			case hir::HIRNodeKind::Pass:
			case hir::HIRNodeKind::Drop:
				break;
			default:
				break;
		}
	}
	
	void MIRLowering::lowerIf( hir::HIRIf& hirIf ) {
		MIRVariableIdentifier conditionVariable = this->lowerExpression( hirIf.branchCondition );
		std::shared_ptr<MIRBasicBlock> thenBlock = this->currentFunction->createBasicBlock( "if.then" );
		std::shared_ptr<MIRBasicBlock> mergeBlock = this->currentFunction->createBasicBlock( "if.merge" );
		std::shared_ptr<MIRBasicBlock> elseBlock = nullptr;
		if( hirIf.elseBranch != nullptr ) {
			elseBlock = this->currentFunction->createBasicBlock( "if.else" );
		}
		MIRInstruction branchInstruction( MIRInstructionKind::BranchConditional );
		branchInstruction.sourceOperands.push_back( conditionVariable );
		branchInstruction.trueBranchTarget = thenBlock->blockIdentifier;
		branchInstruction.falseBranchTarget = ( elseBlock != nullptr ) ? elseBlock->blockIdentifier : mergeBlock->blockIdentifier;
		branchInstruction.sourceLocation = hirIf.sourceLocation;
		this->emitTerminator( branchInstruction );
		this->switchToBlock( thenBlock );
		if( hirIf.thenBranch != nullptr ) {
			this->lowerBlock( *hirIf.thenBranch );
		}
		if( this->currentBlock->isTerminated == false ) {
			MIRInstruction jumpToMerge( MIRInstructionKind::JumpUnconditional );
			jumpToMerge.trueBranchTarget = mergeBlock->blockIdentifier;
			this->emitTerminator( jumpToMerge );
		}
		if( elseBlock != nullptr && hirIf.elseBranch != nullptr ) {
			this->switchToBlock( elseBlock );
			if( hirIf.elseBranch->nodeKind == hir::HIRNodeKind::If ) {
				hir::HIRIf& nestedIf = static_cast<hir::HIRIf&>( *hirIf.elseBranch );
				this->lowerIf( nestedIf );
			}
			else if( hirIf.elseBranch->nodeKind == hir::HIRNodeKind::Block ) {
				hir::HIRBlock& elseBodyBlock = static_cast<hir::HIRBlock&>( *hirIf.elseBranch );
				this->lowerBlock( elseBodyBlock );
			}
			if( this->currentBlock->isTerminated == false ) {
				MIRInstruction jumpToMerge( MIRInstructionKind::JumpUnconditional );
				jumpToMerge.trueBranchTarget = mergeBlock->blockIdentifier;
				this->emitTerminator( jumpToMerge );
			}
		}
		this->switchToBlock( mergeBlock );
	}
	
	void MIRLowering::lowerLoop( hir::HIRLoop& hirLoop ) {
		
		// Desugar range-for: for Type var in start..end → init + condition + body + update
		if( hirLoop.isIteratorLoop && hirLoop.iterableExpression != nullptr &&
			hirLoop.iterableExpression->nodeKind == hir::HIRNodeKind::RangeExpression ) {
			hir::HIRRangeExpression& rangeExpression = static_cast<hir::HIRRangeExpression&>( *hirLoop.iterableExpression );
			semantic::TypeSharedPointer loopVariableType = hirLoop.loopVariableType;
			if( loopVariableType == nullptr ) {
				loopVariableType = rangeExpression.rangeStart != nullptr
					? rangeExpression.rangeStart->resolvedType : nullptr;
			}
			MIRVariableIdentifier loopVariable = this->currentFunction->allocateVariable(
				hirLoop.loopVariableName, loopVariableType, true
			);
			this->variableNameMap[hirLoop.loopVariableName] = loopVariable;
			MIRInstruction allocateInstruction( MIRInstructionKind::AllocateLocal );
			allocateInstruction.destinationVariable = loopVariable;
			allocateInstruction.operandType = loopVariableType;
			allocateInstruction.sourceLocation = hirLoop.sourceLocation;
			this->emitInstruction( allocateInstruction );
			MIRVariableIdentifier startValue = this->lowerExpression( rangeExpression.rangeStart );
			MIRInstruction storeStart( MIRInstructionKind::StoreVariable );
			storeStart.destinationVariable = loopVariable;
			storeStart.sourceOperands.push_back( startValue );
			this->emitInstruction( storeStart );
			MIRVariableIdentifier endValue = this->lowerExpression( rangeExpression.rangeEnd );
			std::shared_ptr<MIRBasicBlock> headerBlock = this->currentFunction->createBasicBlock( "range.header" );
			std::shared_ptr<MIRBasicBlock> bodyBlock = this->currentFunction->createBasicBlock( "range.body" );
			std::shared_ptr<MIRBasicBlock> updateBlock = this->currentFunction->createBasicBlock( "range.update" );
			std::shared_ptr<MIRBasicBlock> exitBlock = this->currentFunction->createBasicBlock( "range.exit" );
			MIRInstruction jumpToHeader( MIRInstructionKind::JumpUnconditional );
			jumpToHeader.trueBranchTarget = headerBlock->blockIdentifier;
			this->emitTerminator( jumpToHeader );
			
			// Header: loopVar < end (or <= for inclusive)
			this->switchToBlock( headerBlock );
			MIRInstruction loadLoopVar( MIRInstructionKind::LoadVariable );
			loadLoopVar.sourceOperands.push_back( loopVariable );
			loadLoopVar.calledFunctionQualifiedName = hirLoop.loopVariableName;
			loadLoopVar.operandType = loopVariableType;
			MIRVariableIdentifier currentValue = this->currentFunction->allocateVariable( "_range_cur", loopVariableType, false );
			loadLoopVar.destinationVariable = currentValue;
			this->emitInstruction( loadLoopVar );
			MIRInstructionKind compareKind = rangeExpression.isInclusive
				? MIRInstructionKind::CompareLessEqual
				: MIRInstructionKind::CompareLessThan;
			MIRInstruction compareInstruction( compareKind );
			compareInstruction.sourceOperands.push_back( currentValue );
			compareInstruction.sourceOperands.push_back( endValue );
			MIRVariableIdentifier conditionResult = this->currentFunction->allocateVariable( "_range_cond", nullptr, false );
			compareInstruction.destinationVariable = conditionResult;
			this->emitInstruction( compareInstruction );
			MIRInstruction branchInstruction( MIRInstructionKind::BranchConditional );
			branchInstruction.sourceOperands.push_back( conditionResult );
			branchInstruction.trueBranchTarget = bodyBlock->blockIdentifier;
			branchInstruction.falseBranchTarget = exitBlock->blockIdentifier;
			this->emitTerminator( branchInstruction );
			LoopContext loopContext;
			loopContext.headerBlockIdentifier = headerBlock->blockIdentifier;
			loopContext.exitBlockIdentifier = exitBlock->blockIdentifier;
			loopContext.updateBlockIdentifier = updateBlock->blockIdentifier;
			loopContext.deferCountAtEntry = this->deferredStatements.size();
			this->loopContextStack.push( loopContext );
			this->switchToBlock( bodyBlock );
			if( hirLoop.loopBody != nullptr ) {
				this->lowerBlock( *hirLoop.loopBody );
			}
			if( this->currentBlock->isTerminated == false ) {
				for( size_t deferIndex = this->deferredStatements.size(); deferIndex > loopContext.deferCountAtEntry; --deferIndex ) {
					this->lowerStatement( this->deferredStatements[deferIndex - 1] );
				}
				MIRInstruction jumpToUpdate( MIRInstructionKind::JumpUnconditional );
				jumpToUpdate.trueBranchTarget = updateBlock->blockIdentifier;
				this->emitTerminator( jumpToUpdate );
			}

			// Update: loopVar = loopVar + 1
			this->switchToBlock( updateBlock );
			MIRInstruction reloadVar( MIRInstructionKind::LoadVariable );
			reloadVar.sourceOperands.push_back( loopVariable );
			reloadVar.calledFunctionQualifiedName = hirLoop.loopVariableName;
			reloadVar.operandType = loopVariableType;
			MIRVariableIdentifier reloadedValue = this->currentFunction->allocateVariable( "_range_reload", loopVariableType, false );
			reloadVar.destinationVariable = reloadedValue;
			this->emitInstruction( reloadVar );
			MIRInstruction oneConstant( MIRInstructionKind::ConstantInteger );
			oneConstant.integerConstantValue = 1;
			oneConstant.operandType = loopVariableType;
			MIRVariableIdentifier oneValue = this->currentFunction->allocateVariable( "_range_step", loopVariableType, false );
			oneConstant.destinationVariable = oneValue;
			this->emitInstruction( oneConstant );
			MIRInstruction incrementInstruction( MIRInstructionKind::AddInteger );
			incrementInstruction.sourceOperands.push_back( reloadedValue );
			incrementInstruction.sourceOperands.push_back( oneValue );
			incrementInstruction.operandType = loopVariableType;
			MIRVariableIdentifier incrementedValue = this->currentFunction->allocateVariable( "_range_next", loopVariableType, false );
			incrementInstruction.destinationVariable = incrementedValue;
			this->emitInstruction( incrementInstruction );
			MIRInstruction storeIncremented( MIRInstructionKind::StoreVariable );
			storeIncremented.destinationVariable = loopVariable;
			storeIncremented.sourceOperands.push_back( incrementedValue );
			this->emitInstruction( storeIncremented );
			MIRInstruction jumpBackToHeader( MIRInstructionKind::JumpUnconditional );
			jumpBackToHeader.trueBranchTarget = headerBlock->blockIdentifier;
			this->emitTerminator( jumpBackToHeader );
			this->loopContextStack.pop();
			this->switchToBlock( exitBlock );
			return;
		}
		
		// Iterator-based for-in: for Type var in iterableObject
		// Desugars to: while iterableObject.has { var = iterableObject.next; body }
		if( hirLoop.isIteratorLoop && hirLoop.iterableExpression != nullptr ) {
			MIRVariableIdentifier iterableVariable = this->lowerExpression( hirLoop.iterableExpression );
			std::string iteratorClassName;
			if( hirLoop.iterableExpression->resolvedType != nullptr ) {
				iteratorClassName = hirLoop.iterableExpression->resolvedType->qualified.empty() == false
					? hirLoop.iterableExpression->resolvedType->qualified
					: hirLoop.iterableExpression->resolvedType->name;
				size_t genericBracketPosition = iteratorClassName.find( '<' );
				if( genericBracketPosition != std::string::npos ) {
					iteratorClassName = iteratorClassName.substr( 0, genericBracketPosition );
				}
			}
			bool isStringIteration = ( iteratorClassName == "String" || iteratorClassName == "uranite.language.string.String" );
			if( isStringIteration ) {
				semantic::TypeSharedPointer charType = std::make_shared<semantic::Type>( semantic::Type::Kind::Char, "Char" );
				MIRVariableIdentifier loopVariable = this->currentFunction->allocateVariable(
					hirLoop.loopVariableName, charType, true
				);
				this->variableNameMap[hirLoop.loopVariableName] = loopVariable;
				MIRInstruction allocateChar( MIRInstructionKind::AllocateLocal );
				allocateChar.destinationVariable = loopVariable;
				allocateChar.operandType = charType;
				this->emitInstruction( allocateChar );
				semantic::TypeSharedPointer i64Type = std::make_shared<semantic::Type>( semantic::Type::Kind::Integer, "I64" );
				MIRVariableIdentifier indexVariable = this->currentFunction->allocateVariable( "_str_index", i64Type, true );
				MIRInstruction allocateIndex( MIRInstructionKind::AllocateLocal );
				allocateIndex.destinationVariable = indexVariable;
				allocateIndex.operandType = i64Type;
				this->emitInstruction( allocateIndex );
				MIRInstruction storeZero( MIRInstructionKind::ConstantInteger );
				storeZero.integerConstantValue = 0;
				storeZero.operandType = i64Type;
				MIRVariableIdentifier zeroVar = this->currentFunction->allocateVariable( "_zero", i64Type, false );
				storeZero.destinationVariable = zeroVar;
				this->emitInstruction( storeZero );
				MIRInstruction initIndex( MIRInstructionKind::StoreVariable );
				initIndex.destinationVariable = indexVariable;
				initIndex.sourceOperands.push_back( zeroVar );
				this->emitInstruction( initIndex );
				MIRInstruction lenCall( MIRInstructionKind::CallFunction );
				lenCall.calledFunctionQualifiedName = "__builtin_strlen";
				lenCall.sourceOperands.push_back( iterableVariable );
				lenCall.operandType = i64Type;
				MIRVariableIdentifier lenVariable = this->currentFunction->allocateVariable( "_str_len", i64Type, false );
				lenCall.destinationVariable = lenVariable;
				this->emitInstruction( lenCall );
				std::shared_ptr<MIRBasicBlock> headerBlock = this->currentFunction->createBasicBlock( "str.iter.header" );
				std::shared_ptr<MIRBasicBlock> bodyBlock = this->currentFunction->createBasicBlock( "str.iter.body" );
				std::shared_ptr<MIRBasicBlock> exitBlock = this->currentFunction->createBasicBlock( "str.iter.exit" );
				MIRInstruction jumpToHeader( MIRInstructionKind::JumpUnconditional );
				jumpToHeader.trueBranchTarget = headerBlock->blockIdentifier;
				this->emitTerminator( jumpToHeader );
				this->switchToBlock( headerBlock );
				MIRInstruction loadIndex( MIRInstructionKind::LoadVariable );
				loadIndex.sourceOperands.push_back( indexVariable );
				loadIndex.operandType = i64Type;
				MIRVariableIdentifier loadedIndex = this->currentFunction->allocateVariable( "_idx", i64Type, false );
				loadIndex.destinationVariable = loadedIndex;
				this->emitInstruction( loadIndex );
				MIRInstruction cmpInstruction( MIRInstructionKind::CompareLessThan );
				cmpInstruction.sourceOperands.push_back( loadedIndex );
				cmpInstruction.sourceOperands.push_back( lenVariable );
				MIRVariableIdentifier cmpResult = this->currentFunction->allocateVariable( "_cmp", nullptr, false );
				cmpInstruction.destinationVariable = cmpResult;
				this->emitInstruction( cmpInstruction );
				MIRInstruction branchInstruction( MIRInstructionKind::BranchConditional );
				branchInstruction.sourceOperands.push_back( cmpResult );
				branchInstruction.trueBranchTarget = bodyBlock->blockIdentifier;
				branchInstruction.falseBranchTarget = exitBlock->blockIdentifier;
				this->emitTerminator( branchInstruction );
				LoopContext loopContext;
				loopContext.headerBlockIdentifier = headerBlock->blockIdentifier;
				loopContext.exitBlockIdentifier = exitBlock->blockIdentifier;
				loopContext.updateBlockIdentifier = INVALID_BLOCK_IDENTIFIER;
				loopContext.deferCountAtEntry = this->deferredStatements.size();
				this->loopContextStack.push( loopContext );
				this->switchToBlock( bodyBlock );
				MIRInstruction gepInstruction( MIRInstructionKind::ComputeIndexAddress );
				gepInstruction.sourceOperands.push_back( iterableVariable );
				gepInstruction.sourceOperands.push_back( loadedIndex );
				MIRVariableIdentifier charPtr = this->currentFunction->allocateVariable( "_char_ptr", nullptr, false );
				gepInstruction.destinationVariable = charPtr;
				this->emitInstruction( gepInstruction );
				MIRInstruction loadChar( MIRInstructionKind::LoadVariable );
				loadChar.sourceOperands.push_back( charPtr );
				MIRVariableIdentifier charValue = this->currentFunction->allocateVariable( "_char_val", nullptr, false );
				loadChar.destinationVariable = charValue;
				this->emitInstruction( loadChar );
				MIRInstruction storeChar( MIRInstructionKind::StoreVariable );
				storeChar.destinationVariable = loopVariable;
				storeChar.sourceOperands.push_back( charValue );
				this->emitInstruction( storeChar );
				if( hirLoop.loopBody != nullptr ) {
					this->lowerBlock( *hirLoop.loopBody );
				}
				MIRInstruction loadIndex2( MIRInstructionKind::LoadVariable );
				loadIndex2.sourceOperands.push_back( indexVariable );
				loadIndex2.operandType = i64Type;
				MIRVariableIdentifier currentIndex = this->currentFunction->allocateVariable( "_idx2", i64Type, false );
				loadIndex2.destinationVariable = currentIndex;
				this->emitInstruction( loadIndex2 );
				MIRInstruction oneConst( MIRInstructionKind::ConstantInteger );
				oneConst.integerConstantValue = 1;
				oneConst.operandType = i64Type;
				MIRVariableIdentifier oneVar = this->currentFunction->allocateVariable( "_one", i64Type, false );
				oneConst.destinationVariable = oneVar;
				this->emitInstruction( oneConst );
				MIRInstruction addInstruction( MIRInstructionKind::AddInteger );
				addInstruction.sourceOperands.push_back( currentIndex );
				addInstruction.sourceOperands.push_back( oneVar );
				addInstruction.operandType = i64Type;
				MIRVariableIdentifier nextIndex = this->currentFunction->allocateVariable( "_next_idx", i64Type, false );
				addInstruction.destinationVariable = nextIndex;
				this->emitInstruction( addInstruction );
				MIRInstruction storeNext( MIRInstructionKind::StoreVariable );
				storeNext.destinationVariable = indexVariable;
				storeNext.sourceOperands.push_back( nextIndex );
				this->emitInstruction( storeNext );
				if( this->currentBlock->isTerminated == false ) {
					LoopContext& activeLoopContext = this->loopContextStack.top();
					for( size_t deferIndex = this->deferredStatements.size(); deferIndex > activeLoopContext.deferCountAtEntry; --deferIndex ) {
						this->lowerStatement( this->deferredStatements[deferIndex - 1] );
					}
					MIRInstruction backToHeader( MIRInstructionKind::JumpUnconditional );
					backToHeader.trueBranchTarget = headerBlock->blockIdentifier;
					this->emitTerminator( backToHeader );
				}
				this->loopContextStack.pop();
				this->switchToBlock( exitBlock );
				return;
			}
			bool isGeneratorIteration = ( 
				hirLoop.iterableExpression->resolvedType != nullptr && 
				hirLoop.iterableExpression->resolvedType->kind == semantic::Type::Kind::Generator 
			);
			if( isGeneratorIteration ) {
				std::string generatorFuncName;
				if( hirLoop.iterableExpression->nodeKind == hir::HIRNodeKind::FunctionCall ) {
					hir::HIRFunctionCall& genCall = static_cast<hir::HIRFunctionCall&>( *hirLoop.iterableExpression );
					if( genCall.calleeExpression != nullptr &&
						genCall.calleeExpression->nodeKind == hir::HIRNodeKind::Identifier ) {
						hir::HIRIdentifier& genIdent = static_cast<hir::HIRIdentifier&>( *genCall.calleeExpression );
						generatorFuncName = genIdent.identifierName;
					}
				}
				semantic::TypeSharedPointer yieldType = hirLoop.loopVariableType;
				if( yieldType == nullptr ) {
					semantic::GeneratorType* genType = static_cast<semantic::GeneratorType*>(
						hirLoop.iterableExpression->resolvedType.get()
					);
					yieldType = genType->yieldType;
				}
				MIRVariableIdentifier loopVariable = this->currentFunction->allocateVariable( hirLoop.loopVariableName, yieldType, true );
				this->variableNameMap[hirLoop.loopVariableName] = loopVariable;
				MIRInstruction allocateLoop( MIRInstructionKind::AllocateLocal );
				allocateLoop.destinationVariable = loopVariable;
				allocateLoop.operandType = yieldType;
				allocateLoop.sourceLocation = hirLoop.sourceLocation;
				this->emitInstruction( allocateLoop );
				std::shared_ptr<MIRBasicBlock> headerBlock = this->currentFunction->createBasicBlock( "gen.header" );
				std::shared_ptr<MIRBasicBlock> bodyBlock = this->currentFunction->createBasicBlock( "gen.body" );
				std::shared_ptr<MIRBasicBlock> exitBlock = this->currentFunction->createBasicBlock( "gen.exit" );
				MIRInstruction jumpToHeader( MIRInstructionKind::JumpUnconditional );
				jumpToHeader.trueBranchTarget = headerBlock->blockIdentifier;
				this->emitTerminator( jumpToHeader );
				this->switchToBlock( headerBlock );
				MIRInstruction hasCall( MIRInstructionKind::CallFunction );
				hasCall.calledFunctionQualifiedName = generatorFuncName + ".__gen_has";
				hasCall.sourceOperands.push_back( iterableVariable );
				MIRVariableIdentifier hasResult = this->currentFunction->allocateVariable( "_gen_has", nullptr, false );
				hasCall.destinationVariable = hasResult;
				this->emitInstruction( hasCall );
				MIRInstruction branchInstr( MIRInstructionKind::BranchConditional );
				branchInstr.sourceOperands.push_back( hasResult );
				branchInstr.trueBranchTarget = bodyBlock->blockIdentifier;
				branchInstr.falseBranchTarget = exitBlock->blockIdentifier;
				this->emitTerminator( branchInstr );
				LoopContext loopContext;
				loopContext.headerBlockIdentifier = headerBlock->blockIdentifier;
				loopContext.exitBlockIdentifier = exitBlock->blockIdentifier;
				loopContext.updateBlockIdentifier = INVALID_BLOCK_IDENTIFIER;
				loopContext.deferCountAtEntry = this->deferredStatements.size();
				this->loopContextStack.push( loopContext );
				this->switchToBlock( bodyBlock );
				MIRInstruction nextCall( MIRInstructionKind::CallFunction );
				nextCall.calledFunctionQualifiedName = generatorFuncName + ".__gen_next";
				nextCall.sourceOperands.push_back( iterableVariable );
				nextCall.operandType = yieldType;
				MIRVariableIdentifier nextResult = this->currentFunction->allocateVariable( "_gen_next", yieldType, false );
				nextCall.destinationVariable = nextResult;
				this->emitInstruction( nextCall );
				MIRInstruction storeVal( MIRInstructionKind::StoreVariable );
				storeVal.destinationVariable = loopVariable;
				storeVal.sourceOperands.push_back( nextResult );
				this->emitInstruction( storeVal );
				if( hirLoop.loopBody != nullptr ) {
					this->lowerBlock( *hirLoop.loopBody );
				}
				if( this->currentBlock->isTerminated == false ) {
					LoopContext& activeLoopContext = this->loopContextStack.top();
					for( size_t deferIndex = this->deferredStatements.size(); deferIndex > activeLoopContext.deferCountAtEntry; --deferIndex ) {
						this->lowerStatement( this->deferredStatements[deferIndex - 1] );
					}
					MIRInstruction backToHeader( MIRInstructionKind::JumpUnconditional );
					backToHeader.trueBranchTarget = headerBlock->blockIdentifier;
					this->emitTerminator( backToHeader );
				}
				this->loopContextStack.pop();
				this->switchToBlock( exitBlock );
				return;
			}
			bool hasDualVariable = hirLoop.loopVariableName2.empty() == false;
			semantic::TypeSharedPointer loopVariableType = hirLoop.loopVariableType;
			if( loopVariableType == nullptr && hirLoop.iterableExpression->resolvedType != nullptr ) {
				loopVariableType = hirLoop.iterableExpression->resolvedType;
			}
			semantic::TypeSharedPointer iterableResolvedType = hirLoop.iterableExpression->resolvedType;
			if( iterableResolvedType == nullptr || iterableResolvedType->kind != semantic::Type::Kind::Class ) {
				return;
			}
			semantic::ClassTypeSharedPointer iterableClassType = std::static_pointer_cast<semantic::ClassType>( iterableResolvedType );
			MIRVariableIdentifier iteratorVariable = iterableVariable;
			std::string iteratorMethodClassName = iteratorClassName;
			semantic::TypeSharedPointer iteratorSemaType = iterableResolvedType;
			if( iterableClassType->implementsInterface( semantic::qname::ITERATOR ) ) {
				iteratorVariable = iterableVariable;
				iteratorMethodClassName = iteratorClassName;
				iteratorSemaType = iterableResolvedType;
			}
			else if( iterableClassType->implementsInterface( semantic::qname::ITERABLE ) ) {
				semantic::MethodInfo* iteratorMethodInfo = iterableClassType->findMethod( "iterator" );
				if( iteratorMethodInfo == nullptr ) {
					return;
				}
				semantic::TypeSharedPointer iteratorReturnType = nullptr;
				if( iteratorMethodInfo->type != nullptr && iteratorMethodInfo->type->kind == semantic::Type::Kind::Function ) {
					iteratorReturnType = std::static_pointer_cast<semantic::FunctionType>( iteratorMethodInfo->type )->returnType;
				}
				MIRInstruction iteratorCall( MIRInstructionKind::CallFunction );
				iteratorCall.calledFunctionQualifiedName = iteratorClassName + ".iterator";
				iteratorCall.sourceOperands.push_back( iterableVariable );
				iteratorCall.operandType = iteratorReturnType;
				MIRVariableIdentifier iteratorResult = this->currentFunction->allocateVariable( "_iterator_obj", iteratorReturnType, false );
				iteratorCall.destinationVariable = iteratorResult;
				this->emitInstruction( iteratorCall );
				iteratorVariable = iteratorResult;
				if( iteratorReturnType != nullptr ) {
					iteratorSemaType = iteratorReturnType;
					std::string iterRetName = iteratorReturnType->name;
					size_t iterRetGenBracket = iterRetName.find( '<' );
					if( iterRetGenBracket != std::string::npos ) {
						iterRetName = iterRetName.substr( 0, iterRetGenBracket );
					}
					iteratorMethodClassName = iterRetName;
				}
			}
			else {
				return;
			}
			semantic::TypeSharedPointer nextReturnType = loopVariableType;
			if( hasDualVariable || nextReturnType == nullptr ) {
				semantic::MethodInfo* nextMethodInfo = nullptr;
				if( iteratorSemaType->kind == semantic::Type::Kind::Class ) {
					nextMethodInfo = std::static_pointer_cast<semantic::ClassType>( iteratorSemaType )->findMethod( "next" );
				}
				else if( iteratorSemaType->kind == semantic::Type::Kind::Interface ) {
					nextMethodInfo = std::static_pointer_cast<semantic::InterfaceType>( iteratorSemaType )->findMethod( "next" );
				}
				if( nextMethodInfo != nullptr && nextMethodInfo->type != nullptr && nextMethodInfo->type->kind == semantic::Type::Kind::Function ) {
					nextReturnType = std::static_pointer_cast<semantic::FunctionType>( nextMethodInfo->type )->returnType;
				}
			}
			if( hasDualVariable && nextReturnType != nullptr && nextReturnType->kind == semantic::Type::Kind::Struct ) {
				semantic::StructTypeSharedPointer pairStructType = std::static_pointer_cast<semantic::StructType>( nextReturnType );
				if( pairStructType->fields.size() >= 2 ) {
					std::unordered_map<std::string, semantic::TypeSharedPointer> resolveMap;
					if( iteratorSemaType != nullptr && iteratorSemaType->kind == semantic::Type::Kind::Class ) {
						semantic::ClassTypeSharedPointer iterClassForSubs = std::static_pointer_cast<semantic::ClassType>( iteratorSemaType );
						resolveMap = iterClassForSubs->typeSubstitutions;
					}
					std::string pairLayoutName = pairStructType->name;
					TypeLayoutDescriptor pairLayout;
					int pairFieldIndex = 0;
					for( const semantic::FieldInfo& pairField : pairStructType->fields ) {
						pairLayout.fieldByteOffsets.push_back( pairFieldIndex * 8 );
						pairLayout.fieldNames.push_back( pairField.name );
						semantic::TypeSharedPointer resolvedFieldType = pairField.type;
						if( resolvedFieldType != nullptr && resolvedFieldType->kind == semantic::Type::Kind::GenericParameter ) {
							std::unordered_map<std::string, semantic::TypeSharedPointer>::iterator substitution = resolveMap.find( resolvedFieldType->name );
							if( substitution != resolveMap.end() ) {
								resolvedFieldType = substitution->second;
							}
						}
						pairLayout.fieldTypes.push_back( resolvedFieldType );
						pairFieldIndex++;
					}
					pairLayout.typeSizeInBytes = pairFieldIndex * 8;
					pairLayout.typeAlignmentInBytes = 8;
					this->currentModule->typeLayoutTable[pairLayoutName] = std::move( pairLayout );
				}
			}
			MIRVariableIdentifier loopVariable = this->currentFunction->allocateVariable( hirLoop.loopVariableName, loopVariableType, true );
			this->variableNameMap[hirLoop.loopVariableName] = loopVariable;
			MIRInstruction allocateInstruction( MIRInstructionKind::AllocateLocal );
			allocateInstruction.destinationVariable = loopVariable;
			allocateInstruction.operandType = loopVariableType;
			allocateInstruction.sourceLocation = hirLoop.sourceLocation;
			this->emitInstruction( allocateInstruction );
			MIRVariableIdentifier loopVariable2 = INVALID_VARIABLE_IDENTIFIER;
			if( hasDualVariable ) {
				semantic::TypeSharedPointer loopVariableType2 = hirLoop.loopVariableType2;
				if( loopVariableType2 == nullptr ) {
					loopVariableType2 = loopVariableType;
				}
				loopVariable2 = this->currentFunction->allocateVariable(
					hirLoop.loopVariableName2, loopVariableType2, true
				);
				this->variableNameMap[hirLoop.loopVariableName2] = loopVariable2;
				MIRInstruction allocateInstruction2( MIRInstructionKind::AllocateLocal );
				allocateInstruction2.destinationVariable = loopVariable2;
				allocateInstruction2.operandType = loopVariableType2;
				allocateInstruction2.sourceLocation = hirLoop.sourceLocation;
				this->emitInstruction( allocateInstruction2 );
			}
			std::shared_ptr<MIRBasicBlock> headerBlock = this->currentFunction->createBasicBlock( "iter.header" );
			std::shared_ptr<MIRBasicBlock> bodyBlock = this->currentFunction->createBasicBlock( "iter.body" );
			std::shared_ptr<MIRBasicBlock> exitBlock = this->currentFunction->createBasicBlock( "iter.exit" );
			MIRInstruction jumpToHeader( MIRInstructionKind::JumpUnconditional );
			jumpToHeader.trueBranchTarget = headerBlock->blockIdentifier;
			this->emitTerminator( jumpToHeader );
			this->switchToBlock( headerBlock );
			MIRInstruction hasCall( MIRInstructionKind::CallFunction );
			hasCall.calledFunctionQualifiedName = iteratorMethodClassName + ".has";
			hasCall.sourceOperands.push_back( iteratorVariable );
			MIRVariableIdentifier hasResult = this->currentFunction->allocateVariable( "_iter_has", nullptr, false );
			hasCall.destinationVariable = hasResult;
			this->emitInstruction( hasCall );
			MIRInstruction branchInstruction( MIRInstructionKind::BranchConditional );
			branchInstruction.sourceOperands.push_back( hasResult );
			branchInstruction.trueBranchTarget = bodyBlock->blockIdentifier;
			branchInstruction.falseBranchTarget = exitBlock->blockIdentifier;
			this->emitTerminator( branchInstruction );
			LoopContext loopContext;
			loopContext.headerBlockIdentifier = headerBlock->blockIdentifier;
			loopContext.exitBlockIdentifier = exitBlock->blockIdentifier;
			loopContext.updateBlockIdentifier = INVALID_BLOCK_IDENTIFIER;
			loopContext.deferCountAtEntry = this->deferredStatements.size();
			this->loopContextStack.push( loopContext );
			this->switchToBlock( bodyBlock );
			MIRInstruction nextCall( MIRInstructionKind::CallFunction );
			nextCall.calledFunctionQualifiedName = iteratorMethodClassName + ".next";
			nextCall.sourceOperands.push_back( iteratorVariable );
			nextCall.operandType = nextReturnType;
			MIRVariableIdentifier nextResult = this->currentFunction->allocateVariable( "_iter_next", nextReturnType, false );
			nextCall.destinationVariable = nextResult;
			this->emitInstruction( nextCall );
			if( hasDualVariable ) {
				MIRInstruction keyGep( MIRInstructionKind::ComputeFieldAddress );
				keyGep.sourceOperands.push_back( nextResult );
				keyGep.fieldAccessName = "key";
				keyGep.fieldLayoutIndex = 0;
				keyGep.operandType = loopVariableType;
				keyGep.sourceLocation = hirLoop.sourceLocation;
				MIRVariableIdentifier keyFieldPtr = this->currentFunction->allocateVariable( "_pair_key_ptr", loopVariableType, false );
				keyGep.destinationVariable = keyFieldPtr;
				this->emitInstruction( keyGep );
				MIRInstruction loadKey( MIRInstructionKind::LoadVariable );
				loadKey.sourceOperands.push_back( keyFieldPtr );
				loadKey.operandType = loopVariableType;
				MIRVariableIdentifier keyValue = this->currentFunction->allocateVariable( "_pair_key", loopVariableType, false );
				loadKey.destinationVariable = keyValue;
				this->emitInstruction( loadKey );
				MIRInstruction storeKey( MIRInstructionKind::StoreVariable );
				storeKey.destinationVariable = loopVariable;
				storeKey.sourceOperands.push_back( keyValue );
				this->emitInstruction( storeKey );
				semantic::TypeSharedPointer loopVariableType2 = hirLoop.loopVariableType2;
				if( loopVariableType2 == nullptr ) {
					loopVariableType2 = loopVariableType;
				}
				MIRInstruction valueGep( MIRInstructionKind::ComputeFieldAddress );
				valueGep.sourceOperands.push_back( nextResult );
				valueGep.fieldAccessName = "value";
				valueGep.fieldLayoutIndex = 1;
				valueGep.operandType = loopVariableType2;
				valueGep.sourceLocation = hirLoop.sourceLocation;
				MIRVariableIdentifier valueFieldPtr = this->currentFunction->allocateVariable( "_pair_val_ptr", loopVariableType2, false );
				valueGep.destinationVariable = valueFieldPtr;
				this->emitInstruction( valueGep );
				MIRInstruction loadValue( MIRInstructionKind::LoadVariable );
				loadValue.sourceOperands.push_back( valueFieldPtr );
				loadValue.operandType = loopVariableType2;
				MIRVariableIdentifier valueResult = this->currentFunction->allocateVariable( "_pair_val", loopVariableType2, false );
				loadValue.destinationVariable = valueResult;
				this->emitInstruction( loadValue );
				MIRInstruction storeValue( MIRInstructionKind::StoreVariable );
				storeValue.destinationVariable = loopVariable2;
				storeValue.sourceOperands.push_back( valueResult );
				this->emitInstruction( storeValue );
			}
			else {
				MIRInstruction storeNext( MIRInstructionKind::StoreVariable );
				storeNext.destinationVariable = loopVariable;
				storeNext.sourceOperands.push_back( nextResult );
				this->emitInstruction( storeNext );
			}
			if( hirLoop.loopBody != nullptr ) {
				this->lowerBlock( *hirLoop.loopBody );
			}
			if( this->currentBlock->isTerminated == false ) {
				LoopContext& activeLoopContext = this->loopContextStack.top();
				for( size_t deferIndex = this->deferredStatements.size(); deferIndex > activeLoopContext.deferCountAtEntry; --deferIndex ) {
					this->lowerStatement( this->deferredStatements[deferIndex - 1] );
				}
				MIRInstruction jumpBack( MIRInstructionKind::JumpUnconditional );
				jumpBack.trueBranchTarget = headerBlock->blockIdentifier;
				this->emitTerminator( jumpBack );
			}
			this->loopContextStack.pop();
			this->switchToBlock( exitBlock );
			return;
		}
		std::shared_ptr<MIRBasicBlock> headerBlock = this->currentFunction->createBasicBlock( "loop.header" );
		std::shared_ptr<MIRBasicBlock> bodyBlock = this->currentFunction->createBasicBlock( "loop.body" );
		std::shared_ptr<MIRBasicBlock> exitBlock = this->currentFunction->createBasicBlock( "loop.exit" );
		std::shared_ptr<MIRBasicBlock> updateBlock = nullptr;
		if( hirLoop.loopUpdate != nullptr ) {
			updateBlock = this->currentFunction->createBasicBlock( "loop.update" );
		}
		if( hirLoop.loopVariableName.empty() == false && hirLoop.loopInitializer != nullptr ) {
			semantic::TypeSharedPointer loopVariableType = hirLoop.loopVariableType;
			if( loopVariableType == nullptr && hirLoop.loopInitializer->resolvedType != nullptr ) {
				loopVariableType = hirLoop.loopInitializer->resolvedType;
			}
			MIRVariableIdentifier loopVariable = this->currentFunction->allocateVariable( hirLoop.loopVariableName, loopVariableType, true );
			this->variableNameMap[hirLoop.loopVariableName] = loopVariable;
			MIRInstruction allocateInstruction( MIRInstructionKind::AllocateLocal );
			allocateInstruction.destinationVariable = loopVariable;
			allocateInstruction.operandType = loopVariableType;
			allocateInstruction.sourceLocation = hirLoop.sourceLocation;
			this->emitInstruction( allocateInstruction );
			MIRVariableIdentifier initValue = this->lowerExpression( hirLoop.loopInitializer );
			MIRInstruction storeInit( MIRInstructionKind::StoreVariable );
			storeInit.destinationVariable = loopVariable;
			storeInit.sourceOperands.push_back( initValue );
			this->emitInstruction( storeInit );
		}
		else if( hirLoop.loopInitializer != nullptr ) {
			this->lowerExpression( hirLoop.loopInitializer );
		}
		MIRInstruction jumpToHeader( MIRInstructionKind::JumpUnconditional );
		jumpToHeader.trueBranchTarget = headerBlock->blockIdentifier;
		this->emitTerminator( jumpToHeader );
		this->switchToBlock( headerBlock );
		if( hirLoop.loopCondition != nullptr ) {
			MIRVariableIdentifier conditionVariable = this->lowerExpression( hirLoop.loopCondition );
			MIRInstruction branchInstruction( MIRInstructionKind::BranchConditional );
			branchInstruction.sourceOperands.push_back( conditionVariable );
			branchInstruction.trueBranchTarget = bodyBlock->blockIdentifier;
			branchInstruction.falseBranchTarget = exitBlock->blockIdentifier;
			branchInstruction.sourceLocation = hirLoop.sourceLocation;
			this->emitTerminator( branchInstruction );
		}
		else {
			MIRInstruction jumpToBody( MIRInstructionKind::JumpUnconditional );
			jumpToBody.trueBranchTarget = bodyBlock->blockIdentifier;
			this->emitTerminator( jumpToBody );
		}
		LoopContext loopContext;
		loopContext.headerBlockIdentifier = headerBlock->blockIdentifier;
		loopContext.exitBlockIdentifier = exitBlock->blockIdentifier;
		loopContext.updateBlockIdentifier = ( updateBlock != nullptr ) ? updateBlock->blockIdentifier : INVALID_BLOCK_IDENTIFIER;
		loopContext.deferCountAtEntry = this->deferredStatements.size();
		this->loopContextStack.push( loopContext );
		this->switchToBlock( bodyBlock );
		if( hirLoop.loopBody != nullptr ) {
			this->lowerBlock( *hirLoop.loopBody );
		}
		if( this->currentBlock->isTerminated == false ) {
			for( size_t deferIndex = this->deferredStatements.size(); deferIndex > loopContext.deferCountAtEntry; --deferIndex ) {
				this->lowerStatement( this->deferredStatements[deferIndex - 1] );
			}
			MIRBlockIdentifier backEdgeTarget = ( updateBlock != nullptr ) ? updateBlock->blockIdentifier : headerBlock->blockIdentifier;
			MIRInstruction jumpBack( MIRInstructionKind::JumpUnconditional );
			jumpBack.trueBranchTarget = backEdgeTarget;
			this->emitTerminator( jumpBack );
		}
		if( updateBlock != nullptr ) {
			this->switchToBlock( updateBlock );
			if( hirLoop.loopUpdate != nullptr ) {
				this->lowerStatement( hirLoop.loopUpdate );
			}
			if( this->currentBlock->isTerminated == false ) {
				MIRInstruction jumpToHeaderFromUpdate( MIRInstructionKind::JumpUnconditional );
				jumpToHeaderFromUpdate.trueBranchTarget = headerBlock->blockIdentifier;
				this->emitTerminator( jumpToHeaderFromUpdate );
			}
		}
		this->loopContextStack.pop();
		this->switchToBlock( exitBlock );
	}
	
	void MIRLowering::lowerMatch( hir::HIRMatch& hirMatch ) {
		MIRVariableIdentifier subjectVariable = this->lowerExpression( hirMatch.matchSubject );
		std::shared_ptr<MIRBasicBlock> mergeBlock = this->currentFunction->createBasicBlock( "match.merge" );
		MIRInstruction switchInstruction( MIRInstructionKind::SwitchBranch );
		switchInstruction.sourceOperands.push_back( subjectVariable );
		switchInstruction.defaultSwitchTarget = mergeBlock->blockIdentifier;
		switchInstruction.sourceLocation = hirMatch.sourceLocation;
		std::vector<std::shared_ptr<MIRBasicBlock>> armBlocks;
		for( size_t armIndex = 0; armIndex < hirMatch.matchArms.size(); armIndex++ ) {
			std::shared_ptr<MIRBasicBlock> armBlock = this->currentFunction->createBasicBlock(
				fmt::format( "match.arm.{}", armIndex )
			);
			armBlocks.push_back( armBlock );
			switchInstruction.switchBranchTargets.push_back( { static_cast<int64_t>( armIndex ), armBlock->blockIdentifier } );
		}
		this->emitTerminator( switchInstruction );
		for( size_t armIndex = 0; armIndex < hirMatch.matchArms.size(); armIndex++ ) {
			this->switchToBlock( armBlocks[armIndex] );
			hir::HIRMatchArm& arm = hirMatch.matchArms[armIndex];
			if( arm.armBody != nullptr ) {
				this->lowerBlock( *arm.armBody );
			}
			if( this->currentBlock->isTerminated == false ) {
				MIRInstruction jumpToMerge( MIRInstructionKind::JumpUnconditional );
				jumpToMerge.trueBranchTarget = mergeBlock->blockIdentifier;
				this->emitTerminator( jumpToMerge );
			}
		}
		this->switchToBlock( mergeBlock );
	}
	
	void MIRLowering::lowerSwitch( hir::HIRSwitch& hirSwitch ) {
		MIRVariableIdentifier subjectVariable = this->lowerExpression( hirSwitch.switchSubject );
		std::shared_ptr<MIRBasicBlock> mergeBlock = this->currentFunction->createBasicBlock( "switch.merge" );
		MIRInstruction switchInstruction( MIRInstructionKind::SwitchBranch );
		switchInstruction.sourceOperands.push_back( subjectVariable );
		switchInstruction.defaultSwitchTarget = mergeBlock->blockIdentifier;
		switchInstruction.sourceLocation = hirSwitch.sourceLocation;
		std::vector<std::shared_ptr<MIRBasicBlock>> caseBlocks;
		for( size_t caseIndex = 0; caseIndex < hirSwitch.switchCases.size(); caseIndex++ ) {
			std::shared_ptr<MIRBasicBlock> caseBlock = this->currentFunction->createBasicBlock(
				fmt::format( "switch.case.{}", caseIndex )
			);
			caseBlocks.push_back( caseBlock );
			hir::HIRSwitchCase& switchCase = hirSwitch.switchCases[caseIndex];
			if( switchCase.isDefaultCase ) {
				switchInstruction.defaultSwitchTarget = caseBlock->blockIdentifier;
			}
			else {
				switchInstruction.switchBranchTargets.push_back( { static_cast<int64_t>( caseIndex ), caseBlock->blockIdentifier } );
			}
		}
		this->emitTerminator( switchInstruction );
		for( size_t caseIndex = 0; caseIndex < hirSwitch.switchCases.size(); caseIndex++ ) {
			this->switchToBlock( caseBlocks[caseIndex] );
			hir::HIRSwitchCase& switchCase = hirSwitch.switchCases[caseIndex];
			if( switchCase.caseBody != nullptr ) {
				this->lowerBlock( *switchCase.caseBody );
			}
			if( this->currentBlock->isTerminated == false ) {
				MIRInstruction jumpToMerge( MIRInstructionKind::JumpUnconditional );
				jumpToMerge.trueBranchTarget = mergeBlock->blockIdentifier;
				this->emitTerminator( jumpToMerge );
			}
		}
		this->switchToBlock( mergeBlock );
	}
	
	void MIRLowering::lowerTryCatch( hir::HIRTryCatch& hirTryCatch ) {
		std::shared_ptr<MIRBasicBlock> tryBlock = this->currentFunction->createBasicBlock( "try.body" );
		std::shared_ptr<MIRBasicBlock> mergeBlock = this->currentFunction->createBasicBlock( "try.merge" );
		std::shared_ptr<MIRBasicBlock> landingPadBlock = this->currentFunction->createBasicBlock( "try.landing" );
		MIRInstruction jumpToTry( MIRInstructionKind::JumpUnconditional );
		jumpToTry.trueBranchTarget = tryBlock->blockIdentifier;
		this->emitTerminator( jumpToTry );
		std::shared_ptr<MIRBasicBlock> finallyBlock = nullptr;
		MIRBlockIdentifier postTryCatchTarget = mergeBlock->blockIdentifier;
		if( hirTryCatch.finallyBody != nullptr ) {
			finallyBlock = this->currentFunction->createBasicBlock( "finally" );
			postTryCatchTarget = finallyBlock->blockIdentifier;
		}
		this->switchToBlock( tryBlock );
		MIRBlockIdentifier previousLandingPad = this->activeLandingPad;
		this->activeLandingPad = landingPadBlock->blockIdentifier;
		if( hirTryCatch.tryBody != nullptr ) {
			this->lowerBlock( *hirTryCatch.tryBody );
		}
		this->activeLandingPad = previousLandingPad;
		if( this->currentBlock->isTerminated == false ) {
			MIRInstruction jumpAfterTry( MIRInstructionKind::JumpUnconditional );
			jumpAfterTry.trueBranchTarget = postTryCatchTarget;
			this->emitTerminator( jumpAfterTry );
		}
		this->switchToBlock( landingPadBlock );
		MIRInstruction landingPadInstruction( MIRInstructionKind::LandingPad );
		landingPadInstruction.sourceLocation = hirTryCatch.sourceLocation;
		MIRVariableIdentifier exceptionVariable = this->currentFunction->allocateVariable( "_caught_exception", nullptr, false );
		landingPadInstruction.destinationVariable = exceptionVariable;
		this->emitInstruction( landingPadInstruction );
		for( size_t handlerIndex = 0; handlerIndex < hirTryCatch.exceptionHandlers.size(); handlerIndex++ ) {
			hir::HIRExceptionHandler& handler = hirTryCatch.exceptionHandlers[handlerIndex];
			std::shared_ptr<MIRBasicBlock> handlerBlock = this->currentFunction->createBasicBlock(
				fmt::format( "catch.handler.{}", handlerIndex )
			);
			if( this->currentBlock->isTerminated == false ) {
				MIRInstruction jumpToHandler( MIRInstructionKind::JumpUnconditional );
				jumpToHandler.trueBranchTarget = handlerBlock->blockIdentifier;
				this->emitTerminator( jumpToHandler );
			}
			this->switchToBlock( handlerBlock );
			if( handler.exceptionVariableName.empty() == false ) {
				MIRVariableIdentifier handlerVariable = this->currentFunction->allocateVariable(
					handler.exceptionVariableName, nullptr, false
				);
				MIRInstruction copyException( MIRInstructionKind::CopyValue );
				copyException.destinationVariable = handlerVariable;
				copyException.sourceOperands.push_back( exceptionVariable );
				this->emitInstruction( copyException );
			}
			if( handler.handlerBody != nullptr ) {
				this->lowerBlock( *handler.handlerBody );
			}
			if( this->currentBlock->isTerminated == false ) {
				MIRInstruction jumpAfterCatch( MIRInstructionKind::JumpUnconditional );
				jumpAfterCatch.trueBranchTarget = postTryCatchTarget;
				this->emitTerminator( jumpAfterCatch );
			}
		}
		if( finallyBlock != nullptr ) {
			this->switchToBlock( finallyBlock );
			this->lowerBlock( *hirTryCatch.finallyBody );
			if( this->currentBlock->isTerminated == false ) {
				MIRInstruction jumpToMerge( MIRInstructionKind::JumpUnconditional );
				jumpToMerge.trueBranchTarget = mergeBlock->blockIdentifier;
				this->emitTerminator( jumpToMerge );
			}
		}
		this->switchToBlock( mergeBlock );
	}
	
	void MIRLowering::lowerDefer( hir::HIRDefer& hirDefer ) {
		if( hirDefer.deferredStatement != nullptr ) {
			this->deferredStatements.push_back( hirDefer.deferredStatement );
		}
	}
	
	void MIRLowering::emitDeferredStatements() {
		for( std::vector<hir::HIRNodeSharedPointer>::reverse_iterator deferIterator = this->deferredStatements.rbegin();
			 deferIterator != this->deferredStatements.rend(); ++deferIterator ) {
			this->lowerStatement( *deferIterator );
		}
	}
	
	MIRVariableIdentifier MIRLowering::lowerExpression( const hir::HIRNodeSharedPointer& hirExpression ) {
		if( hirExpression == nullptr ) {
			return INVALID_VARIABLE_IDENTIFIER;
		}
		switch( hirExpression->nodeKind ) {
			case hir::HIRNodeKind::IntegerLiteral: {
				hir::HIRIntegerLiteral& integerLiteral = static_cast<hir::HIRIntegerLiteral&>( *hirExpression );
				MIRInstruction constantInstruction( MIRInstructionKind::ConstantInteger );
				constantInstruction.integerConstantValue = integerLiteral.integerValue;
				constantInstruction.operandType = integerLiteral.resolvedType;
				constantInstruction.sourceLocation = integerLiteral.sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable( "_const_int", integerLiteral.resolvedType, false );
				constantInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( constantInstruction );
			}
			case hir::HIRNodeKind::FloatLiteral: {
				hir::HIRFloatLiteral& floatLiteral = static_cast<hir::HIRFloatLiteral&>( *hirExpression );
				MIRInstruction constantInstruction( MIRInstructionKind::ConstantFloat );
				constantInstruction.floatConstantValue = floatLiteral.floatValue;
				constantInstruction.operandType = floatLiteral.resolvedType;
				constantInstruction.sourceLocation = floatLiteral.sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable( "_const_float", floatLiteral.resolvedType, false );
				constantInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( constantInstruction );
			}
			case hir::HIRNodeKind::BooleanLiteral: {
				hir::HIRBooleanLiteral& boolLiteral = static_cast<hir::HIRBooleanLiteral&>( *hirExpression );
				MIRInstruction constantInstruction( MIRInstructionKind::ConstantBoolean );
				constantInstruction.booleanConstantValue = boolLiteral.booleanValue;
				constantInstruction.operandType = boolLiteral.resolvedType;
				constantInstruction.sourceLocation = boolLiteral.sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable( "_const_bool", boolLiteral.resolvedType, false );
				constantInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( constantInstruction );
			}
			case hir::HIRNodeKind::StringLiteral: {
				hir::HIRStringLiteral& stringLiteral = static_cast<hir::HIRStringLiteral&>( *hirExpression );
				MIRInstruction constantInstruction( MIRInstructionKind::ConstantString );
				constantInstruction.stringConstantValue = stringLiteral.stringValue;
				constantInstruction.operandType = stringLiteral.resolvedType;
				constantInstruction.sourceLocation = stringLiteral.sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable( "_const_str", stringLiteral.resolvedType, false );
				constantInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( constantInstruction );
			}
			case hir::HIRNodeKind::CharLiteral: {
				hir::HIRCharLiteral& charLiteral = static_cast<hir::HIRCharLiteral&>( *hirExpression );
				MIRInstruction constantInstruction( MIRInstructionKind::ConstantChar );
				constantInstruction.charConstantValue = charLiteral.charValue;
				constantInstruction.operandType = charLiteral.resolvedType;
				constantInstruction.sourceLocation = charLiteral.sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable( "_const_char", charLiteral.resolvedType, false );
				constantInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( constantInstruction );
			}
			case hir::HIRNodeKind::NoneLiteral: {
				MIRInstruction constantInstruction( MIRInstructionKind::ConstantNone );
				constantInstruction.operandType = hirExpression->resolvedType;
				constantInstruction.sourceLocation = hirExpression->sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable( "_const_none", hirExpression->resolvedType, false );
				constantInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( constantInstruction );
			}
			case hir::HIRNodeKind::Identifier: {
				hir::HIRIdentifier& identifier = static_cast<hir::HIRIdentifier&>( *hirExpression );
				std::unordered_map<std::string, MIRVariableIdentifier>::iterator variableLookup =
					this->variableNameMap.find( identifier.identifierName );
				if( variableLookup == this->variableNameMap.end() ) {
					std::unordered_map<std::string, MIRModuleConstant>::iterator constantLookup =
						this->currentModule->moduleConstants.find( identifier.identifierName );
					if( constantLookup != this->currentModule->moduleConstants.end() ) {
						MIRModuleConstant& moduleConstant = constantLookup->second;
						MIRInstruction constantInstruction( MIRInstructionKind::ConstantInteger );
						if( moduleConstant.kind == MIRModuleConstant::Integer ) {
							constantInstruction.instructionKind = MIRInstructionKind::ConstantInteger;
							constantInstruction.integerConstantValue = moduleConstant.integerValue;
						}
						else if( moduleConstant.kind == MIRModuleConstant::Float ) {
							constantInstruction.instructionKind = MIRInstructionKind::ConstantFloat;
							constantInstruction.floatConstantValue = moduleConstant.floatValue;
						}
						else if( moduleConstant.kind == MIRModuleConstant::Boolean ) {
							constantInstruction.instructionKind = MIRInstructionKind::ConstantBoolean;
							constantInstruction.booleanConstantValue = moduleConstant.booleanValue;
						}
						else if( moduleConstant.kind == MIRModuleConstant::String ) {
							constantInstruction.instructionKind = MIRInstructionKind::ConstantString;
							constantInstruction.stringConstantValue = moduleConstant.stringValue;
						}
						constantInstruction.operandType = identifier.resolvedType;
						constantInstruction.sourceLocation = identifier.sourceLocation;
						MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
							identifier.identifierName, identifier.resolvedType, false
						);
						constantInstruction.destinationVariable = resultVariable;
						return this->emitInstruction( constantInstruction );
					}
					std::unordered_map<std::string, MIRGlobalVariable>::iterator globalLookup =
						this->currentModule->globalVariables.find( identifier.identifierName );
					if( globalLookup != this->currentModule->globalVariables.end() ) {
						MIRInstruction loadInstruction( MIRInstructionKind::LoadVariable );
						loadInstruction.calledFunctionQualifiedName = fmt::format( "@{}", identifier.identifierName );
						loadInstruction.operandType = identifier.resolvedType;
						loadInstruction.sourceLocation = identifier.sourceLocation;
						MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
							identifier.identifierName, identifier.resolvedType, false
						);
						loadInstruction.destinationVariable = resultVariable;
						return this->emitInstruction( loadInstruction );
					}
				}
				MIRInstruction loadInstruction( MIRInstructionKind::LoadVariable );
				loadInstruction.calledFunctionQualifiedName = identifier.identifierName;
				semantic::TypeSharedPointer resolvedType = identifier.resolvedType;
				if( variableLookup != this->variableNameMap.end() ) {
					loadInstruction.sourceOperands.push_back( variableLookup->second );
					if( resolvedType == nullptr ) {
						MIRVariableIdentifier sourceVariableId = variableLookup->second;
						if( this->currentFunction->variableDescriptorTable.count( sourceVariableId ) > 0 ) {
							resolvedType = this->currentFunction->variableDescriptorTable[sourceVariableId].variableType;
						}
					}
				}
				loadInstruction.operandType = resolvedType;
				loadInstruction.sourceLocation = identifier.sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
					identifier.identifierName, resolvedType, false
				);
				loadInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( loadInstruction );
			}
			case hir::HIRNodeKind::SelfReference: {
				MIRInstruction loadInstruction( MIRInstructionKind::LoadVariable );
				loadInstruction.calledFunctionQualifiedName = "self";
				semantic::TypeSharedPointer selfResolvedType = hirExpression->resolvedType;
				std::unordered_map<std::string, MIRVariableIdentifier>::iterator selfLookup =
					this->variableNameMap.find( "self" );
				if( selfLookup != this->variableNameMap.end() ) {
					loadInstruction.sourceOperands.push_back( selfLookup->second );
					if( selfResolvedType == nullptr ) {
						MIRVariableIdentifier selfSourceId = selfLookup->second;
						if( this->currentFunction->variableDescriptorTable.count( selfSourceId ) > 0 ) {
							selfResolvedType = this->currentFunction->variableDescriptorTable[selfSourceId].variableType;
						}
					}
				}
				loadInstruction.operandType = selfResolvedType;
				loadInstruction.sourceLocation = hirExpression->sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable( "self", selfResolvedType, false );
				loadInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( loadInstruction );
			}
			case hir::HIRNodeKind::BinaryOperation: {
				hir::HIRBinaryOperation& binaryOp = static_cast<hir::HIRBinaryOperation&>( *hirExpression );
				return this->lowerBinaryOperation( binaryOp );
			}
			case hir::HIRNodeKind::UnaryOperation: {
				hir::HIRUnaryOperation& unaryOp = static_cast<hir::HIRUnaryOperation&>( *hirExpression );
				return this->lowerUnaryOperation( unaryOp );
			}
			case hir::HIRNodeKind::FunctionCall: {
				hir::HIRFunctionCall& callExpression = static_cast<hir::HIRFunctionCall&>( *hirExpression );
				return this->lowerFunctionCall( callExpression );
			}
			case hir::HIRNodeKind::MethodCall: {
				hir::HIRMethodCall& methodCall = static_cast<hir::HIRMethodCall&>( *hirExpression );
				return this->lowerMethodCall( methodCall );
			}
			case hir::HIRNodeKind::FieldAccess: {
				hir::HIRFieldAccess& fieldAccess = static_cast<hir::HIRFieldAccess&>( *hirExpression );
				return this->lowerFieldAccess( fieldAccess );
			}
			case hir::HIRNodeKind::IndexAccess: {
				hir::HIRIndexAccess& indexAccess = static_cast<hir::HIRIndexAccess&>( *hirExpression );
				return this->lowerIndexAccess( indexAccess );
			}
			case hir::HIRNodeKind::Construct: {
				hir::HIRConstruct& construct = static_cast<hir::HIRConstruct&>( *hirExpression );
				return this->lowerConstruct( construct );
			}
			case hir::HIRNodeKind::Cast: {
				hir::HIRCast& castNode = static_cast<hir::HIRCast&>( *hirExpression );
				MIRVariableIdentifier sourceVariable = this->lowerExpression( castNode.sourceExpression );
				MIRInstruction castInstruction( MIRInstructionKind::CastType );
				castInstruction.sourceOperands.push_back( sourceVariable );
				castInstruction.castTargetType = castNode.targetCastType;
				castInstruction.operandType = castNode.resolvedType;
				castInstruction.sourceLocation = castNode.sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable( "_cast", castNode.resolvedType, false );
				castInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( castInstruction );
			}
			case hir::HIRNodeKind::Reference: {
				hir::HIRReference& refNode = static_cast<hir::HIRReference&>( *hirExpression );
				MIRVariableIdentifier targetVariable = this->lowerExpression( refNode.targetExpression );
				MIRInstruction refInstruction( MIRInstructionKind::TakeReference );
				refInstruction.sourceOperands.push_back( targetVariable );
				refInstruction.operandType = refNode.resolvedType;
				refInstruction.sourceLocation = refNode.sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable( "_ref", refNode.resolvedType, false );
				refInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( refInstruction );
			}
			case hir::HIRNodeKind::Dereference: {
				hir::HIRDereference& derefNode = static_cast<hir::HIRDereference&>( *hirExpression );
				MIRVariableIdentifier targetVariable = this->lowerExpression( derefNode.targetExpression );
				MIRInstruction derefInstruction( MIRInstructionKind::DereferencePointer );
				derefInstruction.sourceOperands.push_back( targetVariable );
				derefInstruction.operandType = derefNode.resolvedType;
				derefInstruction.sourceLocation = derefNode.sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable( "_deref", derefNode.resolvedType, false );
				derefInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( derefInstruction );
			}
			case hir::HIRNodeKind::MoveTransfer: {
				hir::HIRMoveTransfer& moveNode = static_cast<hir::HIRMoveTransfer&>( *hirExpression );
				MIRVariableIdentifier sourceVariable = this->lowerExpression( moveNode.movedExpression );
				MIRInstruction moveInstruction( MIRInstructionKind::MoveValue );
				moveInstruction.sourceOperands.push_back( sourceVariable );
				moveInstruction.operandType = moveNode.resolvedType;
				moveInstruction.sourceLocation = moveNode.sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable( "_moved", moveNode.resolvedType, false );
				moveInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( moveInstruction );
			}
			case hir::HIRNodeKind::AddressOf: {
				hir::HIRAddressOf& addrNode = static_cast<hir::HIRAddressOf&>( *hirExpression );
				MIRVariableIdentifier targetVariable = this->lowerExpression( addrNode.targetExpression );
				MIRInstruction addrInstruction( MIRInstructionKind::AddressOf );
				addrInstruction.sourceOperands.push_back( targetVariable );
				addrInstruction.operandType = addrNode.resolvedType;
				addrInstruction.sourceLocation = addrNode.sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable( "_addr", addrNode.resolvedType, false );
				addrInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( addrInstruction );
			}
			case hir::HIRNodeKind::InstanceOf: {
				hir::HIRInstanceOf& instanceNode = static_cast<hir::HIRInstanceOf&>( *hirExpression );
				MIRVariableIdentifier checkedVariable = this->lowerExpression( instanceNode.checkedExpression );
				MIRInstruction checkInstruction( MIRInstructionKind::InstanceOfCheck );
				checkInstruction.sourceOperands.push_back( checkedVariable );
				checkInstruction.operandType = instanceNode.checkedType;
				checkInstruction.sourceLocation = instanceNode.sourceLocation;
				semantic::TypeSharedPointer boolType = std::make_shared<semantic::Type>( semantic::Type::Kind::Bool, "Boolean" );
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable( "_instanceof", boolType, false );
				checkInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( checkInstruction );
			}
			case hir::HIRNodeKind::MatchExpression: {
				hir::HIRMatchExpression& matchExpression = static_cast<hir::HIRMatchExpression&>( *hirExpression );
				MIRVariableIdentifier subjectVariable = this->lowerExpression( matchExpression.matchSubject );
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable( "_match_result", matchExpression.resolvedType, true );
				MIRInstruction allocateResult( MIRInstructionKind::AllocateLocal );
				allocateResult.destinationVariable = resultVariable;
				allocateResult.operandType = matchExpression.resolvedType;
				this->emitInstruction( allocateResult );
				std::shared_ptr<MIRBasicBlock> mergeBlock = this->currentFunction->createBasicBlock( "match.merge" );
				size_t armCount = matchExpression.matchPatterns.size();
				for( size_t armIndex = 0; armIndex < armCount; armIndex++ ) {
					MIRVariableIdentifier patternVariable = this->lowerExpression( matchExpression.matchPatterns[armIndex] );
					MIRInstruction compareInstruction( MIRInstructionKind::CompareEqual );
					MIRVariableIdentifier compareResult = this->currentFunction->allocateVariable( "_match_cmp", nullptr, false );
					compareInstruction.destinationVariable = compareResult;
					compareInstruction.sourceOperands.push_back( subjectVariable );
					compareInstruction.sourceOperands.push_back( patternVariable );
					this->emitInstruction( compareInstruction );
					std::shared_ptr<MIRBasicBlock> armBlock = this->currentFunction->createBasicBlock( fmt::format( "match.arm.{}", armIndex ) );
					std::shared_ptr<MIRBasicBlock> nextBlock = this->currentFunction->createBasicBlock( fmt::format( "match.next.{}", armIndex ) );
					MIRInstruction branchInstruction( MIRInstructionKind::BranchConditional );
					branchInstruction.sourceOperands.push_back( compareResult );
					branchInstruction.trueBranchTarget = armBlock->blockIdentifier;
					branchInstruction.falseBranchTarget = nextBlock->blockIdentifier;
					this->emitTerminator( branchInstruction );
					this->switchToBlock( armBlock );
					MIRVariableIdentifier armValue = this->lowerExpression( matchExpression.matchValues[armIndex] );
					MIRInstruction storeArm( MIRInstructionKind::StoreVariable );
					storeArm.destinationVariable = resultVariable;
					storeArm.sourceOperands.push_back( armValue );
					this->emitInstruction( storeArm );
					MIRInstruction jumpToMerge( MIRInstructionKind::JumpUnconditional );
					jumpToMerge.trueBranchTarget = mergeBlock->blockIdentifier;
					this->emitTerminator( jumpToMerge );
					this->switchToBlock( nextBlock );
				}
				if( matchExpression.matchDefaultValue != nullptr ) {
					MIRVariableIdentifier defaultValue = this->lowerExpression( matchExpression.matchDefaultValue );
					MIRInstruction storeDefault( MIRInstructionKind::StoreVariable );
					storeDefault.destinationVariable = resultVariable;
					storeDefault.sourceOperands.push_back( defaultValue );
					this->emitInstruction( storeDefault );
				}
				if( this->currentBlock->isTerminated == false ) {
					MIRInstruction jumpToMerge( MIRInstructionKind::JumpUnconditional );
					jumpToMerge.trueBranchTarget = mergeBlock->blockIdentifier;
					this->emitTerminator( jumpToMerge );
				}
				this->switchToBlock( mergeBlock );
				MIRInstruction loadResult( MIRInstructionKind::LoadVariable );
				loadResult.sourceOperands.push_back( resultVariable );
				loadResult.operandType = matchExpression.resolvedType;
				MIRVariableIdentifier loadedResult = this->currentFunction->allocateVariable( "_match_loaded", matchExpression.resolvedType, false );
				loadResult.destinationVariable = loadedResult;
				return this->emitInstruction( loadResult );
			}
			case hir::HIRNodeKind::Lambda: {
				hir::HIRLambda& hirLambda = static_cast<hir::HIRLambda&>( *hirExpression );
				std::string lambdaName = fmt::format( "_UR_lambda_{}", this->lambdaCounter++ );
				std::unordered_set<std::string> boundNames;
				for( const hir::HIRParameterDescriptor& paramDescriptor : hirLambda.parameterDescriptors ) {
					boundNames.insert( paramDescriptor.parameterName );
				}
				std::vector<std::string> capturedNames;
				std::unordered_set<std::string> seenCaptures;
				std::function<void( const hir::HIRNodeSharedPointer& )> collectFreeVars =
					[&]( const hir::HIRNodeSharedPointer& node ) {
					if( node == nullptr ) return;
					switch( node->nodeKind ) {
						case hir::HIRNodeKind::Identifier: {
							hir::HIRIdentifier& ident = static_cast<hir::HIRIdentifier&>( *node );
							if( boundNames.count( ident.identifierName ) == 0 &&
								this->variableNameMap.count( ident.identifierName ) > 0 &&
								seenCaptures.count( ident.identifierName ) == 0 ) {
								capturedNames.push_back( ident.identifierName );
								seenCaptures.insert( ident.identifierName );
							}
							break;
						}
						case hir::HIRNodeKind::BinaryOperation: {
							hir::HIRBinaryOperation& binOp = static_cast<hir::HIRBinaryOperation&>( *node );
							collectFreeVars( binOp.leftOperand );
							collectFreeVars( binOp.rightOperand );
							break;
						}
						case hir::HIRNodeKind::UnaryOperation: {
							hir::HIRUnaryOperation& unOp = static_cast<hir::HIRUnaryOperation&>( *node );
							collectFreeVars( unOp.operandExpression );
							break;
						}
						case hir::HIRNodeKind::FunctionCall: {
							hir::HIRFunctionCall& call = static_cast<hir::HIRFunctionCall&>( *node );
							for( const hir::HIRNodeSharedPointer& arg : call.callArguments ) {
								collectFreeVars( arg );
							}
							break;
						}
						case hir::HIRNodeKind::MethodCall: {
							hir::HIRMethodCall& methodCall = static_cast<hir::HIRMethodCall&>( *node );
							collectFreeVars( methodCall.receiverObject );
							for( const hir::HIRNodeSharedPointer& arg : methodCall.callArguments ) {
								collectFreeVars( arg );
							}
							break;
						}
						case hir::HIRNodeKind::Return: {
							hir::HIRReturn& ret = static_cast<hir::HIRReturn&>( *node );
							if( ret.returnValueExpression != nullptr ) {
								collectFreeVars( ret.returnValueExpression );
							}
							break;
						}
						case hir::HIRNodeKind::Block: {
							hir::HIRBlock& block = static_cast<hir::HIRBlock&>( *node );
							for( const hir::HIRNodeSharedPointer& stmt : block.blockStatements ) {
								collectFreeVars( stmt );
							}
							break;
						}
						case hir::HIRNodeKind::ExpressionStatement: {
							hir::HIRExpressionStatement& exprStmt = static_cast<hir::HIRExpressionStatement&>( *node );
							collectFreeVars( exprStmt.expression );
							break;
						}
						case hir::HIRNodeKind::FieldAccess: {
							hir::HIRFieldAccess& fieldAccess = static_cast<hir::HIRFieldAccess&>( *node );
							collectFreeVars( fieldAccess.objectExpression );
							break;
						}
						case hir::HIRNodeKind::IndexAccess: {
							hir::HIRIndexAccess& indexAccess = static_cast<hir::HIRIndexAccess&>( *node );
							collectFreeVars( indexAccess.objectExpression );
							collectFreeVars( indexAccess.indexExpression );
							break;
						}
						case hir::HIRNodeKind::Cast: {
							hir::HIRCast& cast = static_cast<hir::HIRCast&>( *node );
							collectFreeVars( cast.sourceExpression );
							break;
						}
						default: break;
					}
				};
				if( hirLambda.lambdaBody != nullptr ) {
					for( const hir::HIRNodeSharedPointer& stmt : hirLambda.lambdaBody->blockStatements ) {
						collectFreeVars( stmt );
					}
				}
				bool hasCaptures = ( capturedNames.empty() == false );
				std::string effectiveName = hasCaptures ? lambdaName : lambdaName;
				std::string wrapperName = hasCaptures ? fmt::format( "{}.wrap", lambdaName ) : "";
				std::shared_ptr<MIRFunctionDefinition> savedFunction = this->currentFunction;
				std::shared_ptr<MIRBasicBlock> savedBlock = this->currentBlock;
				std::unordered_map<std::string, MIRVariableIdentifier> savedVariableNameMap = this->variableNameMap;
				std::string savedClassName = this->currentClassName;
				std::string savedParentClassName = this->currentParentClassName;
				MIRInstructionIdentifier savedNextInstruction = this->nextInstructionIdentifier;
				std::shared_ptr<MIRFunctionDefinition> lambdaFunction = std::make_shared<MIRFunctionDefinition>();
				lambdaFunction->functionName = lambdaName;
				lambdaFunction->returnTypeDescriptor = hirLambda.returnTypeDescriptor;
				if( lambdaFunction->returnTypeDescriptor == nullptr && hirLambda.resolvedType != nullptr &&
					hirLambda.resolvedType->kind == semantic::Type::Kind::Function ) {
					semantic::FunctionType* funcType = static_cast<semantic::FunctionType*>( hirLambda.resolvedType.get() );
					if( funcType->returnType != nullptr ) {
						lambdaFunction->returnTypeDescriptor = funcType->returnType;
					}
				}
				if( lambdaFunction->returnTypeDescriptor == nullptr ) {
					lambdaFunction->returnTypeDescriptor = std::make_shared<semantic::Type>(
						semantic::Type::Kind::Integer, "I64"
					);
				}
				lambdaFunction->sourceLocation = hirLambda.sourceLocation;
				this->currentFunction = lambdaFunction;
				this->nextInstructionIdentifier = 0;
				this->variableNameMap.clear();
				this->currentClassName = "";
				this->currentParentClassName = "";
				std::shared_ptr<MIRBasicBlock> lambdaEntry = lambdaFunction->createBasicBlock( "entry" );
				this->switchToBlock( lambdaEntry );
				lambdaFunction->entryBlockIdentifier = lambdaEntry->blockIdentifier;
				for( const hir::HIRParameterDescriptor& paramDescriptor : hirLambda.parameterDescriptors ) {
					MIRVariableIdentifier paramVariable = lambdaFunction->allocateVariable(
						paramDescriptor.parameterName, paramDescriptor.parameterType, paramDescriptor.isMutableParameter
					);
					lambdaFunction->variableDescriptorTable[paramVariable].isParameterVariable = true;
					lambdaFunction->parameterVariableIdentifiers.push_back( paramVariable );
					this->variableNameMap[paramDescriptor.parameterName] = paramVariable;
				}
				for( const std::string& captureName : capturedNames ) {
					MIRVariableDescriptor* outerDescriptor = nullptr;
					MIRVariableIdentifier outerVariable = savedVariableNameMap[captureName];
					if( savedFunction->variableDescriptorTable.count( outerVariable ) > 0 ) {
						outerDescriptor = &savedFunction->variableDescriptorTable[outerVariable];
					}
					semantic::TypeSharedPointer captureType = ( outerDescriptor != nullptr )
						? outerDescriptor->variableType : nullptr;
					MIRVariableIdentifier captureParam = lambdaFunction->allocateVariable(
						fmt::format( "cap.{}", captureName ), captureType, false
					);
					lambdaFunction->variableDescriptorTable[captureParam].isParameterVariable = true;
					lambdaFunction->parameterVariableIdentifiers.push_back( captureParam );
					this->variableNameMap[captureName] = captureParam;
				}
				if( hirLambda.lambdaBody != nullptr ) {
					this->lowerBlock( *hirLambda.lambdaBody );
				}
				this->ensureBlockTerminated();
				this->currentModule->functionDefinitions.push_back( lambdaFunction );
				if( hasCaptures ) {
					std::shared_ptr<MIRFunctionDefinition> wrapperFunction = std::make_shared<MIRFunctionDefinition>();
					wrapperFunction->functionName = wrapperName;
					wrapperFunction->returnTypeDescriptor = lambdaFunction->returnTypeDescriptor;
					wrapperFunction->sourceLocation = hirLambda.sourceLocation;
					this->currentFunction = wrapperFunction;
					this->nextInstructionIdentifier = 0;
					this->variableNameMap.clear();
					std::shared_ptr<MIRBasicBlock> wrapperEntry = wrapperFunction->createBasicBlock( "entry" );
					this->switchToBlock( wrapperEntry );
					wrapperFunction->entryBlockIdentifier = wrapperEntry->blockIdentifier;
					std::vector<MIRVariableIdentifier> wrapperParams;
					for( const hir::HIRParameterDescriptor& paramDescriptor : hirLambda.parameterDescriptors ) {
						MIRVariableIdentifier wrapperParam = wrapperFunction->allocateVariable(
							paramDescriptor.parameterName, paramDescriptor.parameterType, false
						);
						wrapperFunction->variableDescriptorTable[wrapperParam].isParameterVariable = true;
						wrapperFunction->parameterVariableIdentifiers.push_back( wrapperParam );
						wrapperParams.push_back( wrapperParam );
					}
					MIRInstruction callInner( MIRInstructionKind::CallFunction );
					callInner.calledFunctionQualifiedName = lambdaName;
					for( MIRVariableIdentifier wrapperParam : wrapperParams ) {
						callInner.sourceOperands.push_back( wrapperParam );
					}
					for( const std::string& captureName : capturedNames ) {
						std::string globalName = fmt::format( "{}.cap.{}", lambdaName, captureName );
						MIRVariableIdentifier loadedCapture = wrapperFunction->allocateVariable(
							fmt::format( "load.cap.{}", captureName ), nullptr, false
						);
						MIRInstruction loadGlobal( MIRInstructionKind::LoadVariable );
						loadGlobal.destinationVariable = loadedCapture;
						loadGlobal.calledFunctionQualifiedName = fmt::format( "@{}", globalName );
						this->emitInstruction( loadGlobal );
						callInner.sourceOperands.push_back( loadedCapture );
					}
					MIRVariableIdentifier callResult = wrapperFunction->allocateVariable(
						"_lambda_result", hirLambda.returnTypeDescriptor, false
					);
					callInner.destinationVariable = callResult;
					callInner.operandType = hirLambda.returnTypeDescriptor;
					this->emitInstruction( callInner );
					MIRInstruction wrapperReturn( MIRInstructionKind::ReturnValue );
					wrapperReturn.sourceOperands.push_back( callResult );
					this->emitTerminator( wrapperReturn );
					this->currentModule->functionDefinitions.push_back( wrapperFunction );
				}
				this->currentFunction = savedFunction;
				this->currentBlock = savedBlock;
				this->variableNameMap = savedVariableNameMap;
				this->currentClassName = savedClassName;
				this->currentParentClassName = savedParentClassName;
				this->nextInstructionIdentifier = savedNextInstruction;
				if( hasCaptures ) {
					for( const std::string& captureName : capturedNames ) {
						std::string globalName = fmt::format( "{}.cap.{}", lambdaName, captureName );
						MIRVariableIdentifier outerVariable = this->variableNameMap[captureName];
						MIRGlobalVariable globalVar;
						globalVar.variableName = globalName;
						if( savedFunction->variableDescriptorTable.count( outerVariable ) > 0 ) {
							globalVar.variableType = savedFunction->variableDescriptorTable[outerVariable].variableType;
						}
						this->currentModule->globalVariables[globalName] = globalVar;
						MIRInstruction storeCapture( MIRInstructionKind::StoreVariable );
						storeCapture.sourceOperands.push_back( outerVariable );
						storeCapture.calledFunctionQualifiedName = fmt::format( "@{}", globalName );
						this->emitInstruction( storeCapture );
					}
				}
				std::string addressFunctionName = hasCaptures ? wrapperName : lambdaName;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
					"_lambda_ptr", hirLambda.resolvedType, false
				);
				MIRInstruction addressOf( MIRInstructionKind::AddressOf );
				addressOf.destinationVariable = resultVariable;
				addressOf.calledFunctionQualifiedName = addressFunctionName;
				return this->emitInstruction( addressOf );
			}
			case hir::HIRNodeKind::Yield: {
				hir::HIRYield& yieldNode = static_cast<hir::HIRYield&>( *hirExpression );
				MIRInstruction yieldInstruction( MIRInstructionKind::Yield );
				if( yieldNode.yieldedExpression != nullptr ) {
					MIRVariableIdentifier yieldedValue = this->lowerExpression( yieldNode.yieldedExpression );
					yieldInstruction.sourceOperands.push_back( yieldedValue );
				}
				yieldInstruction.sourceLocation = yieldNode.sourceLocation;
				this->emitInstruction( yieldInstruction );
				return INVALID_VARIABLE_IDENTIFIER;
			}
			case hir::HIRNodeKind::Await: {
				hir::HIRAwait& awaitNode = static_cast<hir::HIRAwait&>( *hirExpression );
				MIRVariableIdentifier awaitedValue = this->lowerExpression( awaitNode.awaitedExpression );
				MIRInstruction awaitCall( MIRInstructionKind::CallFunction );
				awaitCall.calledFunctionQualifiedName = "__runtime_await";
				awaitCall.sourceOperands.push_back( awaitedValue );
				semantic::TypeSharedPointer resultType = awaitNode.resolvedType;
				if( resultType == nullptr ) {
					resultType = std::make_shared<semantic::Type>( semantic::Type::Kind::Integer, "I64" );
				}
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
					"await.result", resultType, false
				);
				awaitCall.destinationVariable = resultVariable;
				awaitCall.operandType = resultType;
				awaitCall.sourceLocation = awaitNode.sourceLocation;
				if( this->activeLandingPad != INVALID_BLOCK_IDENTIFIER ) {
					awaitCall.landingPadTarget = this->activeLandingPad;
				}
				this->emitInstruction( awaitCall );
				return resultVariable;
			}
			default:
				return INVALID_VARIABLE_IDENTIFIER;
		}
	}
	
	MIRVariableIdentifier MIRLowering::lowerBinaryOperation( hir::HIRBinaryOperation& hirBinaryOp ) {
		MIRVariableIdentifier leftVariable = this->lowerExpression( hirBinaryOp.leftOperand );
		MIRVariableIdentifier rightVariable = this->lowerExpression( hirBinaryOp.rightOperand );
		MIRInstructionKind instructionKind = MIRInstructionKind::NoOperation;
		auto isFloatType = []( const semantic::TypeSharedPointer& type ) -> bool {
			return type != nullptr &&
				( type->kind == semantic::Type::Kind::Float ||
				  ( type->kind == semantic::Type::Kind::Class &&
				    ( type->name == "Float" || type->name == "Double" ||
				      type->name == "F32" || type->name == "F64" ) ) );
		};
		bool isFloatOperation = isFloatType( hirBinaryOp.resolvedType ) ||
			isFloatType( hirBinaryOp.leftOperand->resolvedType ) ||
			isFloatType( hirBinaryOp.rightOperand->resolvedType );
		if( isFloatOperation == false ) {
			if( hirBinaryOp.leftOperand->nodeKind == hir::HIRNodeKind::FloatLiteral ||
				hirBinaryOp.rightOperand->nodeKind == hir::HIRNodeKind::FloatLiteral ) {
				isFloatOperation = true;
			}
		}
		if( isFloatOperation == false ) {
			if( this->currentFunction->variableDescriptorTable.count( leftVariable ) > 0 ) {
				isFloatOperation = isFloatType( this->currentFunction->variableDescriptorTable[leftVariable].variableType );
			}
		}
		if( isFloatOperation == false ) {
			if( this->currentFunction->variableDescriptorTable.count( rightVariable ) > 0 ) {
				isFloatOperation = isFloatType( this->currentFunction->variableDescriptorTable[rightVariable].variableType );
			}
		}
		auto isStringType = []( const semantic::TypeSharedPointer& type ) -> bool {
			return type != nullptr &&
				( type->kind == semantic::Type::Kind::String ||
				  ( type->kind == semantic::Type::Kind::Class && type->name == "String" ) );
		};
		auto isStringHIRNode = [&]( const hir::HIRNodeSharedPointer& node ) -> bool {
			if( node == nullptr ) { return false; }
			if( node->nodeKind == hir::HIRNodeKind::StringLiteral ) { return true; }
			if( isStringType( node->resolvedType ) ) { return true; }
			return false;
		};
		auto isStringMIRVariable = [&]( MIRVariableIdentifier variable ) -> bool {
			std::unordered_map<MIRVariableIdentifier, MIRVariableDescriptor>::iterator descriptorIterator =
				this->currentFunction->variableDescriptorTable.find( variable );
			if( descriptorIterator != this->currentFunction->variableDescriptorTable.end() ) {
				return isStringType( descriptorIterator->second.variableType );
			}
			return false;
		};
		bool isStringOperation = isStringType( hirBinaryOp.resolvedType );
		bool hasStringOperands = isStringType( hirBinaryOp.leftOperand->resolvedType ) ||
			isStringType( hirBinaryOp.rightOperand->resolvedType );
		bool hasStringHIRNodes = isStringHIRNode( hirBinaryOp.leftOperand ) ||
			isStringHIRNode( hirBinaryOp.rightOperand );
		bool hasStringMIRVariables = isStringMIRVariable( leftVariable ) ||
			isStringMIRVariable( rightVariable );
		bool isStringContext = isStringOperation || hasStringOperands ||
			hasStringHIRNodes || hasStringMIRVariables;
		if( isStringContext && hirBinaryOp.operatorKind == token::Type::Plus ) {
			semantic::TypeSharedPointer concatResultType = hirBinaryOp.resolvedType;
			if( concatResultType == nullptr ) {
				concatResultType = std::make_shared<semantic::Type>( semantic::Type::Kind::String, "String" );
			}
			MIRInstruction concatInstruction( MIRInstructionKind::CallFunction );
			concatInstruction.calledFunctionQualifiedName = "concat";
			concatInstruction.sourceOperands.push_back( leftVariable );
			concatInstruction.sourceOperands.push_back( rightVariable );
			concatInstruction.operandType = concatResultType;
			concatInstruction.sourceLocation = hirBinaryOp.sourceLocation;
			MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
				"_concat", concatResultType, false
			);
			concatInstruction.destinationVariable = resultVariable;
			return this->emitInstruction( concatInstruction );
		}
		if( isStringContext &&
			( hirBinaryOp.operatorKind == token::Type::Equal ||
			  hirBinaryOp.operatorKind == token::Type::NotEqual ) ) {
			MIRInstruction equalsCall( MIRInstructionKind::CallFunction );
			equalsCall.calledFunctionQualifiedName = "equals";
			equalsCall.sourceOperands.push_back( leftVariable );
			equalsCall.sourceOperands.push_back( rightVariable );
			equalsCall.operandType = hirBinaryOp.resolvedType;
			equalsCall.sourceLocation = hirBinaryOp.sourceLocation;
			MIRVariableIdentifier equalsResult = this->currentFunction->allocateVariable(
				"_streq", hirBinaryOp.resolvedType, false
			);
			equalsCall.destinationVariable = equalsResult;
			this->emitInstruction( equalsCall );
			if( hirBinaryOp.operatorKind == token::Type::NotEqual ) {
				MIRInstruction negateInstruction( MIRInstructionKind::LogicalNot );
				negateInstruction.sourceOperands.push_back( equalsResult );
				negateInstruction.operandType = hirBinaryOp.resolvedType;
				negateInstruction.sourceLocation = hirBinaryOp.sourceLocation;
				MIRVariableIdentifier negatedResult = this->currentFunction->allocateVariable(
					"_strne", hirBinaryOp.resolvedType, false
				);
				negateInstruction.destinationVariable = negatedResult;
				return this->emitInstruction( negateInstruction );
			}
			return equalsResult;
		}
		semantic::TypeSharedPointer leftType = hirBinaryOp.leftOperand->resolvedType;
		if( leftType != nullptr && leftType->kind == semantic::Type::Kind::Class ) {
			semantic::ClassTypeSharedPointer leftClassType = std::static_pointer_cast<semantic::ClassType>( leftType );
			struct OperatorInterfaceMapping {
				int tokenType;
				const std::string& qualifiedName;
				const char* methodName;
				bool negateAfter;
			};
			static const OperatorInterfaceMapping operatorInterfaceMappings[] = {
				{ ( int ) token::Type::Plus,             semantic::qname::ADDABLE,      "add",            false },
				{ ( int ) token::Type::Minus,            semantic::qname::SUBTRACTABLE,  "subtract",       false },
				{ ( int ) token::Type::Star,             semantic::qname::MULTIPLIABLE,  "multiply",       false },
				{ ( int ) token::Type::Slash,            semantic::qname::DIVIDABLE,     "divide",         false },
				{ ( int ) token::Type::Percent,          semantic::qname::MODULABLE,     "modulo",         false },
				{ ( int ) token::Type::Equal,            semantic::qname::EQUATABLE,     "equals",         false },
				{ ( int ) token::Type::NotEqual,         semantic::qname::EQUATABLE,     "equals",         true },
				{ ( int ) token::Type::LessThan,         semantic::qname::COMPARABLE,    "lessThan",       false },
				{ ( int ) token::Type::GreaterThan,      semantic::qname::COMPARABLE,    "greaterThan",    false },
				{ ( int ) token::Type::LessThanEqual,    semantic::qname::COMPARABLE,    "lessOrEqual",    false },
				{ ( int ) token::Type::GreaterThanEqual, semantic::qname::COMPARABLE,    "greaterOrEqual", false },
			};
			for( const OperatorInterfaceMapping& mapping : operatorInterfaceMappings ) {
				if( mapping.tokenType != ( int ) hirBinaryOp.operatorKind ) {
					continue;
				}
				if( leftClassType->implementsInterface( mapping.qualifiedName ) == false ) {
					break;
				}
				std::string operatorTypeName = leftType->name;
				size_t operatorGenericPos = operatorTypeName.find( '<' );
				if( operatorGenericPos != std::string::npos ) {
					operatorTypeName = operatorTypeName.substr( 0, operatorGenericPos );
				}
				std::string qualifiedMethodName = operatorTypeName + "." + mapping.methodName;
				MIRInstruction methodCall( MIRInstructionKind::CallFunction );
				methodCall.calledFunctionQualifiedName = qualifiedMethodName;
				methodCall.sourceOperands.push_back( leftVariable );
				methodCall.sourceOperands.push_back( rightVariable );
				methodCall.operandType = hirBinaryOp.resolvedType;
				methodCall.sourceLocation = hirBinaryOp.sourceLocation;
				MIRVariableIdentifier callResult = this->currentFunction->allocateVariable(
					"_op_result", hirBinaryOp.resolvedType, false
				);
				methodCall.destinationVariable = callResult;
				this->emitInstruction( methodCall );
				if( mapping.negateAfter ) {
					MIRInstruction negateInstruction( MIRInstructionKind::LogicalNot );
					negateInstruction.sourceOperands.push_back( callResult );
					negateInstruction.operandType = hirBinaryOp.resolvedType;
					MIRVariableIdentifier negatedResult = this->currentFunction->allocateVariable(
						"_op_neq", hirBinaryOp.resolvedType, false
					);
					negateInstruction.destinationVariable = negatedResult;
					return this->emitInstruction( negateInstruction );
				}
				return callResult;
			}
		}
		switch( hirBinaryOp.operatorKind ) {
			case token::Type::Plus:
				instructionKind = isFloatOperation ? MIRInstructionKind::AddFloat : MIRInstructionKind::AddInteger;
				break;
			case token::Type::Minus:
				instructionKind = isFloatOperation ? MIRInstructionKind::SubtractFloat : MIRInstructionKind::SubtractInteger;
				break;
			case token::Type::Star:
				instructionKind = isFloatOperation ? MIRInstructionKind::MultiplyFloat : MIRInstructionKind::MultiplyInteger;
				break;
			case token::Type::Slash:
				instructionKind = isFloatOperation ? MIRInstructionKind::DivideFloat : MIRInstructionKind::DivideInteger;
				break;
			case token::Type::Percent:          instructionKind = MIRInstructionKind::ModuloInteger; break;
			case token::Type::Power:
				instructionKind = isFloatOperation ? MIRInstructionKind::PowerFloat : MIRInstructionKind::PowerInteger;
				break;
			case token::Type::Ampersand:        instructionKind = MIRInstructionKind::BitwiseAnd; break;
			case token::Type::Pipe:             instructionKind = MIRInstructionKind::BitwiseOr; break;
			case token::Type::Caret:            instructionKind = MIRInstructionKind::BitwiseXor; break;
			case token::Type::ShiftLeft:        instructionKind = MIRInstructionKind::ShiftLeft; break;
			case token::Type::ShiftRight:       instructionKind = MIRInstructionKind::ShiftRight; break;
			case token::Type::Equal:            instructionKind = MIRInstructionKind::CompareEqual; break;
			case token::Type::NotEqual:         instructionKind = MIRInstructionKind::CompareNotEqual; break;
			case token::Type::LessThan:         instructionKind = MIRInstructionKind::CompareLessThan; break;
			case token::Type::GreaterThan:      instructionKind = MIRInstructionKind::CompareGreaterThan; break;
			case token::Type::LessThanEqual:    instructionKind = MIRInstructionKind::CompareLessEqual; break;
			case token::Type::GreaterThanEqual: instructionKind = MIRInstructionKind::CompareGreaterEqual; break;
			case token::Type::KeywordAnd:       instructionKind = MIRInstructionKind::LogicalAnd; break;
			case token::Type::KeywordOr:        instructionKind = MIRInstructionKind::LogicalOr; break;
			case token::Type::KeywordIs:        instructionKind = MIRInstructionKind::CompareEqual; break;
			default:                            instructionKind = MIRInstructionKind::NoOperation; break;
		}
		MIRInstruction binaryInstruction( instructionKind );
		binaryInstruction.sourceOperands.push_back( leftVariable );
		binaryInstruction.sourceOperands.push_back( rightVariable );
		binaryInstruction.operandType = hirBinaryOp.resolvedType;
		binaryInstruction.sourceLocation = hirBinaryOp.sourceLocation;
		semantic::TypeSharedPointer resultType = hirBinaryOp.resolvedType;
		if( resultType == nullptr && isFloatOperation ) {
			resultType = std::make_shared<semantic::Type>( semantic::Type::Kind::Float, "F64" );
		}
		MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
			"_binop", resultType, false
		);
		binaryInstruction.destinationVariable = resultVariable;
		return this->emitInstruction( binaryInstruction );
	}
	
	MIRVariableIdentifier MIRLowering::lowerUnaryOperation( hir::HIRUnaryOperation& hirUnaryOp ) {
		MIRVariableIdentifier operandVariable = this->lowerExpression( hirUnaryOp.operandExpression );
		semantic::TypeSharedPointer operandType = hirUnaryOp.operandExpression->resolvedType;
		if( operandType != nullptr && operandType->kind == semantic::Type::Kind::Class &&
			hirUnaryOp.operatorKind == token::Type::Minus ) {
			semantic::ClassTypeSharedPointer operandClassType = std::static_pointer_cast<semantic::ClassType>( operandType );
			if( operandClassType->implementsInterface( semantic::qname::NEGATABLE ) ) {
				std::string operandTypeName = operandType->name;
				size_t operandGenericPos = operandTypeName.find( '<' );
				if( operandGenericPos != std::string::npos ) {
					operandTypeName = operandTypeName.substr( 0, operandGenericPos );
				}
				std::string qualifiedMethodName = operandTypeName + ".negate";
				MIRInstruction methodCall( MIRInstructionKind::CallFunction );
				methodCall.calledFunctionQualifiedName = qualifiedMethodName;
				methodCall.sourceOperands.push_back( operandVariable );
				methodCall.operandType = hirUnaryOp.resolvedType;
				methodCall.sourceLocation = hirUnaryOp.sourceLocation;
				MIRVariableIdentifier callResult = this->currentFunction->allocateVariable(
					"_op_neg", hirUnaryOp.resolvedType, false
				);
				methodCall.destinationVariable = callResult;
				return this->emitInstruction( methodCall );
			}
		}
		MIRInstructionKind instructionKind = MIRInstructionKind::NoOperation;
		switch( hirUnaryOp.operatorKind ) {
			case token::Type::Minus: {
				bool isFloatNegate = hirUnaryOp.resolvedType != nullptr &&
					( hirUnaryOp.resolvedType->kind == semantic::Type::Kind::Float ||
					  ( hirUnaryOp.resolvedType->kind == semantic::Type::Kind::Class &&
					    ( hirUnaryOp.resolvedType->name == "Float" ||
					      hirUnaryOp.resolvedType->name == "Double" ||
					      hirUnaryOp.resolvedType->name == "F32" ||
					      hirUnaryOp.resolvedType->name == "F64" ) ) );
				instructionKind = isFloatNegate
					? MIRInstructionKind::NegateFloat
					: MIRInstructionKind::NegateInteger;
				break;
			}
			case token::Type::Tilde:      instructionKind = MIRInstructionKind::BitwiseNot; break;
			case token::Type::KeywordNot: instructionKind = MIRInstructionKind::LogicalNot; break;
			default:                      instructionKind = MIRInstructionKind::NoOperation; break;
		}
		MIRInstruction unaryInstruction( instructionKind );
		unaryInstruction.sourceOperands.push_back( operandVariable );
		unaryInstruction.operandType = hirUnaryOp.resolvedType;
		unaryInstruction.sourceLocation = hirUnaryOp.sourceLocation;
		MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
			"_unop", hirUnaryOp.resolvedType, false
		);
		unaryInstruction.destinationVariable = resultVariable;
		return this->emitInstruction( unaryInstruction );
	}
	
	MIRVariableIdentifier MIRLowering::lowerFunctionCall( hir::HIRFunctionCall& hirCall ) {
		std::vector<MIRVariableIdentifier> argumentVariables;
		for( const hir::HIRNodeSharedPointer& argument : hirCall.callArguments ) {
			argumentVariables.push_back( this->lowerExpression( argument ) );
		}
		bool useInvoke = ( this->activeLandingPad != INVALID_BLOCK_IDENTIFIER );
		MIRInstruction callInstruction( useInvoke ? MIRInstructionKind::InvokeFunction : MIRInstructionKind::CallFunction );
		if( hirCall.calleeExpression != nullptr ) {
			if( hirCall.calleeExpression->nodeKind == hir::HIRNodeKind::Identifier ) {
				hir::HIRIdentifier& calleeIdentifier = static_cast<hir::HIRIdentifier&>( *hirCall.calleeExpression );
				if( calleeIdentifier.qualifiedScopeName.empty() == false ) {
					callInstruction.calledFunctionQualifiedName = calleeIdentifier.qualifiedScopeName;
				}
				else {
					callInstruction.calledFunctionQualifiedName = calleeIdentifier.identifierName;
				}
			}
			else if( hirCall.calleeExpression->nodeKind == hir::HIRNodeKind::SuperReference ) {
				if( this->currentParentClassName.empty() == false ) {
					std::string parentShortName = this->currentParentClassName;
					size_t lastDotPos = parentShortName.rfind( '.' );
					if( lastDotPos != std::string::npos ) {
						parentShortName = parentShortName.substr( lastDotPos + 1 );
					}
					callInstruction.calledFunctionQualifiedName = this->currentParentClassName + "." + parentShortName;
					std::unordered_map<std::string, MIRVariableIdentifier>::iterator selfLookup =
						this->variableNameMap.find( "self" );
					if( selfLookup != this->variableNameMap.end() ) {
						argumentVariables.insert( argumentVariables.begin(), selfLookup->second );
					}
				}
			}
		}
		callInstruction.sourceOperands = std::move( argumentVariables );
		callInstruction.operandType = hirCall.resolvedType;
		callInstruction.sourceLocation = hirCall.sourceLocation;
		for( const std::pair<std::string, hir::HIRNodeSharedPointer>& keywordArgument : hirCall.keywordArguments ) {
			callInstruction.keywordArgumentKeys.push_back( keywordArgument.first );
			MIRVariableIdentifier valueVariable = this->lowerExpression( keywordArgument.second );
			callInstruction.keywordArgumentValues.push_back( valueVariable );
		}
		MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
			"_call", hirCall.resolvedType, false
		);
		callInstruction.destinationVariable = resultVariable;
		if( useInvoke ) {
			std::shared_ptr<MIRBasicBlock> continuationBlock = this->currentFunction->createBasicBlock( "invoke.cont" );
			callInstruction.trueBranchTarget = continuationBlock->blockIdentifier;
			callInstruction.landingPadTarget = this->activeLandingPad;
			this->emitTerminator( callInstruction );
			this->switchToBlock( continuationBlock );
			return resultVariable;
		}
		return this->emitInstruction( callInstruction );
	}
	
	MIRVariableIdentifier MIRLowering::lowerMethodCall( hir::HIRMethodCall& hirMethodCall ) {
		MIRVariableIdentifier receiverVariable = this->lowerExpression( hirMethodCall.receiverObject );
		std::vector<MIRVariableIdentifier> argumentVariables;
		argumentVariables.push_back( receiverVariable );
		for( const hir::HIRNodeSharedPointer& argument : hirMethodCall.callArguments ) {
			argumentVariables.push_back( this->lowerExpression( argument ) );
		}
		bool useInvoke = ( this->activeLandingPad != INVALID_BLOCK_IDENTIFIER );
		MIRInstruction callInstruction( useInvoke ? MIRInstructionKind::InvokeFunction : MIRInstructionKind::CallFunction );
		std::string ownerClassName;
		if( hirMethodCall.receiverObject != nullptr &&
			hirMethodCall.receiverObject->resolvedType != nullptr ) {
			semantic::TypeSharedPointer receiverType = hirMethodCall.receiverObject->resolvedType;
			if( receiverType->kind == semantic::Type::Kind::Optional ) {
				semantic::OptionalType* optionalType = static_cast<semantic::OptionalType*>( receiverType.get() );
				if( optionalType->inner != nullptr ) {
					receiverType = optionalType->inner;
				}
			}
			ownerClassName = receiverType->name;
			size_t genericBracketPosition = ownerClassName.find( '<' );
			if( genericBracketPosition != std::string::npos ) {
				ownerClassName = ownerClassName.substr( 0, genericBracketPosition );
			}
			if( receiverType->kind == semantic::Type::Kind::GenericParameter ) {
				bool resolved = false;
				if( this->currentClassName.empty() == false ) {
					std::unordered_map<std::string, std::unordered_map<std::string, std::string>>::iterator classIt =
						this->genericClassSubstitutions.find( this->currentClassName );
					if( classIt != this->genericClassSubstitutions.end() ) {
						std::unordered_map<std::string, std::string>::iterator paramIt =
							classIt->second.find( ownerClassName );
						if( paramIt != classIt->second.end() ) {
							ownerClassName = paramIt->second;
							resolved = true;
						}
					}
				}
				if( resolved == false ) {
					ownerClassName = "";
				}
			}
			if( receiverType->kind == semantic::Type::Kind::Interface &&
				this->currentClassName.empty() == false ) {
				std::unordered_map<std::string, std::unordered_map<std::string, std::string>>::iterator classIt =
					this->genericClassSubstitutions.find( this->currentClassName );
				if( classIt != this->genericClassSubstitutions.end() ) {
					for( const std::pair<const std::string, std::string>& substitution : classIt->second ) {
						if( this->typeRegistry != nullptr ) {
							semantic::TypeSharedPointer concreteType = this->typeRegistry->lookupType( substitution.second );
							if( concreteType != nullptr && concreteType->kind == semantic::Type::Kind::Class ) {
								semantic::ClassType* concreteClass = static_cast<semantic::ClassType*>( concreteType.get() );
								for( const semantic::TypeSharedPointer& implementedInterface : concreteClass->interfaces ) {
									if( implementedInterface != nullptr && implementedInterface->name == ownerClassName ) {
										ownerClassName = substitution.second;
										goto interfaceResolved;
									}
								}
							}
						}
					}
					interfaceResolved:;
				}
			}
			if( this->typeRegistry != nullptr ) {
				semantic::TypeSharedPointer resolvedOwnerType = this->typeRegistry->lookupType( ownerClassName );
				if( resolvedOwnerType != nullptr && resolvedOwnerType->qualified.empty() == false ) {
					ownerClassName = resolvedOwnerType->qualified;
					size_t genericPos = ownerClassName.find( '<' );
					if( genericPos != std::string::npos ) {
						ownerClassName = ownerClassName.substr( 0, genericPos );
					}
				}
			}
		}
		if( ownerClassName.empty() && hirMethodCall.receiverObject != nullptr ) {
			if( hirMethodCall.receiverObject->nodeKind == hir::HIRNodeKind::SelfReference ) {
				ownerClassName = this->currentClassName;
			}
			else if( hirMethodCall.receiverObject->nodeKind == hir::HIRNodeKind::FieldAccess ) {
				hir::HIRFieldAccess& fieldAccess = static_cast<hir::HIRFieldAccess&>( *hirMethodCall.receiverObject );
				std::string objectClassName;
				if( fieldAccess.objectExpression != nullptr ) {
					if( fieldAccess.objectExpression->resolvedType != nullptr ) {
						objectClassName = fieldAccess.objectExpression->resolvedType->name;
					}
					else if( fieldAccess.objectExpression->nodeKind == hir::HIRNodeKind::SelfReference ) {
						objectClassName = this->currentClassName;
					}
				}
				if( objectClassName.empty() == false ) {
					size_t genericPosition = objectClassName.find( '<' );
					if( genericPosition != std::string::npos ) {
						objectClassName = objectClassName.substr( 0, genericPosition );
					}
					std::unordered_map<std::string, TypeLayoutDescriptor>::iterator layoutIterator =
						this->currentModule->typeLayoutTable.find( objectClassName );
					if( layoutIterator != this->currentModule->typeLayoutTable.end() ) {
						TypeLayoutDescriptor& layout = layoutIterator->second;
						for( size_t fieldIndex = 0; fieldIndex < layout.fieldNames.size(); fieldIndex++ ) {
							if( layout.fieldNames[fieldIndex] == fieldAccess.fieldName &&
								fieldIndex < layout.fieldTypes.size() &&
								layout.fieldTypes[fieldIndex] != nullptr ) {
								ownerClassName = layout.fieldTypes[fieldIndex]->qualified.empty() == false
									? layout.fieldTypes[fieldIndex]->qualified
									: layout.fieldTypes[fieldIndex]->name;
								size_t genericPosition2 = ownerClassName.find( '<' );
								if( genericPosition2 != std::string::npos ) {
									ownerClassName = ownerClassName.substr( 0, genericPosition2 );
								}
								break;
							}
						}
					}
				}
			}
			else if( hirMethodCall.receiverObject->nodeKind == hir::HIRNodeKind::Identifier ) {
				if( this->currentFunction->variableDescriptorTable.count( receiverVariable ) > 0 ) {
					MIRVariableDescriptor& descriptor = this->currentFunction->variableDescriptorTable[receiverVariable];
					if( descriptor.variableType != nullptr &&
						descriptor.variableType->kind != semantic::Type::Kind::GenericParameter ) {
						ownerClassName = descriptor.variableType->qualified.empty() == false
							? descriptor.variableType->qualified
							: descriptor.variableType->name;
						size_t genericPosition = ownerClassName.find( '<' );
						if( genericPosition != std::string::npos ) {
							ownerClassName = ownerClassName.substr( 0, genericPosition );
						}
					}
				}
				if( ownerClassName.empty() && this->typeRegistry != nullptr ) {
					hir::HIRIdentifier& identifierNode =
						static_cast<hir::HIRIdentifier&>( *hirMethodCall.receiverObject );
					semantic::TypeSharedPointer typeByName =
						this->typeRegistry->lookupType( identifierNode.identifierName );
					if( typeByName != nullptr && typeByName->kind == semantic::Type::Kind::Class ) {
						ownerClassName = typeByName->qualified.empty() == false
							? typeByName->qualified : typeByName->name;
						size_t genericPosition = ownerClassName.find( '<' );
						if( genericPosition != std::string::npos ) {
							ownerClassName = ownerClassName.substr( 0, genericPosition );
						}
					}
				}
			}
		}
		if( ownerClassName.empty() == false ) {
			callInstruction.calledFunctionQualifiedName = ownerClassName + "." + hirMethodCall.methodName;
		}
		else {
			callInstruction.calledFunctionQualifiedName = hirMethodCall.methodName;
		}
		if( ownerClassName.empty() == false && this->typeRegistry != nullptr &&
			argumentVariables.empty() == false ) {
			std::string staticCheckName = ownerClassName;
			size_t lastDotPosition = staticCheckName.rfind( '.' );
			if( lastDotPosition != std::string::npos ) {
				staticCheckName = staticCheckName.substr( lastDotPosition + 1 );
			}
			semantic::TypeSharedPointer ownerType = this->typeRegistry->lookupType( staticCheckName );
			if( ownerType == nullptr ) {
				ownerType = this->typeRegistry->lookupType( ownerClassName );
			}
			if( ownerType != nullptr && ownerType->kind == semantic::Type::Kind::Class ) {
				semantic::ClassType* classType = static_cast<semantic::ClassType*>( ownerType.get() );
				semantic::MethodInfo* methodInfo = classType->findMethod( hirMethodCall.methodName );
				if( methodInfo != nullptr && methodInfo->isStatic ) {
					argumentVariables.erase( argumentVariables.begin() );
				}
			}
		}
		callInstruction.sourceOperands = std::move( argumentVariables );
		callInstruction.operandType = hirMethodCall.resolvedType;
		callInstruction.sourceLocation = hirMethodCall.sourceLocation;
		for( const std::pair<std::string, hir::HIRNodeSharedPointer>& keywordArgument : hirMethodCall.keywordArguments ) {
			callInstruction.keywordArgumentKeys.push_back( keywordArgument.first );
			MIRVariableIdentifier valueVariable = this->lowerExpression( keywordArgument.second );
			callInstruction.keywordArgumentValues.push_back( valueVariable );
		}
		MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
			"_mcall", hirMethodCall.resolvedType, false
		);
		callInstruction.destinationVariable = resultVariable;
		if( useInvoke ) {
			std::shared_ptr<MIRBasicBlock> continuationBlock = this->currentFunction->createBasicBlock( "invoke.cont" );
			callInstruction.trueBranchTarget = continuationBlock->blockIdentifier;
			callInstruction.landingPadTarget = this->activeLandingPad;
			this->emitTerminator( callInstruction );
			this->switchToBlock( continuationBlock );
			return resultVariable;
		}
		return this->emitInstruction( callInstruction );
	}
	
	MIRVariableIdentifier MIRLowering::lowerFieldAccess( hir::HIRFieldAccess& hirFieldAccess ) {
		if( hirFieldAccess.objectExpression != nullptr &&
			hirFieldAccess.objectExpression->resolvedType != nullptr &&
			hirFieldAccess.objectExpression->resolvedType->kind == semantic::Type::Kind::Enum ) {
			std::string enumName = hirFieldAccess.objectExpression->resolvedType->name;
			std::string constantKey = fmt::format( "{}.{}", enumName, hirFieldAccess.fieldName );
			std::unordered_map<std::string, MIRModuleConstant>::iterator constantLookup =
				this->currentModule->moduleConstants.find( constantKey );
			if( constantLookup != this->currentModule->moduleConstants.end() ) {
				MIRInstruction constantInstruction( MIRInstructionKind::ConstantInteger );
				constantInstruction.integerConstantValue = constantLookup->second.integerValue;
				constantInstruction.operandType = hirFieldAccess.resolvedType;
				constantInstruction.sourceLocation = hirFieldAccess.sourceLocation;
				MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
					fmt::format( "_enum_{}_{}", enumName, hirFieldAccess.fieldName ),
					hirFieldAccess.resolvedType, false
				);
				constantInstruction.destinationVariable = resultVariable;
				return this->emitInstruction( constantInstruction );
			}
			semantic::EnumTypeSharedPointer enumType = std::dynamic_pointer_cast<semantic::EnumType>(
				hirFieldAccess.objectExpression->resolvedType
			);
			if( enumType != nullptr ) {
				semantic::EnumVariantInfo* variantInfo = enumType->findVariant( hirFieldAccess.fieldName );
				if( variantInfo != nullptr ) {
					int64_t variantValue = variantInfo->discriminant;
					if( enumType->astDeclaration != nullptr ) {
						for( size_t vi = 0; vi < enumType->astDeclaration->variants.size(); vi++ ) {
							ast::nodes::EnumVariantSharedPointer& astVariant = enumType->astDeclaration->variants[vi];
							if( astVariant->name == hirFieldAccess.fieldName && astVariant->backedValue != nullptr ) {
								if( astVariant->backedValue->kind == ast::Node::Kind::IntegerLiteral ) {
									ast::nodes::IntegerLiteralExpression& intLit =
										static_cast<ast::nodes::IntegerLiteralExpression&>( *astVariant->backedValue );
									variantValue = intLit.value;
								}
								break;
							}
						}
					}
					MIRInstruction constantInstruction( MIRInstructionKind::ConstantInteger );
					constantInstruction.integerConstantValue = variantValue;
					constantInstruction.operandType = hirFieldAccess.resolvedType;
					constantInstruction.sourceLocation = hirFieldAccess.sourceLocation;
					MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
						fmt::format( "_enum_{}_{}", enumName, hirFieldAccess.fieldName ),
						hirFieldAccess.resolvedType, false
					);
					constantInstruction.destinationVariable = resultVariable;
					return this->emitInstruction( constantInstruction );
				}
			}
		}
		MIRVariableIdentifier objectVariable = this->lowerExpression( hirFieldAccess.objectExpression );
		int resolvedFieldIndex = hirFieldAccess.fieldLayoutIndex;
		semantic::TypeSharedPointer objectType = ( hirFieldAccess.objectExpression != nullptr )
			? hirFieldAccess.objectExpression->resolvedType : nullptr;
		if( objectType == nullptr && this->currentFunction->variableDescriptorTable.count( objectVariable ) > 0 ) {
			objectType = this->currentFunction->variableDescriptorTable[objectVariable].variableType;
		}
		if( resolvedFieldIndex < 0 && objectType != nullptr ) {
			std::string objectTypeName = objectType->name;
			size_t genericPosition = objectTypeName.find( '<' );
			if( genericPosition != std::string::npos ) {
				objectTypeName = objectTypeName.substr( 0, genericPosition );
			}
			if( this->currentModule->typeLayoutTable.count( objectTypeName ) > 0 ) {
				const TypeLayoutDescriptor& layout = this->currentModule->typeLayoutTable[objectTypeName];
				for( size_t fieldIndex = 0; fieldIndex < layout.fieldNames.size(); fieldIndex++ ) {
					if( layout.fieldNames[fieldIndex] == hirFieldAccess.fieldName ) {
						resolvedFieldIndex = static_cast<int>( fieldIndex );
						break;
					}
				}
			}
		}
		MIRInstruction gepInstruction( MIRInstructionKind::ComputeFieldAddress );
		gepInstruction.sourceOperands.push_back( objectVariable );
		gepInstruction.fieldAccessName = hirFieldAccess.fieldName;
		gepInstruction.fieldLayoutIndex = resolvedFieldIndex;
		gepInstruction.operandType = hirFieldAccess.resolvedType;
		gepInstruction.sourceLocation = hirFieldAccess.sourceLocation;
		MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
			fmt::format( "_field_{}", hirFieldAccess.fieldName ), hirFieldAccess.resolvedType, false
		);
		gepInstruction.destinationVariable = resultVariable;
		return this->emitInstruction( gepInstruction );
	}
	
	MIRVariableIdentifier MIRLowering::lowerIndexAccess( hir::HIRIndexAccess& hirIndexAccess ) {
		MIRVariableIdentifier objectVariable = this->lowerExpression( hirIndexAccess.objectExpression );
		MIRVariableIdentifier indexVariable = this->lowerExpression( hirIndexAccess.indexExpression );
		MIRInstruction gepInstruction( MIRInstructionKind::ComputeIndexAddress );
		gepInstruction.sourceOperands.push_back( objectVariable );
		gepInstruction.sourceOperands.push_back( indexVariable );
		gepInstruction.operandType = hirIndexAccess.resolvedType;
		gepInstruction.sourceLocation = hirIndexAccess.sourceLocation;
		MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable(
			"_index", hirIndexAccess.resolvedType, false
		);
		gepInstruction.destinationVariable = resultVariable;
		return this->emitInstruction( gepInstruction );
	}
	
	MIRVariableIdentifier MIRLowering::lowerConstruct( hir::HIRConstruct& hirConstruct ) {
		std::vector<MIRVariableIdentifier> fieldVariables;
		for( const std::pair<std::string, hir::HIRNodeSharedPointer>& field : hirConstruct.constructorFields ) {
			fieldVariables.push_back( this->lowerExpression( field.second ) );
		}
		MIRInstruction constructInstruction( MIRInstructionKind::ConstructObject );
		constructInstruction.sourceOperands = std::move( fieldVariables );
		constructInstruction.operandType = hirConstruct.constructedType;
		constructInstruction.sourceLocation = hirConstruct.sourceLocation;
		if( hirConstruct.constructedType != nullptr ) {
			std::string constructedName = hirConstruct.constructedType->qualified.empty() == false
				? hirConstruct.constructedType->qualified
				: hirConstruct.constructedType->name;
			size_t genericBracketPosition = constructedName.find( '<' );
			if( genericBracketPosition != std::string::npos ) {
				constructedName = constructedName.substr( 0, genericBracketPosition );
			}
			constructInstruction.calledFunctionQualifiedName = constructedName;
		}
		MIRVariableIdentifier resultVariable = this->currentFunction->allocateVariable( "_construct", hirConstruct.constructedType, false );
		constructInstruction.destinationVariable = resultVariable;
		return this->emitInstruction( constructInstruction );
	}
	
	MIRVariableIdentifier MIRLowering::emitInstruction( MIRInstruction instruction ) {
		if( this->currentBlock == nullptr || this->currentBlock->isTerminated ) {
			return instruction.destinationVariable;
		}
		instruction.instructionIdentifier = this->nextInstructionIdentifier++;
		MIRVariableIdentifier destinationVariable = instruction.destinationVariable;
		this->currentBlock->blockInstructions.push_back( std::move( instruction ) );
		return destinationVariable;
	}
	
	void MIRLowering::emitTerminator( MIRInstruction terminator ) {
		if( this->currentBlock == nullptr || this->currentBlock->isTerminated ) {
			return;
		}
		terminator.instructionIdentifier = this->nextInstructionIdentifier++;
		this->currentBlock->blockInstructions.push_back( std::move( terminator ) );
		this->currentBlock->isTerminated = true;
	}
	
	void MIRLowering::switchToBlock( std::shared_ptr<MIRBasicBlock> targetBlock ) {
		this->currentBlock = std::move( targetBlock );
	}
	
	void MIRLowering::ensureBlockTerminated() {
		if( this->currentBlock != nullptr && this->currentBlock->isTerminated == false ) {
			this->emitDeferredStatements();
			MIRInstruction returnInstruction( MIRInstructionKind::ReturnValue );
			this->emitTerminator( returnInstruction );
		}
	}
	
} // namespace uranite::ir::mir
