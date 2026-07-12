
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

#include <fmt/format.h>
#include <set>

#include "uranite/codegen/codegen.hpp"
#include "uranite/codegen/default-runtime.hpp"
#include "uranite/codegen/runtime.hpp"
#include "uranite/codegen/intrinsic.hpp"
#include "uranite/semantic/symbol.hpp"
#include "uranite/semantic/typeref.hpp"

namespace uranite::codegen {
	
	static llvm::Type* getPointeeType( llvm::Value* value ) {
		if( llvm::AllocaInst* alloca = llvm::dyn_cast<llvm::AllocaInst>( value ) )
			return alloca->getAllocatedType();
		if( llvm::GetElementPtrInst* gep = llvm::dyn_cast<llvm::GetElementPtrInst>( value ) )
			return gep->getResultElementType();
		if( llvm::GlobalVariable* global = llvm::dyn_cast<llvm::GlobalVariable>( value ) )
			return global->getValueType();
		if( llvm::LoadInst* load = llvm::dyn_cast<llvm::LoadInst>( value ) ) {
			llvm::Value* source = load->getPointerOperand();
			if( llvm::AllocaInst* srcAlloca = llvm::dyn_cast<llvm::AllocaInst>( source ) ) {
				llvm::Type* allocatedType = srcAlloca->getAllocatedType();
				if( allocatedType->isPointerTy() == false )
					return allocatedType;
			}
			if( llvm::GetElementPtrInst* srcGep = llvm::dyn_cast<llvm::GetElementPtrInst>( source ) ) {
				llvm::Type* resultType = srcGep->getResultElementType();
				if( resultType->isPointerTy() == false )
					return resultType;
			}
		}
		return llvm::PointerType::getUnqual( value->getContext() );
	}
	
	static llvm::Type* unwrapStructPointer( llvm::Type* type, const ast::nodes::TypeNodeSharedPointer& typeNode, std::unordered_map<std::string, llvm::StructType*>& structTypes ) {
		if( type->isPointerTy() == false ) return type;
		std::string name;
		if( typeNode->kind == ast::Node::Kind::SimpleType ) {
			name = static_cast<ast::nodes::SimpleTypeNode&>( *typeNode ).name;
		}
		else if( typeNode->kind == ast::Node::Kind::GenericType ) {
			name = static_cast<ast::nodes::GenericTypeNode&>( *typeNode ).name;
		}
		if( name.empty() == false ) {
			std::unordered_map<std::string, llvm::StructType*>::iterator it = structTypes.find( name );
			if( it != structTypes.end() ) return it->second;
		}
		return type;
	}
	
	LLVMCodegen::LLVMCodegen(
		semantic::Analyzer& analizer,
		diagnostic::Engine& diagnostic
	) : analyzer( analizer ),
		module( std::make_unique<llvm::Module>( "uranite.module", context ) ),
		builder( context ),
		diagnostic( diagnostic ),
		runtimeInterface_( std::make_shared<DefaultRuntime>() ) {
		this->module->setTargetTriple( llvm::sys::getDefaultTargetTriple() );
		this->registerBuiltInStructTypes();
	}
	
	void LLVMCodegen::collectFreeVariables( const ast::nodes::ExpressionSharedPointer& expression, const std::unordered_set<std::string>& bound, std::vector<std::string>& freeVariables ) {
		if( expression == nullptr ) {
			return;
		}
		switch( expression->kind ) {
			case ast::Node::Kind::AwaitExpression: {
				this->collectFreeVariables( static_cast<ast::nodes::AwaitExpression&>( *expression ).operand, bound, freeVariables );
				break;
			}
			case ast::Node::Kind::BinaryExpression: {
				ast::nodes::BinaryExpression& binaryExpression = static_cast<ast::nodes::BinaryExpression&>( *expression );
				this->collectFreeVariables( binaryExpression.left, bound, freeVariables );
				this->collectFreeVariables( binaryExpression.right, bound, freeVariables );
				break;
			}
			case ast::Node::Kind::CallExpression: {
				ast::nodes::CallExpression& callExpression = static_cast<ast::nodes::CallExpression&>( *expression );
				this->collectFreeVariables( callExpression.callee, bound, freeVariables );
				for( ast::nodes::ExpressionSharedPointer& callArgumentExpression : callExpression.arguments ) {
					this->collectFreeVariables( callArgumentExpression, bound, freeVariables );
				}
				for( ast::nodes::KeywordArgument& kwarg : callExpression.keywordArguments ) {
					this->collectFreeVariables( kwarg.value, bound, freeVariables );
				}
				break;
			}
			case ast::Node::Kind::IdentifierExpression: {
				std::string& identifierName = static_cast<ast::nodes::IdentifierExpression&>( *expression ).name;
				if( bound.find( identifierName ) == bound.end() && this->namedValues.count( identifierName ) && std::find( freeVariables.begin(), freeVariables.end(), identifierName ) == freeVariables.end() ) {
					freeVariables.push_back( identifierName );
				}
				break;
			}
			case ast::Node::Kind::IndexExpression: {
				ast::nodes::IndexExpression& indexExpression = static_cast<ast::nodes::IndexExpression&>( *expression );
				this->collectFreeVariables( indexExpression.object, bound, freeVariables );
				this->collectFreeVariables( indexExpression.index, bound, freeVariables );
				break;
			}
			case ast::Node::Kind::MemberAccessExpression: {
				this->collectFreeVariables( static_cast<ast::nodes::MemberAccessExpression&>( *expression ).object, bound, freeVariables );
				break;
			}
			case ast::Node::Kind::MethodCallExpression: {
				ast::nodes::MethodCallExpression& methodCallExpression = static_cast<ast::nodes::MethodCallExpression&>( *expression );
				this->collectFreeVariables( methodCallExpression.object, bound, freeVariables );
				for( ast::nodes::ExpressionSharedPointer& methodCallArgumentExpression : methodCallExpression.arguments ) {
					this->collectFreeVariables( methodCallArgumentExpression, bound, freeVariables );
				}
				for( ast::nodes::KeywordArgument& kwarg : methodCallExpression.keywordArguments ) {
					this->collectFreeVariables( kwarg.value, bound, freeVariables );
				}
				break;
			}
			case ast::Node::Kind::UnaryExpression: {
				this->collectFreeVariables( static_cast<ast::nodes::UnaryExpression&>( *expression ).operand, bound, freeVariables );
				break;
			}
			default:
				break;
		}
	}
	
	void LLVMCodegen::collectFreeVariablesInStatement( const ast::nodes::StatementSharedPointer& statement, std::unordered_set<std::string>& bound, std::vector<std::string>& freeVariables ) {
		if( statement == nullptr ) {
			return;
		}
		switch( statement->kind ) {
			case ast::Node::Kind::AssignmentStatement: {
				ast::nodes::AssignStatement& assignStatement = static_cast<ast::nodes::AssignStatement&>( *statement );
				this->collectFreeVariables( assignStatement.target, bound, freeVariables );
				this->collectFreeVariables( assignStatement.value, bound, freeVariables );
				break;
			}
			case ast::Node::Kind::ExpressionStatement: {
				this->collectFreeVariables( static_cast<ast::nodes::ExpressionStatement&>( *statement ).expression, bound, freeVariables );
				break;
			}
			case ast::Node::Kind::ForStatement: {
				ast::nodes::ForStatement& forStatement = static_cast<ast::nodes::ForStatement&>( *statement );
				this->collectFreeVariables( forStatement.iterable, bound, freeVariables );
				this->collectFreeVariables( forStatement.initializer, bound, freeVariables );
				this->collectFreeVariables( forStatement.condition, bound, freeVariables );
				bound.insert( forStatement.variable );
				for( ast::nodes::StatementSharedPointer& forStatementBody : forStatement.body ) {
					this->collectFreeVariablesInStatement( forStatementBody, bound, freeVariables );
				}
				break;
			}
			case ast::Node::Kind::IfStatement: {
				ast::nodes::IfStatement& ifStatement = static_cast<ast::nodes::IfStatement&>( *statement );
				this->collectFreeVariables( ifStatement.condition, bound, freeVariables );
				for( ast::nodes::StatementSharedPointer& thenStatementBody : ifStatement.thenBody ) {
					this->collectFreeVariablesInStatement( thenStatementBody, bound, freeVariables );
				}
				for( ast::nodes::StatementSharedPointer& elseStatementBody : ifStatement.elseBody ) {
					this->collectFreeVariablesInStatement( elseStatementBody, bound, freeVariables );
				}
				break;
			}
			case ast::Node::Kind::ReturnStatement: {
				this->collectFreeVariables( static_cast<ast::nodes::ReturnStatement&>( *statement ).value, bound, freeVariables );
				break;
			}
			case ast::Node::Kind::VariableStatement: {
				ast::nodes::VariableStatement& variableStatement = static_cast<ast::nodes::VariableStatement&>( *statement );
				this->collectFreeVariables( variableStatement.initializer, bound, freeVariables );
				bound.insert( variableStatement.name );
				break;
			}
			case ast::Node::Kind::WhileStatement: {
				ast::nodes::WhileStatement& whileStatement = static_cast<ast::nodes::WhileStatement&>( *statement );
				this->collectFreeVariables( whileStatement.condition, bound, freeVariables );
				for( ast::nodes::StatementSharedPointer& whileStatementBody : whileStatement.body ) {
					this->collectFreeVariablesInStatement( whileStatementBody, bound, freeVariables );
				}
				break;
			}
			default:
				break;
		}
	}
	
	llvm::AllocaInst* LLVMCodegen::createEntryBlockAllocation( llvm::Function* function, const std::string& name, llvm::Type* type ) {
		llvm::IRBuilder<> tmpBuilder( &function->getEntryBlock(), function->getEntryBlock().begin() );
		return tmpBuilder.CreateAlloca( type, nullptr, name );
	}
	
	llvm::Value* LLVMCodegen::createInterfaceFatPointer( llvm::Value* objectPointer, const std::string& className, const std::string& interfaceName ) {
		llvm::StructType* fatPointerType = this->getInterfaceFatPointerType();
		llvm::AllocaInst* fatPointer = this->builder.CreateAlloca( fatPointerType, nullptr, "ifat.tmp" );
		
		// Store object pointer as i8*
		llvm::Value* objectI8 = this->builder.CreateBitCast( objectPointer, llvm::PointerType::getUnqual( this->context ), "obj.cast" );
		llvm::Value* objectSlot = this->builder.CreateStructGEP( fatPointerType, fatPointer, 0, "ifat.obj" );
		
		this->builder.CreateStore(
			objectI8,
			objectSlot
		);
		
		// Store itable pointer as i8*
		std::string interfaceTableKey( fmt::format( "{}::{}", className, interfaceName ) );
		std::unordered_map<std::string, llvm::GlobalVariable*>::iterator interfaceTableIt = this->interfaceTables.find( interfaceTableKey );
		if( interfaceTableIt == this->interfaceTables.end() ) {
			std::string baseClassName( className );
			size_t classGenericPos = baseClassName.find( '<' );
			if( classGenericPos != std::string::npos ) {
				baseClassName = baseClassName.substr( 0, classGenericPos );
			}
			std::string baseInterfaceName( interfaceName );
			size_t ifaceGenericPos = baseInterfaceName.find( '<' );
			if( ifaceGenericPos != std::string::npos ) {
				baseInterfaceName = baseInterfaceName.substr( 0, ifaceGenericPos );
			}
			std::string baseExactKey( fmt::format( "{}::{}", baseClassName, interfaceName ) );
			interfaceTableIt = this->interfaceTables.find( baseExactKey );
			if( interfaceTableIt == this->interfaceTables.end() ) {
				std::string basePrefixKey( fmt::format( "{}::{}<", baseClassName, baseInterfaceName ) );
				for( std::unordered_map<std::string, llvm::GlobalVariable*>::iterator it = this->interfaceTables.begin(); it != this->interfaceTables.end(); ++it ) {
					if( it->first.find( basePrefixKey ) == 0 ) {
						interfaceTableIt = it;
						break;
					}
				}
			}
			if( interfaceTableIt == this->interfaceTables.end() ) {
				std::string classExactKey( fmt::format( "{}::{}", className, baseInterfaceName ) );
				interfaceTableIt = this->interfaceTables.find( classExactKey );
			}
		}
		llvm::Value* interfaceTableI8 = llvm::ConstantPointerNull::get( llvm::PointerType::getUnqual( this->context ) );
		if( interfaceTableIt != this->interfaceTables.end() ) {
			interfaceTableI8 = this->builder.CreateBitCast( interfaceTableIt->second, llvm::PointerType::getUnqual( this->context ), "itable.cast" );
		}
		this->builder.CreateStore( interfaceTableI8, this->builder.CreateStructGEP( fatPointerType, fatPointer, 1, "ifat.itable" ) );
		return this->builder.CreateLoad( fatPointerType, fatPointer, "ifat.val" );
	}
	
	void LLVMCodegen::dump() {
		this->module->print( llvm::outs(), nullptr );
	}
	
	bool LLVMCodegen::generate( ast::nodes::Program& program ) {
		
		if( program.module != nullptr ) {
			this->module->setModuleIdentifier( program.module->name );
			this->module->setSourceFileName( program.module->source->filename );
		}
		else {
			this->module->setModuleIdentifier( "main" );
			if( program.source != nullptr ) {
				this->module->setSourceFileName( program.source->filename );
			}
		}
		
		// Pre-scan to identify overloaded function names (de-duplicate by source location)
		this->overloadedFunctionCounts.clear();
		std::unordered_map<std::string, int>& functionNameCounts = this->overloadedFunctionCounts;
		std::unordered_map<std::string, std::vector<std::string>> functionSourceLocations;
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration->isBuiltin ) continue;
			if( declaration->kind == ast::Node::Kind::FunctionDeclaration ) {
				ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *declaration );
				std::string sourceLocationKey;
				if( functionDeclaration.source != nullptr && functionDeclaration.source->location != nullptr ) {
					sourceLocationKey = fmt::format( "{}:{}", functionDeclaration.source->pathname, functionDeclaration.source->location->line );
				}
				std::vector<std::string>& existingLocations = functionSourceLocations[functionDeclaration.name];
				bool isDuplicateSource = false;
				for( const std::string& existingLocation : existingLocations ) {
					if( sourceLocationKey.empty() == false && existingLocation == sourceLocationKey ) {
						isDuplicateSource = true;
						break;
					}
				}
				if( isDuplicateSource == false ) {
					existingLocations.push_back( sourceLocationKey );
					functionNameCounts[functionDeclaration.name]++;
				}
			}
		}
		
		// Forward declare all functions
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration->isBuiltin ) continue;
			if( declaration->kind == ast::Node::Kind::FunctionDeclaration ) {
				ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *declaration );
				llvm::Type* functionReturnType = this->resolveAstType( functionDeclaration.returnType );
				if( functionDeclaration.isAsync ) {
					this->programUsesAsync = true;
					functionReturnType = llvm::Type::getInt64Ty( this->context );
				}
				std::vector<llvm::Type*> functionParameterTypes;
				FunctionParamInfo fwdParamInfo;
				int fwdNonSelfIdx = 0;
				bool fwdSeenVariadic = false;
				for( ast::nodes::FunctionParameterSharedPointer& functionParameter : functionDeclaration.parameters ) {
					if( functionParameter->isSelf ) {
						continue;
					}
					if( functionParameter->isVariadic ) {
						fwdSeenVariadic = true;
						llvm::Type* elemType = llvm::Type::getInt64Ty( this->context );
						if( functionParameter->type && functionParameter->type->kind == ast::Node::Kind::ArrayType ) {
							ast::nodes::ArrayTypeNode& arrayNode = static_cast<ast::nodes::ArrayTypeNode&>( *functionParameter->type );
							if( arrayNode.elementType ) {
								elemType = this->resolveAstType( arrayNode.elementType );
							}
						}
						else if( functionParameter->type ) {
							elemType = this->resolveAstType( functionParameter->type );
						}
						llvm::StructType* argsStruct = llvm::StructType::get( this->context, {
							llvm::PointerType::getUnqual( elemType ),
							llvm::Type::getInt64Ty( this->context ),
							llvm::Type::getInt64Ty( this->context )
						});
						functionParameterTypes.push_back( llvm::PointerType::getUnqual( argsStruct ) );
						fwdParamInfo.variadicIndex = fwdNonSelfIdx;
						fwdParamInfo.variadicElementLLVMType = elemType;
					}
					else if( functionParameter->isKeyword ) {
						llvm::Type* valType = llvm::Type::getInt64Ty( this->context );
						if( functionParameter->type ) {
							valType = this->resolveAstType( functionParameter->type );
						}
						llvm::StructType* kwargsStruct = llvm::StructType::get( this->context, {
							llvm::PointerType::getUnqual( llvm::PointerType::getUnqual( this->context ) ),
							llvm::PointerType::getUnqual( valType ),
							llvm::Type::getInt64Ty( this->context ),
							llvm::Type::getInt64Ty( this->context )
						});
						functionParameterTypes.push_back( llvm::PointerType::getUnqual( kwargsStruct ) );
						fwdParamInfo.keywordIndex = fwdNonSelfIdx;
						fwdParamInfo.keywordValueLLVMType = valType;
					}
					else if( functionParameter->type ) {
						llvm::Type* functionParameterLLVMType = this->resolveAstType( functionParameter->type );
						if( functionParameterLLVMType->isStructTy() &&
							functionParameterLLVMType != this->getInterfaceFatPointerType() ) {
							functionParameterLLVMType = llvm::PointerType::getUnqual( functionParameterLLVMType );
						}
						functionParameterTypes.push_back( functionParameterLLVMType );
						if( fwdSeenVariadic && functionParameter->defaultValue ) {
							KeywordOnlyParam kwOnly;
							kwOnly.name = functionParameter->name;
							kwOnly.llvmType = functionParameterLLVMType;
							kwOnly.defaultValue = functionParameter->defaultValue;
							fwdParamInfo.keywordOnlyParams.push_back( kwOnly );
						}
					}
					else {
						functionParameterTypes.push_back( llvm::Type::getInt64Ty( this->context ) );
					}
					fwdNonSelfIdx++;
				}
				if( functionDeclaration.isGenerator ) {
					llvm::Type* yieldType = functionReturnType;
					if( yieldType == nullptr || yieldType->isVoidTy() ) {
						yieldType = llvm::Type::getInt64Ty( this->context );
					}
					std::vector<llvm::Type*> functionGeneratorFields = { llvm::Type::getInt32Ty( this->context ), yieldType, llvm::Type::getInt1Ty( this->context ) };
					for( llvm::Type* functionParameterLLVMType : functionParameterTypes ) {
						functionGeneratorFields.push_back( functionParameterLLVMType );
					}
					functionReturnType = llvm::StructType::get( this->context, functionGeneratorFields );
				}
				if( functionDeclaration.name == "main" ) {
					functionReturnType = llvm::Type::getInt32Ty( this->context );
					functionParameterTypes.clear();
					functionParameterTypes.push_back( llvm::Type::getInt32Ty( this->context ) );
					functionParameterTypes.push_back( llvm::PointerType::getUnqual( llvm::PointerType::getUnqual( this->context ) ) );
				}
				std::string preRegistrationKey = functionDeclaration.name;
				bool isOverloadedName = functionNameCounts[functionDeclaration.name] > 1;
				if( isOverloadedName ) {
					std::string preRegParamSignature;
					for( ast::nodes::FunctionParameterSharedPointer& sigParam : functionDeclaration.parameters ) {
						if( sigParam->isSelf ) continue;
						if( preRegParamSignature.empty() == false ) {
							preRegParamSignature += ",";
						}
						if( sigParam->type ) {
							if( sigParam->type->kind == ast::Node::Kind::SimpleType ) {
								preRegParamSignature += static_cast<ast::nodes::SimpleTypeNode&>( *sigParam->type ).name;
							}
							else if( sigParam->type->kind == ast::Node::Kind::GenericType ) {
								ast::nodes::GenericTypeNode& genericNode = static_cast<ast::nodes::GenericTypeNode&>( *sigParam->type );
								preRegParamSignature += genericNode.name;
								if( genericNode.typeArguments.empty() == false ) {
									preRegParamSignature += "<";
									for( size_t gtIdx = 0; gtIdx < genericNode.typeArguments.size(); gtIdx++ ) {
										if( gtIdx > 0 ) preRegParamSignature += ",";
										if( genericNode.typeArguments[gtIdx]->kind == ast::Node::Kind::SimpleType ) {
											preRegParamSignature += static_cast<ast::nodes::SimpleTypeNode&>( *genericNode.typeArguments[gtIdx] ).name;
										}
									}
									preRegParamSignature += ">";
								}
							}
							else if( sigParam->type->kind == ast::Node::Kind::ArrayType ) {
								preRegParamSignature += "Array";
							}
							else if( sigParam->type->kind == ast::Node::Kind::OptionalType ) {
								preRegParamSignature += "Optional";
							}
							else {
								preRegParamSignature += "unknown";
							}
						}
						else {
							preRegParamSignature += "I64";
						}
					}
					preRegistrationKey = fmt::format( "{}#{}", functionDeclaration.name, preRegParamSignature );
				}
				if( this->functions.count( preRegistrationKey ) ) {
					continue;
				}
				std::string functionMangleName( functionDeclaration.name == "main" ? "main" : this->mangleName( preRegistrationKey ) );
				llvm::FunctionType* functionType = llvm::FunctionType::get( functionReturnType, functionParameterTypes, false );
				llvm::Function* function = llvm::Function::Create( functionType, llvm::Function::ExternalLinkage, functionMangleName, this->getModule() );
				this->functions[preRegistrationKey] = function;
				this->functionParamInfos[preRegistrationKey] = fwdParamInfo;
			}
		}
		
		// Generate extern declarations (before structs, so types are available)
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration->isBuiltin ) continue;
			if( declaration->kind == ast::Node::Kind::ExternDeclaration ) {
				this->generateExternDeclaration( static_cast<ast::nodes::ExternDeclaration&>( *declaration ) );
			}
		}
		
		// Generate module-level constant declarations
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration->kind == ast::Node::Kind::ConstantDeclaration ) {
				this->generateDeclaration( declaration );
			}
		}
		
		// Phase 1: Pre-register all struct/class types and method signatures
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration->kind == ast::Node::Kind::ClassDeclaration ) {
				ast::nodes::ClassDeclaration& classDeclaration = static_cast<ast::nodes::ClassDeclaration&>( *declaration );
				if( classDeclaration.isNative ) continue;
				static const std::set<std::string> preOopWrapperClasses = {
					"Int", "I8", "I16", "I32", "I64", "UInt", "U8", "U16", "U32", "U64",
					"Float", "F32", "F64", "Double", "Long", "Integer", "Boolean", "Byte",
					"Char", "String", "Void", "Object"
				};
				if( preOopWrapperClasses.count( classDeclaration.name ) ) continue;
				semantic::ClassTypeSharedPointer preClassType = std::dynamic_pointer_cast<semantic::ClassType>( this->analyzer.types().lookupType( classDeclaration.name ) );
				if( preClassType ) {
					this->getOrCreateStructType( classDeclaration.name, preClassType );
					this->preRegisterClassMethods( classDeclaration, preClassType );
					this->preRegisteredClasses.insert( classDeclaration.name );
				}
			}
			else if( declaration->kind == ast::Node::Kind::StructDeclaration ) {
				ast::nodes::StructDeclaration& structDeclaration = static_cast<ast::nodes::StructDeclaration&>( *declaration );
				semantic::StructTypeSharedPointer preStructType = std::dynamic_pointer_cast<semantic::StructType>( this->analyzer.types().lookupType( structDeclaration.name ) );
				if( preStructType ) {
					this->getOrCreateStructType( structDeclaration.name, preStructType );
				}
			}
			else if( declaration->kind == ast::Node::Kind::EnumDeclaration ) {
				this->generateEnumDeclaration( static_cast<ast::nodes::EnumDeclaration&>( *declaration ) );
			}
		}
		
		// Phase 2: Pre-generate itables for all classes
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration->kind == ast::Node::Kind::ClassDeclaration ) {
				ast::nodes::ClassDeclaration& classDeclaration = static_cast<ast::nodes::ClassDeclaration&>( *declaration );
				if( classDeclaration.isNative ) continue;
				semantic::ClassTypeSharedPointer preClassType = std::dynamic_pointer_cast<semantic::ClassType>( this->analyzer.types().lookupType( classDeclaration.name ) );
				if( preClassType ) {
					std::vector<semantic::InterfaceTypeSharedPointer> interfaceQueue;
					for( semantic::TypeSharedPointer& interface : preClassType->interfaces ) {
						if( interface && interface->kind == semantic::Type::Kind::Interface ) {
							interfaceQueue.push_back( std::static_pointer_cast<semantic::InterfaceType>( interface ) );
						}
					}
					for( size_t qi = 0; qi < interfaceQueue.size(); qi++ ) {
						semantic::InterfaceTypeSharedPointer interfaceType = interfaceQueue[qi];
						if( interfaceType->methodOrder.empty() == false ) {
							this->generateInterfaceTable( classDeclaration.name, preClassType, interfaceType );
						}
						for( semantic::TypeSharedPointer& superInterface : interfaceType->superInterfaces ) {
							if( superInterface && superInterface->kind == semantic::Type::Kind::Interface ) {
								semantic::InterfaceTypeSharedPointer superInterfaceType = std::static_pointer_cast<semantic::InterfaceType>( superInterface );
								std::string superKey = fmt::format( "{}::{}", classDeclaration.name, superInterfaceType->name );
								if( this->interfaceTables.count( superKey ) == 0 ) {
									interfaceQueue.push_back( superInterfaceType );
								}
							}
						}
					}
				}
			}
		}
		
		// Phase 3: Generate class/struct method bodies
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration->kind == ast::Node::Kind::ClassDeclaration ) {
				this->generateClassDeclaration( static_cast<ast::nodes::ClassDeclaration&>( *declaration ) );
			}
			else if( declaration->kind == ast::Node::Kind::StructDeclaration ) {
				this->generateStructDeclaration( static_cast<ast::nodes::StructDeclaration&>( *declaration ) );
			}
		}
		
		// Generate function bodies
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration->isBuiltin ) continue;
			if( declaration->kind == ast::Node::Kind::FunctionDeclaration ) {
				this->generateFunctionDeclaration( static_cast<ast::nodes::FunctionDeclaration&>( *declaration ) );
			}
		}
		
		// Debug: dump IR before verification
		{
			std::string debugFilename( "/tmp/uranite-debug.ll" );
			std::error_code debugErrorCode;
			llvm::raw_fd_ostream debugFile( debugFilename, debugErrorCode, llvm::sys::fs::OF_Text );
			if( debugErrorCode.value() == 0 ) {
				this->module->print( debugFile, nullptr );
			}
		}
		
		// Verify module
		std::string error;
		llvm::raw_string_ostream errorStream( error );
		if( llvm::verifyModule( *this->module, &errorStream ) ) {
			this->diagnostic.error( std::make_shared<lookup::Source>(), fmt::format( "LLVM module verification failed: {}", errorStream.str() ) );
			return false;
		}
		
		return true;
	}
	
	void LLVMCodegen::generateAssignmentStatement( ast::nodes::AssignStatement& statement ) {
		llvm::Value* expressionValue = this->generateExpression( statement.value );
		if( expressionValue == nullptr ) {
			return;
		}
		
		// Memory<T> index assignment: mem[i] = val → GEP + store
		if( statement.target->kind == ast::Node::Kind::IndexExpression ) {
			ast::nodes::IndexExpression& indexExpression = static_cast<ast::nodes::IndexExpression&>( *statement.target );
			if( indexExpression.object->kind == ast::Node::Kind::IdentifierExpression ) {
				ast::nodes::IdentifierExpression& identifierExpression = static_cast<ast::nodes::IdentifierExpression&>( *indexExpression.object );
				std::unordered_map<std::string,llvm::Type*>::iterator memoryElementIterator = this->varMemoryElementTypes.find( identifierExpression.name );
				if( memoryElementIterator != this->varMemoryElementTypes.end() ) {
					std::unordered_map<std::string,llvm::Value*>::iterator namedValueIterator = this->namedValues.find( identifierExpression.name );
					if( namedValueIterator != this->namedValues.end() ) {
						llvm::PointerType* pointerType = llvm::PointerType::getUnqual( memoryElementIterator->second );
						llvm::LoadInst* memoryPointer = this->builder.CreateLoad( pointerType, namedValueIterator->second, "mem.ptr" );
						llvm::Value* indexValue = this->generateExpression( indexExpression.index );
						if( indexValue == nullptr ) {
							return;
						}
						indexValue = this->generateImplicitCast( indexValue, llvm::Type::getInt64Ty( this->context ) );
						expressionValue = this->generateImplicitCast( expressionValue, memoryElementIterator->second );
						this->builder.CreateStore(
							expressionValue,
							this->builder.CreateGEP(
								memoryElementIterator->second,
								memoryPointer,
								indexValue,
								"mem.set.gep"
							)
						);
						return;
					}
				}
			}
			std::string className = this->resolveStructTypeName( indexExpression.object );
			if( className.empty() == false ) {
				std::string setMethodName = fmt::format( "{}::set", className );
				std::unordered_map<std::string, llvm::Function*>::iterator functionIterator = this->functions.find( setMethodName );
				if( functionIterator != this->functions.end() && functionIterator->second != nullptr ) {
					llvm::Value* selfPointer = this->resolveObjectPointer( indexExpression.object );
					llvm::Value* indexValue = this->generateExpression( indexExpression.index );
					if( selfPointer && indexValue ) {
						llvm::Function* setFunction = functionIterator->second;
						if( setFunction->arg_size() >= 2 ) {
							llvm::Type* expectedKeyType = ( setFunction->arg_begin() + 1 )->getType();
							indexValue = this->generateImplicitCast( indexValue, expectedKeyType );
						}
						if( setFunction->arg_size() >= 3 ) {
							llvm::Type* expectedValueType = ( setFunction->arg_begin() + 2 )->getType();
							expressionValue = this->generateImplicitCast( expressionValue, expectedValueType );
						}
						this->builder.CreateCall( setFunction, { selfPointer, indexValue, expressionValue } );
						return;
					}
				}
			}
		}
		
		// Static class field assignment: ClassName.staticField = value
		if( statement.target->kind == ast::Node::Kind::MemberAccessExpression ) {
			ast::nodes::MemberAccessExpression& targetMemberAccess = static_cast<ast::nodes::MemberAccessExpression&>( *statement.target );
			if( targetMemberAccess.object->kind == ast::Node::Kind::IdentifierExpression ) {
				ast::nodes::IdentifierExpression& targetIdentifier = static_cast<ast::nodes::IdentifierExpression&>( *targetMemberAccess.object );
				std::unordered_map<std::string, std::unordered_map<std::string, llvm::GlobalVariable*>>::iterator staticClassIterator = this->staticClassFields.find( targetIdentifier.name );
				if( staticClassIterator != this->staticClassFields.end() ) {
					std::unordered_map<std::string, llvm::GlobalVariable*>::iterator staticFieldIterator = staticClassIterator->second.find( targetMemberAccess.member );
					if( staticFieldIterator != staticClassIterator->second.end() ) {
						llvm::GlobalVariable* staticGlobal = staticFieldIterator->second;
						expressionValue = this->generateImplicitCast( expressionValue, staticGlobal->getValueType() );
						this->builder.CreateStore( expressionValue, staticGlobal );
						return;
					}
				}
			}
		}
		
		llvm::Value* targetPointer = nullptr;
		if( statement.target->kind == ast::Node::Kind::IdentifierExpression ) {
			ast::nodes::IdentifierExpression& targetIdentifier = static_cast<ast::nodes::IdentifierExpression&>( *statement.target );
			std::unordered_map<std::string,llvm::Value*>::iterator targetNamedIt = this->namedValues.find( targetIdentifier.name );
			if( targetNamedIt != this->namedValues.end() ) {
				targetPointer = targetNamedIt->second;
			}
			else {
				llvm::GlobalVariable* globalVar = this->module->getGlobalVariable( targetIdentifier.name, true );
				if( globalVar ) {
					targetPointer = globalVar;
				}
			}
		}
		else {
			targetPointer = this->resolveObjectPointer( statement.target );
		}
		if( targetPointer && targetPointer->getType()->isPointerTy() ) {
			llvm::Type* fieldType = getPointeeType( targetPointer );
			if( expressionValue->getType()->isPointerTy() && fieldType->isStructTy() &&
				getPointeeType( expressionValue ) == fieldType ) {
				llvm::Value* isNull = this->builder.CreateICmpEQ(
					expressionValue,
					llvm::ConstantPointerNull::get( llvm::cast<llvm::PointerType>( expressionValue->getType() ) ),
					"deref.isnull" );
				llvm::BasicBlock* derefNullBlock = llvm::BasicBlock::Create( this->context, "deref.null", this->currentFunction );
				llvm::BasicBlock* derefNonNullBlock = llvm::BasicBlock::Create( this->context, "deref.nonnull", this->currentFunction );
				llvm::BasicBlock* derefMergeBlock = llvm::BasicBlock::Create( this->context, "deref.merge", this->currentFunction );
				this->builder.CreateCondBr( isNull, derefNullBlock, derefNonNullBlock );
				this->builder.SetInsertPoint( derefNonNullBlock );
				llvm::Value* loadedValue = this->builder.CreateLoad( fieldType, expressionValue, "deref" );
				this->builder.CreateBr( derefMergeBlock );
				this->builder.SetInsertPoint( derefNullBlock );
				llvm::Value* zeroValue = llvm::Constant::getNullValue( fieldType );
				this->builder.CreateBr( derefMergeBlock );
				this->builder.SetInsertPoint( derefMergeBlock );
				llvm::PHINode* derefPhi = this->builder.CreatePHI( fieldType, 2, "deref.result" );
				derefPhi->addIncoming( loadedValue, derefNonNullBlock );
				derefPhi->addIncoming( zeroValue, derefNullBlock );
				expressionValue = derefPhi;
			}
			expressionValue = this->generateImplicitCast( expressionValue, fieldType );
			
			// Compound assignment
			if( statement.operation != token::Type::Assignment ) {
				llvm::LoadInst* currentValue = this->builder.CreateLoad( fieldType, targetPointer, "curval" );
				switch( statement.operation ) {
					case token::Type::MinusAssignment:
						if( fieldType->isFloatingPointTy() ) {
							expressionValue = this->builder.CreateFSub( currentValue, expressionValue, "subtmp" );
						}
						else {
							expressionValue = this->builder.CreateSub( currentValue, expressionValue, "subtmp" );
						}
						break;
					case token::Type::PlusAssignment:
						if( fieldType->isFloatingPointTy() ) {
							expressionValue = this->builder.CreateFAdd( currentValue, expressionValue, "addtmp" );
						}
						else {
							expressionValue = this->builder.CreateAdd( currentValue, expressionValue, "addtmp" );
						}
						break;
					case token::Type::SlashAssignment:
						if( fieldType->isFloatingPointTy() ) {
							llvm::Value* fdivZeroCheck = this->builder.CreateFCmpOEQ( expressionValue, llvm::ConstantFP::get( expressionValue->getType(), 0.0 ), "fdivzero.chk" );
							this->generateArithmeticErrorCheck( fdivZeroCheck, "ZeroDivisionError", "float division by zero", statement.source );
							expressionValue = this->builder.CreateFDiv( currentValue, expressionValue, "divtmp" );
						}
						else {
							llvm::Value* divZeroCheck = this->builder.CreateICmpEQ( expressionValue, llvm::ConstantInt::get( expressionValue->getType(), 0 ), "divzero.chk" );
							this->generateArithmeticErrorCheck( divZeroCheck, "ZeroDivisionError", "integer division by zero", statement.source );
							expressionValue = this->builder.CreateSDiv( currentValue, expressionValue, "divtmp" );
						}
						break;
					case token::Type::StarAssignment:
						if( fieldType->isFloatingPointTy() ) {
							expressionValue = this->builder.CreateFMul( currentValue, expressionValue, "multmp" );
						}
						else {
							expressionValue = this->builder.CreateMul( currentValue, expressionValue, "multmp" );
						}
						break;
					default:
						break;
				}
			}
			this->builder.CreateStore( expressionValue, targetPointer );
		}
	}
	
	llvm::Value* LLVMCodegen::generateBinaryExpression( ast::nodes::BinaryExpression& expression ) {
		
		// Handle value-type optional comparison with None before generating both sides
		bool isNoneComparison = expression.operation == token::Type::Equal || expression.operation == token::Type::NotEqual || expression.operation == token::Type::KeywordIs;
		if( isNoneComparison ) {
			bool leftIsNone = expression.left->kind == ast::Node::Kind::NoneLiteral;
			bool rightIsNone = expression.right->kind == ast::Node::Kind::NoneLiteral;
			if( leftIsNone || rightIsNone ) {
				ast::nodes::ExpressionSharedPointer& expressionValue = leftIsNone ? expression.right : expression.left;
				llvm::Value* expressionValueLLVM = this->generateExpression( expressionValue );
				if( expressionValueLLVM && expressionValueLLVM->getType()->isStructTy() ) {
					llvm::Value* expressionHasValueLLVM = this->builder.CreateExtractValue( expressionValueLLVM, 0, "hasval" );
					if( expression.operation == token::Type::Equal || expression.operation == token::Type::KeywordIs ) {
						return this->builder.CreateNot( expressionHasValueLLVM, "isnone" );
					}
					else {
						return expressionHasValueLLVM;
					}
				}
				if( expressionValueLLVM && expressionValueLLVM->getType()->isPointerTy() ) {
					llvm::Value* nullPointer = llvm::ConstantPointerNull::get( llvm::cast<llvm::PointerType>( expressionValueLLVM->getType() ) );
					if( expression.operation == token::Type::Equal || expression.operation == token::Type::KeywordIs ) {
						return this->builder.CreateICmpEQ( expressionValueLLVM, nullPointer, "isnone" );
					}
					else {
						return this->builder.CreateICmpNE( expressionValueLLVM, nullPointer, "isnotnone" );
					}
				}
			}
		}
		
		llvm::Value* expressionLeftLLVMValue = this->generateExpression( expression.left );
		llvm::Value* expressionRightLLVMValue = this->generateExpression( expression.right );
		if( expressionLeftLLVMValue == nullptr || expressionRightLLVMValue == nullptr ) {
			return nullptr;
		}
		
		// Containment operator: expr in items → items.contains(expr)
		if( expression.operation == token::Type::KeywordIn ) {
			std::string typeName = this->resolveStructTypeName( expression.right );
			if( typeName.empty() == false ) {
				std::string containsMethodName = fmt::format( "{}::contains", typeName );
				std::unordered_map<std::string, llvm::Function*>::iterator functionIterator = this->functions.find( containsMethodName );
				if( functionIterator != this->functions.end() && functionIterator->second != nullptr ) {
					llvm::Value* selfPointer = this->resolveObjectPointer( expression.right );
					if( selfPointer ) {
						llvm::Value* keyValue = expressionLeftLLVMValue;
						llvm::Function* containsFunction = functionIterator->second;
						if( containsFunction->arg_size() >= 2 ) {
							llvm::Type* expectedKeyType = ( containsFunction->arg_begin() + 1 )->getType();
							keyValue = this->generateImplicitCast( keyValue, expectedKeyType );
						}
						return this->builder.CreateCall( containsFunction, { selfPointer, keyValue }, "in.contains" );
					}
				}
			}
			return nullptr;
		}
		
		// Pointer-null comparison (for nullable types: ?T == None, ?T != None)
		if( ( expression.operation == token::Type::Equal || expression.operation == token::Type::NotEqual ) && expressionLeftLLVMValue->getType()->isPointerTy() && expressionRightLLVMValue->getType()->isPointerTy() ) {
			bool expressionLeftNull = llvm::isa<llvm::ConstantPointerNull>( expressionLeftLLVMValue );
			bool expressionRightNull = llvm::isa<llvm::ConstantPointerNull>( expressionRightLLVMValue );
			if( expressionLeftNull || expressionRightNull ) {
				
				// Cast null to match non-null pointer type
				if( expressionLeftNull && expressionLeftLLVMValue->getType() != expressionRightLLVMValue->getType() ) {
					expressionLeftLLVMValue = llvm::ConstantPointerNull::get( llvm::cast<llvm::PointerType>( expressionRightLLVMValue->getType() ) );
				}
				else if( expressionRightNull && expressionRightLLVMValue->getType() != expressionLeftLLVMValue->getType() ) {
					expressionRightLLVMValue = llvm::ConstantPointerNull::get(
						llvm::cast<llvm::PointerType>(
							expressionLeftLLVMValue->getType()
						)
					);
				}
				if( expression.operation == token::Type::Equal ) {
					return this->builder.CreateICmpEQ( expressionLeftLLVMValue, expressionRightLLVMValue, "eqtmp" );
				}
				else {
					return this->builder.CreateICmpNE( expressionLeftLLVMValue, expressionRightLLVMValue, "neqtmp" );
				}
			}
		}
		
		// Operator overloading: class/struct types dispatch to methods
		// Checked before string operations because LLVM 19 opaque pointers
		// make all pointer types indistinguishable at the LLVM level
		if( expressionLeftLLVMValue->getType()->isPointerTy() ) {
			std::string operatorTypeName( this->resolveStructTypeName( expression.left ) );
			if( operatorTypeName.empty() == false ) {
				size_t operatorGenericPos = operatorTypeName.find( '<' );
				if( operatorGenericPos != std::string::npos ) {
					operatorTypeName = operatorTypeName.substr( 0, operatorGenericPos );
				}
				std::unordered_map<std::string, llvm::StructType*>::iterator operatorStructIt = this->structTypes.find( operatorTypeName );
				if( operatorStructIt != this->structTypes.end() ) {
					static const std::unordered_map<int, std::string> operatorMethodMaps = {
						{ ( int ) token::Type::Plus,             "add" },
						{ ( int ) token::Type::Slash,            "divide" },
						{ ( int ) token::Type::Equal,            "equals" },
						{ ( int ) token::Type::GreaterThan,      "greaterThan" },
						{ ( int ) token::Type::LessThan,         "lessThan" },
						{ ( int ) token::Type::Percent,          "modulo" },
						{ ( int ) token::Type::Star,             "multiply" },
						{ ( int ) token::Type::NotEqual,         "notEquals" },
						{ ( int ) token::Type::Minus,            "subtract" }
					};
					std::unordered_map<int, std::string>::const_iterator operatorMethodIterator = operatorMethodMaps.find( ( int ) expression.operation );
					if( operatorMethodIterator != operatorMethodMaps.end() ) {
						std::string methodName( fmt::format( "{}::{}", operatorTypeName, operatorMethodIterator->second ) );
						std::unordered_map<std::string, llvm::Function*>::iterator functionIterator = this->functions.find( methodName );
						if( functionIterator != this->functions.end() ) {
							llvm::Value* selfPointer = this->resolveObjectPointer( expression.left );
							if( selfPointer ) {
								std::vector<llvm::Value*> args = { selfPointer, expressionRightLLVMValue };
								if( functionIterator->second->getReturnType()->isVoidTy() ) {
									this->builder.CreateCall( functionIterator->second, args );
									return nullptr;
								}
								return this->builder.CreateCall( functionIterator->second, args, fmt::format( "op.{}", operatorMethodIterator->second ) );
							}
						}
					}
				}
			}
		}

		// String operations (both operands are i8* pointers)
		bool isString =
			expressionLeftLLVMValue->getType()->isPointerTy() &&
			expressionRightLLVMValue->getType()->isPointerTy() &&
			expressionLeftLLVMValue->getType() == llvm::PointerType::getUnqual( this->context );
		
		if( isString ) {
			if( expression.operation == token::Type::Plus ) {
				
				// String concatenation: malloc(strlen(a) + strlen(b) + 1), strcpy, strcat
				llvm::Function* strlenfunction = this->getOrCreateStringLength();
				llvm::Function* strcpyfunction = this->getOrCreateStringCopy();
				llvm::Function* strcatfunction = this->getOrCreateStringConcatenate();
				llvm::Function* mallocfunction = this->getOrCreateMalloc();
				llvm::CallInst* lengthA = this->builder.CreateCall( strlenfunction, { expressionLeftLLVMValue }, "len.a" );
				llvm::CallInst* lengthB = this->builder.CreateCall( strlenfunction, { expressionRightLLVMValue }, "len.b" );
				llvm::Value* totalLength = this->builder.CreateAdd( lengthA, lengthB, "total.len" );
				llvm::Value* allocSize = this->builder.CreateAdd(
					totalLength,
					llvm::ConstantInt::get(
						llvm::Type::getInt64Ty( this->context ),
						1
					),
					"alloc.size"
				);
				llvm::CallInst* buffer = this->builder.CreateCall( mallocfunction, { allocSize }, "str.buf");
				this->builder.CreateCall( strcpyfunction, { buffer, expressionLeftLLVMValue } );
				this->builder.CreateCall( strcatfunction, { buffer, expressionRightLLVMValue } );
				
				return buffer;
			}
			if( expression.operation == token::Type::Equal ||
				expression.operation == token::Type::NotEqual ) {
				llvm::Function* strcmpfunction = this->getOrCreateStringCompare();
				llvm::CallInst* cmpResult = builder.CreateCall( strcmpfunction, { expressionLeftLLVMValue, expressionRightLLVMValue }, "strcmp.res" );
				llvm::ConstantInt* zero = llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->context ), 0 );
				if( expression.operation == token::Type::Equal ) {
					return this->builder.CreateICmpEQ( cmpResult, zero, "streq" );
				}
				else {
					return this->builder.CreateICmpNE( cmpResult, zero, "strne" );
				}
			}
		}
		
		// String + non-string: convert right operand to string, then concatenate
		if( expressionLeftLLVMValue->getType() == llvm::PointerType::getUnqual( this->context ) &&
			expressionRightLLVMValue->getType()->isPointerTy() == false &&
			expression.operation == token::Type::Plus ) {
			llvm::Function* snprintffunction = this->getOrCreateSnprintf();
			llvm::Function* mallocfunction = this->getOrCreateMalloc();
			llvm::Function* strlenfunction = this->getOrCreateStringLength();
			llvm::Function* strcpyfunction = this->getOrCreateStringCopy();
			llvm::Function* strcatfunction = this->getOrCreateStringConcatenate();
			llvm::IntegerType* i64Type = llvm::Type::getInt64Ty( this->context );
			llvm::ConstantInt* bufferSize = llvm::ConstantInt::get( i64Type, 32 );
			llvm::CallInst* convBuffer = this->builder.CreateCall( mallocfunction, { bufferSize }, "conv.buf" );
			if( expressionRightLLVMValue->getType()->isFloatingPointTy() ) {
				this->builder.CreateCall( snprintffunction, { convBuffer, bufferSize, this->builder.CreateGlobalStringPtr( "%.6f", "fmt.float" ), expressionRightLLVMValue } );
			}
			else {
				this->builder.CreateCall(
					snprintffunction,
					{
						convBuffer,
						bufferSize,
						this->builder.CreateGlobalStringPtr( "%ld", "fmt.int" ),
						this->generateImplicitCast( expressionRightLLVMValue, i64Type )
					}
				);
			}
			llvm::CallInst* lengthA = this->builder.CreateCall( strlenfunction, { expressionLeftLLVMValue }, "len.a" );
			llvm::CallInst* lengthB = this->builder.CreateCall( strlenfunction, { convBuffer }, "len.b" );
			llvm::Value* totalLength = this->builder.CreateAdd( lengthA, lengthB, "total.len" );
			llvm::Value* allocSize = this->builder.CreateAdd( totalLength, llvm::ConstantInt::get( i64Type, 1 ), "alloc.size" );
			llvm::CallInst* resultBuffer = this->builder.CreateCall( mallocfunction, { allocSize }, "str.buf" );
			
			this->builder.CreateCall( strcpyfunction, { resultBuffer, expressionLeftLLVMValue } );
			this->builder.CreateCall( strcatfunction, { resultBuffer, convBuffer } );
			
			return resultBuffer;
		}
		
		// Type alignment
		bool isFloat = expressionLeftLLVMValue->getType()->isFloatingPointTy() || expressionRightLLVMValue->getType()->isFloatingPointTy();
		
		if( isFloat ) {
			if( expressionLeftLLVMValue->getType()->isFloatingPointTy() == false ) {
				expressionLeftLLVMValue = this->builder.CreateSIToFP( expressionLeftLLVMValue, llvm::Type::getDoubleTy( this->context ), "tofp" );
			}
			if( expressionRightLLVMValue->getType()->isFloatingPointTy() == false ) {
				expressionRightLLVMValue = this->builder.CreateSIToFP( expressionRightLLVMValue, llvm::Type::getDoubleTy( this->context ), "tofp" );
			}
			if( expressionLeftLLVMValue->getType() != expressionRightLLVMValue->getType() ) {
				if( expressionLeftLLVMValue->getType()->isFloatTy() && expressionRightLLVMValue->getType()->isDoubleTy() ) {
					expressionLeftLLVMValue = this->builder.CreateFPExt( expressionLeftLLVMValue, llvm::Type::getDoubleTy( this->context ), "fpext" );
				}
				else if( expressionLeftLLVMValue->getType()->isDoubleTy() && expressionRightLLVMValue->getType()->isFloatTy() ) {
					expressionRightLLVMValue = this->builder.CreateFPExt( expressionRightLLVMValue, llvm::Type::getDoubleTy( this->context ), "fpext" );
				}
			}
		}
		else {
			
			// Ensure same integer width
			if( expressionLeftLLVMValue->getType() != expressionRightLLVMValue->getType() ) {
				if( expressionLeftLLVMValue->getType()->isIntegerTy() && expressionRightLLVMValue->getType()->isIntegerTy() ) {
					unsigned int leftBits = expressionLeftLLVMValue->getType()->getIntegerBitWidth();
					unsigned int rightBits = expressionRightLLVMValue->getType()->getIntegerBitWidth();
					if (leftBits < rightBits) {
						expressionLeftLLVMValue = this->builder.CreateSExt( expressionLeftLLVMValue, expressionRightLLVMValue->getType(), "sext" );
					}
					else {
						expressionRightLLVMValue = this->builder.CreateSExt( expressionRightLLVMValue, expressionLeftLLVMValue->getType(), "sext" );
					}
				}
			}
			
			// General type alignment: pointer/struct vs integer mismatches
			if( expressionLeftLLVMValue->getType() != expressionRightLLVMValue->getType() ) {
				if( expressionLeftLLVMValue->getType()->isIntegerTy() ) {
					expressionRightLLVMValue = this->generateImplicitCast( expressionRightLLVMValue, expressionLeftLLVMValue->getType() );
				}
				else if( expressionRightLLVMValue->getType()->isIntegerTy() ) {
					expressionLeftLLVMValue = this->generateImplicitCast( expressionLeftLLVMValue, expressionRightLLVMValue->getType() );
				}
				else if( expressionLeftLLVMValue->getType()->isPointerTy() && expressionRightLLVMValue->getType()->isPointerTy() ) {
					expressionRightLLVMValue = this->builder.CreateBitCast( expressionRightLLVMValue, expressionLeftLLVMValue->getType(), "ptr.align" );
				}
			}
		}
		
		// Coerce struct types (e.g. interface fat pointers) to scalars for binary ops
		if( expressionLeftLLVMValue->getType()->isStructTy() ) {
			expressionLeftLLVMValue = this->builder.CreateExtractValue( expressionLeftLLVMValue, 0, "left.scalar" );
		}
		if( expressionRightLLVMValue->getType()->isStructTy() ) {
			expressionRightLLVMValue = this->builder.CreateExtractValue( expressionRightLLVMValue, 0, "right.scalar" );
		}
		switch( expression.operation ) {
			case token::Type::Ampersand:
				return this->builder.CreateAnd( expressionLeftLLVMValue, expressionRightLLVMValue, "andtmp" );
			case token::Type::Caret:
				return this->builder.CreateXor( expressionLeftLLVMValue, expressionRightLLVMValue, "xortmp" );
			case token::Type::Equal:
				return isFloat ? this->builder.CreateFCmpOEQ( expressionLeftLLVMValue, expressionRightLLVMValue, "eqtmp" ) : this->builder.CreateICmpEQ( expressionLeftLLVMValue, expressionRightLLVMValue, "eqtmp" );
			case token::Type::GreaterThan:
				return isFloat ? this->builder.CreateFCmpOGT( expressionLeftLLVMValue, expressionRightLLVMValue, "gttmp" ) : this->builder.CreateICmpSGT( expressionLeftLLVMValue, expressionRightLLVMValue, "gttmp" );
			case token::Type::GreaterThanEqual:
				return isFloat ? this->builder.CreateFCmpOGE( expressionLeftLLVMValue, expressionRightLLVMValue, "getmp" ) : this->builder.CreateICmpSGE( expressionLeftLLVMValue, expressionRightLLVMValue, "getmp" );
			case token::Type::KeywordAnd:
				return this->builder.CreateAnd(
					this->builder.CreateICmpNE(
						expressionLeftLLVMValue,
						llvm::Constant::getNullValue(
							expressionLeftLLVMValue->getType()
						),
						"l"
					),
					this->builder.CreateICmpNE(
						expressionRightLLVMValue,
						llvm::Constant::getNullValue(
							expressionRightLLVMValue->getType()
						),
						"r"
					),
					"landtmp"
				);
			case token::Type::KeywordOr:
				return this->builder.CreateOr(
					this->builder.CreateICmpNE(
						expressionLeftLLVMValue,
						llvm::Constant::getNullValue(
							expressionLeftLLVMValue->getType()
						),
						"l"
					),
					this->builder.CreateICmpNE(
						expressionRightLLVMValue,
						llvm::Constant::getNullValue(
							expressionRightLLVMValue->getType()
						),
						"r"
					),
					"lortmp"
				);
			case token::Type::LessThan:
				return isFloat ? this->builder.CreateFCmpOLT( expressionLeftLLVMValue, expressionRightLLVMValue, "lttmp" ) : this->builder.CreateICmpSLT( expressionLeftLLVMValue, expressionRightLLVMValue, "lttmp" );
			case token::Type::LessThanEqual:
				return isFloat ? this->builder.CreateFCmpOLE( expressionLeftLLVMValue, expressionRightLLVMValue, "letmp" ) : this->builder.CreateICmpSLE( expressionLeftLLVMValue, expressionRightLLVMValue, "letmp" );
			case token::Type::Minus:
				return isFloat ? this->builder.CreateFSub( expressionLeftLLVMValue, expressionRightLLVMValue, "subtmp" ) : this->builder.CreateSub( expressionLeftLLVMValue, expressionRightLLVMValue, "subtmp" );
			case token::Type::NotEqual:
				return isFloat ? this->builder.CreateFCmpONE( expressionLeftLLVMValue, expressionRightLLVMValue, "neqtmp" ) : this->builder.CreateICmpNE( expressionLeftLLVMValue, expressionRightLLVMValue, "neqtmp" );
			case token::Type::Percent:
				if( isFloat ) {
					llvm::Value* fmodZeroCheck = this->builder.CreateFCmpOEQ( expressionRightLLVMValue, llvm::ConstantFP::get( expressionRightLLVMValue->getType(), 0.0 ), "fmodzero.chk" );
					this->generateArithmeticErrorCheck( fmodZeroCheck, "ZeroDivisionError", "float modulo by zero", expression.source );
					return this->builder.CreateFRem( expressionLeftLLVMValue, expressionRightLLVMValue, "modtmp" );
				}
				else {
					llvm::Value* modZeroCheck = this->builder.CreateICmpEQ( expressionRightLLVMValue, llvm::ConstantInt::get( expressionRightLLVMValue->getType(), 0 ), "modzero.chk" );
					this->generateArithmeticErrorCheck( modZeroCheck, "ZeroDivisionError", "integer modulo by zero", expression.source );
					return this->builder.CreateSRem( expressionLeftLLVMValue, expressionRightLLVMValue, "modtmp" );
				}
			case token::Type::Pipe:
				return this->builder.CreateOr( expressionLeftLLVMValue, expressionRightLLVMValue, "ortmp" );
			case token::Type::Plus:
				return isFloat ? this->builder.CreateFAdd( expressionLeftLLVMValue, expressionRightLLVMValue, "addtmp" ) : this->builder.CreateAdd( expressionLeftLLVMValue, expressionRightLLVMValue, "addtmp" );
			case token::Type::ShiftLeft:
				return this->builder.CreateShl( expressionLeftLLVMValue, expressionRightLLVMValue, "shltmp" );
			case token::Type::ShiftRight:
				return this->builder.CreateAShr( expressionLeftLLVMValue, expressionRightLLVMValue, "shrtmp" );
			case token::Type::Slash:
				if( isFloat ) {
					llvm::Value* fdivZeroCheck = this->builder.CreateFCmpOEQ( expressionRightLLVMValue, llvm::ConstantFP::get( expressionRightLLVMValue->getType(), 0.0 ), "fdivzero.chk" );
					this->generateArithmeticErrorCheck( fdivZeroCheck, "ZeroDivisionError", "float division by zero", expression.source );
					return this->builder.CreateFDiv( expressionLeftLLVMValue, expressionRightLLVMValue, "divtmp" );
				}
				else {
					llvm::Value* divZeroCheck = this->builder.CreateICmpEQ( expressionRightLLVMValue, llvm::ConstantInt::get( expressionRightLLVMValue->getType(), 0 ), "divzero.chk" );
					this->generateArithmeticErrorCheck( divZeroCheck, "ZeroDivisionError", "integer division by zero", expression.source );
					return this->builder.CreateSDiv( expressionLeftLLVMValue, expressionRightLLVMValue, "divtmp" );
				}
			case token::Type::Star:
				return isFloat ? this->builder.CreateFMul( expressionLeftLLVMValue, expressionRightLLVMValue, "multmp" ) : this->builder.CreateMul( expressionLeftLLVMValue, expressionRightLLVMValue, "multmp" );
			default:
				return nullptr;
		}
	}
	
	void LLVMCodegen::generateBlock( const std::vector<ast::nodes::StatementSharedPointer>& statements ) {
		for( const ast::nodes::StatementSharedPointer& statement : statements ) {
			if( this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
				this->generateStatement( statement );
				continue;
			}
			break;
		}
	}
	
	llvm::Value* LLVMCodegen::coerceValueToString( llvm::Value* arg ) {
		llvm::Type* i8PtrType = llvm::PointerType::getUnqual( this->context );
		llvm::Type* i64Type = llvm::Type::getInt64Ty( this->context );
		llvm::Module* mod = this->getModule();
		if( arg->getType() == i8PtrType ) {
			return arg;
		}
		if( arg->getType()->isIntegerTy( 1 ) ) {
			llvm::Constant* trueStr = this->builder.CreateGlobalStringPtr( "True", "bool.true" );
			llvm::Constant* falseStr = this->builder.CreateGlobalStringPtr( "False", "bool.false" );
			return this->builder.CreateSelect( arg, trueStr, falseStr, "bool.str" );
		}
		if( arg->getType()->isPointerTy() ) {
			llvm::Type* pointee = getPointeeType( arg );
			if( pointee->isStructTy() ) {
				llvm::StructType* structType = llvm::cast<llvm::StructType>( pointee );
				if( structType->hasName() ) {
					std::string className = structType->getName().str();
					std::string toStringName( fmt::format( "_UR_{}_toString", className ) );
					llvm::Function* toStringFn = mod->getFunction( toStringName );
					if( toStringFn && toStringFn->getReturnType() == i8PtrType ) {
						return this->builder.CreateCall( toStringFn, { arg }, "obj.str" );
					}
					std::string metaGlobalName( fmt::format( "_AE_meta_{}", className ) );
					llvm::GlobalVariable* metaGlobal = mod->getGlobalVariable( metaGlobalName, true );
					std::string reprPrefix;
					if( metaGlobal && metaGlobal->hasInitializer() ) {
						if( llvm::ConstantDataArray* cda = llvm::dyn_cast<llvm::ConstantDataArray>( metaGlobal->getInitializer() ) ) {
							reprPrefix = cda->getAsCString().str();
						}
					}
					if( reprPrefix.empty() ) {
						reprPrefix = className + " object";
					}
					std::string reprFmt( fmt::format( "<{} at 0x%lx>", reprPrefix ) );
					size_t reprBufSize = reprPrefix.size() + 32;
					llvm::FunctionCallee snprintfFn = mod->getOrInsertFunction( "snprintf", llvm::FunctionType::get( llvm::Type::getInt32Ty( this->context ), { i8PtrType, i64Type, i8PtrType }, true ) );
					llvm::FunctionCallee mallocFn = mod->getOrInsertFunction( "malloc", llvm::FunctionType::get( i8PtrType, { i64Type }, false ) );
					llvm::Value* buf = this->builder.CreateCall( mallocFn, { llvm::ConstantInt::get( i64Type, reprBufSize ) }, "repr.buf" );
					llvm::Value* ptrVal = this->builder.CreatePtrToInt( arg, i64Type, "ptr.int" );
					llvm::Constant* reprFmtStr = this->builder.CreateGlobalStringPtr( reprFmt, "repr.fmt" );
					this->builder.CreateCall( snprintfFn, { buf, llvm::ConstantInt::get( i64Type, reprBufSize ), reprFmtStr, ptrVal } );
					return buf;
				}
			}
			llvm::FunctionCallee snprintfFn = mod->getOrInsertFunction( "snprintf", llvm::FunctionType::get( llvm::Type::getInt32Ty( this->context ), { i8PtrType, i64Type, i8PtrType }, true ) );
			llvm::FunctionCallee mallocFn = mod->getOrInsertFunction( "malloc", llvm::FunctionType::get( i8PtrType, { i64Type }, false ) );
			llvm::Value* buf = this->builder.CreateCall( mallocFn, { llvm::ConstantInt::get( i64Type, 24 ) }, "ptr.buf" );
			llvm::Value* ptrVal = this->builder.CreatePtrToInt( arg, i64Type, "ptr.int" );
			llvm::Constant* ptrFmt = this->builder.CreateGlobalStringPtr( "0x%lx", "ptr.fmt" );
			this->builder.CreateCall( snprintfFn, { buf, llvm::ConstantInt::get( i64Type, 24 ), ptrFmt, ptrVal } );
			return buf;
		}
		if( arg->getType()->isDoubleTy() || arg->getType()->isFloatTy() ) {
			llvm::FunctionCallee snprintfFn = mod->getOrInsertFunction( "snprintf", llvm::FunctionType::get( llvm::Type::getInt32Ty( this->context ), { i8PtrType, i64Type, i8PtrType }, true ) );
			llvm::FunctionCallee mallocFn = mod->getOrInsertFunction( "malloc", llvm::FunctionType::get( i8PtrType, { i64Type }, false ) );
			llvm::Value* buf = this->builder.CreateCall( mallocFn, { llvm::ConstantInt::get( i64Type, 48 ) }, "flt.buf" );
			llvm::Value* val = arg;
			if( arg->getType()->isFloatTy() ) {
				val = this->builder.CreateFPExt( arg, llvm::Type::getDoubleTy( this->context ), "f2d" );
			}
			llvm::Constant* fltFmt = this->builder.CreateGlobalStringPtr( "%f", "flt.fmt" );
			this->builder.CreateCall( snprintfFn, { buf, llvm::ConstantInt::get( i64Type, 48 ), fltFmt, val } );
			return buf;
		}
		if( arg->getType()->isIntegerTy() ) {
			llvm::FunctionCallee snprintfFn = mod->getOrInsertFunction( "snprintf", llvm::FunctionType::get( llvm::Type::getInt32Ty( this->context ), { i8PtrType, i64Type, i8PtrType }, true ) );
			llvm::FunctionCallee mallocFn = mod->getOrInsertFunction( "malloc", llvm::FunctionType::get( i8PtrType, { i64Type }, false ) );
			llvm::Value* buf = this->builder.CreateCall( mallocFn, { llvm::ConstantInt::get( i64Type, 24 ) }, "int.buf" );
			llvm::Value* val = arg;
			if( arg->getType() != i64Type ) {
				val = this->builder.CreateSExt( arg, i64Type, "iext" );
			}
			llvm::Constant* intFmt = this->builder.CreateGlobalStringPtr( "%ld", "int.fmt" );
			this->builder.CreateCall( snprintfFn, { buf, llvm::ConstantInt::get( i64Type, 24 ), intFmt, val } );
			return buf;
		}
		return this->builder.CreateGlobalStringPtr( "<unknown>", "unk.str" );
	}
	
	llvm::Value* LLVMCodegen::generateBuiltinPuts( ast::nodes::CallExpression& expression ) {
		llvm::Type* i8PtrType = llvm::PointerType::getUnqual( this->context );
		llvm::Type* i64Type = llvm::Type::getInt64Ty( this->context );
		llvm::Type* i32Type = llvm::Type::getInt32Ty( this->context );
		llvm::FunctionCallee writeFn = this->module->getOrInsertFunction( "write",
			llvm::FunctionType::get( i64Type, { i32Type, i8PtrType, i64Type }, false ) );
		llvm::FunctionCallee strlenFn = this->module->getOrInsertFunction( "strlen",
			llvm::FunctionType::get( i64Type, { i8PtrType }, false ) );
		
		llvm::Value* fdOne = llvm::ConstantInt::get( i32Type, 1 );
		for( size_t i = 0; i < expression.arguments.size(); i++ ) {
			llvm::Value* arg = this->generateExpression( expression.arguments[i] );
			if( arg == nullptr ) {
				continue;
			}
			llvm::Value* strVal = this->coerceValueToString( arg );
			llvm::Value* len = this->builder.CreateCall( strlenFn, { strVal }, "slen" );
			this->builder.CreateCall( writeFn, { fdOne, strVal, len } );
		}
		llvm::Value* nlStr = this->builder.CreateGlobalStringPtr( "\n", "nl" );
		this->builder.CreateCall( writeFn, { fdOne, nlStr, llvm::ConstantInt::get( i64Type, 1 ) } );
		return nullptr;
	}
	
	llvm::Value* LLVMCodegen::generateCallExpression( ast::nodes::CallExpression& expression ) {
		std::string functionName;
		if( expression.callee->kind == ast::Node::Kind::IdentifierExpression ) {
			functionName = static_cast<ast::nodes::IdentifierExpression&>( *expression.callee ).name;
			if( functionName == "puts" ) {
				return this->generateBuiltinPuts( expression );
			}
		}
		else if( expression.callee->kind == ast::Node::Kind::SuperExpression ) {
			if( this->currentClassName.empty() ) {
				return nullptr;
			}
			semantic::TypeSharedPointer currentType = this->analyzer.types().lookupType( this->currentClassName );
			if( currentType == nullptr || currentType->kind != semantic::Type::Kind::Class ) {
				return nullptr;
			}
			semantic::ClassTypeSharedPointer classType = std::static_pointer_cast<semantic::ClassType>( currentType );
			if( classType->baseClass == nullptr || classType->baseClass->kind != semantic::Type::Kind::Class ) {
				return nullptr;
			}
			std::string parentName = classType->baseClass->name;
			this->ensureClassMethodsRegistered( parentName );
			std::string parentCtorKey = fmt::format( "{}::{}", parentName, parentName );
			std::unordered_map<std::string, llvm::Function*>::iterator parentCtorIterator = this->functions.find( parentCtorKey );
			if( parentCtorIterator == this->functions.end() ) {
				parentCtorIterator = this->functions.find( fmt::format( "{}#{}", parentCtorKey, expression.arguments.size() + 1 ) );
			}
			if( parentCtorIterator == this->functions.end() ) {
				return nullptr;
			}
			llvm::Function* parentCtor = parentCtorIterator->second;
			std::unordered_map<std::string, llvm::Value*>::iterator selfIterator = this->namedValues.find( "self" );
			if( selfIterator == this->namedValues.end() ) {
				return nullptr;
			}
			llvm::Value* selfPointer = this->builder.CreateLoad( getPointeeType( selfIterator->second ), selfIterator->second, "self.ptr" );
			llvm::Type* expectedSelfType = parentCtor->getArg( 0 )->getType();
			llvm::Value* parentSelfPointer = this->builder.CreateBitCast( selfPointer, expectedSelfType, "parent.self" );
			std::vector<llvm::Value*> parentArguments;
			parentArguments.push_back( parentSelfPointer );
			for( size_t i = 0; i < expression.arguments.size(); i++ ) {
				llvm::Value* argumentValue = this->generateExpression( expression.arguments[i] );
				if( argumentValue == nullptr ) {
					continue;
				}
				if( i + 1 < parentCtor->arg_size() ) {
					argumentValue = this->generateImplicitCast( argumentValue, parentCtor->getArg( i + 1 )->getType() );
				}
				parentArguments.push_back( argumentValue );
			}
			this->createCallOrInvoke( parentCtor, parentArguments );
			return nullptr;
		}
		else {
			llvm::Value* calleeValue = this->generateExpression( expression.callee );
			if( calleeValue == nullptr ) {
				return nullptr;
			}
			std::vector<llvm::Value*> arguments;
			std::vector<bool> argumentUnsigned;
			for( ast::nodes::ExpressionSharedPointer& argument : expression.arguments ) {
				llvm::Value* value = this->generateExpression( argument );
				if( value ) {
					arguments.push_back( value );
					std::string argTypeName = this->resolveStructTypeName( argument );
					bool isArgUnsigned = ( argTypeName == "U8" || argTypeName == "U16" || argTypeName == "U32" ||
						argTypeName == "U64" || argTypeName == "UInt" || argTypeName == "Byte" || argTypeName == "Char" );
					argumentUnsigned.push_back( isArgUnsigned );
				}
			}
			if( calleeValue->getType()->isPointerTy() ) {
				llvm::Type* pointerType = getPointeeType( calleeValue );
				if( pointerType->isFunctionTy() ) {
					llvm::FunctionType* functionType = llvm::cast<llvm::FunctionType>( pointerType );
					for( size_t i=0; i<arguments.size() && i < functionType->getNumParams(); i++ ) {
						bool isUnsigned = ( i < argumentUnsigned.size() ) ? argumentUnsigned[i] : false;
						arguments[i] = this->generateImplicitCast( arguments[i], functionType->getParamType( i ), isUnsigned );
					}
					if( functionType->getReturnType()->isVoidTy() ) {
						this->builder.CreateCall( functionType, calleeValue, arguments );
						return nullptr;
					}
					return this->builder.CreateCall( functionType, calleeValue, arguments, "calltmp" );
				}
				if( llvm::Function* function = llvm::dyn_cast<llvm::Function>( calleeValue ) ) {
					return this->builder.CreateCall( function, arguments, function->getReturnType()->isVoidTy() ? "" : "calltmp" );
				}
				return this->builder.CreateCall( llvm::FunctionType::get( llvm::Type::getInt64Ty( this->context ), {}, true ), calleeValue, arguments, "calltmp" );
			}
			return nullptr;
		}
		
		// Look up function with overload resolution via type-signature key
		std::unordered_map<std::string,llvm::Function*>::iterator functionIterator = this->functions.find( functionName );
		if( functionIterator != this->functions.end() && functionIterator->second->arg_size() != expression.arguments.size() ) {
			// Build type-signature key from the semantic analyzer's resolved overload
			if( expression.callee->kind == ast::Node::Kind::IdentifierExpression ) {
				ast::nodes::IdentifierExpression& identCallee = static_cast<ast::nodes::IdentifierExpression&>( *expression.callee );
				if( identCallee.resolvedSymbol != nullptr && identCallee.resolvedSymbol->typeref != nullptr &&
					identCallee.resolvedSymbol->typeref->kind == semantic::Type::Kind::Function ) {
					semantic::FunctionTypeSharedPointer resolvedFuncType = std::static_pointer_cast<semantic::FunctionType>( identCallee.resolvedSymbol->typeref );
					std::string callParamSignature;
					for( const semantic::TypeSharedPointer& paramType : resolvedFuncType->parameterTypes ) {
						if( callParamSignature.empty() == false ) {
							callParamSignature += ",";
						}
						callParamSignature += paramType->name;
					}
					std::string overloadKey = fmt::format( "{}#{}", functionName, callParamSignature );
					std::unordered_map<std::string,llvm::Function*>::iterator overloadIterator = this->functions.find( overloadKey );
					if( overloadIterator != this->functions.end() ) {
						functionIterator = overloadIterator;
					}
				}
			}
		}
		if( functionIterator == this->functions.end() ) {
			// Try type-signature key when base name not found at all
			if( expression.callee->kind == ast::Node::Kind::IdentifierExpression ) {
				ast::nodes::IdentifierExpression& identCallee = static_cast<ast::nodes::IdentifierExpression&>( *expression.callee );
				if( identCallee.resolvedSymbol != nullptr && identCallee.resolvedSymbol->typeref != nullptr &&
					identCallee.resolvedSymbol->typeref->kind == semantic::Type::Kind::Function ) {
					semantic::FunctionTypeSharedPointer resolvedFuncType = std::static_pointer_cast<semantic::FunctionType>( identCallee.resolvedSymbol->typeref );
					std::string callParamSignature;
					for( const semantic::TypeSharedPointer& paramType : resolvedFuncType->parameterTypes ) {
						if( callParamSignature.empty() == false ) {
							callParamSignature += ",";
						}
						callParamSignature += paramType->name;
					}
					std::string overloadKey = fmt::format( "{}#{}", functionName, callParamSignature );
					functionIterator = this->functions.find( overloadKey );
				}
			}
		}
		if( functionIterator == this->functions.end() ) {
			
			// Try as external function
			llvm::Function* externalFunction = this->module->getFunction( functionName );
			if( externalFunction ) {
				std::vector<llvm::Value*> arguments;
				for( ast::nodes::ExpressionSharedPointer& argument : expression.arguments ) {
					llvm::Value* value = this->generateExpression( argument );
					if( value ) {
						arguments.push_back( value );
					}
				}
				return this->builder.CreateCall( externalFunction, arguments, "calltmp" );
			}
			
			// Try as function pointer variable (Callable)
			std::unordered_map<std::string,llvm::Value*>::iterator variableIterator = this->namedValues.find( functionName );
			if( variableIterator != this->namedValues.end() ) {
				std::string variableName( fmt::format( "{}.ptr", functionName ) );
				llvm::Value* alloca = variableIterator->second;
				llvm::LoadInst* loaded = this->builder.CreateLoad( getPointeeType( alloca ), alloca, variableName );
				if( loaded->getType()->isPointerTy() ) {
					llvm::FunctionType* functionType = nullptr;
					llvm::Type* pointerElementType = getPointeeType( loaded );
					if( pointerElementType->isFunctionTy() ) {
						functionType = llvm::cast<llvm::FunctionType>( pointerElementType );
					}
					else if( pointerElementType->isPointerTy() ) {
						llvm::Type* i64Type = llvm::Type::getInt64Ty( this->context );
						llvm::Type* callableReturnType = i64Type;
						std::vector<llvm::Type*> callableParamTypes;
						if( expression.callee->kind == ast::Node::Kind::IdentifierExpression ) {
							ast::nodes::IdentifierExpression& identCallee = static_cast<ast::nodes::IdentifierExpression&>( *expression.callee );
							if( identCallee.resolvedSymbol && identCallee.resolvedSymbol->typeref &&
								identCallee.resolvedSymbol->typeref->kind == semantic::Type::Kind::Callable ) {
								semantic::CallableTypeSharedPointer callableType = std::static_pointer_cast<semantic::CallableType>( identCallee.resolvedSymbol->typeref );
								if( callableType->returnType ) {
									callableReturnType = this->toLLVMType( callableType->returnType );
								}
								for( const semantic::TypeSharedPointer& paramType : callableType->parameterTypes ) {
									callableParamTypes.push_back( this->toLLVMType( paramType ) );
								}
							}
						}
						if( callableParamTypes.empty() ) {
							for( size_t i = 0; i < expression.arguments.size(); i++ ) {
								callableParamTypes.push_back( i64Type );
							}
						}
						functionType = llvm::FunctionType::get( callableReturnType, callableParamTypes, false );
					}
					if( functionType ) {
						std::vector<llvm::Value*> arguments;
						for( size_t i=0; i<expression.arguments.size(); i++ ) {
							llvm::Value* value = this->generateExpression( expression.arguments[i] );
							if( value == nullptr ) continue;
							if( i < functionType->getNumParams() ) {
								value = generateImplicitCast( value, functionType->getParamType( i ) );
							}
							arguments.push_back( value );
						}
						if( functionType->getReturnType()->isVoidTy() ) {
							this->builder.CreateCall( functionType, loaded, arguments );
							return nullptr;
						}
						return this->builder.CreateCall( functionType, loaded, arguments, "calltmp" );
					}
				}
			}
			
			for( ast::nodes::ExpressionSharedPointer& argument : expression.arguments ) {
				this->generateExpression( argument );
			}
			return nullptr;
		}
		
		// Look up function's sema type for interface param and union param detection
		semantic::FunctionTypeSharedPointer functionSemantic;
		if( expression.callee->kind == ast::Node::Kind::IdentifierExpression ) {
			ast::nodes::IdentifierExpression& calleIdent = static_cast<ast::nodes::IdentifierExpression&>( *expression.callee );
			if( calleIdent.resolvedSymbol && calleIdent.resolvedSymbol->typeref &&
				calleIdent.resolvedSymbol->typeref->kind == semantic::Type::Kind::Function ) {
				functionSemantic = std::static_pointer_cast<semantic::FunctionType>( calleIdent.resolvedSymbol->typeref );
			}
		}
		if( functionSemantic == nullptr ) {
			functionSemantic = std::dynamic_pointer_cast<semantic::FunctionType>( this->analyzer.types().lookupType( functionName ) );
		}
		
		std::vector<llvm::Value*> arguments;
		llvm::Function* functionLLVM = functionIterator->second;
		llvm::Type* int64Type = llvm::Type::getInt64Ty( this->context );
		
		// Check for variadic/keyword param info (try resolved overload key if base name misses)
		std::string resolvedFunctionKey = functionName;
		for( const std::pair<const std::string, llvm::Function*>& functionEntry : this->functions ) {
			if( functionEntry.second == functionIterator->second ) {
				resolvedFunctionKey = functionEntry.first;
				break;
			}
		}
		std::unordered_map<std::string, FunctionParamInfo>::iterator paramInfoIter = this->functionParamInfos.find( resolvedFunctionKey );
		if( paramInfoIter == this->functionParamInfos.end() ) {
			paramInfoIter = this->functionParamInfos.find( functionName );
		}
		int variadicIdx = ( paramInfoIter != this->functionParamInfos.end() ) ? paramInfoIter->second.variadicIndex : -1;
		int keywordIdx = ( paramInfoIter != this->functionParamInfos.end() ) ? paramInfoIter->second.keywordIndex : -1;
		
		// Determine fixed param count
		size_t fixedParamCount;
		if( variadicIdx >= 0 ) {
			fixedParamCount = static_cast<size_t>( variadicIdx );
		}
		else if( keywordIdx >= 0 ) {
			fixedParamCount = static_cast<size_t>( keywordIdx );
		}
		else {
			fixedParamCount = functionLLVM->arg_size();
		}
		
		// Generate fixed positional arguments
		size_t parameterIndex = 0;
		for( size_t argIdx = 0; argIdx < expression.arguments.size() && parameterIndex < fixedParamCount; argIdx++ ) {
			ast::nodes::ExpressionSharedPointer& argument = expression.arguments[argIdx];
			if( argument->kind == ast::Node::Kind::IdentifierExpression ) {
				ast::nodes::IdentifierExpression& argumentIdentifier = static_cast<ast::nodes::IdentifierExpression&>( *argument );
				if( this->arenaElementTypes.count( argumentIdentifier.name ) ) {
					std::unordered_map<std::string,llvm::Value*>::iterator namedValueIterator = this->namedValues.find( argumentIdentifier.name );
					if( namedValueIterator != this->namedValues.end() ) {
						arguments.push_back( namedValueIterator->second );
						parameterIndex++;
						continue;
					}
				}
			}
			llvm::Value* valueLLVM = this->generateExpression( argument );
			if( valueLLVM == nullptr ) continue;
			if( parameterIndex < functionLLVM->arg_size() ) {
				llvm::Type* expectedType = functionLLVM->getArg( parameterIndex )->getType();
				if( expectedType == this->getInterfaceFatPointerType() && valueLLVM->getType()->isPointerTy() ) {
					std::string className;
					llvm::Type* pointerElementType = getPointeeType( valueLLVM );
					if( pointerElementType->isStructTy() ) {
						className = llvm::cast<llvm::StructType>( pointerElementType )->getName().str();
					}
					if( className.empty() && valueLLVM->hasName() ) {
						std::unordered_map<std::string, std::string>::iterator vsIt = this->variableStructType.find( valueLLVM->getName().str() );
						if( vsIt != this->variableStructType.end() ) {
							className = vsIt->second;
						}
					}
					if( className.empty() && this->lastConstructedClassName.empty() == false ) {
						className = this->lastConstructedClassName;
					}
					if( className.empty() && argument->semanticType &&
						( argument->semanticType->kind == semantic::Type::Kind::Class ||
						  argument->semanticType->kind == semantic::Type::Kind::Struct ) ) {
						className = argument->semanticType->name;
					}
					if( className.empty() == false ) {
						std::string classPrefix( fmt::format( "{}::", className ) );
						for( const std::pair<const std::string, llvm::GlobalVariable*>& pair : this->interfaceTables ) {
							if( pair.first.find( classPrefix ) == 0 ) {
								std::string interfaceName = pair.first.substr( classPrefix.size() );
								this->lastConstructedClassName.clear();
								valueLLVM = this->createInterfaceFatPointer( valueLLVM, className, interfaceName );
								break;
							}
						}
					}
				}
				else if( expectedType->isPointerTy() && valueLLVM->getType()->isPointerTy() == false &&
					functionSemantic && parameterIndex < functionSemantic->parameterTypes.size() &&
					functionSemantic->parameterTypes[parameterIndex]->kind == semantic::Type::Kind::Union ) {
					semantic::UnionTypeSharedPointer unionParamType = std::static_pointer_cast<semantic::UnionType>( functionSemantic->parameterTypes[parameterIndex] );
					uint64_t unionMaxSize = 0;
					llvm::Type* unionWidestLLVMType = llvm::Type::getInt64Ty( this->context );
					for( semantic::TypeSharedPointer& unionMemberType : unionParamType->types ) {
						llvm::Type* unionMemberLLVMType = this->toLLVMType( unionMemberType );
						uint64_t unionMemberSize = this->module->getDataLayout().getTypeAllocSize( unionMemberLLVMType );
						if( unionMemberSize > unionMaxSize ) {
							unionMaxSize = unionMemberSize;
							unionWidestLLVMType = unionMemberLLVMType;
						}
					}
					llvm::StructType* unionStructType = llvm::StructType::get( this->context, {
						llvm::Type::getInt32Ty( this->context ),
						unionWidestLLVMType
					});
					int unionTag = 0;
					for( size_t unionIdx = 0; unionIdx < unionParamType->types.size(); unionIdx++ ) {
						llvm::Type* unionMemberLLVMType = this->toLLVMType( unionParamType->types[unionIdx] );
						if( unionMemberLLVMType == valueLLVM->getType() ) {
							unionTag = static_cast<int>( unionIdx );
							break;
						}
					}
					llvm::Value* unionAlloca = this->createEntryBlockAllocation( this->currentFunction, "union.box", unionStructType );
					llvm::Value* unionTagPtr = this->builder.CreateStructGEP( unionStructType, unionAlloca, 0, "union.tag.ptr" );
					this->builder.CreateStore( llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->context ), unionTag ), unionTagPtr );
					llvm::Value* unionValPtr = this->builder.CreateStructGEP( unionStructType, unionAlloca, 1, "union.val.ptr" );
					this->builder.CreateStore( valueLLVM, unionValPtr );
					valueLLVM = unionAlloca;
				}
				else {
					std::string argTypeName = this->resolveStructTypeName( argument );
					bool isArgUnsigned = ( argTypeName == "U8" || argTypeName == "U16" || argTypeName == "U32" ||
						argTypeName == "U64" || argTypeName == "UInt" || argTypeName == "Byte" || argTypeName == "Char" );
					valueLLVM = this->generateImplicitCast( valueLLVM, expectedType, isArgUnsigned );
				}
			}
			arguments.push_back( valueLLVM );
			parameterIndex++;
		}
		
		// Compute variadic arg count excluding keyword-only positional args
		size_t keywordOnlyParamCount = ( paramInfoIter != this->functionParamInfos.end() ) ? paramInfoIter->second.keywordOnlyParams.size() : 0;
		size_t extraArgCount = 0;
		bool hasVariadicForward = false;
		
		// Pack variadic arguments into Args<T> struct on stack
		if( variadicIdx >= 0 ) {
			size_t extraArgStart = fixedParamCount;
			size_t totalExtraArgs = ( expression.arguments.size() > extraArgStart ) ? expression.arguments.size() - extraArgStart : 0;
			extraArgCount = totalExtraArgs;
			if( keywordOnlyParamCount > 0 && totalExtraArgs > 0 ) {
				ast::nodes::ExpressionSharedPointer& firstExtraArg = expression.arguments[extraArgStart];
				if( firstExtraArg->kind == ast::Node::Kind::IdentifierExpression ) {
					ast::nodes::IdentifierExpression& argIdentifier = static_cast<ast::nodes::IdentifierExpression&>( *firstExtraArg );
					std::unordered_map<std::string, std::string>::iterator argStructTypeIterator = this->variableStructType.find( argIdentifier.name );
					if( argStructTypeIterator != this->variableStructType.end() && argStructTypeIterator->second.find( "Args<" ) == 0 ) {
						hasVariadicForward = true;
						extraArgCount = 0;
					}
				}
			}
			if( hasVariadicForward ) {
				llvm::Value* forwardedArgs = this->generateExpression( expression.arguments[extraArgStart] );
				arguments.push_back( forwardedArgs );
			}
			else {
				llvm::Type* elemType = int64Type;
				if( paramInfoIter != this->functionParamInfos.end() && paramInfoIter->second.variadicElementLLVMType ) {
					elemType = paramInfoIter->second.variadicElementLLVMType;
				}
				llvm::StructType* argsStructType = llvm::StructType::get( this->context, {
					llvm::PointerType::getUnqual( elemType ),
					int64Type,
					int64Type
				});
				llvm::Type* elemPtrType = llvm::PointerType::getUnqual( elemType );
				llvm::AllocaInst* argsAlloca = this->createEntryBlockAllocation( this->currentFunction, "pack.args", argsStructType );
				if( extraArgCount > 0 ) {
					llvm::ArrayType* dataArrayType = llvm::ArrayType::get( elemType, extraArgCount );
					llvm::AllocaInst* dataAlloca = this->createEntryBlockAllocation( this->currentFunction, "pack.args.data", dataArrayType );
					for( size_t i = 0; i < extraArgCount; i++ ) {
						llvm::Value* argVal = this->generateExpression( expression.arguments[extraArgStart + i] );
						if( argVal ) {
							argVal = this->generateImplicitCast( argVal, elemType );
							llvm::Value* elemGep = this->builder.CreateConstGEP2_32( dataArrayType, dataAlloca, 0, static_cast<unsigned>( i ), "pack.args.gep" );
							this->builder.CreateStore( argVal, elemGep );
						}
					}
					llvm::Value* dataPtr = this->builder.CreateBitCast( dataAlloca, elemPtrType, "pack.args.ptr" );
					llvm::Value* dataFieldPtr = this->builder.CreateStructGEP( argsStructType, argsAlloca, 0, "pack.args.data.field" );
					this->builder.CreateStore( dataPtr, dataFieldPtr );
				}
				else {
					llvm::Value* dataFieldPtr = this->builder.CreateStructGEP( argsStructType, argsAlloca, 0, "pack.args.data.field" );
					this->builder.CreateStore( llvm::ConstantPointerNull::get( llvm::cast<llvm::PointerType>( elemPtrType ) ), dataFieldPtr );
				}
				llvm::Value* countFieldPtr = this->builder.CreateStructGEP( argsStructType, argsAlloca, 1, "pack.args.count.field" );
				this->builder.CreateStore( llvm::ConstantInt::get( int64Type, extraArgCount ), countFieldPtr );
				llvm::Value* posFieldPtr = this->builder.CreateStructGEP( argsStructType, argsAlloca, 2, "pack.args.pos.field" );
				this->builder.CreateStore( llvm::ConstantInt::get( int64Type, 0 ), posFieldPtr );
				arguments.push_back( argsAlloca );
			}
		}
		else {
			for( size_t argIdx = fixedParamCount; argIdx < expression.arguments.size(); argIdx++ ) {
				llvm::Value* valueLLVM = this->generateExpression( expression.arguments[argIdx] );
				if( valueLLVM ) {
					arguments.push_back( valueLLVM );
				}
			}
		}
		
		// Resolve keyword-only params (params with defaults after variadic)
		if( paramInfoIter != this->functionParamInfos.end() && paramInfoIter->second.keywordOnlyParams.empty() == false ) {
			std::vector<KeywordOnlyParam>& kwOnlyParams = paramInfoIter->second.keywordOnlyParams;
			size_t variadicSlotCount = extraArgCount + ( hasVariadicForward ? 1 : 0 );
			size_t positionalKwStart = fixedParamCount + variadicSlotCount;
			for( size_t kwpIdx = 0; kwpIdx < kwOnlyParams.size(); kwpIdx++ ) {
				KeywordOnlyParam& kwParam = kwOnlyParams[kwpIdx];
				bool matched = false;
				size_t positionalIndex = positionalKwStart + kwpIdx;
				if( positionalIndex < expression.arguments.size() ) {
					llvm::Value* positionalValue = this->generateExpression( expression.arguments[positionalIndex] );
					if( positionalValue && kwParam.llvmType ) {
						positionalValue = this->generateImplicitCast( positionalValue, kwParam.llvmType );
					}
					arguments.push_back( positionalValue );
					matched = true;
				}
				if( matched == false ) {
					for( size_t kaIdx = 0; kaIdx < expression.keywordArguments.size(); kaIdx++ ) {
						if( expression.keywordArguments[kaIdx].name == kwParam.name ) {
							llvm::Value* kwArgValue = this->generateExpression( expression.keywordArguments[kaIdx].value );
							if( kwArgValue && kwParam.llvmType ) {
								kwArgValue = this->generateImplicitCast( kwArgValue, kwParam.llvmType );
							}
							arguments.push_back( kwArgValue );
							matched = true;
							break;
						}
					}
				}
				if( matched == false ) {
					llvm::Value* defaultValue = this->generateExpression( kwParam.defaultValue );
					if( defaultValue && kwParam.llvmType ) {
						defaultValue = this->generateImplicitCast( defaultValue, kwParam.llvmType );
					}
					arguments.push_back( defaultValue );
				}
			}
		}
		
		// Pack keyword arguments into Kwargs<T> struct on stack
		if( keywordIdx >= 0 ) {
			size_t kwargCount = expression.keywordArguments.size();
			llvm::Type* valType = int64Type;
			if( paramInfoIter != this->functionParamInfos.end() && paramInfoIter->second.keywordValueLLVMType ) {
				valType = paramInfoIter->second.keywordValueLLVMType;
			}
			llvm::Type* keysPtrType = llvm::PointerType::getUnqual( llvm::PointerType::getUnqual( this->context ) );
			llvm::Type* valsPtrType = llvm::PointerType::getUnqual( valType );
			llvm::StructType* kwargsStructType = llvm::StructType::get( this->context, {
				keysPtrType, valsPtrType, int64Type, int64Type
			});
			llvm::AllocaInst* kwargsAlloca = this->createEntryBlockAllocation( this->currentFunction, "pack.kwargs", kwargsStructType );
			if( kwargCount > 0 ) {
				llvm::ArrayType* keysArrayType = llvm::ArrayType::get( llvm::PointerType::getUnqual( this->context ), kwargCount );
				llvm::ArrayType* valsArrayType = llvm::ArrayType::get( valType, kwargCount );
				llvm::AllocaInst* keysAlloca = this->createEntryBlockAllocation( this->currentFunction, "pack.kwargs.keys", keysArrayType );
				llvm::AllocaInst* valsAlloca = this->createEntryBlockAllocation( this->currentFunction, "pack.kwargs.vals", valsArrayType );
				for( size_t i = 0; i < kwargCount; i++ ) {
					ast::nodes::KeywordArgument& kwarg = expression.keywordArguments[i];
					llvm::Value* keyStr = this->builder.CreateGlobalStringPtr( kwarg.name, "kwarg.key" );
					llvm::Value* keyGep = this->builder.CreateConstGEP2_32( keysArrayType, keysAlloca, 0, static_cast<unsigned>( i ), "pack.kwargs.key.gep" );
					this->builder.CreateStore( keyStr, keyGep );
					llvm::Value* valExpr = this->generateExpression( kwarg.value );
					if( valExpr ) {
						valExpr = this->generateImplicitCast( valExpr, valType );
						llvm::Value* valGep = this->builder.CreateConstGEP2_32( valsArrayType, valsAlloca, 0, static_cast<unsigned>( i ), "pack.kwargs.val.gep" );
						this->builder.CreateStore( valExpr, valGep );
					}
				}
				llvm::Value* keysPtr = this->builder.CreateBitCast( keysAlloca, keysPtrType, "pack.kwargs.keys.ptr" );
				llvm::Value* valsPtr = this->builder.CreateBitCast( valsAlloca, valsPtrType, "pack.kwargs.vals.ptr" );
				llvm::Value* keysFieldPtr = this->builder.CreateStructGEP( kwargsStructType, kwargsAlloca, 0, "pack.kwargs.keys.field" );
				this->builder.CreateStore( keysPtr, keysFieldPtr );
				llvm::Value* valsFieldPtr = this->builder.CreateStructGEP( kwargsStructType, kwargsAlloca, 1, "pack.kwargs.vals.field" );
				this->builder.CreateStore( valsPtr, valsFieldPtr );
			}
			else {
				llvm::Value* keysFieldPtr = this->builder.CreateStructGEP( kwargsStructType, kwargsAlloca, 0, "pack.kwargs.keys.field" );
				this->builder.CreateStore( llvm::ConstantPointerNull::get( llvm::cast<llvm::PointerType>( keysPtrType ) ), keysFieldPtr );
				llvm::Value* valsFieldPtr = this->builder.CreateStructGEP( kwargsStructType, kwargsAlloca, 1, "pack.kwargs.vals.field" );
				this->builder.CreateStore( llvm::ConstantPointerNull::get( llvm::cast<llvm::PointerType>( valsPtrType ) ), valsFieldPtr );
			}
			llvm::Value* countFieldPtr = this->builder.CreateStructGEP( kwargsStructType, kwargsAlloca, 2, "pack.kwargs.count.field" );
			this->builder.CreateStore( llvm::ConstantInt::get( int64Type, kwargCount ), countFieldPtr );
			llvm::Value* posFieldPtr = this->builder.CreateStructGEP( kwargsStructType, kwargsAlloca, 3, "pack.kwargs.pos.field" );
			this->builder.CreateStore( llvm::ConstantInt::get( int64Type, 0 ), posFieldPtr );
			arguments.push_back( kwargsAlloca );
		}
		
		if( functionLLVM->getReturnType()->isVoidTy() ) {
			this->createCallOrInvoke( functionLLVM, arguments );
			return nullptr;
		}
		return this->createCallOrInvoke( functionLLVM, arguments, "calltmp" );
	}
	
	void LLVMCodegen::preRegisterClassMethods( ast::nodes::ClassDeclaration& declaration, const semantic::ClassTypeSharedPointer& classType ) {
		this->currentClassName = declaration.name;
		for( ast::nodes::DeclarationSharedPointer& method : declaration.methods ) {
			if( method->kind != ast::Node::Kind::FunctionDeclaration ) {
				continue;
			}
			ast::nodes::FunctionDeclaration& methodFunctionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *method );
			llvm::Type* methodFunctionReturnType = this->resolveAstType( methodFunctionDeclaration.returnType );
			if( methodFunctionDeclaration.isAsync &&
				methodFunctionReturnType &&
				methodFunctionReturnType->isVoidTy() == false ) {
				methodFunctionReturnType = llvm::StructType::get( this->context, { llvm::Type::getInt1Ty( this->context ), methodFunctionReturnType } );
			}
			std::vector<llvm::Type*> parameterTypes;
			FunctionParamInfo methodParamInfo;
			int methodNonSelfParamIndex = 0;
			for( ast::nodes::FunctionParameterSharedPointer& methodFunctionParameter : methodFunctionDeclaration.parameters ) {
				if( methodFunctionParameter->isSelf ) {
					parameterTypes.push_back( llvm::PointerType::getUnqual( this->structTypes[declaration.name] ) );
				}
				else if( methodFunctionParameter->isVariadic ) {
					llvm::Type* variadicElementType = llvm::Type::getInt64Ty( this->context );
					if( methodFunctionParameter->type && methodFunctionParameter->type->kind == ast::Node::Kind::ArrayType ) {
						ast::nodes::ArrayTypeNode& arrayNode = static_cast<ast::nodes::ArrayTypeNode&>( *methodFunctionParameter->type );
						if( arrayNode.elementType ) {
							variadicElementType = this->resolveAstType( arrayNode.elementType );
						}
					}
					else if( methodFunctionParameter->type ) {
						variadicElementType = this->resolveAstType( methodFunctionParameter->type );
					}
					llvm::StructType* argsStruct = llvm::StructType::get( this->context, {
						llvm::PointerType::getUnqual( variadicElementType ),
						llvm::Type::getInt64Ty( this->context ),
						llvm::Type::getInt64Ty( this->context )
					});
					parameterTypes.push_back( llvm::PointerType::getUnqual( argsStruct ) );
					methodParamInfo.variadicIndex = methodNonSelfParamIndex;
					methodParamInfo.variadicElementLLVMType = variadicElementType;
					methodNonSelfParamIndex++;
				}
				else if( methodFunctionParameter->type ) {
					llvm::Type* methodFunctionParameterLLVMType = this->resolveAstType( methodFunctionParameter->type );
					if( methodFunctionParameterLLVMType->isStructTy() && methodFunctionParameterLLVMType != this->getInterfaceFatPointerType() ) {
						methodFunctionParameterLLVMType = llvm::PointerType::getUnqual( methodFunctionParameterLLVMType );
					}
					parameterTypes.push_back( methodFunctionParameterLLVMType );
					methodNonSelfParamIndex++;
				}
				else {
					parameterTypes.push_back( llvm::Type::getInt64Ty( this->context ) );
					methodNonSelfParamIndex++;
				}
			}
			std::string mapKey( fmt::format( "{}::{}", declaration.name, methodFunctionDeclaration.name ) );
			std::string methodFunctionMangledName( this->mangleName( methodFunctionDeclaration.name, declaration.name ) );
			llvm::FunctionType* methodFunctionType = llvm::FunctionType::get( methodFunctionReturnType, parameterTypes, false );
			llvm::Function* methodFunction = llvm::Function::Create( methodFunctionType, llvm::Function::ExternalLinkage, methodFunctionMangledName, this->getModule() );
			if( this->functions.count( mapKey ) ) {
				mapKey+= fmt::format( "#{}", std::to_string( parameterTypes.size() ) );
			}
			this->functions[mapKey] = methodFunction;
			this->functionParamInfos[mapKey] = methodParamInfo;
		}
		this->currentClassName.clear();
	}
	
	bool LLVMCodegen::ensureClassMethodsRegistered( const std::string& className ) {
		if( this->preRegisteredClasses.count( className ) ) {
			return true;
		}
		static const std::set<std::string> oopWrapperClasses = {
			"Int", "I8", "I16", "I32", "I64", "UInt", "U8", "U16", "U32", "U64",
			"Float", "F32", "F64", "Double", "Long", "Integer", "Boolean", "Byte",
			"Char", "String", "Void", "Object"
		};
		if( oopWrapperClasses.count( className ) ) {
			return false;
		}
		semantic::TypeSharedPointer semanticType = this->analyzer.types().lookupType( className );
		if( semanticType == nullptr ) {
			return false;
		}
		if( semanticType->kind != semantic::Type::Kind::Class && semanticType->kind != semantic::Type::Kind::Struct ) {
			return false;
		}
		
		// Save codegen state before lazy registration
		std::string savedClassName = this->currentClassName;
		llvm::Function* savedFunction = this->currentFunction;
		std::unordered_map<std::string, llvm::Value*> savedNamedValues = this->namedValues;
		std::unordered_map<std::string, std::string> savedVariableStructType = this->variableStructType;
		std::unordered_map<std::string, llvm::Type*> savedVarMemoryElementTypes = this->varMemoryElementTypes;
		std::unordered_map<std::string, llvm::Type*> savedArenaElementTypes = this->arenaElementTypes;
		llvm::BasicBlock* savedInsertPoint = this->builder.GetInsertBlock();
		std::stack<llvm::BasicBlock*> savedLandingPads;
		std::swap( savedLandingPads, this->landingPads );
		std::vector<std::vector<ScopeCleanupEntry>> savedScopeCleanupStack;
		std::swap( savedScopeCleanupStack, this->scopeCleanupStack );
		std::vector<std::vector<ast::nodes::StatementSharedPointer>> savedDeferScopeStack;
		std::swap( savedDeferScopeStack, this->deferScopeStack );
		std::unordered_map<std::string, llvm::Value*> savedAliveFlags;
		std::swap( savedAliveFlags, this->aliveFlags );
		if( semanticType->kind == semantic::Type::Kind::Struct ) {
			semantic::StructTypeSharedPointer structType = std::static_pointer_cast<semantic::StructType>( semanticType );
			if( structType->astDeclaration == nullptr ) {
				return false;
			}
			if( structType->astDeclaration->genericParameters.empty() == false && className.find( '<' ) == std::string::npos ) {
				if( this->structTypes.count( className ) == 0 ) {
					return false;
				}
			}
			this->preRegisteredClasses.insert( className );
			this->generateStructDeclaration( *structType->astDeclaration );
		}
		else {
			semantic::ClassTypeSharedPointer classType = std::static_pointer_cast<semantic::ClassType>( semanticType );
			if( classType->astDeclaration == nullptr || classType->astDeclaration->isNative ) {
				return false;
			}
			if( classType->astDeclaration->genericParameters.empty() == false && className.find( '<' ) == std::string::npos ) {
				if( this->structTypes.count( className ) == 0 ) {
					return false;
				}
			}
			this->preRegisteredClasses.insert( className );
			this->getOrCreateStructType( className, classType );
			this->preRegisterClassMethods( *classType->astDeclaration, classType );
			this->generateClassDeclaration( *classType->astDeclaration );
		}

		// Restore codegen state
		this->currentClassName = savedClassName;
		this->currentFunction = savedFunction;
		this->namedValues = savedNamedValues;
		this->variableStructType = savedVariableStructType;
		this->varMemoryElementTypes = savedVarMemoryElementTypes;
		this->arenaElementTypes = savedArenaElementTypes;
		std::swap( this->landingPads, savedLandingPads );
		std::swap( this->scopeCleanupStack, savedScopeCleanupStack );
		std::swap( this->deferScopeStack, savedDeferScopeStack );
		std::swap( this->aliveFlags, savedAliveFlags );
		if( savedInsertPoint ) {
			this->builder.SetInsertPoint( savedInsertPoint );
		}
		return true;
	}
	
	void LLVMCodegen::generateClassDeclaration( ast::nodes::ClassDeclaration& declaration ) {
		if( declaration.isNative ) {
			return;
		}
		
		// OOP wrapper classes are lowered to primitives — skip method body generation
		static const std::set<std::string> oopWrapperClasses = {
			"Int", "I8", "I16", "I32", "I64", "UInt", "U8", "U16", "U32", "U64",
			"Float", "F32", "F64", "Double", "Long", "Integer", "Boolean", "Byte",
			"Char", "String", "Void", "Object"
		};
		if( oopWrapperClasses.count( declaration.name ) ) {
			return;
		}
		semantic::ClassTypeSharedPointer classType = std::dynamic_pointer_cast<semantic::ClassType>( this->analyzer.types().lookupType( declaration.name ) );
		this->getOrCreateStructType( declaration.name, classType );
		
		// Pass 1: Methods already pre-registered in Phase 1
		this->currentClassName = declaration.name;
		
		// Pass 2: Generate method bodies
		for( ast::nodes::DeclarationSharedPointer& method : declaration.methods ) {
			if( method->kind != ast::Node::Kind::FunctionDeclaration ) {
				continue;
			}
			ast::nodes::FunctionDeclaration& methodFunctionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *method );
			bool isEmptyBody = methodFunctionDeclaration.body.empty();
			std::string baseKey( fmt::format( "{}::{}", declaration.name, methodFunctionDeclaration.name ) );
			std::string mapKey( baseKey );
			llvm::Type* expectedReturnType = this->resolveAstType( methodFunctionDeclaration.returnType );
			if( methodFunctionDeclaration.isAsync && expectedReturnType && expectedReturnType->isVoidTy() == false ) {
				expectedReturnType = llvm::StructType::get( this->context, { llvm::Type::getInt1Ty( this->context ), expectedReturnType } );
			}
			llvm::Function* methodFunction = nullptr;
			std::unordered_map<std::string, llvm::Function*>::iterator funcIterator = this->functions.find( mapKey );
			if( funcIterator != this->functions.end() && funcIterator->second != nullptr &&
				funcIterator->second->arg_size() == methodFunctionDeclaration.parameters.size() &&
				funcIterator->second->getReturnType() == expectedReturnType ) {
				methodFunction = funcIterator->second;
			}
			if( methodFunction == nullptr ) {
				mapKey = fmt::format( "{}#{}", baseKey, methodFunctionDeclaration.parameters.size() );
				funcIterator = this->functions.find( mapKey );
				if( funcIterator != this->functions.end() && funcIterator->second != nullptr ) {
					methodFunction = funcIterator->second;
				}
			}
			if( methodFunction == nullptr || methodFunction->empty() == false ) {
				continue;
			}
			if( isEmptyBody ) {
				llvm::BasicBlock* stubBlock = llvm::BasicBlock::Create( this->context, "entry", methodFunction );
				this->builder.SetInsertPoint( stubBlock );
				if( methodFunction->getReturnType()->isVoidTy() ) {
					this->builder.CreateRetVoid();
				}
				else {
					this->builder.CreateRet( llvm::Constant::getNullValue( methodFunction->getReturnType() ) );
				}
				continue;
			}
			llvm::BasicBlock* methodFunctionBasicBlock = llvm::BasicBlock::Create( this->context, "entry", methodFunction );
			
			this->builder.SetInsertPoint( methodFunctionBasicBlock );
			this->currentFunction = methodFunction;
			this->namedValues.clear();
			this->variableStructType.clear();
			this->varMemoryElementTypes.clear();
			this->arenaElementTypes.clear();
			
			size_t methodFunctionParameterIndex = 0;
			for( ast::nodes::FunctionParameterSharedPointer& methodFunctionParameter : methodFunctionDeclaration.parameters ) {
				if( methodFunctionParameterIndex >= methodFunction->arg_size() ) {
					break;
				}
				std::string methodFunctionParameterName( methodFunctionParameter->isSelf ? "self" : methodFunctionParameter->name );
				llvm::Argument* methodFunctionParameterLLVM = methodFunction->getArg( methodFunctionParameterIndex );
				methodFunctionParameterLLVM->setName( methodFunctionParameterName );
				llvm::AllocaInst* alloca = this->createEntryBlockAllocation( methodFunction, methodFunctionParameterName, methodFunctionParameterLLVM->getType() );
				this->builder.CreateStore( methodFunctionParameterLLVM, alloca );
				this->namedValues[methodFunctionParameterName] = alloca;
				if( methodFunctionParameter->isSelf ) {
					this->variableStructType["self"] = declaration.name;
				}
				else if( methodFunctionParameter->isVariadic ) {
					std::string variadicElementTypeName = "I64";
					if( methodFunctionParameter->type && methodFunctionParameter->type->kind == ast::Node::Kind::ArrayType ) {
						ast::nodes::ArrayTypeNode& arrayTypeNode = static_cast<ast::nodes::ArrayTypeNode&>( *methodFunctionParameter->type );
						if( arrayTypeNode.elementType && arrayTypeNode.elementType->kind == ast::Node::Kind::SimpleType ) {
							variadicElementTypeName = static_cast<ast::nodes::SimpleTypeNode&>( *arrayTypeNode.elementType ).name;
						}
					}
					else if( methodFunctionParameter->type && methodFunctionParameter->type->kind == ast::Node::Kind::SimpleType ) {
						variadicElementTypeName = static_cast<ast::nodes::SimpleTypeNode&>( *methodFunctionParameter->type ).name;
					}
					std::string argsTypeName( fmt::format( "Args<{}>", variadicElementTypeName ) );
					this->variableStructType[methodFunctionParameterName] = argsTypeName;
				}
				else if( methodFunctionParameter->isSelf == false && methodFunctionParameter->type ) {
					std::string methodFunctionParameterTypeName;
					if( methodFunctionParameter->type->kind == ast::Node::Kind::SimpleType ) {
						methodFunctionParameterTypeName = static_cast<ast::nodes::SimpleTypeNode&>( *methodFunctionParameter->type ).name;
					}
					else if( methodFunctionParameter->type->kind == ast::Node::Kind::GenericType) {
						ast::nodes::GenericTypeNode& genericTypeNode = static_cast<ast::nodes::GenericTypeNode&>( *methodFunctionParameter->type );
						methodFunctionParameterTypeName = genericTypeNode.name;
						if( genericTypeNode.name == "Memory" ) {
							llvm::Type* elementType = llvm::Type::getInt64Ty( this->context );
							if( genericTypeNode.typeArguments.empty() == false ) {
								elementType = this->resolveAstType( genericTypeNode.typeArguments[0] );
							}
							this->varMemoryElementTypes[methodFunctionParameterName] = elementType;
						}
						if( genericTypeNode.name == "Arena" ) {
							llvm::Type* elementType = llvm::Type::getInt64Ty( this->context );
							if( genericTypeNode.typeArguments.empty() == false ) {
								elementType = this->resolveAstType( genericTypeNode.typeArguments[0] );
								elementType = unwrapStructPointer( elementType, genericTypeNode.typeArguments[0], this->structTypes );
							}
							this->arenaElementTypes[methodFunctionParameterName] = elementType;
						}
					}
					if( methodFunctionParameterTypeName.empty() == false ) {
						this->variableStructType[methodFunctionParameterName] = methodFunctionParameterTypeName;
					}
				}
				methodFunctionParameterIndex++;
			}
			
			// Auto-assign property params in constructor: self.field = param
			if( methodFunctionDeclaration.name == declaration.name ) {
				std::unordered_map<std::string,llvm::Value*>::iterator selfIterator = this->namedValues.find( "self" );
				if( selfIterator != this->namedValues.end() ) {
					llvm::LoadInst* selfPointer = this->builder.CreateLoad( getPointeeType( selfIterator->second ), selfIterator->second, "self.ptr" );
					std::unordered_map<std::string,llvm::StructType*>::iterator structTypeIterator = this->structTypes.find( declaration.name );
					if( structTypeIterator != this->structTypes.end() ) {
						int fieldIndex = 0;
						llvm::StructType* structType = structTypeIterator->second;
						for( ast::nodes::FunctionParameterSharedPointer& methodFunctionParameter : methodFunctionDeclaration.parameters ) {
							if( methodFunctionParameter->isProperty && methodFunctionParameter->isSelf == false ) {
								std::unordered_map<std::string,llvm::Value*>::iterator methodFunctionParameterValueIterator = this->namedValues.find( methodFunctionParameter->name );
								if( methodFunctionParameterValueIterator != this->namedValues.end() && fieldIndex < static_cast<int>( structType->getNumElements() ) ) {
									std::string methodFunctionParameterValueName( fmt::format( "{}.val", methodFunctionParameter->name ) );
									std::string methodFunctionParameterGEPName( fmt::format( "{}.field", methodFunctionParameter->name ) );
									llvm::LoadInst* methodFunctionParameterValue = this->builder.CreateLoad(
										getPointeeType( methodFunctionParameterValueIterator->second ),
										methodFunctionParameterValueIterator->second,
										methodFunctionParameterValueName
									);
									llvm::Value* methodFunctionParameterValueGEP = this->builder.CreateStructGEP( structType, selfPointer, fieldIndex, methodFunctionParameterGEPName );
									llvm::Type* methodFunctionParameterFieldType = structType->getElementType( fieldIndex );
									if( methodFunctionParameterValueGEP->getType()->isPointerTy() &&
										methodFunctionParameterFieldType->isStructTy() &&
										getPointeeType( methodFunctionParameterValue ) == methodFunctionParameterFieldType ) {
										methodFunctionParameterValue = this->builder.CreateLoad( methodFunctionParameterFieldType, methodFunctionParameterValue, fmt::format( "{}.deref", methodFunctionParameter->name ) );
									}
									this->builder.CreateStore( methodFunctionParameterValue, methodFunctionParameterValueGEP );
								}
							}
							if( methodFunctionParameter->isSelf == false ) {
								fieldIndex++;
							}
						}
					}
				}
			}
			
			this->currentFunctionEmittedPushFrame = false;
			if( methodFunctionDeclaration.source && methodFunctionDeclaration.source->location ) {
				std::string methodDisplayName = fmt::format( "{}.{}", declaration.name, methodFunctionDeclaration.name );
				this->emitPushFrame( methodFunctionDeclaration.source->filename, methodFunctionDeclaration.source->location->line, methodFunctionDeclaration.source->location->column, methodDisplayName );
				this->currentFunctionEmittedPushFrame = true;
			}
			for( ast::nodes::StatementSharedPointer& methodFunctionStatement : methodFunctionDeclaration.body ) {
				this->generateStatement( methodFunctionStatement );
			}
			llvm::BasicBlock* methodFunctionLastBasicBlock = this->builder.GetInsertBlock();
			if( methodFunctionLastBasicBlock && methodFunctionLastBasicBlock->getTerminator() == nullptr ) {
				if( this->currentFunctionEmittedPushFrame ) {
					this->emitPopFrame();
				}
				if( methodFunction->getReturnType()->isVoidTy() ) {
					this->builder.CreateRetVoid();
				}
				else {
					this->builder.CreateRet( llvm::Constant::getNullValue( methodFunction->getReturnType() ) );
				}
			}
			this->currentFunction = nullptr;
		}
		
		for( ast::nodes::DeclarationSharedPointer& nested : declaration.nestedDeclarations ) {
			this->generateDeclaration( nested );
		}
		
		// Generate VTable after all methods are created
		if( classType->virtualTable.empty() == false ) {
			this->generateVirtualTable( declaration.name, classType );
		}
		
		// Generate ITables for each interface this class implements (including parent interfaces)
		std::vector<semantic::InterfaceTypeSharedPointer> interfaceQueue;
		for( semantic::TypeSharedPointer& interface : classType->interfaces ) {
			if( interface && interface->kind == semantic::Type::Kind::Interface ) {
				interfaceQueue.push_back( std::static_pointer_cast<semantic::InterfaceType>( interface ) );
			}
		}
		for( size_t qi = 0; qi < interfaceQueue.size(); qi++ ) {
			semantic::InterfaceTypeSharedPointer interfaceType = interfaceQueue[qi];
			if( interfaceType->methodOrder.empty() == false ) {
				this->generateInterfaceTable( declaration.name, classType, interfaceType );
			}
			for( semantic::TypeSharedPointer& superInterface : interfaceType->superInterfaces ) {
				if( superInterface && superInterface->kind == semantic::Type::Kind::Interface ) {
					semantic::InterfaceTypeSharedPointer superInterfaceType = std::static_pointer_cast<semantic::InterfaceType>( superInterface );
					std::string superKey = fmt::format( "{}::{}", declaration.name, superInterfaceType->name );
					if( this->interfaceTables.count( superKey ) == 0 ) {
						interfaceQueue.push_back( superInterfaceType );
					}
				}
			}
		}
		if( classType ) {
			std::string metaGlobalName( fmt::format( "_AE_meta_{}", declaration.name ) );
			if( this->getModule()->getGlobalVariable( metaGlobalName, true ) == nullptr ) {
				std::string qualname = classType->package.empty() ? declaration.name : fmt::format( "{}.{}", classType->package, declaration.name );
				std::string metaValue( fmt::format( "{} class", qualname ) );
				llvm::Constant* metaConstant = llvm::ConstantDataArray::getString( this->context, metaValue, true );
				new llvm::GlobalVariable( *this->getModule(), metaConstant->getType(), true, llvm::GlobalValue::PrivateLinkage, metaConstant, metaGlobalName );
			}
		}
		this->currentClassName.clear();
	}
	
	llvm::Value* LLVMCodegen::generateConstructExpression( ast::nodes::ConstructExpression& expression ) {
		
		// Intrinsic: new Memory<T>(capacity) → malloc(capacity * sizeof(T))
		std::string constructName;
		if( expression.type->kind == ast::Node::Kind::SimpleType ) {
			constructName = static_cast<ast::nodes::SimpleTypeNode&>( *expression.type ).name;
		}
		else if( expression.type->kind == ast::Node::Kind::GenericType ) {
			constructName = static_cast<ast::nodes::GenericTypeNode&>( *expression.type ).name;
		}
		
		// Intrinsic Only
		{
			if( constructName == "Arena" ) {
				return this->generateConstructArenaExpression( expression );
			}
			if( constructName == "Memory" ) {
				return this->generateConstructMemoryExpression( expression );
			}
		}
		
		// Resolve the type name from the type node
		std::string typeName;
		std::string monomorphizedName; // Full monomorphized name like "Box<i64>"
		if( expression.type->kind == ast::Node::Kind::SimpleType ) {
			typeName = static_cast<ast::nodes::SimpleTypeNode&>( *expression.type ).name;
		}
		else if( expression.type->kind == ast::Node::Kind::GenericType ) {
			ast::nodes::GenericTypeNode& genericTypeNode = static_cast<ast::nodes::GenericTypeNode&>( *expression.type );
			typeName = genericTypeNode.name;
			
			monomorphizedName = this->buildMonomorphizedName( expression.type );
		}
		
		// Look up the LLVM struct type — try monomorphized name first, then base name
		llvm::StructType* structType = nullptr;
		std::string resolvedName( typeName );
		
		if( monomorphizedName.empty() == false ) {
			std::unordered_map<std::string,llvm::StructType*>::iterator structTypeIterator = this->structTypes.find( monomorphizedName );
			if( structTypeIterator != this->structTypes.end() ) {
				structType = structTypeIterator->second;
				resolvedName = monomorphizedName;
			}
		}
		
		if( structType == nullptr ) {
			std::unordered_map<std::string,llvm::StructType*>::iterator structTypeIterator = this->structTypes.find( typeName );
			if( structTypeIterator != this->structTypes.end() ) {
				structType = structTypeIterator->second;
			}
		}
		
		// If type still not found, try to create it from sema type registry
		if( structType == nullptr ) {
			semantic::TypeSharedPointer semanticType = ! monomorphizedName.empty() ? this->analyzer.types().lookupType( monomorphizedName ) : nullptr;
			if( semanticType == nullptr ) semanticType = this->analyzer.types().lookupType( typeName );
			if( semanticType && ( semanticType->kind == semantic::Type::Kind::Class || semanticType->kind == semantic::Type::Kind::Struct ) ) {
				structType = this->getOrCreateStructType( semanticType->name, semanticType );
				resolvedName = semanticType->name;
			}
		}
		
		if( structType == nullptr ) {
			for( std::pair<std::string,ast::nodes::ExpressionSharedPointer> pair : expression.fields ) {
				if( pair.second ) {
					this->generateExpression( pair.second );
				}
			}
			return llvm::ConstantPointerNull::get( llvm::PointerType::getUnqual( this->context ) );
		}
		
		// Heap-allocate using malloc
		std::string objectName( fmt::format( "{}.obj", resolvedName ) );
		llvm::DataLayout layout = this->module->getDataLayout();
		uint64_t structSize = layout.getTypeAllocSize( structType );
		llvm::Function* mallocFunction = getOrCreateMalloc();
		llvm::ConstantInt* sizeValue = llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->context ), structSize );
		llvm::CallInst* rawPointer = this->builder.CreateCall( mallocFunction, { sizeValue }, "rawptr" );
		llvm::Value* objectPointer = this->builder.CreateBitCast( rawPointer, llvm::PointerType::getUnqual( structType ), objectName );
		
		// Initialize vtable pointer if this class has virtual methods
		std::unordered_map<std::string,llvm::GlobalVariable*>::iterator vtableIterator = this->virtualTables.find( resolvedName );
		if( vtableIterator != this->virtualTables.end() ) {
			llvm::Value* vtableGEP = this->builder.CreateStructGEP( structType, objectPointer, 0, "vtable.slot" );
			llvm::Value* vtablePointerCast = this->builder.CreateBitCast( vtableIterator->second, llvm::PointerType::getUnqual( this->context ), "vtable.ptr" );
			this->builder.CreateStore( vtablePointerCast, vtableGEP );
		}
		
		// Populate fields: try constructor call first, fallback to direct field init
		if( resolvedName != typeName && this->structTypes.count( typeName ) == 0 && structType != nullptr ) {
			this->structTypes[typeName] = structType;
			if( this->structFieldIndices.count( resolvedName ) && this->structFieldIndices.count( typeName ) == 0 ) {
				this->structFieldIndices[typeName] = this->structFieldIndices[resolvedName];
			}
		}
		this->ensureClassMethodsRegistered( typeName );
		size_t constructParameterCount = static_cast<int>( expression.fields.size() ) + 1; // +1 for self
		std::string constructKey( fmt::format( "{}::{}", typeName, typeName ) );
		std::string constructFunctionName( fmt::format( "{}#{}", constructKey, constructParameterCount ) );
		std::unordered_map<std::string,llvm::Function*>::iterator constructFunction = this->functions.find( constructKey );
		if( constructFunction != this->functions.end() && constructFunction->second->arg_size() != constructParameterCount ) {
			std::unordered_map<std::string,llvm::Function*>::iterator constructOverloadFunction = this->functions.find( constructFunctionName );
			if( constructOverloadFunction != this->functions.end() ) {
				constructFunction = constructOverloadFunction;
			}
			else if( constructFunction->second->arg_size() < constructParameterCount ) {
				constructFunction = this->functions.end();
			}
		}
		if( constructFunction != this->functions.end() ) {

			// Call constructor: ClassName::ClassName(self, args...)
			std::string resolvedConstructKey = constructFunction->first;
			std::unordered_map<std::string, FunctionParamInfo>::iterator constructParamInfoIter = this->functionParamInfos.find( resolvedConstructKey );
			if( constructParamInfoIter == this->functionParamInfos.end() ) {
				constructParamInfoIter = this->functionParamInfos.find( constructKey );
			}
			int constructVariadicIdx = ( constructParamInfoIter != this->functionParamInfos.end() ) ? constructParamInfoIter->second.variadicIndex : -1;
			size_t constructFixedFieldCount;
			if( constructVariadicIdx >= 0 ) {
				constructFixedFieldCount = static_cast<size_t>( constructVariadicIdx );
			}
			else {
				constructFixedFieldCount = expression.fields.size();
			}
			size_t constructFunctionParameterIndex = 1;
			std::vector<llvm::Value*> constructFunctionArguments;
			constructFunctionArguments.push_back( objectPointer );
			for( size_t fieldIdx = 0; fieldIdx < expression.fields.size() && fieldIdx < constructFixedFieldCount; fieldIdx++ ) {
				llvm::Value* expressionFieldValue = this->generateExpression( expression.fields[fieldIdx].second );
				if( expressionFieldValue == nullptr ) continue;
				if( constructFunctionParameterIndex < constructFunction->second->arg_size() ) {
					expressionFieldValue = this->generateImplicitCast( expressionFieldValue, constructFunction->second->getArg( constructFunctionParameterIndex )->getType() );
				}
				constructFunctionArguments.push_back( expressionFieldValue );
				constructFunctionParameterIndex++;
			}
			if( constructVariadicIdx >= 0 ) {
				llvm::Type* int64Type = llvm::Type::getInt64Ty( this->context );
				size_t constructExtraStart = constructFixedFieldCount;
				size_t constructExtraCount = ( expression.fields.size() > constructExtraStart ) ? expression.fields.size() - constructExtraStart : 0;
				bool constructHasForward = false;
				if( constructExtraCount > 0 ) {
					ast::nodes::ExpressionSharedPointer& firstExtraField = expression.fields[constructExtraStart].second;
					if( firstExtraField->kind == ast::Node::Kind::IdentifierExpression ) {
						ast::nodes::IdentifierExpression& argIdentifier = static_cast<ast::nodes::IdentifierExpression&>( *firstExtraField );
						std::unordered_map<std::string, std::string>::iterator argStructTypeIter = this->variableStructType.find( argIdentifier.name );
						if( argStructTypeIter != this->variableStructType.end() && argStructTypeIter->second.find( "Args<" ) == 0 ) {
							constructHasForward = true;
						}
					}
				}
				if( constructHasForward ) {
					llvm::Value* forwardedArgs = this->generateExpression( expression.fields[constructExtraStart].second );
					constructFunctionArguments.push_back( forwardedArgs );
				}
				else {
					llvm::Type* constructElemType = int64Type;
					if( constructParamInfoIter != this->functionParamInfos.end() && constructParamInfoIter->second.variadicElementLLVMType ) {
						constructElemType = constructParamInfoIter->second.variadicElementLLVMType;
					}
					llvm::StructType* constructArgsStructType = llvm::StructType::get( this->context, {
						llvm::PointerType::getUnqual( constructElemType ),
						int64Type,
						int64Type
					});
					llvm::Type* constructElemPtrType = llvm::PointerType::getUnqual( constructElemType );
					llvm::AllocaInst* constructArgsAlloca = this->createEntryBlockAllocation( this->currentFunction, "cpack.args", constructArgsStructType );
					if( constructExtraCount > 0 ) {
						llvm::ArrayType* dataArrayType = llvm::ArrayType::get( constructElemType, constructExtraCount );
						llvm::AllocaInst* dataAlloca = this->createEntryBlockAllocation( this->currentFunction, "cpack.args.data", dataArrayType );
						for( size_t varArgIndex = 0; varArgIndex < constructExtraCount; varArgIndex++ ) {
							llvm::Value* argVal = this->generateExpression( expression.fields[constructExtraStart + varArgIndex].second );
							if( argVal ) {
								argVal = this->generateImplicitCast( argVal, constructElemType );
								llvm::Value* elemGep = this->builder.CreateConstGEP2_32( dataArrayType, dataAlloca, 0, static_cast<unsigned>( varArgIndex ), "cpack.args.gep" );
								this->builder.CreateStore( argVal, elemGep );
							}
						}
						llvm::Value* dataPtr = this->builder.CreateBitCast( dataAlloca, constructElemPtrType, "cpack.args.ptr" );
						llvm::Value* dataFieldPtr = this->builder.CreateStructGEP( constructArgsStructType, constructArgsAlloca, 0, "cpack.args.data.field" );
						this->builder.CreateStore( dataPtr, dataFieldPtr );
					}
					else {
						llvm::Value* dataFieldPtr = this->builder.CreateStructGEP( constructArgsStructType, constructArgsAlloca, 0, "cpack.args.data.field" );
						this->builder.CreateStore( llvm::ConstantPointerNull::get( llvm::cast<llvm::PointerType>( constructElemPtrType ) ), dataFieldPtr );
					}
					llvm::Value* countFieldPtr = this->builder.CreateStructGEP( constructArgsStructType, constructArgsAlloca, 1, "cpack.args.count.field" );
					this->builder.CreateStore( llvm::ConstantInt::get( int64Type, constructExtraCount ), countFieldPtr );
					llvm::Value* posFieldPtr = this->builder.CreateStructGEP( constructArgsStructType, constructArgsAlloca, 2, "cpack.args.pos.field" );
					this->builder.CreateStore( llvm::ConstantInt::get( int64Type, 0 ), posFieldPtr );
					constructFunctionArguments.push_back( constructArgsAlloca );
				}
			}
			for( size_t i = constructFunctionArguments.size(); i < constructFunction->second->arg_size(); i++ ) {
				llvm::Type* parameterType = constructFunction->second->getArg( i )->getType();
				constructFunctionArguments.push_back( llvm::Constant::getNullValue( parameterType ) );
			}
			this->createCallOrInvoke( constructFunction->second, constructFunctionArguments );
		}
		else {
			
			// Direct field initialization from construct arguments
			for( size_t i=0; i<expression.fields.size() && i<structType->getNumElements(); i++ ) {
				llvm::Value* expressionFieldValue = this->generateExpression( expression.fields[i].second );
				if( expressionFieldValue != nullptr ) {
					expressionFieldValue = this->generateImplicitCast( expressionFieldValue, structType->getElementType( i ) );
					std::string expressionValueName( fmt::format( "{}.field", resolvedName ) );
					llvm::Value* expressionFieldValueGEP = this->builder.CreateStructGEP( structType, objectPointer, i, expressionValueName );
					this->builder.CreateStore( expressionFieldValue, expressionFieldValueGEP );
				}
			}
		}
		
		// Initialize fields with default values from class/struct declaration
		semantic::TypeSharedPointer constructSemanticType = this->analyzer.types().lookupType( typeName );
		if( constructSemanticType && constructSemanticType->kind == semantic::Type::Kind::Class ) {
			semantic::ClassTypeSharedPointer constructClassType = std::static_pointer_cast<semantic::ClassType>( constructSemanticType );
			if( constructClassType->astDeclaration ) {
				unsigned int defaultFieldIndex = 0;
				bool hasVtable = constructClassType->virtualTable.empty() == false;
				if( hasVtable ) {
					defaultFieldIndex = 1;
				}
				for( std::shared_ptr<ast::nodes::FieldDeclarationNode>& fieldDeclaration : constructClassType->astDeclaration->fields ) {
					if( fieldDeclaration->defaultValue && defaultFieldIndex < structType->getNumElements() ) {
						llvm::Value* defaultFieldValue = this->generateExpression( fieldDeclaration->defaultValue );
						if( defaultFieldValue ) {
							defaultFieldValue = this->generateImplicitCast( defaultFieldValue, structType->getElementType( defaultFieldIndex ) );
							std::string defaultFieldName( fmt::format( "{}.default.{}", resolvedName, fieldDeclaration->name ) );
							llvm::Value* defaultFieldGEP = this->builder.CreateStructGEP( structType, objectPointer, defaultFieldIndex, defaultFieldName );
							this->builder.CreateStore( defaultFieldValue, defaultFieldGEP );
						}
					}
					defaultFieldIndex++;
				}
			}
		}
		if( this->isThrowableClass( typeName ) ) {
			if( expression.source ) {
				this->injectTracebackInfo( objectPointer, resolvedName, expression.source, false );
			}
			else {
				std::unordered_map<std::string, std::unordered_map<std::string, unsigned>>::iterator tbFieldMapIterator = this->structFieldIndices.find( resolvedName );
				if( tbFieldMapIterator != this->structFieldIndices.end() ) {
					std::unordered_map<std::string, unsigned>::iterator tbFieldIterator = tbFieldMapIterator->second.find( "traceback" );
					if( tbFieldIterator != tbFieldMapIterator->second.end() && tbFieldIterator->second < structType->getNumElements() ) {
						llvm::Value* tbNullGEP = this->builder.CreateStructGEP( structType, objectPointer, tbFieldIterator->second, "tb.null.ptr" );
						this->builder.CreateStore( llvm::ConstantPointerNull::get( llvm::cast<llvm::PointerType>( structType->getElementType( tbFieldIterator->second ) ) ), tbNullGEP );
					}
				}
			}
		}
		this->lastConstructedClassName = resolvedName;
		return objectPointer;
	}

	llvm::Value* LLVMCodegen::generateConstructArenaExpression( ast::nodes::ConstructExpression& expression ) {
		
		// Intrinsic: new Arena<T>(capacity) → malloc block + {T*, 0, cap} struct
		// For class/struct types, arena stores actual structs (pool allocator),
		// NOT pointers — alloc() returns T* into contiguous arena memory.
		llvm::Type* elementType = llvm::Type::getInt64Ty( this->context );
		if( expression.type->kind == ast::Node::Kind::GenericType ) {
			ast::nodes::GenericTypeNode& genericTypeNode = static_cast<ast::nodes::GenericTypeNode&>( *expression.type );
			if( genericTypeNode.typeArguments.empty() == false ) {
				elementType = this->resolveAstType( genericTypeNode.typeArguments[0] );
				elementType = unwrapStructPointer( elementType, genericTypeNode.typeArguments[0], this->structTypes );
			}
		}
		
		llvm::IntegerType* i64 = llvm::Type::getInt64Ty( this->context );
		llvm::Value* capValue = llvm::ConstantInt::get( i64, 1024 );
		if( expression.fields.empty() == false && expression.fields[0].second ) {
			capValue = this->generateExpression( expression.fields[0].second );
			if( capValue ) {
				capValue = this->generateImplicitCast( capValue, i64 );
			}
			else {
				capValue = llvm::ConstantInt::get( i64, 1024 );
			}
		}
		llvm::ConstantInt* elementSize = llvm::ConstantInt::get( i64, this->module->getDataLayout().getTypeAllocSize( elementType ) );
		llvm::Value* totalBytes = this->builder.CreateMul( capValue, elementSize, "arena.bytes" );
		llvm::CallInst* rawPointer = this->builder.CreateCall( this->getOrCreateMalloc(), { totalBytes }, "arena.raw" );
		llvm::Value* typedPointer = this->builder.CreateBitCast( rawPointer, llvm::PointerType::getUnqual( elementType ), "arena.typed" );
		
		// Build arena struct: {T*, i64 count, i64 capacity}
		llvm::StructType* arenaStructType = llvm::StructType::get( this->context, { llvm::PointerType::getUnqual( elementType ), i64, i64 } );
		llvm::AllocaInst* arenaAlloca = this->createEntryBlockAllocation( this->currentFunction, "arena.struct", arenaStructType );
		this->builder.CreateStore( typedPointer, this->builder.CreateStructGEP( arenaStructType, arenaAlloca, 0, "arena.base.ptr" ) );
		this->builder.CreateStore( llvm::ConstantInt::get( i64, 0 ), this->builder.CreateStructGEP( arenaStructType, arenaAlloca, 1, "arena.count.ptr" ) );
		this->builder.CreateStore( capValue, this->builder.CreateStructGEP( arenaStructType, arenaAlloca, 2, "arena.cap.ptr" ) );
		this->lastArenaElementType = elementType;
		
		return arenaAlloca;
	}
	
	llvm::Value* LLVMCodegen::generateConstructMemoryExpression( ast::nodes::ConstructExpression& expression ) {
		llvm::Type* elementType = llvm::Type::getInt64Ty( this->context );
		if( expression.type->kind == ast::Node::Kind::GenericType ) {
			ast::nodes::GenericTypeNode& genericTypeNode = static_cast<ast::nodes::GenericTypeNode&>( *expression.type );
			if( genericTypeNode.typeArguments.empty() == false ) {
				elementType = this->resolveAstType( genericTypeNode.typeArguments[0] );
			}
		}
		if( elementType->isPointerTy() && expression.type->kind == ast::Node::Kind::GenericType ) {
			ast::nodes::GenericTypeNode& gtn = static_cast<ast::nodes::GenericTypeNode&>( *expression.type );
			if( gtn.typeArguments.empty() == false ) {
				elementType = unwrapStructPointer( elementType, gtn.typeArguments[0], this->structTypes );
			}
		}
		llvm::IntegerType* i64 = llvm::Type::getInt64Ty( this->context );
		llvm::Value* capValue = llvm::ConstantInt::get( i64, 16 );
		if( expression.fields.empty() == false && expression.fields[0].second ) {
			capValue = this->generateExpression( expression.fields[0].second );
			if( capValue ) {
				capValue = this->generateImplicitCast( capValue, i64 );
			}
			else {
				capValue = llvm::ConstantInt::get( i64, 16 );
			}
		}
		llvm::ConstantInt* elementSize = llvm::ConstantInt::get( i64, this->module->getDataLayout().getTypeAllocSize( elementType ) );
		llvm::CallInst* rawPointer = this->builder.CreateCall( this->getOrCreateCalloc(), { capValue, elementSize }, "mem.raw" );
		llvm::Value* typedPointer = this->builder.CreateBitCast( rawPointer, llvm::PointerType::getUnqual( elementType ), "mem.typed" );
		this->lastMemoryElementType = elementType;
		return typedPointer;
	}
	
	void LLVMCodegen::generateDeclaration( const ast::nodes::DeclarationSharedPointer& declaration ) {
		if( declaration->isBuiltin ) return;
		switch( declaration->kind ) {
			case ast::Node::Kind::FunctionDeclaration:
				this->generateFunctionDeclaration( static_cast<ast::nodes::FunctionDeclaration&>( *declaration ) );
				break;
			case ast::Node::Kind::ClassDeclaration:
				this->generateClassDeclaration( static_cast<ast::nodes::ClassDeclaration&>( *declaration ) );
				break;
			case ast::Node::Kind::StructDeclaration:
				this->generateStructDeclaration( static_cast<ast::nodes::StructDeclaration&>( *declaration ) );
				break;
			case ast::Node::Kind::EnumDeclaration:
				this->generateEnumDeclaration( static_cast<ast::nodes::EnumDeclaration&>( *declaration ) );
				break;
			case ast::Node::Kind::ExternDeclaration:
				this->generateExternDeclaration( static_cast<ast::nodes::ExternDeclaration&>( *declaration ) );
				break;
			case ast::Node::Kind::ConstantDeclaration: {
				ast::nodes::ConstantDeclaration& constDecl = static_cast<ast::nodes::ConstantDeclaration&>( *declaration );
				bool treatAsGlobalVariable = constDecl.isGlobalVariable;
				if( treatAsGlobalVariable == false && constDecl.initializer ) {
					ast::Node::Kind initKind = constDecl.initializer->kind;
					if( initKind != ast::Node::Kind::IntegerLiteral &&
						initKind != ast::Node::Kind::FloatLiteral &&
						initKind != ast::Node::Kind::BooleanLiteral &&
						initKind != ast::Node::Kind::StringLiteral &&
						initKind != ast::Node::Kind::NoneLiteral ) {
						treatAsGlobalVariable = true;
					}
				}
				if( treatAsGlobalVariable ) {
					llvm::Type* globalType = constDecl.type ? this->resolveAstType( constDecl.type ) : llvm::PointerType::getUnqual( this->context );
					if( globalType->isStructTy() ) {
						globalType = globalType->getPointerTo();
					}
					llvm::GlobalVariable* globalVar = new llvm::GlobalVariable(
						*this->module,
						globalType,
						false,
						llvm::GlobalValue::InternalLinkage,
						llvm::Constant::getNullValue( globalType ),
						constDecl.name
					);
					this->namedValues[constDecl.name] = globalVar;
					if( constDecl.initializer ) {
						this->globalVariableInits.push_back( &constDecl );
					}
				}
				else {
					llvm::Type* constLLVMType = constDecl.type ? this->resolveAstType( constDecl.type ) : llvm::Type::getInt64Ty( this->context );
					llvm::Constant* constValue = llvm::Constant::getNullValue( constLLVMType );
					if( constDecl.initializer ) {
						if( constDecl.initializer->kind == ast::Node::Kind::IntegerLiteral ) {
							int64_t intVal = static_cast<ast::nodes::IntegerLiteralExpression&>( *constDecl.initializer ).value;
							constValue = llvm::ConstantInt::get( constLLVMType, intVal, true );
						}
						else if( constDecl.initializer->kind == ast::Node::Kind::FloatLiteral ) {
							double floatVal = static_cast<ast::nodes::FloatLiteralExpression&>( *constDecl.initializer ).value;
							constValue = llvm::ConstantFP::get( constLLVMType, floatVal );
						}
						else if( constDecl.initializer->kind == ast::Node::Kind::BooleanLiteral ) {
							bool boolVal = static_cast<ast::nodes::BoolLiteralExpression&>( *constDecl.initializer ).value;
							constValue = llvm::ConstantInt::get( constLLVMType, boolVal ? 1 : 0 );
						}
						else if( constDecl.initializer->kind == ast::Node::Kind::StringLiteral ) {
							std::string strVal = static_cast<ast::nodes::StringLiteralExpression&>( *constDecl.initializer ).value;
							llvm::Constant* strArray = llvm::ConstantDataArray::getString( this->context, strVal, true );
							llvm::GlobalVariable* strGlobal = new llvm::GlobalVariable(
								*this->module, strArray->getType(), true,
								llvm::GlobalValue::PrivateLinkage, strArray,
								constDecl.name + ".str"
							);
							strGlobal->setUnnamedAddr( llvm::GlobalValue::UnnamedAddr::Global );
							constValue = llvm::ConstantExpr::getBitCast(
								llvm::ConstantExpr::getInBoundsGetElementPtr(
									strArray->getType(), strGlobal,
									llvm::ArrayRef<llvm::Constant*>({
										llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->context ), 0 ),
										llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->context ), 0 )
									})
								),
								llvm::PointerType::getUnqual( this->context )
							);
						}
					}
					this->constantValues[constDecl.name] = constValue;
				}
				break;
			}
			default: break;
		}
	}
	
	void LLVMCodegen::generateEnumDeclaration( ast::nodes::EnumDeclaration& declaration ) {
		
		// Enums are represented as tagged unions
		semantic::EnumTypeSharedPointer enumType = std::dynamic_pointer_cast<semantic::EnumType>( this->analyzer.types().lookupType( declaration.name ) );
		
		this->enumTypeNames.insert( declaration.name );
		
		// Create struct: { i32 tag, [max_payload_size x i8] }
		int enumMaxPayloadSize = 0;
		for( semantic::EnumVariantInfo& enumVariant : enumType->variants ) {
			int enumPayloadSize = 0;
			for( semantic::TypeSharedPointer& enumVariantAssociatedType : enumVariant.associatedTypes ) {
				enumPayloadSize+= this->module->getDataLayout().getTypeAllocSize( this->toLLVMType( enumVariantAssociatedType ) );
			}
			enumMaxPayloadSize = std::max( enumMaxPayloadSize, enumPayloadSize );
		}
		
		llvm::IntegerType* enumTagType = llvm::Type::getInt32Ty( this->context );
		std::vector<llvm::Type*> enumMembers = { enumTagType };
		if( enumMaxPayloadSize > 0) {
			enumMembers.push_back(
				llvm::ArrayType::get(
					llvm::Type::getInt8Ty( this->context ),
					enumMaxPayloadSize
				)
			);
		}
		
		llvm::StructType* enumStructType = llvm::StructType::create( this->context, enumMembers, declaration.name );
		this->structTypes[declaration.name] = enumStructType;
		
		// Register variant discriminants and backed values
		for( size_t vi=0; vi<enumType->variants.size() && vi < declaration.variants.size(); vi++ ) {
			semantic::EnumVariantInfo& enumVariant = enumType->variants[vi];
			ast::nodes::EnumVariantSharedPointer& enumASTVariant = declaration.variants[vi];
			std::string enumVariantKey( fmt::format( "{}::{}", declaration.name, enumVariant.name ) );
			this->enumVariants[enumVariantKey] = enumVariant.discriminant;
			if( enumASTVariant->backedValue ) {
				ast::nodes::ExpressionSharedPointer& enumBackedValue = enumASTVariant->backedValue;
				if( enumBackedValue->kind == ast::Node::Kind::IntegerLiteral ) {
					ast::nodes::IntegerLiteralExpression& enumBackedValueLiteral = static_cast<ast::nodes::IntegerLiteralExpression&>( *enumBackedValue );
					llvm::Type* enumBackedIntType = llvm::Type::getInt32Ty( this->context );
					if( enumType && enumType->backedType ) {
						llvm::Type* resolvedBackedType = this->toLLVMType( enumType->backedType );
						if( resolvedBackedType->isIntegerTy() ) {
							enumBackedIntType = resolvedBackedType;
						}
					}
					this->enumBackedValues[enumVariantKey] = llvm::ConstantInt::get( enumBackedIntType, enumBackedValueLiteral.value );
				}
				else if( enumBackedValue->kind == ast::Node::Kind::StringLiteral ) {
					std::string enumBackedValueName( fmt::format( "enum.{}.{}", declaration.name, enumVariant.name  ) );
					ast::nodes::StringLiteralExpression& enumBackedValueLiteral = static_cast<ast::nodes::StringLiteralExpression&>( *enumBackedValue );
					llvm::Constant* enumBackedValueStringConstant = llvm::ConstantDataArray::getString( this->context, enumBackedValueLiteral.value, true );
					llvm::GlobalVariable* enumBackedValueGlobalVariable = new llvm::GlobalVariable( *this->getModule(), enumBackedValueStringConstant->getType(), true, llvm::GlobalValue::PrivateLinkage, enumBackedValueStringConstant, enumBackedValueName );
					this->enumBackedValues[enumVariantKey] = llvm::ConstantExpr::getBitCast( enumBackedValueGlobalVariable, llvm::PointerType::getUnqual( this->context ) );
				}
				else if( enumBackedValue->kind == ast::Node::Kind::FloatLiteral ) {
					ast::nodes::FloatLiteralExpression& enumBackedValueLiteral = static_cast<ast::nodes::FloatLiteralExpression&>( *enumBackedValue );
					this->enumBackedValues[enumVariantKey] = llvm::ConstantFP::get( llvm::Type::getDoubleTy( this->context ), enumBackedValueLiteral.value );
				}
				else if( enumBackedValue->kind == ast::Node::Kind::BooleanLiteral ) {
					ast::nodes::BoolLiteralExpression& enumBackedValueLiteral = static_cast<ast::nodes::BoolLiteralExpression&>( *enumBackedValue );
					this->enumBackedValues[enumVariantKey] = llvm::ConstantInt::get( llvm::Type::getInt1Ty( this->context ), enumBackedValueLiteral.value ? 1 : 0 );
				}
			}
		}
		
		// Generate enum methods
		llvm::IntegerType* enumMethodSelfType = llvm::Type::getInt32Ty( this->context );
		
		// Collect per-variant overrides
		std::unordered_map<std::string,std::vector<std::pair<int, ast::nodes::FunctionDeclaration*>>> enumVariantOverrides;
		for( size_t vi=0; vi<declaration.variants.size(); vi++ ) {
			ast::nodes::EnumVariantSharedPointer& enumASTVariant = declaration.variants[vi];
			for( ast::nodes::DeclarationSharedPointer& enumMethodDeclaration : enumASTVariant->methods ) {
				if( enumMethodDeclaration->kind == ast::Node::Kind::FunctionDeclaration ) {
					ast::nodes::FunctionDeclaration& enumMethodFunctionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *enumMethodDeclaration );
					enumVariantOverrides[enumMethodFunctionDeclaration.name].push_back( { ( int ) vi, &enumMethodFunctionDeclaration } );
				}
			}
		}
		
		std::function<void(ast::nodes::FunctionDeclaration&, const std::string&)> generateEnumMethod = [&]( ast::nodes::FunctionDeclaration& functionDeclaration, const std::string& functionName ) {
			llvm::Type* functionReturnType = this->resolveAstType( functionDeclaration.returnType );
			if( functionReturnType == nullptr ) {
				functionReturnType = llvm::Type::getVoidTy( this->context );
			}
			std::vector<llvm::Type*> functionParameterTypes = { enumMethodSelfType };
			for( ast::nodes::FunctionParameterSharedPointer& functionParameter : functionDeclaration.parameters ) {
				if( functionParameter->isSelf ) continue;
				if( functionParameter->type ) {
					llvm::Type* functionParameterTypeResolvedLLVM = llvm::Type::getInt64Ty( this->context );
					semantic::TypeSharedPointer functionParameterTypeResolved = this->analyzer.types().lookupType( static_cast<ast::nodes::SimpleTypeNode&>( *functionParameter->type ).name );
					if( functionParameterTypeResolved ) {
						functionParameterTypeResolvedLLVM = this->toLLVMType( functionParameterTypeResolved );
					}
					functionParameterTypes.push_back( functionParameterTypeResolvedLLVM );
				}
			}
			llvm::FunctionType* functionType = llvm::FunctionType::get( functionReturnType, functionParameterTypes, false );
			llvm::Function* function = llvm::Function::Create(functionType, llvm::Function::InternalLinkage, functionName, this->getModule() );
			function->setPersonalityFn( this->getOrCreatePersonalityFunction() );
			this->functions[functionName] = function;
			llvm::BasicBlock* functionBasicBlock = llvm::BasicBlock::Create( this->context, "entry", function );
			llvm::BasicBlock* functionSavedBlock = this->builder.GetInsertBlock();
			llvm::Function* functionSaved = this->currentFunction;
			std::unordered_map<std::string,llvm::Value*> functionSavedValues = this->namedValues;
			this->builder.SetInsertPoint( functionBasicBlock );
			this->currentFunction = function;
			this->namedValues.clear();
			this->variableStructType.clear();
			this->varMemoryElementTypes.clear();
			this->arenaElementTypes.clear();
			unsigned functionArgumentIndex = 0;
			for( llvm::Argument& functionArgument : function->args() ) {
				if( functionArgumentIndex == 0 ) {
					functionArgument.setName( "self" );
					llvm::AllocaInst* functionSelfAlloca = this->createEntryBlockAllocation( function, "self", enumMethodSelfType );
					this->builder.CreateStore( &functionArgument, functionSelfAlloca );
					this->namedValues["self"] = functionSelfAlloca;
				}
				else {
					size_t functionParameterIndex = 0;
					for( ast::nodes::FunctionParameterSharedPointer& functionParameter : functionDeclaration.parameters ) {
						if( functionParameter->isSelf ) continue;
						if( functionParameterIndex == functionArgumentIndex - 1) {
							functionArgument.setName( functionParameter->name );
							llvm::AllocaInst* functionParameterAlloca = this->createEntryBlockAllocation( function, functionParameter->name, functionArgument.getType() );
							this->builder.CreateStore( &functionArgument, functionParameterAlloca );
							this->namedValues[functionParameter->name] = functionParameterAlloca;
							break;
						}
						functionParameterIndex++;
					}
				}
				functionArgumentIndex++;
			}
			
			for( ast::nodes::StatementSharedPointer& functionBodyStatement : functionDeclaration.body ) {
				this->generateStatement( functionBodyStatement );
			}
			
			if( this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
				if( functionReturnType->isVoidTy() ) {
					this->builder.CreateRetVoid();
				}
				else {
					this->builder.CreateRet( llvm::Constant::getNullValue( functionReturnType ) );
				}
			}
			
			this->namedValues = functionSavedValues;
			this->currentFunction = functionSaved;
			if( functionSavedBlock ) {
				this->builder.SetInsertPoint( functionSavedBlock );
			}
		};
		
		// Generate shared enum methods
		for( ast::nodes::DeclarationSharedPointer& enumMethod : declaration.methods ) {
			if( enumMethod->kind != ast::Node::Kind::FunctionDeclaration ) {
				continue;
			}
			ast::nodes::FunctionDeclaration& enumMethodFunctionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *enumMethod );
			std::string enumMethodFunctionName( fmt::format( "{}::{}", declaration.name, enumMethodFunctionDeclaration.name ) );
			std::unordered_map<std::string,std::vector<std::pair<int,ast::nodes::FunctionDeclaration*>>>::iterator enumMethodFunctionOverrideIterator = enumVariantOverrides.find( enumMethodFunctionDeclaration.name );
			if( enumMethodFunctionOverrideIterator != enumVariantOverrides.end() ) {
				
				// Has overrides — generate base + variant versions, then dispatch function
				std::string enumMethodFunctionNameWithBase( fmt::format( "{}.base", enumMethodFunctionName ) );
				generateEnumMethod( enumMethodFunctionDeclaration, enumMethodFunctionNameWithBase );
				for( std::pair<int,ast::nodes::FunctionDeclaration*> pair : enumMethodFunctionOverrideIterator->second ) {
					generateEnumMethod( *pair.second, fmt::format( "{}.v{}", enumMethodFunctionName, pair.first ) );
				}
				
				// Generate dispatch function
				llvm::Function* enumMethodFunctionBase = this->functions[enumMethodFunctionNameWithBase];
				llvm::FunctionType* enumMethodFunctionType = enumMethodFunctionBase->getFunctionType();
				llvm::Function* enumMethodFunctionDispatch = llvm::Function::Create( enumMethodFunctionType, llvm::Function::InternalLinkage, enumMethodFunctionNameWithBase, this->getModule() );
				enumMethodFunctionDispatch->setPersonalityFn( this->getOrCreatePersonalityFunction() );
				this->functions[enumMethodFunctionName] = enumMethodFunctionDispatch;
				llvm::BasicBlock* enumMethodFunctionEntryBasicBlock = llvm::BasicBlock::Create( this->context, "entry", enumMethodFunctionDispatch );
				llvm::BasicBlock* enumMethodFunctionSavedBlock = this->builder.GetInsertBlock();
				this->builder.SetInsertPoint( enumMethodFunctionEntryBasicBlock );
				llvm::Function::arg_iterator enumSelfArgumentIterator = enumMethodFunctionDispatch->arg_begin();
				llvm::BasicBlock* enumMethodFunctionDefaultBasicBlock = llvm::BasicBlock::Create( this->context, "default", enumMethodFunctionDispatch );
				llvm::SwitchInst* switchInst = this->builder.CreateSwitch(
					enumSelfArgumentIterator,
					enumMethodFunctionDefaultBasicBlock,
					enumMethodFunctionOverrideIterator->second.size()
				);
				
				for( std::pair<int,ast::nodes::FunctionDeclaration*> pair : enumMethodFunctionOverrideIterator->second ) {
					std::string enumMethodFunctionCaseName( fmt::format( "case.{}", pair.first ) );
					llvm::BasicBlock* enumMethodFunctionCaseBasicBlock = llvm::BasicBlock::Create( this->context, enumMethodFunctionCaseName, enumMethodFunctionDispatch );
					switchInst->addCase( llvm::ConstantInt::get( enumMethodSelfType, pair.first ), enumMethodFunctionCaseBasicBlock );
					this->builder.SetInsertPoint( enumMethodFunctionCaseBasicBlock );
					std::vector<llvm::Value*> enumMethodFunctionArguments;
					for( llvm::Argument& enumMethodFunctionArgument : enumMethodFunctionDispatch->args() ) {
						enumMethodFunctionArguments.push_back( &enumMethodFunctionArgument );
					}
					llvm::Function* enumMethodFunctionVariable = this->functions[fmt::format( "{}.v{}", enumMethodFunctionName, pair.first )];
					llvm::CallInst* enumMethodFunctionVariableResult = this->builder.CreateCall( enumMethodFunctionVariable, enumMethodFunctionArguments );
					if( enumMethodFunctionType->getReturnType()->isVoidTy() ) {
						this->builder.CreateRetVoid();
					}
					else this->builder.CreateRet( enumMethodFunctionVariableResult );
				}
				
				this->builder.SetInsertPoint( enumMethodFunctionDefaultBasicBlock );
				
				std::vector<llvm::Value*> enumMethodFunctionArguments;
				for( llvm::Argument& enumMethodFunctionArgument : enumMethodFunctionDispatch->args() ) {
					enumMethodFunctionArguments.push_back( &enumMethodFunctionArgument );
				}
				llvm::CallInst* enumMethodFunctionResult = this->builder.CreateCall( enumMethodFunctionBase, enumMethodFunctionArguments );
				if( enumMethodFunctionType->getReturnType()->isVoidTy() ) {
					this->builder.CreateRetVoid();
				}
				else {
					this->builder.CreateRet( enumMethodFunctionResult );
				}
				if( enumMethodFunctionSavedBlock ) {
					this->builder.SetInsertPoint( enumMethodFunctionSavedBlock );
				}
			}
			else {
				generateEnumMethod(enumMethodFunctionDeclaration, enumMethodFunctionName );
			}
		}
		
		// Generate variant-only methods (no shared base)
		for( std::pair<std::string,std::vector<std::pair<int, ast::nodes::FunctionDeclaration*>>> pair : enumVariantOverrides ) {
			bool hasShared = false;
			for( ast::nodes::DeclarationSharedPointer& enumMethodDeclaration : declaration.methods ) {
				if( enumMethodDeclaration->kind == ast::Node::Kind::FunctionDeclaration && static_cast<ast::nodes::FunctionDeclaration&>( *enumMethodDeclaration ).name == pair.first ) {
					hasShared = true;
					break;
				}
			}
			if( hasShared == false ) {
				for( std::pair<int, ast::nodes::FunctionDeclaration*>& pairNonShared : pair.second ) {
					generateEnumMethod( *pairNonShared.second, fmt::format( "{}::{}.v{}", declaration.name, pair.first, pairNonShared.first ) );
				}
			}
		}
		if( enumType ) {
			std::string metaGlobalName( fmt::format( "_AE_meta_{}", declaration.name ) );
			if( this->getModule()->getGlobalVariable( metaGlobalName, true ) == nullptr ) {
				std::string qualname = enumType->package.empty() ? declaration.name : fmt::format( "{}.{}", enumType->package, declaration.name );
				std::string metaValue( fmt::format( "{} enum", qualname ) );
				llvm::Constant* metaConstant = llvm::ConstantDataArray::getString( this->context, metaValue, true );
				new llvm::GlobalVariable( *this->getModule(), metaConstant->getType(), true, llvm::GlobalValue::PrivateLinkage, metaConstant, metaGlobalName );
			}
		}
	}
	
	llvm::Value* LLVMCodegen::generateExpression( const ast::nodes::ExpressionSharedPointer& expression ) {
		if( expression == nullptr ) {
			return nullptr;
		}
		switch( expression->kind ) {
			case ast::Node::Kind::ArrayExpression: {
				ast::nodes::ArrayExpression& expressionArray = static_cast<ast::nodes::ArrayExpression&>( *expression );
				llvm::IntegerType* expressionI64Type = llvm::Type::getInt64Ty( this->context );
				if( expressionArray.elements.empty() ) {
					this->lastMemoryElementType = expressionI64Type;
					llvm::ConstantInt* exprEmptySize = llvm::ConstantInt::get( expressionI64Type, 0 );
					return this->builder.CreateCall( this->getOrCreateMalloc(), {exprEmptySize}, "arr.empty" );
				}
				llvm::Value* expressionFirstElement = this->generateExpression( expressionArray.elements[0] );
				if( expressionFirstElement == nullptr ) {
					return nullptr;
				}
				llvm::Type* exprElementType = expressionFirstElement->getType();
				this->lastMemoryElementType = exprElementType;
				uint64_t exprElementSize = this->module->getDataLayout().getTypeAllocSize( exprElementType );
				llvm::ConstantInt* expressionTotalBytes = llvm::ConstantInt::get( expressionI64Type, exprElementSize * expressionArray.elements.size() );
				llvm::Value* expressionRawPointer = this->builder.CreateCall( this->getOrCreateMalloc(), {expressionTotalBytes}, "arr.raw" );
				llvm::Value* expressionTypedArrayPointer = this->builder.CreateBitCast( expressionRawPointer, llvm::PointerType::getUnqual( exprElementType ), "arr.typed" );
				for( size_t expressionIndex = 0; expressionIndex < expressionArray.elements.size(); expressionIndex++ ) {
					llvm::Value* expressionValue = ( expressionIndex == 0 ) ? expressionFirstElement : this->generateExpression( expressionArray.elements[expressionIndex] );
					if( expressionValue != nullptr ) {
						expressionValue = this->generateImplicitCast( expressionValue, exprElementType );
						this->builder.CreateStore(
							expressionValue,
							this->builder.CreateGEP(
								exprElementType,
								expressionTypedArrayPointer,
								llvm::ConstantInt::get(
									expressionI64Type,
									expressionIndex
								),
								"arr.gep"
							)
						);
					}
				}
				return expressionTypedArrayPointer;
			}
			case ast::Node::Kind::AwaitExpression: {
				ast::nodes::AwaitExpression& expressionAwait = static_cast<ast::nodes::AwaitExpression&>( *expression );
				llvm::Value* expressionAwaitValue = this->generateExpression( expressionAwait.operand );
				if( expressionAwaitValue != nullptr ) {
					if( expressionAwaitValue->getType()->isIntegerTy( 64 ) ) {
						llvm::Function* expressionAwaitFn = this->module->getFunction( "runtimeAwait" );
						if( expressionAwaitFn == nullptr ) {
							expressionAwaitFn = this->module->getFunction( "uraniteAwaitTask" );
							if( expressionAwaitFn == nullptr ) {
								llvm::FunctionType* expressionAwaitSig = llvm::FunctionType::get( llvm::Type::getInt64Ty( this->context ), {llvm::Type::getInt64Ty( this->context )}, false );
								expressionAwaitFn = llvm::Function::Create( expressionAwaitSig, llvm::Function::ExternalLinkage, "uraniteAwaitTask", this->getModule() );
							}
						}
						llvm::Value* expressionTaskIdAlloca = this->createEntryBlockAllocation( this->currentFunction, "await.taskid", llvm::Type::getInt64Ty( this->context ) );
						this->builder.CreateStore( expressionAwaitValue, expressionTaskIdAlloca );
						llvm::Value* expressionAwaitResult = this->builder.CreateCall( expressionAwaitFn, { expressionAwaitValue }, "await.result" );
						bool expressionHasErrReturnsI1 = false;
						llvm::Function* expressionHasErrFn = this->module->getFunction( "runtimeTaskHasError" );
						if( expressionHasErrFn != nullptr ) {
							expressionHasErrReturnsI1 = true;
						}
						else {
							expressionHasErrFn = this->module->getFunction( "uraniteTaskHasError" );
							if( expressionHasErrFn == nullptr ) {
								llvm::FunctionType* expressionHasErrSig = llvm::FunctionType::get( llvm::Type::getInt32Ty( this->context ), {llvm::Type::getInt64Ty( this->context )}, false );
								expressionHasErrFn = llvm::Function::Create( expressionHasErrSig, llvm::Function::ExternalLinkage, "uraniteTaskHasError", this->getModule() );
							}
						}
						llvm::Value* expressionTaskIdReload = this->builder.CreateLoad( llvm::Type::getInt64Ty( this->context ), expressionTaskIdAlloca );
						llvm::Value* expressionHasErrCall = this->builder.CreateCall( expressionHasErrFn, {expressionTaskIdReload}, "await.haserr" );
						llvm::Value* expressionIsErr;
						if( expressionHasErrReturnsI1 ) {
							expressionIsErr = expressionHasErrCall;
						}
						else {
							expressionIsErr = this->builder.CreateICmpNE( expressionHasErrCall, llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->context ), 0 ), "await.iserr" );
						}
						llvm::BasicBlock* expressionAwaitOkBasicBlock = llvm::BasicBlock::Create( this->context, "await.ok", this->currentFunction );
						llvm::BasicBlock* expressionAwaitErrBasicBlock = llvm::BasicBlock::Create( this->context, "await.err", this->currentFunction );
						this->builder.CreateCondBr( expressionIsErr, expressionAwaitErrBasicBlock, expressionAwaitOkBasicBlock );
						this->builder.SetInsertPoint( expressionAwaitErrBasicBlock );
						llvm::Function* expressionGetErrFn = this->module->getFunction( "runtimeTaskGetError" );
						if( expressionGetErrFn == nullptr ) {
							expressionGetErrFn = this->module->getFunction( "uraniteTaskGetError" );
							if( expressionGetErrFn == nullptr ) {
								llvm::FunctionType* expressionGetErrSig = llvm::FunctionType::get( llvm::PointerType::getUnqual( this->context ), {llvm::Type::getInt64Ty( this->context )}, false );
								expressionGetErrFn = llvm::Function::Create( expressionGetErrSig, llvm::Function::ExternalLinkage, "uraniteTaskGetError", this->getModule() );
							}
						}
						llvm::Value* expressionTaskIdReload2 = this->builder.CreateLoad( llvm::Type::getInt64Ty( this->context ), expressionTaskIdAlloca );
						llvm::Value* exprErrMsg = this->builder.CreateCall( expressionGetErrFn, {expressionTaskIdReload2}, "await.errmsg" );
						this->ensureClassMethodsRegistered( "Exception" );
						llvm::StructType* awaitExcStructType = nullptr;
						std::unordered_map<std::string, llvm::StructType*>::iterator awaitExcStructIter = this->structTypes.find( "Exception" );
						if( awaitExcStructIter != this->structTypes.end() ) {
							awaitExcStructType = awaitExcStructIter->second;
						}
						if( awaitExcStructType != nullptr ) {
							llvm::DataLayout awaitExcLayout = this->module->getDataLayout();
							uint64_t awaitExcSize = awaitExcLayout.getTypeAllocSize( awaitExcStructType );
							llvm::Function* awaitExcMalloc = this->getOrCreateMalloc();
							llvm::Value* awaitExcRaw = this->builder.CreateCall( awaitExcMalloc, { llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->context ), awaitExcSize ) }, "await.exc.raw" );
							llvm::Value* awaitExcObj = this->builder.CreateBitCast( awaitExcRaw, llvm::PointerType::getUnqual( awaitExcStructType ), "await.exc.obj" );
							std::string awaitExcCtorKey( "Exception::Exception" );
							std::unordered_map<std::string, llvm::Function*>::iterator awaitExcCtorIter = this->functions.find( awaitExcCtorKey );
							if( awaitExcCtorIter != this->functions.end() && awaitExcCtorIter->second != nullptr ) {
								llvm::Function* awaitExcCtor = awaitExcCtorIter->second;
								std::vector<llvm::Value*> awaitExcCtorArgs;
								awaitExcCtorArgs.push_back( awaitExcObj );
								awaitExcCtorArgs.push_back( exprErrMsg );
								awaitExcCtorArgs.push_back( llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->context ), 0 ) );
								for( size_t awaitExcArgIdx = awaitExcCtorArgs.size(); awaitExcArgIdx < awaitExcCtor->arg_size(); awaitExcArgIdx++ ) {
									awaitExcCtorArgs.push_back( llvm::Constant::getNullValue( awaitExcCtor->getArg( awaitExcArgIdx )->getType() ) );
								}
								this->builder.CreateCall( awaitExcCtor, awaitExcCtorArgs );
							}
							llvm::Value* awaitExcCasted = this->builder.CreateBitCast( awaitExcObj, llvm::PointerType::getUnqual( this->context ), "await.exc.cast" );
							llvm::Value* awaitExcTypeName = this->builder.CreateGlobalStringPtr( "Exception", "await.exc.typename" );
							llvm::Function* uraniteThrowFn = this->getOrCreateUraniteThrow();
							if( this->landingPads.empty() ) {
								this->builder.CreateCall( uraniteThrowFn, { awaitExcCasted, awaitExcTypeName } );
								this->builder.CreateUnreachable();
							}
							else {
								llvm::BasicBlock* awaitThrowUnreachable = llvm::BasicBlock::Create( this->context, "await.throw.unreachable", this->currentFunction );
								this->builder.CreateInvoke( uraniteThrowFn, awaitThrowUnreachable, this->landingPads.top(), { awaitExcCasted, awaitExcTypeName } );
								this->builder.SetInsertPoint( awaitThrowUnreachable );
								this->builder.CreateUnreachable();
							}
						}
						this->builder.SetInsertPoint( expressionAwaitOkBasicBlock );
						return expressionAwaitResult;
					}
					return expressionAwaitValue;
				}
				return nullptr;
			}
			case ast::Node::Kind::BinaryExpression:
				return this->generateBinaryExpression( static_cast<ast::nodes::BinaryExpression&>( *expression ) );
			case ast::Node::Kind::BooleanLiteral: {
				ast::nodes::BoolLiteralExpression& expressionBool = static_cast<ast::nodes::BoolLiteralExpression&>( *expression );
				return llvm::ConstantInt::get( llvm::Type::getInt1Ty( this->context ), expressionBool.value ? 1 : 0 );
			}
			case ast::Node::Kind::CallExpression:
				return this->generateCallExpression( static_cast<ast::nodes::CallExpression&>( *expression ) );
			case ast::Node::Kind::CastExpression: {
				ast::nodes::CastExpression& expressionCast = static_cast<ast::nodes::CastExpression&>( *expression );
				llvm::Value* expressionCastValue = this->generateExpression( expressionCast.expression );
				if( expressionCastValue != nullptr ) {
					llvm::Type* expressionTargetType = this->resolveAstType( expressionCast.targetType );
					if( expressionTargetType != nullptr ) {
						if( expressionTargetType == this->getInterfaceFatPointerType() && expressionCastValue->getType()->isPointerTy() ) {
							std::string castClassName = this->resolveStructTypeName( expressionCast.expression );
							std::string castBaseInterfaceName;
							if( expressionCast.targetType->kind == ast::Node::Kind::SimpleType ) {
								castBaseInterfaceName = static_cast<ast::nodes::SimpleTypeNode&>( *expressionCast.targetType ).name;
							}
							else if( expressionCast.targetType->kind == ast::Node::Kind::GenericType ) {
								castBaseInterfaceName = static_cast<ast::nodes::GenericTypeNode&>( *expressionCast.targetType ).name;
							}
							if( castClassName.empty() == false && castBaseInterfaceName.empty() == false ) {
								std::string castExactKey( fmt::format( "{}::{}", castClassName, castBaseInterfaceName ) );
								std::unordered_map<std::string, llvm::GlobalVariable*>::iterator castExactIt = this->interfaceTables.find( castExactKey );
								if( castExactIt != this->interfaceTables.end() ) {
									return this->createInterfaceFatPointer( expressionCastValue, castClassName, castBaseInterfaceName );
								}
								std::string castPrefixKey( fmt::format( "{}::{}<", castClassName, castBaseInterfaceName ) );
								for( std::pair<const std::string, llvm::GlobalVariable*>& castItableEntry : this->interfaceTables ) {
									if( castItableEntry.first.find( castPrefixKey ) == 0 ) {
										std::string castMatchedInterfaceName( castItableEntry.first.substr( castClassName.size() + 2 ) );
										return this->createInterfaceFatPointer( expressionCastValue, castClassName, castMatchedInterfaceName );
									}
								}
							}
							this->lastTargetInterfaceName = castBaseInterfaceName;
						}
						if( expressionTargetType->isStructTy() && expressionCastValue->getType()->isPointerTy() ) {
							llvm::PointerType* expressionTargetPointer = llvm::PointerType::getUnqual( expressionTargetType );
							return this->builder.CreateBitCast( expressionCastValue, expressionTargetPointer, "cast.ptr" );
						}
						bool castIsUnsigned = false;
						std::string sourceTypeName = this->resolveStructTypeName( expressionCast.expression );
						if( sourceTypeName == "U8" || sourceTypeName == "U16" || sourceTypeName == "U32" || sourceTypeName == "U64" ||
							sourceTypeName == "UInt" || sourceTypeName == "Byte" || sourceTypeName == "Char" ) {
							castIsUnsigned = true;
						}
						return this->generateImplicitCast( expressionCastValue, expressionTargetType, castIsUnsigned );
					}
					return expressionCastValue;
				}
				return nullptr;
			}
			case ast::Node::Kind::CharLiteral: {
				ast::nodes::CharLiteralExpression& expressionChar = static_cast<ast::nodes::CharLiteralExpression&>( *expression );
				return llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->context ), expressionChar.value );
			}
			case ast::Node::Kind::ComprehensionExpression: {
				ast::nodes::ComprehensionExpression& expressionComp = static_cast<ast::nodes::ComprehensionExpression&>( *expression );
				if( expressionComp.iterable && expressionComp.iterable->kind == ast::Node::Kind::RangeExpression ) {
					ast::nodes::RangeExpression& expressionRange = static_cast<ast::nodes::RangeExpression&>( *expressionComp.iterable );
					llvm::Value* expressionStart = this->generateExpression( expressionRange.start );
					llvm::Value* exprEnd = this->generateExpression( expressionRange.end );
					if( expressionStart == nullptr || exprEnd == nullptr ) {
						return nullptr;
					}
					llvm::IntegerType* expressionI64 = llvm::Type::getInt64Ty( this->context );
					expressionStart = this->generateImplicitCast( expressionStart, expressionI64 );
					exprEnd = this->generateImplicitCast( exprEnd, expressionI64 );
					
					llvm::Value* expressionSize = this->builder.CreateSub( exprEnd, expressionStart, "comp.size" );
					if( expressionRange.inclusive ) {
						expressionSize = this->builder.CreateAdd( expressionSize, llvm::ConstantInt::get( expressionI64, 1 ), "comp.size.inc" );
					}
					llvm::Type* expressionCompElemType = expressionI64;
					if( expressionComp.semanticType && expressionComp.semanticType->kind == semantic::Type::Kind::Array ) {
						semantic::ArrayTypeSharedPointer expressionSemaArray = std::static_pointer_cast<semantic::ArrayType>( expressionComp.semanticType );
						expressionCompElemType = this->toLLVMType( expressionSemaArray->elementType );
					}
					llvm::Value* expressionVarAlloca = this->createEntryBlockAllocation( this->currentFunction, expressionComp.variable, expressionI64 );
					this->namedValues[expressionComp.variable] = expressionVarAlloca;
					uint64_t expressionCompElemSize = this->module->getDataLayout().getTypeAllocSize( expressionCompElemType );
					std::string expressionAllocaEndName( fmt::format( "{}.end", expressionComp.variable ) );
					std::string expressionAllocaIndexName( fmt::format( "{}.idx", expressionComp.variable ) );
					llvm::Value* expressionTotalBytes = this->builder.CreateMul( expressionSize, llvm::ConstantInt::get( expressionI64, expressionCompElemSize ), "comp.bytes" );
					llvm::Value* expressionRawPointer = this->builder.CreateCall( this->getOrCreateMalloc(), { expressionTotalBytes }, "comp.ptr" );
					llvm::Value* expressionArrayPointer = this->builder.CreateBitCast( expressionRawPointer, llvm::PointerType::getUnqual( expressionCompElemType ), "comp.arr" );
					llvm::Value* expressionIndexAlloca = this->createEntryBlockAllocation( this->currentFunction, expressionAllocaIndexName, expressionI64 );
					this->builder.CreateStore( llvm::ConstantInt::get( expressionI64, 0 ), expressionIndexAlloca );
					llvm::Value* exprEndAlloca = this->createEntryBlockAllocation( this->currentFunction, expressionAllocaEndName, expressionI64 );
					this->builder.CreateStore( exprEnd, exprEndAlloca );
					this->builder.CreateStore( expressionStart, expressionVarAlloca );
					llvm::BasicBlock* expressionConditionBasicBlock = llvm::BasicBlock::Create( this->context, "comp.cond", this->currentFunction );
					llvm::BasicBlock* expressionBodyBasicBlock = llvm::BasicBlock::Create( this->context, "comp.body", this->currentFunction );
					llvm::BasicBlock* expressionIncBasicBlock = llvm::BasicBlock::Create( this->context, "comp.inc", this->currentFunction );
					llvm::BasicBlock* exprEndBasicBlock = llvm::BasicBlock::Create( this->context, "comp.end", this->currentFunction );
					this->builder.CreateBr( expressionConditionBasicBlock );
					this->builder.SetInsertPoint( expressionConditionBasicBlock );
					llvm::Value* expressionCurVar = this->builder.CreateLoad( expressionI64, expressionVarAlloca, "comp.cur" );
					llvm::Value* exprEndLoad = this->builder.CreateLoad( expressionI64, exprEndAlloca, "comp.end" );
					llvm::Value* expressionLoopCondition = expressionRange.inclusive ? this->builder.CreateICmpSLE( expressionCurVar, exprEndLoad, "comp.cond" ) : this->builder.CreateICmpSLT( expressionCurVar, exprEndLoad, "comp.cond" );
					this->builder.CreateCondBr( expressionLoopCondition, expressionBodyBasicBlock, exprEndBasicBlock );
					this->builder.SetInsertPoint( expressionBodyBasicBlock );
					llvm::Value* expressionBodyValue = this->generateExpression( expressionComp.bodyExpression );
					if( expressionBodyValue != nullptr ) {
						expressionBodyValue = this->generateImplicitCast( expressionBodyValue, expressionCompElemType );
						if( expressionComp.condition ) {
							llvm::Value* expressionConditionValue = this->generateExpression( expressionComp.condition );
							if( expressionConditionValue != nullptr ) {
								llvm::BasicBlock* expressionStoreBasicBlock = llvm::BasicBlock::Create( this->context, "comp.store", this->currentFunction );
								llvm::BasicBlock* expressionSkipBasicBlock = llvm::BasicBlock::Create( this->context, "comp.skip", this->currentFunction );
								this->builder.CreateCondBr( expressionConditionValue, expressionStoreBasicBlock, expressionSkipBasicBlock );
								this->builder.SetInsertPoint( expressionStoreBasicBlock );
								llvm::Value* expressionIndex = this->builder.CreateLoad( expressionI64, expressionIndexAlloca, "comp.idx" );
								llvm::Value* expressionGep = this->builder.CreateGEP( expressionCompElemType, expressionArrayPointer, expressionIndex, "comp.elem" );
								this->builder.CreateStore( expressionBodyValue, expressionGep );
								this->builder.CreateStore( this->builder.CreateAdd( expressionIndex, llvm::ConstantInt::get( expressionI64, 1 ) ), expressionIndexAlloca );
								this->builder.CreateBr( expressionIncBasicBlock );
								this->builder.SetInsertPoint( expressionSkipBasicBlock );
								this->builder.CreateBr( expressionIncBasicBlock );
							}
						}
						else {
							llvm::Value* expressionIndex = this->builder.CreateLoad( expressionI64, expressionIndexAlloca, "comp.idx" );
							llvm::Value* expressionGep = this->builder.CreateGEP( expressionCompElemType, expressionArrayPointer, expressionIndex, "comp.elem" );
							this->builder.CreateStore( expressionBodyValue, expressionGep );
							this->builder.CreateStore( this->builder.CreateAdd( expressionIndex, llvm::ConstantInt::get( expressionI64, 1 ) ), expressionIndexAlloca );
						}
					}
					if( this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
						this->builder.CreateBr( expressionIncBasicBlock );
					}
					this->builder.SetInsertPoint( expressionIncBasicBlock );
					llvm::Value* expressionIncVar = this->builder.CreateLoad( expressionI64, expressionVarAlloca, "comp.inc.var" );
					this->builder.CreateStore( this->builder.CreateAdd( expressionIncVar, llvm::ConstantInt::get( expressionI64, 1 ) ), expressionVarAlloca );
					this->builder.CreateBr( expressionConditionBasicBlock );
					this->builder.SetInsertPoint( exprEndBasicBlock );
					this->lastMemoryElementType = expressionCompElemType;
					return expressionArrayPointer;
				}
				return llvm::ConstantPointerNull::get( llvm::PointerType::getUnqual( this->context ) );
			}
			case ast::Node::Kind::ConstructExpression:
				return this->generateConstructExpression( static_cast<ast::nodes::ConstructExpression&>( *expression ) );
			case ast::Node::Kind::FloatLiteral: {
				ast::nodes::FloatLiteralExpression& expressionFloat = static_cast<ast::nodes::FloatLiteralExpression&>( *expression );
				return llvm::ConstantFP::get( llvm::Type::getDoubleTy( this->context ), expressionFloat.value );
			}
			case ast::Node::Kind::IdentifierExpression:
				return this->generateIdentifierExpression( static_cast<ast::nodes::IdentifierExpression&>( *expression ) );
			case ast::Node::Kind::IndexExpression:
				return this->generateIndexExpression( static_cast<ast::nodes::IndexExpression&>( *expression ) );
			case ast::Node::Kind::InstanceofExpression: {
				ast::nodes::InstanceofExpression& expressionIo = static_cast<ast::nodes::InstanceofExpression&>( *expression );
				llvm::Value* expressionObj = this->generateExpression( expressionIo.object );
				if( expressionObj == nullptr ) {
					return llvm::ConstantInt::getFalse( this->context );
				}
				std::string expressionObjName = this->resolveStructTypeName( expressionIo.object );
				std::string expressionTargetName;
				if( expressionIo.targetType && expressionIo.targetType->kind == ast::Node::Kind::SimpleType ) {
					expressionTargetName = static_cast<ast::nodes::SimpleTypeNode&>( *expressionIo.targetType ).name;
				}
				if( expressionObjName.empty() || expressionTargetName.empty() ) {
					return llvm::ConstantInt::getFalse( this->context );
				}
				if( expressionObjName == expressionTargetName ) {
					return llvm::ConstantInt::getTrue( this->context );
				}
				semantic::TypeSharedPointer expressionObjType = this->analyzer.types().lookupType( expressionObjName );
				semantic::TypeSharedPointer expressionTargetType = this->analyzer.types().lookupType( expressionTargetName );
				if( expressionObjType && expressionTargetType && this->analyzer.types().isAssignable( expressionTargetType, expressionObjType ) ) {
					return llvm::ConstantInt::getTrue( this->context );
				}
				return llvm::ConstantInt::getFalse( this->context );
			}
			case ast::Node::Kind::IntegerLiteral: {
				ast::nodes::IntegerLiteralExpression& expressionInt = static_cast<ast::nodes::IntegerLiteralExpression&>( *expression );
				return llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->context ), expressionInt.value, true );
			}
			case ast::Node::Kind::LambdaExpression:
				return this->generateLambdaExpression( static_cast<ast::nodes::LambdaExpression&>( *expression ) );
			case ast::Node::Kind::MatchExpression:
				return this->generateMatchExpression( static_cast<ast::nodes::MatchExpression&>( *expression ) );
			case ast::Node::Kind::MemberAccessExpression:
				return this->generateMemberAccessExpression( static_cast<ast::nodes::MemberAccessExpression&>( *expression ) );
			case ast::Node::Kind::MethodCallExpression:
				return this->generateMethodCallExpression( static_cast<ast::nodes::MethodCallExpression&>( *expression ) );
			case ast::Node::Kind::NoneLiteral:
				return llvm::ConstantPointerNull::get( llvm::PointerType::getUnqual( this->context ) );
			case ast::Node::Kind::RegexLiteral: {
				ast::nodes::RegexLiteralExpression& regexExpression = static_cast<ast::nodes::RegexLiteralExpression&>( *expression );
				return this->builder.CreateGlobalStringPtr( regexExpression.pattern, "regex.pattern" );
			}
			case ast::Node::Kind::SelfExpression: {
				std::unordered_map<std::string, llvm::Value*>::iterator expressionSelfIt = this->namedValues.find( "self" );
				if( expressionSelfIt != this->namedValues.end() ) {
					return this->builder.CreateLoad( getPointeeType( expressionSelfIt->second ), expressionSelfIt->second, "self" );
				}
				return nullptr;
			}
			case ast::Node::Kind::StringLiteral: {
				ast::nodes::StringLiteralExpression& expressionStr = static_cast<ast::nodes::StringLiteralExpression&>( *expression );
				return this->builder.CreateGlobalStringPtr( expressionStr.value, "str" );
			}
			case ast::Node::Kind::SubclassofExpression: {
				ast::nodes::SubclassofExpression& expressionSo = static_cast<ast::nodes::SubclassofExpression&>( *expression );
				std::string expressionSrcName, expressionTgtName;
				if( expressionSo.sourceType && expressionSo.sourceType->kind == ast::Node::Kind::SimpleType ) {
					expressionSrcName = static_cast<ast::nodes::SimpleTypeNode&>( *expressionSo.sourceType ).name;
				}
				if( expressionSo.targetType && expressionSo.targetType->kind == ast::Node::Kind::SimpleType ) {
					expressionTgtName = static_cast<ast::nodes::SimpleTypeNode&>( *expressionSo.targetType ).name;
				}
				if( expressionSrcName.empty() || expressionTgtName.empty() ) {
					return llvm::ConstantInt::getFalse( this->context );
				}
				semantic::TypeSharedPointer expressionSrcType = this->analyzer.types().lookupType( expressionSrcName );
				if( expressionSrcType && expressionSrcType->kind == semantic::Type::Kind::Class ) {
					semantic::ClassTypeSharedPointer expressionClass = std::static_pointer_cast<semantic::ClassType>( expressionSrcType );
					semantic::TypeSharedPointer expressionBase = expressionClass->baseClass;
					while( expressionBase ) {
						if( expressionBase->name == expressionTgtName ) {
							return llvm::ConstantInt::getTrue( this->context );
						}
						if( expressionBase->kind == semantic::Type::Kind::Class ) {
                        	expressionBase = std::static_pointer_cast<semantic::ClassType>( expressionBase )->baseClass;
						}
						else {
							break;
						}
					}
				}
				return llvm::ConstantInt::getFalse( this->context );
			}
			case ast::Node::Kind::TypeReferenceExpression: {
				ast::nodes::TypeReferenceExpression& expressionTr = static_cast<ast::nodes::TypeReferenceExpression&>( *expression );
				uint64_t expressionHash = std::hash<std::string>{}( expressionTr.typeName );
				return llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->context ), expressionHash );
			}
			case ast::Node::Kind::UnaryExpression:
				return this->generateUnaryExpression( static_cast<ast::nodes::UnaryExpression&>( *expression ) );
			case ast::Node::Kind::YieldExpression: {
				ast::nodes::YieldExpression& expressionYield = static_cast<ast::nodes::YieldExpression&>( *expression );
				llvm::Value* expressionYieldValue = expressionYield.value ? this->generateExpression( expressionYield.value ) : nullptr;
				if( this->currentGenerator && expressionYieldValue ) {
					this->builder.CreateStore( expressionYieldValue, this->currentGenerator->valueVar );
					this->builder.CreateStore( llvm::ConstantInt::getFalse( this->context ), this->currentGenerator->doneVar );
					int expressionStateId = this->currentGenerator->nextStateId++;
					this->builder.CreateStore( llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->context ), expressionStateId ), this->currentGenerator->stateVar );
					this->builder.CreateBr( this->currentGenerator->exitBB );
					llvm::BasicBlock* expressionResumeBasicBlock = llvm::BasicBlock::Create( this->context, fmt::format( "resume.{}", expressionStateId ), this->currentFunction );
					this->builder.SetInsertPoint( expressionResumeBasicBlock );
				}
				return expressionYieldValue;
			}
			default: {
				std::string expressionKindName = fmt::format( "{}", static_cast<int>( expression->kind ) );
				this->diagnostic.error(
					expression->source,
					fmt::format( "codegen: unhandled expression kind {}", expressionKindName )
				);
				return nullptr;
			}
		}
	}
	
	void LLVMCodegen::generateExternDeclaration( ast::nodes::ExternDeclaration& declaration ) {
		
		// Skip if function already exists in LLVM module with same link name (e.g., async
		// runtime functions created by codegen with different signatures than module extern
		// declarations). But allow extern to override a same-name Uranite function (e.g.,
		// C realloc vs mmap-based allocator realloc) — the extern is the user's explicit intent.
		if( this->module->getFunction( declaration.linkName ) ) {
			return;
		}
		
		// Build parameter types
		std::vector<llvm::Type*> externParameterTypes;
		for( ast::nodes::ExternParameter& externParameter : declaration.parameters ) {
			if( externParameter.type ) {
				std::string externParameterTypeName;
				if( externParameter.type->kind == ast::Node::Kind::SimpleType ) {
					externParameterTypeName = static_cast<ast::nodes::SimpleTypeNode&>( *externParameter.type ).name;
				}
				else if( externParameter.type->kind == ast::Node::Kind::GenericType ) {
					externParameterTypeName = static_cast<ast::nodes::GenericTypeNode&>( *externParameter.type ).name;
				}
				semantic::TypeSharedPointer externParameterTypeResolved = externParameterTypeName.empty() ? nullptr : this->analyzer.types().lookupType( externParameterTypeName );
				if( externParameterTypeResolved != nullptr ) {
					externParameterTypes.push_back( this->toLLVMType( externParameterTypeResolved ) );
					continue;
				}
				externParameterTypes.push_back( this->resolveAstType( externParameter.type ) );
			}
			else {
				externParameterTypes.push_back( llvm::Type::getInt64Ty( this->context ) );
			}
		}
		
		// Build return type
		this->functions[declaration.name] = llvm::Function::Create(
			llvm::FunctionType::get(
				this->resolveAstType(
					declaration.returnType
				),
				externParameterTypes,
				declaration.isVariadic
			),
			llvm::Function::ExternalLinkage,
			declaration.linkName,
			this->getModule()
		);
	}
	
	void LLVMCodegen::generateForStatement( ast::nodes::ForStatement& forStatement ) {
		llvm::Function* forFunction = this->currentFunction;
		if( forStatement.isCStyle ) {
			
			// C-style for: for Type var = init; cond; update:
			llvm::Type* forVarType = llvm::Type::getInt64Ty( this->context );
			if( forStatement.variableType ) {
				semantic::TypeSharedPointer forResolvedType = this->analyzer.types().lookupType( static_cast<ast::nodes::SimpleTypeNode&>( *forStatement.variableType ).name );
				if( forResolvedType ) forVarType = this->toLLVMType( forResolvedType );
			}
			llvm::Value* forAlloca = this->createEntryBlockAllocation( forFunction, forStatement.variable, forVarType );
			this->namedValues[forStatement.variable] = forAlloca;
			if( forStatement.initializer ) {
				llvm::Value* forInitValue = this->generateExpression( forStatement.initializer );
				if( forInitValue ) {
					forInitValue = this->generateImplicitCast( forInitValue, forVarType );
					this->builder.CreateStore( forInitValue, forAlloca );
				}
			}
			
			llvm::BasicBlock* forConditionBasicBlock = llvm::BasicBlock::Create( this->context, "cfor.cond", forFunction );
			llvm::BasicBlock* forBodyBasicBlock = llvm::BasicBlock::Create( this->context, "cfor.body", forFunction );
			llvm::BasicBlock* forIncBasicBlock = llvm::BasicBlock::Create( this->context, "cfor.inc", forFunction );
			llvm::BasicBlock* forEndBasicBlock = llvm::BasicBlock::Create( this->context, "cfor.end", forFunction );
			
			this->builder.CreateBr( forConditionBasicBlock );
			this->builder.SetInsertPoint( forConditionBasicBlock );
			if( forStatement.condition ) {
				llvm::Value* forConditionValue = this->generateExpression( forStatement.condition );
				if( forConditionValue ) {
					if( forConditionValue->getType()->isIntegerTy( 1 ) == false ) {
						if( forConditionValue->getType()->isStructTy() ) {
							forConditionValue = this->builder.CreateExtractValue( forConditionValue, 0, "struct.scalar" );
						}
						forConditionValue = this->builder.CreateICmpNE( forConditionValue, llvm::Constant::getNullValue( forConditionValue->getType() ), "tobool" );
					}
					this->builder.CreateCondBr( forConditionValue, forBodyBasicBlock, forEndBasicBlock );
				}
				else {
					this->builder.CreateBr( forBodyBasicBlock );
				}
			}
			else {
				this->builder.CreateBr( forBodyBasicBlock );
			}
			this->builder.SetInsertPoint( forBodyBasicBlock );
			this->breakTargets.push( forEndBasicBlock );
			this->continueTargets.push( forIncBasicBlock );
			this->pushCleanupScope();
			this->pushDeferScope();
			for( ast::nodes::StatementSharedPointer& forBodyStatement : forStatement.body ) {
				this->generateStatement( forBodyStatement );
			}
			this->emitCurrentScopeDefers();
			this->popDeferScope();
			this->popCleanupScope();
			this->continueTargets.pop();
			this->breakTargets.pop();
			if( this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
				this->builder.CreateBr( forIncBasicBlock );
			}
			this->builder.SetInsertPoint( forIncBasicBlock );
			if( forStatement.update ) this->generateStatement( forStatement.update );
			this->builder.CreateBr( forConditionBasicBlock );
			this->builder.SetInsertPoint( forEndBasicBlock );
			return;
		}
		
		// Range-based for: for [Type] var in expression:
		// Handle RangeExpression (0..5, 1..=10) directly
		if( forStatement.iterable && forStatement.iterable->kind == ast::Node::Kind::RangeExpression ) {
			ast::nodes::RangeExpression& forRange = static_cast<ast::nodes::RangeExpression&>( *forStatement.iterable );
			llvm::Type* forRangeVarType = llvm::Type::getInt64Ty( this->context );
			if( forStatement.variableType ) {
				semantic::TypeSharedPointer forRangeResolved = this->analyzer.types().lookupType( static_cast<ast::nodes::SimpleTypeNode&>( *forStatement.variableType ).name );
				if( forRangeResolved ) {
					forRangeVarType = this->toLLVMType( forRangeResolved );
				}
			}
			llvm::Value* forStartValue = this->generateExpression( forRange.start );
			llvm::Value* forEndValue = this->generateExpression( forRange.end );
			if( forStartValue == nullptr ) {
				forStartValue = llvm::ConstantInt::get( forRangeVarType, 0 );
			}
			if( forEndValue == nullptr ) {
				forEndValue = llvm::ConstantInt::get( forRangeVarType, 0 );
			}
			
			forStartValue = this->generateImplicitCast( forStartValue, forRangeVarType );
			forEndValue = this->generateImplicitCast( forEndValue, forRangeVarType );
			
			llvm::Value* forRangeAlloca = this->createEntryBlockAllocation( forFunction, forStatement.variable, forRangeVarType );
			
			this->builder.CreateStore( forStartValue, forRangeAlloca );
			this->namedValues[forStatement.variable] = forRangeAlloca;
			if( forStatement.variable2.empty() == false ) {
				llvm::Value* forIndex2Alloca = this->createEntryBlockAllocation( forFunction, forStatement.variable2, forRangeVarType );
				this->builder.CreateStore( llvm::ConstantInt::get( forRangeVarType, 0 ), forIndex2Alloca );
				this->namedValues[forStatement.variable2] = forIndex2Alloca;
			}
			
			std::string forEndAllocaName = fmt::format( "{}.end", forStatement.variable );
			llvm::Value* forEndAlloca = this->createEntryBlockAllocation( forFunction, forEndAllocaName, forRangeVarType );
			
			this->builder.CreateStore( forEndValue, forEndAlloca );
			this->namedValues[forEndAllocaName] = forEndAlloca;
			
			llvm::BasicBlock* forRangeConditionBasicBlock = llvm::BasicBlock::Create( this->context, "rfor.cond", forFunction );
			llvm::BasicBlock* forRangeBodyBasicBlock = llvm::BasicBlock::Create( this->context, "rfor.body", forFunction );
			llvm::BasicBlock* forRangeIncBasicBlock = llvm::BasicBlock::Create( this->context, "rfor.inc", forFunction );
			llvm::BasicBlock* forRangeEndBasicBlock = llvm::BasicBlock::Create( this->context, "rfor.end", forFunction );
			
			this->builder.CreateBr( forRangeConditionBasicBlock );
			this->builder.SetInsertPoint( forRangeConditionBasicBlock );
			
			llvm::Value* forCurIndex = this->builder.CreateLoad( forRangeVarType, forRangeAlloca, "idx" );
			llvm::Value* forCurEnd = this->builder.CreateLoad( forRangeVarType, forEndAlloca, "end.val" );
			llvm::Value* forRangeCondition = forRange.inclusive ? this->builder.CreateICmpSLE( forCurIndex, forCurEnd, "forcond" ) : this->builder.CreateICmpSLT( forCurIndex, forCurEnd, "forcond" );
			
			this->builder.CreateCondBr( forRangeCondition, forRangeBodyBasicBlock, forRangeEndBasicBlock );
			this->builder.SetInsertPoint( forRangeBodyBasicBlock );
			this->breakTargets.push( forRangeEndBasicBlock );
			this->continueTargets.push( forRangeIncBasicBlock );
			this->pushCleanupScope();
			this->pushDeferScope();
			for( ast::nodes::StatementSharedPointer& forRangeStatement : forStatement.body ) {
				this->generateStatement( forRangeStatement );
			}
			this->emitCurrentScopeDefers();
			this->popDeferScope();
			this->popCleanupScope();
			this->continueTargets.pop();
			this->breakTargets.pop();
			if( this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
				this->builder.CreateBr( forRangeIncBasicBlock );
			}
			
			this->builder.SetInsertPoint( forRangeIncBasicBlock );
			llvm::Value* forIncIndex = this->builder.CreateLoad( forRangeVarType, forRangeAlloca, "inc.idx" );
			llvm::Value* forNextIndex = this->builder.CreateAdd( forIncIndex, llvm::ConstantInt::get( forRangeVarType, 1 ), "nextidx" );
			this->builder.CreateStore( forNextIndex, forRangeAlloca );
			
			if( forStatement.variable2.empty() == false ) {
				std::unordered_map<std::string, llvm::Value*>::iterator forIndex2It = this->namedValues.find( forStatement.variable2 );
				if( forIndex2It != this->namedValues.end() ) {
					llvm::Value* forIncIndex2 = this->builder.CreateLoad( forRangeVarType, forIndex2It->second, "inc.idx2" );
					llvm::Value* forNextIndex2 = this->builder.CreateAdd( forIncIndex2, llvm::ConstantInt::get( forRangeVarType, 1 ), "nextidx2" );
					this->builder.CreateStore( forNextIndex2, forIndex2It->second );
				}
			}
			this->builder.CreateBr( forRangeConditionBasicBlock );
			this->builder.SetInsertPoint( forRangeEndBasicBlock );
			return;
		}
		
		// Generator iteration: for T val in generatorFunc(args):
		if( forStatement.iterable && forStatement.iterable->semanticType && forStatement.iterable->semanticType->kind == semantic::Type::Kind::Generator ) {
			semantic::GeneratorTypeSharedPointer forGenType = std::static_pointer_cast<semantic::GeneratorType>( forStatement.iterable->semanticType );
			llvm::Value* forIteratorValue = this->generateExpression( forStatement.iterable );
			if( forIteratorValue == nullptr ) {
				return;
			}
			llvm::StructType* forGenStructType = llvm::cast<llvm::StructType>( forIteratorValue->getType() );
			llvm::Type* forYieldLLVMType = forGenStructType->getElementType( 1 );
			llvm::Value* forGenAlloca = this->createEntryBlockAllocation( forFunction, "gen.iter", forGenStructType );
			
			this->builder.CreateStore( forIteratorValue, forGenAlloca );
			
			llvm::Value* forYieldVarAlloca = this->createEntryBlockAllocation( forFunction, forStatement.variable, forYieldLLVMType );
			
			this->namedValues[forStatement.variable] = forYieldVarAlloca;
			
			std::string forCalleeName;
			if( forStatement.iterable->kind == ast::Node::Kind::CallExpression ) {
				ast::nodes::CallExpression& forCall = static_cast<ast::nodes::CallExpression&>( *forStatement.iterable );
				if( forCall.callee->kind == ast::Node::Kind::IdentifierExpression ) {
					forCalleeName = static_cast<ast::nodes::IdentifierExpression&>( *forCall.callee ).name;
				}
			}
			
			std::string forNextFnKey = fmt::format( "{}.next", forCalleeName );
			llvm::Function* forNextFunction = this->functions.count( forNextFnKey ) ? this->functions[forNextFnKey] : nullptr;
			
			llvm::BasicBlock* forGenConditionBasicBlock = llvm::BasicBlock::Create( this->context, "gfor.cond", forFunction );
			llvm::BasicBlock* forGenBodyBasicBlock = llvm::BasicBlock::Create( this->context, "gfor.body", forFunction );
			llvm::BasicBlock* forGenIncBasicBlock = llvm::BasicBlock::Create( this->context, "gfor.inc", forFunction );
			llvm::BasicBlock* forGenEndBasicBlock = llvm::BasicBlock::Create( this->context, "gfor.end", forFunction );
			
			llvm::Value* forDonePointer0 = this->builder.CreateStructGEP( forGenStructType, forGenAlloca, 2, "done.ptr0" );
			llvm::Value* forDone0 = this->builder.CreateLoad( llvm::Type::getInt1Ty( this->context ), forDonePointer0, "done0" );
			
			this->builder.CreateCondBr( forDone0, forGenEndBasicBlock, forGenBodyBasicBlock );
			this->builder.SetInsertPoint( forGenBodyBasicBlock );
			
			llvm::Value* forValuePointer = this->builder.CreateStructGEP( forGenStructType, forGenAlloca, 1, "val.ptr" );
			llvm::Value* forGenValue = this->builder.CreateLoad( forYieldLLVMType, forValuePointer, "gen.val" );
			
			this->builder.CreateStore( forGenValue, forYieldVarAlloca );
			this->breakTargets.push( forGenEndBasicBlock );
			this->continueTargets.push( forGenIncBasicBlock );
			this->pushDeferScope();
			for( ast::nodes::StatementSharedPointer& forGenStatement : forStatement.body ) {
				this->generateStatement( forGenStatement );
			}
			this->emitCurrentScopeDefers();
			this->popDeferScope();
			this->continueTargets.pop();
			this->breakTargets.pop();
			if( this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
				this->builder.CreateBr( forGenIncBasicBlock );
			}
			this->builder.SetInsertPoint( forGenIncBasicBlock );
			if( forNextFunction ) {
				this->builder.CreateCall( forNextFunction, {forGenAlloca} );
			}
			this->builder.CreateBr( forGenConditionBasicBlock );
			this->builder.SetInsertPoint( forGenConditionBasicBlock );
			
			llvm::Value* forDonePointer = this->builder.CreateStructGEP( forGenStructType, forGenAlloca, 2, "done.ptr" );
			llvm::Value* forDone = this->builder.CreateLoad( llvm::Type::getInt1Ty( this->context ), forDonePointer, "done" );
			
			this->builder.CreateCondBr( forDone, forGenEndBasicBlock, forGenBodyBasicBlock );
			this->builder.SetInsertPoint( forGenEndBasicBlock );
			
			return;
		}
		
		llvm::Value* forBaseIteratorValue = this->generateExpression( forStatement.iterable );
		if( forBaseIteratorValue == nullptr ) {
			return;
		}
		
		// String iteration: for Char c in "hello":
		if( forBaseIteratorValue->getType()->isPointerTy() && getPointeeType( forBaseIteratorValue )->isIntegerTy( 8 ) ) {
			llvm::Function* forStrlenFn = this->getOrCreateStringLength();
			llvm::Value* forStrLen = this->builder.CreateCall( forStrlenFn, {forBaseIteratorValue}, "strlen" );
			
			llvm::Type* forIndexType = llvm::Type::getInt64Ty( this->context );
			llvm::Value* forStrIndexAlloca = this->createEntryBlockAllocation( forFunction, fmt::format( "{}.idx", forStatement.variable ), forIndexType );
			
			this->builder.CreateStore( llvm::ConstantInt::get( forIndexType, 0 ), forStrIndexAlloca );
			
			llvm::Type* forCharType = llvm::Type::getInt32Ty( this->context );
			llvm::Value* forStrVarAlloca = this->createEntryBlockAllocation( forFunction, forStatement.variable, forCharType );
			
			this->namedValues[forStatement.variable] = forStrVarAlloca;
			
			llvm::BasicBlock* forStrConditionBasicBlock = llvm::BasicBlock::Create( this->context, "sfor.cond", forFunction );
			llvm::BasicBlock* forStrBodyBasicBlock = llvm::BasicBlock::Create( this->context, "sfor.body", forFunction );
			llvm::BasicBlock* forStrIncBasicBlock = llvm::BasicBlock::Create( this->context, "sfor.inc", forFunction );
			llvm::BasicBlock* forStrEndBasicBlock = llvm::BasicBlock::Create( this->context, "sfor.end", forFunction );
			
			this->builder.CreateBr( forStrConditionBasicBlock );
			this->builder.SetInsertPoint( forStrConditionBasicBlock );
			
			llvm::Value* forCurStrIndex = this->builder.CreateLoad( forIndexType, forStrIndexAlloca, "index" );
			llvm::Value* forLenExt = this->builder.CreateZExt( forStrLen, forIndexType, "lenext" );
			llvm::Value* forStrCondition = this->builder.CreateICmpULT( forCurStrIndex, forLenExt, "forcond" );
			
			this->builder.CreateCondBr( forStrCondition, forStrBodyBasicBlock, forStrEndBasicBlock );
			this->builder.SetInsertPoint( forStrBodyBasicBlock );
			
			llvm::Value* forBodyIndex = this->builder.CreateLoad( forIndexType, forStrIndexAlloca, "body.idx" );
			llvm::Value* forCharPointer = this->builder.CreateGEP( llvm::Type::getInt8Ty( this->context ), forBaseIteratorValue, forBodyIndex, "charptr" );
			llvm::Value* forCharValue = this->builder.CreateLoad( llvm::Type::getInt8Ty( this->context ), forCharPointer, "char" );
			llvm::Value* forCharExt = this->builder.CreateZExt( forCharValue, forCharType, "charext" );
			
			this->builder.CreateStore( forCharExt, forStrVarAlloca );
			this->breakTargets.push( forStrEndBasicBlock );
			this->continueTargets.push( forStrIncBasicBlock );
			this->pushDeferScope();
			for( ast::nodes::StatementSharedPointer& forStrStatement : forStatement.body ) {
				this->generateStatement( forStrStatement );
			}
			this->emitCurrentScopeDefers();
			this->popDeferScope();
			this->continueTargets.pop();
			this->breakTargets.pop();
			if( this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
				this->builder.CreateBr( forStrIncBasicBlock );
			}
			this->builder.SetInsertPoint( forStrIncBasicBlock );
			
			llvm::Value* forStrIncIndex = this->builder.CreateLoad( forIndexType, forStrIndexAlloca, "inc.idx" );
			llvm::Value* forStrNextIndex = this->builder.CreateAdd( forStrIncIndex, llvm::ConstantInt::get( forIndexType, 1 ), "nextidx" );
			
			this->builder.CreateStore( forStrNextIndex, forStrIndexAlloca );
			this->builder.CreateBr( forStrConditionBasicBlock );
			this->builder.SetInsertPoint( forStrEndBasicBlock );
			
			return;
		}
		
		// Array iteration fallback: for [Type] var in arrayExpression:
		if( forStatement.iterable->semanticType && forStatement.iterable->semanticType->kind == semantic::Type::Kind::Array ) {
			semantic::ArrayTypeSharedPointer forArrayType = std::static_pointer_cast<semantic::ArrayType>( forStatement.iterable->semanticType );
			int64_t forArraySizeLimit = forArrayType->size >= 0 ? forArrayType->size : 0;
			
			llvm::Type* forArrayIndexType = llvm::Type::getInt64Ty( this->context );
			llvm::Type* forArrayElemType = this->toLLVMType( forArrayType->elementType );
			
			llvm::Value* forArrayIndexAlloca = this->createEntryBlockAllocation( forFunction, fmt::format( "{}.idx", forStatement.variable ), forArrayIndexType );
			this->builder.CreateStore( llvm::ConstantInt::get( forArrayIndexType, 0 ), forArrayIndexAlloca );
			
			llvm::Value* forArrayVarAlloca = this->createEntryBlockAllocation( forFunction, forStatement.variable, forArrayElemType );
			this->namedValues[forStatement.variable] = forArrayVarAlloca;
			
			llvm::BasicBlock* forArrayConditionBasicBlock = llvm::BasicBlock::Create( this->context, "afor.cond", forFunction );
			llvm::BasicBlock* forArrayBodyBasicBlock = llvm::BasicBlock::Create( this->context, "afor.body", forFunction );
			llvm::BasicBlock* forArrayIncBasicBlock = llvm::BasicBlock::Create( this->context, "afor.inc", forFunction );
			llvm::BasicBlock* forArrayEndBasicBlock = llvm::BasicBlock::Create( this->context, "afor.end", forFunction );
			
			this->builder.CreateBr( forArrayConditionBasicBlock );
			this->builder.SetInsertPoint( forArrayConditionBasicBlock );
			
			llvm::Value* forCurArrayIndex = this->builder.CreateLoad( forArrayIndexType, forArrayIndexAlloca, "idx" );
			llvm::Value* forArrayLimit = llvm::ConstantInt::get( forArrayIndexType, forArraySizeLimit );
			llvm::Value* forArrayCondition = this->builder.CreateICmpSLT( forCurArrayIndex, forArrayLimit, "forcond" );
			
			this->builder.CreateCondBr( forArrayCondition, forArrayBodyBasicBlock, forArrayEndBasicBlock );
			this->builder.SetInsertPoint( forArrayBodyBasicBlock );
			
			llvm::Value* forArrayBodyIndex = this->builder.CreateLoad( forArrayIndexType, forArrayIndexAlloca, "body.idx" );
			llvm::Value* forArrayElemPointer = this->builder.CreateGEP( forArrayElemType, forBaseIteratorValue, forArrayBodyIndex, "elemptr" );
			llvm::Value* forArrayElemValue = this->builder.CreateLoad( forArrayElemType, forArrayElemPointer, "elem" );
			
			this->builder.CreateStore( forArrayElemValue, forArrayVarAlloca );
			this->breakTargets.push( forArrayEndBasicBlock );
			this->continueTargets.push( forArrayIncBasicBlock );
			this->pushDeferScope();
			for( ast::nodes::StatementSharedPointer& forArrayStatement : forStatement.body ) {
				this->generateStatement( forArrayStatement );
			}
			this->emitCurrentScopeDefers();
			this->popDeferScope();
			this->continueTargets.pop();
			this->breakTargets.pop();
			if( this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
				this->builder.CreateBr( forArrayIncBasicBlock );
			}
			this->builder.SetInsertPoint( forArrayIncBasicBlock );
			
			llvm::Value* forArrayIncIndexValue = this->builder.CreateLoad( forArrayIndexType, forArrayIndexAlloca, "inc.idx" );
			llvm::Value* forArrayNextIndexValue = this->builder.CreateAdd( forArrayIncIndexValue, llvm::ConstantInt::get( forArrayIndexType, 1 ), "nextidx" );
			
			this->builder.CreateStore( forArrayNextIndexValue, forArrayIndexAlloca );
			this->builder.CreateBr( forArrayConditionBasicBlock );
			this->builder.SetInsertPoint( forArrayEndBasicBlock );
			
			return;
		}
		
		// Iterable protocol: class/struct with has()/next() methods
		if( forStatement.iterable->semanticType && ( forStatement.iterable->semanticType->kind == semantic::Type::Kind::Class || forStatement.iterable->semanticType->kind == semantic::Type::Kind::Struct ) ) {
			std::string forIteratorTypeName = forStatement.iterable->semanticType->name;
			std::string forBaseTypeName = forIteratorTypeName;
			size_t forGenericBracket = forBaseTypeName.find( '<' );
			if( forGenericBracket != std::string::npos ) {
				forBaseTypeName = forBaseTypeName.substr( 0, forGenericBracket );
			}
			llvm::Function* forHasNextFn = nullptr;
			for( const char* forMethodName : {"hasNext", "has"} ) {
				std::string forHasKey = fmt::format( "{}::{}", forIteratorTypeName, forMethodName );
				if( this->functions.count( forHasKey ) ) {
					forHasNextFn = this->functions[forHasKey];
					break;
				}
				if( forBaseTypeName != forIteratorTypeName ) {
					std::string forHasBaseKey = fmt::format( "{}::{}", forBaseTypeName, forMethodName );
					if( this->functions.count( forHasBaseKey ) ) {
						forHasNextFn = this->functions[forHasBaseKey];
						break;
					}
				}
			}
			std::string forNextKey = fmt::format( "{}::next", forIteratorTypeName );
			llvm::Function* forNextFn = this->functions.count( forNextKey ) ? this->functions[forNextKey] : nullptr;
			if( forNextFn == nullptr && forBaseTypeName != forIteratorTypeName ) {
				std::string forNextBaseKey = fmt::format( "{}::next", forBaseTypeName );
				forNextFn = this->functions.count( forNextBaseKey ) ? this->functions[forNextBaseKey] : nullptr;
			}
			if( forHasNextFn && forNextFn ) {
				llvm::Value* forObjPointer = forBaseIteratorValue;
				if( forObjPointer->getType()->isPointerTy() == false ) {
					return;
				}
				llvm::Type* forExpectedSelfType = forHasNextFn->getFunctionType()->getParamType( 0 );
				if( forObjPointer->getType() != forExpectedSelfType ) {
					forObjPointer = this->builder.CreateBitCast( forObjPointer, forExpectedSelfType, "iter.cast" );
				}
				bool forIsOptional = false;
				llvm::Type* forNextRetType = forNextFn->getReturnType();
				llvm::Type* forElemType = forNextRetType;
				if( forNextRetType->isStructTy() ) {
					llvm::StructType* forOptStruct = llvm::cast<llvm::StructType>( forNextRetType );
					if( forOptStruct->getNumElements() == 2 && forOptStruct->getElementType( 0 )->isIntegerTy( 1 ) ) {
						forElemType = forOptStruct->getElementType( 1 );
						forIsOptional = true;
					}
				}
				llvm::Value* forIteratorVarAlloca = this->createEntryBlockAllocation( forFunction, forStatement.variable, forElemType );
				this->namedValues[forStatement.variable] = forIteratorVarAlloca;
				llvm::BasicBlock* forIteratorConditionBasicBlock = llvm::BasicBlock::Create( this->context, "iter.cond", forFunction );
				llvm::BasicBlock* forIteratorBodyBasicBlock = llvm::BasicBlock::Create( this->context, "iter.body", forFunction );
				llvm::BasicBlock* forIteratorIncBasicBlock = llvm::BasicBlock::Create( this->context, "iter.inc", forFunction );
				llvm::BasicBlock* forIteratorEndBasicBlock = llvm::BasicBlock::Create( this->context, "iter.end", forFunction );
				this->builder.CreateBr( forIteratorConditionBasicBlock );
				this->builder.SetInsertPoint( forIteratorConditionBasicBlock );
				llvm::Value* forHasNextRes = this->builder.CreateCall( forHasNextFn, {forObjPointer}, "has" );
				llvm::Value* forIteratorConditionValue = forHasNextRes;
				if( forIteratorConditionValue->getType()->isIntegerTy( 1 ) == false ) {
					if( forIteratorConditionValue->getType()->isStructTy() ) {
						forIteratorConditionValue = this->builder.CreateExtractValue( forIteratorConditionValue, 0, "struct.scalar" );
					}
					forIteratorConditionValue = this->builder.CreateICmpNE( forIteratorConditionValue, llvm::Constant::getNullValue( forIteratorConditionValue->getType() ), "tobool" );
				}
				this->builder.CreateCondBr( forIteratorConditionValue, forIteratorBodyBasicBlock, forIteratorEndBasicBlock );
				this->builder.SetInsertPoint( forIteratorBodyBasicBlock );
				llvm::Value* forNextRes = this->builder.CreateCall( forNextFn, {forObjPointer}, "next" );
				llvm::Value* forElemValue = forNextRes;
				if( forIsOptional ) {
					forElemValue = this->builder.CreateExtractValue( forNextRes, 1, "unwrap" );
				}
				this->builder.CreateStore( forElemValue, forIteratorVarAlloca );
				this->breakTargets.push( forIteratorEndBasicBlock );
				this->continueTargets.push( forIteratorIncBasicBlock );
				this->pushDeferScope();
				for( ast::nodes::StatementSharedPointer forIteratorStatement : forStatement.body ) {
					this->generateStatement( forIteratorStatement );
				}
				this->emitCurrentScopeDefers();
				this->popDeferScope();
				this->continueTargets.pop();
				this->breakTargets.pop();
				if( this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
					this->builder.CreateBr( forIteratorIncBasicBlock );
				}
				this->builder.SetInsertPoint( forIteratorIncBasicBlock );
				this->builder.CreateBr( forIteratorConditionBasicBlock );
				this->builder.SetInsertPoint( forIteratorEndBasicBlock );
				return;
			}
		}
	}
	
	void LLVMCodegen::generateFunctionDeclaration( ast::nodes::FunctionDeclaration& declaration ) {
		if( declaration.isNative ) {
			return;
		}
		// Build parameter type signature for overload-aware mangling
		std::string paramTypeSignature;
		bool hasOverloadKey = false;
		{
			std::unordered_map<std::string, int>::const_iterator overloadCountIterator = this->overloadedFunctionCounts.find( declaration.name );
			if( overloadCountIterator != this->overloadedFunctionCounts.end() && overloadCountIterator->second > 1 ) {
				hasOverloadKey = true;
			}
		}
		if( hasOverloadKey ) {
			for( ast::nodes::FunctionParameterSharedPointer& functionParameter : declaration.parameters ) {
				if( functionParameter->isSelf ) continue;
				if( paramTypeSignature.empty() == false ) {
					paramTypeSignature += ",";
				}
				if( functionParameter->type ) {
					if( functionParameter->type->kind == ast::Node::Kind::SimpleType ) {
						paramTypeSignature += static_cast<ast::nodes::SimpleTypeNode&>( *functionParameter->type ).name;
					}
					else if( functionParameter->type->kind == ast::Node::Kind::GenericType ) {
						ast::nodes::GenericTypeNode& genericNode = static_cast<ast::nodes::GenericTypeNode&>( *functionParameter->type );
						paramTypeSignature += genericNode.name;
						if( genericNode.typeArguments.empty() == false ) {
							paramTypeSignature += "<";
							for( size_t gtIdx = 0; gtIdx < genericNode.typeArguments.size(); gtIdx++ ) {
								if( gtIdx > 0 ) paramTypeSignature += ",";
								if( genericNode.typeArguments[gtIdx]->kind == ast::Node::Kind::SimpleType ) {
									paramTypeSignature += static_cast<ast::nodes::SimpleTypeNode&>( *genericNode.typeArguments[gtIdx] ).name;
								}
							}
							paramTypeSignature += ">";
						}
					}
					else if( functionParameter->type->kind == ast::Node::Kind::ArrayType ) {
						paramTypeSignature += "Array";
					}
					else if( functionParameter->type->kind == ast::Node::Kind::OptionalType ) {
						paramTypeSignature += "Optional";
					}
					else {
						paramTypeSignature += "unknown";
					}
				}
				else {
					paramTypeSignature += "I64";
				}
			}
		}
		std::string overloadRegistrationKey = hasOverloadKey ? fmt::format( "{}#{}", declaration.name, paramTypeSignature ) : declaration.name;
		std::string expectedMangleName( declaration.name == "main" ? "main" : this->mangleName( overloadRegistrationKey ) );
		llvm::Function* function = this->module->getFunction( expectedMangleName );
		if( function == nullptr && this->functions.count( overloadRegistrationKey ) ) {
			llvm::Function* existing = this->functions[overloadRegistrationKey];
			if( existing->getName() == expectedMangleName ) {
				function = existing;
			}
		}
		if( function == nullptr && this->functions.count( declaration.name ) ) {
			llvm::Function* existing = this->functions[declaration.name];
			std::string baseMangleName( declaration.name == "main" ? "main" : this->mangleName( declaration.name ) );
			if( existing->getName() == baseMangleName && hasOverloadKey == false ) {
				function = existing;
			}
		}
		if( function == nullptr ) {
			llvm::Type* functionReturnType = this->resolveAstType( declaration.returnType );
			if( declaration.isAsync ) {
				functionReturnType = llvm::Type::getInt64Ty( this->context );
			}
			std::vector<llvm::Type*> functionParameterTypes;
			FunctionParamInfo paramInfo;
			int nonSelfParamIndex = 0;
			bool seenVariadic = false;
			for( ast::nodes::FunctionParameterSharedPointer& functionParameter : declaration.parameters ) {
				if( functionParameter->isSelf ) continue;
				if( functionParameter->isVariadic ) {
					seenVariadic = true;
					llvm::Type* elemType = llvm::Type::getInt64Ty( this->context );
					if( functionParameter->type && functionParameter->type->kind == ast::Node::Kind::ArrayType ) {
						ast::nodes::ArrayTypeNode& arrayNode = static_cast<ast::nodes::ArrayTypeNode&>( *functionParameter->type );
						if( arrayNode.elementType ) {
							elemType = this->resolveAstType( arrayNode.elementType );
						}
					}
					else if( functionParameter->type ) {
						elemType = this->resolveAstType( functionParameter->type );
					}
					llvm::StructType* argsStruct = llvm::StructType::get( this->context, {
						llvm::PointerType::getUnqual( elemType ),
						llvm::Type::getInt64Ty( this->context ),
						llvm::Type::getInt64Ty( this->context )
					});
					functionParameterTypes.push_back( llvm::PointerType::getUnqual( argsStruct ) );
					paramInfo.variadicIndex = nonSelfParamIndex;
					paramInfo.variadicElementLLVMType = elemType;
				}
				else if( functionParameter->isKeyword ) {
					llvm::Type* valType = llvm::Type::getInt64Ty( this->context );
					if( functionParameter->type ) {
						valType = this->resolveAstType( functionParameter->type );
					}
					llvm::StructType* kwargsStruct = llvm::StructType::get( this->context, {
						llvm::PointerType::getUnqual( llvm::PointerType::getUnqual( this->context ) ),
						llvm::PointerType::getUnqual( valType ),
						llvm::Type::getInt64Ty( this->context ),
						llvm::Type::getInt64Ty( this->context )
					});
					functionParameterTypes.push_back( llvm::PointerType::getUnqual( kwargsStruct ) );
					paramInfo.keywordIndex = nonSelfParamIndex;
					paramInfo.keywordValueLLVMType = valType;
				}
				else if( functionParameter->type ) {
					llvm::Type* functionParameterLLVMType = this->resolveAstType( functionParameter->type );
					if( functionParameterLLVMType->isStructTy() && functionParameterLLVMType != this->getInterfaceFatPointerType() ) {
						functionParameterLLVMType = llvm::PointerType::getUnqual( functionParameterLLVMType );
					}
					functionParameterTypes.push_back( functionParameterLLVMType );
					if( seenVariadic && functionParameter->defaultValue ) {
						KeywordOnlyParam kwOnly;
						kwOnly.name = functionParameter->name;
						kwOnly.llvmType = functionParameterLLVMType;
						kwOnly.defaultValue = functionParameter->defaultValue;
						paramInfo.keywordOnlyParams.push_back( kwOnly );
					}
				}
				else {
					functionParameterTypes.push_back( llvm::Type::getInt64Ty( this->context ) );
				}
				nonSelfParamIndex++;
			}
			std::string mangledName( this->mangleName( overloadRegistrationKey ) );
			llvm::FunctionType* functionLLVMType = llvm::FunctionType::get( functionReturnType, functionParameterTypes, false );
			function = llvm::Function::Create(
				functionLLVMType,
				llvm::Function::InternalLinkage,
				mangledName,
				this->getModule()
			);
			this->functions[overloadRegistrationKey] = function;
			this->functionParamInfos[overloadRegistrationKey] = paramInfo;
		}
		if( declaration.body.empty() && declaration.isAbstract ) {
			return;
		}

		// Skip if function body already generated (duplicate from module merging)
		if( function->empty() == false ) {
			return;
		}
		function->setPersonalityFn( this->getOrCreatePersonalityFunction() );
		
		// Generator function: generate state machine
		if( declaration.isGenerator ) {
			llvm::Type* yieldType = this->resolveAstType( declaration.returnType );
			if( yieldType == nullptr || yieldType->isVoidTy() ) {
				yieldType = llvm::Type::getInt64Ty( this->context );
			}
			
			// Collect non-self parameter types for storage in generator struct
			std::vector<llvm::Type*> functionParameterLLVMTypes;
			std::vector<std::string> functionParameterNames;
			for( ast::nodes::FunctionParameterSharedPointer& functionParameter : declaration.parameters ) {
				if( functionParameter->isSelf) {
					continue;
				}
				llvm::Type* functionParameterLLVMType = llvm::Type::getInt64Ty( this->context );;
				if( functionParameter->type ) {
					functionParameterLLVMType = this->resolveAstType( functionParameter->type );
				}
				if( functionParameterLLVMType->isStructTy() &&
					functionParameterLLVMType != this->getInterfaceFatPointerType() ) {
					functionParameterLLVMType = llvm::PointerType::getUnqual( functionParameterLLVMType );
				}
				functionParameterLLVMTypes.push_back( functionParameterLLVMType );
				functionParameterNames.push_back( functionParameter->name );
			}
			
			// Generator struct: {i32 state, T value, i1 done, P1, P2, ...}
			std::vector<llvm::Type*> structFields = { llvm::Type::getInt32Ty( this->context ), yieldType, llvm::Type::getInt1Ty( this->context ) };
			for( llvm::Type* functionParameterLLVMType : functionParameterLLVMTypes ) {
				structFields.push_back( functionParameterLLVMType );
			}
			llvm::StructType* generatorStructType = llvm::StructType::get( this->context, structFields );
			
			// Create next function: void _next(generatorStruct*)
			std::string nextFunctionName( fmt::format( "{}.next", mangleName( declaration.name ) ) );
			llvm::Function* nextFunction = llvm::Function::Create(
				llvm::FunctionType::get(
					llvm::Type::getVoidTy( this->context ),
					{
						llvm::PointerType::getUnqual( generatorStructType )
					},
					false
				),
				llvm::Function::InternalLinkage,
				nextFunctionName,
				this->getModule()
			);
			nextFunction->setPersonalityFn( this->getOrCreatePersonalityFunction() );
			std::unordered_map<std::string,llvm::Value*> savedValues = this->namedValues;
			llvm::Function* savedFunction = this->currentFunction;
			llvm::BasicBlock* savedBlock = this->builder.GetInsertBlock();
			
			this->builder.SetInsertPoint( llvm::BasicBlock::Create( this->context, "entry", nextFunction ) );
			this->currentFunction = nextFunction;
			this->namedValues.clear();
			this->variableStructType.clear();
			this->varMemoryElementTypes.clear();
			this->arenaElementTypes.clear();

			llvm::Function::arg_iterator generatorPointer = nextFunction->arg_begin();
			generatorPointer->setName( "gen" );
			llvm::Value* statePointer = this->builder.CreateStructGEP( generatorStructType, generatorPointer, 0, "state.ptr" );
			llvm::Value* valuePointer = this->builder.CreateStructGEP( generatorStructType, generatorPointer, 1, "value.ptr" );
			llvm::Value* donePointer = this->builder.CreateStructGEP( generatorStructType, generatorPointer, 2, "done.ptr" );
			
			// Load parameters from generator struct into local allocas
			for( size_t i=0; i<functionParameterNames.size(); i++ ) {
				std::string functionParameterPointerName( fmt::format( "{}.ptr", functionParameterNames[i] ) );
				llvm::Value* functionParameterPointer = this->builder.CreateStructGEP(generatorStructType, generatorPointer, 3 + i, functionParameterPointerName );
				llvm::AllocaInst* functionParameterAlloca = this->createEntryBlockAllocation( nextFunction, functionParameterNames[i], functionParameterLLVMTypes[i] );
				llvm::LoadInst* functionParameterValue = this->builder.CreateLoad( functionParameterLLVMTypes[i], functionParameterPointer, functionParameterNames[i] );
				this->builder.CreateStore( functionParameterValue, functionParameterAlloca );
				this->namedValues[functionParameterNames[i]] = functionParameterAlloca;
			}
			
			GeneratorContext generatorContext;
			generatorContext.stateVar = this->createEntryBlockAllocation( nextFunction, "state.local", llvm::Type::getInt32Ty( this->context ) );
			generatorContext.valueVar = this->createEntryBlockAllocation( nextFunction, "value.local", yieldType );
			generatorContext.doneVar = this->createEntryBlockAllocation( nextFunction, "done.local", llvm::Type::getInt1Ty( this->context ) );
			generatorContext.prefix = this->mangleName( declaration.name );
			generatorContext.nextStateId = 1;
			
			llvm::LoadInst* loadedState = this->builder.CreateLoad( llvm::Type::getInt32Ty( this->context ), statePointer, "state" );
			this->builder.CreateStore( loadedState, generatorContext.stateVar );
			this->builder.CreateStore( llvm::ConstantInt::getFalse( this->context ), generatorContext.doneVar );
			
			GeneratorContext* savedGenerator = this->currentGenerator;
			this->currentGenerator = &generatorContext;
			
			// Create dispatch switch for resume points
			llvm::BasicBlock* bodyBasicBlock = llvm::BasicBlock::Create( this->context, "body.start", nextFunction );
			llvm::BasicBlock* doneBasicBlock = llvm::BasicBlock::Create( this->context, "done", nextFunction );
			llvm::BasicBlock* exitBasicBlock = llvm::BasicBlock::Create( this->context, "exit", nextFunction );
			
			generatorContext.exitBB = exitBasicBlock;
			
			llvm::SwitchInst* switchInst = this->builder.CreateSwitch( loadedState, doneBasicBlock, 16 );
			switchInst->addCase( llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->context ), 0 ), bodyBasicBlock );
			
			// Done block: set done=true
			this->builder.SetInsertPoint( doneBasicBlock );
			this->builder.CreateStore( llvm::ConstantInt::getTrue( this->context ), generatorContext.doneVar );
			this->builder.CreateBr( exitBasicBlock );
			
			// Body block
			this->builder.SetInsertPoint( bodyBasicBlock );
			
			// Snapshot param names before body codegen — these aren't locals
			std::unordered_set<std::string> functionParameterSet( functionParameterNames.begin(), functionParameterNames.end() );
			
			for( ast::nodes::StatementSharedPointer& statement : declaration.body ) {
				this->generateStatement( statement );
			}
			
			// After all body: mark done
			if( this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
				this->builder.CreateStore( llvm::ConstantInt::getTrue( this->context ), generatorContext.doneVar );
				this->builder.CreateBr( exitBasicBlock );
			}
			
			// Collect local variables created during body codegen
			// These need global storage to persist across _next calls
			std::vector<std::pair<std::string, llvm::AllocaInst*>> localVariables;
			for( std::pair<std::string,llvm::Value*> pair : this->namedValues ) {
				if( functionParameterSet.count( pair.first ) ) continue;
				if( llvm::AllocaInst* alloca = llvm::dyn_cast<llvm::AllocaInst>( pair.second ) ) {
					localVariables.push_back({ pair.first, alloca });
				}
			}
			
			// Create globals for persisting local vars
			for( std::pair<std::string,llvm::AllocaInst*> pair : localVariables ) {
				std::string name( pair.first );
				std::string globalVariableName( fmt::format( "{}.local.{}", generatorContext.prefix, name ) );
				llvm::AllocaInst* alloca = pair.second;
				llvm::GlobalVariable* globalVariable = new llvm::GlobalVariable( *this->getModule(),
					alloca->getAllocatedType(),
					false,
					llvm::GlobalValue::InternalLinkage,
					llvm::Constant::getNullValue(
						alloca->getAllocatedType()
					),
					globalVariableName
				);
				generatorContext.persistedLocals[name] = globalVariable;
			}
			
			// Wire up resume points to the switch
			// Also add restore-from-globals blocks for each resume point
			for( llvm::BasicBlock& nextFunctionBasicBlock : *nextFunction ) {
				if( nextFunctionBasicBlock.getName().starts_with( "resume." ) ) {
					std::string stateIdStr( nextFunctionBasicBlock.getName().substr( 7 ).str() );
					int stateId = std::stoi( stateIdStr );
					
					// Create a restore block before the resume point
					std::string restoreBasicBlockName( fmt::format( "restore.{}", std::to_string( stateId ) ) );
					llvm::BasicBlock* restoreBasicBlock = llvm::BasicBlock::Create( this->context, restoreBasicBlockName, nextFunction );
					switchInst->addCase( llvm::ConstantInt::get(llvm::Type::getInt32Ty( this->context ), stateId), restoreBasicBlock );
					
					this->builder.SetInsertPoint( restoreBasicBlock );
					
					// Load all locals from globals
					for( std::pair<std::string,llvm::AllocaInst*> pair : localVariables ) {
						std::string name( pair.first );
						std::string globalVariableName( fmt::format( "restore.{}", name ) );
						llvm::AllocaInst* alloca = pair.second;
						llvm::GlobalVariable* globalVariable = generatorContext.persistedLocals[name];
						llvm::LoadInst* value = this->builder.CreateLoad( alloca->getAllocatedType(), globalVariable, globalVariableName );
						this->builder.CreateStore( value, alloca );
					}
					this->builder.CreateBr( &nextFunctionBasicBlock );
				}
			}
			
			// Exit block: save locals to globals, then write back state/value/done
			this->builder.SetInsertPoint( exitBasicBlock );
			
			// Save all local vars to globals
			for( std::pair<std::string,llvm::AllocaInst*> pair : localVariables ) {
				std::string name( pair.first );
				std::string globalVariableName( fmt::format( "save.{}", name ) );
				llvm::AllocaInst* alloca = pair.second;
				llvm::GlobalVariable* globalVariable = generatorContext.persistedLocals[name];
				llvm::LoadInst* globalVariableValue = this->builder.CreateLoad(alloca->getAllocatedType(), alloca, globalVariableName );
				this->builder.CreateStore( globalVariableValue, globalVariable );
			}
			
			llvm::LoadInst* finalState = this->builder.CreateLoad( llvm::Type::getInt32Ty( this->context ), generatorContext.stateVar, "final.state" );
			this->builder.CreateStore( finalState, statePointer );
			llvm::LoadInst* finalValue = this->builder.CreateLoad( yieldType, generatorContext.valueVar, "final.value" );
			this->builder.CreateStore( finalValue, valuePointer );
			llvm::LoadInst* finalDone = this->builder.CreateLoad( llvm::Type::getInt1Ty( this->context ), generatorContext.doneVar, "final.done" );
			this->builder.CreateStore(finalDone, donePointer);
			this->builder.CreateRetVoid();
			
			this->currentGenerator = savedGenerator;
			
			// Init function: allocate gen struct, store params, init state, call next, return struct
			this->currentFunction = savedFunction;
			this->namedValues = savedValues;
			
			if( function->empty() == false ) {
				if( savedBlock ) {
					this->builder.SetInsertPoint( savedBlock );
				}
				return;
			}
			
			this->builder.SetInsertPoint( llvm::BasicBlock::Create( this->context, "entry", function ) );
			this->currentFunction = function;
			
			llvm::AllocaInst* generatorAlloca = this->builder.CreateAlloca( generatorStructType, nullptr, "gen.struct" );
			llvm::Value* initStatePointer = this->builder.CreateStructGEP( generatorStructType, generatorAlloca, 0, "init.state" );
			this->builder.CreateStore( llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->context ), 0 ), initStatePointer );
			llvm::Value* initDonePointer = this->builder.CreateStructGEP( generatorStructType, generatorAlloca, 2, "init.done" );
			this->builder.CreateStore( llvm::ConstantInt::getFalse( this->context ), initDonePointer);
			
			// Store parameters into generator struct
			size_t argumentIndex = 0;
			for( llvm::Argument& argumentLLVM : function->args() ) {
				if( argumentIndex < functionParameterNames.size() ) {
					std::string argumentName( fmt::format( "init.{}", functionParameterNames[argumentIndex] ) );
					llvm::Value* pPointer = this->builder.CreateStructGEP( generatorStructType, generatorAlloca, 3 + argumentIndex, argumentName );
					this->builder.CreateStore( &argumentLLVM, pPointer );
				}
				argumentIndex++;
			}
			
			// Call next once to get first value
			this->builder.CreateCall( nextFunction, { generatorAlloca } );
			
			llvm::LoadInst* resultValue = this->builder.CreateLoad( generatorStructType, generatorAlloca, "gen.result" );
			this->builder.CreateRet( resultValue );
			
			// Store next function for iteration
			this->functions[fmt::format( "{}.next", declaration.name )] = nextFunction;
			
			this->namedValues = savedValues;
			this->currentFunction = savedFunction;
			
			if( savedBlock ) {
				this->builder.SetInsertPoint( savedBlock );
			}
			return;
		}
		
		// Async function: create task wrapper, spawn via scheduler
		if( declaration.isAsync && declaration.name != "main" ) {
			
			std::unordered_map<std::string,llvm::Value*> savedValues = this->namedValues;
			llvm::Function* savedFunction = this->currentFunction;
			llvm::BasicBlock* savedInsertPoint = this->builder.GetInsertBlock();
			
			// Collect param types for args struct
			std::vector<llvm::Type*> argumentFieldTypes;
			std::vector<std::string> argumentNames;
			for( ast::nodes::FunctionParameterSharedPointer& functionParameter : declaration.parameters ) {
				if( functionParameter->isSelf ) {
					continue;
				}
				llvm::Type* functionParameterType = llvm::Type::getInt64Ty( this->context );
				if( functionParameter->type ) {
					functionParameterType = this->resolveAstType( functionParameter->type );
				}
				if( functionParameterType->isStructTy() &&
					functionParameterType != this->getInterfaceFatPointerType() ) {
					functionParameterType = llvm::PointerType::getUnqual( functionParameterType );
				}
				argumentFieldTypes.push_back( functionParameterType );
				argumentNames.push_back( functionParameter->name );
			}
			
			// Create args struct type
			// llvm::StructType* argumentsStructType = nullptr;
			// if( argumentFieldTypes.empty() == false ) {
			// 	argumentsStructType = llvm::StructType::get( this->context, argumentFieldTypes );
			// }
			
			// Resolve actual return type (inner type, not i64 task ID)
			llvm::Type* innerFunctionReturnType = this->resolveAstType( declaration.returnType );
			if( innerFunctionReturnType == nullptr || innerFunctionReturnType->isVoidTy() ) {
				innerFunctionReturnType = llvm::Type::getInt64Ty( this->context );
			}
			
			// Create task wrapper: void _async_name(UraniteTask*)
			std::string wrapperName( fmt::format( "{}.async", mangleName( declaration.name ) ) );
			llvm::PointerType* wrapperTaskPointerType = llvm::PointerType::getUnqual( this->context );
			llvm::FunctionType* wrapperType = llvm::FunctionType::get( llvm::Type::getVoidTy( this->context ), { wrapperTaskPointerType }, false );
			llvm::Function* wrapperFunction = llvm::Function::Create( wrapperType, llvm::Function::InternalLinkage, wrapperName,this->getModule() );
			
			// Pre-create globals for arg passing
			std::vector<llvm::GlobalVariable*> argumentGlobals;
			for( size_t i=0; i<argumentNames.size(); i++ ) {
				std::string globalVariableName( fmt::format( "{}.arg.{}", mangleName( declaration.name ), argumentNames[i] ) );
				llvm::GlobalVariable* globalVariable = new llvm::GlobalVariable( *this->getModule(),
					argumentFieldTypes[i],
					false,
					llvm::GlobalValue::InternalLinkage,
					llvm::Constant::getNullValue( argumentFieldTypes[i] ),
					globalVariableName
				);
				argumentGlobals.push_back( globalVariable );
			}
			
			llvm::BasicBlock* wrapperBasicBlock = llvm::BasicBlock::Create( this->context, "entry", wrapperFunction );
			this->builder.SetInsertPoint( wrapperBasicBlock );
			this->currentFunction = wrapperFunction;
			this->namedValues.clear();
			this->variableStructType.clear();
			this->varMemoryElementTypes.clear();
			this->arenaElementTypes.clear();

			llvm::Function::arg_iterator taskArgument = wrapperFunction->arg_begin();
			taskArgument->setName( "task" );
			
			this->asyncTaskArgument = taskArgument;
			
			// Load args from globals (stored before spawn)
			for( size_t i=0; i<argumentNames.size(); i++ ) {
				llvm::GlobalVariable* globalVariable = argumentGlobals[i];
				llvm::Value* value = this->builder.CreateLoad( argumentFieldTypes[i], globalVariable, argumentNames[i] );
				llvm::AllocaInst* alloca = this->createEntryBlockAllocation( wrapperFunction, argumentNames[i], argumentFieldTypes[i] );
				this->builder.CreateStore( value, alloca );
				this->namedValues[argumentNames[i]] = alloca;
			}
			wrapperFunction->setPersonalityFn( this->getOrCreatePersonalityFunction() );
			
			// Create catch landing pad for implicit try/catch around async body
			llvm::BasicBlock* asyncCatchBasicBlock = llvm::BasicBlock::Create( this->context, "async.catch", wrapperFunction );
			this->landingPads.push( asyncCatchBasicBlock );
			
			// --- Async body: normal execution ---
			for( ast::nodes::StatementSharedPointer& statement : declaration.body ) {
				if( this->builder.GetInsertBlock()->getTerminator() ) {
					break;
				}
				this->generateStatement( statement );
			}
			this->landingPads.pop();
			
			// Normal completion: call runtimeComplete and return
			if( this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
				llvm::Function* completeFunction = this->module->getFunction( "runtimeComplete" );
				if( completeFunction != nullptr ) {
					this->builder.CreateCall(
						completeFunction,
						{
							llvm::ConstantInt::get(
								llvm::Type::getInt64Ty( this->context ),
								0
							)
						}
					);
				}
				else {
					completeFunction = this->module->getFunction( "uraniteTaskComplete" );
					if( completeFunction == nullptr ) {
						llvm::FunctionType* completeType = llvm::FunctionType::get(
							llvm::Type::getVoidTy( this->context ),
							{
								wrapperTaskPointerType,
								llvm::Type::getInt64Ty( this->context )
							},
							false
						);
						completeFunction = llvm::Function::Create( completeType, llvm::Function::ExternalLinkage, "uraniteTaskComplete", this->getModule() );
					}
					llvm::Value* completeTaskArg = taskArgument;
					if( completeFunction->getArg( 0 )->getType()->isIntegerTy() && taskArgument->getType()->isPointerTy() ) {
						completeTaskArg = this->builder.CreatePtrToInt( taskArgument, completeFunction->getArg( 0 )->getType(), "task.i64" );
					}
					this->builder.CreateCall(
						completeFunction,
						{
							completeTaskArg,
							llvm::ConstantInt::get(
								llvm::Type::getInt64Ty( this->context ),
								0
							)
						}
					);
				}
				this->builder.CreateRetVoid();
			}
			
			// --- Async catch: landingpad, extract exception, report error, return ---
			this->builder.SetInsertPoint( asyncCatchBasicBlock );
			{
				llvm::LandingPadInst* landingPad = this->builder.CreateLandingPad(
					llvm::StructType::get( this->context, { llvm::PointerType::getUnqual( this->context ), llvm::Type::getInt32Ty( this->context ) } ),
					1,
					"async.lp"
				);
				landingPad->addClause( llvm::ConstantPointerNull::get( llvm::PointerType::getUnqual( this->context ) ) );
				
				llvm::Value* exceptionPtr = this->builder.CreateExtractValue( landingPad, 0, "async.exc.ptr" );
				llvm::Value* uraniteObject = this->builder.CreateCall( this->getOrCreateUraniteBeginCatch(), { exceptionPtr }, "async.exc.obj" );
				
				llvm::Function* errorFunction = this->module->getFunction( "runtimeError" );
				if( errorFunction != nullptr ) {
					this->builder.CreateCall( errorFunction, { uraniteObject } );
				}
				else {
					errorFunction = this->module->getFunction( "uraniteTaskError" );
					if( errorFunction == nullptr ) {
						llvm::FunctionType* throwableType = llvm::FunctionType::get(
							llvm::Type::getVoidTy( this->context ),
							{
								wrapperTaskPointerType,
								llvm::PointerType::getUnqual( this->context )
							},
							false
						);
						errorFunction = llvm::Function::Create( throwableType, llvm::Function::ExternalLinkage, "uraniteTaskError", this->getModule() );
					}
					llvm::Value* errorTaskArg = taskArgument;
					if( errorFunction->getArg( 0 )->getType()->isIntegerTy() && taskArgument->getType()->isPointerTy() ) {
						errorTaskArg = this->builder.CreatePtrToInt( taskArgument, errorFunction->getArg( 0 )->getType(), "task.i64" );
					}
					this->builder.CreateCall( errorFunction, { errorTaskArg, uraniteObject } );
				}
				
				this->builder.CreateCall( this->getOrCreateUraniteEndCatch(), { exceptionPtr } );
				this->builder.CreateRetVoid();
			}
			
			this->asyncTaskArgument = nullptr;
			
			// Now generate the original function: packs args, spawns task, returns ID
			this->currentFunction = savedFunction;
			this->namedValues = savedValues;
			
			if( function->empty() == false ) {
				if( savedInsertPoint ) {
					this->builder.SetInsertPoint( savedInsertPoint );
				}
				return;
			}
			
			this->builder.SetInsertPoint( llvm::BasicBlock::Create( this->context, "entry", function ) );
			this->currentFunction = function;
			this->namedValues.clear();
			this->variableStructType.clear();
			this->varMemoryElementTypes.clear();
			this->arenaElementTypes.clear();

			// Register params
			size_t parameterIndex = 0;
			for( ast::nodes::FunctionParameterSharedPointer& functionParameter : declaration.parameters ) {
				if( functionParameter->isSelf ) continue;
				if( parameterIndex >= function->arg_size() ) {
					break;
				}
				llvm::Argument* functionArgument = function->getArg( parameterIndex );
				functionArgument->setName( functionParameter->name );
				llvm::AllocaInst* alloca = this->createEntryBlockAllocation( function, functionParameter->name, functionArgument->getType() );
				this->builder.CreateStore( functionArgument, alloca );
				this->namedValues[functionParameter->name] = alloca;
				parameterIndex++;
			}
			
			// Store args to globals before spawning
			for( size_t i = 0; i < argumentNames.size(); i++ ) {
				std::string argumentName( fmt::format( "{}.val", argumentNames[i] ) );
				this->builder.CreateStore( this->builder.CreateLoad( argumentFieldTypes[i], this->namedValues[argumentNames[i]], argumentName ), argumentGlobals[i] );
			}
			
			llvm::ConstantPointerNull* userDataValue = llvm::ConstantPointerNull::get( llvm::PointerType::getUnqual( this->context ) );
			
			// Spawn task
			llvm::Function* spawnFunction = this->module->getFunction( "runtimeSpawn" );
			if( spawnFunction != nullptr ) {
				llvm::Value* wrapperPointer = this->builder.CreatePtrToInt( wrapperFunction, llvm::Type::getInt64Ty( this->context ), "wrapper.i64" );
				llvm::Value* userDataI64 = llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->context ), 0 );
				llvm::CallInst* taskId = this->builder.CreateCall( spawnFunction, { wrapperPointer, userDataI64 }, "taskId" );
				this->builder.CreateRet( taskId );
				this->namedValues = savedValues;
				this->currentFunction = savedFunction;
				if( savedInsertPoint ) {
					this->builder.SetInsertPoint( savedInsertPoint );
				}
				return;
			}
			spawnFunction = this->module->getFunction( "uraniteSpawnTask" );
			if( spawnFunction == nullptr ) {
				llvm::FunctionType* spawnFunctionType = llvm::FunctionType::get(
					llvm::Type::getInt64Ty( this->context ),
					{
						llvm::PointerType::getUnqual( this->context ),
						llvm::PointerType::getUnqual( this->context )
					},
					false
				);
				spawnFunction = llvm::Function::Create( spawnFunctionType, llvm::Function::ExternalLinkage, "uraniteSpawnTask", this->getModule() );
			}
			
			llvm::Value* spawnArg0;
			llvm::Value* spawnArg1;
			if( spawnFunction->getArg( 0 )->getType()->isIntegerTy() ) {
				spawnArg0 = this->builder.CreatePtrToInt( wrapperFunction, spawnFunction->getArg( 0 )->getType(), "wrapper.i64" );
				spawnArg1 = llvm::ConstantInt::get( spawnFunction->getArg( 1 )->getType(), 0 );
			}
			else {
				spawnArg0 = this->builder.CreateBitCast( wrapperFunction, llvm::PointerType::getUnqual( this->context ), "wrapper.ptr" );
				spawnArg1 = userDataValue;
			}
			llvm::CallInst* taskId = this->builder.CreateCall( spawnFunction, { spawnArg0, spawnArg1 }, "taskId" );
			
			this->builder.CreateRet( taskId );
			this->namedValues = savedValues;
			this->currentFunction = savedFunction;
			if( savedInsertPoint ) {
				this->builder.SetInsertPoint( savedInsertPoint );
			}
			return;
		}
		
		// Save parent function state for nested functions
		std::unordered_map<std::string,llvm::Value*> savedValues = this->namedValues;
		std::unordered_map<std::string,llvm::Value*> savedAliveFlags = this->aliveFlags;
		llvm::Function* savedFunction = this->currentFunction;
		llvm::BasicBlock* savedInsertPoint = this->builder.GetInsertBlock();
		llvm::BasicBlock* basicBlock = llvm::BasicBlock::Create( this->context, "entry", function);
		
		this->builder.SetInsertPoint( basicBlock );
		this->currentFunction = function;
		this->namedValues.clear();
		this->aliveFlags.clear();
		this->variableStructType.clear();
		this->varMemoryElementTypes.clear();
		this->arenaElementTypes.clear();
		if( this->currentClassName.empty() == false ) {
			this->variableStructType["self"] = this->currentClassName;
		}

		// Register parameters (main uses C calling convention — wire user params from argc/argv)
		if( declaration.name == "main" && declaration.parameters.empty() == false ) {
			llvm::Type* i64Type = llvm::Type::getInt64Ty( this->context );
			for( ast::nodes::FunctionParameterSharedPointer& mainParam : declaration.parameters ) {
				if( mainParam->isSelf ) continue;
				if( mainParam->name == "argc" ) {
					llvm::Value* argcVal = this->builder.CreateSExt( function->getArg( 0 ), i64Type, "argc.ext" );
					llvm::AllocaInst* alloca = this->createEntryBlockAllocation( function, "argc", i64Type );
					this->builder.CreateStore( argcVal, alloca );
					this->namedValues["argc"] = alloca;
					this->variableStructType["argc"] = "I64";
				}
				else if( mainParam->name == "argv" ) {
					this->variableStructType["argv"] = "ArrayList";
				}
			}
		}
		size_t functionParameterIndex = 0;
		for( ast::nodes::FunctionParameterSharedPointer& fuctionParameter : declaration.parameters ) {
			if( fuctionParameter->isSelf ) continue;
			if( declaration.name == "main" ) continue;
			if( functionParameterIndex >= function->arg_size() ) break;
			
			llvm::Argument* functionArgument = function->getArg( functionParameterIndex );
			functionArgument->setName( fuctionParameter->name );
			
			llvm::AllocaInst* alloca = this->createEntryBlockAllocation(
				function,
				fuctionParameter->name,
				functionArgument->getType()
			);
			this->builder.CreateStore( functionArgument, alloca );
			this->namedValues[fuctionParameter->name] = alloca;
			
			// Track param type for method resolution
			if( fuctionParameter->type ) {
				std::string functionParameterTypeName;
				if( fuctionParameter->type->kind == ast::Node::Kind::SimpleType ) {
					functionParameterTypeName = static_cast<ast::nodes::SimpleTypeNode&>( *fuctionParameter->type ).name;
				}
				else if( fuctionParameter->type->kind == ast::Node::Kind::GenericType ) {
					ast::nodes::GenericTypeNode& genericTypeNode = static_cast<ast::nodes::GenericTypeNode&>( *fuctionParameter->type );
					functionParameterTypeName = genericTypeNode.name;
					if( genericTypeNode.name == "Memory" ) {
						llvm::Type* elementType = llvm::Type::getInt64Ty( this->context );
						if( genericTypeNode.typeArguments.empty() == false ) {
							elementType = this->resolveAstType( genericTypeNode.typeArguments[0] );
						}
						this->varMemoryElementTypes[fuctionParameter->name] = elementType;
					}
					if( genericTypeNode.name == "Arena" ) {
						llvm::Type* elementType = llvm::Type::getInt64Ty( this->context );
						if( genericTypeNode.typeArguments.empty() == false ) {
							elementType = this->resolveAstType( genericTypeNode.typeArguments[0] );
							elementType = unwrapStructPointer( elementType, genericTypeNode.typeArguments[0], this->structTypes );
						}
						this->arenaElementTypes[fuctionParameter->name] = elementType;
						this->namedValues[fuctionParameter->name] = functionArgument;
						functionParameterIndex++;
						continue;
					}
				}
				if( fuctionParameter->isVariadic ) {
					std::string elemName = "I64";
					if( fuctionParameter->type && fuctionParameter->type->kind == ast::Node::Kind::ArrayType ) {
						ast::nodes::ArrayTypeNode& arrayNode = static_cast<ast::nodes::ArrayTypeNode&>( *fuctionParameter->type );
						if( arrayNode.elementType && arrayNode.elementType->kind == ast::Node::Kind::SimpleType ) {
							elemName = static_cast<ast::nodes::SimpleTypeNode&>( *arrayNode.elementType ).name;
						}
					}
					semantic::TypeSharedPointer elemSemaType = this->analyzer.types().lookupType( elemName );
					std::string monoName = fmt::format( "Args<{}>", elemSemaType ? elemSemaType->toString() : elemName );
					functionParameterTypeName = monoName;
				}
				else if( fuctionParameter->isKeyword ) {
					std::string valName = "I64";
					if( fuctionParameter->type && fuctionParameter->type->kind == ast::Node::Kind::SimpleType ) {
						valName = static_cast<ast::nodes::SimpleTypeNode&>( *fuctionParameter->type ).name;
					}
					semantic::TypeSharedPointer valSemaType = this->analyzer.types().lookupType( valName );
					std::string monoName = fmt::format( "Kwargs<{}>", valSemaType ? valSemaType->toString() : valName );
					functionParameterTypeName = monoName;
				}
				if( functionParameterTypeName.empty() == false ) {
					this->variableStructType[fuctionParameter->name] = functionParameterTypeName;
				}
			}
			functionParameterIndex++;
		}
		
		// Initialize global variables
		if( declaration.name == "main" && this->globalVariableInits.empty() == false ) {
			llvm::Function* savedFn = this->currentFunction;
			for( ast::nodes::ConstantDeclaration* globalDecl : this->globalVariableInits ) {
				llvm::GlobalVariable* globalPtr = this->module->getGlobalVariable( globalDecl->name, true );
				if( globalPtr != nullptr && globalDecl->initializer ) {
					this->currentFunction = function;
					llvm::Value* initVal = this->generateExpression( globalDecl->initializer );
					if( initVal != nullptr ) {
						this->builder.CreateStore( initVal, globalPtr );
					}
				}
			}
			this->currentFunction = savedFn;
		}
		
		// Populate cli argc/argv globals from C main parameters
		if( declaration.name == "main" && function->arg_size() >= 2 ) {
			llvm::Type* i8PtrType = llvm::PointerType::getUnqual( this->context );
			llvm::Type* i64Type = llvm::Type::getInt64Ty( this->context );
			llvm::Value* argcValue = this->builder.CreateSExt( function->getArg( 0 ), i64Type, "argc.ext" );
			llvm::Value* argvRaw = function->getArg( 1 );
			llvm::GlobalVariable* argcGlobal = this->module->getGlobalVariable( "argc", true );
			if( argcGlobal ) {
				this->builder.CreateStore( argcValue, argcGlobal );
			}
			llvm::GlobalVariable* argvGlobal = this->module->getGlobalVariable( "argv", true );
			if( argvGlobal ) {
				llvm::Type* argvValueType = argvGlobal->getValueType();
				llvm::StructType* listStructType = nullptr;
				if( argvValueType->isStructTy() ) {
					listStructType = llvm::cast<llvm::StructType>( argvValueType );
				}
				else if( argvValueType->isPointerTy() ) {
					std::unordered_map<std::string, llvm::StructType*>::iterator argvStructIt = this->structTypes.find( "ArrayList" );
					if( argvStructIt != this->structTypes.end() ) {
						listStructType = argvStructIt->second;
					}
				}
				if( listStructType ) {
					if( listStructType->getNumElements() >= 3 ) {
						llvm::Value* listPtr = this->builder.CreateLoad( argvValueType, argvGlobal, "argv.list" );
						llvm::Value* ptrSize = llvm::ConstantInt::get( i64Type, this->module->getDataLayout().getPointerSize() );
						llvm::Value* totalBytes = this->builder.CreateMul( argcValue, ptrSize, "argv.bytes" );
						llvm::FunctionCallee mallocFn = this->module->getOrInsertFunction( "malloc", llvm::FunctionType::get( i8PtrType, { i64Type }, false ) );
						llvm::Value* rawBuf = this->builder.CreateCall( mallocFn, { totalBytes }, "argv.buf" );
						llvm::Type* strPtrPtrType = llvm::PointerType::getUnqual( i8PtrType );
						llvm::Value* typedBuf = this->builder.CreateBitCast( rawBuf, strPtrPtrType, "argv.typed" );
						llvm::Value* srcI8 = this->builder.CreateBitCast( argvRaw, i8PtrType, "argv.src" );
						llvm::Value* dstI8 = rawBuf;
						this->builder.CreateCall( this->getOrCreateMemoryCopy(), { dstI8, srcI8, totalBytes, this->builder.getInt1( false ) } );
						llvm::Value* dataGep = this->builder.CreateStructGEP( listStructType, listPtr, 0, "argv.data.ptr" );
						llvm::Value* countGep = this->builder.CreateStructGEP( listStructType, listPtr, 1, "argv.count.ptr" );
						llvm::Value* capGep = this->builder.CreateStructGEP( listStructType, listPtr, 2, "argv.cap.ptr" );
						llvm::Type* dataFieldType = listStructType->getElementType( 0 );
						llvm::Value* dataCast = typedBuf;
						if( typedBuf->getType() != dataFieldType ) {
							dataCast = this->builder.CreateBitCast( typedBuf, dataFieldType, "argv.data.cast" );
						}
						this->builder.CreateStore( dataCast, dataGep );
						this->builder.CreateStore( argcValue, countGep );
						this->builder.CreateStore( argcValue, capGep );
					}
				}
			}
		}
		
		// Wire main's user-declared argv param to the populated global
		if( declaration.name == "main" && this->namedValues.find( "argv" ) == this->namedValues.end() ) {
			for( ast::nodes::FunctionParameterSharedPointer& mainParam : declaration.parameters ) {
				if( mainParam->name == "argv" ) {
					llvm::GlobalVariable* argvGlobal = this->module->getGlobalVariable( "argv", true );
					if( argvGlobal ) {
						this->namedValues["argv"] = argvGlobal;
					}
					break;
				}
			}
		}
		
		// Inject scheduler init for main when async is used
		if( declaration.name == "main" && this->programUsesAsync ) {
			this->builder.CreateCall( this->getOrCreateSchedulerInit() );
		}
		
		// Generate nested functions first
		for( ast::nodes::DeclarationSharedPointer& nested : declaration.nestedFunctions ) {
			this->generateDeclaration( nested );
		}
		
		// Restore insertion point after nested functions
		if( function->empty() == false ) {
			this->builder.SetInsertPoint(&function->back());
		}
		
		// Pre-scan for defer statements and heap allocations to set up cleanup landing pad
		bool hasDeferStatements = false;
		bool hasHeapAllocations = false;
		for( ast::nodes::StatementSharedPointer& stmt : declaration.body ) {
			if( stmt->kind == ast::Node::Kind::DeferStatement ) {
				hasDeferStatements = true;
			}
			if( stmt->kind == ast::Node::Kind::VariableStatement ) {
				ast::nodes::VariableStatement& varStmt = static_cast<ast::nodes::VariableStatement&>( *stmt );
				if( varStmt.ownershipAnnotation && varStmt.ownershipAnnotation->isHeapAllocated &&
					varStmt.ownershipAnnotation->isArenaManaged == false && varStmt.ownershipAnnotation->isMoved == false ) {
					hasHeapAllocations = true;
				}
			}
		}
		llvm::BasicBlock* cleanupPadBlock = nullptr;
		if( hasDeferStatements || hasHeapAllocations ) {
			cleanupPadBlock = llvm::BasicBlock::Create( this->context, "cleanup.pad", function );
			this->landingPads.push( cleanupPadBlock );
		}
		
		// Push frame for call stack tracing (skip for functions without explicit throw
		// statements — compiler-generated arithmetic checks embed source info directly)
		bool bodyHasThrowStatement = false;
		for( ast::nodes::StatementSharedPointer& stmt : declaration.body ) {
			if( stmt->kind == ast::Node::Kind::ThrowStatement ) {
				bodyHasThrowStatement = true;
				break;
			}
		}
		this->currentFunctionEmittedPushFrame = false;
		if( ( bodyHasThrowStatement || declaration.name == "main" ) && declaration.source && declaration.source->location ) {
			std::string displayName = declaration.name;
			if( this->currentClassName.empty() == false ) {
				displayName = fmt::format( "{}.{}", this->currentClassName, declaration.name );
			}
			this->emitPushFrame( declaration.source->filename, declaration.source->location->line, declaration.source->location->column, displayName );
			this->currentFunctionEmittedPushFrame = true;
		}

		// Generate body
		this->pushCleanupScope();
		this->pushDeferScope();
		for( ast::nodes::StatementSharedPointer& statement : declaration.body ) {
			if( this->builder.GetInsertBlock()->getTerminator() ) {
				break;
			}
			this->generateStatement( statement );
		}
		if( hasDeferStatements || hasHeapAllocations ) {
			this->landingPads.pop();
		}
		
		// Add implicit return if needed
		llvm::BasicBlock* lastBasicBlock = this->builder.GetInsertBlock();
		if( lastBasicBlock && lastBasicBlock->getTerminator() == nullptr ) {
			
			// Pop frame from call stack before implicit return
			if( this->currentFunctionEmittedPushFrame ) {
				this->emitPopFrame();
			}
			
			// Emit auto-free cleanup before implicit return
			this->emitAllScopeCleanups();
			
			// Emit deferred statements before implicit return
			this->emitAllDefers();
			if (function->getReturnType()->isVoidTy()) {
				this->builder.CreateRetVoid();
			}
			else {
				this->builder.CreateRet(llvm::Constant::getNullValue(function->getReturnType()));
			}
		}
		
		// Save cleanup entries before popping (needed for landing pad body)
		std::vector<ScopeCleanupEntry> savedCleanupEntries;
		if( this->scopeCleanupStack.empty() == false ) {
			savedCleanupEntries = this->scopeCleanupStack.back();
			this->scopeCleanupStack.pop_back();
		}
		
		// Generate cleanup landing pad for defer/autofree during unwinding
		if( ( hasDeferStatements || hasHeapAllocations ) && cleanupPadBlock != nullptr ) {
			this->builder.SetInsertPoint( cleanupPadBlock );
			llvm::StructType* lpResultType = llvm::StructType::get( this->context, {
				llvm::PointerType::getUnqual( this->context ),
				llvm::Type::getInt32Ty( this->context )
			} );
			llvm::LandingPadInst* cleanupLandingPad = this->builder.CreateLandingPad( lpResultType, 0, "cleanup.lp" );
			cleanupLandingPad->setCleanup( true );
			
			this->emitScopeCleanup( savedCleanupEntries );
			this->emitAllDefers();
			this->builder.CreateResume( cleanupLandingPad );
		}
		this->popDeferScope();
		
		// Restore parent function state
		this->currentFunction = savedFunction;
		this->namedValues = savedValues;
		this->aliveFlags = savedAliveFlags;
		if( savedInsertPoint ) {
			this->builder.SetInsertPoint( savedInsertPoint );
		}
	}
	
	llvm::Value* LLVMCodegen::generateIdentifierExpression( ast::nodes::IdentifierExpression& identifierExpression ) {
		std::unordered_map<std::string,llvm::Value*>::iterator namedValueIterator = this->namedValues.find( identifierExpression.name );
		if( namedValueIterator != this->namedValues.end() ) {
			llvm::Value* identifierAlloca = namedValueIterator->second;
			return this->builder.CreateLoad( getPointeeType( identifierAlloca ), identifierAlloca, identifierExpression.name );
		}
		
		std::unordered_map<std::string, llvm::Constant*>::iterator constantValueIterator = this->constantValues.find( identifierExpression.name );
		if( constantValueIterator != this->constantValues.end() ) {
			return constantValueIterator->second;
		}
		
		// Check global variables in LLVM module
		llvm::GlobalVariable* globalVar = this->module->getGlobalVariable( identifierExpression.name, true );
		if( globalVar ) {
			return this->builder.CreateLoad( globalVar->getValueType(), globalVar, identifierExpression.name );
		}
		
		// Check functions (returns function pointer for Callable assignment)
		std::unordered_map<std::string,llvm::Function*>::iterator functionIterator = this->functions.find( identifierExpression.name );
		if( functionIterator != this->functions.end() ) {
			return functionIterator->second;
		}
		
		// Type name as value (Meta<T> support)
		semantic::TypeSharedPointer identifierSemanticType = this->analyzer.types().lookupType( identifierExpression.name );
		if( identifierSemanticType ) {
			return llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->context ), std::hash<std::string>{}( identifierExpression.name ) );
		}
		return nullptr;
	}
	
	void LLVMCodegen::generateIfStatement( ast::nodes::IfStatement& ifStatement ) {
		llvm::Value* conditionValue = this->generateExpression( ifStatement.condition );
		if( conditionValue == nullptr ) {
			return;
		}
		
		// Ensure condition is i1
		if( conditionValue->getType()->isIntegerTy( 1 ) == false ) {
			if( conditionValue->getType()->isStructTy() ) {
				conditionValue = this->builder.CreateExtractValue( conditionValue, 0, "struct.scalar" );
			}
			conditionValue = this->builder.CreateICmpNE( conditionValue, llvm::Constant::getNullValue( conditionValue->getType() ), "ifcond" );
		}
		
		llvm::Function* currentFunctionHandle = this->currentFunction;
		llvm::BasicBlock* thenBasicBlock = llvm::BasicBlock::Create( this->context, "then", currentFunctionHandle );
		llvm::BasicBlock* elseBasicBlock = llvm::BasicBlock::Create( this->context, "else", currentFunctionHandle );
		llvm::BasicBlock* mergeBasicBlock = llvm::BasicBlock::Create( this->context, "ifcont", currentFunctionHandle );
		
		this->builder.CreateCondBr( conditionValue, thenBasicBlock, elseBasicBlock );
		
		// Then block
		this->builder.SetInsertPoint( thenBasicBlock );
		for( ast::nodes::StatementSharedPointer& thenStatement : ifStatement.thenBody ) {
			this->generateStatement( thenStatement );
		}
		if( this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
			this->builder.CreateBr( mergeBasicBlock );
		}
		
		// Elif/Else blocks
		this->builder.SetInsertPoint( elseBasicBlock );
		
		if( ifStatement.elifBranches.empty() == false ) {
			for( size_t branchIndex = 0; branchIndex < ifStatement.elifBranches.size(); branchIndex++ ) {
				ast::nodes::ExpressionSharedPointer elseIfCondition = ifStatement.elifBranches[branchIndex].first;
				std::vector<ast::nodes::StatementSharedPointer> elseIfBody = ifStatement.elifBranches[branchIndex].second;
				llvm::Value* elseIfConditionValue = this->generateExpression( elseIfCondition );
				if( elseIfConditionValue && elseIfConditionValue->getType()->isIntegerTy( 1 ) == false ) {
					if( elseIfConditionValue->getType()->isStructTy() ) {
						elseIfConditionValue = this->builder.CreateExtractValue( elseIfConditionValue, 0, "struct.scalar" );
					}
					elseIfConditionValue = this->builder.CreateICmpNE( elseIfConditionValue, llvm::Constant::getNullValue( elseIfConditionValue->getType() ), "elifcond" );
				}
				llvm::BasicBlock* elseIfThenBasicBlock = llvm::BasicBlock::Create( this->context, "elif.then", currentFunctionHandle );
				llvm::BasicBlock* elseIfNextBasicBlock = llvm::BasicBlock::Create( this->context, "elif.next", currentFunctionHandle );
				if( elseIfConditionValue ) {
					this->builder.CreateCondBr( elseIfConditionValue, elseIfThenBasicBlock, elseIfNextBasicBlock );
				}
				else {
					this->builder.CreateBr( elseIfNextBasicBlock );
				}
				this->builder.SetInsertPoint( elseIfThenBasicBlock );
				for( ast::nodes::StatementSharedPointer& elseIfStatement : elseIfBody ) {
					this->generateStatement( elseIfStatement );
				}
				if( this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
					this->builder.CreateBr( mergeBasicBlock );
				}
				this->builder.SetInsertPoint( elseIfNextBasicBlock );
			}
		}
		if( ifStatement.elseBody.empty() == false ) {
			for( ast::nodes::StatementSharedPointer& elseStatement : ifStatement.elseBody ) {
				this->generateStatement( elseStatement );
			}
		}
		if( this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
			this->builder.CreateBr( mergeBasicBlock );
		}
		this->builder.SetInsertPoint( mergeBasicBlock );
	}
	
	llvm::Value* LLVMCodegen::generateImplicitCast( llvm::Value* value, llvm::Type* targetType, bool isUnsigned ) {
		if( value == nullptr || targetType == nullptr ) {
			return value;
		}
		if( value->getType() == targetType ) {
			return value;
		}
		
		// Integer widening
		if( value->getType()->isIntegerTy() && targetType->isIntegerTy() ) {
			if( value->getType()->getIntegerBitWidth() < targetType->getIntegerBitWidth() ) {
				if( isUnsigned ) {
					return this->builder.CreateZExt( value, targetType, "zero_extend" );
				}
				return this->builder.CreateSExt( value, targetType, "sign_extend" );
			}
			return this->builder.CreateTrunc( value, targetType, "truncate" );
		}
		
		// Integer to floating point
		if( value->getType()->isIntegerTy() && targetType->isFloatingPointTy() ) {
			if( isUnsigned ) {
				return this->builder.CreateUIToFP( value, targetType, "unsigned_integer_to_floating_point" );
			}
			return this->builder.CreateSIToFP( value, targetType, "signed_integer_to_floating_point" );
		}
		
		// Floating point to integer
		if( value->getType()->isFloatingPointTy() && targetType->isIntegerTy() ) {
			return this->builder.CreateFPToSI( value, targetType, "floating_point_to_signed_integer" );
		}
		
		// Floating point widening
		if( value->getType()->isFloatTy() && targetType->isDoubleTy() ) {
			return this->builder.CreateFPExt( value, targetType, "floating_point_extend" );
		}
		
		// Floating point narrowing
		if( value->getType()->isDoubleTy() && targetType->isFloatTy() ) {
			return this->builder.CreateFPTrunc( value, targetType, "floating_point_truncate" );
		}
		
		// Null pointer to struct pointer: return typed null pointer
		if( llvm::isa<llvm::ConstantPointerNull>( value ) && targetType->isPointerTy() ) {
			return llvm::ConstantPointerNull::get( llvm::cast<llvm::PointerType>( targetType ) );
		}
		
		// Struct value to pointer-to-struct: spill to alloca
		if( value->getType()->isStructTy() && targetType->isPointerTy() && value->getType() != this->getInterfaceFatPointerType() ) {
			llvm::AllocaInst* spillAlloca = this->createEntryBlockAllocation( this->currentFunction, "spill", value->getType() );
			this->builder.CreateStore( value, spillAlloca );
			return spillAlloca;
		}
		
		// Class pointer → pointer-to-optional-interface: wrap into {i1 true, fat_ptr}, spill to alloca
		// With LLVM 19 opaque pointers, targetType carries no element type info.
		// This wrapping must be handled by callers passing struct type directly.
		if( value->getType()->isPointerTy() && targetType->isStructTy() ) {
			llvm::StructType* targetPointeeStruct = llvm::cast<llvm::StructType>( targetType );
			if( targetPointeeStruct->getNumElements() == 2 &&
				targetPointeeStruct->getElementType( 0 )->isIntegerTy( 1 ) &&
				targetPointeeStruct->getElementType( 1 ) == this->getInterfaceFatPointerType() ) {
				llvm::Type* valuePointeeType = getPointeeType( value );
				if( valuePointeeType->isStructTy() && llvm::isa<llvm::ConstantPointerNull>( value ) == false ) {
					llvm::Value* optionalValue = this->generateImplicitCast( value, targetType );
					llvm::AllocaInst* spillAlloca = this->createEntryBlockAllocation( this->currentFunction, "opt.iface.spill", targetType );
					this->builder.CreateStore( optionalValue, spillAlloca );
					return spillAlloca;
				}
			}
		}
		
		// Pointer cast
		if( value->getType()->isPointerTy() && targetType->isPointerTy() ) {
			return this->builder.CreateBitCast( value, targetType, "pointer_cast" );
		}
		
		// Pointer to integer (for generic collection storage)
		if( value->getType()->isPointerTy() && targetType->isIntegerTy() ) {
			return this->builder.CreatePtrToInt( value, targetType, "pointer_to_integer" );
		}
		
		// Integer to pointer (for generic collection retrieval)
		if( value->getType()->isIntegerTy() && targetType->isPointerTy() ) {
			return this->builder.CreateIntToPtr( value, targetType, "integer_to_pointer" );
		}
		
		// Class pointer → interface fat pointer: wrap {object, interface_table}
		if( targetType->isStructTy() && targetType == this->getInterfaceFatPointerType() && value->getType()->isPointerTy() ) {
			std::string className;
			llvm::Type* pointerElementType = getPointeeType( value );
			if( pointerElementType->isStructTy() ) {
				className = llvm::cast<llvm::StructType>( pointerElementType )->getName().str();
			}
			if( className.empty() && value->hasName() ) {
				std::unordered_map<std::string, std::string>::iterator vsIt = this->variableStructType.find( value->getName().str() );
				if( vsIt != this->variableStructType.end() ) {
					className = vsIt->second;
				}
			}
			if( className.empty() && this->lastConstructedClassName.empty() == false ) {
				className = this->lastConstructedClassName;
			}
			if( className.empty() == false ) {
				std::string itableClassName( className );
				size_t genericBracketPos = itableClassName.find( '<' );
				if( genericBracketPos != std::string::npos ) {
					std::string baseClassName( itableClassName.substr( 0, genericBracketPos ) );
					std::string basePrefix( fmt::format( "{}::", baseClassName ) );
					bool baseHasItable = false;
					for( std::pair<const std::string, llvm::GlobalVariable*>& entry : this->interfaceTables ) {
						if( entry.first.find( basePrefix ) == 0 ) {
							baseHasItable = true;
							break;
						}
					}
					if( baseHasItable ) {
						itableClassName = baseClassName;
					}
				}
				std::string classNamePrefix( fmt::format( "{}::", itableClassName ) );
				if( this->lastTargetInterfaceName.empty() == false ) {
					std::string exactKey( fmt::format( "{}::{}", itableClassName, this->lastTargetInterfaceName ) );
					std::unordered_map<std::string, llvm::GlobalVariable*>::iterator exactIt = this->interfaceTables.find( exactKey );
					if( exactIt != this->interfaceTables.end() ) {
						std::string interfaceName = this->lastTargetInterfaceName;
						this->lastConstructedClassName.clear();
						this->lastTargetInterfaceName.clear();
						return this->createInterfaceFatPointer( value, itableClassName, interfaceName );
					}
				}
				std::string firstInterfaceName;
				for( std::pair<const std::string, llvm::GlobalVariable*>& interfaceTableEntry : this->interfaceTables ) {
					const std::string& interfaceTableKey = interfaceTableEntry.first;
					if( interfaceTableKey.find( classNamePrefix ) == 0 ) {
						firstInterfaceName = interfaceTableKey.substr( itableClassName.size() + 2 );
						break;
					}
				}
				if( firstInterfaceName.empty() == false ) {
					this->lastConstructedClassName.clear();
					this->lastTargetInterfaceName.clear();
					return this->createInterfaceFatPointer( value, itableClassName, firstInterfaceName );
				}
			}
			this->lastConstructedClassName.clear();
			llvm::StructType* fatPointerType = this->getInterfaceFatPointerType();
			llvm::Value* objectI8Pointer = this->builder.CreateBitCast( value, llvm::PointerType::getUnqual( this->context ), "object.cast" );
			llvm::Value* fatPointerResult = llvm::UndefValue::get( fatPointerType );
			fatPointerResult = this->builder.CreateInsertValue( fatPointerResult, objectI8Pointer, 0, "interface_fat_pointer.object" );
			fatPointerResult = this->builder.CreateInsertValue( fatPointerResult, llvm::ConstantPointerNull::get( llvm::PointerType::getUnqual( this->context ) ), 1, "interface_fat_pointer.interface_table" );
			return fatPointerResult;
		}
		
		// Wrap value into tagged optional struct: T -> {i1 true, T value}
		// or null pointer -> {i1 false, T zero_initializer}
		if( targetType->isStructTy() ) {
			llvm::StructType* structTargetType = llvm::cast<llvm::StructType>( targetType );
			if( structTargetType->getNumElements() == 2 && structTargetType->getElementType( 0 )->isIntegerTy( 1 ) ) {
				
				// None (null pointer) → {false, 0}
				llvm::Type* innerType = structTargetType->getElementType( 1 );
				if( value->getType()->isPointerTy() && llvm::isa<llvm::ConstantPointerNull>( value ) ) {
					return llvm::ConstantStruct::get( structTargetType, {
						llvm::ConstantInt::getFalse( this->context ),
						llvm::Constant::getNullValue( innerType )
					});
				}
				llvm::Value* hasValueFlag = llvm::ConstantInt::getTrue( this->context );
				if( value->getType()->isPointerTy() ) {
					llvm::Value* isNull = this->builder.CreateICmpEQ( value, llvm::ConstantPointerNull::get( llvm::PointerType::getUnqual( this->context ) ), "is_null" );
					hasValueFlag = this->builder.CreateNot( isNull, "has_value" );
				}
				llvm::Value* innerValue = this->generateImplicitCast( value, innerType );
				llvm::Value* optionalResult = llvm::UndefValue::get( structTargetType );
				optionalResult = this->builder.CreateInsertValue( optionalResult, hasValueFlag, 0, "set_has_value" );
				optionalResult = this->builder.CreateInsertValue( optionalResult, innerValue, 1, "set_value" );
				return optionalResult;
			}
			
			// Union boxing: value -> {i32 tag, value}
			if( structTargetType->getNumElements() == 2 && structTargetType->getElementType( 0 )->isIntegerTy( 32 ) ) {
				llvm::Type* payloadType = structTargetType->getElementType( 1 );
				llvm::Value* castedValue = this->generateImplicitCast( value, payloadType );
				llvm::Value* unionResult = llvm::UndefValue::get( structTargetType );
				
				// Tag 0 for now — proper tag resolution needs semantic type info
				unionResult = this->builder.CreateInsertValue( unionResult, llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->context ), 0 ), 0, "union.tag" );
				unionResult = this->builder.CreateInsertValue( unionResult, castedValue, 1, "union.value" );
				return unionResult;
			}
		}
		
		// Pointer to struct (union passed by pointer)
		// With opaque pointers, detect union struct from Value pointee type
		if( targetType->isPointerTy() ) {
			llvm::Type* pointerElementType = getPointeeType( value );
			if( pointerElementType->isStructTy() == false ) {
				pointerElementType = nullptr;
			}
			if( pointerElementType && pointerElementType->isStructTy() ) {
				llvm::StructType* structPointerType = llvm::cast<llvm::StructType>( pointerElementType );
				if( structPointerType->getNumElements() == 2 && structPointerType->getElementType( 0 )->isIntegerTy( 32 ) && value->getType() != targetType ) {
					llvm::Type* payloadType = structPointerType->getElementType( 1 );
					llvm::Value* castedValue = this->generateImplicitCast( value, payloadType );
					llvm::Value* unionAllocation = this->createEntryBlockAllocation( this->currentFunction, "union.temporary", structPointerType );
					llvm::Value* tagPointer = this->builder.CreateStructGEP( structPointerType, unionAllocation, 0, "union.tag.pointer" );
					
					this->builder.CreateStore( llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->context ), 0 ), tagPointer );
					
					llvm::Value* valuePointer = this->builder.CreateStructGEP( structPointerType, unionAllocation, 1, "union.value.pointer" );
					
					this->builder.CreateStore( castedValue, valuePointer );
					
					return unionAllocation;
				}
			}
		}
		
		// Optional struct {i1, T} → T: extract inner value
		if( value->getType()->isStructTy() && targetType->isStructTy() == false ) {
			llvm::StructType* optionalStructType = llvm::cast<llvm::StructType>( value->getType() );
			if( optionalStructType->getNumElements() == 2 && optionalStructType->getElementType( 0 )->isIntegerTy( 1 ) ) {
				llvm::Value* innerValue = this->builder.CreateExtractValue( value, 1, "optional.val" );
				return this->generateImplicitCast( innerValue, targetType );
			}
		}
		
		// OOP wrapper struct pointer → primitive: load inner value field
		if( value->getType()->isPointerTy() && ( targetType->isIntegerTy() || targetType->isFloatingPointTy() ) ) {
			llvm::Type* pointeeType = getPointeeType( value );
			if( pointeeType->isStructTy() ) {
				llvm::StructType* structType = llvm::cast<llvm::StructType>( pointeeType );
				if( structType->getNumElements() == 1 && ( structType->getElementType( 0 )->isIntegerTy() || structType->getElementType( 0 )->isFloatingPointTy() ) ) {
					llvm::Value* fieldPointer = this->builder.CreateStructGEP( structType, value, 0, "unwrap.ptr" );
					llvm::Value* innerValue = this->builder.CreateLoad( structType->getElementType( 0 ), fieldPointer, "unwrap.val" );
					return this->generateImplicitCast( innerValue, targetType );
				}
			}
		}
		
		// Interface fat pointer → pointer type: extract object pointer and bitcast
		if( value->getType()->isStructTy() && value->getType() == this->getInterfaceFatPointerType() && targetType->isPointerTy() ) {
			llvm::Value* objectPointer = this->builder.CreateExtractValue( value, 0, "fat.obj" );
			return this->builder.CreateBitCast( objectPointer, targetType, "fat.cast" );
		}
		
		// Interface fat pointer → integer type: extract object pointer and ptrtoint
		if( value->getType()->isStructTy() && value->getType() == this->getInterfaceFatPointerType() && targetType->isIntegerTy() ) {
			llvm::Value* objectPointer = this->builder.CreateExtractValue( value, 0, "fat.obj" );
			return this->builder.CreatePtrToInt( objectPointer, targetType, "fat.int" );
		}
		return value;
	}
	
	llvm::Value* LLVMCodegen::generateIndexExpression( ast::nodes::IndexExpression& indexExpression ) {
		
		// Memory<T> indexing: mem[i] → GEP + load
		if( indexExpression.object->kind == ast::Node::Kind::IdentifierExpression ) {
			ast::nodes::IdentifierExpression& identifierExpression = static_cast<ast::nodes::IdentifierExpression&>( *indexExpression.object );
			std::unordered_map<std::string, llvm::Type*>::iterator memoryElementTypeIterator = this->varMemoryElementTypes.find( identifierExpression.name );
			if( memoryElementTypeIterator != this->varMemoryElementTypes.end() ) {
				std::unordered_map<std::string, llvm::Value*>::iterator namedValueIterator = this->namedValues.find( identifierExpression.name );
				if( namedValueIterator != this->namedValues.end() ) {
					llvm::Type* elementPointerType = llvm::PointerType::getUnqual( memoryElementTypeIterator->second );
					llvm::Value* memoryPointer = this->builder.CreateLoad( elementPointerType, namedValueIterator->second, "memory.pointer" );
					llvm::Value* memoryIndexValue = this->generateExpression( indexExpression.index );
					if( memoryIndexValue == nullptr ) {
						return nullptr;
					}
					memoryIndexValue = this->generateImplicitCast( memoryIndexValue, llvm::Type::getInt64Ty( this->context ) );
					llvm::Value* memoryGetElementPointer = this->builder.CreateGEP( memoryElementTypeIterator->second, memoryPointer, memoryIndexValue, "memory.index.gep" );
					return this->builder.CreateLoad( memoryElementTypeIterator->second, memoryGetElementPointer, "memory.index.value" );
				}
			}
		}
		
		// Inline ArrayExpression indexing: [1,2,3][idx] → GEP + load
		if( indexExpression.object->kind == ast::Node::Kind::ArrayExpression ) {
			llvm::Value* arrayPointer = this->generateExpression( indexExpression.object );
			llvm::Type* arrayElementType = this->lastMemoryElementType;
			if( arrayElementType == nullptr ) {
				arrayElementType = llvm::Type::getInt64Ty( this->context );
			}
			llvm::Value* arrayIndexValue = this->generateExpression( indexExpression.index );
			if( arrayPointer == nullptr || arrayIndexValue == nullptr ) {
				return nullptr;
			}
			arrayIndexValue = this->generateImplicitCast( arrayIndexValue, llvm::Type::getInt64Ty( this->context ) );
			llvm::Value* arrayInlineGetElementPointer = this->builder.CreateGEP( arrayElementType, arrayPointer, arrayIndexValue, "inline.array.gep" );
			return this->builder.CreateLoad( arrayElementType, arrayInlineGetElementPointer, "inline.array.value" );
		}
		
		// Inline ComprehensionExpression indexing: [expr for ...][idx] → GEP + load
		if( indexExpression.object->kind == ast::Node::Kind::ComprehensionExpression ) {
			llvm::Value* compPointer = this->generateExpression( indexExpression.object );
			llvm::Type* compElementType = this->lastMemoryElementType;
			if( compElementType == nullptr ) {
				compElementType = llvm::Type::getInt64Ty( this->context );
			}
			llvm::Value* compIndexValue = this->generateExpression( indexExpression.index );
			if( compPointer == nullptr || compIndexValue == nullptr ) {
				return nullptr;
			}
			compIndexValue = this->generateImplicitCast( compIndexValue, llvm::Type::getInt64Ty( this->context ) );
			llvm::Value* compGep = this->builder.CreateGEP( compElementType, compPointer, compIndexValue, "comp.index.gep" );
			return this->builder.CreateLoad( compElementType, compGep, "comp.index.value" );
		}

		llvm::Value* objectValue = this->generateExpression( indexExpression.object );
		llvm::Value* indexValue = this->generateExpression( indexExpression.index );
		if( objectValue != nullptr && indexValue != nullptr ) {
			if( objectValue->getType()->isPointerTy() ) {
				llvm::Type* objectElementType = getPointeeType( objectValue );
				if( objectElementType->isArrayTy() ) {
					llvm::Value* constantZeroValue = llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->context ), 0 );
					llvm::Value* arrayElementGetElementPointer = this->builder.CreateGEP( objectElementType, objectValue, { constantZeroValue, indexValue }, "element.pointer" );
					return this->builder.CreateLoad( getPointeeType( arrayElementGetElementPointer ), arrayElementGetElementPointer, "element.value" );
				}
				if( objectElementType->isStructTy() == false ) {
					indexValue = this->generateImplicitCast( indexValue, llvm::Type::getInt64Ty( this->context ) );
					llvm::Value* pointerElementGetElementPointer = this->builder.CreateGEP( objectElementType, objectValue, indexValue, "pointer.element.gep" );
					return this->builder.CreateLoad( objectElementType, pointerElementGetElementPointer, "pointer.element.value" );
				}
			}
			std::string className = this->resolveStructTypeName( indexExpression.object );
			if( className.empty() == false ) {
				std::string getMethodName = fmt::format( "{}::get", className );
				std::unordered_map<std::string, llvm::Function*>::iterator functionIterator = this->functions.find( getMethodName );
				if( functionIterator != this->functions.end() && functionIterator->second != nullptr ) {
					llvm::Value* selfPointer = this->resolveObjectPointer( indexExpression.object );
					if( selfPointer ) {
						llvm::Function* getFunction = functionIterator->second;
						llvm::Value* keyValue = indexValue;
						if( getFunction->arg_size() >= 2 ) {
							llvm::Type* expectedKeyType = ( getFunction->arg_begin() + 1 )->getType();
							keyValue = this->generateImplicitCast( keyValue, expectedKeyType );
						}
						return this->builder.CreateCall( getFunction, { selfPointer, keyValue }, "indexable.get" );
					}
				}
			}
		}
		return nullptr;
	}
	
	llvm::Value* LLVMCodegen::generateInterfaceMethodCall( ast::nodes::MethodCallExpression& methodCallExpression, llvm::Value* interfaceFatPointer, const std::string& interfaceName ) {
		semantic::InterfaceTypeSharedPointer interfaceType = std::dynamic_pointer_cast<semantic::InterfaceType>( this->analyzer.types().lookupType( interfaceName ) );
		if( interfaceType == nullptr ) {
			return nullptr;
		}
		semantic::MethodInfo* methodInformation = interfaceType->findMethod( methodCallExpression.method );
		if( methodInformation == nullptr || methodInformation->interfaceTableIndex < 0 ) {
			return nullptr;
		}
		llvm::StructType* interfaceFatPointerType = this->getInterfaceFatPointerType();

		// Unwrap nullable interface wrapper {i1, {i8*, i8*}} → {i8*, i8*} if present
		if( interfaceFatPointer->getType()->isStructTy() ) {
			llvm::StructType* incomingStructType = llvm::cast<llvm::StructType>( interfaceFatPointer->getType() );
			if( incomingStructType->getNumElements() == 2 &&
				incomingStructType->getElementType( 0 )->isIntegerTy( 1 ) &&
				incomingStructType->getElementType( 1 ) == interfaceFatPointerType ) {
				interfaceFatPointer = this->builder.CreateExtractValue( interfaceFatPointer, 1, "unwrap.nullable.iface.dispatch" );
			}
		}

		// fatPtr is a value ({i8*, i8*}), store it to access fields
		llvm::Value* interfaceFatPointerAllocation = this->createEntryBlockAllocation( this->currentFunction, "ifat.dispatch", interfaceFatPointerType );
		this->builder.CreateStore( interfaceFatPointer, interfaceFatPointerAllocation );
		
		// Extract object pointer (field 0)
		llvm::Value* objectSlot = this->builder.CreateStructGEP( interfaceFatPointerType, interfaceFatPointerAllocation, 0, "ifat.obj.ptr" );
		llvm::Value* objectPointerI8 = this->builder.CreateLoad( llvm::PointerType::getUnqual( this->context ), objectSlot, "obj.i8" );
		
		// Extract itable pointer (field 1)
		llvm::Value* interfaceTableSlot = this->builder.CreateStructGEP( interfaceFatPointerType, interfaceFatPointerAllocation, 1, "ifat.itable.ptr" );
		llvm::Value* interfaceTableI8 = this->builder.CreateLoad( llvm::PointerType::getUnqual( this->context ), interfaceTableSlot, "itable.i8" );
		
		// Cast itable to function pointer array
		llvm::FunctionType* voidFunctionType = llvm::FunctionType::get( llvm::Type::getVoidTy( this->context ), false );
		llvm::PointerType* functionPointerType = llvm::PointerType::getUnqual( voidFunctionType );
		llvm::PointerType* interfaceTablePointerType = llvm::PointerType::getUnqual( functionPointerType );
		llvm::Value* interfaceTableCast = this->builder.CreateBitCast( interfaceTableI8, interfaceTablePointerType, "itable.cast" );
		
		// Load function pointer at interfaceTableIndex
		llvm::Value* functionPointerAddress = this->builder.CreateGEP(
			functionPointerType,
			interfaceTableCast,
			llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->context ), methodInformation->interfaceTableIndex ),
			"ifunc.ptr.ptr"
		);
		llvm::Value* interfaceFunctionPointer = this->builder.CreateLoad( functionPointerType, functionPointerAddress, "ifunc.ptr" );
		
		// Build actual function type from interface method signature
		semantic::FunctionTypeSharedPointer methodFunctionType = std::dynamic_pointer_cast<semantic::FunctionType>( methodInformation->type );
		if( methodFunctionType != nullptr ) {
			std::vector<llvm::Type*> parameterLLVMTypes;
			llvm::Type* returnLLVMType = this->toLLVMType( methodFunctionType->returnType );
			
			// self as i8*
			parameterLLVMTypes.push_back( llvm::PointerType::getUnqual( this->context ) );
			
			for( const semantic::TypeSharedPointer& parameterType : methodFunctionType->parameterTypes ) {
				llvm::Type* llvmParameterType = this->toLLVMType( parameterType );
				if( llvmParameterType->isStructTy() && llvmParameterType != this->getInterfaceFatPointerType() ) {
					llvmParameterType = llvm::PointerType::getUnqual( llvmParameterType );
				}
				parameterLLVMTypes.push_back( llvmParameterType );
			}
			
			// Cast function pointer to correct type
			llvm::FunctionType* callFunctionType = llvm::FunctionType::get( returnLLVMType, parameterLLVMTypes, false );
			llvm::Value* castedFunction = this->builder.CreateBitCast(
				interfaceFunctionPointer,
				llvm::PointerType::getUnqual( callFunctionType ),
				"ifunc.cast"
			);
			
			// Build args: self (obj pointer) + call args
			std::vector<llvm::Value*> callArguments;
			callArguments.push_back( objectPointerI8 );
			
			for( ast::nodes::ExpressionSharedPointer& argumentNode : methodCallExpression.arguments ) {
				llvm::Value* argumentValue = this->generateExpression( argumentNode );
				if( argumentValue != nullptr ) {
					size_t argumentIndex = callArguments.size();
					if( argumentIndex < parameterLLVMTypes.size() ) {
						argumentValue = this->generateImplicitCast( argumentValue, parameterLLVMTypes[argumentIndex] );
					}
					callArguments.push_back( argumentValue );
				}
			}
			if( returnLLVMType->isVoidTy() ) {
				this->builder.CreateCall( callFunctionType, castedFunction, callArguments );
				return nullptr;
			}
			return this->builder.CreateCall( callFunctionType, castedFunction, callArguments, "icall.tmp" );
		}
		return nullptr;
	}
	
	llvm::Value* LLVMCodegen::generateInterfacePropertyAccess( llvm::Value* interfaceFatPointer, const semantic::InterfaceTypeSharedPointer& interfaceType, semantic::MethodInfo* methodInfo ) {
		llvm::StructType* fatPtrType = this->getInterfaceFatPointerType();

		// Unwrap nullable interface wrapper {i1, {i8*, i8*}} → {i8*, i8*} if present
		if( interfaceFatPointer->getType()->isStructTy() ) {
			llvm::StructType* incomingStructType = llvm::cast<llvm::StructType>( interfaceFatPointer->getType() );
			if( incomingStructType->getNumElements() == 2 &&
				incomingStructType->getElementType( 0 )->isIntegerTy( 1 ) &&
				incomingStructType->getElementType( 1 ) == fatPtrType ) {
				interfaceFatPointer = this->builder.CreateExtractValue( interfaceFatPointer, 1, "unwrap.nullable.iface.prop" );
			}
		}
		llvm::Value* fatPtrAlloc = this->createEntryBlockAllocation( this->currentFunction, "ifat.prop", fatPtrType );
		this->builder.CreateStore( interfaceFatPointer, fatPtrAlloc );
		
		llvm::Value* objSlot = this->builder.CreateStructGEP( fatPtrType, fatPtrAlloc, 0, "ifat.obj.ptr" );
		llvm::Value* objI8 = this->builder.CreateLoad( llvm::PointerType::getUnqual( this->context ), objSlot, "obj.i8" );
		llvm::Value* itableSlot = this->builder.CreateStructGEP( fatPtrType, fatPtrAlloc, 1, "ifat.itable.ptr" );
		llvm::Value* itableI8 = this->builder.CreateLoad( llvm::PointerType::getUnqual( this->context ), itableSlot, "itable.i8" );
		
		llvm::FunctionType* voidFnType = llvm::FunctionType::get( llvm::Type::getVoidTy( this->context ), false );
		llvm::PointerType* fnPtrType = llvm::PointerType::getUnqual( voidFnType );
		llvm::PointerType* itablePtrType = llvm::PointerType::getUnqual( fnPtrType );
		llvm::Value* itableCast = this->builder.CreateBitCast( itableI8, itablePtrType, "itable.cast" );
		
		llvm::Value* fnPtrAddr = this->builder.CreateGEP(
			fnPtrType, itableCast,
			llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->context ), methodInfo->interfaceTableIndex ),
			"iprop.ptr.ptr"
		);
		llvm::Value* fnPtr = this->builder.CreateLoad( fnPtrType, fnPtrAddr, "iprop.ptr" );
		
		semantic::FunctionTypeSharedPointer methodFnType = std::dynamic_pointer_cast<semantic::FunctionType>( methodInfo->type );
		if( methodFnType ) {
			llvm::Type* retType = this->toLLVMType( methodFnType->returnType );
			std::vector<llvm::Type*> paramTypes = { llvm::PointerType::getUnqual( this->context ) };
			llvm::FunctionType* callType = llvm::FunctionType::get( retType, paramTypes, false );
			llvm::Value* castedFn = this->builder.CreateBitCast( fnPtr, llvm::PointerType::getUnqual( callType ), "iprop.cast" );
			
			if( retType->isVoidTy() ) {
				this->builder.CreateCall( callType, castedFn, { objI8 } );
				return nullptr;
			}
			std::string propCallLabel = fmt::format( "iprop.{}", methodInfo->name );
			return this->builder.CreateCall( callType, castedFn, { objI8 }, propCallLabel );
		}
		return nullptr;
	}
	
	void LLVMCodegen::generateInterfaceTable( const std::string& className, const semantic::ClassTypeSharedPointer& classType, const semantic::InterfaceTypeSharedPointer& interfaceType ) {
		std::string interfaceTableKey = fmt::format( "{}::{}", className, interfaceType->name );
		if( this->interfaceTables.count( interfaceTableKey ) ) {
			return;
		}
		llvm::FunctionType* voidFunctionType = llvm::FunctionType::get( llvm::Type::getVoidTy( this->context ), false );
		llvm::PointerType* functionPointerType = llvm::PointerType::getUnqual( voidFunctionType );
		std::vector<llvm::Constant*> interfaceTableEntries;
		for( const std::string& methodName : interfaceType->methodOrder ) {
			
			// Find concrete implementation: try className::method, then walk parents
			llvm::Function* implementationFunction = nullptr;
			std::string lookUpKey = fmt::format( "{}::{}", className, methodName );
			std::unordered_map<std::string, llvm::Function*>::iterator functionIterator = this->functions.find( lookUpKey );
			if( functionIterator != this->functions.end() ) {
				implementationFunction = functionIterator->second;
			}
			else {
				
				// Walk parent chain
				semantic::TypeSharedPointer baseClassType = classType->baseClass;
				while( baseClassType && implementationFunction == nullptr ) {
					lookUpKey = fmt::format( "{}::{}", baseClassType->name, methodName );
					std::unordered_map<std::string, llvm::Function*>::iterator parentFunctionIterator = this->functions.find( lookUpKey );
					if( parentFunctionIterator != this->functions.end() ) {
						implementationFunction = parentFunctionIterator->second;
					}
					if( baseClassType->kind == semantic::Type::Kind::Class ) {
						baseClassType = std::static_pointer_cast<semantic::ClassType>( baseClassType )->baseClass;
					}
					else {
						break;
					}
				}
			}
			if( implementationFunction ) {
				interfaceTableEntries.push_back( llvm::ConstantExpr::getBitCast( implementationFunction, functionPointerType ) );
			}
			else {
				std::string pureVirtualStubName = fmt::format( "_AE_pure_virtual_{}_{}", className, methodName );
				llvm::Function* pureVirtualStub = this->module->getFunction( pureVirtualStubName );
				if( pureVirtualStub == nullptr ) {
					pureVirtualStub = llvm::Function::Create(
						voidFunctionType,
						llvm::Function::InternalLinkage,
						pureVirtualStubName,
						this->getModule()
					);
					llvm::BasicBlock* stubBlock = llvm::BasicBlock::Create( this->context, "entry", pureVirtualStub );
					
					llvm::Function* savedFunction = this->currentFunction;
					llvm::BasicBlock* savedInsertBlock = this->builder.GetInsertBlock();
					llvm::BasicBlock::iterator savedInsertPoint = this->builder.GetInsertPoint();
					
					this->currentFunction = pureVirtualStub;
					this->builder.SetInsertPoint( stubBlock );
					
					std::string pureVirtualMessage = fmt::format( "pure virtual function called: {}::{}", className, methodName );
					lookup::SourceSharedPointer emptySource = nullptr;
					this->generateThrowError( "Error", pureVirtualMessage, emptySource );
					
					this->currentFunction = savedFunction;
					if( savedInsertBlock ) {
						this->builder.SetInsertPoint( savedInsertBlock, savedInsertPoint );
					}
				}
				interfaceTableEntries.push_back( llvm::ConstantExpr::getBitCast( pureVirtualStub, functionPointerType ) );
			}
		}
		if( interfaceTableEntries.empty() ) {
			return;
		}
		std::string interfaceTableName( fmt::format( "_AE_itable_{}_{}", className, interfaceType->name ) );
		std::string interfaceTableClassName( fmt::format( "{}::{}", className, interfaceType->name ) );
		llvm::ArrayType* interfaceTableArrayType = llvm::ArrayType::get( functionPointerType, interfaceTableEntries.size() );
		llvm::GlobalVariable* interfaceTableGlobal = new llvm::GlobalVariable(
			*this->module,
			interfaceTableArrayType,
			true,
			llvm::GlobalValue::InternalLinkage,
			llvm::ConstantArray::get( interfaceTableArrayType, interfaceTableEntries ),
			interfaceTableName
		);
		this->interfaceTables[interfaceTableClassName] = interfaceTableGlobal;
	}
	
	llvm::Value* LLVMCodegen::generateLambdaExpression( ast::nodes::LambdaExpression& lambdaExpression ) {
		std::string lambdaName = fmt::format( "_AE_lambda_{}", this->lambdaCounter++ );
		
		// Collect free variables from enclosing scope
		std::unordered_set<std::string> boundVariables;
		for( ast::nodes::FunctionParameterSharedPointer& parameter : lambdaExpression.parameters ) {
			boundVariables.insert( parameter->name );
		}
		std::vector<std::string> capturedVariables;
		for( ast::nodes::StatementSharedPointer& lambdaStatement : lambdaExpression.body ) {
			this->collectFreeVariablesInStatement( lambdaStatement, boundVariables, capturedVariables );
		}
		
		// Build parameter types: explicit parameters + captured variables
		std::vector<llvm::Type*> functionParameterTypes;
		for( ast::nodes::FunctionParameterSharedPointer& parameter : lambdaExpression.parameters ) {
			if( parameter->type ) {
				functionParameterTypes.push_back( this->resolveAstType( parameter->type ) );
			}
			else {
				functionParameterTypes.push_back( llvm::Type::getInt64Ty( this->context ) );
			}
		}
		std::vector<llvm::Value*> capturedValues;
		std::vector<llvm::Type*> capturedTypes;
		for( const std::string& capturedVariableName : capturedVariables ) {
			std::unordered_map<std::string, llvm::Value*>::iterator namedValueIterator = this->namedValues.find( capturedVariableName );
			if( namedValueIterator != this->namedValues.end() ) {
				std::string captureLabel = fmt::format( "cap.{}", capturedVariableName );
				llvm::Value* capturedAllocation = namedValueIterator->second;
				llvm::Value* loadedCapturedValue = this->builder.CreateLoad(
					getPointeeType( capturedAllocation ),
					capturedAllocation,
					captureLabel
				);
				
				capturedValues.push_back( loadedCapturedValue );
				capturedTypes.push_back( loadedCapturedValue->getType() );
				functionParameterTypes.push_back( loadedCapturedValue->getType() );
			}
		}
		llvm::Type* functionReturnType = lambdaExpression.returnType ? this->resolveAstType( lambdaExpression.returnType ) : nullptr;
		if( functionReturnType == nullptr ) {
			functionReturnType = llvm::Type::getInt64Ty( this->context );
		}
		if( lambdaExpression.isAsync && functionReturnType && functionReturnType->isVoidTy() == false ) {
			functionReturnType = llvm::StructType::get( this->context, {
				llvm::Type::getInt1Ty( this->context ), functionReturnType
			});
		}
		llvm::FunctionType* lambdaFunctionType = llvm::FunctionType::get( functionReturnType, functionParameterTypes, false );
		llvm::Function* lambdaFunction = llvm::Function::Create(
			lambdaFunctionType,
			llvm::Function::InternalLinkage,
			lambdaName,
			this->getModule()
		);
		lambdaFunction->setPersonalityFn( this->getOrCreatePersonalityFunction() );
		std::unordered_map<std::string,llvm::Value*> savedNamedValues = this->namedValues;
		llvm::BasicBlock* savedInsertBlock = this->builder.GetInsertBlock();
		llvm::Function* savedCurrentFunction = this->currentFunction;
		llvm::BasicBlock* lambdaEntryBlock = llvm::BasicBlock::Create( this->context, "entry", lambdaFunction );
		
		this->builder.SetInsertPoint( lambdaEntryBlock );
		this->currentFunction = lambdaFunction;
		this->namedValues.clear();
		this->variableStructType.clear();
		this->varMemoryElementTypes.clear();
		this->arenaElementTypes.clear();
		
		// Bind explicit parameters
		unsigned int parameterIndex = 0;
		for( ast::nodes::FunctionParameterSharedPointer& parameterNode : lambdaExpression.parameters ) {
			llvm::Function::arg_iterator argumentIterator = lambdaFunction->arg_begin() + parameterIndex;
			argumentIterator->setName( parameterNode->name );
			
			llvm::Value* parameterAllocation = this->createEntryBlockAllocation( lambdaFunction, parameterNode->name, argumentIterator->getType() );
			this->builder.CreateStore( &*argumentIterator, parameterAllocation );
			this->namedValues[parameterNode->name] = parameterAllocation;
			parameterIndex++;
		}
		
		// Bind captured variables as local allocations
		for( size_t captureIndex = 0; captureIndex < capturedVariables.size(); captureIndex++ ) {
			std::string captureName = fmt::format( "cap.{}", capturedVariables[captureIndex] );
			llvm::Function::arg_iterator captureArgumentIterator = lambdaFunction->arg_begin() + lambdaExpression.parameters.size() + captureIndex;
			llvm::Value* captureAllocation = this->createEntryBlockAllocation( lambdaFunction, capturedVariables[captureIndex], captureArgumentIterator->getType() );
			captureArgumentIterator->setName( captureName );
			this->builder.CreateStore( &*captureArgumentIterator, captureAllocation );
			this->namedValues[capturedVariables[captureIndex]] = captureAllocation;
		}
		for( ast::nodes::StatementSharedPointer& lambdaBodyStatement : lambdaExpression.body ) {
			this->generateStatement( lambdaBodyStatement );
		}
		if( this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
			if( functionReturnType->isVoidTy() ) {
				this->builder.CreateRetVoid();
			}
			else {
				this->builder.CreateRet( llvm::Constant::getNullValue( functionReturnType ) );
			}
		}
		this->namedValues = savedNamedValues;
		this->currentFunction = savedCurrentFunction;
		this->builder.SetInsertPoint( savedInsertBlock );
		if( capturedVariables.empty() ) {
			return lambdaFunction;
		}
		std::string wrapperFunctionName = fmt::format( "{}.wrap", lambdaName );
		std::vector<llvm::GlobalVariable*> capturedGlobalVariables;
		for( size_t captureVariableIndex = 0; captureVariableIndex < capturedVariables.size(); captureVariableIndex++ ) {
			std::string globalVariableName = fmt::format( "{}.cap.{}", lambdaName, capturedVariables[captureVariableIndex] );
			llvm::GlobalVariable* capturedGlobal = new llvm::GlobalVariable(
				*this->module,
				capturedTypes[captureVariableIndex],
				false,
				llvm::GlobalValue::InternalLinkage,
				llvm::Constant::getNullValue( capturedTypes[captureVariableIndex] ),
				globalVariableName
			);
			capturedGlobalVariables.push_back( capturedGlobal );
			this->builder.CreateStore( capturedValues[captureVariableIndex], capturedGlobal );
		}
		std::vector<llvm::Type*> wrapperParameterTypes;
		for( ast::nodes::FunctionParameterSharedPointer& wrapperParameter : lambdaExpression.parameters ) {
			if( wrapperParameter->type ) {
				wrapperParameterTypes.push_back( this->resolveAstType( wrapperParameter->type ) );
			}
			else {
				wrapperParameterTypes.push_back( llvm::Type::getInt64Ty( this->context ) );
			}
		}
		llvm::FunctionType* wrapperFunctionType = llvm::FunctionType::get( functionReturnType, wrapperParameterTypes, false );
		llvm::Function* wrapperFunction = llvm::Function::Create(
			wrapperFunctionType,
			llvm::Function::InternalLinkage,
			wrapperFunctionName,
			this->getModule()
		);
		wrapperFunction->setPersonalityFn( this->getOrCreatePersonalityFunction() );
		llvm::BasicBlock* wrapperEntryBlock = llvm::BasicBlock::Create( this->context, "entry", wrapperFunction );
		llvm::BasicBlock* savedInsertBlockAfterWrapper = this->builder.GetInsertBlock();
		
		this->builder.SetInsertPoint( wrapperEntryBlock );
		
		std::vector<llvm::Value*> lambdaCallArguments;
		for( unsigned int wrapperArgumentIndex = 0; wrapperArgumentIndex < wrapperFunction->arg_size(); wrapperArgumentIndex++ ) {
			lambdaCallArguments.push_back( wrapperFunction->getArg( wrapperArgumentIndex ) );
		}
		for( size_t globalCaptureIndex = 0; globalCaptureIndex < capturedGlobalVariables.size(); globalCaptureIndex++ ) {
			std::string loadLabel = fmt::format( "load.cap.{}", capturedVariables[globalCaptureIndex] );
			llvm::Value* loadedCapturedGlobal = this->builder.CreateLoad(
				capturedTypes[globalCaptureIndex],
				capturedGlobalVariables[globalCaptureIndex],
				loadLabel
			);
			lambdaCallArguments.push_back( loadedCapturedGlobal );
		}
		llvm::Value* lambdaResult = this->builder.CreateCall( lambdaFunction, lambdaCallArguments );
		if( functionReturnType->isVoidTy() ) {
			this->builder.CreateRetVoid();
		}
		else {
			this->builder.CreateRet( lambdaResult );
		}
		this->builder.SetInsertPoint( savedInsertBlockAfterWrapper );
		return wrapperFunction;
	}
	
	llvm::Value* LLVMCodegen::generateMatchExpression( ast::nodes::MatchExpression& matchExpression ) {
		llvm::Value* subjectValue = this->generateExpression( matchExpression.subject );
		if( subjectValue != nullptr ) {
			std::vector<llvm::Value*> patternValues;
			std::vector<llvm::Value*> armValues;
			for( size_t index = 0; index < matchExpression.patterns.size(); index++ ) {
				patternValues.push_back( this->generateExpression( matchExpression.patterns[index] ) );
				armValues.push_back( this->generateExpression( matchExpression.values[index] ) );
			}
			llvm::Value* defaultValue = nullptr;
			if( matchExpression.defaultValue ) {
				defaultValue = this->generateExpression( matchExpression.defaultValue );
			}
			llvm::Type* matchResultType = nullptr;
			for( llvm::Value* armValue : armValues ) {
				if( armValue != nullptr ) {
					matchResultType = armValue->getType();
					break;
				}
			}
			if( matchResultType == nullptr && defaultValue != nullptr ) {
				matchResultType = defaultValue->getType();
			}
			if( matchResultType == nullptr ) {
				matchResultType = llvm::Type::getInt64Ty( this->context );
			}
			llvm::Value* currentMatchResult = llvm::Constant::getNullValue( matchResultType );
			if( defaultValue ) {
				currentMatchResult = this->generateImplicitCast( defaultValue, matchResultType );
			}
			
			// Iterate backwards to build the selection chain
			for( int index = static_cast<int>( patternValues.size() ) - 1; index >= 0; index-- ) {
				if( patternValues[index] == nullptr || armValues[index] == nullptr ) {
					continue;
				}
				llvm::Value* castedArmValue = this->generateImplicitCast( armValues[index], matchResultType );
				llvm::Value* castedPatternValue = this->generateImplicitCast( patternValues[index], subjectValue->getType() );
				llvm::Value* comparisonResult;
				std::string comparisonLabel = fmt::format( "match.compare.index.{}", index );
				std::string selectLabel = fmt::format( "match.select.index.{}", index );
				if( subjectValue->getType()->isIntegerTy() ) {
					comparisonResult = this->builder.CreateICmpEQ( subjectValue, castedPatternValue, comparisonLabel );
				}
				else if( subjectValue->getType()->isFloatingPointTy() ) {
					comparisonResult = this->builder.CreateFCmpOEQ( subjectValue, castedPatternValue, comparisonLabel );
				}
				else {
					comparisonResult = this->builder.CreateICmpEQ( subjectValue, castedPatternValue, comparisonLabel );
				}
				currentMatchResult = this->builder.CreateSelect( comparisonResult, castedArmValue, currentMatchResult, selectLabel );
			}
			return currentMatchResult;
		}
		return nullptr;
	}
	
	void LLVMCodegen::generateMatchStatement( ast::nodes::MatchStatement& matchStatement ) {
		
		// Detect if the match subject is an enum.
		// For simple enums (i32), subjectTagValue is the loaded integer.
		// For payload enums (struct), we extract the tag from the alloca pointer.
		bool isEnumMatch = false;
		llvm::Value* subjectTagValue = nullptr;
		llvm::Value* subjectValue = nullptr;
		if( matchStatement.subject->kind == ast::Node::Kind::IdentifierExpression ) {
			ast::nodes::IdentifierExpression& identifierExpression = static_cast<ast::nodes::IdentifierExpression&>( *matchStatement.subject );
			std::unordered_map<std::string,llvm::Value*>::iterator namedValueIterator = this->namedValues.find( identifierExpression.name );
			if( namedValueIterator != this->namedValues.end() ) {
				llvm::Type* elementPointerType = getPointeeType( namedValueIterator->second );
				if( elementPointerType->isStructTy() ) {
					
					// Payload enum stored as struct
					llvm::StructType* structType = llvm::cast<llvm::StructType>( elementPointerType );
					if( this->enumTypeNames.count( structType->getName().str() ) ) {
						isEnumMatch = true;
						llvm::Value* tagPointer = this->builder.CreateStructGEP( structType, namedValueIterator->second, 0, "subj.tagptr" );
						subjectTagValue = this->builder.CreateLoad( llvm::Type::getInt32Ty( this->context ), tagPointer, "subj.tag" );
					}
				}
				else if( elementPointerType->isIntegerTy( 32 ) ) {
					
					// Check if this variable holds a simple enum (i32)
					std::unordered_map<std::string, std::string>::iterator variableTypeIterator = this->variableStructType.find( identifierExpression.name );
					if( variableTypeIterator != this->variableStructType.end() && this->enumTypeNames.count( variableTypeIterator->second ) ) {
						isEnumMatch = true;
						subjectTagValue = this->builder.CreateLoad( elementPointerType, namedValueIterator->second, "subj.tag" );
					}
				}
			}
		}
		if( subjectTagValue == nullptr ) {
			subjectValue = this->generateExpression( matchStatement.subject );
			if( subjectValue == nullptr ) {
				return;
			}
			
			// Check if the loaded value is an i32 that could be a simple enum
			if( subjectValue->getType()->isIntegerTy( 32 ) ) {
				
				// Heuristic: if the first arm is an enum member access, treat as enum match
				if( matchStatement.arms.empty() == false && matchStatement.arms[0]->pattern->kind == ast::Node::Kind::MemberAccessExpression ) {
					ast::nodes::MemberAccessExpression& memberAccess = static_cast<ast::nodes::MemberAccessExpression&>( *matchStatement.arms[0]->pattern );
					if( memberAccess.object->kind == ast::Node::Kind::IdentifierExpression ) {
						ast::nodes::IdentifierExpression& objectIdentifier = static_cast<ast::nodes::IdentifierExpression&>( *memberAccess.object );
						if( this->enumTypeNames.count( objectIdentifier.name ) ) {
							isEnumMatch = true;
							subjectTagValue = subjectValue;
							subjectValue = nullptr;
						}
					}
				}
			}
		}
		llvm::Function* currentFunctionTarget = this->currentFunction;
		llvm::BasicBlock* mergeBasicBlock = llvm::BasicBlock::Create( this->context, "match.end", currentFunctionTarget );
		for( size_t armIndex = 0; armIndex < matchStatement.arms.size(); armIndex++ ) {
			ast::nodes::MatchArmNodeSharedPointer& matchArm = matchStatement.arms[armIndex];
			std::string armBlockLabel = fmt::format( "match.arm.{}", armIndex );
			std::string nextBlockLabel = fmt::format( "match.next.{}", armIndex );
			llvm::BasicBlock* armBasicBlock = llvm::BasicBlock::Create( this->context, armBlockLabel, currentFunctionTarget );
			llvm::BasicBlock* nextBasicBlock = mergeBasicBlock;
			if( armIndex + 1 < matchStatement.arms.size() ) {
				nextBasicBlock = llvm::BasicBlock::Create( this->context, nextBlockLabel, currentFunctionTarget );
			}
			llvm::Value* comparisonValue = nullptr;
			
			// Handle enum variant pattern: Direction.North
			if( isEnumMatch && matchArm->pattern->kind == ast::Node::Kind::MemberAccessExpression ) {
				ast::nodes::MemberAccessExpression& variantAccess = static_cast<ast::nodes::MemberAccessExpression&>( *matchArm->pattern );
				if( variantAccess.object->kind == ast::Node::Kind::IdentifierExpression ) {
					ast::nodes::IdentifierExpression& variantObject = static_cast<ast::nodes::IdentifierExpression&>( *variantAccess.object );
					std::string variantLookupKey = fmt::format( "{}::{}", variantObject.name, variantAccess.member );
					std::unordered_map<std::string, int>::iterator variantIterator = this->enumVariants.find( variantLookupKey );
					if( variantIterator != this->enumVariants.end() ) {
						llvm::ConstantInt* patternTagConstant = llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->context ), variantIterator->second );
						comparisonValue = this->builder.CreateICmpEQ( subjectTagValue, patternTagConstant, "enum.match" );
					}
				}
			}
			if( comparisonValue == nullptr ) {
				llvm::Value* patternValue = this->generateExpression( matchArm->pattern );
				if( patternValue != nullptr ) {
					llvm::Value* valueToCompare = ( subjectValue != nullptr ) ? subjectValue : subjectTagValue;
					if( valueToCompare->getType() == patternValue->getType() ) {
						if( valueToCompare->getType()->isIntegerTy() ) {
							comparisonValue = this->builder.CreateICmpEQ( valueToCompare, patternValue, "match.cmp" );
						}
						else if( valueToCompare->getType()->isFloatingPointTy() ) {
							comparisonValue = this->builder.CreateFCmpOEQ( valueToCompare, patternValue, "match.cmp" );
						}
						else {
							comparisonValue = this->builder.CreateICmpEQ( valueToCompare, patternValue, "match.cmp" );
						}
					}
					else if( matchArm->pattern->kind == ast::Node::Kind::IdentifierExpression ) {
						
						// Wildcard / variable binding
						comparisonValue = llvm::ConstantInt::get( llvm::Type::getInt1Ty( this->context ), 1 );
					}
				}
				else {
					
					// Wildcard or always match (default branch)
					this->builder.CreateBr( armBasicBlock );
					this->builder.SetInsertPoint( armBasicBlock );
					for( ast::nodes::StatementSharedPointer& bodyStatement : matchArm->body ) {
						this->generateStatement( bodyStatement );
					}
					if( this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
						this->builder.CreateBr( mergeBasicBlock );
					}
					if( nextBasicBlock != mergeBasicBlock ) {
						this->builder.SetInsertPoint( nextBasicBlock );
					}
					continue;
				}
			}
			if( comparisonValue != nullptr ) {
				this->builder.CreateCondBr( comparisonValue, armBasicBlock, nextBasicBlock );
			}
			else {
				this->builder.CreateBr( nextBasicBlock );
			}
			this->builder.SetInsertPoint( armBasicBlock );
			for( ast::nodes::StatementSharedPointer& armBodyStatement : matchArm->body ) {
				this->generateStatement( armBodyStatement );
			}
			if( this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
				this->builder.CreateBr( mergeBasicBlock );
			}
			if( nextBasicBlock != mergeBasicBlock ) {
				this->builder.SetInsertPoint( nextBasicBlock );
			}
		}
		this->builder.SetInsertPoint( mergeBasicBlock );
	}
	
	llvm::Value* LLVMCodegen::generateMemberAccessExpression( ast::nodes::MemberAccessExpression& memberAccessExpression ) {
		
		// Enum builtin properties: Color.RED.name, Color.RED.value
		if( memberAccessExpression.object->kind == ast::Node::Kind::MemberAccessExpression ) {
			ast::nodes::MemberAccessExpression& innerAccess = static_cast<ast::nodes::MemberAccessExpression&>( *memberAccessExpression.object );
			if( innerAccess.object->kind == ast::Node::Kind::IdentifierExpression ) {
				ast::nodes::IdentifierExpression& enumIdentifier = static_cast<ast::nodes::IdentifierExpression&>( *innerAccess.object );
				if( this->enumTypeNames.count( enumIdentifier.name ) ) {
					std::string variantKey = fmt::format( "{}::{}", enumIdentifier.name, innerAccess.member );
					std::unordered_map<std::string, int>::iterator variantIterator = this->enumVariants.find( variantKey );
					if( variantIterator != this->enumVariants.end() ) {
						if( memberAccessExpression.member == "name" ) {
							return this->builder.CreateGlobalStringPtr( innerAccess.member, "enum.name" );
						}
						if( memberAccessExpression.member == "value" ) {
							std::unordered_map<std::string, llvm::Constant*>::iterator backedValueIterator = this->enumBackedValues.find( variantKey );
							if( backedValueIterator != this->enumBackedValues.end() ) {
								return backedValueIterator->second;
							}
							return llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->context ), variantIterator->second );
						}
					}
				}
			}
		}
		
		// Enum variable .name/.value via runtime lookup
		if( memberAccessExpression.member == "name" || memberAccessExpression.member == "value" ) {
			llvm::Value* evaluatedObjectValue = this->generateExpression( memberAccessExpression.object );
			if( evaluatedObjectValue != nullptr && evaluatedObjectValue->getType()->isIntegerTy() ) {
				std::string resolvedEnumName;
				if( memberAccessExpression.object->kind == ast::Node::Kind::IdentifierExpression ) {
					ast::nodes::IdentifierExpression& identifier = static_cast<ast::nodes::IdentifierExpression&>( *memberAccessExpression.object );
					std::unordered_map<std::string, std::string>::iterator variableTypeIterator = this->variableStructType.find( identifier.name );
					if( variableTypeIterator != this->variableStructType.end() && this->enumTypeNames.count( variableTypeIterator->second ) ) {
						resolvedEnumName = variableTypeIterator->second;
					}
				}
				if( resolvedEnumName.empty() == false && memberAccessExpression.member == "name" ) {
					semantic::EnumTypeSharedPointer enumType = std::dynamic_pointer_cast<semantic::EnumType>( this->analyzer.types().lookupType( resolvedEnumName ) );
					if( enumType ) {
						llvm::Value* defaultStringPointer = this->builder.CreateGlobalStringPtr( "?", "enum.unknown" );
						llvm::Value* currentSelectionResult = defaultStringPointer;
						for( const semantic::EnumVariantInfo& variant : enumType->variants ) {
							std::string labelName = fmt::format( "enum.{}", variant.name );
							std::string comparisonLabel = fmt::format( "cmp.{}", variant.name );
							std::string selectionLabel = fmt::format( "sel.{}", variant.name );
							llvm::Value* variantNameString = this->builder.CreateGlobalStringPtr( variant.name, labelName );
							llvm::Value* isMatchingVariant = this->builder.CreateICmpEQ(
								evaluatedObjectValue,
								llvm::ConstantInt::get( evaluatedObjectValue->getType(), variant.discriminant ),
								comparisonLabel
							);
							currentSelectionResult = this->builder.CreateSelect( isMatchingVariant, variantNameString, currentSelectionResult, selectionLabel );
						}
						return currentSelectionResult;
					}
				}
				if( resolvedEnumName.empty() == false && memberAccessExpression.member == "value" ) {
					semantic::EnumTypeSharedPointer enumType = std::dynamic_pointer_cast<semantic::EnumType>( this->analyzer.types().lookupType( resolvedEnumName ) );
					if( enumType && enumType->variants.empty() == false ) {
						std::string firstVariantKey = fmt::format( "{}::{}", resolvedEnumName, enumType->variants[0].name );
						std::unordered_map<std::string, llvm::Constant*>::iterator backedValueIterator = this->enumBackedValues.find( firstVariantKey );
						if( backedValueIterator != this->enumBackedValues.end() ) {
							llvm::Value* currentBackedResult = llvm::Constant::getNullValue( backedValueIterator->second->getType() );
							for( const semantic::EnumVariantInfo& variant : enumType->variants ) {
								std::string variantKey = fmt::format( "{}::{}", resolvedEnumName, variant.name );
								std::unordered_map<std::string, llvm::Constant*>::iterator currentBackedValueIterator = this->enumBackedValues.find( variantKey );
								if( currentBackedValueIterator == this->enumBackedValues.end() ) {
									continue;
								}
								std::string comparisonLabel = fmt::format( "vcmp.{}", variant.name );
								std::string selectionLabel = fmt::format( "vsel.{}", variant.name );
								llvm::Value* isMatchingVariant = this->builder.CreateICmpEQ(
									evaluatedObjectValue,
									llvm::ConstantInt::get( evaluatedObjectValue->getType(), variant.discriminant ),
									comparisonLabel
								);
								currentBackedResult = this->builder.CreateSelect( isMatchingVariant, currentBackedValueIterator->second, currentBackedResult, selectionLabel );
							}
							return currentBackedResult;
						}
					}
					return evaluatedObjectValue;
				}
			}
		}
		
		// Check for enum variant access: Direction.North
		if( memberAccessExpression.object->kind == ast::Node::Kind::IdentifierExpression ) {
			ast::nodes::IdentifierExpression& identifier = static_cast<ast::nodes::IdentifierExpression&>( *memberAccessExpression.object );
			if( this->enumTypeNames.count( identifier.name ) ) {
				std::string variantLookupKey = fmt::format( "{}::{}", identifier.name, memberAccessExpression.member );
				std::unordered_map<std::string, int>::iterator variantIterator = this->enumVariants.find( variantLookupKey );
				if( variantIterator != this->enumVariants.end() ) {
					semantic::TypeSharedPointer enumSemanticType = this->analyzer.types().lookupType( identifier.name );
					llvm::Type* enumLLVMType = this->toLLVMType( enumSemanticType );
					if( enumLLVMType->isIntegerTy() ) {
						std::unordered_map<std::string, llvm::Constant*>::iterator backedValueIterator = this->enumBackedValues.find( variantLookupKey );
						if( backedValueIterator != this->enumBackedValues.end() ) {
							return backedValueIterator->second;
						}
						return llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->context ), variantIterator->second );
					}
					
					// Payload enum: construct struct { i32 tag, [N x i8] payload }
					llvm::StructType* enumStructType = this->structTypes[identifier.name];
					std::string allocationLabel = fmt::format( "{}.variant", identifier.name );
					llvm::Value* variantAllocation = this->createEntryBlockAllocation( this->currentFunction, allocationLabel, enumStructType );
					llvm::Value* tagPointer = this->builder.CreateStructGEP( enumStructType, variantAllocation, 0, "tagptr" );
					
					this->builder.CreateStore( llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->context ), variantIterator->second ), tagPointer );
					
					return variantAllocation;
				}
			}
			
			// Static class field access: ClassName.staticField
			std::unordered_map<std::string, std::unordered_map<std::string, llvm::GlobalVariable*>>::iterator staticClassIterator = this->staticClassFields.find( identifier.name );
			if( staticClassIterator != this->staticClassFields.end() ) {
				std::unordered_map<std::string, llvm::GlobalVariable*>::iterator staticFieldIterator = staticClassIterator->second.find( memberAccessExpression.member );
				if( staticFieldIterator != staticClassIterator->second.end() ) {
					llvm::GlobalVariable* staticGlobal = staticFieldIterator->second;
					return this->builder.CreateLoad( staticGlobal->getValueType(), staticGlobal, memberAccessExpression.member );
				}
			}
		}
		
		llvm::Value* resolvedObjectPointer = this->resolveObjectPointer( memberAccessExpression.object );
		if( resolvedObjectPointer != nullptr && resolvedObjectPointer->getType()->isPointerTy() ) {
			llvm::Type* pointerElementType = getPointeeType( resolvedObjectPointer );
			llvm::StructType* structType = nullptr;
			std::string structName;
			if( pointerElementType->isStructTy() ) {
				structType = llvm::cast<llvm::StructType>( pointerElementType );
				structName = structType->getName().str();
			}
			else {
				structName = this->resolveStructTypeName( memberAccessExpression.object );
				if( structName.empty() && memberAccessExpression.object->semanticType &&
					( memberAccessExpression.object->semanticType->kind == semantic::Type::Kind::Class ||
					  memberAccessExpression.object->semanticType->kind == semantic::Type::Kind::Struct ) ) {
					structName = memberAccessExpression.object->semanticType->name;
				}
				if( structName.empty() == false ) {
					std::unordered_map<std::string, llvm::StructType*>::iterator stIt = this->structTypes.find( structName );
					if( stIt == this->structTypes.end() ) {
						size_t genericParamPos = structName.find( '<' );
						if( genericParamPos != std::string::npos ) {
							std::string baseStructName( structName.substr( 0, genericParamPos ) );
							stIt = this->structTypes.find( baseStructName );
							if( stIt != this->structTypes.end() ) {
								structName = baseStructName;
							}
						}
					}
					else {
						size_t genericParamPos = structName.find( '<' );
						if( genericParamPos != std::string::npos ) {
							std::string baseStructName( structName.substr( 0, genericParamPos ) );
							std::unordered_map<std::string, llvm::StructType*>::iterator baseIt = this->structTypes.find( baseStructName );
							if( baseIt != this->structTypes.end() ) {
								structName = baseStructName;
								stIt = baseIt;
							}
						}
					}
					if( stIt != this->structTypes.end() ) {
						structType = stIt->second;
						if( pointerElementType->isPointerTy() && llvm::isa<llvm::GetElementPtrInst>( resolvedObjectPointer ) ) {
							resolvedObjectPointer = this->builder.CreateLoad(
								llvm::PointerType::getUnqual( this->context ),
								resolvedObjectPointer, "nested.obj.ptr" );
						}
					}
				}
			}
			if( structType && structName.empty() == false ) {
				std::unordered_map<std::string,std::unordered_map<std::string, unsigned int>>::iterator structIterator = this->structFieldIndices.find( structName );
				if( structIterator != this->structFieldIndices.end() ) {
					std::unordered_map<std::string, unsigned int>::iterator fieldIterator = structIterator->second.find( memberAccessExpression.member );
					if( fieldIterator != structIterator->second.end() ) {
						llvm::Value* fieldPointer = this->builder.CreateStructGEP( structType, resolvedObjectPointer, fieldIterator->second, "fieldptr" );
						return this->builder.CreateLoad( getPointeeType( fieldPointer ), fieldPointer, memberAccessExpression.member );
					}
				}
				
				// Not a field — check if it's a property method and auto-call
				semantic::TypeSharedPointer semanticType = this->analyzer.types().lookupType( structName );
				if( semanticType != nullptr && semanticType->kind == semantic::Type::Kind::Class ) {
					semantic::ClassTypeSharedPointer classType = std::static_pointer_cast<semantic::ClassType>( semanticType );
					semantic::MethodInfo* methodInformation = classType->findMethod( memberAccessExpression.member );
					if( methodInformation != nullptr && methodInformation->isProperty ) {
						std::string functionLookUpKey = fmt::format( "{}::{}", structName, memberAccessExpression.member );
						std::unordered_map<std::string, llvm::Function*>::iterator functionIterator = this->functions.find( functionLookUpKey );
						if( functionIterator == this->functions.end() ) {
							semantic::TypeSharedPointer baseClassType = classType->baseClass;
							while( baseClassType != nullptr && functionIterator == this->functions.end() ) {
								std::string baseFunctionKey = fmt::format( "{}::{}", baseClassType->name, memberAccessExpression.member );
								functionIterator = this->functions.find( baseFunctionKey );
								if( baseClassType->kind == semantic::Type::Kind::Class ) {
									baseClassType = std::static_pointer_cast<semantic::ClassType>( baseClassType )->baseClass;
								}
								else {
									break;
								}
							}
						}
						if( functionIterator != this->functions.end() ) {
							llvm::Function* targetPropertyFunction = functionIterator->second;
							llvm::Type* expectedSelfType = targetPropertyFunction->getArg( 0 )->getType();
							std::vector<llvm::Value*> propertyCallArguments = { this->generateImplicitCast( resolvedObjectPointer, expectedSelfType ) };
							if( targetPropertyFunction->getReturnType()->isVoidTy() ) {
								this->builder.CreateCall( targetPropertyFunction, propertyCallArguments );
								return nullptr;
							}
							std::string propertyCallLabel = fmt::format( "prop.{}", memberAccessExpression.member );
							return this->builder.CreateCall( targetPropertyFunction, propertyCallArguments, propertyCallLabel );
						}
					}
				}
				
				// Interface/Trait property dispatch via itable
				{
					std::string interfaceTypeName = this->resolveStructTypeName( memberAccessExpression.object );
					if( interfaceTypeName.empty() == false ) {
						semantic::TypeSharedPointer interfaceSemanticType = this->analyzer.types().lookupType( interfaceTypeName );
						if( interfaceSemanticType && ( interfaceSemanticType->kind == semantic::Type::Kind::Interface || interfaceSemanticType->kind == semantic::Type::Kind::Trait ) ) {
							semantic::InterfaceTypeSharedPointer interfaceType = std::dynamic_pointer_cast<semantic::InterfaceType>( interfaceSemanticType );
							if( interfaceType ) {
								semantic::MethodInfo* methodInfo = interfaceType->findMethod( memberAccessExpression.member );
								if( methodInfo && methodInfo->isProperty && methodInfo->interfaceTableIndex >= 0 ) {
									llvm::Value* fatPointer = this->generateExpression( memberAccessExpression.object );
									return this->generateInterfacePropertyAccess( fatPointer, interfaceType, methodInfo );
								}
							}
						}
					}
				}
				
				// Try builtin descriptor for property-like access
				llvm::Value* selfExpressionValue = this->generateExpression( memberAccessExpression.object );
				std::string resolvedStructTypeName = this->resolveStructTypeName( memberAccessExpression.object );
				std::vector<llvm::Value*> emptyArguments;
				std::vector<std::pair<std::string, llvm::Value*>> emptyKwargs;
				llvm::Value* builtinResult = this->tryBuiltInDescriptor( resolvedStructTypeName, memberAccessExpression.member, selfExpressionValue, emptyArguments, emptyKwargs );
				if( builtinResult != nullptr ) {
					return builtinResult;
				}
			}
		}
		llvm::Value* fallbackObjectValue = this->generateExpression( memberAccessExpression.object );
		if( fallbackObjectValue != nullptr && fallbackObjectValue->getType()->isPointerTy() ) {
			llvm::Type* fallbackElementType = getPointeeType( fallbackObjectValue );
			if( fallbackElementType->isStructTy() ) {
				llvm::StructType* fallbackStructType = llvm::cast<llvm::StructType>( fallbackElementType );
				std::string fallbackStructName = fallbackStructType->getName().str();
				std::unordered_map<std::string,std::unordered_map<std::string,unsigned int>>::iterator fallbackStructIterator = this->structFieldIndices.find( fallbackStructName );
				if( fallbackStructIterator != this->structFieldIndices.end() ) {
					std::unordered_map<std::string,unsigned int>::iterator fallbackFieldIterator = fallbackStructIterator->second.find( memberAccessExpression.member );
					if( fallbackFieldIterator != fallbackStructIterator->second.end() ) {
						llvm::Value* fallbackFieldPointer = this->builder.CreateStructGEP( fallbackStructType, fallbackObjectValue, fallbackFieldIterator->second, "fieldptr" );
						return this->builder.CreateLoad( getPointeeType( fallbackFieldPointer ), fallbackFieldPointer, memberAccessExpression.member );
					}
				}
				semantic::TypeSharedPointer fallbackSemanticType = this->analyzer.types().lookupType( fallbackStructName );
				if( fallbackSemanticType != nullptr && fallbackSemanticType->kind == semantic::Type::Kind::Class ) {
					semantic::ClassTypeSharedPointer fallbackClassType = std::static_pointer_cast<semantic::ClassType>( fallbackSemanticType );
					semantic::MethodInfo* fallbackMethodInfo = fallbackClassType->findMethod( memberAccessExpression.member );
					if( fallbackMethodInfo != nullptr && fallbackMethodInfo->isProperty ) {
						std::string fallbackFuncKey = fmt::format( "{}::{}", fallbackStructName, memberAccessExpression.member );
						std::unordered_map<std::string, llvm::Function*>::iterator fallbackFuncIterator = this->functions.find( fallbackFuncKey );
						if( fallbackFuncIterator != this->functions.end() ) {
							llvm::Function* fallbackPropertyFunction = fallbackFuncIterator->second;
							llvm::Type* fallbackExpectedSelfType = fallbackPropertyFunction->getArg( 0 )->getType();
							std::vector<llvm::Value*> fallbackPropertyArgs = { this->generateImplicitCast( fallbackObjectValue, fallbackExpectedSelfType ) };
							if( fallbackPropertyFunction->getReturnType()->isVoidTy() ) {
								this->builder.CreateCall( fallbackPropertyFunction, fallbackPropertyArgs );
								return nullptr;
							}
							return this->builder.CreateCall( fallbackPropertyFunction, fallbackPropertyArgs, fmt::format( "prop.{}", memberAccessExpression.member ) );
						}
					}
				}
			}
		}
		return fallbackObjectValue;
	}
	
	llvm::Value* LLVMCodegen::generateMethodCallExpression( ast::nodes::MethodCallExpression& methodCallExpression ) {
		
		// Memory<T> method calls: mem.get(i), mem.set(i,v), mem.free(), mem.copyTo(dest,len)
		// Resolve Memory<T> pointer and element type from either a local variable or a struct field
		llvm::Value* memoryPointer = nullptr;
		llvm::Type* memoryElementType = nullptr;
		if( methodCallExpression.object->kind == ast::Node::Kind::IdentifierExpression ) {
			ast::nodes::IdentifierExpression& identifierExpression = static_cast<ast::nodes::IdentifierExpression&>( *methodCallExpression.object );
			std::unordered_map<std::string,llvm::Type*>::iterator memoryElementTypeIterator = this->varMemoryElementTypes.find( identifierExpression.name );
			if( memoryElementTypeIterator != this->varMemoryElementTypes.end() ) {
				std::unordered_map<std::string, llvm::Value*>::iterator namedValueIterator = this->namedValues.find( identifierExpression.name );
				if( namedValueIterator != this->namedValues.end() ) {
					memoryElementType = memoryElementTypeIterator->second;
					llvm::PointerType* pointerType = llvm::PointerType::getUnqual( memoryElementType );
					memoryPointer = this->builder.CreateLoad( pointerType, namedValueIterator->second, "mem.ptr" );
				}
			}
		}
		else if( methodCallExpression.object->kind == ast::Node::Kind::MemberAccessExpression ) {
			ast::nodes::MemberAccessExpression& memberAccessExpression = static_cast<ast::nodes::MemberAccessExpression&>( *methodCallExpression.object );
			llvm::Value* objectPointer = this->resolveObjectPointer( memberAccessExpression.object );
			if( objectPointer && objectPointer->getType()->isPointerTy() ) {
				llvm::StructType* structType = nullptr;
				std::string structName;
				llvm::Type* pointerElementType = getPointeeType( objectPointer );
				if( pointerElementType->isStructTy() ) {
					structType = llvm::cast<llvm::StructType>( pointerElementType );
					structName = structType->getName().str();
				}
				else {
					structName = this->resolveStructTypeName( memberAccessExpression.object );
					if( structName.empty() && memberAccessExpression.object->semanticType &&
						( memberAccessExpression.object->semanticType->kind == semantic::Type::Kind::Class ||
						  memberAccessExpression.object->semanticType->kind == semantic::Type::Kind::Struct ) ) {
						structName = memberAccessExpression.object->semanticType->name;
					}
					if( structName.empty() == false ) {
						std::unordered_map<std::string, llvm::StructType*>::iterator stIt = this->structTypes.find( structName );
						if( stIt != this->structTypes.end() ) {
							structType = stIt->second;
						}
					}
				}
				if( structType ) {
					std::unordered_map<std::string,std::unordered_map<std::string,unsigned int>>::iterator structFieldIndexIterator = this->structFieldIndices.find( structName );
					if( structFieldIndexIterator != this->structFieldIndices.end() ) {
						std::unordered_map<std::string,unsigned int>::iterator fieldIndexIterator = structFieldIndexIterator->second.find( memberAccessExpression.member );
						if( fieldIndexIterator != structFieldIndexIterator->second.end() ) {
							llvm::Type* fieldType = structType->getElementType( fieldIndexIterator->second );
							if( fieldType->isPointerTy() ) {
								semantic::TypeSharedPointer semanticType = this->analyzer.types().lookupType( structName );
								bool isMemoryField = false;
								if( semanticType && semanticType->kind == semantic::Type::Kind::Class ) {
									semantic::ClassTypeSharedPointer classType = std::static_pointer_cast<semantic::ClassType>( semanticType );
									for( const semantic::FieldInfo& field : classType->fields ) {
										if( field.name == memberAccessExpression.member && field.type &&
											( field.type->qualified == semantic::qname::MEMORY || field.type->name == "Memory" || field.type->name.find( "Memory<" ) == 0 ||
										  semantic::qname::startsWith( field.type->qualified, semantic::qname::MEMORY + "<" ) ) ) {
											isMemoryField = true;
											break;
										}
									}
								}
								if( isMemoryField ) {
									memoryElementType = llvm::Type::getInt64Ty( this->context );
									if( semanticType && semanticType->kind == semantic::Type::Kind::Class ) {
										semantic::ClassTypeSharedPointer memClassType = std::static_pointer_cast<semantic::ClassType>( semanticType );
										for( const semantic::FieldInfo& fi : memClassType->fields ) {
											if( fi.name == memberAccessExpression.member && fi.type && fi.type->kind == semantic::Type::Kind::Class ) {
												semantic::ClassTypeSharedPointer memFieldType = std::static_pointer_cast<semantic::ClassType>( fi.type );
												if( memFieldType->typeSubstitutions.empty() == false ) {
													memoryElementType = this->toLLVMType( memFieldType->typeSubstitutions.begin()->second );
												}
												break;
											}
										}
									}
									llvm::Value* memberGep = this->builder.CreateStructGEP( structType, objectPointer, fieldIndexIterator->second, "mem.field.ptr" );
									memoryPointer = this->builder.CreateLoad( fieldType, memberGep, "mem.field.val" );
								}
							}
						}
					}
				}
			}
		}
		if( memoryPointer && memoryElementType ) {
			llvm::Type* int64Type = llvm::Type::getInt64Ty( this->context );
			if( methodCallExpression.method == "get" && methodCallExpression.arguments.size() == 1 ) {
				llvm::Value* indexValue = this->generateExpression( methodCallExpression.arguments[0] );
				if( indexValue != nullptr ) {
					indexValue = this->generateImplicitCast( indexValue, int64Type );
					llvm::Value* gepValue = this->builder.CreateGEP( memoryElementType, memoryPointer, indexValue, "mem.get.gep" );
					if( memoryElementType->isStructTy() ) {
						return gepValue;
					}
					return this->builder.CreateLoad( memoryElementType, gepValue, "mem.get.val" );
				}
				return nullptr;
			}
			if( methodCallExpression.method == "set" && methodCallExpression.arguments.size() == 2 ) {
				llvm::Value* indexValue = this->generateExpression( methodCallExpression.arguments[0] );
				llvm::Value* assignmentValue = this->generateExpression( methodCallExpression.arguments[1] );
				if( indexValue != nullptr && assignmentValue != nullptr ) {
					indexValue = this->generateImplicitCast( indexValue, int64Type );
					if( assignmentValue->getType()->isPointerTy() && memoryElementType->isStructTy() ) {
						assignmentValue = this->builder.CreateLoad( memoryElementType, assignmentValue, "mem.set.load" );
					}
					else {
						assignmentValue = this->generateImplicitCast( assignmentValue, memoryElementType );
					}
					llvm::Value* gepValue = this->builder.CreateGEP( memoryElementType, memoryPointer, indexValue, "mem.set.gep" );
					this->builder.CreateStore( assignmentValue, gepValue );
				}
				return nullptr;
			}
			if( methodCallExpression.method == "free" && methodCallExpression.arguments.empty() ) {
				llvm::Value* i8Pointer = this->builder.CreateBitCast( memoryPointer, llvm::PointerType::getUnqual( this->context ), "mem.free.cast" );
				this->builder.CreateCall( this->getOrCreateFree(), { i8Pointer } );
				return nullptr;
			}
			if( methodCallExpression.method == "copyTo" && methodCallExpression.arguments.size() == 2 ) {
				llvm::Value* destinationValue = this->generateExpression( methodCallExpression.arguments[0] );
				llvm::Value* lengthValue = this->generateExpression( methodCallExpression.arguments[1] );
				if( destinationValue != nullptr && lengthValue != nullptr ) {
					lengthValue = this->generateImplicitCast( lengthValue, int64Type );
					uint64_t elementSize = this->module->getDataLayout().getTypeAllocSize( memoryElementType );
					llvm::Value* totalBytes = this->builder.CreateMul( lengthValue, llvm::ConstantInt::get( int64Type, elementSize ), "copy.bytes" );
					llvm::Value* sourceI8 = this->builder.CreateBitCast( memoryPointer, llvm::PointerType::getUnqual( this->context ), "copy.src" );
					llvm::Value* destinationI8 = this->builder.CreateBitCast( destinationValue, llvm::PointerType::getUnqual( this->context ), "copy.dst" );
					this->builder.CreateCall( this->getOrCreateMemoryCopy(), { destinationI8, sourceI8, totalBytes, this->builder.getInt1( false ) } );
				}
				return nullptr;
			}
		}
		
		// Arena<T> method calls
		if( methodCallExpression.object->kind == ast::Node::Kind::IdentifierExpression ) {
			ast::nodes::IdentifierExpression& identifierExpression = static_cast<ast::nodes::IdentifierExpression&>( *methodCallExpression.object );
			std::unordered_map<std::string, llvm::Type*>::iterator arenaIterator = this->arenaElementTypes.find( identifierExpression.name );
			if( arenaIterator != this->arenaElementTypes.end() ) {
				llvm::Type* arenaElementType = arenaIterator->second;
				llvm::Type* int64Type = llvm::Type::getInt64Ty( this->context );
				std::unordered_map<std::string, llvm::Value*>::iterator namedValueIterator = this->namedValues.find( identifierExpression.name );
				if( namedValueIterator != this->namedValues.end() ) {
					llvm::Value* arenaAllocation = namedValueIterator->second;
					llvm::PointerType* pointerToElementType = llvm::PointerType::getUnqual( arenaElementType );
					llvm::Type* arenaStructType = llvm::StructType::get( this->context, { pointerToElementType, int64Type, int64Type } );
					if( methodCallExpression.method == "alloc" && methodCallExpression.arguments.empty() ) {
						llvm::Value* basePointer = this->builder.CreateLoad( pointerToElementType, this->builder.CreateStructGEP( arenaStructType, arenaAllocation, 0, "arena.base.ptr" ), "arena.base" );
						llvm::Value* countPointer = this->builder.CreateStructGEP( arenaStructType, arenaAllocation, 1, "arena.count.ptr" );
						llvm::Value* currentCount = this->builder.CreateLoad( int64Type, countPointer, "arena.count" );
						llvm::Value* slotGEP = this->builder.CreateGEP( arenaElementType, basePointer, currentCount, "arena.slot" );
						llvm::Value* incrementedCount = this->builder.CreateAdd( currentCount, llvm::ConstantInt::get( int64Type, 1 ), "arena.count.inc" );
						this->builder.CreateStore( incrementedCount, countPointer );
						return slotGEP;
					}
					if( methodCallExpression.method == "freeAll" && methodCallExpression.arguments.empty() ) {
						llvm::Value* countPointer = this->builder.CreateStructGEP( arenaStructType, arenaAllocation, 1, "arena.count.ptr" );
						this->builder.CreateStore( llvm::ConstantInt::get( int64Type, 0 ), countPointer );
						return nullptr;
					}
					if( methodCallExpression.method == "destroy" && methodCallExpression.arguments.empty() ) {
						llvm::Value* basePointer = this->builder.CreateLoad( pointerToElementType, this->builder.CreateStructGEP( arenaStructType, arenaAllocation, 0, "arena.base.ptr" ), "arena.base" );
						this->builder.CreateCall( this->getOrCreateFree(), { basePointer } );
						return nullptr;
					}
					if( methodCallExpression.method == "count" && methodCallExpression.arguments.empty() ) {
						llvm::Value* countPointer = this->builder.CreateStructGEP( arenaStructType, arenaAllocation, 1, "arena.count.ptr" );
						return this->builder.CreateLoad( int64Type, countPointer, "arena.count" );
					}
					if( methodCallExpression.method == "capacity" && methodCallExpression.arguments.empty() ) {
						llvm::Value* capacityPointer = this->builder.CreateStructGEP( arenaStructType, arenaAllocation, 2, "arena.cap.ptr" );
						return this->builder.CreateLoad( int64Type, capacityPointer, "arena.cap" );
					}
				}
			}
		}
		
		std::string typeName = this->resolveStructTypeName( methodCallExpression.object );
		if( typeName.empty() && methodCallExpression.object->semanticType &&
			( methodCallExpression.object->semanticType->kind == semantic::Type::Kind::Class ||
			  methodCallExpression.object->semanticType->kind == semantic::Type::Kind::Struct ) ) {
			typeName = methodCallExpression.object->semanticType->name;
		}
		if( typeName.empty() == false && typeName[0] == '?' ) {
			typeName = typeName.substr( 1 );
		}
		if( typeName.empty() == false ) {
			semantic::TypeSharedPointer semanticType = this->analyzer.types().lookupType( typeName );
			if( semanticType && ( semanticType->kind == semantic::Type::Kind::Interface || semanticType->kind == semantic::Type::Kind::Trait ) ) {
				llvm::Value* fatPointer = this->generateExpression( methodCallExpression.object );
				if( fatPointer ) {
					llvm::StructType* interfaceFatPointerType = this->getInterfaceFatPointerType();
					if( fatPointer->getType()->isStructTy() ) {
						llvm::StructType* valueStructType = llvm::cast<llvm::StructType>( fatPointer->getType() );
						if( valueStructType->getNumElements() == 2 &&
							valueStructType->getElementType( 0 )->isIntegerTy( 1 ) &&
							valueStructType->getElementType( 1 ) == interfaceFatPointerType ) {
							fatPointer = this->builder.CreateExtractValue( fatPointer, 1, "unwrap.nullable.iface" );
						}
					}
					else if( fatPointer->getType()->isPointerTy() ) {
						llvm::Type* pointeeType = getPointeeType( fatPointer );
						if( pointeeType->isStructTy() ) {
							llvm::StructType* pointeeStructType = llvm::cast<llvm::StructType>( pointeeType );
							if( pointeeStructType->getNumElements() == 2 &&
								pointeeStructType->getElementType( 0 )->isIntegerTy( 1 ) &&
								pointeeStructType->getElementType( 1 ) == interfaceFatPointerType ) {
								llvm::Value* loadedNullable = this->builder.CreateLoad( pointeeStructType, fatPointer, "load.nullable.iface" );
								fatPointer = this->builder.CreateExtractValue( loadedNullable, 1, "unwrap.nullable.iface" );
							}
						}
					}
					return this->generateInterfaceMethodCall( methodCallExpression, fatPointer, typeName );
				}
				return nullptr;
			}
		}
		bool isStaticStyleCall = false;
		llvm::Value* selfPointer = this->resolveObjectPointer( methodCallExpression.object );
		if( selfPointer && methodCallExpression.object->kind == ast::Node::Kind::MemberAccessExpression ) {
			llvm::Type* fieldType = getPointeeType( selfPointer );
			if( fieldType->isPointerTy() ) {
				selfPointer = this->builder.CreateLoad( fieldType, selfPointer, "field.obj" );
			}
		}
		if( selfPointer == nullptr && typeName.empty() == false ) {
			if( methodCallExpression.object->kind == ast::Node::Kind::IdentifierExpression ) {
				ast::nodes::IdentifierExpression& objectIdentifier = static_cast<ast::nodes::IdentifierExpression&>( *methodCallExpression.object );
				if( this->namedValues.find( objectIdentifier.name ) == this->namedValues.end() ) {
					semantic::TypeSharedPointer objectSemanticType = this->analyzer.types().lookupType( objectIdentifier.name );
					if( objectSemanticType && ( objectSemanticType->kind == semantic::Type::Kind::Class || objectSemanticType->kind == semantic::Type::Kind::Struct ) ) {
						isStaticStyleCall = true;
					}
				}
			}
			if( isStaticStyleCall == false ) {
				llvm::Value* selfValue = this->generateExpression( methodCallExpression.object );
				if( selfValue ) {
					bool objectIsClassInstance = false;
					if( methodCallExpression.object->kind == ast::Node::Kind::IdentifierExpression ) {
						ast::nodes::IdentifierExpression& objectIdent = static_cast<ast::nodes::IdentifierExpression&>( *methodCallExpression.object );
						if( objectIdent.resolvedSymbol && objectIdent.resolvedSymbol->typeref && objectIdent.resolvedSymbol->typeref->kind == semantic::Type::Kind::Class ) {
							if( typeName == "String" ) {
								if( this->aliveFlags.find( objectIdent.name ) != this->aliveFlags.end() ) {
									objectIsClassInstance = true;
								}
							}
							else {
								objectIsClassInstance = true;
							}
						}
					}
					else if( methodCallExpression.object->kind == ast::Node::Kind::ConstructExpression ) {
						objectIsClassInstance = true;
					}
					if( objectIsClassInstance && selfValue->getType()->isPointerTy() ) {
						std::unordered_map<std::string, llvm::StructType*>::iterator wrapperStructIt = this->structTypes.find( typeName );
						if( wrapperStructIt != this->structTypes.end() && wrapperStructIt->second->getNumElements() == 1 ) {
							llvm::Value* innerPtr = this->builder.CreateStructGEP( wrapperStructIt->second, selfValue, 0, "oop.self.field" );
							selfValue = this->builder.CreateLoad( wrapperStructIt->second->getElementType( 0 ), innerPtr, "oop.self.val" );
						}
					}
					std::vector<llvm::Value*> builtinArguments;
					for( ast::nodes::ExpressionSharedPointer& argumentNode : methodCallExpression.arguments ) {
						llvm::Value* argumentValue = this->generateExpression( argumentNode );
						if( argumentValue && argumentValue->getType()->isPointerTy() ) {
							bool argIsStringWrapper = false;
							if( argumentNode->kind == ast::Node::Kind::IdentifierExpression ) {
								ast::nodes::IdentifierExpression& argIdent = static_cast<ast::nodes::IdentifierExpression&>( *argumentNode );
								if( argIdent.resolvedSymbol && argIdent.resolvedSymbol->typeref &&
									argIdent.resolvedSymbol->typeref->name == "String" &&
									argIdent.resolvedSymbol->typeref->kind == semantic::Type::Kind::Class &&
									this->aliveFlags.find( argIdent.name ) != this->aliveFlags.end() ) {
									argIsStringWrapper = true;
								}
							}
							else if( argumentNode->kind == ast::Node::Kind::ConstructExpression ) {
								ast::nodes::ConstructExpression& argConstruct = static_cast<ast::nodes::ConstructExpression&>( *argumentNode );
								if( argConstruct.type && argConstruct.type->kind == ast::Node::Kind::SimpleType ) {
									ast::nodes::SimpleTypeNode& argTypeNode = static_cast<ast::nodes::SimpleTypeNode&>( *argConstruct.type );
									if( argTypeNode.name == "String" ) {
										argIsStringWrapper = true;
									}
								}
							}
							if( argIsStringWrapper ) {
								std::unordered_map<std::string, llvm::StructType*>::iterator strStructIt = this->structTypes.find( "String" );
								if( strStructIt != this->structTypes.end() && strStructIt->second->getNumElements() == 1 ) {
									llvm::Value* argFieldPtr = this->builder.CreateStructGEP( strStructIt->second, argumentValue, 0, "oop.arg.field" );
									argumentValue = this->builder.CreateLoad( strStructIt->second->getElementType( 0 ), argFieldPtr, "oop.arg.val" );
								}
							}
						}
						if( argumentValue ) {
							builtinArguments.push_back( argumentValue );
						}
					}
					std::vector<std::pair<std::string, llvm::Value*>> builtinKwargs;
					for( ast::nodes::KeywordArgument& kwarg : methodCallExpression.keywordArguments ) {
						llvm::Value* kwValue = this->generateExpression( kwarg.value );
						if( kwValue ) {
							builtinKwargs.push_back( { kwarg.name, kwValue } );
						}
					}
					llvm::Value* builtinResult = this->tryBuiltInDescriptor( typeName, methodCallExpression.method, selfValue, builtinArguments, builtinKwargs );
					if( builtinResult ) {
						return builtinResult;
					}
					selfPointer = selfValue;
				}
			}
		}
		if( selfPointer == nullptr && isStaticStyleCall == false ) {
			for( ast::nodes::ExpressionSharedPointer& argumentNode : methodCallExpression.arguments ) {
				this->generateExpression( argumentNode );
			}
			return nullptr;
		}
		this->ensureClassMethodsRegistered( typeName );
		std::string methodName = fmt::format( "{}::{}", typeName, methodCallExpression.method );
		std::unordered_map<std::string, llvm::Function*>::iterator functionIterator = this->functions.find( methodName );
		
		// Fallback: try overloaded method with param count suffix
		if( functionIterator != this->functions.end() && functionIterator->second->arg_size() != methodCallExpression.arguments.size() + 1 ) {
			std::string overloadName = fmt::format( "{}#{}", methodName, methodCallExpression.arguments.size() + 1 );
			std::unordered_map<std::string,llvm::Function*>::iterator overloadIterator = this->functions.find( overloadName );
			if( overloadIterator != this->functions.end() ) {
				functionIterator = overloadIterator;
			}
		}
		
		// Fallback: for monomorphized types like "Box<i64>", try base name "Box"
		if( functionIterator == this->functions.end() ) {
			size_t angleBracketPosition = typeName.find( '<' );
			if( angleBracketPosition != std::string::npos ) {
				std::string baseTypeName = typeName.substr( 0, angleBracketPosition );
				functionIterator = this->functions.find( fmt::format( "{}::{}", baseTypeName, methodCallExpression.method ) );
			}
		}
		
		// Fallback: walk parent class chain
		if( functionIterator == this->functions.end() ) {
			semantic::TypeSharedPointer semanticType = this->analyzer.types().lookupType( typeName );
			if( semanticType && semanticType->kind == semantic::Type::Kind::Class ) {
				semantic::ClassTypeSharedPointer classType = std::static_pointer_cast<semantic::ClassType>( semanticType );
				semantic::TypeSharedPointer baseClassType = classType->baseClass;
				while( baseClassType && functionIterator == this->functions.end() ) {
					this->ensureClassMethodsRegistered( baseClassType->name );
					std::string parentMethodName = fmt::format( "{}::{}", baseClassType->name, methodCallExpression.method );
					functionIterator = this->functions.find( parentMethodName );
					if( functionIterator == this->functions.end() ) {
						functionIterator = this->functions.find( fmt::format( "{}#{}", parentMethodName, methodCallExpression.arguments.size() + 1 ) );
					}
					baseClassType = std::static_pointer_cast<semantic::ClassType>( baseClassType )->baseClass;
				}
			}
		}
		if( functionIterator != this->functions.end() ) {
			llvm::Function* targetFunction = functionIterator->second;
			semantic::TypeSharedPointer semanticType = this->analyzer.types().lookupType( typeName );
			bool useVirtualDispatch = false;
			int virtualTableIndex = -1;
			bool isActuallyStaticMethod = false;
			if( semanticType && semanticType->kind == semantic::Type::Kind::Class ) {
				semantic::ClassTypeSharedPointer classType = std::static_pointer_cast<semantic::ClassType>( semanticType );
				semantic::MethodInfo* methodInfo = classType->findMethod( methodCallExpression.method );
				if( methodInfo && methodInfo->isVirtual && methodInfo->virtualTableIndex >= 0 ) {
					if( methodInfo->isFinal == false && classType->isFinal == false ) {
						useVirtualDispatch = true;
						virtualTableIndex = methodInfo->virtualTableIndex;
					}
				}
				if( methodInfo && methodInfo->isStatic ) {
					isActuallyStaticMethod = true;
				}
			}
			std::vector<llvm::Value*> callArguments;
			if( this->enumTypeNames.count( typeName ) ) {
				llvm::Value* enumTagValue = this->builder.CreateLoad( llvm::Type::getInt32Ty( this->context ), selfPointer, "enum.tag" );
				callArguments.push_back( enumTagValue );
			}
			else if( isStaticStyleCall ) {
				if( isActuallyStaticMethod == false && targetFunction->arg_size() > 0 ) {
					llvm::Type* expectedSelfType = targetFunction->getArg( 0 )->getType();
					if( expectedSelfType->isPointerTy() ) {
						callArguments.push_back( llvm::ConstantPointerNull::get( llvm::cast<llvm::PointerType>( expectedSelfType ) ) );
					}
				}
			}
			else {
				llvm::Type* expectedSelfType = targetFunction->getArg( 0 )->getType();
				callArguments.push_back( this->generateImplicitCast( selfPointer, expectedSelfType ) );
			}
			semantic::MethodInfo* semanticMethodInfo = nullptr;
			if( semanticType && semanticType->kind == semantic::Type::Kind::Class ) {
				semantic::ClassTypeSharedPointer methodClassType = std::static_pointer_cast<semantic::ClassType>( semanticType );
				semanticMethodInfo = methodClassType->findMethod( methodCallExpression.method );
			}
			semantic::FunctionTypeSharedPointer semanticFunctionType;
			if( semanticMethodInfo && semanticMethodInfo->type && semanticMethodInfo->type->kind == semantic::Type::Kind::Function ) {
				semanticFunctionType = std::static_pointer_cast<semantic::FunctionType>( semanticMethodInfo->type );
			}
			std::string resolvedMethodKey = functionIterator->first;
			std::unordered_map<std::string, FunctionParamInfo>::iterator methodParamInfoIter = this->functionParamInfos.find( resolvedMethodKey );
			if( methodParamInfoIter == this->functionParamInfos.end() ) {
				methodParamInfoIter = this->functionParamInfos.find( methodName );
			}
			int methodVariadicIdx = ( methodParamInfoIter != this->functionParamInfos.end() ) ? methodParamInfoIter->second.variadicIndex : -1;
			size_t methodFixedParamCount;
			if( methodVariadicIdx >= 0 ) {
				methodFixedParamCount = static_cast<size_t>( methodVariadicIdx );
			}
			else {
				methodFixedParamCount = targetFunction->arg_size() - ( isActuallyStaticMethod ? 0 : 1 );
			}
			size_t parameterIndex = isActuallyStaticMethod ? 0 : 1;
			for( size_t argIdx = 0; argIdx < methodCallExpression.arguments.size() && ( argIdx < methodFixedParamCount ); argIdx++ ) {
				ast::nodes::ExpressionSharedPointer& argumentNode = methodCallExpression.arguments[argIdx];
				llvm::Value* argumentValue = this->generateExpression( argumentNode );
				if( argumentValue == nullptr ) continue;
				if( parameterIndex < targetFunction->arg_size() ) {
					llvm::Type* expectedArgType = targetFunction->getArg( parameterIndex )->getType();
					if( expectedArgType == this->getInterfaceFatPointerType() && argumentValue->getType()->isPointerTy() && semanticFunctionType ) {
						size_t semanticParamIndex = isActuallyStaticMethod ? parameterIndex : parameterIndex - 1;
						if( semanticParamIndex < semanticFunctionType->parameterTypes.size() ) {
							semantic::TypeSharedPointer paramSemanticType = semanticFunctionType->parameterTypes[semanticParamIndex];
							if( paramSemanticType && ( paramSemanticType->kind == semantic::Type::Kind::Interface || paramSemanticType->kind == semantic::Type::Kind::Trait ) ) {
								std::string argClassName;
								llvm::Type* argPointeeType = getPointeeType( argumentValue );
								if( argPointeeType->isStructTy() ) {
									argClassName = llvm::cast<llvm::StructType>( argPointeeType )->getName().str();
								}
								if( argClassName.empty() && argumentValue->hasName() ) {
									std::unordered_map<std::string, std::string>::iterator vsIt = this->variableStructType.find( argumentValue->getName().str() );
									if( vsIt != this->variableStructType.end() ) {
										argClassName = vsIt->second;
									}
								}
								if( argClassName.empty() && this->lastConstructedClassName.empty() == false ) {
									argClassName = this->lastConstructedClassName;
								}
								if( argClassName.empty() && argumentNode->semanticType &&
									( argumentNode->semanticType->kind == semantic::Type::Kind::Class ||
									  argumentNode->semanticType->kind == semantic::Type::Kind::Struct ) ) {
									argClassName = argumentNode->semanticType->name;
								}
								if( argClassName.empty() == false ) {
									this->lastConstructedClassName.clear();
									argumentValue = this->createInterfaceFatPointer( argumentValue, argClassName, paramSemanticType->name );
									callArguments.push_back( argumentValue );
									parameterIndex++;
									continue;
								}
							}
						}
					}
					argumentValue = this->generateImplicitCast( argumentValue, expectedArgType );
				}
				callArguments.push_back( argumentValue );
				parameterIndex++;
			}
			if( methodVariadicIdx >= 0 ) {
				llvm::Type* int64Type = llvm::Type::getInt64Ty( this->context );
				size_t methodExtraArgStart = methodFixedParamCount;
				size_t methodExtraArgCount = ( methodCallExpression.arguments.size() > methodExtraArgStart ) ? methodCallExpression.arguments.size() - methodExtraArgStart : 0;
				bool methodHasVariadicForward = false;
				if( methodExtraArgCount > 0 ) {
					ast::nodes::ExpressionSharedPointer& firstExtraArg = methodCallExpression.arguments[methodExtraArgStart];
					if( firstExtraArg->kind == ast::Node::Kind::IdentifierExpression ) {
						ast::nodes::IdentifierExpression& argIdentifier = static_cast<ast::nodes::IdentifierExpression&>( *firstExtraArg );
						std::unordered_map<std::string, std::string>::iterator argStructTypeIter = this->variableStructType.find( argIdentifier.name );
						if( argStructTypeIter != this->variableStructType.end() && argStructTypeIter->second.find( "Args<" ) == 0 ) {
							methodHasVariadicForward = true;
						}
					}
				}
				if( methodHasVariadicForward ) {
					llvm::Value* forwardedArgs = this->generateExpression( methodCallExpression.arguments[methodExtraArgStart] );
					callArguments.push_back( forwardedArgs );
				}
				else {
					llvm::Type* variadicElemType = int64Type;
					if( methodParamInfoIter != this->functionParamInfos.end() && methodParamInfoIter->second.variadicElementLLVMType ) {
						variadicElemType = methodParamInfoIter->second.variadicElementLLVMType;
					}
					llvm::StructType* methodArgsStructType = llvm::StructType::get( this->context, {
						llvm::PointerType::getUnqual( variadicElemType ),
						int64Type,
						int64Type
					});
					llvm::Type* variadicElemPtrType = llvm::PointerType::getUnqual( variadicElemType );
					llvm::AllocaInst* methodArgsAlloca = this->createEntryBlockAllocation( this->currentFunction, "mpack.args", methodArgsStructType );
					if( methodExtraArgCount > 0 ) {
						llvm::ArrayType* dataArrayType = llvm::ArrayType::get( variadicElemType, methodExtraArgCount );
						llvm::AllocaInst* dataAlloca = this->createEntryBlockAllocation( this->currentFunction, "mpack.args.data", dataArrayType );
						for( size_t varArgIndex = 0; varArgIndex < methodExtraArgCount; varArgIndex++ ) {
							llvm::Value* argVal = this->generateExpression( methodCallExpression.arguments[methodExtraArgStart + varArgIndex] );
							if( argVal ) {
								argVal = this->generateImplicitCast( argVal, variadicElemType );
								llvm::Value* elemGep = this->builder.CreateConstGEP2_32( dataArrayType, dataAlloca, 0, static_cast<unsigned>( varArgIndex ), "mpack.args.gep" );
								this->builder.CreateStore( argVal, elemGep );
							}
						}
						llvm::Value* dataPtr = this->builder.CreateBitCast( dataAlloca, variadicElemPtrType, "mpack.args.ptr" );
						llvm::Value* dataFieldPtr = this->builder.CreateStructGEP( methodArgsStructType, methodArgsAlloca, 0, "mpack.args.data.field" );
						this->builder.CreateStore( dataPtr, dataFieldPtr );
					}
					else {
						llvm::Value* dataFieldPtr = this->builder.CreateStructGEP( methodArgsStructType, methodArgsAlloca, 0, "mpack.args.data.field" );
						this->builder.CreateStore( llvm::ConstantPointerNull::get( llvm::cast<llvm::PointerType>( variadicElemPtrType ) ), dataFieldPtr );
					}
					llvm::Value* countFieldPtr = this->builder.CreateStructGEP( methodArgsStructType, methodArgsAlloca, 1, "mpack.args.count.field" );
					this->builder.CreateStore( llvm::ConstantInt::get( int64Type, methodExtraArgCount ), countFieldPtr );
					llvm::Value* posFieldPtr = this->builder.CreateStructGEP( methodArgsStructType, methodArgsAlloca, 2, "mpack.args.pos.field" );
					this->builder.CreateStore( llvm::ConstantInt::get( int64Type, 0 ), posFieldPtr );
					callArguments.push_back( methodArgsAlloca );
				}
			}
			else {
				for( size_t argIdx = methodFixedParamCount; argIdx < methodCallExpression.arguments.size(); argIdx++ ) {
					llvm::Value* argumentValue = this->generateExpression( methodCallExpression.arguments[argIdx] );
					if( argumentValue == nullptr ) continue;
					if( parameterIndex < targetFunction->arg_size() ) {
						llvm::Type* expectedArgType = targetFunction->getArg( parameterIndex )->getType();
						argumentValue = this->generateImplicitCast( argumentValue, expectedArgType );
					}
					callArguments.push_back( argumentValue );
					parameterIndex++;
				}
			}
			if( useVirtualDispatch && selfPointer->getType()->isPointerTy() ) {
				llvm::Type* objectPointerElementType = getPointeeType( selfPointer );
				if( objectPointerElementType->isStructTy() ) {
					llvm::StructType* structType = llvm::cast<llvm::StructType>( objectPointerElementType );
					llvm::Value* vtablePointerAddress = this->builder.CreateStructGEP( structType, selfPointer, 0, "vtable.ptr.ptr" );
					llvm::Value* vtablePointer = this->builder.CreateLoad( llvm::PointerType::getUnqual( this->context ), vtablePointerAddress, "vtable.ptr" );
					llvm::FunctionType* voidFunctionType = llvm::FunctionType::get( llvm::Type::getVoidTy( this->context ), false );
					llvm::PointerType* functionPointerType = llvm::PointerType::getUnqual( voidFunctionType );
					llvm::PointerType* vtableType = llvm::PointerType::getUnqual( functionPointerType );
					llvm::Value* vtableCast = this->builder.CreateBitCast( vtablePointer, vtableType, "vtable.cast" );
					llvm::Value* functionPointerAddress = this->builder.CreateGEP( functionPointerType, vtableCast, llvm::ConstantInt::get( llvm::Type::getInt32Ty( this->context ), virtualTableIndex ), "vfunc.ptr.ptr" );
					llvm::Value* virtualFunctionPointer = this->builder.CreateLoad( functionPointerType, functionPointerAddress, "vfunc.ptr" );
					llvm::FunctionType* actualFunctionType = targetFunction->getFunctionType();
					llvm::Value* castedVirtualFunction = this->builder.CreateBitCast( virtualFunctionPointer, llvm::PointerType::getUnqual( actualFunctionType ), "vfunc.cast" );
					if( actualFunctionType->getReturnType()->isVoidTy() ) {
						this->createCallOrInvoke( actualFunctionType, castedVirtualFunction, callArguments );
						return nullptr;
					}
					return this->createCallOrInvoke( actualFunctionType, castedVirtualFunction, callArguments, "vcalltmp" );
				}
			}
			if( targetFunction->getReturnType()->isVoidTy() ) {
				this->createCallOrInvoke( targetFunction, callArguments );
				return nullptr;
			}
			llvm::Value* methodCallResult = this->createCallOrInvoke( targetFunction, callArguments, "mcalltmp" );
			if( typeName.find( '<' ) != std::string::npos && methodCallResult ) {
				semantic::TypeSharedPointer monomorphizedType = this->analyzer.types().lookupType( typeName );
				if( monomorphizedType ) {
					semantic::MethodInfo* monomorphizedMethodInfo = nullptr;
					if( monomorphizedType->kind == semantic::Type::Kind::Class ) {
						monomorphizedMethodInfo = std::static_pointer_cast<semantic::ClassType>( monomorphizedType )->findMethod( methodCallExpression.method );
					}
					else if( monomorphizedType->kind == semantic::Type::Kind::Struct ) {
						monomorphizedMethodInfo = std::static_pointer_cast<semantic::StructType>( monomorphizedType )->findMethod( methodCallExpression.method );
					}
					if( monomorphizedMethodInfo && monomorphizedMethodInfo->type && monomorphizedMethodInfo->type->kind == semantic::Type::Kind::Function ) {
						semantic::FunctionTypeSharedPointer monomorphizedFunctionType = std::static_pointer_cast<semantic::FunctionType>( monomorphizedMethodInfo->type );
						if( monomorphizedFunctionType->returnType ) {
							llvm::Type* expectedReturnType = this->toLLVMType( monomorphizedFunctionType->returnType );
							if( expectedReturnType && expectedReturnType != methodCallResult->getType() ) {
								methodCallResult = this->generateImplicitCast( methodCallResult, expectedReturnType );
							}
						}
					}
				}
			}
			return methodCallResult;
		}
		llvm::Value* fallbackSelfValue = selfPointer ? selfPointer : this->generateExpression( methodCallExpression.object );
		if( fallbackSelfValue && fallbackSelfValue->getType()->isPointerTy() ) {
			bool fallbackIsClassInstance = false;
			if( methodCallExpression.object->kind == ast::Node::Kind::IdentifierExpression ) {
				ast::nodes::IdentifierExpression& fallbackObjIdent = static_cast<ast::nodes::IdentifierExpression&>( *methodCallExpression.object );
				if( fallbackObjIdent.resolvedSymbol && fallbackObjIdent.resolvedSymbol->typeref && fallbackObjIdent.resolvedSymbol->typeref->kind == semantic::Type::Kind::Class ) {
					if( typeName == "String" ) {
						if( this->aliveFlags.find( fallbackObjIdent.name ) != this->aliveFlags.end() ) {
							fallbackIsClassInstance = true;
						}
					}
					else {
						fallbackIsClassInstance = true;
					}
				}
			}
			else if( methodCallExpression.object->kind == ast::Node::Kind::ConstructExpression ) {
				fallbackIsClassInstance = true;
			}
			if( fallbackIsClassInstance ) {
				std::unordered_map<std::string, llvm::StructType*>::iterator fallbackStructIt = this->structTypes.find( typeName );
				if( fallbackStructIt != this->structTypes.end() && fallbackStructIt->second->getNumElements() == 1 ) {
					llvm::Value* fallbackFieldPtr = this->builder.CreateStructGEP( fallbackStructIt->second, fallbackSelfValue, 0, "oop.self.field" );
					fallbackSelfValue = this->builder.CreateLoad( fallbackStructIt->second->getElementType( 0 ), fallbackFieldPtr, "oop.self.val" );
				}
			}
		}
		std::vector<llvm::Value*> fallbackBuiltinArguments;
		for( ast::nodes::ExpressionSharedPointer& argumentNode : methodCallExpression.arguments ) {
			llvm::Value* argumentValue = this->generateExpression( argumentNode );
			if( argumentValue && argumentValue->getType()->isPointerTy() ) {
				bool argIsStringWrapper = false;
				if( argumentNode->kind == ast::Node::Kind::IdentifierExpression ) {
					ast::nodes::IdentifierExpression& argIdent = static_cast<ast::nodes::IdentifierExpression&>( *argumentNode );
					if( argIdent.resolvedSymbol && argIdent.resolvedSymbol->typeref &&
						argIdent.resolvedSymbol->typeref->name == "String" &&
						argIdent.resolvedSymbol->typeref->kind == semantic::Type::Kind::Class &&
						this->aliveFlags.find( argIdent.name ) != this->aliveFlags.end() ) {
						argIsStringWrapper = true;
					}
				}
				else if( argumentNode->kind == ast::Node::Kind::ConstructExpression ) {
					ast::nodes::ConstructExpression& argConstruct = static_cast<ast::nodes::ConstructExpression&>( *argumentNode );
					if( argConstruct.type && argConstruct.type->kind == ast::Node::Kind::SimpleType ) {
						ast::nodes::SimpleTypeNode& argTypeNode = static_cast<ast::nodes::SimpleTypeNode&>( *argConstruct.type );
						if( argTypeNode.name == "String" ) {
							argIsStringWrapper = true;
						}
					}
				}
				if( argIsStringWrapper ) {
					std::unordered_map<std::string, llvm::StructType*>::iterator strStructIt = this->structTypes.find( "String" );
					if( strStructIt != this->structTypes.end() && strStructIt->second->getNumElements() == 1 ) {
						llvm::Value* argFieldPtr = this->builder.CreateStructGEP( strStructIt->second, argumentValue, 0, "oop.arg.field" );
						argumentValue = this->builder.CreateLoad( strStructIt->second->getElementType( 0 ), argFieldPtr, "oop.arg.val" );
					}
				}
			}
			if( argumentValue ) {
				fallbackBuiltinArguments.push_back( argumentValue );
			}
		}
		std::vector<std::pair<std::string, llvm::Value*>> fallbackKwargs;
		for( ast::nodes::KeywordArgument& kwarg : methodCallExpression.keywordArguments ) {
			llvm::Value* kwValue = this->generateExpression( kwarg.value );
			if( kwValue ) {
				fallbackKwargs.push_back( { kwarg.name, kwValue } );
			}
		}
		llvm::Value* builtinResult = this->tryBuiltInDescriptor( typeName, methodCallExpression.method, fallbackSelfValue, fallbackBuiltinArguments, fallbackKwargs );
		if( builtinResult ) {
			return builtinResult;
		}
		if( methodCallExpression.method == "toString" && methodCallExpression.arguments.empty() && fallbackSelfValue ) {
			if( fallbackSelfValue->getType() == llvm::PointerType::getUnqual( this->context ) ) {
				return fallbackSelfValue;
			}
			if( fallbackSelfValue->getType()->isPointerTy() ) {
				llvm::Type* pointeeType = getPointeeType( fallbackSelfValue );
				if( pointeeType->isStructTy() ) {
					llvm::StructType* structType = llvm::cast<llvm::StructType>( pointeeType );
					std::string className = structType->getName().str();
					std::string metaGlobalName( fmt::format( "_AE_meta_{}", className ) );
					llvm::GlobalVariable* metaGlobal = this->module->getGlobalVariable( metaGlobalName, true );
					std::string reprPrefix;
					if( metaGlobal && metaGlobal->hasInitializer() ) {
						if( llvm::ConstantDataArray* cda = llvm::dyn_cast<llvm::ConstantDataArray>( metaGlobal->getInitializer() ) ) {
							reprPrefix = cda->getAsCString().str();
						}
					}
					if( reprPrefix.empty() ) {
						reprPrefix = className + " object";
					}
					std::string reprFmt( fmt::format( "<{} at 0x%lx>", reprPrefix ) );
					size_t reprBufSize = reprPrefix.size() + 32;
					llvm::Type* i8PtrType = llvm::PointerType::getUnqual( this->context );
					llvm::Type* i64Type = llvm::Type::getInt64Ty( this->context );
					llvm::FunctionCallee snprintfFn = this->module->getOrInsertFunction( "snprintf", llvm::FunctionType::get( llvm::Type::getInt32Ty( this->context ), { i8PtrType, i64Type, i8PtrType }, true ) );
					llvm::FunctionCallee mallocFn = this->module->getOrInsertFunction( "malloc", llvm::FunctionType::get( i8PtrType, { i64Type }, false ) );
					llvm::Value* buf = this->builder.CreateCall( mallocFn, { llvm::ConstantInt::get( i64Type, reprBufSize ) }, "repr.buf" );
					llvm::Value* ptrVal = this->builder.CreatePtrToInt( fallbackSelfValue, i64Type, "ptr.int" );
					llvm::Constant* reprFmtStr = this->builder.CreateGlobalStringPtr( reprFmt, "repr.fmt" );
					this->builder.CreateCall( snprintfFn, { buf, llvm::ConstantInt::get( i64Type, reprBufSize ), reprFmtStr, ptrVal } );
					return buf;
				}
			}
		}
		if( methodCallExpression.method == "hashCode" && methodCallExpression.arguments.empty() && fallbackSelfValue ) {
			if( fallbackSelfValue->getType()->isIntegerTy() ) {
				return this->generateImplicitCast( fallbackSelfValue, llvm::Type::getInt64Ty( this->context ) );
			}
			if( fallbackSelfValue->getType()->isPointerTy() ) {
				return this->builder.CreatePtrToInt( fallbackSelfValue, llvm::Type::getInt64Ty( this->context ), "ptr.hash" );
			}
		}
		return nullptr;
	}
	
	void LLVMCodegen::generateReturnStatement( ast::nodes::ReturnStatement& returnStatement ) {
		
		// If returning a heap-allocated variable by name, mark it not-alive to prevent auto-free
		if( returnStatement.value && returnStatement.value->kind == ast::Node::Kind::IdentifierExpression ) {
			ast::nodes::IdentifierExpression& returnIdentifier = static_cast<ast::nodes::IdentifierExpression&>( *returnStatement.value );
			std::unordered_map<std::string, llvm::Value*>::iterator aliveFlagIterator = this->aliveFlags.find( returnIdentifier.name );
			if( aliveFlagIterator != this->aliveFlags.end() ) {
				this->builder.CreateStore( llvm::ConstantInt::getFalse( this->context ), aliveFlagIterator->second );
			}
		}
		
		// Evaluate return value FIRST (before cleanup/pop — callee frames must stay on stack during evaluation)
		llvm::Value* returnValue = nullptr;
		if( returnStatement.value ) {
			returnValue = this->generateExpression( returnStatement.value );
			if( returnValue ) {
				if( returnValue->getType()->isStructTy() &&
					this->currentFunction->getReturnType()->isPointerTy() ) {
					llvm::Value* objectPointer = this->resolveObjectPointer( returnStatement.value );
					if( objectPointer ) {
						returnValue = objectPointer;
					}
				}
				if( this->currentFunction->getReturnType() == this->getInterfaceFatPointerType() &&
					returnValue->getType()->isPointerTy() && this->currentClassName.empty() == false ) {
					std::string funcName = this->currentFunction->getName().str();
					std::string methodPrefix( fmt::format( "_UR_{}_", this->currentClassName ) );
					if( funcName.find( methodPrefix ) == 0 ) {
						std::string methodName = funcName.substr( methodPrefix.size() );
						size_t hashPos = methodName.find( '#' );
						if( hashPos != std::string::npos ) {
							methodName = methodName.substr( 0, hashPos );
						}
						semantic::TypeSharedPointer classType = this->analyzer.types().lookupType( this->currentClassName );
						if( classType && classType->kind == semantic::Type::Kind::Class ) {
							semantic::ClassType* classTypePtr = static_cast<semantic::ClassType*>( classType.get() );
							for( const semantic::MethodInfo& method : classTypePtr->methods ) {
								if( method.name == methodName && method.type &&
									method.type->kind == semantic::Type::Kind::Function ) {
									semantic::FunctionType* funcType = static_cast<semantic::FunctionType*>( method.type.get() );
									if( funcType->returnType &&
										( funcType->returnType->kind == semantic::Type::Kind::Interface ||
										  funcType->returnType->kind == semantic::Type::Kind::Trait ) ) {
										this->lastTargetInterfaceName = funcType->returnType->name;
									}
									break;
								}
							}
						}
					}
				}
				returnValue = this->generateImplicitCast( returnValue, this->currentFunction->getReturnType() );
			}
		}

		// Pop frame from call stack (after expression evaluation, before actual return)
		if( this->currentFunctionEmittedPushFrame ) {
			this->emitPopFrame();
		}
		
		// Emit auto-free cleanup before return
		this->emitAllScopeCleanups();
		
		// Emit deferred statements in reverse order before returning
		this->emitAllDefers();
		
		// Run scheduler before main returns (flush pending async tasks)
		if( this->programUsesAsync && this->currentFunction && this->currentFunction->getName() == "main" ) {
			this->builder.CreateCall( this->getOrCreateRunScheduler() );
		}
		
		// Inside async task wrapper: return value -> uraniteTaskComplete(task, value) + ret void
		if( this->asyncTaskArgument ) {
			llvm::Value* finalTaskResult = llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->context ), 0 );
			if( returnValue ) {
				finalTaskResult = this->generateImplicitCast( returnValue, llvm::Type::getInt64Ty( this->context ) );
			}
			llvm::Function* taskCompleteFunction = this->module->getFunction( "runtimeComplete" );
			if( taskCompleteFunction != nullptr ) {
				this->builder.CreateCall( taskCompleteFunction, { finalTaskResult } );
			}
			else {
				taskCompleteFunction = this->module->getFunction( "uraniteTaskComplete" );
				if( taskCompleteFunction == nullptr ) {
					llvm::FunctionType* taskCompleteType = llvm::FunctionType::get(
						llvm::Type::getVoidTy( this->context ),
						{
							llvm::PointerType::getUnqual( this->context ),
							llvm::Type::getInt64Ty( this->context )
						},
						false
					);
					taskCompleteFunction = llvm::Function::Create(
						taskCompleteType,
						llvm::Function::ExternalLinkage,
						"uraniteTaskComplete",
						this->getModule()
					);
				}
				llvm::Value* returnTaskArg = this->asyncTaskArgument;
				if( taskCompleteFunction->getArg( 0 )->getType()->isIntegerTy() && returnTaskArg->getType()->isPointerTy() ) {
					returnTaskArg = this->builder.CreatePtrToInt( returnTaskArg, taskCompleteFunction->getArg( 0 )->getType(), "task.i64" );
				}
				this->builder.CreateCall( taskCompleteFunction, { returnTaskArg, finalTaskResult } );
			}
			this->builder.CreateRetVoid();
			return;
		}
		if( returnValue ) {
			this->builder.CreateRet( returnValue );
		}
		else if( returnStatement.value ) {
			if( this->currentFunction->getReturnType()->isVoidTy() ) {
				this->builder.CreateRetVoid();
			}
			else {
				this->builder.CreateRet( llvm::Constant::getNullValue( this->currentFunction->getReturnType() ) );
			}
		}
		else {
			if( this->currentFunction->getReturnType()->isVoidTy() ) {
				this->builder.CreateRetVoid();
			}
			else {
				this->builder.CreateRet( llvm::Constant::getNullValue( this->currentFunction->getReturnType() ) );
			}
		}
	}
	
	void LLVMCodegen::generateStatement( const ast::nodes::StatementSharedPointer& statement ) {
		if( statement == nullptr ) {
			return;
		}
		switch( statement->kind ) {
			case ast::Node::Kind::VariableStatement:
				this->generateVariableStatement( static_cast<ast::nodes::VariableStatement&>( *statement ) );
				break;
			case ast::Node::Kind::AssignmentStatement:
				this->generateAssignmentStatement( static_cast<ast::nodes::AssignStatement&>( *statement ) );
				break;
			case ast::Node::Kind::ReturnStatement:
				this->generateReturnStatement( static_cast<ast::nodes::ReturnStatement&>( *statement ) );
				break;
			case ast::Node::Kind::IfStatement:
				this->generateIfStatement( static_cast<ast::nodes::IfStatement&>( *statement ) );
				break;
			case ast::Node::Kind::MatchStatement:
				this->generateMatchStatement( static_cast<ast::nodes::MatchStatement&>( *statement ) );
				break;
			case ast::Node::Kind::SwitchStatement:
				this->generateSwitchStatement( static_cast<ast::nodes::SwitchStatement&>( *statement ) );
				break;
			case ast::Node::Kind::ForStatement:
				this->generateForStatement( static_cast<ast::nodes::ForStatement&>( *statement ) );
				break;
			case ast::Node::Kind::WhileStatement:
				this->generateWhileStatement( static_cast<ast::nodes::WhileStatement&>( *statement ) );
				break;
			case ast::Node::Kind::ExpressionStatement:
				this->generateExpression( static_cast<ast::nodes::ExpressionStatement&>( *statement ).expression );
				break;
			case ast::Node::Kind::BlockStatement:
				for( const ast::nodes::StatementSharedPointer& blockStatement : static_cast<ast::nodes::BlockStatement&>( *statement ).statements ) {
					this->generateStatement( blockStatement );
				}
				break;
			case ast::Node::Kind::UnsafeBlock:
				for( const ast::nodes::StatementSharedPointer& unsafeStatement : static_cast<ast::nodes::UnsafeBlockStatement&>( *statement ).body ) {
					this->generateStatement( unsafeStatement );
				}
				break;
			case ast::Node::Kind::BreakStatement:
				if( this->breakTargets.empty() == false ) {
					this->emitCurrentScopeDefers();
					if( this->scopeCleanupStack.empty() == false ) {
						this->emitScopeCleanup( this->scopeCleanupStack.back() );
					}
					this->builder.CreateBr( this->breakTargets.top() );
				}
				break;
			case ast::Node::Kind::ContinueStatement:
				if( this->continueTargets.empty() == false ) {
					this->emitCurrentScopeDefers();
					if( this->scopeCleanupStack.empty() == false ) {
						this->emitScopeCleanup( this->scopeCleanupStack.back() );
					}
					this->builder.CreateBr( this->continueTargets.top() );
				}
				break;
			case ast::Node::Kind::PassStatement:
				break;
			case ast::Node::Kind::InlineAssemblyStatement: {
				this->generateInlineAssemblyStatement( static_cast<ast::nodes::InlineAssemblyStatement&>( *statement ) );
				break;
			}
			case ast::Node::Kind::DeleteStatement: {
				ast::nodes::DeleteStatement& deleteStatement = static_cast<ast::nodes::DeleteStatement&>( *statement );
				if( deleteStatement.expression->kind == ast::Node::Kind::IndexExpression ) {
					ast::nodes::IndexExpression& indexExpr = static_cast<ast::nodes::IndexExpression&>( *deleteStatement.expression );
					std::string className = this->resolveStructTypeName( indexExpr.object );
					if( className.empty() == false ) {
						std::string removeMethodName = fmt::format( "{}::remove", className );
						std::unordered_map<std::string, llvm::Function*>::iterator functionIterator = this->functions.find( removeMethodName );
						if( functionIterator != this->functions.end() && functionIterator->second != nullptr ) {
							llvm::Value* selfPointer = this->resolveObjectPointer( indexExpr.object );
							llvm::Value* keyValue = this->generateExpression( indexExpr.index );
							if( selfPointer && keyValue ) {
								llvm::Function* removeFunction = functionIterator->second;
								if( removeFunction->arg_size() >= 2 ) {
									llvm::Type* expectedKeyType = ( removeFunction->arg_begin() + 1 )->getType();
									keyValue = this->generateImplicitCast( keyValue, expectedKeyType );
								}
								this->builder.CreateCall( removeFunction, { selfPointer, keyValue } );
							}
							break;
						}
					}
				}
				llvm::Value* expressionValue = this->generateExpression( deleteStatement.expression );
				if( expressionValue == nullptr ) {
					break;
				}
				llvm::Value* pointerValue = expressionValue;
				if( expressionValue->getType()->isIntegerTy( 64 ) ) {
					pointerValue = this->builder.CreateIntToPtr( expressionValue, llvm::PointerType::getUnqual( this->context ), "delete.inttoptr" );
				}
				else if( expressionValue->getType()->isPointerTy() == false ) {
					break;
				}
				std::string typeName = this->resolveStructTypeName( deleteStatement.expression );
				if( typeName.empty() == false ) {
					std::string destroyFunctionName = fmt::format( "{}::destroy", typeName );
					std::unordered_map<std::string, llvm::Function*>::iterator functionIterator = this->functions.find( destroyFunctionName );
					if( functionIterator != this->functions.end() && functionIterator->second != nullptr ) {
						llvm::Function* destroyFunction = functionIterator->second;
						if( destroyFunction->arg_size() > 0 ) {
							llvm::Value* selfPointer = pointerValue;
							llvm::Type* expectedArgumentType = destroyFunction->arg_begin()->getType();
							if( selfPointer->getType() != expectedArgumentType ) {
								selfPointer = this->builder.CreateBitCast( selfPointer, expectedArgumentType, "destroy.cast" );
							}
							this->builder.CreateCall( destroyFunction, { selfPointer } );
						}
						else {
							this->builder.CreateCall( destroyFunction, {} );
						}
					}
				}
				llvm::Value* rawPointerForFree = this->builder.CreateBitCast( pointerValue, llvm::PointerType::getUnqual( this->context ), "rawfree" );
				this->builder.CreateCall( this->getOrCreateFree(), { rawPointerForFree } );
				if( deleteStatement.expression->kind == ast::Node::Kind::IdentifierExpression ) {
					ast::nodes::IdentifierExpression& deleteIdentifier = static_cast<ast::nodes::IdentifierExpression&>( *deleteStatement.expression );
					std::unordered_map<std::string, llvm::Value*>::iterator aliveFlagIterator = this->aliveFlags.find( deleteIdentifier.name );
					if( aliveFlagIterator != this->aliveFlags.end() ) {
						this->builder.CreateStore( llvm::ConstantInt::getFalse( this->context ), aliveFlagIterator->second );
					}
				}
				break;
			}
			case ast::Node::Kind::TryCatchStatement:
				this->generateTryCatchStatement( static_cast<ast::nodes::TryCatchStatement&>( *statement ) );
				break;
			case ast::Node::Kind::ThrowStatement:
				this->generateThrowStatement( static_cast<ast::nodes::ThrowStatement&>( *statement ) );
				break;	
			case ast::Node::Kind::DeferStatement: {
				ast::nodes::DeferStatement& deferStatement = static_cast<ast::nodes::DeferStatement&>( *statement );
				this->addDefer( deferStatement.body );
				break;
			}
			default: {
				this->diagnostic.warning(
					statement->source,
					fmt::format( "codegen: unhandled statement kind {}", static_cast<int>( statement->kind ) )
				);
				break;
			}
		}
	}
		
	void LLVMCodegen::generateStructDeclaration( ast::nodes::StructDeclaration& structDeclaration ) {
		semantic::StructTypeSharedPointer structType = std::dynamic_pointer_cast<semantic::StructType>( this->analyzer.types().lookupType( structDeclaration.name ) );
		if( structType == nullptr ) {
			return;
		}
		this->getOrCreateStructType( structDeclaration.name, structType );
		this->currentClassName = structDeclaration.name;
		for( ast::nodes::DeclarationSharedPointer& structMethod : structDeclaration.methods ) {
			if( structMethod->kind != ast::Node::Kind::FunctionDeclaration ) {
				continue;
			}
			std::vector<llvm::Type*> functionParameterTypes;
			ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *structMethod );
			llvm::Type* functionReturnType = this->resolveAstType( functionDeclaration.returnType );
			for( ast::nodes::FunctionParameterSharedPointer& parameterNode : functionDeclaration.parameters ) {
				if( parameterNode->isSelf ) {
					functionParameterTypes.push_back( llvm::PointerType::getUnqual( this->structTypes[structDeclaration.name] ) );
				}
				else if( parameterNode->type ) {
					std::string parameterTypeName;
					if( parameterNode->type->kind == ast::Node::Kind::SimpleType ) {
						parameterTypeName = static_cast<ast::nodes::SimpleTypeNode&>( *parameterNode->type ).name;
					}
					else if( parameterNode->type->kind == ast::Node::Kind::GenericType ) {
						parameterTypeName = static_cast<ast::nodes::GenericTypeNode&>( *parameterNode->type ).name;
					}
					semantic::TypeSharedPointer parameterResolvedType = parameterTypeName.empty() ? nullptr : this->analyzer.types().lookupType( parameterTypeName );
					if( parameterResolvedType ) {
						llvm::Type* parameterLLVMType = this->toLLVMType( parameterResolvedType );
						if( parameterLLVMType->isStructTy() && parameterLLVMType != this->getInterfaceFatPointerType() ) {
							parameterLLVMType = llvm::PointerType::getUnqual( parameterLLVMType );
						}
						functionParameterTypes.push_back( parameterLLVMType );
					}
					else {
						functionParameterTypes.push_back( llvm::Type::getInt64Ty( this->context ) );
					}
				}
			}
			std::string functionMangledName = this->mangleName( functionDeclaration.name, structDeclaration.name );
			std::string functionName( fmt::format( "{}::{}", structDeclaration.name, functionDeclaration.name ) );
			llvm::FunctionType* functionType = llvm::FunctionType::get( functionReturnType, functionParameterTypes, false );
			llvm::Function* function = llvm::Function::Create( functionType, llvm::Function::ExternalLinkage, functionMangledName, this->getModule() );
			
			this->functions[functionName] = function;
			if( functionDeclaration.body.empty() == false ) {
				function->setPersonalityFn( this->getOrCreatePersonalityFunction() );
				llvm::BasicBlock* functionEntryBlock = llvm::BasicBlock::Create( this->context, "entry", function );
				this->builder.SetInsertPoint( functionEntryBlock );
				this->currentFunction = function;
				this->namedValues.clear();
				this->variableStructType.clear();
				this->varMemoryElementTypes.clear();
				this->arenaElementTypes.clear();
				size_t parameterIndex = 0;
				for( ast::nodes::FunctionParameterSharedPointer& functionParameter : functionDeclaration.parameters ) {
					if( parameterIndex >= function->arg_size() ) {
						break;
					}
					std::string functionParameterName = functionParameter->isSelf ? "self" : functionParameter->name;
					llvm::Argument* functionArgument = function->getArg( parameterIndex );
					functionArgument->setName( functionParameterName );
					
					llvm::Value* parameterAlloca = this->createEntryBlockAllocation( function, functionParameterName, functionArgument->getType() );
					
					this->builder.CreateStore( functionArgument, parameterAlloca );
					this->namedValues[functionParameterName] = parameterAlloca;
					
					if( functionParameter->isSelf == false && functionParameter->type ) {
						std::string parameterTypeNameInAST;
						if( functionParameter->type->kind == ast::Node::Kind::SimpleType ) {
							parameterTypeNameInAST = static_cast<ast::nodes::SimpleTypeNode&>( *functionParameter->type ).name;
						}
						else if( functionParameter->type->kind == ast::Node::Kind::GenericType ) {
							ast::nodes::GenericTypeNode& genericTypeNode = static_cast<ast::nodes::GenericTypeNode&>( *functionParameter->type );
							parameterTypeNameInAST = genericTypeNode.name;
							if( genericTypeNode.name == "Memory" ) {
								llvm::Type* elementType = llvm::Type::getInt64Ty( this->context );
								if( genericTypeNode.typeArguments.empty() == false ) {
									elementType = this->resolveAstType( genericTypeNode.typeArguments[0] );
								}
								this->varMemoryElementTypes[functionParameterName] = elementType;
							}
							if( genericTypeNode.name == "Arena" ) {
								llvm::Type* elementType = llvm::Type::getInt64Ty( this->context );
								if( genericTypeNode.typeArguments.empty() == false ) {
									elementType = this->resolveAstType( genericTypeNode.typeArguments[0] );
									elementType = unwrapStructPointer( elementType, genericTypeNode.typeArguments[0], this->structTypes );
								}
								this->arenaElementTypes[functionParameterName] = elementType;
							}
						}
						if( parameterTypeNameInAST.empty() == false ) {
							this->variableStructType[functionParameterName] = parameterTypeNameInAST;
						}
					}
					parameterIndex++;
				}
				
				// Auto-assign property parameters in constructor
				if( functionDeclaration.name == structDeclaration.name ) {
					std::unordered_map<std::string, llvm::Value*>::iterator selfIterator = this->namedValues.find( "self" );
					if( selfIterator != this->namedValues.end() ) {
						llvm::Value* selfPointer = this->builder.CreateLoad( getPointeeType( selfIterator->second ), selfIterator->second, "self.ptr" );
						std::unordered_map<std::string, llvm::StructType*>::iterator structTypeIterator = this->structTypes.find( structDeclaration.name );
						if( structTypeIterator != this->structTypes.end() ) {
							llvm::StructType* structLLVMType = structTypeIterator->second;
							int structFieldIndex = 0;
							for( ast::nodes::FunctionParameterSharedPointer& propertyParameter : functionDeclaration.parameters ) {
								if( propertyParameter->isProperty && propertyParameter->isSelf == false ) {
									std::unordered_map<std::string, llvm::Value*>::iterator propertyValueIterator = this->namedValues.find( propertyParameter->name );
									if( propertyValueIterator != this->namedValues.end() && structFieldIndex < (int)structLLVMType->getNumElements() ) {
										std::string propertyDerefName( fmt::format( "{}.deref", propertyParameter->name ) );
										std::string propertyFieldName( fmt::format( "{}.field", propertyParameter->name ) );
										std::string propertyValueName( fmt::format( "{}.val", propertyParameter->name ) );
										llvm::Value* propertyValue = this->builder.CreateLoad( getPointeeType( propertyValueIterator->second ), propertyValueIterator->second, propertyValueName );
										llvm::Value* propertyGEP = this->builder.CreateStructGEP( structLLVMType, selfPointer, structFieldIndex, propertyFieldName );
										llvm::Type* propertyFieldType = structLLVMType->getElementType( structFieldIndex );
										if( propertyValue->getType()->isPointerTy() && propertyFieldType->isStructTy() && getPointeeType( propertyValue ) == propertyFieldType ) {
											propertyValue = this->builder.CreateLoad( propertyFieldType, propertyValue, propertyDerefName );
										}
										this->builder.CreateStore( propertyValue, propertyGEP );
									}
								}
								if( propertyParameter->isSelf == false ) {
									structFieldIndex++;
								}
							}
						}
					}
				}
				for( ast::nodes::StatementSharedPointer& functionBodyStatement : functionDeclaration.body ) {
					this->generateStatement( functionBodyStatement );
				}
				llvm::BasicBlock* functionLastBlock = this->builder.GetInsertBlock();
				if( functionLastBlock && functionLastBlock->getTerminator() == nullptr ) {
					if( function->getReturnType()->isVoidTy() ) {
						this->builder.CreateRetVoid();
					}
					else {
						this->builder.CreateRet( llvm::Constant::getNullValue( function->getReturnType() ) );
					}
				}
				this->currentFunction = nullptr;
			}
		}
		for( ast::nodes::DeclarationSharedPointer& structNestedDeclaration : structDeclaration.nestedDeclarations ) {
			this->generateDeclaration( structNestedDeclaration );
		}
		if( structType ) {
			std::string metaGlobalName( fmt::format( "_AE_meta_{}", structDeclaration.name ) );
			if( this->getModule()->getGlobalVariable( metaGlobalName, true ) == nullptr ) {
				std::string qualname = structType->package.empty() ? structDeclaration.name : fmt::format( "{}.{}", structType->package, structDeclaration.name );
				std::string metaValue( fmt::format( "{} struct", qualname ) );
				llvm::Constant* metaConstant = llvm::ConstantDataArray::getString( this->context, metaValue, true );
				new llvm::GlobalVariable( *this->getModule(), metaConstant->getType(), true, llvm::GlobalValue::PrivateLinkage, metaConstant, metaGlobalName );
			}
		}
		this->currentClassName.clear();
	}
	
	void LLVMCodegen::generateSwitchStatement( ast::nodes::SwitchStatement& switchStatement ) {
		llvm::Value* switchSubjectValue = generateExpression( switchStatement.subject );
		if( switchSubjectValue != nullptr ) {
			if( switchSubjectValue->getType()->isIntegerTy() == false ) {
				switchSubjectValue = this->generateImplicitCast( switchSubjectValue, llvm::Type::getInt64Ty( this->context ) );
			}
			llvm::Function* switchParentFunction = this->builder.GetInsertBlock()->getParent();
			llvm::BasicBlock* switchMergeBlock = llvm::BasicBlock::Create( this->context, "switch.end", switchParentFunction );
			llvm::BasicBlock* switchDefaultBlock = switchMergeBlock;
			struct SwitchCaseInformation {
				llvm::BasicBlock* block;
				ast::nodes::SwitchCaseNode* node;
			};
			std::vector<SwitchCaseInformation> switchCaseTable;
			
			// Stage 1: Initialize BasicBlocks for every branch in the switch statement
			for( const ast::nodes::SwitchCaseNodeSharedPointer& caseNode : switchStatement.cases ) {
				std::string switchBlockName = fmt::format( "switch.{}", caseNode->isDefault ? "default" : "case" );
				llvm::BasicBlock* switchCaseBlock = llvm::BasicBlock::Create( this->context, switchBlockName, switchParentFunction );
				if( caseNode->isDefault ) {
					switchDefaultBlock = switchCaseBlock;
				}
				switchCaseTable.push_back( {switchCaseBlock, caseNode.get()} );
			}
			
			llvm::SwitchInst* switchInstruction = this->builder.CreateSwitch( switchSubjectValue, switchDefaultBlock, switchCaseTable.size() );
			
			// Stage 2: Pattern registration and body generation for each switch case
			for( const SwitchCaseInformation& caseInfo : switchCaseTable ) {
				if( caseInfo.node->isDefault == false && caseInfo.node->pattern ) {
					this->builder.SetInsertPoint( this->builder.GetInsertBlock() );
					llvm::Value* switchPatternValue = this->generateExpression( caseInfo.node->pattern );
					if( switchPatternValue ) {
						switchPatternValue = this->generateImplicitCast( switchPatternValue, switchSubjectValue->getType() );
						if( llvm::ConstantInt* switchConstantPattern = llvm::dyn_cast<llvm::ConstantInt>( switchPatternValue ) ) {
							switchInstruction->addCase( switchConstantPattern, caseInfo.block );
						}
					}
				}
				bool switchHasTerminated = false;
				this->builder.SetInsertPoint( caseInfo.block );
				for( ast::nodes::StatementSharedPointer switchBodyStatement : caseInfo.node->body ) {
					if( switchBodyStatement && switchBodyStatement->kind == ast::Node::Kind::BreakStatement ) {
						this->builder.CreateBr( switchMergeBlock );
						switchHasTerminated = true;
						break;
					}
					this->generateStatement( switchBodyStatement );
				}
				if( switchHasTerminated == false && this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
					this->builder.CreateBr( switchMergeBlock );
				}
			}
			this->builder.SetInsertPoint( switchMergeBlock );
		}
	}
	
	void LLVMCodegen::generateThrowStatement( ast::nodes::ThrowStatement& statement ) {
		llvm::Value* throwExpressionValue = this->generateExpression( statement.expression );
		if( throwExpressionValue ) {
			std::string thrownTypeName = "Exception";
			if( throwExpressionValue->getType()->isPointerTy() ) {
				llvm::Type* pointeeType = getPointeeType( throwExpressionValue );
				if( pointeeType->isStructTy() ) {
					thrownTypeName = llvm::cast<llvm::StructType>( pointeeType )->getName().str();
					if( this->isThrowableClass( thrownTypeName ) && statement.source ) {
						this->injectTracebackInfo( throwExpressionValue, thrownTypeName, statement.source );
					}
				}
			}
			llvm::Value* castedExceptionValue = throwExpressionValue;
			if( throwExpressionValue->getType() != llvm::PointerType::getUnqual( this->context ) ) {
				castedExceptionValue = this->builder.CreateBitCast( throwExpressionValue, llvm::PointerType::getUnqual( this->context ), "exc.cast" );
			}
			llvm::Value* typeNameValue = this->builder.CreateGlobalStringPtr( thrownTypeName, "throw.typename" );
			llvm::Function* uraniteThrow = this->getOrCreateUraniteThrow();
			if( this->landingPads.empty() ) {
				this->builder.CreateCall( uraniteThrow, { castedExceptionValue, typeNameValue } );
				this->builder.CreateUnreachable();
			}
			else {
				llvm::BasicBlock* throwUnreachableBlock = llvm::BasicBlock::Create( this->context, "throw.unreachable", this->currentFunction );
				this->builder.CreateInvoke( uraniteThrow, throwUnreachableBlock, this->landingPads.top(), { castedExceptionValue, typeNameValue } );
				this->builder.SetInsertPoint( throwUnreachableBlock );
				this->builder.CreateUnreachable();
			}
		}
	}
	
	void LLVMCodegen::generateTryCatchStatement( ast::nodes::TryCatchStatement& statement ) {
		llvm::Function* currentFunctionHandle = this->currentFunction;
		currentFunctionHandle->setPersonalityFn( this->getOrCreatePersonalityFunction() );
		llvm::BasicBlock* catchPadBlock = llvm::BasicBlock::Create( this->context, "catch.pad", currentFunctionHandle );
		llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create( this->context, "try.merge", currentFunctionHandle );
		this->landingPads.push( catchPadBlock );
		this->generateBlock( statement.tryBody );
		this->landingPads.pop();
		if( this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
			if( statement.finallyBody.empty() == false ) {
				this->generateBlock( statement.finallyBody );
			}
			if( this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
				this->builder.CreateBr( mergeBlock );
			}
		}
		this->builder.SetInsertPoint( catchPadBlock );
		llvm::StructType* landingPadResultType = llvm::StructType::get( this->context, {
			llvm::PointerType::getUnqual( this->context ),
			llvm::Type::getInt32Ty( this->context )
		} );
		llvm::LandingPadInst* landingPad = this->builder.CreateLandingPad( landingPadResultType, 1, "lp" );
		landingPad->addClause( llvm::ConstantPointerNull::get( llvm::PointerType::getUnqual( this->context ) ) );
		llvm::Value* excPtr = this->builder.CreateExtractValue( landingPad, 0, "exc.ptr" );
		llvm::Function* beginCatch = this->getOrCreateUraniteBeginCatch();
		llvm::Value* uraniteObject = this->builder.CreateCall( beginCatch, { excPtr }, "caught.obj" );
		for( const ast::nodes::ExceptionClause& exceptionClause : statement.exceptionClauses ) {
			if( exceptionClause.variableName.empty() == false ) {
				llvm::Type* catchVarType = llvm::PointerType::getUnqual( this->context );
				if( exceptionClause.exceptionTypes.empty() == false ) {
					std::string exceptionTypeName = static_cast<ast::nodes::SimpleTypeNode&>( *exceptionClause.exceptionTypes[0] ).name;
					std::unordered_map<std::string, llvm::StructType*>::iterator structIterator = this->structTypes.find( exceptionTypeName );
					if( structIterator != this->structTypes.end() ) {
						catchVarType = llvm::PointerType::getUnqual( structIterator->second );
					}
				}
				llvm::Value* typedObject = this->builder.CreateBitCast( uraniteObject, catchVarType, "caught.typed" );
				llvm::Value* exceptionVariableAllocation = this->createEntryBlockAllocation(
					currentFunctionHandle,
					exceptionClause.variableName,
					catchVarType
				);
				this->builder.CreateStore( typedObject, exceptionVariableAllocation );
				this->namedValues[exceptionClause.variableName] = exceptionVariableAllocation;
			}
			this->generateBlock( exceptionClause.body );
		}
		if( this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
			llvm::Function* endCatch = this->getOrCreateUraniteEndCatch();
			this->builder.CreateCall( endCatch, { excPtr } );
			if( statement.finallyBody.empty() == false ) {
				this->generateBlock( statement.finallyBody );
			}
			if( this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
				this->builder.CreateBr( mergeBlock );
			}
		}
		this->builder.SetInsertPoint( mergeBlock );
	}
	
	llvm::Value* LLVMCodegen::generateUnaryExpression( ast::nodes::UnaryExpression& expression ) {
		if( expression.operation == token::Type::KeywordAddressof ) {
			if( expression.operand->kind == ast::Node::Kind::IdentifierExpression ) {
				ast::nodes::IdentifierExpression& identifierExpression = static_cast<ast::nodes::IdentifierExpression&>( *expression.operand );
				std::unordered_map<std::string,llvm::Function*>::iterator functionIterator = this->functions.find( identifierExpression.name );
				if( functionIterator != this->functions.end() ) {
					return this->builder.CreatePtrToInt( functionIterator->second, llvm::Type::getInt64Ty( this->context ), "addressof" );
				}
				std::string mangledName = fmt::format( "_UR_{}", identifierExpression.name );
				llvm::Function* moduleFunction = this->module->getFunction( mangledName );
				if( moduleFunction ) {
					return this->builder.CreatePtrToInt( moduleFunction, llvm::Type::getInt64Ty( this->context ), "addressof" );
				}
			}
			llvm::Value* objectPointer = this->resolveObjectPointer( expression.operand );
			if( objectPointer ) {
				return this->builder.CreatePtrToInt( objectPointer, llvm::Type::getInt64Ty( this->context ), "addressof" );
			}
			return nullptr;
		}
		llvm::Value* unaryOperand = this->generateExpression( expression.operand );
		if( unaryOperand == nullptr ) {
			return nullptr;
		}
		
		// Operator overloading: unary - → negate(), unary ! → not() on class types
		if( unaryOperand->getType()->isPointerTy() ) {
			std::string unaryTypeName = resolveStructTypeName( expression.operand );
			if( unaryTypeName.empty() == false ) {
				std::string unaryMethodName;
				if( expression.operation == token::Type::Minus ) {
					unaryMethodName = "negate";
				}
				if( unaryMethodName.empty() == false ) {
					std::string unaryFullMethodName = fmt::format( "{}::{}", unaryTypeName, unaryMethodName );
					std::unordered_map<std::string,llvm::Function*>::iterator unaryFunctionIterator = this->functions.find( unaryFullMethodName );
					if( unaryFunctionIterator != this->functions.end() ) {
						llvm::Value* unarySelfPointer = this->resolveObjectPointer( expression.operand );
						if( unarySelfPointer ) {
							std::string unaryCallResultName = fmt::format( "op.{}", unaryMethodName );
							return this->builder.CreateCall( unaryFunctionIterator->second, {unarySelfPointer}, unaryCallResultName );
						}
					}
				}
			}
		}
		switch( expression.operation ) {
			case token::Type::Ampersand: {
				
				// Take address - operand should be an alloca
				// If it's already a pointer from a load, return it
				return unaryOperand;
			}
			case token::Type::Bang:
			case token::Type::KeywordNot: {
				if( unaryOperand->getType()->isIntegerTy( 1 ) ) {
					return this->builder.CreateNot( unaryOperand, "nottmp" );
				}
				return this->builder.CreateICmpEQ( unaryOperand, llvm::Constant::getNullValue( unaryOperand->getType() ), "nottmp" );
			}
			case token::Type::Minus: {
				if( unaryOperand->getType()->isFloatingPointTy() ) {
					return this->builder.CreateFNeg( unaryOperand, "negtmp" );
				}
				return this->builder.CreateNeg( unaryOperand, "negtmp" );
			}
			case token::Type::Star: {
				
				// Dereference
				if( unaryOperand->getType()->isPointerTy() ) {
					return this->builder.CreateLoad( getPointeeType( unaryOperand ), unaryOperand, "deref" );
				}
				return unaryOperand;
			}
			case token::Type::Tilde: {
				return this->builder.CreateNot( unaryOperand, "bnottmp" );
			}
			default:
				return unaryOperand;
		}
	}
	
	void LLVMCodegen::generateVariableStatement( ast::nodes::VariableStatement& statement ) {
		llvm::Type* variableType = llvm::Type::getInt64Ty( this->context ); // default
		std::string variableStructTypeName;
		if( statement.type ) {
			
			// Handle OptionalType (?Type) — unwrap to get inner type
			ast::nodes::TypeNode* variableTypeNode = statement.type.get();
			bool variableIsOptional = false;
			if( variableTypeNode->kind == ast::Node::Kind::OptionalType ) {
				variableIsOptional = true;
				variableTypeNode = static_cast<ast::nodes::OptionalTypeNode*>( variableTypeNode )->innerType.get();
			}
			if( variableTypeNode->kind == ast::Node::Kind::CallableType ) {
				ast::nodes::CallableTypeNode& callableTypeNode = static_cast<ast::nodes::CallableTypeNode&>( *variableTypeNode );
				std::string callableReturnTypeName;
				if( callableTypeNode.returnType && callableTypeNode.returnType->kind == ast::Node::Kind::SimpleType ) {
					callableReturnTypeName = static_cast<ast::nodes::SimpleTypeNode&>( *callableTypeNode.returnType ).name;
				}
				else if( callableTypeNode.returnType && callableTypeNode.returnType->kind == ast::Node::Kind::GenericType ) {
					callableReturnTypeName = static_cast<ast::nodes::GenericTypeNode&>( *callableTypeNode.returnType ).name;
				}
				semantic::TypeSharedPointer callableResolvedReturnType = callableReturnTypeName.empty() ? this->analyzer.types().getVoid() : this->analyzer.types().lookupType( callableReturnTypeName );
				if( callableResolvedReturnType == nullptr ) {
					callableResolvedReturnType = this->analyzer.types().getVoid();
				}
				std::vector<semantic::TypeSharedPointer> callableParamSemanticTypes;
				for( const ast::nodes::TypeNodeSharedPointer& callableParamTypeNode : callableTypeNode.parameterTypes ) {
					std::string callableParamName;
					if( callableParamTypeNode->kind == ast::Node::Kind::SimpleType ) {
						callableParamName = static_cast<ast::nodes::SimpleTypeNode&>( *callableParamTypeNode ).name;
					}
					else if( callableParamTypeNode->kind == ast::Node::Kind::GenericType ) {
						callableParamName = static_cast<ast::nodes::GenericTypeNode&>( *callableParamTypeNode ).name;
					}
					semantic::TypeSharedPointer callableResolvedParamType = callableParamName.empty() ? this->analyzer.types().getInteger64() : this->analyzer.types().lookupType( callableParamName );
					callableParamSemanticTypes.push_back( callableResolvedParamType ? callableResolvedParamType : this->analyzer.types().getInteger64() );
				}
				semantic::TypeSharedPointer callableSemanticType = this->analyzer.types().makeCallable( callableResolvedReturnType, callableParamSemanticTypes );
				variableType = this->toLLVMType( callableSemanticType );
			}
			else if( variableTypeNode->kind == ast::Node::Kind::GenericType ) {
				ast::nodes::GenericTypeNode* genericTypeNodePtr = static_cast<ast::nodes::GenericTypeNode*>( variableTypeNode );
				std::string genericTypeName = genericTypeNodePtr->name;
				if( genericTypeName == "Future" && genericTypeNodePtr->typeArguments.empty() == false ) {
					semantic::TypeSharedPointer futureResolvedType = this->analyzer.types().lookupType( "Future" );
					if( futureResolvedType && futureResolvedType->kind == semantic::Type::Kind::Class ) {
						variableType = this->toLLVMType( futureResolvedType );
						if( variableType->isStructTy() ) {
							variableType = llvm::PointerType::getUnqual( variableType );
						}
						variableStructTypeName = genericTypeName;
					}
					else {
						variableType = llvm::Type::getInt64Ty( this->context );
					}
				}
				else if( genericTypeName == "Generator" && genericTypeNodePtr->typeArguments.empty() == false ) {
					variableType = this->resolveAstType( statement.type );
				}
				else if( genericTypeName.empty() == false ) {
					semantic::TypeSharedPointer genericResolvedType = this->analyzer.types().lookupType( genericTypeName );
					if( genericResolvedType ) {
						if( variableIsOptional ) {
							semantic::TypeSharedPointer optionalType = this->analyzer.types().makeOptional( genericResolvedType );
							variableType = this->toLLVMType( optionalType );
						}
						else {
							variableType = this->toLLVMType( genericResolvedType );
							
							// Class/struct variables hold pointers, not values
							if( variableType->isStructTy() && ( genericResolvedType->kind == semantic::Type::Kind::Class || genericResolvedType->kind == semantic::Type::Kind::Struct ) ) {
								variableType = llvm::PointerType::getUnqual( variableType );
							}
						}
						if( genericResolvedType->kind == semantic::Type::Kind::Class || genericResolvedType->kind == semantic::Type::Kind::Struct || genericResolvedType->kind == semantic::Type::Kind::Enum || genericResolvedType->kind == semantic::Type::Kind::Interface || genericResolvedType->kind == semantic::Type::Kind::Trait ) {
							variableStructTypeName = genericTypeName;
						}
					}
				}
			}
			else if( variableTypeNode->kind == ast::Node::Kind::MetaType ) {
				ast::nodes::MetaTypeNode& metaTypeNode = static_cast<ast::nodes::MetaTypeNode&>( *variableTypeNode );
				std::string metaInnerTypeName;
				if( metaTypeNode.innerType && metaTypeNode.innerType->kind == ast::Node::Kind::SimpleType ) {
					metaInnerTypeName = static_cast<ast::nodes::SimpleTypeNode&>( *metaTypeNode.innerType ).name;
				}
				semantic::TypeSharedPointer metaResolvedInnerType = metaInnerTypeName.empty() ? this->analyzer.types().getVoid() : this->analyzer.types().lookupType( metaInnerTypeName );
				if( metaResolvedInnerType ) {
					semantic::TypeSharedPointer metaSemanticType = this->analyzer.types().makeMeta( metaResolvedInnerType );
					variableType = this->toLLVMType( metaSemanticType );
				}
			}
			else if( variableTypeNode->kind == ast::Node::Kind::SimpleType ) {
				std::string simpleTypeName = static_cast<ast::nodes::SimpleTypeNode*>( variableTypeNode )->name;
				if( simpleTypeName.empty()  == false ) {
					semantic::TypeSharedPointer simpleResolvedType = this->analyzer.types().lookupType( simpleTypeName );
					if( simpleResolvedType ) {
						if( variableIsOptional ) {
							semantic::TypeSharedPointer optionalType = this->analyzer.types().makeOptional( simpleResolvedType );
							variableType = this->toLLVMType( optionalType );
						}
						else {
							variableType = this->toLLVMType( simpleResolvedType );
							
							// Class/struct variables hold pointers, not values
							if( variableType->isStructTy() && ( simpleResolvedType->kind == semantic::Type::Kind::Class || simpleResolvedType->kind == semantic::Type::Kind::Struct ) ) {
								variableType = llvm::PointerType::getUnqual( variableType );
							}
						}
						if( simpleResolvedType->kind == semantic::Type::Kind::Class || simpleResolvedType->kind == semantic::Type::Kind::Struct || simpleResolvedType->kind == semantic::Type::Kind::Enum || simpleResolvedType->kind == semantic::Type::Kind::Interface || simpleResolvedType->kind == semantic::Type::Kind::Trait ) {
							variableStructTypeName = simpleTypeName;
						}
					}
				}
			}
		}
		llvm::Value* variableInitializerValue = nullptr;
		if( statement.initializer ) {
			if( statement.initializer->kind == ast::Node::Kind::NoneLiteral && variableType->isStructTy() ) {
				variableInitializerValue = llvm::Constant::getNullValue( variableType );
			}
			else {
				variableInitializerValue = this->generateExpression( statement.initializer );
				if( variableInitializerValue && statement.type == nullptr ) {
					variableType = variableInitializerValue->getType();
				}
			}
		}
		if( statement.initializer && statement.initializer->kind == ast::Node::Kind::ConstructExpression ) {
			ast::nodes::ConstructExpression& constructExpression = static_cast<ast::nodes::ConstructExpression&>( *statement.initializer );
			std::string constructTypeName;
			if( constructExpression.type->kind == ast::Node::Kind::SimpleType ) {
				constructTypeName = static_cast<ast::nodes::SimpleTypeNode&>( *constructExpression.type ).name;
			}
			else if( constructExpression.type->kind == ast::Node::Kind::GenericType ) {
				constructTypeName = static_cast<ast::nodes::GenericTypeNode&>( *constructExpression.type ).name;
			}
			
			// Handle Memory<T> from new Memory<T>(cap)
			if( constructTypeName == "Memory" && variableInitializerValue ) {
				llvm::Type* memoryElementLLVMType = this->lastMemoryElementType;
				if( memoryElementLLVMType == nullptr ) {
					memoryElementLLVMType = llvm::Type::getInt64Ty( this->context );
				}
				llvm::PointerType* memoryPointerLLVMType = llvm::PointerType::getUnqual( memoryElementLLVMType );
				llvm::Value* memoryAllocaValue = this->createEntryBlockAllocation( this->currentFunction, statement.name, memoryPointerLLVMType );
				this->builder.CreateStore( variableInitializerValue, memoryAllocaValue );
				this->namedValues[statement.name] = memoryAllocaValue;
				this->varMemoryElementTypes[statement.name] = memoryElementLLVMType;
				this->lastMemoryElementType = nullptr;
				return;
			}
			if( constructTypeName == "Arena" && variableInitializerValue ) {
				
				// initValue is already an alloca pointer to arena struct from generateConstructExpression
				this->namedValues[statement.name] = variableInitializerValue;
				llvm::Type* arenaElementLLVMType = this->lastArenaElementType;
				if( arenaElementLLVMType == nullptr ) {
					arenaElementLLVMType = llvm::Type::getInt64Ty( this->context );
				}
				this->arenaElementTypes[statement.name] = arenaElementLLVMType;
				this->lastArenaElementType = nullptr;
				return;
			}
		}
		if( statement.type && variableInitializerValue && variableInitializerValue->getType()->isPointerTy() ) {
			std::string declaredTypeDescriptorName;
			if( statement.type->kind == ast::Node::Kind::SimpleType ) {
				declaredTypeDescriptorName = static_cast<ast::nodes::SimpleTypeNode&>( *statement.type ).name;
			}
			else if( statement.type->kind == ast::Node::Kind::GenericType ) {
				declaredTypeDescriptorName = static_cast<ast::nodes::GenericTypeNode&>( *statement.type ).name;
			}
			
			// Handle Memory<T> from declared type (e.g. Memory<E> oldData = self.data)
			if( declaredTypeDescriptorName == "Memory" ) {
				llvm::Type* memoryElementLLVMType = nullptr;
				if( statement.type->kind == ast::Node::Kind::GenericType ) {
					ast::nodes::GenericTypeNode& genericTypeNode = static_cast<ast::nodes::GenericTypeNode&>( *statement.type );
					if( genericTypeNode.typeArguments.empty() == false ) {
						memoryElementLLVMType = this->resolveAstType( genericTypeNode.typeArguments[0] );
					}
				}
				if( memoryElementLLVMType == nullptr ) {
					memoryElementLLVMType = llvm::Type::getInt64Ty( this->context );
				}
				llvm::Type* memoryPointerLLVMType = variableInitializerValue->getType();
				llvm::Value* memoryAllocaValue = this->createEntryBlockAllocation( this->currentFunction, statement.name, memoryPointerLLVMType );
				this->builder.CreateStore( variableInitializerValue, memoryAllocaValue );
				this->namedValues[statement.name] = memoryAllocaValue;
				this->varMemoryElementTypes[statement.name] = memoryElementLLVMType;
				return;
			}
		}
		if( variableInitializerValue && variableInitializerValue->getType()->isPointerTy() ) {
			llvm::Type* initializerPointerElementLLVMType = getPointeeType( variableInitializerValue );

			// Handle struct/class values from ConstructExpression
			bool isStructVariable = initializerPointerElementLLVMType->isStructTy();
			if( isStructVariable == false && variableStructTypeName.empty() == false ) {
				isStructVariable = this->structTypes.find( variableStructTypeName ) != this->structTypes.end();
			}
			if( isStructVariable ) {
				llvm::Value* structAllocaValue = this->createEntryBlockAllocation( this->currentFunction, statement.name, variableInitializerValue->getType() );
				this->builder.CreateStore( variableInitializerValue, structAllocaValue );
				this->namedValues[statement.name] = structAllocaValue;
				if( variableStructTypeName.empty() == false ) {
					this->variableStructType[statement.name] = variableStructTypeName;
				}
				else if( initializerPointerElementLLVMType->isStructTy() ) {
					this->variableStructType[statement.name] = llvm::cast<llvm::StructType>( initializerPointerElementLLVMType )->getName().str();
				}
				if( statement.ownershipAnnotation && statement.ownershipAnnotation->isHeapAllocated &&
					statement.ownershipAnnotation->isArenaManaged == false && statement.ownershipAnnotation->isMoved == false ) {
					std::string cleanupTypeName = statement.ownershipAnnotation->typeName;
					bool implementsDroper = false;
					semantic::TypeSharedPointer resolvedType = this->analyzer.types().lookupType( cleanupTypeName );
					if( resolvedType && resolvedType->kind == semantic::Type::Kind::Class ) {
						semantic::ClassType* classType = static_cast<semantic::ClassType*>( resolvedType.get() );
						implementsDroper = classType->implementsDroper;
					}
					this->registerCleanupEntry( statement.name, cleanupTypeName, structAllocaValue, implementsDroper );
				}
				return;
			}
		}
		if( variableInitializerValue && variableInitializerValue->getType()->isStructTy() && variableType->isPointerTy() ) {
			llvm::StructType* initStructType = llvm::cast<llvm::StructType>( variableInitializerValue->getType() );
			if( initStructType == this->getInterfaceFatPointerType() ) {
				llvm::Value* extractedObjectPointer = this->builder.CreateExtractValue( variableInitializerValue, 0, "iface.obj" );
				llvm::Value* variablePtrAlloca = this->createEntryBlockAllocation( this->currentFunction, statement.name, variableType );
				this->builder.CreateStore( extractedObjectPointer, variablePtrAlloca );
				this->namedValues[statement.name] = variablePtrAlloca;
				if( variableStructTypeName.empty() == false ) {
					this->variableStructType[statement.name] = variableStructTypeName;
				}
				if( statement.ownershipAnnotation && statement.ownershipAnnotation->isHeapAllocated &&
					statement.ownershipAnnotation->isArenaManaged == false && statement.ownershipAnnotation->isMoved == false ) {
					std::string cleanupTypeName = statement.ownershipAnnotation->typeName;
					bool implementsDroper = false;
					semantic::TypeSharedPointer resolvedType = this->analyzer.types().lookupType( cleanupTypeName );
					if( resolvedType && resolvedType->kind == semantic::Type::Kind::Class ) {
						semantic::ClassType* classType = static_cast<semantic::ClassType*>( resolvedType.get() );
						implementsDroper = classType->implementsDroper;
					}
					this->registerCleanupEntry( statement.name, cleanupTypeName, variablePtrAlloca, implementsDroper );
				}
				return;
			}
			llvm::Value* structStackAlloca = this->createEntryBlockAllocation( this->currentFunction, statement.name + ".val", variableInitializerValue->getType() );
			this->builder.CreateStore( variableInitializerValue, structStackAlloca );
			llvm::Value* variablePtrAlloca = this->createEntryBlockAllocation( this->currentFunction, statement.name, variableType );
			this->builder.CreateStore( structStackAlloca, variablePtrAlloca );
			this->namedValues[statement.name] = variablePtrAlloca;
			if( variableStructTypeName.empty() == false ) {
				this->variableStructType[statement.name] = variableStructTypeName;
			}
			else if( variableInitializerValue->getType()->isStructTy() ) {
				this->variableStructType[statement.name] = llvm::cast<llvm::StructType>( variableInitializerValue->getType() )->getName().str();
			}
			if( statement.ownershipAnnotation && statement.ownershipAnnotation->isHeapAllocated &&
				statement.ownershipAnnotation->isArenaManaged == false && statement.ownershipAnnotation->isMoved == false ) {
				std::string cleanupTypeName = statement.ownershipAnnotation->typeName;
				bool implementsDroper = false;
				semantic::TypeSharedPointer resolvedType = this->analyzer.types().lookupType( cleanupTypeName );
				if( resolvedType && resolvedType->kind == semantic::Type::Kind::Class ) {
					semantic::ClassType* classType = static_cast<semantic::ClassType*>( resolvedType.get() );
					implementsDroper = classType->implementsDroper;
				}
				this->registerCleanupEntry( statement.name, cleanupTypeName, variablePtrAlloca, implementsDroper );
			}
			return;
		}
		llvm::Value* variableFinalAllocaValue = this->createEntryBlockAllocation( this->currentFunction, statement.name, variableType );
		if( variableInitializerValue ) {
			bool variableIsUnsigned = ( variableStructTypeName == "U8" || variableStructTypeName == "U16" ||
				variableStructTypeName == "U32" || variableStructTypeName == "U64" ||
				variableStructTypeName == "UInt" || variableStructTypeName == "Byte" || variableStructTypeName == "Char" );
			variableInitializerValue = this->generateImplicitCast( variableInitializerValue, variableType, variableIsUnsigned );
			this->builder.CreateStore( variableInitializerValue, variableFinalAllocaValue );
		}
		this->namedValues[statement.name] = variableFinalAllocaValue;
		if( variableStructTypeName.empty() == false ) {
			this->variableStructType[statement.name] = variableStructTypeName;
		}
	}
	
	void LLVMCodegen::generateVirtualTable( const std::string& className, const semantic::ClassTypeSharedPointer& classType ) {
		
		// VTable is a global array of function pointers
		// In Uranite, this serves as the primary dispatch mechanism for dynamic polymorphism
		std::vector<llvm::Constant*> vtableEntries;
		llvm::FunctionType* vtableVoidFunctionType = llvm::FunctionType::get( llvm::Type::getVoidTy( this->context ), false );
		llvm::PointerType* vtableFunctionPointerType = llvm::PointerType::getUnqual( vtableVoidFunctionType );
		for( semantic::MethodInfo* vtableMethod : classType->virtualTable ) {
			std::string vtableFullMethodName = fmt::format( "{}::{}", className, vtableMethod->name );
			llvm::Function* vtableFunction = this->functions[vtableFullMethodName];
			if( vtableFunction ) {
				
				// Cast the specific function signature to a generic function pointer for the table
				vtableEntries.push_back( llvm::ConstantExpr::getBitCast( vtableFunction, vtableFunctionPointerType ) );
			}
			else {
				
				// Placeholder for pure virtual methods or missing implementations
				vtableEntries.push_back( llvm::ConstantPointerNull::get( vtableFunctionPointerType ) );
			}
		}
		llvm::ArrayType* vtableArrayType = llvm::ArrayType::get( vtableFunctionPointerType, vtableEntries.size() );
		std::string vtableGlobalName = fmt::format( "_AE_vtable_{}", className );
		llvm::GlobalVariable* vtableGlobalVariable = new llvm::GlobalVariable(
			*this->module,
			vtableArrayType,
			true, // isConstant: VTable should be immutable in read-only memory
			llvm::GlobalValue::InternalLinkage,
			llvm::ConstantArray::get( vtableArrayType, vtableEntries ),
			vtableGlobalName
		);
		this->virtualTables[className] = vtableGlobalVariable;
	}
	
	void LLVMCodegen::generateWhileStatement( ast::nodes::WhileStatement& statement ) {
		llvm::Function* whileFunction = this->currentFunction;
		llvm::BasicBlock* whileBodyBlock = llvm::BasicBlock::Create( this->context, "while.body", whileFunction );
		llvm::BasicBlock* whileConditionBlock = llvm::BasicBlock::Create( this->context, "while.cond", whileFunction );
		llvm::BasicBlock* whileEndBlock = llvm::BasicBlock::Create( this->context, "while.end", whileFunction );
		
		this->builder.CreateBr( whileConditionBlock );
		this->builder.SetInsertPoint( whileConditionBlock );
		
		llvm::Value* whileConditionValue = this->generateExpression( statement.condition );
		
		if( whileConditionValue && whileConditionValue->getType()->isIntegerTy( 1 ) == false ) {
			if( whileConditionValue->getType()->isStructTy() ) {
				whileConditionValue = this->builder.CreateExtractValue( whileConditionValue, 0, "struct.scalar" );
			}
			whileConditionValue = this->builder.CreateICmpNE( whileConditionValue, llvm::Constant::getNullValue( whileConditionValue->getType() ), "whilecond" );
		}
		if( whileConditionValue ) {
			this->builder.CreateCondBr( whileConditionValue, whileBodyBlock, whileEndBlock );
		}
		else {
			this->builder.CreateBr( whileEndBlock );
		}
		this->builder.SetInsertPoint( whileBodyBlock );
		this->breakTargets.push( whileEndBlock );
		this->continueTargets.push( whileConditionBlock );
		this->pushCleanupScope();
		this->pushDeferScope();
		for( ast::nodes::StatementSharedPointer& bodyStatement : statement.body ) {
			this->generateStatement( bodyStatement );
		}
		this->emitCurrentScopeDefers();
		this->popDeferScope();
		this->popCleanupScope();
		this->continueTargets.pop();
		this->breakTargets.pop();
		if( this->builder.GetInsertBlock()->getTerminator() == nullptr ) {
			this->builder.CreateBr( whileConditionBlock );
		}
		this->builder.SetInsertPoint( whileEndBlock );
	}
	
	llvm::Function* LLVMCodegen::getOrCreatePersonalityFunction() {
		RuntimeFunctionSpec spec = this->runtimeInterface_->getPersonalityFunction( this->context );
		llvm::Function* function = this->module->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType,
				spec.functionName, this->getModule()
			);
		}
		return function;
	}
	
	llvm::Function* LLVMCodegen::getOrCreateUraniteThrow() {
		RuntimeFunctionSpec spec = this->runtimeInterface_->getThrowFunction( this->context );
		llvm::Function* function = this->module->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType,
				spec.functionName, this->getModule()
			);
			if( spec.isNoReturn ) {
				function->addFnAttr( llvm::Attribute::NoReturn );
			}
		}
		return function;
	}
	
	llvm::Function* LLVMCodegen::getOrCreateUraniteBeginCatch() {
		RuntimeFunctionSpec spec = this->runtimeInterface_->getBeginCatchFunction( this->context );
		llvm::Function* function = this->module->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType,
				spec.functionName, this->getModule()
			);
		}
		return function;
	}
	
	llvm::Function* LLVMCodegen::getOrCreateUraniteEndCatch() {
		RuntimeFunctionSpec spec = this->runtimeInterface_->getEndCatchFunction( this->context );
		llvm::Function* function = this->module->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType,
				spec.functionName, this->getModule()
			);
		}
		return function;
	}
	
	llvm::Function* LLVMCodegen::getOrCreatePushFrame() {
		RuntimeFunctionSpec spec = this->runtimeInterface_->getPushFrameFunction( this->context );
		llvm::Function* function = this->module->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType,
				spec.functionName, this->getModule()
			);
		}
		return function;
	}
	
	llvm::Function* LLVMCodegen::getOrCreatePopFrame() {
		RuntimeFunctionSpec spec = this->runtimeInterface_->getPopFrameFunction( this->context );
		llvm::Function* function = this->module->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType,
				spec.functionName, this->getModule()
			);
		}
		return function;
	}
	
	void LLVMCodegen::emitPushFrame( const std::string& file, int64_t line, int64_t column, const std::string& functionName ) {
		llvm::Function* pushFrame = this->getOrCreatePushFrame();
		llvm::Value* fileStr = this->builder.CreateGlobalStringPtr( file, "frame.file" );
		llvm::Value* lineVal = llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->context ), line );
		llvm::Value* colVal = llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->context ), column );
		llvm::Value* funcStr = this->builder.CreateGlobalStringPtr( functionName, "frame.func" );
		this->builder.CreateCall( pushFrame, { fileStr, lineVal, colVal, funcStr } );
	}
	
	void LLVMCodegen::emitPopFrame() {
		llvm::Function* popFrame = this->getOrCreatePopFrame();
		this->builder.CreateCall( popFrame, {} );
	}
	
	llvm::Value* LLVMCodegen::createCallOrInvoke( llvm::Function* callee, llvm::ArrayRef<llvm::Value*> args, const llvm::Twine& name ) {
		if( this->landingPads.empty() ) {
			return this->builder.CreateCall( callee, args, name );
		}
		llvm::BasicBlock* normalDest = llvm::BasicBlock::Create( this->context, "invoke.cont", this->currentFunction );
		llvm::InvokeInst* invokeResult = this->builder.CreateInvoke( callee, normalDest, this->landingPads.top(), args, name );
		this->builder.SetInsertPoint( normalDest );
		return invokeResult;
	}
	
	llvm::Value* LLVMCodegen::createCallOrInvoke( llvm::FunctionType* type, llvm::Value* callee, llvm::ArrayRef<llvm::Value*> args, const llvm::Twine& name ) {
		if( this->landingPads.empty() ) {
			return this->builder.CreateCall( type, callee, args, name );
		}
		llvm::BasicBlock* normalDest = llvm::BasicBlock::Create( this->context, "invoke.cont", this->currentFunction );
		llvm::InvokeInst* invokeResult = this->builder.CreateInvoke( type, callee, normalDest, this->landingPads.top(), args, name );
		this->builder.SetInsertPoint( normalDest );
		return invokeResult;
	}
	
	void LLVMCodegen::generateArithmeticErrorCheck( llvm::Value* condition, const std::string& errorClassName, const std::string& message, const lookup::SourceSharedPointer& source ) {
		llvm::BasicBlock* errorBlock = llvm::BasicBlock::Create( this->context, "arith.error", this->currentFunction );
		llvm::BasicBlock* continueBlock = llvm::BasicBlock::Create( this->context, "arith.continue", this->currentFunction );
		this->builder.CreateCondBr( condition, errorBlock, continueBlock );
		this->builder.SetInsertPoint( errorBlock );
		this->generateThrowError( errorClassName, message, source );
		this->builder.SetInsertPoint( continueBlock );
	}
	
	void LLVMCodegen::generateThrowError( const std::string& errorClassName, const std::string& message, const lookup::SourceSharedPointer& source ) {
		this->ensureClassMethodsRegistered( errorClassName );
		llvm::StructType* errorStructType = nullptr;
		std::unordered_map<std::string, llvm::StructType*>::iterator structIterator = this->structTypes.find( errorClassName );
		if( structIterator != this->structTypes.end() ) {
			errorStructType = structIterator->second;
		}
		if( errorStructType != nullptr ) {
			llvm::DataLayout layout = this->module->getDataLayout();
			uint64_t structSize = layout.getTypeAllocSize( errorStructType );
			llvm::Function* mallocFunction = this->getOrCreateMalloc();
			llvm::ConstantInt* sizeValue = llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->context ), structSize );
			llvm::CallInst* rawPointer = this->builder.CreateCall( mallocFunction, { sizeValue }, "err.raw" );
			llvm::Value* errorPointer = this->builder.CreateBitCast( rawPointer, llvm::PointerType::getUnqual( errorStructType ), "err.obj" );
			std::string constructorKey( fmt::format( "{}::{}", errorClassName, errorClassName ) );
			std::unordered_map<std::string, llvm::Function*>::iterator constructorIterator = this->functions.find( constructorKey );
			if( constructorIterator != this->functions.end() && constructorIterator->second != nullptr ) {
				llvm::Function* constructorFunction = constructorIterator->second;
				llvm::Value* messageValue = this->builder.CreateGlobalStringPtr( message, "err.msg" );
				llvm::ConstantInt* codeValue = llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->context ), 0 );
				std::vector<llvm::Value*> constructorArguments;
				constructorArguments.push_back( errorPointer );
				constructorArguments.push_back( messageValue );
				constructorArguments.push_back( codeValue );
				for( size_t i = constructorArguments.size(); i < constructorFunction->arg_size(); i++ ) {
					constructorArguments.push_back( llvm::Constant::getNullValue( constructorFunction->getArg( i )->getType() ) );
				}
				this->builder.CreateCall( constructorFunction, constructorArguments );
			}
			if( source ) {
				this->injectTracebackInfo( errorPointer, errorClassName, source );
			}
			llvm::Value* castedException = this->builder.CreateBitCast( errorPointer, llvm::PointerType::getUnqual( this->context ), "err.cast" );
			llvm::Value* typeNameStr = this->builder.CreateGlobalStringPtr( errorClassName, "throw.typename" );
			llvm::Function* throwFunction = this->getOrCreateUraniteThrow();
			if( this->landingPads.empty() ) {
				this->builder.CreateCall( throwFunction, { castedException, typeNameStr } );
			}
			else {
				llvm::BasicBlock* throwUnreachable = llvm::BasicBlock::Create( this->context, "throw.unreachable", this->currentFunction );
				this->builder.CreateInvoke( throwFunction, throwUnreachable, this->landingPads.top(), { castedException, typeNameStr } );
				this->builder.SetInsertPoint( throwUnreachable );
			}
		}
		this->builder.CreateUnreachable();
	}
	
	void LLVMCodegen::generateInlineAssemblyStatement( ast::nodes::InlineAssemblyStatement& statement ) {
		// Arch block form: select variant matching compile target
		if( statement.archVariants.empty() == false ) {
			llvm::Triple triple( this->module->getTargetTriple() );
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
			for( ast::nodes::InlineAssemblyArchVariant& variant : statement.archVariants ) {
				if( variant.targetArch == targetArch ) {
					ast::nodes::InlineAssemblyStatement resolvedStatement(
						statement.isVolatile,
						variant.asmTemplate,
						variant.outputs,
						variant.inputs,
						variant.clobbers,
						statement.source
					);
					this->generateInlineAssemblyStatement( resolvedStatement );
					return;
				}
			}
			std::string availableArchs;
			for( ast::nodes::InlineAssemblyArchVariant& variant : statement.archVariants ) {
				if( availableArchs.empty() == false ) {
					availableArchs += ", ";
				}
				availableArchs += variant.targetArch;
			}
			this->diagnostic.error(
				statement.source,
				fmt::format( "no inline assembly variant for target architecture '{}' (available: {})", targetArch, availableArchs )
			);
			return;
		}

		std::string constraintString;
		std::vector<llvm::Type*> outputTypes;
		std::vector<llvm::Value*> outputPointers;
		std::vector<llvm::Value*> inputValues;
		std::vector<llvm::Type*> inputTypes;
		for( ast::nodes::InlineAssemblyOperand& output : statement.outputs ) {
			if( constraintString.empty() == false ) {
				constraintString+= ",";
			}
			constraintString+= output.constraint;
			llvm::Value* outputPointer = nullptr;
			if( output.expression->kind == ast::Node::Kind::IdentifierExpression ) {
				ast::nodes::IdentifierExpression& outputIdentifier = static_cast<ast::nodes::IdentifierExpression&>( *output.expression );
				std::unordered_map<std::string,llvm::Value*>::iterator outputIterator = this->namedValues.find( outputIdentifier.name );
				if( outputIterator != this->namedValues.end() ) {
					outputPointer = outputIterator->second;
				}
			}
			if( outputPointer == nullptr ) {
				outputPointer = this->resolveObjectPointer( output.expression );
			}
			outputPointers.push_back( outputPointer );
			if( outputPointer != nullptr ) {
				llvm::Type* outputElementType = getPointeeType( outputPointer );
				if( outputElementType->isStructTy() ) {
					llvm::StructType* outputStructType = llvm::cast<llvm::StructType>( outputElementType );
					if( outputStructType->getNumElements() == 2 &&
						outputStructType->getElementType( 0 )->isIntegerTy( 1 ) &&
						outputStructType->getElementType( 1 ) == this->getInterfaceFatPointerType() ) {
						outputTypes.push_back( llvm::Type::getInt64Ty( this->context ) );
					}
					else {
						outputTypes.push_back( outputElementType );
					}
				}
				else {
					outputTypes.push_back( outputElementType );
				}
			}
			else {
				outputTypes.push_back( llvm::Type::getInt64Ty( this->context ) );
			}
		}
		for( ast::nodes::InlineAssemblyOperand& input : statement.inputs ) {
			if( constraintString.empty() == false ) {
				constraintString+= ",";
			}
			constraintString+= input.constraint;
			llvm::Value* inputValue = this->generateExpression( input.expression );
			if( inputValue == nullptr ) {
				inputValue = llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->context ), 0 );
			}
			else if( inputValue->getType()->isStructTy() ) {
				llvm::Value* inputObjectPointer = this->resolveObjectPointer( input.expression );
				if( inputObjectPointer != nullptr && inputObjectPointer->getType()->isPointerTy() ) {
					inputValue = this->builder.CreatePtrToInt( inputObjectPointer, llvm::Type::getInt64Ty( this->context ), "asm.input.ptrtoint" );
				}
			}
			else if( inputValue->getType()->isPointerTy() ) {
				inputValue = this->builder.CreatePtrToInt( inputValue, llvm::Type::getInt64Ty( this->context ), "asm.input.ptrtoint" );
			}
			inputValues.push_back( inputValue );
			inputTypes.push_back( inputValue->getType() );
		}
		for( const std::string& clobber : statement.clobbers ) {
			if( constraintString.empty() == false ) {
				constraintString+= ",";
			}
			constraintString+= fmt::format( "~{{{}}}", clobber );
		}
		llvm::Type* resultType = nullptr;
		if( outputTypes.empty() ) {
			resultType = llvm::Type::getVoidTy( this->context );
		}
		else if( outputTypes.size() == 1 ) {
			resultType = outputTypes[0];
		}
		else {
			resultType = llvm::StructType::get( this->context, outputTypes );
		}
		llvm::FunctionType* asmFunctionType = llvm::FunctionType::get( resultType, inputTypes, false );
		llvm::InlineAsm* inlineAsm = llvm::InlineAsm::get(
			asmFunctionType,
			statement.asmTemplate,
			constraintString,
			statement.isVolatile
		);
		llvm::CallInst* asmResult = this->builder.CreateCall( asmFunctionType, inlineAsm, inputValues );
		if( statement.outputs.empty() == false ) {
			for( size_t outputIndex = 0; outputIndex < statement.outputs.size(); outputIndex++ ) {
				llvm::Value* outputPointer = outputPointers[outputIndex];
				if( outputPointer != nullptr ) {
					llvm::Value* outputValue = asmResult;
					if( statement.outputs.size() > 1 ) {
						outputValue = this->builder.CreateExtractValue( asmResult, outputIndex, "asm.out" );
					}
					llvm::Type* storeType = getPointeeType( outputPointer );
					if( outputValue->getType() != storeType ) {
						llvm::StructType* interfaceFatPointerType = this->getInterfaceFatPointerType();
						if( storeType->isStructTy() ) {
							llvm::StructType* storeStructType = llvm::cast<llvm::StructType>( storeType );
							if( storeStructType->getNumElements() == 2 &&
								storeStructType->getElementType( 0 )->isIntegerTy( 1 ) &&
								storeStructType->getElementType( 1 ) == interfaceFatPointerType &&
								outputValue->getType()->isIntegerTy( 64 ) ) {
								llvm::Value* fatPtrAddress = this->builder.CreateIntToPtr(
									outputValue,
									llvm::PointerType::getUnqual( interfaceFatPointerType ),
									"asm.nullable.iface.addr"
								);
								llvm::Value* loadedFatPointer = this->builder.CreateLoad( interfaceFatPointerType, fatPtrAddress, "asm.nullable.iface.fat" );
								llvm::Value* nullableValue = llvm::UndefValue::get( storeStructType );
								nullableValue = this->builder.CreateInsertValue( nullableValue, llvm::ConstantInt::getTrue( this->context ), 0, "asm.nullable.iface.flag" );
								nullableValue = this->builder.CreateInsertValue( nullableValue, loadedFatPointer, 1, "asm.nullable.iface.wrap" );
								this->builder.CreateStore( nullableValue, outputPointer );
								continue;
							}
						}
						if( storeType->isIntegerTy() && outputValue->getType()->isIntegerTy() ) {
							outputValue = this->builder.CreateIntCast( outputValue, storeType, true, "asm.out.cast" );
						}
					}
					this->builder.CreateStore( outputValue, outputPointer );
				}
			}
		}
	}
	
	llvm::Function* LLVMCodegen::getOrCreateFree() {
		RuntimeFunctionSpec spec = this->runtimeInterface_->getFreeFunction( this->context );
		llvm::Function* function = this->module->getFunction( spec.functionName );
		if( function != nullptr && function->getFunctionType() != spec.functionSignature ) {
			function->eraseFromParent();
			function = nullptr;
		}
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType,
				spec.functionName, this->getModule()
			);
		}
		return function;
	}
	
	llvm::Function* LLVMCodegen::getOrCreateMalloc() {
		RuntimeFunctionSpec spec = this->runtimeInterface_->getMallocFunction( this->context );
		llvm::Function* function = this->module->getFunction( spec.functionName );
		if( function != nullptr && function->getFunctionType() != spec.functionSignature ) {
			function->eraseFromParent();
			function = nullptr;
		}
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType,
				spec.functionName, this->getModule()
			);
		}
		return function;
	}
	
	llvm::Function* LLVMCodegen::getOrCreateCalloc() {
		RuntimeFunctionSpec spec = this->runtimeInterface_->getCallocFunction( this->context );
		llvm::Function* function = this->module->getFunction( spec.functionName );
		if( function != nullptr && function->getFunctionType() != spec.functionSignature ) {
			function->eraseFromParent();
			function = nullptr;
		}
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType,
				spec.functionName, this->getModule()
			);
		}
		return function;
	}
	
	llvm::Function* LLVMCodegen::getOrCreateMemoryCopy() {
		RuntimeFunctionSpec spec = this->runtimeInterface_->getMemcpyFunction( this->context );
		llvm::Function* function = this->module->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType,
				spec.functionName, this->getModule()
			);
		}
		return function;
	}
	
	llvm::Function* LLVMCodegen::getOrCreateRunScheduler() {
		// implemented on `uranite::codegen::runtime::async::getOrCreateRunScheduler` because we'll remove every C-Runtime
		return runtime::async::getOrCreateRunScheduler( this->context, this->getModule() );
	}
	
	llvm::Function* LLVMCodegen::getOrCreateSchedulerInit() {
		// implemented on `uranite::codegen::runtime::async::getOrCreateSchedulerInit` because we'll remove every C-Runtime
		return runtime::async::getOrCreateSchedulerInit( this->context, this->getModule() );
	}
	
	
	llvm::Function* LLVMCodegen::getOrCreateStringCompare() {
		RuntimeFunctionSpec spec = this->runtimeInterface_->getStrcmpFunction( this->context );
		llvm::Function* function = this->module->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType,
				spec.functionName, this->getModule()
			);
		}
		return function;
	}
	
	llvm::Function* LLVMCodegen::getOrCreateStringConcatenate() {
		RuntimeFunctionSpec spec = this->runtimeInterface_->getStrcatFunction( this->context );
		llvm::Function* function = this->module->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType,
				spec.functionName, this->getModule()
			);
		}
		return function;
	}
	
	llvm::Function* LLVMCodegen::getOrCreateStringCopy() {
		RuntimeFunctionSpec spec = this->runtimeInterface_->getStrcpyFunction( this->context );
		llvm::Function* function = this->module->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType,
				spec.functionName, this->getModule()
			);
		}
		return function;
	}
	
	llvm::Function* LLVMCodegen::getOrCreateSnprintf() {
		RuntimeFunctionSpec spec = this->runtimeInterface_->getSnprintfFunction( this->context );
		llvm::Function* function = this->module->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType,
				spec.functionName, this->getModule()
			);
		}
		return function;
	}
	
	llvm::Function* LLVMCodegen::getOrCreateStringLength() {
		RuntimeFunctionSpec spec = this->runtimeInterface_->getStrlenFunction( this->context );
		llvm::Function* function = this->module->getFunction( spec.functionName );
		if( function == nullptr ) {
			function = llvm::Function::Create(
				spec.functionSignature, spec.linkageType,
				spec.functionName, this->getModule()
			);
		}
		return function;
	}
	
	llvm::StructType* LLVMCodegen::getOrCreateStructType( const std::string& name, const semantic::TypeSharedPointer& type ) {
		std::unordered_map<std::string, llvm::StructType*>::iterator structTypeIterator = this->structTypes.find( name );
		if( structTypeIterator != this->structTypes.end() ) {
			return structTypeIterator->second;
		}
		std::string baseName;
		if( type->kind == semantic::Type::Kind::Class ) {
			semantic::ClassTypeSharedPointer classType = std::static_pointer_cast<semantic::ClassType>( type );
			if( classType->astDeclaration ) {
				baseName = classType->astDeclaration->name;
			}
		}
		else if( type->kind == semantic::Type::Kind::Struct ) {
			semantic::StructTypeSharedPointer structType = std::static_pointer_cast<semantic::StructType>( type );
			if( structType->astDeclaration ) {
				baseName = structType->astDeclaration->name;
			}
		}
		if( baseName.empty() == false && baseName != name ) {
			std::unordered_map<std::string, llvm::StructType*>::iterator baseIterator = this->structTypes.find( baseName );
			if( baseIterator != this->structTypes.end() ) {
				this->structTypes[name] = baseIterator->second;
				this->structFieldIndices[name] = this->structFieldIndices[baseName];
				return baseIterator->second;
			}
		}
		llvm::StructType* createdStructType = llvm::StructType::getTypeByName( this->context, name );
		if( createdStructType == nullptr ) {
			createdStructType = llvm::StructType::create( this->context, name );
		}
		this->structTypes[name] = createdStructType;
		std::vector<llvm::Type*> structMemberTypes;
		if( type->kind == semantic::Type::Kind::Class ) {
			unsigned int classFieldIndex = 0;
			semantic::ClassTypeSharedPointer classType = std::static_pointer_cast<semantic::ClassType>( type );
			
			// VTable pointer if class has virtual methods
			if( classType->virtualTable.empty() == false ) {
				structMemberTypes.push_back( llvm::PointerType::getUnqual( this->context ) ); // vtable ptr
				classFieldIndex++;
			}
			for( const semantic::FieldInfo& classField : classType->fields ) {
				if( classField.isStatic ) {
					llvm::Type* staticFieldType = this->toLLVMType( classField.type );
					std::string staticGlobalName = fmt::format( "{}.{}", name, classField.name );
					llvm::GlobalVariable* staticGlobal = new llvm::GlobalVariable(
						*this->module,
						staticFieldType,
						false,
						llvm::GlobalValue::InternalLinkage,
						llvm::Constant::getNullValue( staticFieldType ),
						staticGlobalName
					);
					this->staticClassFields[name][classField.name] = staticGlobal;
					continue;
				}
				bool classFieldIsSelfReference = false;
				if( classField.type && classField.type->kind == semantic::Type::Kind::Class && classField.type->name == name ) {
					classFieldIsSelfReference = true;
				}
				else if( classField.type && classField.type->kind == semantic::Type::Kind::Optional ) {
					semantic::OptionalTypeSharedPointer classOptionalType = std::static_pointer_cast<semantic::OptionalType>( classField.type );
					if( classOptionalType->inner && classOptionalType->inner->kind == semantic::Type::Kind::Class && classOptionalType->inner->name == name ) {
						classFieldIsSelfReference = true;
					}
				}
				if( classFieldIsSelfReference ) {
					structMemberTypes.push_back( llvm::PointerType::getUnqual( createdStructType ) );
				}
				else {
					llvm::Type* fieldLLVMType = toLLVMType( classField.type );
					if( classField.type && ( classField.type->kind == semantic::Type::Kind::Class || classField.type->kind == semantic::Type::Kind::Struct ) &&
						fieldLLVMType->isStructTy() && fieldLLVMType != this->getInterfaceFatPointerType() ) {
						fieldLLVMType = llvm::PointerType::getUnqual( fieldLLVMType );
					}
					structMemberTypes.push_back( fieldLLVMType );
				}
				this->structFieldIndices[name][classField.name] = classFieldIndex++;
			}
		}
		else if( type->kind == semantic::Type::Kind::Struct ) {
			unsigned int structFieldIndex = 0;
			semantic::StructTypeSharedPointer semanticStructType = std::static_pointer_cast<semantic::StructType>( type );
			for( const semantic::FieldInfo& structField : semanticStructType->fields ) {
				bool structFieldIsSelfReference = false;
				if( structField.type && structField.type->kind == semantic::Type::Kind::Struct && structField.type->name == name ) {
					structFieldIsSelfReference = true;
				}
				else if( structField.type && structField.type->kind == semantic::Type::Kind::Optional ) {
					semantic::OptionalTypeSharedPointer structOptionalType = std::static_pointer_cast<semantic::OptionalType>( structField.type );
					if( structOptionalType->inner && structOptionalType->inner->kind == semantic::Type::Kind::Struct && structOptionalType->inner->name == name ) {
						structFieldIsSelfReference = true;
					}
				}
				if( structFieldIsSelfReference ) {
					structMemberTypes.push_back( llvm::PointerType::getUnqual( createdStructType ) );
				}
				else {
					llvm::Type* fieldLLVMType = toLLVMType( structField.type );
					if( structField.type && ( structField.type->kind == semantic::Type::Kind::Class || structField.type->kind == semantic::Type::Kind::Struct ) &&
						fieldLLVMType->isStructTy() && fieldLLVMType != this->getInterfaceFatPointerType() ) {
						fieldLLVMType = llvm::PointerType::getUnqual( fieldLLVMType );
					}
					structMemberTypes.push_back( fieldLLVMType );
				}
				this->structFieldIndices[name][structField.name] = structFieldIndex++;
			}
		}
		if( structMemberTypes.empty() ) {
			structMemberTypes.push_back( llvm::Type::getInt8Ty( this->context ) ); // non-empty struct
		}
		createdStructType->setBody( structMemberTypes );
		return createdStructType;
	}
	
	std::string LLVMCodegen::buildMonomorphizedName( const ast::nodes::TypeNodeSharedPointer& typeNode ) {
		if( typeNode->kind == ast::Node::Kind::SimpleType ) {
			return static_cast<ast::nodes::SimpleTypeNode&>( *typeNode ).name;
		}
		if( typeNode->kind == ast::Node::Kind::GenericType ) {
			ast::nodes::GenericTypeNode& genericNode = static_cast<ast::nodes::GenericTypeNode&>( *typeNode );
			std::string result = fmt::format( "{}<", genericNode.name );
			for( size_t i = 0; i < genericNode.typeArguments.size(); i++ ) {
				if( i > 0 ) {
					result+= ",";
				}
				result+= this->buildMonomorphizedName( genericNode.typeArguments[i] );
			}
			result+= ">";
			return result;
		}
		return "?";
	}
	
	llvm::StructType* LLVMCodegen::getInterfaceFatPointerType() {
		if( this->interfaceFatPointerType ==nullptr ) {
			this->interfaceFatPointerType = llvm::StructType::create( this->context, { llvm::PointerType::getUnqual( this->context ), llvm::PointerType::getUnqual( this->context ) }, "interface.fat.ptr" );
		}
		return this->interfaceFatPointerType;
	}
	
	std::string LLVMCodegen::mangleName( const std::string& name, const std::string& className ) {
		if( className.empty() ) {
			return "_UR_" + name;
		}
		return "_UR_" + className + "_" + name;
	}
	
	void LLVMCodegen::registerBuiltInStructTypes() {
		for( std::pair<std::string,descriptor::BuiltinTypeDescriptor> pair : this->builtinRegistry.allTypes() ) {
			if( this->structTypes.count( pair.first ) ) {
				continue;
			}
			llvm::Type* llvmType = pair.second.llvmTypeFactory( this->context );
			if( llvmType->isVoidTy() == false ) {
				llvm::StructType* llvmStructType = llvm::StructType::create( this->context, { llvmType }, pair.first );
				this->structTypes[pair.first] = llvmStructType;
				this->structFieldIndices[pair.first]["value"] = 0;
			}
		}
	}
	
	llvm::Type* LLVMCodegen::resolveAstType( const ast::nodes::TypeNodeSharedPointer& typeNode ) {
		if( typeNode == nullptr ) {
			return llvm::Type::getVoidTy( this->context );
		}
		if( typeNode->kind == ast::Node::Kind::CallableType ) {
			ast::nodes::CallableTypeNode& callableTypeNode = static_cast<ast::nodes::CallableTypeNode&>( *typeNode );
			llvm::Type* callableReturnTypeLLVM = this->resolveAstType( callableTypeNode.returnType );
			std::vector<llvm::Type*> callableParameterTypesLLVM;
			for( const ast::nodes::TypeNodeSharedPointer& parameterReference : callableTypeNode.parameterTypes ) {
				callableParameterTypesLLVM.push_back( this->resolveAstType( parameterReference ) );
			}
			llvm::FunctionType* callableFunctionType = llvm::FunctionType::get( callableReturnTypeLLVM, callableParameterTypesLLVM, false );
			return llvm::PointerType::getUnqual( callableFunctionType );
		}
		else if( typeNode->kind == ast::Node::Kind::GenericType ) {
			ast::nodes::GenericTypeNode& genericTypeNode = static_cast<ast::nodes::GenericTypeNode&>( *typeNode );
			if( genericTypeNode.name == "Arena" && genericTypeNode.typeArguments.empty() == false ) {
				llvm::Type* arenaInnerTypeLLVM = this->resolveAstType( genericTypeNode.typeArguments[0] );
				if( arenaInnerTypeLLVM == nullptr || arenaInnerTypeLLVM->isVoidTy() ) {
					arenaInnerTypeLLVM = llvm::Type::getInt64Ty( this->context );
				}
				if( arenaInnerTypeLLVM->isPointerTy() ) {
					arenaInnerTypeLLVM = unwrapStructPointer( arenaInnerTypeLLVM, genericTypeNode.typeArguments[0], this->structTypes );
				}
				llvm::StructType* arenaStructType = llvm::StructType::get( this->context, {
					llvm::PointerType::getUnqual( arenaInnerTypeLLVM ),
					llvm::Type::getInt64Ty( this->context ),
					llvm::Type::getInt64Ty( this->context )
				});
				return llvm::PointerType::getUnqual( arenaStructType );
			}
			if( genericTypeNode.name == "Future" && genericTypeNode.typeArguments.empty() == false ) {
				return llvm::Type::getInt64Ty( this->context );
			}
			if( genericTypeNode.name == "Generator" && genericTypeNode.typeArguments.empty() == false ) {
				llvm::Type* generatorInnerTypeLLVM = this->resolveAstType( genericTypeNode.typeArguments[0] );
				if( generatorInnerTypeLLVM == nullptr || generatorInnerTypeLLVM->isVoidTy() ) {
					generatorInnerTypeLLVM = llvm::Type::getInt64Ty( this->context );
				}
				return generatorInnerTypeLLVM;
			}
			if( genericTypeNode.name == "Memory" && genericTypeNode.typeArguments.empty() == false ) {
				llvm::Type* memoryInnerTypeLLVM = this->resolveAstType( genericTypeNode.typeArguments[0] );
				if( memoryInnerTypeLLVM == nullptr || memoryInnerTypeLLVM->isVoidTy() ) {
					memoryInnerTypeLLVM = llvm::Type::getInt64Ty( this->context );
				}
				return llvm::PointerType::getUnqual( memoryInnerTypeLLVM );
			}
			std::string monoName = this->buildMonomorphizedName( typeNode );
			semantic::TypeSharedPointer genericSemanticType = this->analyzer.types().lookupType( monoName );
			if( genericSemanticType == nullptr ) {
				genericSemanticType = this->analyzer.types().lookupType( genericTypeNode.name );
			}
			if( genericSemanticType == nullptr ) {
				std::unordered_map<std::string, llvm::StructType*>::iterator genericStructIterator = this->structTypes.find( genericTypeNode.name );
				if( genericStructIterator != this->structTypes.end() ) {
					return llvm::PointerType::getUnqual( genericStructIterator->second );
				}
				return llvm::Type::getInt64Ty( this->context );
			}
			llvm::Type* genericBaseLLVMType = toLLVMType( genericSemanticType );
			if( genericBaseLLVMType->isStructTy() && ( this->structTypes.count( monoName ) || this->structTypes.count( genericTypeNode.name ) ) ) {
				return llvm::PointerType::getUnqual( genericBaseLLVMType );
			}
			return genericBaseLLVMType;
		}
		else if( typeNode->kind == ast::Node::Kind::MetaType ) {
			return llvm::Type::getInt64Ty( this->context );
		}
		else if( typeNode->kind == ast::Node::Kind::OptionalType ) {
			ast::nodes::OptionalTypeNode& optionalTypeNode = static_cast<ast::nodes::OptionalTypeNode&>( *typeNode );
			llvm::Type* optionalInnerLLVMType = this->resolveAstType( optionalTypeNode.innerType );
			if( optionalInnerLLVMType->isPointerTy() ) {
				return optionalInnerLLVMType;
			}
			return llvm::StructType::get( this->context, {
				llvm::Type::getInt1Ty( this->context ),
				optionalInnerLLVMType
			});
		}
		else if( typeNode->kind == ast::Node::Kind::PointerType ) {
			ast::nodes::PointerTypeNode& pointerTypeNode = static_cast<ast::nodes::PointerTypeNode&>( *typeNode );
			llvm::Type* pointerInnerLLVMType = this->resolveAstType( pointerTypeNode.innerType );
			if( pointerInnerLLVMType->isVoidTy() ) {
				return llvm::PointerType::getUnqual( this->context );
			}
			return llvm::PointerType::getUnqual( pointerInnerLLVMType );
		}
		else if( typeNode->kind == ast::Node::Kind::ReferenceType ) {
			ast::nodes::ReferenceTypeNode& referenceTypeNode = static_cast<ast::nodes::ReferenceTypeNode&>( *typeNode );
			llvm::Type* referenceInnerLLVMType = this->resolveAstType( referenceTypeNode.innerType );
			return llvm::PointerType::getUnqual( referenceInnerLLVMType );
		}
		else if( typeNode->kind == ast::Node::Kind::SimpleType ) {
			ast::nodes::SimpleTypeNode& simpleTypeNode = static_cast<ast::nodes::SimpleTypeNode&>( *typeNode );
			semantic::TypeSharedPointer simpleSemanticType = this->analyzer.types().lookupType( simpleTypeNode.name );
			if( simpleSemanticType == nullptr ) {
				return llvm::Type::getInt64Ty( this->context );
			}
			llvm::Type* simpleLLVMType = toLLVMType( simpleSemanticType );
			if( simpleLLVMType->isStructTy() && this->structTypes.count( simpleTypeNode.name ) ) {
				return llvm::PointerType::getUnqual( simpleLLVMType );
			}
			return simpleLLVMType;
		}
		else if( typeNode->kind == ast::Node::Kind::UnionType ) {
			uint64_t unionMaximumTypeSize = 0;
			ast::nodes::UnionTypeNode& unionTypeNode = static_cast<ast::nodes::UnionTypeNode&>( *typeNode );
			llvm::Type* unionWidestLLVMType = llvm::Type::getInt64Ty( this->context );
			for( const ast::nodes::TypeNodeSharedPointer& unionMemberTypeReference : unionTypeNode.types ) {
				llvm::Type* unionMemberLLVMType = this->resolveAstType( unionMemberTypeReference );
				uint64_t unionMemberAllocatedSize = this->module->getDataLayout().getTypeAllocSize( unionMemberLLVMType );
				if( unionMemberAllocatedSize > unionMaximumTypeSize ) {
					unionMaximumTypeSize = unionMemberAllocatedSize;
					unionWidestLLVMType = unionMemberLLVMType;
				}
			}
			return llvm::StructType::get( this->context, {
				llvm::Type::getInt32Ty( this->context ),
				unionWidestLLVMType
			});
		}
		return llvm::Type::getInt64Ty( this->context );
	}
	
	llvm::Value* LLVMCodegen::resolveObjectPointer( const ast::nodes::ExpressionSharedPointer& expression ) {
		if( expression->kind == ast::Node::Kind::IdentifierExpression ) {
			ast::nodes::IdentifierExpression& identifierExpression = static_cast<ast::nodes::IdentifierExpression&>( *expression );
			std::unordered_map<std::string,llvm::Value*>::iterator identifierValueIterator = this->namedValues.find( identifierExpression.name );
			if( identifierValueIterator != this->namedValues.end() ) {
				llvm::Value* identifierObjectPointer = identifierValueIterator->second;
				llvm::Type* identifierElementPointerType = getPointeeType( identifierObjectPointer );
				if( identifierElementPointerType->isPointerTy() ) {
					std::string identifierLoadName = fmt::format( "{}.ptr", identifierExpression.name );
					llvm::Value* loadedPointer = this->builder.CreateLoad( identifierElementPointerType, identifierObjectPointer, identifierLoadName );
					return loadedPointer;
				}
				std::unordered_map<std::string, std::string>::iterator structTypeIterator = this->variableStructType.find( identifierExpression.name );
				if( structTypeIterator != this->variableStructType.end() ) {
					std::unordered_map<std::string, llvm::StructType*>::iterator targetStructIterator = this->structTypes.find( structTypeIterator->second );
					if( targetStructIterator != this->structTypes.end() ) {
						llvm::StructType* targetStruct = targetStructIterator->second;
						if( identifierElementPointerType->isStructTy() && identifierElementPointerType != targetStruct ) {
							llvm::PointerType* targetPointerType = llvm::PointerType::getUnqual( targetStruct );
							std::string castName = fmt::format( "{}.cast", identifierExpression.name );
							return this->builder.CreateBitCast( identifierObjectPointer, targetPointerType, castName );
						}
					}
				}
				return identifierObjectPointer;
			}
			llvm::GlobalVariable* globalVar = this->module->getGlobalVariable( identifierExpression.name, true );
			if( globalVar ) {
				llvm::Type* globalValueType = globalVar->getValueType();
				if( globalValueType->isPointerTy() ) {
					std::string globalLoadName = fmt::format( "{}.gptr", identifierExpression.name );
					return this->builder.CreateLoad( globalValueType, globalVar, globalLoadName );
				}
				return globalVar;
			}
		}
		else if( expression->kind == ast::Node::Kind::IndexExpression ) {
			ast::nodes::IndexExpression& indexExpression = static_cast<ast::nodes::IndexExpression&>( *expression );
			llvm::Value* indexExpressionObjectValue = this->generateExpression( indexExpression.object );
			llvm::Value* indexExpressionIndexValue = this->generateExpression( indexExpression.index );
			if( indexExpressionObjectValue && indexExpressionIndexValue && indexExpressionObjectValue->getType()->isPointerTy() ) {
				llvm::Type* indexExpressionElementPointerType = getPointeeType( indexExpressionObjectValue );
				if( indexExpressionElementPointerType->isArrayTy() ) {
					llvm::ConstantInt* indexExpressionZeroConstant = llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->context ), 0 );
					return this->builder.CreateGEP( indexExpressionElementPointerType, indexExpressionObjectValue, {indexExpressionZeroConstant, indexExpressionIndexValue}, "elemptr" );
				}
			}
		}
		else if( expression->kind == ast::Node::Kind::MemberAccessExpression ) {
			ast::nodes::MemberAccessExpression& memberAccessExpression = static_cast<ast::nodes::MemberAccessExpression&>( *expression );
			llvm::Value* memberAccessObjectPointer = this->resolveObjectPointer( memberAccessExpression.object );
			if( memberAccessObjectPointer && memberAccessObjectPointer->getType()->isPointerTy() ) {
				llvm::StructType* memberAccessStructLLVMType = nullptr;
				std::string memberAccessStructName;
				llvm::Type* memberAccessPointerElementType = getPointeeType( memberAccessObjectPointer );
				if( memberAccessPointerElementType->isStructTy() ) {
					memberAccessStructLLVMType = llvm::cast<llvm::StructType>( memberAccessPointerElementType );
					memberAccessStructName = memberAccessStructLLVMType->getName().str();
				}
				else {
					memberAccessStructName = this->resolveStructTypeName( memberAccessExpression.object );
					if( memberAccessStructName.empty() == false ) {
						std::unordered_map<std::string, llvm::StructType*>::iterator stIt = this->structTypes.find( memberAccessStructName );
						if( stIt != this->structTypes.end() ) {
							memberAccessStructLLVMType = stIt->second;
							if( memberAccessPointerElementType->isPointerTy() && llvm::isa<llvm::GetElementPtrInst>( memberAccessObjectPointer ) ) {
								memberAccessObjectPointer = this->builder.CreateLoad(
									llvm::PointerType::getUnqual( this->context ),
									memberAccessObjectPointer, "nested.obj.ptr" );
							}
						}
					}
				}
				if( memberAccessStructLLVMType ) {
					unsigned int memberAccessFieldIndex = 0;
					std::unordered_map<std::string,std::unordered_map<std::string,unsigned int>>::iterator memberAccessStructIterator = this->structFieldIndices.find( memberAccessStructName );
					if( memberAccessStructIterator != this->structFieldIndices.end() ) {
						std::unordered_map<std::string,unsigned int>::iterator memberAccessFieldIterator = memberAccessStructIterator->second.find( memberAccessExpression.member );
						if( memberAccessFieldIterator != memberAccessStructIterator->second.end() ) {
							memberAccessFieldIndex = memberAccessFieldIterator->second;
						}
					}
					return this->builder.CreateStructGEP( memberAccessStructLLVMType, memberAccessObjectPointer, memberAccessFieldIndex, "fieldptr" );
				}
				if( memberAccessPointerElementType->isPointerTy() ) {
					llvm::Value* loadedClassPointer = this->builder.CreateLoad( memberAccessPointerElementType, memberAccessObjectPointer, "nested.ptr" );
					llvm::StructType* nestedStructType = nullptr;
					std::string nestedTypeName = this->resolveStructTypeName( memberAccessExpression.object );
					if( nestedTypeName.empty() == false ) {
						std::unordered_map<std::string, llvm::StructType*>::iterator stIt = this->structTypes.find( nestedTypeName );
						if( stIt != this->structTypes.end() ) {
							nestedStructType = stIt->second;
						}
					}
					if( nestedStructType == nullptr ) {
						return loadedClassPointer;
					}
					std::string nestedStructName = nestedStructType->getName().str();
					unsigned int nestedFieldIndex = 0;
					std::unordered_map<std::string,std::unordered_map<std::string,unsigned int>>::iterator nestedStructIterator = this->structFieldIndices.find( nestedStructName );
					if( nestedStructIterator != this->structFieldIndices.end() ) {
						std::unordered_map<std::string,unsigned int>::iterator nestedFieldIterator = nestedStructIterator->second.find( memberAccessExpression.member );
						if( nestedFieldIterator != nestedStructIterator->second.end() ) {
							nestedFieldIndex = nestedFieldIterator->second;
						}
					}
					return this->builder.CreateStructGEP( nestedStructType, loadedClassPointer, nestedFieldIndex, "fieldptr" );
				}
			}
		}
		else if( expression->kind == ast::Node::Kind::SelfExpression ) {
			std::unordered_map<std::string,llvm::Value*>::iterator selfValueIterator = this->namedValues.find( "self" );
			if( selfValueIterator != this->namedValues.end() ) {
				llvm::Value* selfObjectPointer = selfValueIterator->second;
				llvm::Type* selfElementPointerType = getPointeeType( selfObjectPointer );
				if( selfElementPointerType->isPointerTy() ) {
					return this->builder.CreateLoad( selfElementPointerType, selfObjectPointer, "self.ptr" );
				}
				return selfObjectPointer;
			}
		}	
		return nullptr;
	}
	
	std::string LLVMCodegen::resolveStructTypeName( const ast::nodes::ExpressionSharedPointer& expression ) {
		if( expression->kind == ast::Node::Kind::BinaryExpression ) {
			ast::nodes::BinaryExpression& binaryExpression = static_cast<ast::nodes::BinaryExpression&>( *expression );
			if( binaryExpression.operation == token::Type::Equal ||
				binaryExpression.operation == token::Type::NotEqual ||
				binaryExpression.operation == token::Type::LessThan ||
				binaryExpression.operation == token::Type::LessThanEqual ||
				binaryExpression.operation == token::Type::GreaterThan ||
				binaryExpression.operation == token::Type::GreaterThanEqual ||
				binaryExpression.operation == token::Type::KeywordAnd ||
				binaryExpression.operation == token::Type::KeywordOr ) {
				return "Boolean";
			}
			std::string binaryExpressionLeftTypeName = this->resolveStructTypeName( binaryExpression.left );
			return binaryExpressionLeftTypeName.empty() ? this->resolveStructTypeName( binaryExpression.right ) : binaryExpressionLeftTypeName;
		}
		else if( expression->kind == ast::Node::Kind::BooleanLiteral ) {
			return "Boolean";
		}
		else if( expression->kind == ast::Node::Kind::CallExpression ) {
			return "";
		}
		else if( expression->kind == ast::Node::Kind::CharLiteral ) {
			return "Char";
		}
		else if( expression->kind == ast::Node::Kind::FloatLiteral ) {
			return "F64";
		}
		else if( expression->kind == ast::Node::Kind::IdentifierExpression ) {
			ast::nodes::IdentifierExpression& identifierExpression = static_cast<ast::nodes::IdentifierExpression&>( *expression );
			std::unordered_map<std::string, std::string>::iterator identifierVariableTypeIterator = this->variableStructType.find( identifierExpression.name );
			if( identifierVariableTypeIterator != this->variableStructType.end() ) {
				return identifierVariableTypeIterator->second;
			}
			std::unordered_map<std::string, llvm::Value*>::iterator identifierValueIterator = this->namedValues.find( identifierExpression.name );
			if( identifierValueIterator != this->namedValues.end() ) {
				llvm::Type* identifierElementPointerType = getPointeeType( identifierValueIterator->second );
				if( identifierElementPointerType->isFloatTy() ) return "F32";
				if( identifierElementPointerType->isDoubleTy() ) return "F64";
				if( identifierElementPointerType->isIntegerTy( 1 ) ) return "Boolean";
				if( identifierElementPointerType->isIntegerTy( 8 ) ) return "I8";
				if( identifierElementPointerType->isIntegerTy( 16 ) ) return "I16";
				if( identifierElementPointerType->isIntegerTy( 32 ) ) return "I32";
				if( identifierElementPointerType->isIntegerTy( 64 ) ) return "I64";
				if( identifierElementPointerType == llvm::PointerType::getUnqual( this->context ) ) return "String";
				if( identifierElementPointerType->isStructTy() ) {
					return llvm::cast<llvm::StructType>( identifierElementPointerType )->getName().str();
				}
				if( identifierElementPointerType->isPointerTy() ) {
					std::unordered_map<std::string, std::string>::iterator vsIt = this->variableStructType.find( identifierExpression.name );
					if( vsIt != this->variableStructType.end() ) {
						return vsIt->second;
					}
				}
			}
			llvm::GlobalVariable* identifierGlobalVar = this->module->getGlobalVariable( identifierExpression.name, true );
			if( identifierGlobalVar ) {
				llvm::Type* identifierGlobalType = identifierGlobalVar->getValueType();
				if( identifierGlobalType->isStructTy() ) {
					return llvm::cast<llvm::StructType>( identifierGlobalType )->getName().str();
				}
				if( identifierGlobalType->isPointerTy() ) {
					std::unordered_map<std::string, std::string>::iterator vsIt = this->variableStructType.find( identifierExpression.name );
					if( vsIt != this->variableStructType.end() ) {
						return vsIt->second;
					}
				}
			}
			semantic::TypeSharedPointer identifierSemanticType = this->analyzer.types().lookupType( identifierExpression.name );
			if( identifierSemanticType &&
				( identifierSemanticType->kind == semantic::Type::Kind::Class ||
				  identifierSemanticType->kind == semantic::Type::Kind::Struct ) ) {
				if( identifierSemanticType->kind == semantic::Type::Kind::Class ) {
					semantic::ClassTypeSharedPointer identifierClassType = std::static_pointer_cast<semantic::ClassType>( identifierSemanticType );
					if( identifierClassType->astDeclaration &&
						identifierClassType->astDeclaration->genericParameters.empty() == false ) {
						return "";
					}
				}
				return identifierExpression.name;
			}
		}
		else if( expression->kind == ast::Node::Kind::IntegerLiteral ) {
			return "I64";
		}
		else if( expression->kind == ast::Node::Kind::MemberAccessExpression ) {
			ast::nodes::MemberAccessExpression& memberAccessExpression = static_cast<ast::nodes::MemberAccessExpression&>( *expression );
			
			if( memberAccessExpression.object->kind == ast::Node::Kind::IdentifierExpression ) {
				ast::nodes::IdentifierExpression& memberAccessObjectIdentifier = static_cast<ast::nodes::IdentifierExpression&>( *memberAccessExpression.object );
				if( this->enumTypeNames.count( memberAccessObjectIdentifier.name ) ) return memberAccessObjectIdentifier.name;
			}
			llvm::Value* memberAccessObjectPointer = this->resolveObjectPointer( memberAccessExpression.object );
			if( memberAccessObjectPointer && memberAccessObjectPointer->getType()->isPointerTy() ) {
				llvm::Type* memberAccessPointerElementType = getPointeeType( memberAccessObjectPointer );
				std::string memberAccessStructName;
				if( memberAccessPointerElementType->isStructTy() ) {
					llvm::StructType* memberAccessStructLLVMType = llvm::cast<llvm::StructType>( memberAccessPointerElementType );
					memberAccessStructName = memberAccessStructLLVMType->getName().str();
				}
				else {
					memberAccessStructName = this->resolveStructTypeName( memberAccessExpression.object );
				}
				if( memberAccessStructName.empty() == false ) {
					semantic::TypeSharedPointer memberAccessSemanticType = this->analyzer.types().lookupType( memberAccessStructName );
					if( memberAccessSemanticType ) {
						std::function<std::string( const semantic::TypeSharedPointer& )> memberAccessResolveFieldTypeName = []( const semantic::TypeSharedPointer& fieldType ) -> std::string {
							if( fieldType == nullptr ) return "";
							std::string typeName;
							if( fieldType->kind == semantic::Type::Kind::Optional ) {
								semantic::OptionalTypeSharedPointer optionalType = std::static_pointer_cast<semantic::OptionalType>( fieldType );
								typeName = optionalType->inner ? optionalType->inner->name : "";
							}
							else {
								typeName = fieldType->name;
							}
							size_t genericPos = typeName.find( '<' );
							if( genericPos != std::string::npos ) {
								typeName = typeName.substr( 0, genericPos );
							}
							return typeName;
						};
						if( memberAccessSemanticType->kind == semantic::Type::Kind::Class ) {
							semantic::ClassTypeSharedPointer memberAccessClassType = std::static_pointer_cast<semantic::ClassType>( memberAccessSemanticType );
							for( const semantic::FieldInfo& field : memberAccessClassType->fields ) {
								if( field.name == memberAccessExpression.member ) {
									return memberAccessResolveFieldTypeName( field.type );
								}
							}
						}
						else if( memberAccessSemanticType->kind == semantic::Type::Kind::Struct ) {
							semantic::StructTypeSharedPointer memberAccessStructType = std::static_pointer_cast<semantic::StructType>( memberAccessSemanticType );
							for( const semantic::FieldInfo& field : memberAccessStructType->fields ) {
								if( field.name == memberAccessExpression.member ) {
									return memberAccessResolveFieldTypeName( field.type );
								}
							}
						}
					}
				}
			}
		}
		else if( expression->kind == ast::Node::Kind::MethodCallExpression ) {
			ast::nodes::MethodCallExpression& methodCallExpression = static_cast<ast::nodes::MethodCallExpression&>( *expression );
			std::string methodCallObjectTypeName = this->resolveStructTypeName( methodCallExpression.object );
			if( methodCallObjectTypeName.empty() == false ) {
				const descriptor::BuiltinMethodDescriptor* methodCallLookupResult = this->builtinRegistry.lookupMethod( methodCallObjectTypeName, methodCallExpression.method );
				if( methodCallLookupResult ) {
					return methodCallLookupResult->returnTypeName;
				}
				semantic::TypeSharedPointer methodCallSemanticType = this->analyzer.types().lookupType( methodCallObjectTypeName );
				if( methodCallSemanticType ) {
					semantic::MethodInfo* methodCallMethodInfo = nullptr;
					if( methodCallSemanticType->kind == semantic::Type::Kind::Class ) {
						methodCallMethodInfo = std::static_pointer_cast<semantic::ClassType>( methodCallSemanticType )->findMethod( methodCallExpression.method );
					}
					else if( methodCallSemanticType->kind == semantic::Type::Kind::Struct ) {
						methodCallMethodInfo = std::static_pointer_cast<semantic::StructType>( methodCallSemanticType )->findMethod( methodCallExpression.method );
					}
					else if( methodCallSemanticType->kind == semantic::Type::Kind::Interface ) {
						methodCallMethodInfo = std::static_pointer_cast<semantic::InterfaceType>( methodCallSemanticType )->findMethod( methodCallExpression.method );
					}
					if( methodCallMethodInfo && methodCallMethodInfo->type && methodCallMethodInfo->type->kind == semantic::Type::Kind::Function ) {
						semantic::FunctionTypeSharedPointer methodCallFunctionType = std::static_pointer_cast<semantic::FunctionType>( methodCallMethodInfo->type );
						if( methodCallFunctionType->returnType && methodCallFunctionType->returnType->name.empty() == false ) {
							return methodCallFunctionType->returnType->name;
						}
					}
				}
			}
			return methodCallObjectTypeName;
		}
		else if( expression->kind == ast::Node::Kind::SelfExpression ) {
			return this->currentClassName;
		}
		else if( expression->kind == ast::Node::Kind::StringLiteral ) {
			return "String";
		}
		else if( expression->kind == ast::Node::Kind::UnaryExpression ) {
			ast::nodes::UnaryExpression& unaryExpression = static_cast<ast::nodes::UnaryExpression&>( *expression );
			return this->resolveStructTypeName( unaryExpression.operand );
		}
		return "";
	}
	
	llvm::Type* LLVMCodegen::toLLVMType( const semantic::TypeSharedPointer& type ) {
		if( type == nullptr ) {
			return llvm::Type::getVoidTy( this->context );
		}
		switch( type->kind ) {
			case semantic::Type::Kind::Array: {
				semantic::ArrayTypeSharedPointer arrayType = std::static_pointer_cast<semantic::ArrayType>( type );
				llvm::Type* arrayElementLLVMType = this->toLLVMType( arrayType->elementType );
				if( arrayType->size >= 0 ) {
					return llvm::ArrayType::get( arrayElementLLVMType, arrayType->size );
				}
				
				// Dynamic array: pointer + length struct
				return llvm::StructType::get( this->context, {
					llvm::PointerType::getUnqual( arrayElementLLVMType ),
					llvm::Type::getInt64Ty( this->context ) // length
				});
			}
			case semantic::Type::Kind::Bool:
				return llvm::Type::getInt1Ty( this->context );
			case semantic::Type::Kind::Callable: {
				semantic::CallableTypeSharedPointer callableType = std::static_pointer_cast<semantic::CallableType>( type );
				llvm::Type* callableReturnType = this->toLLVMType( callableType->returnType );
				std::vector<llvm::Type*> callableParameterTypes;
				for( semantic::TypeSharedPointer& parameter : callableType->parameterTypes ) {
					callableParameterTypes.push_back( this->toLLVMType( parameter ) );
				}
				return llvm::PointerType::getUnqual(
					llvm::FunctionType::get(
						callableReturnType,
						callableParameterTypes,
						false
					)
				);
			}
			case semantic::Type::Kind::Char:
				return llvm::Type::getInt32Ty( this->context ); // Unicode char
			case semantic::Type::Kind::Class: {
				semantic::ClassTypeSharedPointer classType = std::static_pointer_cast<semantic::ClassType>( type );
				
				// OOP wrapper types → primitive LLVM types
				std::string className = classType->astDeclaration ? classType->astDeclaration->name : type->name;
				const std::string& q = type->qualified;
				if( q == semantic::qname::INT || q == semantic::qname::I64 || q == semantic::qname::LONG ||
					className == "Int" || className == "I64" || className == "Long" ) {
					return llvm::Type::getInt64Ty( this->context );
				}
				if( q == semantic::qname::I32 || q == semantic::qname::INTEGER ||
					className == "I32" || className == "Integer" ) {
					return llvm::Type::getInt32Ty( this->context );
				}
				if( q == semantic::qname::I16 || className == "I16" ) {
					return llvm::Type::getInt16Ty( this->context );
				}
				if( q == semantic::qname::I8 || q == semantic::qname::BYTE ||
					className == "I8" || className == "Byte" ) {
					return llvm::Type::getInt8Ty( this->context );
				}
				if( q == semantic::qname::UINT || q == semantic::qname::U64 ||
					className == "UInt" || className == "U64" ) {
					return llvm::Type::getInt64Ty( this->context );
				}
				if( q == semantic::qname::U32 || className == "U32" ) {
					return llvm::Type::getInt32Ty( this->context );
				}
				if( q == semantic::qname::U16 || className == "U16" ) {
					return llvm::Type::getInt16Ty( this->context );
				}
				if( q == semantic::qname::U8 || className == "U8" ) {
					return llvm::Type::getInt8Ty( this->context );
				}
				if( q == semantic::qname::F64 || q == semantic::qname::DOUBLE || q == semantic::qname::FLOAT ||
					className == "F64" || className == "Double" || className == "Float" ) {
					return llvm::Type::getDoubleTy( this->context );
				}
				if( q == semantic::qname::F32 || className == "F32" ) {
					return llvm::Type::getFloatTy( this->context );
				}
				if( q == semantic::qname::BOOLEAN || className == "Boolean" ) {
					return llvm::Type::getInt1Ty( this->context );
				}
				if( q == semantic::qname::STRING || className == "String" ) {
					return llvm::PointerType::getUnqual( this->context );
				}
				if( q == semantic::qname::CHAR || className == "Char" ) {
					return llvm::Type::getInt32Ty( this->context );
				}
				if( q == semantic::qname::OBJECT || className == "Object" ) {
					return llvm::PointerType::getUnqual( this->context );
				}
				if( q == semantic::qname::VOID || q == semantic::qname::NONETYPE ||
					className == "Void" || className == "NoneType" ) {
					return llvm::Type::getVoidTy( this->context );
				}
				
				// Memory<T> → T* (raw pointer, not a struct)
				if( classType->astDeclaration != nullptr &&
					( type->qualified == semantic::qname::MEMORY || semantic::qname::startsWith( type->qualified, semantic::qname::MEMORY + "<" ) || classType->astDeclaration->name == "Memory" ) ) {
					std::unordered_map<std::string, semantic::TypeSharedPointer>::iterator classSubstitutionIterator = classType->typeSubstitutions.begin();
					if( classSubstitutionIterator != classType->typeSubstitutions.end() ) {
						return llvm::PointerType::getUnqual( this->toLLVMType( classSubstitutionIterator->second ) );
					}
					return llvm::PointerType::getUnqual( this->context );
				}
				if( type->qualified == semantic::qname::MEMORY || semantic::qname::startsWith( type->qualified, semantic::qname::MEMORY + "<" ) || type->name == "Memory" ) {
					return llvm::PointerType::getUnqual( this->context );
				}
				
				// Arena<T> → {T*, i64, i64}* (pointer to arena struct, passed by reference)
				if( classType->astDeclaration &&
					( type->qualified == semantic::qname::ARENA || semantic::qname::startsWith( type->qualified, semantic::qname::ARENA + "<" ) || classType->astDeclaration->name == "Arena" ) ) {
					llvm::Type* classArenaElementLLVMType = llvm::Type::getInt64Ty( this->context );
					std::unordered_map<std::string, semantic::TypeSharedPointer>::iterator classArenaSubstitutionIterator = classType->typeSubstitutions.begin();
					if( classArenaSubstitutionIterator != classType->typeSubstitutions.end() ) {
						classArenaElementLLVMType = this->toLLVMType( classArenaSubstitutionIterator->second );
						if( classArenaElementLLVMType->isStructTy() ) {
							// Raw struct type for pool allocation
						}
						else if( classArenaElementLLVMType->isPointerTy() ) {
							std::string substitutionTypeName = classArenaSubstitutionIterator->second->name;
							std::unordered_map<std::string, llvm::StructType*>::iterator stIt = this->structTypes.find( substitutionTypeName );
							if( stIt != this->structTypes.end() ) {
								classArenaElementLLVMType = stIt->second;
							}
						}
					}
					llvm::StructType* classArenaStructType = llvm::StructType::get( this->context, {
						llvm::PointerType::getUnqual( classArenaElementLLVMType ),
						llvm::Type::getInt64Ty( this->context ),
						llvm::Type::getInt64Ty( this->context )
					});
					return llvm::PointerType::getUnqual( classArenaStructType );
				}
				if( type->qualified == semantic::qname::ARENA || semantic::qname::startsWith( type->qualified, semantic::qname::ARENA + "<" ) || type->name == "Arena" ) {
					llvm::StructType* classArenaStaticStructType = llvm::StructType::get( this->context, {
						llvm::PointerType::getUnqual( this->context ),
						llvm::Type::getInt64Ty( this->context ),
						llvm::Type::getInt64Ty( this->context )
					});
					return llvm::PointerType::getUnqual( classArenaStaticStructType );
				}
				
				// Monomorphized classes → use base class struct type
				if( classType->astDeclaration && classType->astDeclaration->name != type->name ) {
					std::string classBaseName( classType->astDeclaration->name );
					semantic::TypeSharedPointer classBaseType = this->analyzer.types().lookupType( classBaseName );
					if( classBaseType ) {
						return this->getOrCreateStructType( classBaseName, classBaseType );
					}
				}
				return this->getOrCreateStructType( type->name, type );
			}
			case semantic::Type::Kind::Enum: {
				
				// Simple enums (no payload) are represented as i32 tag
				semantic::EnumTypeSharedPointer enumType = std::static_pointer_cast<semantic::EnumType>( type );
				for( semantic::EnumVariantInfo& enumVariant : enumType->variants ) {
					if( enumVariant.associatedTypes.empty() == false ) {
						
						// Use the tagged union struct type
						std::unordered_map<std::string, llvm::StructType*>::iterator enumStructIterator = this->structTypes.find( type->name );
						if( enumStructIterator != this->structTypes.end() ) {
							return enumStructIterator->second;
						}
					}
				}
				return llvm::Type::getInt32Ty( this->context );
			}
			case semantic::Type::Kind::Float: {
				semantic::FloatTypeSharedPointer floatType = std::static_pointer_cast<semantic::FloatType>( type );
				if( floatType->bitWidth == 32 ) {
					return llvm::Type::getFloatTy( this->context );
				}
				return llvm::Type::getDoubleTy( this->context );
			}
			case semantic::Type::Kind::Function: {
				semantic::FunctionTypeSharedPointer functionType = std::static_pointer_cast<semantic::FunctionType>( type );
				llvm::Type* functionReturnType = this->toLLVMType( functionType->returnType );
				std::vector<llvm::Type*> functionParameterTypes;
				for( semantic::TypeSharedPointer& parameter : functionType->parameterTypes ) {
					functionParameterTypes.push_back( this->toLLVMType( parameter ) );
				}
				return llvm::PointerType::getUnqual(
					llvm::FunctionType::get(
						functionReturnType,
						functionParameterTypes,
						false
					)
				);
			}
			case semantic::Type::Kind::Future:
				
				// Future<T> is an i64 task ID at runtime
				return llvm::Type::getInt64Ty( this->context );
			case semantic::Type::Kind::Generator: {
				semantic::GeneratorTypeSharedPointer generatorType = std::static_pointer_cast<semantic::GeneratorType>( type );
				llvm::Type* generatorYieldLLVMType = this->toLLVMType( generatorType->yieldType );
				return llvm::StructType::get( this->context, { llvm::Type::getInt32Ty( this->context ), generatorYieldLLVMType, llvm::Type::getInt1Ty( this->context ) });
			}
			case semantic::Type::Kind::Integer:
				return llvm::IntegerType::get( this->context, std::static_pointer_cast<semantic::IntegerType>( type )->bitWidth );
			case semantic::Type::Kind::Interface:
				return this->getInterfaceFatPointerType(); // {i8* obj, i8* itable}
			case semantic::Type::Kind::Meta:
				return llvm::Type::getInt64Ty( this->context );
			case semantic::Type::Kind::Optional: {
				semantic::OptionalTypeSharedPointer optionalType = std::static_pointer_cast<semantic::OptionalType>( type );
				llvm::Type* optionalInnerLLVMType = this->toLLVMType( optionalType->inner );
				if( optionalInnerLLVMType->isStructTy() &&
					( optionalType->inner->kind == semantic::Type::Kind::Class ||
					  optionalType->inner->kind == semantic::Type::Kind::Struct ) ) {
					return llvm::PointerType::getUnqual( optionalInnerLLVMType );
				}
				if( optionalInnerLLVMType->isPointerTy() == false ) {
					return llvm::StructType::get( this->context, { llvm::Type::getInt1Ty( this->context ), optionalInnerLLVMType } );
				}
				return optionalInnerLLVMType;
			}
			case semantic::Type::Kind::Pointer:
				return llvm::PointerType::getUnqual( this->context );
			case semantic::Type::Kind::Reference:
				
				// All pointers/references are i8* for simplicity
				return llvm::PointerType::getUnqual( this->context );
			case semantic::Type::Kind::String:
				return llvm::PointerType::getUnqual( this->context ); // char*
			case semantic::Type::Kind::Struct: {
				semantic::StructTypeSharedPointer structType = std::static_pointer_cast<semantic::StructType>( type );
				if( structType->astDeclaration && structType->astDeclaration->name != type->name ) {
					std::string structBaseName( structType->astDeclaration->name );
					semantic::TypeSharedPointer structBaseType = this->analyzer.types().lookupType( structBaseName );
					if( structBaseType ) {
						return this->getOrCreateStructType( structBaseName, structBaseType );
					}
				}
				return this->getOrCreateStructType( type->name, type );
			}
			case semantic::Type::Kind::Trait:
				return this->getInterfaceFatPointerType(); // {i8* obj, i8* itable}
			case semantic::Type::Kind::Tuple: {
				semantic::TupleTypeSharedPointer tupleType = std::static_pointer_cast<semantic::TupleType>( type );
				std::vector<llvm::Type*> tupleElementTypes;
				for( semantic::TypeSharedPointer& element : tupleType->elements ) {
					tupleElementTypes.push_back( this->toLLVMType( element ) );
				}
				return llvm::StructType::get( this->context, tupleElementTypes );
			}
			case semantic::Type::Kind::Union: {
				semantic::UnionTypeSharedPointer unionType = std::static_pointer_cast<semantic::UnionType>( type );
				
				// Tagged union: {i32 tag, largest_member_type}
				unsigned int unionMaxSize = 0;
				llvm::Type* unionWidestLLVMType = llvm::Type::getInt64Ty( this->context );
				for( semantic::TypeSharedPointer& item : unionType->types ) {
					llvm::Type* itemLLVMType = this->toLLVMType( item );
					llvm::TypeSize itemSize = this->module->getDataLayout().getTypeAllocSize( itemLLVMType );
					if( itemSize > unionMaxSize ) {
						unionMaxSize = static_cast<unsigned int>( itemSize );
						unionWidestLLVMType = itemLLVMType;
					}
				}
				return llvm::StructType::get( this->context, { llvm::Type::getInt32Ty( this->context ), unionWidestLLVMType } );
			}
			case semantic::Type::Kind::Void:
				return llvm::Type::getVoidTy( this->context );
			default:
				return llvm::Type::getInt64Ty( this->context ); // fallback
		}
	}
	
	llvm::Value* LLVMCodegen::unwrapOOPWrapperValue( llvm::Value* value, const std::string& wrapperTypeHint ) {
		static const std::set<std::string> oopWrapperClasses = {
			"Int", "I8", "I16", "I32", "I64", "UInt", "U8", "U16", "U32", "U64",
			"Float", "F32", "F64", "Double", "Long", "Integer", "Boolean", "Byte",
			"Char", "String", "Void", "Object"
		};
		if( value == nullptr ) {
			return value;
		}
		if( value->getType()->isPointerTy() ) {
			llvm::Type* pointeeType = getPointeeType( value );
			if( pointeeType->isStructTy() ) {
				llvm::StructType* structType = llvm::cast<llvm::StructType>( pointeeType );
				if( structType->hasName() && oopWrapperClasses.count( structType->getName().str() ) && structType->getNumElements() == 1 && structType != this->getInterfaceFatPointerType() ) {
					llvm::Value* fieldPointer = this->builder.CreateStructGEP( structType, value, 0, "wrapper.field.ptr" );
					return this->builder.CreateLoad( structType->getElementType( 0 ), fieldPointer, "wrapper.val" );
				}
			}
			else if( pointeeType->isPointerTy() && wrapperTypeHint.empty() == false && oopWrapperClasses.count( wrapperTypeHint ) ) {
				std::unordered_map<std::string, llvm::StructType*>::iterator stIt = this->structTypes.find( wrapperTypeHint );
				if( stIt != this->structTypes.end() && stIt->second->getNumElements() == 1 && stIt->second != this->getInterfaceFatPointerType() ) {
					llvm::Type* innerElementType = stIt->second->getElementType( 0 );
					if( innerElementType->isIntegerTy() || innerElementType->isFloatingPointTy() ) {
						llvm::Value* fieldPointer = this->builder.CreateStructGEP( stIt->second, value, 0, "wrapper.field.ptr" );
						return this->builder.CreateLoad( innerElementType, fieldPointer, "wrapper.val" );
					}
				}
			}
		}
		return value;
	}
	
	bool LLVMCodegen::isThrowableClass( const std::string& typeName ) {
		semantic::TypeSharedPointer type = this->analyzer.types().lookupType( typeName );
		while( type && type->kind == semantic::Type::Kind::Class ) {
			semantic::ClassTypeSharedPointer classType = std::static_pointer_cast<semantic::ClassType>( type );
			for( const semantic::TypeSharedPointer& iface : classType->interfaces ) {
				if( iface->qualified == semantic::qname::THROWABLE || iface->name == "Throwable" ) {
					return true;
				}
			}
			type = classType->baseClass;
		}
		return false;
	}
	
	void LLVMCodegen::injectTracebackInfo( llvm::Value* objectPointer, const std::string& typeName, const lookup::SourceSharedPointer& source, bool overwriteFileAndLine ) {
		std::unordered_map<std::string, std::unordered_map<std::string, unsigned>>::iterator fieldIndicesIterator = this->structFieldIndices.find( typeName );
		if( fieldIndicesIterator == this->structFieldIndices.end() ) {
			return;
		}
		std::unordered_map<std::string, unsigned>& fieldIndices = fieldIndicesIterator->second;
		llvm::Type* objectPtrElementType = getPointeeType( objectPointer );
		if( objectPtrElementType->isStructTy() == false ) {
			return;
		}
		llvm::StructType* objectStructType = llvm::cast<llvm::StructType>( objectPtrElementType );
		if( overwriteFileAndLine ) {
			std::unordered_map<std::string, unsigned>::iterator fileFieldIterator = fieldIndices.find( "file" );
			if( fileFieldIterator != fieldIndices.end() && fileFieldIterator->second < objectStructType->getNumElements() ) {
				llvm::Constant* fileStr = this->builder.CreateGlobalStringPtr( source->filename, "traceback.file" );
				llvm::Value* fileGEP = this->builder.CreateStructGEP( objectStructType, objectPointer, fileFieldIterator->second, "tb.file.ptr" );
				this->builder.CreateStore( fileStr, fileGEP );
			}
			std::unordered_map<std::string, unsigned>::iterator lineFieldIterator = fieldIndices.find( "line" );
			if( lineFieldIterator != fieldIndices.end() && lineFieldIterator->second < objectStructType->getNumElements() ) {
				uint32_t lineNumber = source->location ? source->location->line : 0;
				llvm::Value* lineValue = llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->context ), lineNumber );
				llvm::Value* lineGEP = this->builder.CreateStructGEP( objectStructType, objectPointer, lineFieldIterator->second, "tb.line.ptr" );
				this->builder.CreateStore( lineValue, lineGEP );
			}
		}
		std::unordered_map<std::string, unsigned>::iterator tracebackFieldIterator = fieldIndices.find( "traceback" );
		if( tracebackFieldIterator != fieldIndices.end() && tracebackFieldIterator->second < objectStructType->getNumElements() ) {
			this->ensureClassMethodsRegistered( "Traceback" );
			llvm::StructType* tracebackStructType = nullptr;
			std::unordered_map<std::string, llvm::StructType*>::iterator tbStructIterator = this->structTypes.find( "Traceback" );
			if( tbStructIterator != this->structTypes.end() ) {
				tracebackStructType = tbStructIterator->second;
			}
			else {
				semantic::TypeSharedPointer tbSemaType = this->analyzer.types().lookupType( "Traceback" );
				if( tbSemaType ) {
					tracebackStructType = this->getOrCreateStructType( "Traceback", tbSemaType );
				}
			}
			if( tracebackStructType ) {
				llvm::DataLayout layout = this->module->getDataLayout();
				uint64_t tbSize = layout.getTypeAllocSize( tracebackStructType );
				llvm::Function* mallocFunction = this->getOrCreateMalloc();
				llvm::Value* tbRaw = this->builder.CreateCall( mallocFunction, { llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->context ), tbSize ) }, "tb.raw" );
				llvm::Value* tbPointer = this->builder.CreateBitCast( tbRaw, llvm::PointerType::getUnqual( tracebackStructType ), "tb.obj" );
				std::unordered_map<std::string, std::unordered_map<std::string, unsigned>>::iterator tbFieldIndicesIterator = this->structFieldIndices.find( "Traceback" );
				if( tbFieldIndicesIterator != this->structFieldIndices.end() ) {
					std::unordered_map<std::string, unsigned>& tbFieldIndices = tbFieldIndicesIterator->second;
					std::unordered_map<std::string, unsigned>::iterator tbFileIterator = tbFieldIndices.find( "file" );
					if( tbFileIterator != tbFieldIndices.end() ) {
						llvm::Constant* tbFileStr = this->builder.CreateGlobalStringPtr( source->filename, "tb.inner.file" );
						llvm::Value* tbFileGEP = this->builder.CreateStructGEP( tracebackStructType, tbPointer, tbFileIterator->second, "tb.file.gep" );
						this->builder.CreateStore( tbFileStr, tbFileGEP );
					}
					std::unordered_map<std::string, unsigned>::iterator tbLineIterator = tbFieldIndices.find( "line" );
					if( tbLineIterator != tbFieldIndices.end() ) {
						uint32_t lineNumber = source->location ? source->location->line : 0;
						llvm::Value* tbLineValue = llvm::ConstantInt::get( llvm::Type::getInt64Ty( this->context ), lineNumber );
						llvm::Value* tbLineGEP = this->builder.CreateStructGEP( tracebackStructType, tbPointer, tbLineIterator->second, "tb.line.gep" );
						this->builder.CreateStore( tbLineValue, tbLineGEP );
					}
					std::unordered_map<std::string, unsigned>::iterator tbFuncIterator = tbFieldIndices.find( "functionName" );
					if( tbFuncIterator != tbFieldIndices.end() ) {
						std::string functionName = this->currentFunction ? this->currentFunction->getName().str() : "<unknown>";
						llvm::Constant* tbFuncStr = this->builder.CreateGlobalStringPtr( functionName, "tb.inner.func" );
						llvm::Value* tbFuncGEP = this->builder.CreateStructGEP( tracebackStructType, tbPointer, tbFuncIterator->second, "tb.func.gep" );
						this->builder.CreateStore( tbFuncStr, tbFuncGEP );
					}
					std::unordered_map<std::string, unsigned>::iterator tbModIterator = tbFieldIndices.find( "moduleName" );
					if( tbModIterator != tbFieldIndices.end() ) {
						llvm::Constant* tbModStr = this->builder.CreateGlobalStringPtr( source->filename, "tb.inner.mod" );
						llvm::Value* tbModGEP = this->builder.CreateStructGEP( tracebackStructType, tbPointer, tbModIterator->second, "tb.mod.gep" );
						this->builder.CreateStore( tbModStr, tbModGEP );
					}
				}
				llvm::Value* tbFieldGEP = this->builder.CreateStructGEP( objectStructType, objectPointer, tracebackFieldIterator->second, "tb.field.ptr" );
				this->builder.CreateStore( tbPointer, tbFieldGEP );
			}
		}
	}
	
	llvm::Value* LLVMCodegen::tryBuiltInDescriptor( const std::string& typeName, const std::string& methodName, llvm::Value* self, std::vector<llvm::Value*>& arguments, std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments ) {
		const descriptor::BuiltinMethodDescriptor* method = this->builtinRegistry.lookupMethod( typeName, methodName );
		if( method && method->irGenerator ) {
			llvm::Value* unwrappedSelf = self;
			if( unwrappedSelf != nullptr && unwrappedSelf->getType()->isPointerTy() ) {
				llvm::Type* pointeeType = getPointeeType( unwrappedSelf );
				if( pointeeType->isPointerTy() ) {
					llvm::Type* primitiveType = nullptr;
					if( typeName == "Int" || typeName == "I64" || typeName == "Long" || typeName == "Integer" ) {
						primitiveType = llvm::Type::getInt64Ty( this->context );
					}
					else if( typeName == "I32" ) {
						primitiveType = llvm::Type::getInt32Ty( this->context );
					}
					else if( typeName == "I16" ) {
						primitiveType = llvm::Type::getInt16Ty( this->context );
					}
					else if( typeName == "I8" || typeName == "Byte" ) {
						primitiveType = llvm::Type::getInt8Ty( this->context );
					}
					else if( typeName == "UInt" || typeName == "U64" ) {
						primitiveType = llvm::Type::getInt64Ty( this->context );
					}
					else if( typeName == "U32" ) {
						primitiveType = llvm::Type::getInt32Ty( this->context );
					}
					else if( typeName == "U16" ) {
						primitiveType = llvm::Type::getInt16Ty( this->context );
					}
					else if( typeName == "U8" || typeName == "Char" ) {
						primitiveType = llvm::Type::getInt8Ty( this->context );
					}
					else if( typeName == "Float" || typeName == "F64" || typeName == "Double" ) {
						primitiveType = llvm::Type::getDoubleTy( this->context );
					}
					else if( typeName == "F32" ) {
						primitiveType = llvm::Type::getFloatTy( this->context );
					}
					else if( typeName == "Boolean" ) {
						primitiveType = llvm::Type::getInt1Ty( this->context );
					}
					else if( typeName == "String" ) {
						primitiveType = llvm::PointerType::getUnqual( this->context );
					}
					if( primitiveType ) {
						pointeeType = primitiveType;
					}
				}
				if( ( pointeeType->isIntegerTy() || pointeeType->isFloatingPointTy() ) && typeName != "String" ) {
					unwrappedSelf = this->builder.CreateLoad( pointeeType, unwrappedSelf, "builtin.self.load" );
				}
				else if( pointeeType->isPointerTy() && typeName != "String" ) {
					unwrappedSelf = this->builder.CreateLoad( pointeeType, unwrappedSelf, "builtin.self.load" );
				}
			}
			unwrappedSelf = this->unwrapOOPWrapperValue( unwrappedSelf );
			std::vector<llvm::Value*> unwrappedArguments;
			for( llvm::Value* argument : arguments ) {
				llvm::Value* unwrappedArg = this->unwrapOOPWrapperValue( argument );
				if( unwrappedArg->getType()->isPointerTy() && unwrappedSelf && unwrappedSelf->getType()->isPointerTy() == false ) {
					unwrappedArg = this->builder.CreateLoad( unwrappedSelf->getType(), unwrappedArg, "builtin.arg.load" );
				}
				unwrappedArguments.push_back( unwrappedArg );
			}
			std::vector<std::pair<std::string, llvm::Value*>> unwrappedKwargs;
			for( std::pair<std::string, llvm::Value*>& kwarg : keywordArguments ) {
				llvm::Value* unwrappedKwVal = this->unwrapOOPWrapperValue( kwarg.second );
				if( unwrappedKwVal->getType()->isPointerTy() && unwrappedSelf && unwrappedSelf->getType()->isPointerTy() == false ) {
					unwrappedKwVal = this->builder.CreateLoad( unwrappedSelf->getType(), unwrappedKwVal, "builtin.kwarg.load" );
				}
				unwrappedKwargs.push_back( { kwarg.first, unwrappedKwVal } );
			}
			return method->irGenerator( this->builder, this->context, unwrappedSelf, unwrappedArguments, unwrappedKwargs );
		}
		return nullptr;
	}
	
	bool LLVMCodegen::writeIR( const std::string& filename ) {
		std::error_code errorCode;
		llvm::raw_fd_ostream filestream( filename, errorCode, llvm::sys::fs::OF_Text );
		if( errorCode ) {
			this->diagnostic.error( std::make_shared<lookup::Source>(), fmt::format( "could not open file: \"{}\": {}", filename, errorCode.message() ) );
			return false;
		}
		this->module->print( filestream, nullptr );
		return true;
	}
	
	void LLVMCodegen::pushDeferScope() {
		this->deferScopeStack.push_back( {} );
	}
	
	void LLVMCodegen::popDeferScope() {
		if( this->deferScopeStack.empty() == false ) {
			this->deferScopeStack.pop_back();
		}
	}
	
	void LLVMCodegen::addDefer( ast::nodes::StatementSharedPointer statement ) {
		if( this->deferScopeStack.empty() == false ) {
			this->deferScopeStack.back().push_back( statement );
		}
	}
	
	void LLVMCodegen::emitCurrentScopeDefers() {
		if( this->deferScopeStack.empty() ) {
			return;
		}
		llvm::BasicBlock* currentBlock = this->builder.GetInsertBlock();
		if( currentBlock == nullptr || currentBlock->getTerminator() != nullptr ) {
			return;
		}
		std::vector<ast::nodes::StatementSharedPointer>& currentScopeDefers = this->deferScopeStack.back();
		for( std::vector<ast::nodes::StatementSharedPointer>::reverse_iterator deferIterator = currentScopeDefers.rbegin(); deferIterator != currentScopeDefers.rend(); ++deferIterator ) {
			this->generateStatement( *deferIterator );
		}
	}
	
	void LLVMCodegen::emitAllDefers() {
		llvm::BasicBlock* currentBlock = this->builder.GetInsertBlock();
		if( currentBlock == nullptr || currentBlock->getTerminator() != nullptr ) {
			return;
		}
		for( std::vector<std::vector<ast::nodes::StatementSharedPointer>>::reverse_iterator scopeIterator = this->deferScopeStack.rbegin(); scopeIterator != this->deferScopeStack.rend(); ++scopeIterator ) {
			for( std::vector<ast::nodes::StatementSharedPointer>::reverse_iterator deferIterator = scopeIterator->rbegin(); deferIterator != scopeIterator->rend(); ++deferIterator ) {
				this->generateStatement( *deferIterator );
			}
		}
	}
	
	void LLVMCodegen::pushCleanupScope() {
		this->scopeCleanupStack.push_back( {} );
	}
	
	void LLVMCodegen::popCleanupScope() {
		if( this->scopeCleanupStack.empty() ) {
			return;
		}
		std::vector<ScopeCleanupEntry>& topEntries = this->scopeCleanupStack.back();
		this->emitScopeCleanup( topEntries );
		this->scopeCleanupStack.pop_back();
	}
	
	void LLVMCodegen::emitScopeCleanup( std::vector<ScopeCleanupEntry>& entries ) {
		if( entries.empty() || this->currentFunction == nullptr ) {
			return;
		}
		llvm::BasicBlock* currentBlock = this->builder.GetInsertBlock();
		if( currentBlock == nullptr || currentBlock->getTerminator() != nullptr ) {
			return;
		}
		for( std::vector<ScopeCleanupEntry>::reverse_iterator entryIterator = entries.rbegin(); entryIterator != entries.rend(); ++entryIterator ) {
			ScopeCleanupEntry& entry = *entryIterator;
			llvm::Value* aliveValue = this->builder.CreateLoad( llvm::Type::getInt1Ty( this->context ), entry.aliveFlag, entry.variableName + ".alive.load" );
			llvm::BasicBlock* cleanupDoBlock = llvm::BasicBlock::Create( this->context, "autofree.do." + entry.variableName, this->currentFunction );
			llvm::BasicBlock* cleanupSkipBlock = llvm::BasicBlock::Create( this->context, "autofree.skip." + entry.variableName, this->currentFunction );
			this->builder.CreateCondBr( aliveValue, cleanupDoBlock, cleanupSkipBlock );
			this->builder.SetInsertPoint( cleanupDoBlock );
			llvm::Value* objectPointer = this->builder.CreateLoad( getPointeeType( entry.allocaPtr ), entry.allocaPtr, entry.variableName + ".load" );
			if( entry.implementsDroper ) {
				std::string dropFunctionName = fmt::format( "{}::drop", entry.typeName );
				std::unordered_map<std::string, llvm::Function*>::iterator dropIterator = this->functions.find( dropFunctionName );
				if( dropIterator != this->functions.end() && dropIterator->second != nullptr ) {
					llvm::Function* dropFunction = dropIterator->second;
					llvm::Value* selfPointer = objectPointer;
					if( dropFunction->arg_size() > 0 ) {
						llvm::Type* expectedType = dropFunction->arg_begin()->getType();
						if( selfPointer->getType() != expectedType ) {
							selfPointer = this->builder.CreateBitCast( selfPointer, expectedType, "drop.cast" );
						}
						this->createCallOrInvoke( dropFunction, { selfPointer } );
					}
				}
			}
			
			// Field cleanup for classes that implement Droper is handled above via drop().
			// Non-Droper classes do NOT get recursive field freeing because the autofree
			// cannot distinguish owned fields from borrowed references, causing double-free
			// when multiple objects share the same field pointer.
			
			llvm::Value* rawPointer = this->builder.CreateBitCast( objectPointer, llvm::PointerType::getUnqual( this->context ), "autofree.raw" );
			this->builder.CreateCall( this->getOrCreateFree(), { rawPointer } );
			this->builder.CreateStore( llvm::ConstantInt::getFalse( this->context ), entry.aliveFlag );
			this->builder.CreateBr( cleanupSkipBlock );
			this->builder.SetInsertPoint( cleanupSkipBlock );
		}
	}
	
	void LLVMCodegen::emitAllScopeCleanups() {
		for( std::vector<std::vector<ScopeCleanupEntry>>::reverse_iterator scopeIterator = this->scopeCleanupStack.rbegin(); scopeIterator != this->scopeCleanupStack.rend(); ++scopeIterator ) {
			this->emitScopeCleanup( *scopeIterator );
		}
	}
	
	void LLVMCodegen::registerCleanupEntry( const std::string& varName, const std::string& typeName, llvm::Value* alloca, bool implementsDroper ) {
		if( this->scopeCleanupStack.empty() ) {
			return;
		}
		llvm::AllocaInst* aliveFlag = this->createEntryBlockAllocation( this->currentFunction, varName + ".alive", llvm::Type::getInt1Ty( this->context ) );
		llvm::IRBuilder<> initBuilder( aliveFlag->getParent(), std::next( llvm::BasicBlock::iterator( aliveFlag ) ) );
		initBuilder.CreateStore( llvm::ConstantInt::getFalse( this->context ), aliveFlag );
		this->builder.CreateStore( llvm::ConstantInt::getTrue( this->context ), aliveFlag );
		ScopeCleanupEntry entry;
		entry.variableName = varName;
		entry.typeName = typeName;
		entry.allocaPtr = alloca;
		entry.aliveFlag = aliveFlag;
		entry.implementsDroper = implementsDroper;
		this->scopeCleanupStack.back().push_back( entry );
		this->aliveFlags[varName] = aliveFlag;
	}
	
	bool LLVMCodegen::writeObject( const std::string& filename ) {
		std::error_code errorCode;
		llvm::raw_fd_ostream filestream( filename, errorCode, llvm::sys::fs::OF_None );
		if( errorCode ) {
			this->diagnostic.error( std::make_shared<lookup::Source>(), fmt::format( "could not open file: \"{}\": {}", filename, errorCode.message() ) );
			return false;
		}
		this->module->print( filestream, nullptr );
		return true;
	}
	
}
