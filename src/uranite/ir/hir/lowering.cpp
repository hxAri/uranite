
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

#include <stdexcept>

#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

#include "uranite/ir/hir/lowering.hpp"

namespace uranite::ir::hir {
	
	static std::string deriveModulePath( const std::string& pathname ) {
		size_t stdlibsPos = pathname.find( "stdlibs/" );
		if( stdlibsPos == std::string::npos ) {
			return "";
		}
		std::string relative = pathname.substr( stdlibsPos + 8 );
		size_t extPos = relative.rfind( ".urn" );
		if( extPos != std::string::npos ) {
			relative = relative.substr( 0, extPos );
		}
		std::string modulePath = "uranite.";
		for( char c : relative ) {
			if( c == '/' ) {
				modulePath += '.';
			}
			else {
				modulePath += c;
			}
		}
		return modulePath;
	}
	
	HIRLowering::HIRLowering( semantic::Analyzer& semanticAnalyzer, diagnostic::Engine& diagnosticEngine )
		: semanticAnalyzer( semanticAnalyzer ),
		  diagnosticEngine( diagnosticEngine ) {
	}
	
	semantic::TypeSharedPointer HIRLowering::resolveTypeNode( const ast::nodes::TypeNodeSharedPointer& typeNode ) {
		if( typeNode == nullptr ) {
			return nullptr;
		}
		return this->semanticAnalyzer.resolveTypeFromNode( typeNode );
	}
	
	std::shared_ptr<HIRModule> HIRLowering::lower( ast::nodes::Program& programRoot ) {
		std::string moduleName = "";
		if( programRoot.module != nullptr ) {
			moduleName = programRoot.module->name;
		}
		std::shared_ptr<HIRModule> hirModule = std::make_shared<HIRModule>( moduleName, programRoot.source );
		for( const ast::nodes::DeclarationSharedPointer& declaration : programRoot.declarations ) {
			if( declaration == nullptr ) {
				continue;
			}
			switch( declaration->kind ) {
				case ast::Node::Kind::FunctionDeclaration: {
					ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *declaration );
					std::shared_ptr<HIRFunctionDefinition> hirFunction = this->lowerFunctionDeclaration( functionDeclaration );
					if( hirFunction != nullptr ) {
						hirModule->functionDefinitions.push_back( std::move( hirFunction ) );
					}
					for( const ast::nodes::DeclarationSharedPointer& nestedDeclaration : functionDeclaration.nestedFunctions ) {
						if( nestedDeclaration != nullptr && nestedDeclaration->kind == ast::Node::Kind::FunctionDeclaration ) {
							ast::nodes::FunctionDeclaration& nestedFunction =
								static_cast<ast::nodes::FunctionDeclaration&>( *nestedDeclaration );
							std::shared_ptr<HIRFunctionDefinition> hirNested = this->lowerFunctionDeclaration( nestedFunction );
							if( hirNested != nullptr ) {
								hirModule->functionDefinitions.push_back( std::move( hirNested ) );
							}
						}
					}
					break;
				}
				case ast::Node::Kind::ClassDeclaration: {
					ast::nodes::ClassDeclaration& classDeclaration = static_cast<ast::nodes::ClassDeclaration&>( *declaration );
					std::shared_ptr<HIRClassDefinition> hirClass = this->lowerClassDeclaration( classDeclaration );
					if( hirClass != nullptr ) {
						hirModule->classDefinitions.push_back( std::move( hirClass ) );
					}
					break;
				}
				case ast::Node::Kind::StructDeclaration: {
					ast::nodes::StructDeclaration& structDeclaration = static_cast<ast::nodes::StructDeclaration&>( *declaration );
					std::shared_ptr<HIRStructDefinition> hirStruct = this->lowerStructDefinition( structDeclaration );
					if( hirStruct != nullptr ) {
						hirModule->structDefinitions.push_back( std::move( hirStruct ) );
					}
					break;
				}
				case ast::Node::Kind::EnumDeclaration: {
					ast::nodes::EnumDeclaration& enumDeclaration = static_cast<ast::nodes::EnumDeclaration&>( *declaration );
					std::shared_ptr<HIREnumDefinition> hirEnum = this->lowerEnumDeclaration( enumDeclaration );
					if( hirEnum != nullptr ) {
						hirModule->enumDefinitions.push_back( std::move( hirEnum ) );
					}
					break;
				}
				case ast::Node::Kind::InterfaceDeclaration: {
					ast::nodes::InterfaceDeclaration& interfaceDeclaration = static_cast<ast::nodes::InterfaceDeclaration&>( *declaration );
					std::shared_ptr<HIRInterfaceDefinition> hirInterface = this->lowerInterfaceDeclaration( interfaceDeclaration );
					if( hirInterface != nullptr ) {
						hirModule->interfaceDefinitions.push_back( std::move( hirInterface ) );
					}
					break;
				}
				case ast::Node::Kind::ExternDeclaration: {
					ast::nodes::ExternDeclaration& externDeclaration = static_cast<ast::nodes::ExternDeclaration&>( *declaration );
					std::shared_ptr<HIRExternFunctionDeclaration> hirExtern = this->lowerExternDeclaration( externDeclaration );
					if( hirExtern != nullptr ) {
						hirModule->externFunctionDeclarations.push_back( std::move( hirExtern ) );
					}
					break;
				}
				case ast::Node::Kind::ConstantDeclaration: {
					ast::nodes::ConstantDeclaration& constantDeclaration = static_cast<ast::nodes::ConstantDeclaration&>( *declaration );
					std::shared_ptr<HIRGlobalVariableDefinition> hirGlobal = this->lowerConstantDeclaration( constantDeclaration );
					if( hirGlobal != nullptr ) {
						bool treatAsGlobalVariable = constantDeclaration.isGlobalVariable;
						if( treatAsGlobalVariable == false && constantDeclaration.initializer != nullptr ) {
							ast::Node::Kind initializerKind = constantDeclaration.initializer->kind;
							if( initializerKind != ast::Node::Kind::IntegerLiteral &&
								initializerKind != ast::Node::Kind::FloatLiteral &&
								initializerKind != ast::Node::Kind::BooleanLiteral &&
								initializerKind != ast::Node::Kind::StringLiteral &&
								initializerKind != ast::Node::Kind::NoneLiteral ) {
								treatAsGlobalVariable = true;
							}
						}
						if( treatAsGlobalVariable ) {
							hirModule->globalVariableDefinitions.push_back( std::move( hirGlobal ) );
						}
						else {
							std::shared_ptr<HIRConstantDefinition> hirConstant = std::make_shared<HIRConstantDefinition>(
								hirGlobal->variableName,
								hirGlobal->variableType,
								hirGlobal->initializerExpression,
								hirGlobal->sourceLocation
							);
							hirConstant->accessModifier = hirGlobal->accessModifier;
							hirModule->constantDefinitions.push_back( std::move( hirConstant ) );
						}
					}
					break;
				}
				default:
					break;
			}
		}
		return hirModule;
	}
	
	std::shared_ptr<HIRFunctionDefinition> HIRLowering::lowerFunctionDeclaration( ast::nodes::FunctionDeclaration& declaration ) {
		semantic::TypeSharedPointer returnType = this->resolveTypeNode( declaration.returnType );
		std::shared_ptr<HIRFunctionDefinition> hirFunction = std::make_shared<HIRFunctionDefinition>(
			declaration.name,
			returnType,
			declaration.source
		);
		hirFunction->accessModifier = declaration.access;
		hirFunction->isVirtualMethod = declaration.isVirtual;
		hirFunction->isStaticMethod = declaration.isStatic;
		hirFunction->isAsyncFunction = declaration.isAsync;
		hirFunction->isOverrideMethod = declaration.isOverride;
		hirFunction->isAbstractMethod = declaration.isAbstract;
		hirFunction->isFinalMethod = declaration.isFinal;
		hirFunction->isConstMethod = declaration.isConst;
		hirFunction->isGeneratorFunction = declaration.isGenerator;
		hirFunction->isNativeMethod = declaration.isNative;
		hirFunction->isPropertyMethod = declaration.isProperty;
		if( declaration.source != nullptr ) {
			std::string modulePath = deriveModulePath( declaration.source->filename );
			if( modulePath.empty() == false ) {
				hirFunction->mangledName = modulePath + "." + declaration.name;
			}
		}
		for( const ast::nodes::FunctionParameterSharedPointer& parameter : declaration.parameters ) {
			if( parameter != nullptr ) {
				hirFunction->parameterDescriptors.push_back( this->lowerParameter( *parameter ) );
			}
		}
		for( const ast::nodes::GenericParameterSharedPointer& genericParameter : declaration.genericParameters ) {
			if( genericParameter != nullptr ) {
				hirFunction->genericParameters.push_back( this->lowerGenericParameter( *genericParameter ) );
			}
		}
		if( declaration.body.empty() == false ) {
			hirFunction->functionBody = this->lowerStatementBlock( declaration.body, declaration.source );
		}
		return hirFunction;
	}
	
	std::shared_ptr<HIRClassDefinition> HIRLowering::lowerClassDeclaration( ast::nodes::ClassDeclaration& declaration ) {
		std::shared_ptr<HIRClassDefinition> hirClass = std::make_shared<HIRClassDefinition>(
			declaration.name,
			declaration.source
		);
		hirClass->accessModifier = declaration.access;
		hirClass->isFinalClass = declaration.isFinal;
		hirClass->isAbstractClass = declaration.isAbstract;
		hirClass->isReadonlyClass = declaration.isReadonly;
		hirClass->isNativeClass = declaration.isNative;
		hirClass->usedTraitNames = declaration.usedTraits;
		if( declaration.baseClassType != nullptr &&
			declaration.baseClassType->kind == ast::Node::Kind::SimpleType ) {
			ast::nodes::SimpleTypeNode& baseType =
				static_cast<ast::nodes::SimpleTypeNode&>( *declaration.baseClassType );
			hirClass->parentClassQualifiedName = baseType.name;
			{
				semantic::TypeSharedPointer parentType = this->semanticAnalyzer.types().lookupType( baseType.name );
				if( parentType != nullptr && parentType->qualified.empty() == false ) {
					hirClass->parentClassQualifiedName = parentType->qualified;
				}
			}
		}
		int fieldIndex = 0;
		for( const ast::nodes::FieldDeclarationSharedPointer& field : declaration.fields ) {
			if( field != nullptr ) {
				hirClass->fieldDescriptors.push_back( this->lowerFieldDeclaration( *field, fieldIndex++ ) );
			}
		}
		for( const ast::nodes::GenericParameterSharedPointer& genericParameter : declaration.genericParameters ) {
			if( genericParameter != nullptr ) {
				hirClass->genericParameters.push_back( this->lowerGenericParameter( *genericParameter ) );
			}
		}
		semantic::TypeSharedPointer classType = this->semanticAnalyzer.types().lookupType( declaration.name );
		std::string classQualifiedName = ( classType != nullptr && classType->qualified.empty() == false )
			? classType->qualified : declaration.name;
		hirClass->classQualifiedName = classQualifiedName;
		for( const ast::nodes::TypeNodeSharedPointer& interfaceNode : declaration.interfaces ) {
			if( interfaceNode == nullptr ) {
				continue;
			}
			std::string interfaceName;
			if( interfaceNode->kind == ast::Node::Kind::SimpleType ) {
				interfaceName = static_cast<ast::nodes::SimpleTypeNode&>( *interfaceNode ).name;
			}
			else if( interfaceNode->kind == ast::Node::Kind::GenericType ) {
				interfaceName = static_cast<ast::nodes::GenericTypeNode&>( *interfaceNode ).name;
			}
			if( interfaceName.empty() == false ) {
				std::string interfaceQualified = interfaceName;
				semantic::TypeSharedPointer interfaceSemaType = this->semanticAnalyzer.types().lookupType( interfaceName );
				if( interfaceSemaType != nullptr && interfaceSemaType->qualified.empty() == false ) {
					interfaceQualified = interfaceSemaType->qualified;
				}
				hirClass->implementedInterfaceQualifiedNames.push_back( interfaceQualified );
			}
		}
		for( const ast::nodes::DeclarationSharedPointer& method : declaration.methods ) {
			if( method != nullptr && method->kind == ast::Node::Kind::FunctionDeclaration ) {
				ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *method );
				std::shared_ptr<HIRFunctionDefinition> hirMethod = this->lowerFunctionDeclaration( functionDeclaration );
				if( hirMethod != nullptr ) {
					hirMethod->ownerClassName = classQualifiedName;
					for( HIRParameterDescriptor& parameterDescriptor : hirMethod->parameterDescriptors ) {
						if( parameterDescriptor.isSelfParameter && parameterDescriptor.parameterType == nullptr ) {
							parameterDescriptor.parameterType = classType;
						}
					}
					hirClass->methodDefinitions.push_back( std::move( hirMethod ) );
				}
			}
		}
		return hirClass;
	}
	
	std::shared_ptr<HIRStructDefinition> HIRLowering::lowerStructDefinition( ast::nodes::StructDeclaration& declaration ) {
		semantic::TypeSharedPointer structType = this->semanticAnalyzer.types().lookupType( declaration.name );
		std::string structQualifiedName = ( structType != nullptr && structType->qualified.empty() == false )
			? structType->qualified : declaration.name;
		std::shared_ptr<HIRStructDefinition> hirStruct = std::make_shared<HIRStructDefinition>(
			declaration.name,
			declaration.source
		);
		hirStruct->accessModifier = declaration.access;
		hirStruct->usedTraitNames = declaration.usedTraits;
		hirStruct->structQualifiedName = structQualifiedName;
		int fieldIndex = 0;
		for( const ast::nodes::FieldDeclarationSharedPointer& field : declaration.fields ) {
			if( field != nullptr ) {
				hirStruct->fieldDescriptors.push_back( this->lowerFieldDeclaration( *field, fieldIndex++ ) );
			}
		}
		for( const ast::nodes::GenericParameterSharedPointer& genericParameter : declaration.genericParameters ) {
			if( genericParameter != nullptr ) {
				hirStruct->genericParameters.push_back( this->lowerGenericParameter( *genericParameter ) );
			}
		}
		for( const ast::nodes::DeclarationSharedPointer& method : declaration.methods ) {
			if( method != nullptr && method->kind == ast::Node::Kind::FunctionDeclaration ) {
				ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *method );
				std::shared_ptr<HIRFunctionDefinition> hirMethod = this->lowerFunctionDeclaration( functionDeclaration );
				if( hirMethod != nullptr ) {
					hirMethod->ownerClassName = structQualifiedName;
					for( HIRParameterDescriptor& parameterDescriptor : hirMethod->parameterDescriptors ) {
						if( parameterDescriptor.isSelfParameter && parameterDescriptor.parameterType == nullptr ) {
							parameterDescriptor.parameterType = structType;
						}
					}
					hirStruct->methodDefinitions.push_back( std::move( hirMethod ) );
				}
			}
		}
		return hirStruct;
	}
	
	std::shared_ptr<HIREnumDefinition> HIRLowering::lowerEnumDeclaration( ast::nodes::EnumDeclaration& declaration ) {
		std::shared_ptr<HIREnumDefinition> hirEnum = std::make_shared<HIREnumDefinition>(
			declaration.name,
			declaration.source
		);
		hirEnum->accessModifier = declaration.access;
		semantic::TypeSharedPointer enumType = this->semanticAnalyzer.types().lookupType( declaration.name );
		std::string enumQualifiedName = ( enumType != nullptr && enumType->qualified.empty() == false )
			? enumType->qualified : declaration.name;
		for( const ast::nodes::GenericParameterSharedPointer& genericParameter : declaration.genericParameters ) {
			if( genericParameter != nullptr ) {
				hirEnum->genericParameters.push_back( this->lowerGenericParameter( *genericParameter ) );
			}
		}
		for( const ast::nodes::EnumVariantSharedPointer& variant : declaration.variants ) {
			if( variant == nullptr ) {
				continue;
			}
			HIREnumVariantDescriptor variantDescriptor;
			variantDescriptor.variantName = variant->name;
			if( variant->backedValue != nullptr ) {
				variantDescriptor.backedValueExpression = this->lowerExpression( variant->backedValue );
			}
			if( variant->discriminant != nullptr ) {
				variantDescriptor.discriminantExpression = this->lowerExpression( variant->discriminant );
			}
			int variantFieldIndex = 0;
			for( const ast::nodes::FieldDeclarationSharedPointer& field : variant->fields ) {
				if( field != nullptr ) {
					variantDescriptor.variantFields.push_back( this->lowerFieldDeclaration( *field, variantFieldIndex++ ) );
				}
			}
			for( const ast::nodes::DeclarationSharedPointer& method : variant->methods ) {
				if( method != nullptr && method->kind == ast::Node::Kind::FunctionDeclaration ) {
					ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *method );
					std::shared_ptr<HIRFunctionDefinition> hirMethod = this->lowerFunctionDeclaration( functionDeclaration );
					if( hirMethod != nullptr ) {
						variantDescriptor.variantMethods.push_back( std::move( hirMethod ) );
					}
				}
			}
			hirEnum->variantDescriptors.push_back( std::move( variantDescriptor ) );
		}
		for( const ast::nodes::DeclarationSharedPointer& method : declaration.methods ) {
			if( method != nullptr && method->kind == ast::Node::Kind::FunctionDeclaration ) {
				ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *method );
				std::shared_ptr<HIRFunctionDefinition> hirMethod = this->lowerFunctionDeclaration( functionDeclaration );
				if( hirMethod != nullptr ) {
					hirMethod->ownerClassName = enumQualifiedName;
					hirEnum->methodDefinitions.push_back( std::move( hirMethod ) );
				}
			}
		}
		return hirEnum;
	}
	
	std::shared_ptr<HIRInterfaceDefinition> HIRLowering::lowerInterfaceDeclaration( ast::nodes::InterfaceDeclaration& declaration ) {
		std::shared_ptr<HIRInterfaceDefinition> hirInterface = std::make_shared<HIRInterfaceDefinition>(
			declaration.name,
			declaration.source
		);
		hirInterface->accessModifier = declaration.access;
		hirInterface->isFinalInterface = declaration.isFinal;
		semantic::TypeSharedPointer interfaceType = this->semanticAnalyzer.types().lookupType( declaration.name );
		std::string interfaceQualifiedName = ( interfaceType != nullptr && interfaceType->qualified.empty() == false )
			? interfaceType->qualified : declaration.name;
		for( const ast::nodes::GenericParameterSharedPointer& genericParameter : declaration.genericParameters ) {
			if( genericParameter != nullptr ) {
				hirInterface->genericParameters.push_back( this->lowerGenericParameter( *genericParameter ) );
			}
		}
		for( const ast::nodes::DeclarationSharedPointer& method : declaration.methods ) {
			if( method != nullptr && method->kind == ast::Node::Kind::FunctionDeclaration ) {
				ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *method );
				std::shared_ptr<HIRFunctionDefinition> hirMethod = this->lowerFunctionDeclaration( functionDeclaration );
				if( hirMethod != nullptr ) {
					hirMethod->ownerClassName = interfaceQualifiedName;
					hirInterface->methodDefinitions.push_back( std::move( hirMethod ) );
				}
			}
		}
		return hirInterface;
	}
	
	std::shared_ptr<HIRExternFunctionDeclaration> HIRLowering::lowerExternDeclaration( ast::nodes::ExternDeclaration& declaration ) {
		std::shared_ptr<HIRExternFunctionDeclaration> hirExtern = std::make_shared<HIRExternFunctionDeclaration>(
			declaration.name,
			declaration.linkName,
			declaration.source
		);
		hirExtern->accessModifier = declaration.access;
		hirExtern->isVariadicFunction = declaration.isVariadic;
		for( const ast::nodes::ExternParameter& parameter : declaration.parameters ) {
			HIRParameterDescriptor parameterDescriptor;
			parameterDescriptor.parameterName = parameter.name;
			parameterDescriptor.parameterType = this->resolveTypeNode( parameter.type );
			hirExtern->parameterDescriptors.push_back( std::move( parameterDescriptor ) );
		}
		hirExtern->returnTypeDescriptor = this->resolveTypeNode( declaration.returnType );
		return hirExtern;
	}
	
	std::shared_ptr<HIRGlobalVariableDefinition> HIRLowering::lowerConstantDeclaration( ast::nodes::ConstantDeclaration& declaration ) {
		semantic::TypeSharedPointer variableType = this->resolveTypeNode( declaration.type );
		HIRNodeSharedPointer initializerExpression = nullptr;
		if( declaration.initializer != nullptr ) {
			initializerExpression = this->lowerExpression( declaration.initializer );
		}
		std::shared_ptr<HIRGlobalVariableDefinition> hirGlobal = std::make_shared<HIRGlobalVariableDefinition>(
			declaration.name,
			variableType,
			initializerExpression,
			declaration.source
		);
		hirGlobal->accessModifier = declaration.access;
		hirGlobal->isGlobalVariable = declaration.isGlobalVariable;
		return hirGlobal;
	}
	
	std::shared_ptr<HIRBlock> HIRLowering::lowerStatementBlock(
		const std::vector<ast::nodes::StatementSharedPointer>& statements,
		const lookup::SourceSharedPointer& sourceLocation
	) {
		std::shared_ptr<HIRBlock> hirBlock = std::make_shared<HIRBlock>( sourceLocation );
		for( const ast::nodes::StatementSharedPointer& statement : statements ) {
			if( statement == nullptr ) {
				continue;
			}
			HIRNodeSharedPointer loweredStatement = this->lowerStatement( statement );
			if( loweredStatement != nullptr ) {
				hirBlock->blockStatements.push_back( std::move( loweredStatement ) );
			}
		}
		return hirBlock;
	}
	
	HIRNodeSharedPointer HIRLowering::lowerStatement( const ast::nodes::StatementSharedPointer& statement ) {
		if( statement == nullptr ) {
			return nullptr;
		}
		switch( statement->kind ) {
			case ast::Node::Kind::VariableStatement: {
				ast::nodes::VariableStatement& variableStatement = static_cast<ast::nodes::VariableStatement&>( *statement );
				semantic::TypeSharedPointer variableType = this->resolveTypeNode( variableStatement.type );
				HIRNodeSharedPointer initializerExpression = nullptr;
				if( variableStatement.initializer != nullptr ) {
					initializerExpression = this->lowerExpression( variableStatement.initializer );
				}
				std::shared_ptr<HIRVariableBinding> hirVariable = std::make_shared<HIRVariableBinding>(
					variableStatement.name,
					variableType,
					initializerExpression,
					variableStatement.source
				);
				hirVariable->isMutableBinding = variableStatement.isMutable;
				hirVariable->isConstantBinding = variableStatement.isConstant;
				if( variableStatement.ownershipAnnotation.has_value() ) {
					hirVariable->isHeapAllocated = variableStatement.ownershipAnnotation->isHeapAllocated;
				}
				return hirVariable;
			}
			case ast::Node::Kind::AssignmentStatement: {
				ast::nodes::AssignStatement& assignStatement = static_cast<ast::nodes::AssignStatement&>( *statement );
				HIRNodeSharedPointer targetExpression = this->lowerExpression( assignStatement.target );
				HIRNodeSharedPointer valueExpression = this->lowerExpression( assignStatement.value );
				return std::make_shared<HIRAssignment>(
					assignStatement.operation,
					std::move( targetExpression ),
					std::move( valueExpression ),
					assignStatement.source
				);
			}
			case ast::Node::Kind::ReturnStatement: {
				ast::nodes::ReturnStatement& returnStatement = static_cast<ast::nodes::ReturnStatement&>( *statement );
				HIRNodeSharedPointer returnValue = nullptr;
				if( returnStatement.value != nullptr ) {
					returnValue = this->lowerExpression( returnStatement.value );
				}
				return std::make_shared<HIRReturn>( std::move( returnValue ), returnStatement.source );
			}
			case ast::Node::Kind::IfStatement: {
				ast::nodes::IfStatement& ifStatement = static_cast<ast::nodes::IfStatement&>( *statement );
				return this->desugarIfStatement( ifStatement );
			}
			case ast::Node::Kind::ForStatement: {
				ast::nodes::ForStatement& forStatement = static_cast<ast::nodes::ForStatement&>( *statement );
				return this->desugarForStatement( forStatement );
			}
			case ast::Node::Kind::WhileStatement: {
				ast::nodes::WhileStatement& whileStatement = static_cast<ast::nodes::WhileStatement&>( *statement );
				return this->desugarWhileStatement( whileStatement );
			}
			case ast::Node::Kind::BreakStatement: {
				return std::make_shared<HIRBreak>( statement->source );
			}
			case ast::Node::Kind::ContinueStatement: {
				return std::make_shared<HIRContinue>( statement->source );
			}
			case ast::Node::Kind::DeferStatement: {
				ast::nodes::DeferStatement& deferStatement = static_cast<ast::nodes::DeferStatement&>( *statement );
				HIRNodeSharedPointer deferredBody = this->lowerStatement( deferStatement.body );
				return std::make_shared<HIRDefer>( std::move( deferredBody ), deferStatement.source );
			}
			case ast::Node::Kind::DeleteStatement: {
				ast::nodes::DeleteStatement& deleteStatement = static_cast<ast::nodes::DeleteStatement&>( *statement );
				HIRNodeSharedPointer targetExpression = this->lowerExpression( deleteStatement.expression );
				return std::make_shared<HIRDelete>( std::move( targetExpression ), deleteStatement.source );
			}
			case ast::Node::Kind::ThrowStatement: {
				ast::nodes::ThrowStatement& throwStatement = static_cast<ast::nodes::ThrowStatement&>( *statement );
				HIRNodeSharedPointer thrownExpression = this->lowerExpression( throwStatement.expression );
				return std::make_shared<HIRThrow>( std::move( thrownExpression ), throwStatement.source );
			}
			case ast::Node::Kind::TryCatchStatement: {
				ast::nodes::TryCatchStatement& tryCatchStatement = static_cast<ast::nodes::TryCatchStatement&>( *statement );
				std::shared_ptr<HIRTryCatch> hirTryCatch = std::make_shared<HIRTryCatch>( tryCatchStatement.source );
				hirTryCatch->tryBody = this->lowerStatementBlock( tryCatchStatement.tryBody, tryCatchStatement.source );
				for( const ast::nodes::ExceptionClause& clause : tryCatchStatement.exceptionClauses ) {
					HIRExceptionHandler handler;
					handler.exceptionVariableName = clause.variableName;
					handler.sourceLocation = clause.source;
					for( const ast::nodes::TypeNodeSharedPointer& exceptionType : clause.exceptionTypes ) {
						semantic::TypeSharedPointer resolvedExceptionType = this->resolveTypeNode( exceptionType );
						if( resolvedExceptionType != nullptr ) {
							handler.exceptionTypes.push_back( resolvedExceptionType );
						}
					}
					handler.handlerBody = this->lowerStatementBlock( clause.body, clause.source );
					hirTryCatch->exceptionHandlers.push_back( std::move( handler ) );
				}
				if( tryCatchStatement.finallyBody.empty() == false ) {
					hirTryCatch->finallyBody = this->lowerStatementBlock( tryCatchStatement.finallyBody, tryCatchStatement.source );
				}
				return hirTryCatch;
			}
			case ast::Node::Kind::MatchStatement: {
				ast::nodes::MatchStatement& matchStatement = static_cast<ast::nodes::MatchStatement&>( *statement );
				HIRNodeSharedPointer matchSubject = this->lowerExpression( matchStatement.subject );
				std::vector<HIRMatchArm> matchArms;
				for( const ast::nodes::MatchArmNodeSharedPointer& arm : matchStatement.arms ) {
					if( arm == nullptr ) {
						continue;
					}
					HIRMatchArm hirArm;
					hirArm.armPattern = this->lowerExpression( arm->pattern );
					if( arm->guard != nullptr ) {
						hirArm.armGuard = this->lowerExpression( arm->guard );
					}
					hirArm.armBody = this->lowerStatementBlock( arm->body, arm->source );
					matchArms.push_back( std::move( hirArm ) );
				}
				return std::make_shared<HIRMatch>( std::move( matchSubject ), std::move( matchArms ), matchStatement.source );
			}
			case ast::Node::Kind::SwitchStatement: {
				ast::nodes::SwitchStatement& switchStatement = static_cast<ast::nodes::SwitchStatement&>( *statement );
				HIRNodeSharedPointer switchSubject = this->lowerExpression( switchStatement.subject );
				std::vector<HIRSwitchCase> switchCases;
				for( const ast::nodes::SwitchCaseNodeSharedPointer& caseNode : switchStatement.cases ) {
					if( caseNode == nullptr ) {
						continue;
					}
					HIRSwitchCase hirCase;
					hirCase.isDefaultCase = caseNode->isDefault;
					if( caseNode->pattern != nullptr ) {
						hirCase.casePattern = this->lowerExpression( caseNode->pattern );
					}
					hirCase.caseBody = this->lowerStatementBlock( caseNode->body, caseNode->source );
					switchCases.push_back( std::move( hirCase ) );
				}
				return std::make_shared<HIRSwitch>( std::move( switchSubject ), std::move( switchCases ), switchStatement.source );
			}
			case ast::Node::Kind::InlineAssemblyStatement: {
				ast::nodes::InlineAssemblyStatement& asmStatement = static_cast<ast::nodes::InlineAssemblyStatement&>( *statement );
				std::shared_ptr<HIRInlineAssembly> hirAsm = std::make_shared<HIRInlineAssembly>( asmStatement.source );
				hirAsm->isVolatile = asmStatement.isVolatile;
				if( asmStatement.archVariants.empty() == false ) {
					llvm::Triple triple( llvm::sys::getDefaultTargetTriple() );
					std::string targetArch;
					switch( triple.getArch() ) {
						case llvm::Triple::x86_64:
							targetArch = "x86-64";
							break;
						case llvm::Triple::aarch64:
						case llvm::Triple::aarch64_be:
							targetArch = "aarch64";
							break;
						case llvm::Triple::riscv64:
							targetArch = "riscv64";
							break;
						case llvm::Triple::arm:
						case llvm::Triple::armeb:
							targetArch = "arm";
							break;
						default:
							targetArch = triple.getArchName().str();
							break;
					}
					for( const ast::nodes::InlineAssemblyArchVariant& variant : asmStatement.archVariants ) {
						if( variant.targetArch == targetArch ) {
							hirAsm->assemblyTemplate = variant.asmTemplate;
							hirAsm->clobberRegisters = variant.clobbers;
							for( const ast::nodes::InlineAssemblyOperand& output : variant.outputs ) {
								HIRAsmOperand hirOperand;
								hirOperand.constraintString = output.constraint;
								hirOperand.boundExpression = this->lowerExpression( output.expression );
								hirAsm->outputOperands.push_back( std::move( hirOperand ) );
							}
							for( const ast::nodes::InlineAssemblyOperand& input : variant.inputs ) {
								HIRAsmOperand hirOperand;
								hirOperand.constraintString = input.constraint;
								hirOperand.boundExpression = this->lowerExpression( input.expression );
								hirAsm->inputOperands.push_back( std::move( hirOperand ) );
							}
							return hirAsm;
						}
					}
					return hirAsm;
				}
				hirAsm->assemblyTemplate = asmStatement.asmTemplate;
				hirAsm->clobberRegisters = asmStatement.clobbers;
				for( const ast::nodes::InlineAssemblyOperand& output : asmStatement.outputs ) {
					HIRAsmOperand hirOperand;
					hirOperand.constraintString = output.constraint;
					hirOperand.boundExpression = this->lowerExpression( output.expression );
					hirAsm->outputOperands.push_back( std::move( hirOperand ) );
				}
				for( const ast::nodes::InlineAssemblyOperand& input : asmStatement.inputs ) {
					HIRAsmOperand hirOperand;
					hirOperand.constraintString = input.constraint;
					hirOperand.boundExpression = this->lowerExpression( input.expression );
					hirAsm->inputOperands.push_back( std::move( hirOperand ) );
				}
				return hirAsm;
			}
			case ast::Node::Kind::UnsafeBlock: {
				ast::nodes::UnsafeBlockStatement& unsafeBlock = static_cast<ast::nodes::UnsafeBlockStatement&>( *statement );
				std::shared_ptr<HIRBlock> unsafeBody = this->lowerStatementBlock( unsafeBlock.body, unsafeBlock.source );
				return std::make_shared<HIRUnsafeBlock>( std::move( unsafeBody ), unsafeBlock.source );
			}
			case ast::Node::Kind::ExpressionStatement: {
				ast::nodes::ExpressionStatement& expressionStatement = static_cast<ast::nodes::ExpressionStatement&>( *statement );
				HIRNodeSharedPointer expression = this->lowerExpression( expressionStatement.expression );
				return std::make_shared<HIRExpressionStatement>( std::move( expression ), expressionStatement.source );
			}
			case ast::Node::Kind::PassStatement: {
				return std::make_shared<HIRPass>( statement->source );
			}
			case ast::Node::Kind::BlockStatement: {
				ast::nodes::BlockStatement& blockStatement = static_cast<ast::nodes::BlockStatement&>( *statement );
				return this->lowerStatementBlock( blockStatement.statements, blockStatement.source );
			}
			default:
				return nullptr;
		}
	}
	
	HIRNodeSharedPointer HIRLowering::lowerExpression( const ast::nodes::ExpressionSharedPointer& expression ) {
		if( expression == nullptr ) {
			return nullptr;
		}
		switch( expression->kind ) {
			case ast::Node::Kind::IntegerLiteral: {
				ast::nodes::IntegerLiteralExpression& integerLiteral = static_cast<ast::nodes::IntegerLiteralExpression&>( *expression );
				return std::make_shared<HIRIntegerLiteral>(
					integerLiteral.value,
					integerLiteral.raw,
					expression->semanticType,
					expression->source
				);
			}
			case ast::Node::Kind::FloatLiteral: {
				ast::nodes::FloatLiteralExpression& floatLiteral = static_cast<ast::nodes::FloatLiteralExpression&>( *expression );
				return std::make_shared<HIRFloatLiteral>(
					floatLiteral.value,
					floatLiteral.rawRepresentation,
					expression->semanticType,
					expression->source
				);
			}
			case ast::Node::Kind::BooleanLiteral: {
				ast::nodes::BoolLiteralExpression& boolLiteral = static_cast<ast::nodes::BoolLiteralExpression&>( *expression );
				return std::make_shared<HIRBooleanLiteral>(
					boolLiteral.value,
					expression->semanticType,
					expression->source
				);
			}
			case ast::Node::Kind::StringLiteral: {
				ast::nodes::StringLiteralExpression& stringLiteral = static_cast<ast::nodes::StringLiteralExpression&>( *expression );
				return std::make_shared<HIRStringLiteral>(
					stringLiteral.value,
					expression->semanticType,
					expression->source
				);
			}
			case ast::Node::Kind::CharLiteral: {
				ast::nodes::CharLiteralExpression& charLiteral = static_cast<ast::nodes::CharLiteralExpression&>( *expression );
				return std::make_shared<HIRCharLiteral>(
					charLiteral.value,
					expression->semanticType,
					expression->source
				);
			}
			case ast::Node::Kind::NoneLiteral: {
				return std::make_shared<HIRNoneLiteral>( expression->semanticType, expression->source );
			}
			case ast::Node::Kind::IdentifierExpression: {
				ast::nodes::IdentifierExpression& identifierExpression = static_cast<ast::nodes::IdentifierExpression&>( *expression );
				std::shared_ptr<HIRIdentifier> hirIdentifier = std::make_shared<HIRIdentifier>(
					identifierExpression.name,
					expression->semanticType,
					expression->source
				);
				if( identifierExpression.resolvedSymbol != nullptr &&
					identifierExpression.resolvedSymbol->kind == semantic::Symbol::Kind::Function &&
					identifierExpression.resolvedSymbol->source != nullptr ) {
					std::string modulePath = deriveModulePath( identifierExpression.resolvedSymbol->source->filename );
					if( modulePath.empty() == false ) {
						hirIdentifier->qualifiedScopeName = modulePath + "." + identifierExpression.name;
					}
				}
				return hirIdentifier;
			}
			case ast::Node::Kind::SelfExpression: {
				return std::make_shared<HIRSelfReference>( expression->semanticType, expression->source );
			}
			case ast::Node::Kind::SuperExpression: {
				return std::make_shared<HIRSuperReference>( expression->semanticType, expression->source );
			}
			case ast::Node::Kind::BinaryExpression: {
				ast::nodes::BinaryExpression& binaryExpression = static_cast<ast::nodes::BinaryExpression&>( *expression );
				HIRNodeSharedPointer leftOperand = this->lowerExpression( binaryExpression.left );
				HIRNodeSharedPointer rightOperand = this->lowerExpression( binaryExpression.right );
				return std::make_shared<HIRBinaryOperation>(
					binaryExpression.operation,
					std::move( leftOperand ),
					std::move( rightOperand ),
					expression->semanticType,
					expression->source
				);
			}
			case ast::Node::Kind::UnaryExpression: {
				ast::nodes::UnaryExpression& unaryExpression = static_cast<ast::nodes::UnaryExpression&>( *expression );
				if( unaryExpression.operation == token::Type::KeywordMove ) {
					HIRNodeSharedPointer operand = this->lowerExpression( unaryExpression.operand );
					return std::make_shared<HIRMoveTransfer>( std::move( operand ), expression->semanticType, expression->source );
				}
				if( unaryExpression.operation == token::Type::KeywordAddressof ) {
					HIRNodeSharedPointer operand = this->lowerExpression( unaryExpression.operand );
					return std::make_shared<HIRAddressOf>( std::move( operand ), expression->semanticType, expression->source );
				}
				if( unaryExpression.operation == token::Type::Ampersand ) {
					HIRNodeSharedPointer operand = this->lowerExpression( unaryExpression.operand );
					return std::make_shared<HIRReference>( std::move( operand ), expression->semanticType, expression->source );
				}
				if( unaryExpression.operation == token::Type::Star && unaryExpression.isPrefix ) {
					HIRNodeSharedPointer operand = this->lowerExpression( unaryExpression.operand );
					return std::make_shared<HIRDereference>( std::move( operand ), expression->semanticType, expression->source );
				}
				HIRNodeSharedPointer operand = this->lowerExpression( unaryExpression.operand );
				return std::make_shared<HIRUnaryOperation>(
					unaryExpression.operation,
					std::move( operand ),
					unaryExpression.isPrefix,
					expression->semanticType,
					expression->source
				);
			}
			case ast::Node::Kind::CallExpression: {
				ast::nodes::CallExpression& callExpression = static_cast<ast::nodes::CallExpression&>( *expression );
				HIRNodeSharedPointer calleeExpression = this->lowerExpression( callExpression.callee );
				std::vector<HIRNodeSharedPointer> callArguments;
				for( const ast::nodes::ExpressionSharedPointer& argument : callExpression.arguments ) {
					callArguments.push_back( this->lowerExpression( argument ) );
				}
				std::shared_ptr<HIRFunctionCall> hirCall = std::make_shared<HIRFunctionCall>(
					std::move( calleeExpression ),
					std::move( callArguments ),
					expression->semanticType,
					expression->source
				);
				for( const ast::nodes::KeywordArgument& keywordArgument : callExpression.keywordArguments ) {
					hirCall->keywordArguments.push_back( { keywordArgument.name, this->lowerExpression( keywordArgument.value ) } );
				}
				return hirCall;
			}
			case ast::Node::Kind::MethodCallExpression: {
				ast::nodes::MethodCallExpression& methodCallExpression = static_cast<ast::nodes::MethodCallExpression&>( *expression );
				HIRNodeSharedPointer receiverObject = this->lowerExpression( methodCallExpression.object );
				std::vector<HIRNodeSharedPointer> callArguments;
				for( const ast::nodes::ExpressionSharedPointer& argument : methodCallExpression.arguments ) {
					callArguments.push_back( this->lowerExpression( argument ) );
				}
				std::shared_ptr<HIRMethodCall> hirMethodCall = std::make_shared<HIRMethodCall>(
					std::move( receiverObject ),
					methodCallExpression.method,
					std::move( callArguments ),
					expression->semanticType,
					expression->source
				);
				for( const ast::nodes::KeywordArgument& keywordArgument : methodCallExpression.keywordArguments ) {
					hirMethodCall->keywordArguments.push_back( { keywordArgument.name, this->lowerExpression( keywordArgument.value ) } );
				}
				return hirMethodCall;
			}
			case ast::Node::Kind::MemberAccessExpression: {
				ast::nodes::MemberAccessExpression& memberAccess = static_cast<ast::nodes::MemberAccessExpression&>( *expression );
				HIRNodeSharedPointer objectExpression = this->lowerExpression( memberAccess.object );
				bool isPropertyAccess = false;
				if( memberAccess.object->semanticType != nullptr ) {
					semantic::TypeSharedPointer objectType = memberAccess.object->semanticType;
					semantic::MethodInfo* methodInfo = nullptr;
					bool hasField = false;
					if( objectType->kind == semantic::Type::Kind::Class ) {
						semantic::ClassTypeSharedPointer classType = std::static_pointer_cast<semantic::ClassType>( objectType );
						methodInfo = classType->findMethod( memberAccess.member );
						hasField = ( classType->findField( memberAccess.member ) != nullptr );
					}
					else if( objectType->kind == semantic::Type::Kind::Interface ) {
						methodInfo = std::static_pointer_cast<semantic::InterfaceType>( objectType )->findMethod( memberAccess.member );
					}
					else if( objectType->kind == semantic::Type::Kind::Trait ) {
						methodInfo = std::static_pointer_cast<semantic::TraitType>( objectType )->findMethod( memberAccess.member );
					}
					else if( objectType->kind == semantic::Type::Kind::Struct ) {
						semantic::StructTypeSharedPointer structType = std::static_pointer_cast<semantic::StructType>( objectType );
						methodInfo = structType->findMethod( memberAccess.member );
						hasField = ( structType->findField( memberAccess.member ) != nullptr );
					}
					if( methodInfo != nullptr && methodInfo->isProperty && hasField == false ) {
						isPropertyAccess = true;
					}
				}
				if( isPropertyAccess ) {
					std::vector<HIRNodeSharedPointer> emptyArguments;
					return std::make_shared<HIRMethodCall>(
						std::move( objectExpression ),
						memberAccess.member,
						std::move( emptyArguments ),
						expression->semanticType,
						expression->source
					);
				}
				return std::make_shared<HIRFieldAccess>(
					std::move( objectExpression ),
					memberAccess.member,
					expression->semanticType,
					expression->source
				);
			}
			case ast::Node::Kind::IndexExpression: {
				ast::nodes::IndexExpression& indexExpression = static_cast<ast::nodes::IndexExpression&>( *expression );
				HIRNodeSharedPointer objectExpression = this->lowerExpression( indexExpression.object );
				HIRNodeSharedPointer indexValue = this->lowerExpression( indexExpression.index );
				return std::make_shared<HIRIndexAccess>(
					std::move( objectExpression ),
					std::move( indexValue ),
					expression->semanticType,
					expression->source
				);
			}
			case ast::Node::Kind::ConstructExpression: {
				ast::nodes::ConstructExpression& constructExpression = static_cast<ast::nodes::ConstructExpression&>( *expression );
				std::vector<std::pair<std::string, HIRNodeSharedPointer>> constructorFields;
				for( const std::pair<std::string, ast::nodes::ExpressionSharedPointer>& field : constructExpression.fields ) {
					constructorFields.push_back( { field.first, this->lowerExpression( field.second ) } );
				}
				semantic::TypeSharedPointer constructedType = expression->semanticType;
				if( constructedType == nullptr && constructExpression.type != nullptr ) {
					std::string typeName;
					if( constructExpression.type->kind == ast::Node::Kind::SimpleType ) {
						typeName = static_cast<ast::nodes::SimpleTypeNode&>( *constructExpression.type ).name;
					}
					else if( constructExpression.type->kind == ast::Node::Kind::GenericType ) {
						typeName = static_cast<ast::nodes::GenericTypeNode&>( *constructExpression.type ).name;
					}
					if( typeName.empty() == false ) {
						constructedType = std::make_shared<semantic::Type>( semantic::Type::Kind::Class, typeName );
					}
				}
				return std::make_shared<HIRConstruct>(
					constructedType,
					std::move( constructorFields ),
					expression->source
				);
			}
			case ast::Node::Kind::CastExpression: {
				ast::nodes::CastExpression& castExpression = static_cast<ast::nodes::CastExpression&>( *expression );
				HIRNodeSharedPointer sourceExpression = this->lowerExpression( castExpression.expression );
				semantic::TypeSharedPointer targetType = this->resolveTypeNode( castExpression.targetType );
				return std::make_shared<HIRCast>( std::move( sourceExpression ), targetType, expression->source );
			}
			case ast::Node::Kind::AwaitExpression: {
				ast::nodes::AwaitExpression& awaitExpression = static_cast<ast::nodes::AwaitExpression&>( *expression );
				HIRNodeSharedPointer awaitedExpression = this->lowerExpression( awaitExpression.operand );
				return std::make_shared<HIRAwait>( std::move( awaitedExpression ), expression->semanticType, expression->source );
			}
			case ast::Node::Kind::YieldExpression: {
				ast::nodes::YieldExpression& yieldExpression = static_cast<ast::nodes::YieldExpression&>( *expression );
				HIRNodeSharedPointer yieldedExpression = this->lowerExpression( yieldExpression.value );
				return std::make_shared<HIRYield>( std::move( yieldedExpression ), expression->semanticType, expression->source );
			}
			case ast::Node::Kind::LambdaExpression: {
				ast::nodes::LambdaExpression& lambdaExpression = static_cast<ast::nodes::LambdaExpression&>( *expression );
				std::shared_ptr<HIRLambda> hirLambda = std::make_shared<HIRLambda>( expression->semanticType, expression->source );
				hirLambda->isAsyncLambda = lambdaExpression.isAsync;
				for( const ast::nodes::FunctionParameterSharedPointer& parameter : lambdaExpression.parameters ) {
					if( parameter != nullptr ) {
						hirLambda->parameterDescriptors.push_back( this->lowerParameter( *parameter ) );
					}
				}
				hirLambda->returnTypeDescriptor = this->resolveTypeNode( lambdaExpression.returnType );
				hirLambda->lambdaBody = this->lowerStatementBlock( lambdaExpression.body, lambdaExpression.source );
				return hirLambda;
			}
			case ast::Node::Kind::MatchExpression: {
				ast::nodes::MatchExpression& matchExpression = static_cast<ast::nodes::MatchExpression&>( *expression );
				std::shared_ptr<HIRMatchExpression> hirMatch = std::make_shared<HIRMatchExpression>(
					this->lowerExpression( matchExpression.subject ),
					expression->semanticType,
					expression->source
				);
				for( const ast::nodes::ExpressionSharedPointer& pattern : matchExpression.patterns ) {
					hirMatch->matchPatterns.push_back( this->lowerExpression( pattern ) );
				}
				for( const ast::nodes::ExpressionSharedPointer& value : matchExpression.values ) {
					hirMatch->matchValues.push_back( this->lowerExpression( value ) );
				}
				if( matchExpression.defaultValue != nullptr ) {
					hirMatch->matchDefaultValue = this->lowerExpression( matchExpression.defaultValue );
				}
				return hirMatch;
			}
			case ast::Node::Kind::ArrayExpression: {
				ast::nodes::ArrayExpression& arrayExpression = static_cast<ast::nodes::ArrayExpression&>( *expression );
				std::vector<HIRNodeSharedPointer> elementExpressions;
				for( const ast::nodes::ExpressionSharedPointer& element : arrayExpression.elements ) {
					elementExpressions.push_back( this->lowerExpression( element ) );
				}
				return std::make_shared<HIRArrayLiteral>( std::move( elementExpressions ), expression->semanticType, expression->source );
			}
			case ast::Node::Kind::TupleExpression: {
				ast::nodes::TupleExpression& tupleExpression = static_cast<ast::nodes::TupleExpression&>( *expression );
				std::vector<HIRNodeSharedPointer> elementExpressions;
				for( const ast::nodes::ExpressionSharedPointer& element : tupleExpression.elements ) {
					elementExpressions.push_back( this->lowerExpression( element ) );
				}
				return std::make_shared<HIRTupleLiteral>( std::move( elementExpressions ), expression->semanticType, expression->source );
			}
			case ast::Node::Kind::TypeReferenceExpression: {
				ast::nodes::TypeReferenceExpression& typeReference = static_cast<ast::nodes::TypeReferenceExpression&>( *expression );
				return std::make_shared<HIRTypeReference>( typeReference.typeName, expression->semanticType, expression->source );
			}
			case ast::Node::Kind::InstanceofExpression: {
				ast::nodes::InstanceofExpression& instanceofExpression = static_cast<ast::nodes::InstanceofExpression&>( *expression );
				HIRNodeSharedPointer checkedExpression = this->lowerExpression( instanceofExpression.object );
				semantic::TypeSharedPointer checkedType = this->resolveTypeNode( instanceofExpression.targetType );
				return std::make_shared<HIRInstanceOf>( std::move( checkedExpression ), checkedType, expression->source );
			}
			case ast::Node::Kind::SubclassofExpression: {
				ast::nodes::SubclassofExpression& subclassofExpression = static_cast<ast::nodes::SubclassofExpression&>( *expression );
				semantic::TypeSharedPointer childType = this->resolveTypeNode( subclassofExpression.sourceType );
				semantic::TypeSharedPointer parentType = this->resolveTypeNode( subclassofExpression.targetType );
				return std::make_shared<HIRSubclassOf>( childType, parentType, expression->source );
			}
			case ast::Node::Kind::RangeExpression: {
				ast::nodes::RangeExpression& rangeExpression = static_cast<ast::nodes::RangeExpression&>( *expression );
				HIRNodeSharedPointer rangeStart = this->lowerExpression( rangeExpression.start );
				HIRNodeSharedPointer rangeEnd = this->lowerExpression( rangeExpression.end );
				return std::make_shared<HIRRangeExpression>(
					std::move( rangeStart ),
					std::move( rangeEnd ),
					rangeExpression.inclusive,
					expression->semanticType,
					expression->source
				);
			}
			case ast::Node::Kind::ComprehensionExpression: {
				ast::nodes::ComprehensionExpression& comprehensionExpression = static_cast<ast::nodes::ComprehensionExpression&>( *expression );
				std::shared_ptr<HIRComprehension> hirComprehension = std::make_shared<HIRComprehension>( expression->semanticType, expression->source );
				hirComprehension->bodyExpression = this->lowerExpression( comprehensionExpression.bodyExpression );
				hirComprehension->conditionExpression = this->lowerExpression( comprehensionExpression.condition );
				hirComprehension->iterableExpression = this->lowerExpression( comprehensionExpression.iterable );
				hirComprehension->iteratorVariableName = comprehensionExpression.variable;
				hirComprehension->iteratorVariableType = this->resolveTypeNode( comprehensionExpression.variableType );
				return hirComprehension;
			}
			default:
				return nullptr;
		}
	}
	
	std::shared_ptr<HIRIf> HIRLowering::desugarIfStatement( ast::nodes::IfStatement& ifStatement ) {
		HIRNodeSharedPointer branchCondition = this->lowerExpression( ifStatement.condition );
		std::shared_ptr<HIRBlock> thenBranch = this->lowerStatementBlock( ifStatement.thenBody, ifStatement.source );
		HIRNodeSharedPointer elseBranch = nullptr;
		if( ifStatement.elifBranches.empty() == false ) {
			HIRNodeSharedPointer currentElse = nullptr;
			if( ifStatement.elseBody.empty() == false ) {
				currentElse = this->lowerStatementBlock( ifStatement.elseBody, ifStatement.source );
			}
			for( int elifIndex = static_cast<int>( ifStatement.elifBranches.size() ) - 1; elifIndex >= 0; elifIndex-- ) {
				const std::pair<ast::nodes::ExpressionSharedPointer, std::vector<ast::nodes::StatementSharedPointer>>& elifBranch = ifStatement.elifBranches[elifIndex];
				HIRNodeSharedPointer elifCondition = this->lowerExpression( elifBranch.first );
				std::shared_ptr<HIRBlock> elifBody = this->lowerStatementBlock( elifBranch.second, ifStatement.source );
				currentElse = std::make_shared<HIRIf>(
					std::move( elifCondition ),
					std::move( elifBody ),
					std::move( currentElse ),
					ifStatement.source
				);
			}
			elseBranch = std::move( currentElse );
		}
		else if( ifStatement.elseBody.empty() == false ) {
			elseBranch = this->lowerStatementBlock( ifStatement.elseBody, ifStatement.source );
		}
		return std::make_shared<HIRIf>(
			std::move( branchCondition ),
			std::move( thenBranch ),
			std::move( elseBranch ),
			ifStatement.source
		);
	}
	
	std::shared_ptr<HIRLoop> HIRLowering::desugarForStatement( ast::nodes::ForStatement& forStatement ) {
		std::shared_ptr<HIRLoop> hirLoop = std::make_shared<HIRLoop>( forStatement.source );
		hirLoop->loopVariableName = forStatement.variable;
		hirLoop->loopVariableName2 = forStatement.variable2;
		hirLoop->loopVariableType = this->resolveTypeNode( forStatement.variableType );
		hirLoop->loopVariableType2 = this->resolveTypeNode( forStatement.variableType2 );
		if( forStatement.isCStyle ) {
			hirLoop->loopInitializer = this->lowerExpression( forStatement.initializer );
			hirLoop->loopCondition = this->lowerExpression( forStatement.condition );
			if( forStatement.update != nullptr ) {
				hirLoop->loopUpdate = this->lowerStatement( forStatement.update );
			}
		}
		else {
			hirLoop->isIteratorLoop = true;
			hirLoop->iterableExpression = this->lowerExpression( forStatement.iterable );
		}
		hirLoop->loopBody = this->lowerStatementBlock( forStatement.body, forStatement.source );
		return hirLoop;
	}
	
	std::shared_ptr<HIRLoop> HIRLowering::desugarWhileStatement( ast::nodes::WhileStatement& whileStatement ) {
		std::shared_ptr<HIRLoop> hirLoop = std::make_shared<HIRLoop>( whileStatement.source );
		hirLoop->loopCondition = this->lowerExpression( whileStatement.condition );
		hirLoop->loopBody = this->lowerStatementBlock( whileStatement.body, whileStatement.source );
		return hirLoop;
	}
	
	HIRParameterDescriptor HIRLowering::lowerParameter( ast::nodes::FunctionParameterNode& parameter ) {
		HIRParameterDescriptor descriptor;
		descriptor.parameterName = parameter.name;
		descriptor.parameterType = this->resolveTypeNode( parameter.type );
		if( parameter.defaultValue != nullptr ) {
			descriptor.defaultValueExpression = this->lowerExpression( parameter.defaultValue );
		}
		descriptor.isSelfParameter = parameter.isSelf;
		descriptor.isVariadicParameter = parameter.isVariadic;
		descriptor.isMutableParameter = parameter.isMutable;
		descriptor.isReferenceParameter = parameter.isReference;
		descriptor.isKeywordParameter = parameter.isKeyword;
		descriptor.isPropertyParameter = parameter.isProperty;
		descriptor.propertyAccess = parameter.propertyAccess;
		descriptor.isReadonlyProperty = parameter.isReadonly;
		return descriptor;
	}
	
	HIRFieldDescriptor HIRLowering::lowerFieldDeclaration( ast::nodes::FieldDeclarationNode& field, int fieldIndex ) {
		HIRFieldDescriptor descriptor;
		descriptor.fieldName = field.name;
		descriptor.fieldType = this->resolveTypeNode( field.type );
		descriptor.fieldIndex = fieldIndex;
		descriptor.fieldAccess = field.access;
		descriptor.isStaticField = field.isStatic;
		descriptor.isFinalField = field.isFinal;
		descriptor.isReadonlyField = field.isReadonly;
		if( field.defaultValue != nullptr ) {
			descriptor.defaultValueExpression = this->lowerExpression( field.defaultValue );
		}
		return descriptor;
	}
	
	HIRGenericParameterDescriptor HIRLowering::lowerGenericParameter( ast::nodes::GenericParameterNode& genericParameter ) {
		HIRGenericParameterDescriptor descriptor;
		descriptor.parameterName = genericParameter.name;
		descriptor.defaultType = this->resolveTypeNode( genericParameter.defaultType );
		for( const ast::nodes::TypeNodeSharedPointer& constraint : genericParameter.constraints ) {
			semantic::TypeSharedPointer resolvedConstraint = this->resolveTypeNode( constraint );
			if( resolvedConstraint != nullptr ) {
				descriptor.constraintTypes.push_back( resolvedConstraint );
			}
		}
		return descriptor;
	}
	
} // namespace uranite::ir::hir
