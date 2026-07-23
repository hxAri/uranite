
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

#include <unordered_set>

#include "uranite/descriptor/descriptor.hpp"
#include "uranite/semantic/analyzer.hpp"

namespace uranite::semantic {
	
	/**
	 * @brief Recursively checks if an expression or any of its sub-expressions contain a yield.
	 * 
	 * This function traverses the Abstract Syntax Tree (AST) to determine if a `YieldExpression`
	 * exists within the given expression tree. It handles nested structures such as
	 * binary operations and function call arguments.
	 * 
	 * @param expression A shared pointer to the AST node to be inspected.
	 * 
	 * @return true If a yield expression is found within the node or its children.
	 * @return false If no yield expression is found, or if the input expression is null.
	 * 
	 * @note This is a static helper function that performs a depth-first search (DFS)
	 *       on the expression tree.
	 */
	static bool containsYield( const ast::nodes::ExpressionSharedPointer& expression ) {
		if( expression == nullptr ) {
			return false;
		}
		if( expression->kind == ast::Node::Kind::YieldExpression ) {
			return true;
		}
		if( expression->kind == ast::Node::Kind::BinaryExpression ) {
			ast::nodes::BinaryExpression& binaryExpression = static_cast<ast::nodes::BinaryExpression&>( *expression );
			return containsYield( binaryExpression.left ) || containsYield( binaryExpression.right );
		}
		if( expression->kind == ast::Node::Kind::CallExpression ) {
			ast::nodes::CallExpression& callExpression = static_cast<ast::nodes::CallExpression&>( *expression );
			for( ast::nodes::ExpressionSharedPointer& argument : callExpression.arguments ) {
				if( containsYield( argument ) ) {
					return true;
				}
			}
		}
		return false;
	}
	
	/**
	 * @brief Recursively checks if a statement or any of its nested components contain a yield expression.
	 * 
	 * This function performs a deep traversal of a statement node. It checks for yields in:
	 * - Simple statements (Expressions, Returns, Variable initializers).
	 * - Control flow bodies (If, For, While blocks).
	 * 
	 * @param statement A shared pointer to the AST statement node to be inspected.
	 * 
	 * @return true If a yield is found within the statement or its nested blocks.
	 * @return false If no yield is found, or if the input statement is null.
	 * 
	 * @note This function complements @ref containsYield by handling statement-level
	 *       AST nodes and recursing into their constituent expression or statement bodies.
	 */
	static bool statementContainsYield( const ast::nodes::StatementSharedPointer& statement ) {
		if( statement != nullptr ) {
			if( statement->kind == ast::Node::Kind::ExpressionStatement ) {
				return containsYield( static_cast<ast::nodes::ExpressionStatement&>( *statement ).expression );
			}
			if( statement->kind == ast::Node::Kind::ForStatement ) {
				for( const ast::nodes::StatementSharedPointer& forBodyStatement : static_cast<ast::nodes::ForStatement&>( *statement ).body ) {
					if( statementContainsYield( forBodyStatement ) ) {
						return true;
					}
				}
			}
			if( statement->kind == ast::Node::Kind::IfStatement ) {
				ast::nodes::IfStatement& ifStatement = static_cast<ast::nodes::IfStatement&>( *statement );
				for( const ast::nodes::StatementSharedPointer& thenBodyStatement : ifStatement.thenBody ) {
					if( statementContainsYield( thenBodyStatement ) ) {
						return true;
					}
				}
				for( const ast::nodes::StatementSharedPointer& elseBodyStatement : ifStatement.elseBody ) {
					if( statementContainsYield( elseBodyStatement ) ) {
						return true;
					}
				}
			}
			if( statement->kind == ast::Node::Kind::ReturnStatement ) {
				return containsYield( static_cast<ast::nodes::ReturnStatement&>( *statement ).value );
			}
			if( statement->kind == ast::Node::Kind::VariableStatement ) {
				return containsYield( static_cast<ast::nodes::VariableStatement&>( *statement ).initializer );
			}
			if( statement->kind == ast::Node::Kind::WhileStatement ) {
				for( const ast::nodes::StatementSharedPointer& whileBodyStatement : static_cast<ast::nodes::WhileStatement&>( *statement ).body ) {
					if( statementContainsYield( whileBodyStatement ) ) {
						return true;
					}
				}
			}
		}
		return false;
	}
	
	Analyzer::Analyzer( diagnostic::Engine& diagnostic ) : diagnostic( diagnostic ) {
		this->currentScope = std::make_shared<Scope>( Scope::Kind::Global );
		this->globalScope_ = this->currentScope;
		descriptor::Builtin builtinRegistry;
		builtinRegistry.populateSemaTypes( this->typeRegistry );
	}
	
	void Analyzer::setUserImportScope( const std::string& sourceFile, const std::unordered_set<std::string>& importedIdentifiers ) {
		this->userSourceFile_ = sourceFile;
		this->userImportedIdentifiers_ = importedIdentifiers;
	}

	void Analyzer::importModuleTypes( const std::unordered_map<std::string, TypeSharedPointer>& types ) {
		for( const std::pair<const std::string, TypeSharedPointer>& entry : types ) {
			if( entry.second->kind == Type::Kind::Class ) {
				ClassTypeSharedPointer importedClass = std::static_pointer_cast<ClassType>( entry.second );
				if( importedClass->astDeclaration == nullptr ) {
					continue;
				}
				TypeSharedPointer existing = this->typeRegistry.lookupType( entry.first );
				if( existing != nullptr && existing->kind == Type::Kind::Class ) {
					ClassTypeSharedPointer existingClass = std::static_pointer_cast<ClassType>( existing );
					if( existingClass->astDeclaration == nullptr ) {
						existingClass->astDeclaration = importedClass->astDeclaration;
						existingClass->fields = importedClass->fields;
						existingClass->methods = importedClass->methods;
						existingClass->baseClass = importedClass->baseClass;
						existingClass->interfaces = importedClass->interfaces;
						existingClass->genericParameters = importedClass->genericParameters;
						existingClass->typeSubstitutions = importedClass->typeSubstitutions;
						existingClass->virtualTable = importedClass->virtualTable;
						existingClass->objectSize = importedClass->objectSize;
						existingClass->alignment = importedClass->alignment;
					}
				}
				else {
					this->typeRegistry.registerType( entry.first, entry.second );
				}
				continue;
			}
			if( entry.second->kind == Type::Kind::Interface ) {
				InterfaceTypeSharedPointer importedInterface = std::static_pointer_cast<InterfaceType>( entry.second );
				if( importedInterface->astDeclaration == nullptr ) {
					continue;
				}
				TypeSharedPointer existing = this->typeRegistry.lookupType( entry.first );
				if( existing == nullptr ) {
					this->typeRegistry.registerType( entry.first, entry.second );
				}
				continue;
			}
			if( entry.second->kind == Type::Kind::Struct ) {
				StructTypeSharedPointer importedStruct = std::static_pointer_cast<StructType>( entry.second );
				if( importedStruct->astDeclaration == nullptr ) {
					continue;
				}
				TypeSharedPointer existing = this->typeRegistry.lookupType( entry.first );
				if( existing == nullptr ) {
					this->typeRegistry.registerType( entry.first, entry.second );
				}
				continue;
			}
			if( entry.second->kind == Type::Kind::Enum ) {
				EnumTypeSharedPointer importedEnum = std::static_pointer_cast<EnumType>( entry.second );
				if( importedEnum->astDeclaration == nullptr ) {
					continue;
				}
				TypeSharedPointer existing = this->typeRegistry.lookupType( entry.first );
				if( existing == nullptr ) {
					this->typeRegistry.registerType( entry.first, entry.second );
				}
				continue;
			}
			TypeSharedPointer existing = this->typeRegistry.lookupType( entry.first );
			if( existing == nullptr ) {
				this->typeRegistry.registerType( entry.first, entry.second );
			}
		}
	}
	
	void Analyzer::importModuleSymbols( const std::unordered_map<std::string, std::vector<SymbolSharedPointer>>& symbols ) {
		for( const std::pair<const std::string, std::vector<SymbolSharedPointer>>& entry : symbols ) {
			for( const SymbolSharedPointer& symbol : entry.second ) {
				this->currentScope->define( entry.first, symbol );
			}
		}
	}
	
	std::unordered_map<std::string, TypeSharedPointer> Analyzer::getRegisteredTypes() const {
		return this->typeRegistry.getUserTypes();
	}
	
	std::unordered_map<std::string, std::vector<SymbolSharedPointer>> Analyzer::getRegisteredSymbols() const {
		return this->globalScope_->symbols();
	}
	
	bool Analyzer::analyzeModuleRegistration( ast::nodes::Program& program ) {
		if( program.module != nullptr ) {
			this->currentPackageName = program.module->name;
		}
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration != nullptr ) {
				this->registerTypeDeclaration( declaration );
			}
		}
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration == nullptr || declaration->kind != ast::Node::Kind::InterfaceDeclaration ) {
				continue;
			}
			ast::nodes::InterfaceDeclaration& interfaceDeclaration = static_cast<ast::nodes::InterfaceDeclaration&>( *declaration );
			InterfaceTypeSharedPointer interfaceType = std::dynamic_pointer_cast<InterfaceType>( this->typeRegistry.lookupType( interfaceDeclaration.name ) );
			if( interfaceType == nullptr || interfaceType->methods.empty() == false ) {
				continue;
			}
			for( ast::nodes::GenericParameterSharedPointer& genericParameter : interfaceDeclaration.genericParameters ) {
				GenericParameterTypeSharedPointer genericParameterType = std::make_shared<GenericParameterType>( genericParameter->name );
				interfaceType->genericParameters.push_back( genericParameterType );
				this->typeRegistry.registerType( genericParameter->name, genericParameterType );
			}
			for( ast::nodes::DeclarationSharedPointer& method : interfaceDeclaration.methods ) {
				if( method->kind != ast::Node::Kind::FunctionDeclaration ) {
					continue;
				}
				ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *method );
				std::vector<TypeSharedPointer> methodParameterTypes;
				std::vector<std::string> methodParameterNames;
				for( ast::nodes::FunctionParameterSharedPointer& parameter : functionDeclaration.parameters ) {
					if( parameter->isSelf ) {
						continue;
					}
					TypeSharedPointer parameterResolvedType = this->resolveType( parameter->type );
					methodParameterTypes.push_back( parameterResolvedType ? parameterResolvedType : this->typeRegistry.getError() );
					methodParameterNames.push_back( parameter->name );
				}
				TypeSharedPointer methodReturnType = functionDeclaration.returnType ? this->resolveType( functionDeclaration.returnType ) : this->typeRegistry.getVoid();
				TypeSharedPointer methodFunctionType = this->typeRegistry.makeFunction( methodParameterTypes, methodReturnType );
				std::static_pointer_cast<FunctionType>( methodFunctionType )->parameterNames = std::move( methodParameterNames );
				MethodInfo methodInformation;
				methodInformation.access = functionDeclaration.access;
				methodInformation.isFinal = false;
				methodInformation.isOverride = false;
				methodInformation.isProperty = functionDeclaration.isProperty;
				methodInformation.isStatic = functionDeclaration.isStatic;
				methodInformation.isVirtual = true;
				methodInformation.name = functionDeclaration.name;
				methodInformation.type = methodFunctionType;
				methodInformation.virtualTableIndex = -1;
				interfaceType->methods.push_back( methodInformation );
			}
		}
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration == nullptr || declaration->kind != ast::Node::Kind::InterfaceDeclaration ) {
				continue;
			}
			ast::nodes::InterfaceDeclaration& interfaceDeclaration = static_cast<ast::nodes::InterfaceDeclaration&>( *declaration );
			InterfaceTypeSharedPointer interfaceType = std::dynamic_pointer_cast<InterfaceType>( this->typeRegistry.lookupType( interfaceDeclaration.name ) );
			if( interfaceType == nullptr ) {
				continue;
			}
			this->pushScope( Scope::Kind::Class );
			for( ast::nodes::TypeNodeSharedPointer& superInterfaceNode : interfaceDeclaration.superInterfaces ) {
				TypeSharedPointer resolvedSuperInterface = this->resolveType( superInterfaceNode );
				if( resolvedSuperInterface && resolvedSuperInterface->kind == Type::Kind::Interface ) {
					interfaceType->superInterfaces.push_back( resolvedSuperInterface );
				}
			}
			this->popScope();
		}
		{
			bool hasChanges = true;
			int interfaceMethodPropagationIteration = 0;
			static constexpr int MAX_INTERFACE_PROPAGATION_ITERATIONS = 1000;
			while( hasChanges ) {
				if( interfaceMethodPropagationIteration++ >= MAX_INTERFACE_PROPAGATION_ITERATIONS ) {
					this->diagnostic.error( nullptr, "circular interface inheritance detected: method propagation did not converge" );
					break;
				}
				hasChanges = false;
				for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
					if( declaration == nullptr || declaration->kind != ast::Node::Kind::InterfaceDeclaration ) {
						continue;
					}
					InterfaceTypeSharedPointer interfaceType = std::dynamic_pointer_cast<InterfaceType>( this->typeRegistry.lookupType( static_cast<ast::nodes::InterfaceDeclaration&>( *declaration ).name ) );
					if( interfaceType == nullptr ) {
						continue;
					}
					for( TypeSharedPointer& superInterfaceType : interfaceType->superInterfaces ) {
						if( superInterfaceType == nullptr || superInterfaceType->kind != Type::Kind::Interface ) {
							continue;
						}
						InterfaceTypeSharedPointer superInterface = std::static_pointer_cast<InterfaceType>( superInterfaceType );
						for( MethodInfo& superMethod : superInterface->methods ) {
							bool methodExists = false;
							for( MethodInfo& existingMethod : interfaceType->methods ) {
								if( existingMethod.name == superMethod.name ) {
									methodExists = true;
									break;
								}
							}
							if( methodExists == false ) {
								interfaceType->methods.push_back( superMethod );
								hasChanges = true;
							}
						}
					}
				}
			}
		}
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration == nullptr || declaration->kind != ast::Node::Kind::InterfaceDeclaration ) {
				continue;
			}
			InterfaceTypeSharedPointer interfaceType = std::dynamic_pointer_cast<InterfaceType>( this->typeRegistry.lookupType( static_cast<ast::nodes::InterfaceDeclaration&>( *declaration ).name ) );
			if( interfaceType == nullptr ) {
				continue;
			}
			interfaceType->methodOrder.clear();
			for( int methodIndex = 0; methodIndex < (int)interfaceType->methods.size(); methodIndex++ ) {
				interfaceType->methods[methodIndex].interfaceTableIndex = methodIndex;
				interfaceType->methodOrder.push_back( interfaceType->methods[methodIndex].name );
			}
		}
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration == nullptr ) {
				continue;
			}
			if( declaration->kind == ast::Node::Kind::ClassDeclaration ) {
				ast::nodes::ClassDeclaration& classDeclaration = static_cast<ast::nodes::ClassDeclaration&>( *declaration );
				ClassTypeSharedPointer classType = std::dynamic_pointer_cast<ClassType>( this->typeRegistry.lookupType( classDeclaration.name ) );
				if( classType != nullptr ) {
					this->populateClassMembers( classType );
				}
			}
			else if( declaration->kind == ast::Node::Kind::StructDeclaration ) {
				ast::nodes::StructDeclaration& structDeclaration = static_cast<ast::nodes::StructDeclaration&>( *declaration );
				StructTypeSharedPointer structType = std::dynamic_pointer_cast<StructType>( this->typeRegistry.lookupType( structDeclaration.name ) );
				if( structType != nullptr ) {
					this->populateStructFields( structType );
				}
			}
			else if( declaration->kind == ast::Node::Kind::EnumDeclaration ) {
				ast::nodes::EnumDeclaration& enumDeclaration = static_cast<ast::nodes::EnumDeclaration&>( *declaration );
				EnumTypeSharedPointer enumType = std::dynamic_pointer_cast<EnumType>( this->typeRegistry.lookupType( enumDeclaration.name ) );
				if( enumType != nullptr && enumType->variants.empty() ) {
					this->pushScope( Scope::Kind::Class );
					for( TypeSharedPointer& genericParameter : enumType->genericParameters ) {
						this->typeRegistry.registerType( genericParameter->name, genericParameter );
					}
					for( ast::nodes::TypeNodeSharedPointer& interfaceNode : enumDeclaration.interfaces ) {
						TypeSharedPointer resolvedInterface = this->resolveType( interfaceNode );
						if( resolvedInterface && resolvedInterface->kind == Type::Kind::Interface ) {
							enumType->interfaces.push_back( resolvedInterface );
						}
					}
					int moduleEnumDiscriminant = 0;
					for( ast::nodes::EnumVariantSharedPointer& variant : enumDeclaration.variants ) {
						EnumVariantInfo variantInfo;
						variantInfo.name = variant->name;
						variantInfo.discriminant = moduleEnumDiscriminant++;
						enumType->variants.push_back( variantInfo );
					}
					this->popScope();
				}
			}
		}
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration == nullptr ) {
				continue;
			}
			if( declaration->kind == ast::Node::Kind::ExternDeclaration ) {
				ast::nodes::ExternDeclaration& externDeclaration = static_cast<ast::nodes::ExternDeclaration&>( *declaration );
				std::vector<TypeSharedPointer> parameterTypes;
				std::vector<std::string> parameterNames;
				for( ast::nodes::ExternParameter& parameter : externDeclaration.parameters ) {
					TypeSharedPointer parameterResolvedType = this->resolveType( parameter.type );
					parameterTypes.push_back( parameterResolvedType ? parameterResolvedType : this->typeRegistry.getError() );
					parameterNames.push_back( parameter.name );
				}
				TypeSharedPointer returnType = externDeclaration.returnType ? this->resolveType( externDeclaration.returnType ) : this->typeRegistry.getVoid();
				TypeSharedPointer functionType = this->typeRegistry.makeFunction( parameterTypes, returnType, externDeclaration.isVariadic );
				std::static_pointer_cast<FunctionType>( functionType )->parameterNames = std::move( parameterNames );
				SymbolSharedPointer functionSymbol = std::make_shared<Symbol>( externDeclaration.name, Symbol::Kind::Function, externDeclaration.source, functionType );
				functionSymbol->access = externDeclaration.access;
				functionSymbol->isInitialized = true;
				this->currentScope->define( externDeclaration.name, functionSymbol );
			}
			else if( declaration->kind == ast::Node::Kind::FunctionDeclaration ) {
				ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *declaration );
				std::vector<TypeSharedPointer> parameterTypes;
				std::vector<std::string> parameterNames;
				int regVariadicIdx = -1;
				int regKeywordIdx = -1;
				bool regSeenVariadic = false;
				TypeSharedPointer regVariadicElemType;
				TypeSharedPointer regKwargValType;
				std::vector<std::string> regKwOnlyNames;
				std::vector<TypeSharedPointer> regKwOnlyTypes;
				int regParamIdx = 0;
				for( ast::nodes::FunctionParameterSharedPointer& parameter : functionDeclaration.parameters ) {
					if( parameter->isSelf ) {
						continue;
					}
					TypeSharedPointer parameterResolvedType = this->resolveType( parameter->type );
					if( parameter->isVariadic ) {
						regSeenVariadic = true;
						TypeSharedPointer elementType = parameterResolvedType;
						if( parameterResolvedType && parameterResolvedType->kind == Type::Kind::Array ) {
							elementType = std::static_pointer_cast<ArrayType>( parameterResolvedType )->elementType;
						}
						regVariadicElemType = elementType ? elementType : this->typeRegistry.getError();
						regVariadicIdx = regParamIdx;
						parameterTypes.push_back( this->monomorphizeGenericType( "Args", { regVariadicElemType }, parameter->source ) );
					}
					else if( parameter->isKeyword ) {
						regKwargValType = parameterResolvedType ? parameterResolvedType : this->typeRegistry.getError();
						regKeywordIdx = regParamIdx;
						parameterTypes.push_back( this->monomorphizeGenericType( "Kwargs", { regKwargValType }, parameter->source ) );
					}
					else {
						parameterTypes.push_back( parameterResolvedType ? parameterResolvedType : this->typeRegistry.getError() );
						if( regSeenVariadic && parameter->defaultValue ) {
							regKwOnlyNames.push_back( parameter->name );
							regKwOnlyTypes.push_back( parameterResolvedType ? parameterResolvedType : this->typeRegistry.getError() );
						}
					}
					parameterNames.push_back( parameter->name );
					regParamIdx++;
				}
				TypeSharedPointer returnType = functionDeclaration.returnType ? this->resolveType( functionDeclaration.returnType ) : this->typeRegistry.getVoid();
				TypeSharedPointer functionType = this->typeRegistry.makeFunction( parameterTypes, returnType );
				FunctionTypeSharedPointer concreteFuncType = std::static_pointer_cast<FunctionType>( functionType );
				concreteFuncType->parameterNames = std::move( parameterNames );
				concreteFuncType->variadicParameterIndex = regVariadicIdx;
				concreteFuncType->keywordParameterIndex = regKeywordIdx;
				concreteFuncType->variadicElementType = regVariadicElemType;
				concreteFuncType->keywordValueType = regKwargValType;
				concreteFuncType->keywordOnlyParamNames = std::move( regKwOnlyNames );
				concreteFuncType->keywordOnlyParamTypes = std::move( regKwOnlyTypes );
				SymbolSharedPointer functionSymbol = std::make_shared<Symbol>( functionDeclaration.name, Symbol::Kind::Function, functionDeclaration.source, functionType );
				functionSymbol->access = functionDeclaration.access;
				functionSymbol->isInitialized = true;
				this->currentScope->define( functionDeclaration.name, functionSymbol );
			}
			else if( declaration->kind == ast::Node::Kind::ConstantDeclaration ) {
				ast::nodes::ConstantDeclaration& constDeclaration = static_cast<ast::nodes::ConstantDeclaration&>( *declaration );
				TypeSharedPointer constResolvedType = constDeclaration.type ? this->resolveType( constDeclaration.type ) : nullptr;
				if( constResolvedType ) {
					SymbolSharedPointer constSymbol = std::make_shared<Symbol>( constDeclaration.name, Symbol::Kind::Variable, constDeclaration.source, constResolvedType );
					constSymbol->access = constDeclaration.access;
					constSymbol->isInitialized = true;
					if( constDeclaration.isGlobalVariable ) {
						constSymbol->isMutable = true;
					}
					this->currentScope->define( constDeclaration.name, constSymbol );
				}
			}
		}
		return this->diagnostic.hasErrors() == false;
	}
	
	bool Analyzer::analyze( ast::nodes::Program& program ) {
		if( program.module != nullptr ) {
			this->currentPackageName = program.module->name;
		}
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration != nullptr ) {
				this->registerTypeDeclaration( declaration );
			}
		}
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration == nullptr || declaration->kind != ast::Node::Kind::InterfaceDeclaration ) {
				continue;
			}
			ast::nodes::InterfaceDeclaration& interfaceDeclaration = static_cast<ast::nodes::InterfaceDeclaration&>( *declaration );
			InterfaceTypeSharedPointer interfaceType = std::dynamic_pointer_cast<InterfaceType>( this->typeRegistry.lookupType( interfaceDeclaration.name ) );
			if( interfaceType == nullptr || interfaceType->methods.empty() == false ) {
				continue;
			}
			for( ast::nodes::GenericParameterSharedPointer& genericParameter : interfaceDeclaration.genericParameters ) {
				GenericParameterTypeSharedPointer genericParameterType = std::make_shared<GenericParameterType>( genericParameter->name );
				interfaceType->genericParameters.push_back( genericParameterType );
				this->typeRegistry.registerType( genericParameter->name, genericParameterType );
			}
			for( ast::nodes::DeclarationSharedPointer& method : interfaceDeclaration.methods ) {
				if( method->kind != ast::Node::Kind::FunctionDeclaration ) {
					continue;
				}
				ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *method );
				std::vector<TypeSharedPointer> methodParameterTypes;
				std::vector<std::string> methodParameterNames;
				for( ast::nodes::FunctionParameterSharedPointer& parameter : functionDeclaration.parameters ) {
					if( parameter->isSelf ) {
						continue;
					}
					TypeSharedPointer parameterResolvedType = this->resolveType( parameter->type );
					methodParameterTypes.push_back( parameterResolvedType ? parameterResolvedType : this->typeRegistry.getError() );
					methodParameterNames.push_back( parameter->name );
				}
				TypeSharedPointer methodReturnType = functionDeclaration.returnType ? this->resolveType( functionDeclaration.returnType ) : this->typeRegistry.getVoid();
				TypeSharedPointer methodFunctionType = this->typeRegistry.makeFunction( methodParameterTypes, methodReturnType );
				std::static_pointer_cast<FunctionType>( methodFunctionType )->parameterNames = std::move( methodParameterNames );
				MethodInfo methodInformation;
				methodInformation.access = functionDeclaration.access;
				methodInformation.isFinal = false;
				methodInformation.isOverride = false;
				methodInformation.isProperty = functionDeclaration.isProperty;
				methodInformation.isStatic = functionDeclaration.isStatic;
				methodInformation.isVirtual = true;
				methodInformation.name = functionDeclaration.name;
				methodInformation.type = methodFunctionType;
				methodInformation.virtualTableIndex = -1;
				interfaceType->methods.push_back( methodInformation );
			}
		}
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration == nullptr ) {
				continue;
			}
			if( declaration->kind == ast::Node::Kind::ClassDeclaration ) {
				ast::nodes::ClassDeclaration& classDeclaration = static_cast<ast::nodes::ClassDeclaration&>( *declaration );
				ClassTypeSharedPointer classType = std::dynamic_pointer_cast<ClassType>( this->typeRegistry.lookupType( classDeclaration.name ) );
				if( classType == nullptr ) {
					continue;
				}
				this->pushScope( Scope::Kind::Class );
				for( TypeSharedPointer& genericParameter : classType->genericParameters ) {
					this->typeRegistry.registerType( genericParameter->name, genericParameter );
				}
				for( ast::nodes::TypeNodeSharedPointer& interfaceNode : classDeclaration.interfaces ) {
					TypeSharedPointer resolvedInterface = this->resolveType( interfaceNode );
					if( resolvedInterface && resolvedInterface->kind == Type::Kind::Interface ) {
						classType->interfaces.push_back( resolvedInterface );
					}
				}
				this->popScope();
			}
			else if( declaration->kind == ast::Node::Kind::EnumDeclaration ) {
				ast::nodes::EnumDeclaration& enumDeclaration = static_cast<ast::nodes::EnumDeclaration&>( *declaration );
				EnumTypeSharedPointer enumType = std::dynamic_pointer_cast<EnumType>( this->typeRegistry.lookupType( enumDeclaration.name ) );
				if( enumType == nullptr ) {
					continue;
				}
				this->pushScope( Scope::Kind::Class );
				for( TypeSharedPointer& genericParameter : enumType->genericParameters ) {
					this->typeRegistry.registerType( genericParameter->name, genericParameter );
				}
				for( ast::nodes::TypeNodeSharedPointer& interfaceNode : enumDeclaration.interfaces ) {
					TypeSharedPointer resolvedInterface = this->resolveType( interfaceNode );
					if( resolvedInterface && resolvedInterface->kind == Type::Kind::Interface ) {
						enumType->interfaces.push_back( resolvedInterface );
					}
				}
				this->popScope();
			}
			else if( declaration->kind == ast::Node::Kind::InterfaceDeclaration ) {
				ast::nodes::InterfaceDeclaration& interfaceDeclaration = static_cast<ast::nodes::InterfaceDeclaration&>( *declaration );
				InterfaceTypeSharedPointer interfaceType = std::dynamic_pointer_cast<InterfaceType>( this->typeRegistry.lookupType( interfaceDeclaration.name ) );
				if( interfaceType == nullptr ) {
					continue;
				}
				this->pushScope( Scope::Kind::Class );
				for( ast::nodes::GenericParameterSharedPointer& genericParameter : interfaceDeclaration.genericParameters ) {
					GenericParameterTypeSharedPointer genericParameterType = std::make_shared<GenericParameterType>( genericParameter->name );
					interfaceType->genericParameters.push_back( genericParameterType );
					this->typeRegistry.registerType( genericParameter->name, genericParameterType );
				}
				for( ast::nodes::TypeNodeSharedPointer& superInterfaceNode : interfaceDeclaration.superInterfaces ) {
					TypeSharedPointer resolvedSuperInterface = this->resolveType( superInterfaceNode );
					if( resolvedSuperInterface && resolvedSuperInterface->kind == Type::Kind::Interface ) {
						interfaceType->superInterfaces.push_back( resolvedSuperInterface );
					}
				}
				this->popScope();
			}
		}
		{
			bool hasChanges = true;
			int interfaceMethodPropagationIteration = 0;
			static constexpr int MAX_INTERFACE_PROPAGATION_ITERATIONS = 1000;
			while( hasChanges ) {
				if( interfaceMethodPropagationIteration++ >= MAX_INTERFACE_PROPAGATION_ITERATIONS ) {
					this->diagnostic.error( nullptr, "circular interface inheritance detected: method propagation did not converge" );
					break;
				}
				hasChanges = false;
				for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
					if( declaration == nullptr || declaration->kind != ast::Node::Kind::InterfaceDeclaration ) {
						continue;
					}
					InterfaceTypeSharedPointer interfaceType = std::dynamic_pointer_cast<InterfaceType>( this->typeRegistry.lookupType( static_cast<ast::nodes::InterfaceDeclaration&>( *declaration ).name ) );
					if( interfaceType == nullptr ) {
						continue;
					}
					for( TypeSharedPointer& superInterfaceType : interfaceType->superInterfaces ) {
						if( superInterfaceType == nullptr || superInterfaceType->kind != Type::Kind::Interface ) {
							continue;
						}
						InterfaceTypeSharedPointer superInterface = std::static_pointer_cast<InterfaceType>( superInterfaceType );
						for( MethodInfo& superMethod : superInterface->methods ) {
							bool methodExists = false;
							for( MethodInfo& existingMethod : interfaceType->methods ) {
								if( existingMethod.name == superMethod.name ) {
									methodExists = true;
									break;
								}
							}
							if( methodExists == false ) {
								interfaceType->methods.push_back( superMethod );
								hasChanges = true;
							}
						}
					}
				}
			}
		}
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration == nullptr || declaration->kind != ast::Node::Kind::InterfaceDeclaration ) {
				continue;
			}
			InterfaceTypeSharedPointer interfaceType = std::dynamic_pointer_cast<InterfaceType>( this->typeRegistry.lookupType( static_cast<ast::nodes::InterfaceDeclaration&>( *declaration ).name ) );
			if( interfaceType == nullptr ) {
				continue;
			}
			interfaceType->methodOrder.clear();
			for( int methodIndex = 0; methodIndex < (int)interfaceType->methods.size(); methodIndex++ ) {
				interfaceType->methods[methodIndex].interfaceTableIndex = methodIndex;
				interfaceType->methodOrder.push_back( interfaceType->methods[methodIndex].name );
			}
		}
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration == nullptr ) {
				continue;
			}
			switch( declaration->kind ) {
				case ast::Node::Kind::ClassDeclaration:
				case ast::Node::Kind::EnumDeclaration:
				case ast::Node::Kind::InterfaceDeclaration:
				case ast::Node::Kind::StructDeclaration:
					break;
				case ast::Node::Kind::ExternDeclaration: {
					ast::nodes::ExternDeclaration& externDeclaration = static_cast<ast::nodes::ExternDeclaration&>( *declaration );
					std::vector<TypeSharedPointer> parameterTypes;
					std::vector<std::string> parameterNames;
					for( ast::nodes::ExternParameter& parameter : externDeclaration.parameters ) {
						TypeSharedPointer parameterResolvedType = this->resolveType( parameter.type );
						parameterTypes.push_back( parameterResolvedType ? parameterResolvedType : this->typeRegistry.getError() );
						parameterNames.push_back( parameter.name );
					}
					TypeSharedPointer returnType = externDeclaration.returnType ? this->resolveType( externDeclaration.returnType ) : this->typeRegistry.getVoid();
					TypeSharedPointer functionType = this->typeRegistry.makeFunction( parameterTypes, returnType, externDeclaration.isVariadic );
					std::static_pointer_cast<FunctionType>( functionType )->parameterNames = std::move( parameterNames );
					SymbolSharedPointer functionSymbol = std::make_shared<Symbol>( externDeclaration.name, Symbol::Kind::Function, externDeclaration.source, functionType );
					functionSymbol->access = externDeclaration.access;
					functionSymbol->isInitialized = true;
					this->currentScope->define( externDeclaration.name, functionSymbol );
					break;
				}
				case ast::Node::Kind::FunctionDeclaration: {
					ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *declaration );
					for( ast::nodes::StatementSharedPointer& bodyStatement : functionDeclaration.body ) {
						if( statementContainsYield( bodyStatement ) ) {
							functionDeclaration.isGenerator = true;
							break;
						}
					}
					std::vector<TypeSharedPointer> parameterTypes;
					std::vector<std::string> parameterNames;
					int preRegVariadicIdx = -1;
					int preRegKeywordIdx = -1;
					bool preRegSeenVariadic = false;
					TypeSharedPointer preRegVariadicElemType;
					TypeSharedPointer preRegKwargValType;
					std::vector<std::string> preRegKwOnlyNames;
					std::vector<TypeSharedPointer> preRegKwOnlyTypes;
					int preRegParamIdx = 0;
					for( ast::nodes::FunctionParameterSharedPointer& parameter : functionDeclaration.parameters ) {
						if( parameter->isSelf ) {
							continue;
						}
						TypeSharedPointer parameterResolvedType = this->resolveType( parameter->type );
						if( parameter->isVariadic ) {
							preRegSeenVariadic = true;
							TypeSharedPointer elementType = parameterResolvedType;
							if( parameterResolvedType && parameterResolvedType->kind == Type::Kind::Array ) {
								elementType = std::static_pointer_cast<ArrayType>( parameterResolvedType )->elementType;
							}
							preRegVariadicElemType = elementType ? elementType : this->typeRegistry.getError();
							preRegVariadicIdx = preRegParamIdx;
							parameterTypes.push_back( this->monomorphizeGenericType( "Args", { preRegVariadicElemType }, parameter->source ) );
						}
						else if( parameter->isKeyword ) {
							preRegKwargValType = parameterResolvedType ? parameterResolvedType : this->typeRegistry.getError();
							preRegKeywordIdx = preRegParamIdx;
							parameterTypes.push_back( this->monomorphizeGenericType( "Kwargs", { preRegKwargValType }, parameter->source ) );
						}
						else {
							parameterTypes.push_back( parameterResolvedType ? parameterResolvedType : this->typeRegistry.getError() );
							if( preRegSeenVariadic && parameter->defaultValue ) {
								preRegKwOnlyNames.push_back( parameter->name );
								preRegKwOnlyTypes.push_back( parameterResolvedType ? parameterResolvedType : this->typeRegistry.getError() );
							}
						}
						parameterNames.push_back( parameter->name );
						preRegParamIdx++;
					}
					TypeSharedPointer returnType = functionDeclaration.returnType ? this->resolveType( functionDeclaration.returnType ) : this->typeRegistry.getVoid();
					if( returnType && returnType->kind == Type::Kind::Generator ) {
						functionDeclaration.isGenerator = true;
					}
					if( functionDeclaration.isAsync && returnType->kind != Type::Kind::Future ) {
						std::string asyncErrorMessage = fmt::format( "async function \"{}\" must declare return type as 'Future<T>'", functionDeclaration.name );
						this->diagnostic.error( functionDeclaration.source, asyncErrorMessage );
					}
					if( functionDeclaration.isGenerator && returnType->kind != Type::Kind::Generator ) {
						returnType = this->typeRegistry.makeGenerator( returnType );
					}
					TypeSharedPointer functionType = this->typeRegistry.makeFunction( parameterTypes, returnType );
					FunctionTypeSharedPointer concreteFuncType = std::static_pointer_cast<FunctionType>( functionType );
					concreteFuncType->parameterNames = std::move( parameterNames );
					concreteFuncType->variadicParameterIndex = preRegVariadicIdx;
					concreteFuncType->keywordParameterIndex = preRegKeywordIdx;
					concreteFuncType->variadicElementType = preRegVariadicElemType;
					concreteFuncType->keywordValueType = preRegKwargValType;
					concreteFuncType->keywordOnlyParamNames = std::move( preRegKwOnlyNames );
					concreteFuncType->keywordOnlyParamTypes = std::move( preRegKwOnlyTypes );
					if( functionDeclaration.raisesTypes.empty() == false ) {
						for( ast::nodes::TypeNodeSharedPointer& raiseTypeNode : functionDeclaration.raisesTypes ) {
							TypeSharedPointer resolvedException = this->resolveType( raiseTypeNode );
							if( resolvedException ) {
								concreteFuncType->exceptionTypes.push_back( resolvedException );
							}
						}
					}
					SymbolSharedPointer functionSymbol = std::make_shared<Symbol>( functionDeclaration.name, Symbol::Kind::Function, functionDeclaration.source, functionType );
					functionSymbol->access = functionDeclaration.access;
					functionSymbol->isInitialized = true;
					if( this->currentScope->define( functionDeclaration.name, functionSymbol ) == false ) {
						std::string duplicateOverloadMessage = fmt::format( "function \"{}\" already has an overload with the same parameter signature", functionDeclaration.name );
						this->diagnostic.error( functionDeclaration.source, duplicateOverloadMessage );
					}
					break;
				}
				case ast::Node::Kind::TypeAliasDeclaration: {
					ast::nodes::TypeAliasDeclaration& aliasDeclaration = static_cast<ast::nodes::TypeAliasDeclaration&>( *declaration );
					TypeSharedPointer resolvedAliasType = this->resolveType( aliasDeclaration.aliasedTypeNode );
					if( resolvedAliasType ) {
						this->typeRegistry.registerType( aliasDeclaration.name, resolvedAliasType );
					}
					break;
				}
				case ast::Node::Kind::ConstantDeclaration: {
					ast::nodes::ConstantDeclaration& constDeclaration = static_cast<ast::nodes::ConstantDeclaration&>( *declaration );
					TypeSharedPointer constResolvedType = constDeclaration.type ? this->resolveType( constDeclaration.type ) : nullptr;
					if( constResolvedType ) {
						SymbolSharedPointer constSymbol = std::make_shared<Symbol>( constDeclaration.name, Symbol::Kind::Variable, constDeclaration.source, constResolvedType );
						constSymbol->access = constDeclaration.access;
						constSymbol->isInitialized = true;
						if( constDeclaration.isGlobalVariable ) {
							constSymbol->isMutable = true;
						}
						this->currentScope->define( constDeclaration.name, constSymbol );
					}
					break;
				}
				default:
					break;
			}
		}
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration && declaration->kind == ast::Node::Kind::TraitDeclaration ) {
				this->analyzeTraitDeclaration( static_cast<ast::nodes::TraitDeclaration&>( *declaration ) );
			}
		}
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration && declaration->kind == ast::Node::Kind::InterfaceDeclaration ) {
				this->analyzeInterfaceDeclaration( static_cast<ast::nodes::InterfaceDeclaration&>( *declaration ) );
			}
		}
		for( ast::nodes::DeclarationSharedPointer& declaration : program.declarations ) {
			if( declaration == nullptr || declaration->kind != ast::Node::Kind::ClassDeclaration ) {
				continue;
			}
			ast::nodes::ClassDeclaration& classDeclaration = static_cast<ast::nodes::ClassDeclaration&>( *declaration );
			ClassTypeSharedPointer classType = std::dynamic_pointer_cast<ClassType>( this->typeRegistry.lookupType( classDeclaration.name ) );
			if( classType == nullptr || classType->methods.empty() == false ) {
				continue;
			}
			this->pushScope( Scope::Kind::Class );
			this->currentScope->classType = classType;
			for( TypeSharedPointer& genericParameter : classType->genericParameters ) {
				this->typeRegistry.registerType( genericParameter->name, genericParameter );
			}
			for( ast::nodes::DeclarationSharedPointer& method : classDeclaration.methods ) {
				if( method->kind != ast::Node::Kind::FunctionDeclaration ) {
					continue;
				}
				ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *method );
				std::vector<TypeSharedPointer> methodParameterTypes;
				std::vector<std::string> methodParameterNames;
				for( ast::nodes::FunctionParameterSharedPointer& parameter : functionDeclaration.parameters ) {
					if( parameter->isSelf ) {
						continue;
					}
					TypeSharedPointer parameterResolvedType = this->resolveType( parameter->type );
					methodParameterTypes.push_back( parameterResolvedType ? parameterResolvedType : this->typeRegistry.getError() );
					methodParameterNames.push_back( parameter->name );
				}
				TypeSharedPointer methodReturnType = functionDeclaration.returnType ? this->resolveType( functionDeclaration.returnType ) : this->typeRegistry.getVoid();
				TypeSharedPointer methodFunctionType = this->typeRegistry.makeFunction( methodParameterTypes, methodReturnType );
				std::static_pointer_cast<FunctionType>( methodFunctionType )->parameterNames = std::move( methodParameterNames );
				MethodInfo methodInformation;
				methodInformation.access = functionDeclaration.access;
				methodInformation.isFinal = functionDeclaration.isFinal;
				methodInformation.isOverride = functionDeclaration.isOverride;
				methodInformation.isProperty = functionDeclaration.isProperty;
				methodInformation.isStatic = functionDeclaration.isStatic;
				methodInformation.isVirtual = functionDeclaration.isVirtual;
				methodInformation.name = functionDeclaration.name;
				methodInformation.type = methodFunctionType;
				methodInformation.virtualTableIndex = -1;
				classType->methods.push_back( methodInformation );
			}
			this->popScope();
		}
		for( size_t passIndex = 0; passIndex < program.declarations.size(); passIndex++ ) {
			ast::nodes::DeclarationSharedPointer& declaration = program.declarations[passIndex];
			if( declaration != nullptr && declaration->kind == ast::Node::Kind::StructDeclaration ) {
				this->analyzeDeclaration( declaration );
			}
		}
		for( size_t passIndex = 0; passIndex < program.declarations.size(); passIndex++ ) {
			ast::nodes::DeclarationSharedPointer& declaration = program.declarations[passIndex];
			if( declaration != nullptr && declaration->kind == ast::Node::Kind::ClassDeclaration ) {
				ast::nodes::ClassDeclaration& classDeclaration = static_cast<ast::nodes::ClassDeclaration&>( *declaration );
				if( classDeclaration.baseClassType == nullptr ) {
					this->analyzeDeclaration( declaration );
				}
			}
			else if( declaration != nullptr && declaration->kind == ast::Node::Kind::EnumDeclaration ) {
				this->analyzeDeclaration( declaration );
			}
		}
		for( size_t passIndex = 0; passIndex < program.declarations.size(); passIndex++ ) {
			ast::nodes::DeclarationSharedPointer& declaration = program.declarations[passIndex];
			if( declaration != nullptr && declaration->kind == ast::Node::Kind::ClassDeclaration ) {
				ast::nodes::ClassDeclaration& classDeclaration = static_cast<ast::nodes::ClassDeclaration&>( *declaration );
				if( classDeclaration.baseClassType != nullptr ) {
					this->analyzeDeclaration( declaration );
				}
			}
		}
		for( size_t passIndex = 0; passIndex < program.declarations.size(); passIndex++ ) {
			ast::nodes::DeclarationSharedPointer& declaration = program.declarations[passIndex];
			if( declaration == nullptr ) {
				continue;
			}
			if( declaration->kind == ast::Node::Kind::ClassDeclaration ||
				declaration->kind == ast::Node::Kind::EnumDeclaration ||
				declaration->kind == ast::Node::Kind::InterfaceDeclaration ||
				declaration->kind == ast::Node::Kind::StructDeclaration ||
				declaration->kind == ast::Node::Kind::TraitDeclaration ) {
				continue;
			}
			this->analyzeDeclaration( declaration );
		}
		return this->diagnostic.hasErrors() == false;
	}
	
	void Analyzer::analyzeAssignStatement( ast::nodes::AssignStatement& statement ) {
		TypeSharedPointer targetType = this->analyzeExpression( statement.target );
		TypeSharedPointer valueType = this->analyzeExpression( statement.value );
		if( statement.target->kind == ast::Node::Kind::IdentifierExpression ) {
			ast::nodes::IdentifierExpression& identifierExpression = static_cast<ast::nodes::IdentifierExpression&>( *statement.target );
			SymbolSharedPointer variableSymbol = this->currentScope->lookup( identifierExpression.name );
			if( variableSymbol && variableSymbol->isMutable == false ) {
				std::string immutableErrorMessage = fmt::format( "cannot assign to immutable variable \"{}\"", identifierExpression.name );
				this->diagnostic.error( statement.source, immutableErrorMessage, "declare with 'mut' to make it mutable" );
			}
			if( variableSymbol && variableSymbol->ownership->isMoved ) {
				std::string movedErrorMessage = fmt::format( "use of moved value \"{}\"", identifierExpression.name );
				this->diagnostic.error( statement.source, movedErrorMessage );
			}
		}
		if( statement.target->kind == ast::Node::Kind::MemberAccessExpression ) {
			ast::nodes::MemberAccessExpression& memberExpression = static_cast<ast::nodes::MemberAccessExpression&>( *statement.target );
			if( targetType ) {
				TypeSharedPointer objectType = this->analyzeExpression( memberExpression.object );
				if( objectType && objectType->kind == Type::Kind::Class ) {
					ClassTypeSharedPointer classTypeReference = std::static_pointer_cast<ClassType>( objectType );
					FieldInfo* fieldInformation = classTypeReference->findField( memberExpression.member );
					if( fieldInformation && fieldInformation->isReadonly ) {
						bool isInsideConstructor = false;
						if( this->currentScope->classType && this->currentScope->classType->kind == Type::Kind::Class ) {
							ScopeSharedPointer parentScope = this->currentScope->parent();
							if( parentScope && parentScope->classType == this->currentScope->classType ) {
								isInsideConstructor = true;
							}
						}
						if( isInsideConstructor == false ) {
							std::string readonlyErrorMessage = fmt::format( "cannot assign to readonly field \"{}\" of class \"{}\"", memberExpression.member, classTypeReference->name );
							this->diagnostic.error( statement.source, readonlyErrorMessage );
						}
					}
				}
			}
		}
		if( targetType && valueType && this->typeRegistry.isAssignable( targetType, valueType ) == false ) {
			std::string assignErrorMessage = fmt::format( "cannot assign value of type \"{}\" to target of type \"{}\"", valueType->toString(), targetType->toString() );
			this->diagnostic.error( statement.source, assignErrorMessage );
		}
	}
	
	TypeSharedPointer Analyzer::analyzeBinaryExpression( ast::nodes::BinaryExpression& expression ) {
		TypeSharedPointer leftSideType = this->analyzeExpression( expression.left );
		TypeSharedPointer rightSideType = this->analyzeExpression( expression.right );
		if( leftSideType == nullptr || rightSideType == nullptr ) {
			return this->typeRegistry.getError();
		}
		switch( expression.operation ) {
			case token::Type::Ampersand:
			case token::Type::Caret:
			case token::Type::Pipe:
			case token::Type::ShiftLeft:
			case token::Type::ShiftRight: {
				if( leftSideType->isIntegral() && rightSideType->isIntegral() ) {
					return leftSideType;
				}
				this->diagnostic.error( expression.source, "bitwise operators require integer operands" );
				return this->typeRegistry.getError();
			}
			case token::Type::Equal:
			case token::Type::GreaterThan:
			case token::Type::GreaterThanEqual:
			case token::Type::LessThan:
			case token::Type::LessThanEqual:
			case token::Type::NotEqual: {
				if( leftSideType->kind == Type::Kind::Class || leftSideType->kind == Type::Kind::Struct ) {
					ClassTypeSharedPointer leftSideClassType = std::dynamic_pointer_cast<ClassType>( leftSideType );
					if( leftSideClassType ) {
						static const std::unordered_map<int,std::string> comparisonMethodNames = {
							{ ( int ) token::Type::Equal, "equals" },
							{ ( int ) token::Type::GreaterThan, "greaterThan" },
							{ ( int ) token::Type::LessThan, "lessThan" },
							{ ( int ) token::Type::NotEqual, "notEquals" }
						};
						std::unordered_map<int,std::string>::const_iterator methodIterator = comparisonMethodNames.find( (int)expression.operation );
						if( methodIterator != comparisonMethodNames.end() ) {
							MethodInfo* methodInformation = leftSideClassType->findMethod( methodIterator->second );
							if( methodInformation ) {
								return this->typeRegistry.getBool();
							}
						}
					}
				}
				if( this->typeRegistry.isComparable( leftSideType, rightSideType ) == false ) {
					std::string compareErrorMessage = fmt::format( "cannot compare types \"{}\" and \"{}\"", leftSideType->toString(), rightSideType->toString() );
					this->diagnostic.error( expression.source, compareErrorMessage );
				}
				return this->typeRegistry.getBool();
			}
			case token::Type::KeywordIn: {
				if( rightSideType->kind == Type::Kind::Class || rightSideType->kind == Type::Kind::Struct ) {
					ClassTypeSharedPointer rightSideClassType = std::dynamic_pointer_cast<ClassType>( rightSideType );
					if( rightSideClassType ) {
						MethodInfo* containsMethod = rightSideClassType->findMethod( "contains" );
						if( containsMethod ) {
							return this->typeRegistry.getBool();
						}
					}
				}
				std::string inOperatorErrorMessage = fmt::format( "type \"{}\" does not support 'in' operator; implement Indexable interface with 'contains' method", rightSideType->toString() );
				this->diagnostic.error( expression.source, inOperatorErrorMessage );
				return this->typeRegistry.getBool();
			}
			case token::Type::KeywordIs: {
				return this->typeRegistry.getBool();
			}
			case token::Type::KeywordAnd:
			case token::Type::KeywordOr: {
				semantic::TypeSharedPointer boolType = this->typeRegistry.getBool();
				if( this->typeRegistry.isAssignable( boolType, leftSideType ) == false || this->typeRegistry.isAssignable( boolType, rightSideType ) == false ) {
					this->diagnostic.error( expression.source, "logical operators require boolean operands" );
				}
				return this->typeRegistry.getBool();
			}
			case token::Type::Minus:
			case token::Type::Percent:
			case token::Type::Plus:
			case token::Type::Power:
			case token::Type::Slash:
			case token::Type::Star: {
				static const std::set<std::string> numericOopTypes = {
					"Int", "I8", "I16", "I32", "I64",
					"UInt", "U8", "U16", "U32", "U64",
					"Float", "F32", "F64", "Double",
					"Integer", "Long", "Byte"
				};
				static const std::set<std::string> floatOopTypes = {
					"Float", "F32", "F64", "Double"
				};
				bool leftIsNumeric = leftSideType->isNumeric() || numericOopTypes.count( leftSideType->name ) > 0;
				bool rightIsNumeric = rightSideType->isNumeric() || numericOopTypes.count( rightSideType->name ) > 0;
				if( leftIsNumeric && rightIsNumeric ) {
					bool leftIsFloat = leftSideType->isFloatingPoint() || floatOopTypes.count( leftSideType->name ) > 0;
					bool rightIsFloat = rightSideType->isFloatingPoint() || floatOopTypes.count( rightSideType->name ) > 0;
					if( leftIsFloat || rightIsFloat ) {
						return this->typeRegistry.getFloat64();
					}
					return this->typeRegistry.getInteger64();
				}
				if( expression.operation == token::Type::Plus && ( leftSideType->kind == Type::Kind::String || leftSideType->qualified == qname::STRING ) ) {
					return leftSideType;
				}
				if( leftSideType->kind == Type::Kind::Class || leftSideType->kind == Type::Kind::Struct ) {
					ClassTypeSharedPointer leftSideClassType = std::dynamic_pointer_cast<ClassType>( leftSideType );
					if( leftSideClassType ) {
						static const std::unordered_map<int,std::string> operatorMethodNames = {
							{ ( int ) token::Type::Percent, "modulo" },
							{ ( int ) token::Type::Star, "multiply" },
							{ ( int ) token::Type::Slash, "divide" },
							{ ( int ) token::Type::Plus, "add" },
							{ ( int ) token::Type::Minus, "subtract" }
						};
						std::unordered_map<int,std::string>::const_iterator methodIterator = operatorMethodNames.find( (int)expression.operation );
						if( methodIterator != operatorMethodNames.end() ) {
							MethodInfo* methodInformation = leftSideClassType->findMethod( methodIterator->second );
							if( methodInformation ) {
								return leftSideType;
							}
						}
					}
				}
				std::string operationString = token::toString( expression.operation );
				std::string leftTypeString = leftSideType->toString();
				std::string rightTypeString = rightSideType->toString();
				std::string binaryErrorMessage = fmt::format( "invalid operands to binary \"{}\": \"{}\" and \"{}\"", operationString, leftTypeString, rightTypeString );
				this->diagnostic.error( expression.source, binaryErrorMessage );
				return this->typeRegistry.getError();
			}
			default: {
				return this->typeRegistry.getError();
			}
		}
	}
	
	TypeSharedPointer Analyzer::analyzeCallExpression( ast::nodes::CallExpression& expression ) {
		TypeSharedPointer calleeType = this->analyzeExpression( expression.callee );
		if( calleeType == nullptr || calleeType->isError() ) {
			return this->typeRegistry.getError();
		}
		if( calleeType->kind == Type::Kind::Callable ) {
			CallableTypeSharedPointer callableType = std::static_pointer_cast<CallableType>(calleeType);
			for( size_t index = 0; index < expression.arguments.size(); index++ ) {
				TypeSharedPointer argumentType = this->analyzeExpression( expression.arguments[index] );
				if( index < callableType->parameterTypes.size() && argumentType && this->typeRegistry.isAssignable( callableType->parameterTypes[index], argumentType ) == false ) {
					std::string expectedTypeName = callableType->parameterTypes[index]->toString();
					std::string foundTypeName = argumentType->toString();
					std::string argumentMismatchErrorMessage = fmt::format( "argument type mismatch: expected \"{}\" but found \"{}\"", expectedTypeName, foundTypeName );
					this->diagnostic.error( expression.arguments[index]->source, argumentMismatchErrorMessage );
				}
			}
			return callableType->returnType;
		}
		if( calleeType->kind != Type::Kind::Function ) {
			if( calleeType->kind == Type::Kind::Class || calleeType->kind == Type::Kind::Struct ) {
				for( ast::nodes::ExpressionSharedPointer& argument : expression.arguments ) {
					this->analyzeExpression( argument );
				}
				return calleeType;
			}
			this->diagnostic.error( expression.source, "expression is not callable" );
			return this->typeRegistry.getError();
		}
		FunctionTypeSharedPointer functionType = std::static_pointer_cast<FunctionType>( calleeType );
		if( expression.callee->kind == ast::Node::Kind::IdentifierExpression ) {
			ast::nodes::IdentifierExpression& identifierCallee = static_cast<ast::nodes::IdentifierExpression&>( *expression.callee );
			std::vector<SymbolSharedPointer> overloadCandidates = this->currentScope->lookupAll( identifierCallee.name );
			if( overloadCandidates.size() > 1 ) {
				std::vector<TypeSharedPointer> argumentTypes;
				for( ast::nodes::ExpressionSharedPointer& argument : expression.arguments ) {
					argumentTypes.push_back( this->analyzeExpression( argument ) );
				}
				size_t argumentCount = argumentTypes.size();
				int bestScore = -1;
				FunctionTypeSharedPointer bestFunctionType = nullptr;
				SymbolSharedPointer bestSymbol = nullptr;
				bool bestIsExactArity = false;
				bool ambiguous = false;
				for( const SymbolSharedPointer& candidate : overloadCandidates ) {
					if( candidate->kind != Symbol::Kind::Function || candidate->typeref == nullptr || candidate->typeref->kind != Type::Kind::Function ) {
						continue;
					}
					FunctionTypeSharedPointer candidateFunction = std::static_pointer_cast<FunctionType>( candidate->typeref );
					size_t candidateFixedParamCount;
					if( candidateFunction->variadicParameterIndex >= 0 ) {
						candidateFixedParamCount = static_cast<size_t>( candidateFunction->variadicParameterIndex );
					}
					else if( candidateFunction->keywordParameterIndex >= 0 ) {
						candidateFixedParamCount = static_cast<size_t>( candidateFunction->keywordParameterIndex );
					}
					else {
						candidateFixedParamCount = candidateFunction->parameterTypes.size();
					}
					if( argumentCount < candidateFixedParamCount ) {
						continue;
					}
					if( argumentCount > candidateFixedParamCount && candidateFunction->variadicParameterIndex < 0 && candidateFunction->isVariadic == false ) {
						continue;
					}
					int score = 0;
					bool compatible = true;
					for( size_t paramIndex = 0; paramIndex < candidateFixedParamCount && paramIndex < argumentCount; paramIndex++ ) {
						if( argumentTypes[paramIndex] == nullptr || argumentTypes[paramIndex]->isError() ) {
							continue;
						}
						if( candidateFunction->parameterTypes[paramIndex]->toString() == argumentTypes[paramIndex]->toString() ) {
							score += 2;
						}
						else if( this->typeRegistry.isAssignable( candidateFunction->parameterTypes[paramIndex], argumentTypes[paramIndex] ) ) {
							score += 1;
						}
						else {
							compatible = false;
							break;
						}
					}
					if( compatible == false ) {
						continue;
					}
					bool candidateIsExactArity = ( candidateFunction->variadicParameterIndex < 0 && candidateFunction->isVariadic == false && argumentCount == candidateFunction->parameterTypes.size() );
					if( score > bestScore ) {
						bestScore = score;
						bestFunctionType = candidateFunction;
						bestSymbol = candidate;
						bestIsExactArity = candidateIsExactArity;
						ambiguous = false;
					}
					else if( score == bestScore && bestFunctionType != nullptr ) {
						if( candidateIsExactArity && bestIsExactArity == false ) {
							bestFunctionType = candidateFunction;
							bestSymbol = candidate;
							bestIsExactArity = candidateIsExactArity;
							ambiguous = false;
						}
						else if( candidateIsExactArity == false && bestIsExactArity ) {
						}
						else {
							ambiguous = true;
						}
					}
				}
				if( ambiguous ) {
					std::string ambiguousOverloadMessage = fmt::format( "ambiguous call to overloaded function \"{}\"", identifierCallee.name );
					this->diagnostic.error( expression.source, ambiguousOverloadMessage );
				}
				if( bestFunctionType != nullptr ) {
					functionType = bestFunctionType;
					calleeType = bestFunctionType;
					identifierCallee.resolvedSymbol = bestSymbol;
				}
			}
		}
		size_t fixedParamCount;
		if( functionType->variadicParameterIndex >= 0 ) {
			fixedParamCount = static_cast<size_t>( functionType->variadicParameterIndex );
		}
		else if( functionType->keywordParameterIndex >= 0 ) {
			fixedParamCount = static_cast<size_t>( functionType->keywordParameterIndex );
		}
		else {
			fixedParamCount = functionType->parameterTypes.size();
		}
		size_t positionalArgCount = expression.arguments.size();
		if( positionalArgCount < fixedParamCount ) {
			std::string expectedCount = std::to_string( fixedParamCount );
			std::string actualCount = std::to_string( positionalArgCount );
			std::string argumentUnderflowErrorMessage = fmt::format( "expected at least {} arguments but got {}", expectedCount, actualCount );
			this->diagnostic.error( expression.source, argumentUnderflowErrorMessage );
		}
		else if( positionalArgCount > fixedParamCount && functionType->variadicParameterIndex < 0 && functionType->isVariadic == false ) {
			std::string expectedCount = std::to_string( fixedParamCount );
			std::string actualCount = std::to_string( positionalArgCount );
			std::string argumentOverflowErrorMessage = fmt::format( "expected {} arguments but got {}", expectedCount, actualCount );
			this->diagnostic.error( expression.source, argumentOverflowErrorMessage );
		}
		if( expression.keywordArguments.empty() == false && functionType->keywordParameterIndex < 0 && functionType->keywordOnlyParamNames.empty() ) {
			this->diagnostic.error( expression.source, "function does not accept keyword arguments" );
		}
		for( size_t index = 0; index < fixedParamCount && index < positionalArgCount; index++ ) {
			TypeSharedPointer argumentType = this->analyzeExpression( expression.arguments[index] );
			if( argumentType && this->typeRegistry.isAssignable( functionType->parameterTypes[index], argumentType ) == false ) {
				std::string expectedTypeName = functionType->parameterTypes[index]->toString();
				std::string foundTypeName = argumentType->toString();
				std::string functionArgumentMismatchErrorMessage = fmt::format( "argument type mismatch: expected \"{}\" but found \"{}\"", expectedTypeName, foundTypeName );
				this->diagnostic.error( expression.arguments[index]->source, functionArgumentMismatchErrorMessage );
			}
		}
		if( functionType->variadicParameterIndex >= 0 && functionType->variadicElementType ) {
			size_t keywordOnlyCount = functionType->keywordOnlyParamNames.size();
			size_t variadicEndIndex = positionalArgCount;
			if( keywordOnlyCount > 0 && positionalArgCount > fixedParamCount ) {
				bool hasVariadicForward = false;
				size_t firstExtraArgIndex = fixedParamCount;
				if( firstExtraArgIndex < expression.arguments.size() ) {
					TypeSharedPointer firstExtraArgType = this->analyzeExpression( expression.arguments[firstExtraArgIndex] );
					if( firstExtraArgType && firstExtraArgType->name.find( "Args<" ) == 0 ) {
						hasVariadicForward = true;
					}
				}
				if( hasVariadicForward ) {
					variadicEndIndex = fixedParamCount + 1;
				}
			}
			for( size_t index = fixedParamCount; index < variadicEndIndex; index++ ) {
				TypeSharedPointer argumentType = this->analyzeExpression( expression.arguments[index] );
				if( argumentType && argumentType->name.find( "Args<" ) == 0 ) {
					continue;
				}
				if( argumentType && this->typeRegistry.isAssignable( functionType->variadicElementType, argumentType ) == false ) {
					std::string expectedTypeName = functionType->variadicElementType->toString();
					std::string foundTypeName = argumentType->toString();
					std::string variadicMismatchErrorMessage = fmt::format( "variadic argument type mismatch: expected \"{}\" but found \"{}\"", expectedTypeName, foundTypeName );
					this->diagnostic.error( expression.arguments[index]->source, variadicMismatchErrorMessage );
				}
			}
			for( size_t index = variadicEndIndex; index < positionalArgCount; index++ ) {
				size_t kwParamIdx = index - variadicEndIndex;
				TypeSharedPointer argumentType = this->analyzeExpression( expression.arguments[index] );
				if( kwParamIdx < keywordOnlyCount && argumentType && functionType->keywordOnlyParamTypes[kwParamIdx] ) {
					if( this->typeRegistry.isAssignable( functionType->keywordOnlyParamTypes[kwParamIdx], argumentType ) == false ) {
						std::string expectedTypeName = functionType->keywordOnlyParamTypes[kwParamIdx]->toString();
						std::string foundTypeName = argumentType->toString();
						std::string kwPositionalMismatchMessage = fmt::format( "argument type mismatch for \"{}\": expected \"{}\" but found \"{}\"", functionType->keywordOnlyParamNames[kwParamIdx], expectedTypeName, foundTypeName );
						this->diagnostic.error( expression.arguments[index]->source, kwPositionalMismatchMessage );
					}
				}
			}
		}
		else {
			for( size_t index = fixedParamCount; index < positionalArgCount; index++ ) {
				this->analyzeExpression( expression.arguments[index] );
			}
		}
		if( functionType->keywordParameterIndex >= 0 && functionType->keywordValueType ) {
			std::unordered_set<std::string> seenKeywords;
			for( ast::nodes::KeywordArgument& kwarg : expression.keywordArguments ) {
				TypeSharedPointer valueType = this->analyzeExpression( kwarg.value );
				if( valueType && this->typeRegistry.isAssignable( functionType->keywordValueType, valueType ) == false ) {
					std::string expectedTypeName = functionType->keywordValueType->toString();
					std::string foundTypeName = valueType->toString();
					std::string kwargMismatchErrorMessage = fmt::format( "keyword argument \"{}\" type mismatch: expected \"{}\" but found \"{}\"", kwarg.name, expectedTypeName, foundTypeName );
					this->diagnostic.error( kwarg.source, kwargMismatchErrorMessage );
				}
				if( seenKeywords.count( kwarg.name ) ) {
					std::string duplicateKwargMessage = fmt::format( "duplicate keyword argument \"{}\"", kwarg.name );
					this->diagnostic.error( kwarg.source, duplicateKwargMessage );
				}
				seenKeywords.insert( kwarg.name );
			}
		}
		else if( functionType->keywordOnlyParamNames.empty() == false ) {
			std::unordered_set<std::string> seenKeywords;
			for( ast::nodes::KeywordArgument& kwarg : expression.keywordArguments ) {
				TypeSharedPointer valueType = this->analyzeExpression( kwarg.value );
				bool found = false;
				for( size_t kwIdx = 0; kwIdx < functionType->keywordOnlyParamNames.size(); kwIdx++ ) {
					if( functionType->keywordOnlyParamNames[kwIdx] == kwarg.name ) {
						found = true;
						if( valueType && functionType->keywordOnlyParamTypes[kwIdx] && this->typeRegistry.isAssignable( functionType->keywordOnlyParamTypes[kwIdx], valueType ) == false ) {
							std::string expectedTypeName = functionType->keywordOnlyParamTypes[kwIdx]->toString();
							std::string foundTypeName = valueType->toString();
							std::string kwargMismatchMessage = fmt::format( "keyword argument \"{}\" type mismatch: expected \"{}\" but found \"{}\"", kwarg.name, expectedTypeName, foundTypeName );
							this->diagnostic.error( kwarg.source, kwargMismatchMessage );
						}
						break;
					}
				}
				if( found == false ) {
					std::string unknownKwargMessage = fmt::format( "unknown keyword argument \"{}\"", kwarg.name );
					this->diagnostic.error( kwarg.source, unknownKwargMessage );
				}
				if( seenKeywords.count( kwarg.name ) ) {
					std::string duplicateMessage = fmt::format( "duplicate keyword argument \"{}\"", kwarg.name );
					this->diagnostic.error( kwarg.source, duplicateMessage );
				}
				seenKeywords.insert( kwarg.name );
			}
		}
		else {
			for( ast::nodes::KeywordArgument& kwarg : expression.keywordArguments ) {
				this->analyzeExpression( kwarg.value );
			}
		}
		if( functionType->exceptionTypes.empty() == false && this->isInsideTryBlock == false ) {
			std::string raisedTypes;
			for( size_t index = 0; index < functionType->exceptionTypes.size(); index++ ) {
				if( index > 0 ) {
					std::string raisedTypesList = fmt::format( "{}, ", raisedTypes );
					raisedTypes = fmt::format( "{}{}", raisedTypesList, functionType->exceptionTypes[index]->name );
				}
				else {
					raisedTypes = functionType->exceptionTypes[index]->name;
				}
			}
			bool callerRaises = false;
			for( TypeSharedPointer& raisedType : this->currentExceptionTypes ) {
				for( TypeSharedPointer& functionRaisedType : functionType->exceptionTypes ) {
					if( raisedType->equals( functionRaisedType ) ) {
						callerRaises = true;
						break;
					}
				}
				if( callerRaises ) {
					break;
				}
			}
			if( callerRaises == false ) {
				std::string exceptionSafetyErrorMessage = fmt::format( "call to function that raises \"{}\" must be wrapped in try/except or caller must declare 'raises'", raisedTypes );
				this->diagnostic.error( expression.source, exceptionSafetyErrorMessage );
			}
		}
		return functionType->returnType;
	}
	
	void Analyzer::analyzeClassDeclaration( ast::nodes::ClassDeclaration& declaration ) {
		ClassTypeSharedPointer classType = std::dynamic_pointer_cast<ClassType>( this->typeRegistry.lookupType( declaration.name ) );
		if( classType == nullptr ) {
			return;
		}
		classType->isAbstract = declaration.isAbstract;
		classType->isFinal = declaration.isFinal;
		classType->isReadonly = declaration.isReadonly;
		
		// Register generic type parameters early so they resolve in base/interface types
		// Generic parameters were pre-registered in this->registerTypeDeclaration; rebuild with constraints
		classType->genericParameters.clear();
		for( ast::nodes::GenericParameterSharedPointer& genericParameter : declaration.genericParameters ) {
			GenericParameterTypeSharedPointer genericParameterType = std::make_shared<GenericParameterType>( genericParameter->name );
			for( ast::nodes::TypeNodeSharedPointer& constraint : genericParameter->constraints ) {
				TypeSharedPointer resolvedConstraintType = this->resolveType( constraint );
				if( resolvedConstraintType && resolvedConstraintType->isError() == false ) {
					if( resolvedConstraintType->kind != Type::Kind::Interface && resolvedConstraintType->kind != Type::Kind::Trait ) {
						std::string constraintWarningMessage = fmt::format( "generic constraint \"{}\" is not an interface or trait", resolvedConstraintType->name );
						this->diagnostic.warning( declaration.source, constraintWarningMessage );
					}
					genericParameterType->constraints.push_back( resolvedConstraintType );
				}
			}
			classType->genericParameters.push_back( genericParameterType );
			this->typeRegistry.registerType( genericParameter->name, genericParameterType );
		}
		if( declaration.baseClassType ) {
			TypeSharedPointer resolvedBaseType = this->resolveType( declaration.baseClassType );
			if( resolvedBaseType ) {
				if( resolvedBaseType->kind == Type::Kind::Interface ) {
					classType->interfaces.push_back( resolvedBaseType );
				}
				else if( resolvedBaseType->kind == Type::Kind::Class ) {
					classType->baseClass = resolvedBaseType;
					ClassTypeSharedPointer baseClassType = std::static_pointer_cast<ClassType>( resolvedBaseType );
					if( baseClassType->isFinal ) {
						std::string finalClassErrorMessage = fmt::format( "cannot extend final class \"{}\"", baseClassType->name );
						this->diagnostic.error( declaration.source, finalClassErrorMessage );
					}
				}
				else {
					std::string invalidBaseTypeErrorMessage = fmt::format( "base type \"{}\" is not a class or interface", resolvedBaseType->name );
					this->diagnostic.error( declaration.source, invalidBaseTypeErrorMessage );
				}
			}
		}
		for( ast::nodes::TypeNodeSharedPointer& interfaceNode : declaration.interfaces ) {
			TypeSharedPointer interfaceType = this->resolveType( interfaceNode );
			if( interfaceType ) {
				if( interfaceType->kind != Type::Kind::Interface ) {
					std::string invalidInterfaceErrorMessage = fmt::format( "\"{}\" is not an interface", interfaceType->name );
					this->diagnostic.error( declaration.source, invalidInterfaceErrorMessage );
				}
				else {
					classType->interfaces.push_back( interfaceType );
				}
			}
		}
		for( std::string& traitName : declaration.usedTraits ) {
			TraitTypeSharedPointer traitType = std::dynamic_pointer_cast<TraitType>( this->typeRegistry.lookupType( traitName ) );
			if( traitType == nullptr ) {
				std::string unknownTraitErrorMessage = fmt::format( "unknown trait \"{}\"", traitName );
				this->diagnostic.error( declaration.source, unknownTraitErrorMessage );
				continue;
			}
			if( traitType->astDeclaration ) {
				for( ast::nodes::FieldDeclarationSharedPointer& traitField : traitType->astDeclaration->fields ) {
					bool fieldExists = false;
					for( ast::nodes::FieldDeclarationSharedPointer& classField : declaration.fields ) {
						if( classField->name == traitField->name ) {
							fieldExists = true;
							break;
						}
					}
					if( fieldExists == false ) {
						declaration.fields.push_back( traitField );
					}
				}
				for( ast::nodes::DeclarationSharedPointer& traitMethod : traitType->astDeclaration->methods ) {
					ast::nodes::FunctionDeclaration& traitFunction = static_cast<ast::nodes::FunctionDeclaration&>( *traitMethod );
					bool methodExists = false;
					for( ast::nodes::DeclarationSharedPointer& classMethod : declaration.methods ) {
						if( classMethod->kind == ast::Node::Kind::FunctionDeclaration &&
							static_cast<ast::nodes::FunctionDeclaration&>( *classMethod ).name == traitFunction.name ) {
							methodExists = true;
							break;
						}
					}
					if( methodExists == false ) {
						declaration.methods.push_back( traitMethod );
					}
				}
			}
		}
		this->pushScope( Scope::Kind::Class );
		this->currentScope->classType = classType;
		classType->fields.clear();
		int fieldIndex = 0;
		if( classType->baseClass && classType->baseClass->kind == Type::Kind::Class ) {
			std::vector<ClassTypeSharedPointer> ancestors;
			ClassTypeSharedPointer baseTracker = std::static_pointer_cast<ClassType>( classType->baseClass );
			while( baseTracker ) {
				ancestors.push_back( baseTracker );
				if( baseTracker->baseClass && baseTracker->baseClass->kind == Type::Kind::Class ) {
					baseTracker = std::static_pointer_cast<ClassType>( baseTracker->baseClass );
				}
				else {
					break;
				}
			}
			std::unordered_set<std::string> addedFields;
			for( std::vector<ClassTypeSharedPointer>::reverse_iterator ancestorIterator = ancestors.rbegin(); ancestorIterator != ancestors.rend(); ++ancestorIterator ) {
				for( FieldInfo& inheritedField : ( *ancestorIterator )->fields ) {
					if( addedFields.insert( inheritedField.name ).second ) {
						classType->fields.push_back( inheritedField );
						fieldIndex++;
					}
				}
			}
		}
		for( std::shared_ptr<ast::nodes::FieldDeclarationNode>& fieldDeclaration : declaration.fields ) {
			if( declaration.isBuiltin == false && fieldDeclaration->access == ast::AccessModifier::Default ) {
				std::string visibilityErrorMessage = fmt::format( "field \"{}\" in class \"{}\" must have an explicit visibility modifier (public, private, or protect)", fieldDeclaration->name, declaration.name );
				this->diagnostic.error( fieldDeclaration->source, visibilityErrorMessage );
			}
			for( FieldInfo& existingField : classType->fields ) {
				if( existingField.name == fieldDeclaration->name ) {
					std::string shadowWarningMessage = fmt::format( "field \"{}\" in class \"{}\" shadows inherited field", fieldDeclaration->name, declaration.name );
					this->diagnostic.warning( fieldDeclaration->source, shadowWarningMessage );
					break;
				}
			}
			TypeSharedPointer fieldType = this->resolveType( fieldDeclaration->type );
			FieldInfo fieldInformation;
			fieldInformation.name = fieldDeclaration->name;
			fieldInformation.type = fieldType ? fieldType : this->typeRegistry.getError();
			fieldInformation.access = fieldDeclaration->access;
			fieldInformation.isStatic = fieldDeclaration->isStatic;
			if( fieldDeclaration->isStatic == false ) {
				fieldInformation.index = fieldIndex++;
			}
			else {
				fieldInformation.index = -1;
			}
			classType->fields.push_back( fieldInformation );
			SymbolSharedPointer fieldSymbol = std::make_shared<Symbol>( fieldDeclaration->name, Symbol::Kind::Field, fieldDeclaration->source, fieldInformation.type );
			fieldSymbol->access = fieldDeclaration->access;
			this->currentScope->define( fieldDeclaration->name, fieldSymbol );
		}
		for( ast::nodes::DeclarationSharedPointer& constructorMethod : declaration.methods ) {
			if( constructorMethod->kind == ast::Node::Kind::FunctionDeclaration ) {
				ast::nodes::FunctionDeclaration& constructorFunction = static_cast<ast::nodes::FunctionDeclaration&>( *constructorMethod );
				if( constructorFunction.name == declaration.name ) {
					for( ast::nodes::FunctionParameterSharedPointer& propertyParameter : constructorFunction.parameters ) {
						if( propertyParameter->isProperty && propertyParameter->type ) {
							TypeSharedPointer propertyType = this->resolveType( propertyParameter->type );
							FieldInfo propertyFieldInformation;
							propertyFieldInformation.name = propertyParameter->name;
							propertyFieldInformation.type = propertyType ? propertyType : this->typeRegistry.getError();
							propertyFieldInformation.access = propertyParameter->propertyAccess;
							propertyFieldInformation.index = fieldIndex++;
							classType->fields.push_back( propertyFieldInformation );
							SymbolSharedPointer propertySymbol = std::make_shared<Symbol>( propertyParameter->name, Symbol::Kind::Field, propertyParameter->source, propertyFieldInformation.type );
							propertySymbol->access = propertyParameter->propertyAccess;
							this->currentScope->define( propertyParameter->name, propertySymbol );
						}
					}
				}
			}
		}
		for( ast::nodes::DeclarationSharedPointer& methodDeclaration : declaration.methods ) {
			if( declaration.isBuiltin == false && methodDeclaration->access == ast::AccessModifier::Default ) {
				if( methodDeclaration->kind == ast::Node::Kind::FunctionDeclaration ) {
					ast::nodes::FunctionDeclaration& functionMethod = static_cast<ast::nodes::FunctionDeclaration&>( *methodDeclaration );
					std::string methodVisibilityErrorMessage = fmt::format( "method \"{}\" in class \"{}\" must have an explicit visibility modifier (public, private, or protect)", functionMethod.name, declaration.name );
					this->diagnostic.error( methodDeclaration->source, methodVisibilityErrorMessage );
				}
			}
			this->analyzeDeclaration( methodDeclaration );
			if( methodDeclaration->kind == ast::Node::Kind::FunctionDeclaration ) {
				ast::nodes::FunctionDeclaration& functionMethod = static_cast<ast::nodes::FunctionDeclaration&>( *methodDeclaration );
				if( functionMethod.isStatic == false ) {
					bool hasSelf = false;
					for( ast::nodes::FunctionParameterSharedPointer& parameter : functionMethod.parameters ) {
						if( parameter->isSelf ) { hasSelf = true; break; }
					}
					if( hasSelf == false ) {
						std::string selfErrorMessage = fmt::format( "non-static method \"{}\" in class \"{}\" must have 'self' as first parameter, or be declared 'static'", functionMethod.name, declaration.name );
						this->diagnostic.error( functionMethod.source, selfErrorMessage );
					}
				}
				else {
					for( ast::nodes::FunctionParameterSharedPointer& parameter : functionMethod.parameters ) {
						if( parameter->isSelf ) {
							std::string selfErrorMessage = fmt::format( "static method \"{}\" in class \"{}\" must not have 'self' parameter", functionMethod.name, declaration.name );
							this->diagnostic.error( parameter->source, selfErrorMessage );
							break;
						}
					}
				}
				std::vector<TypeSharedPointer> methodParameterTypes;
				std::vector<std::string> methodParameterNames;
				for( ast::nodes::FunctionParameterSharedPointer& parameter : functionMethod.parameters ) {
					if( parameter->isSelf ) {
						continue;
					}
					TypeSharedPointer parameterType = this->resolveType( parameter->type );
					methodParameterTypes.push_back( parameterType ? parameterType : this->typeRegistry.getError() );
					methodParameterNames.push_back( parameter->name );
				}
				TypeSharedPointer methodReturnType = functionMethod.returnType ? this->resolveType( functionMethod.returnType ) : this->typeRegistry.getVoid();
				TypeSharedPointer methodFunctionType = this->typeRegistry.makeFunction( methodParameterTypes, methodReturnType );
				std::static_pointer_cast<FunctionType>( methodFunctionType )->parameterNames = std::move( methodParameterNames );
				MethodInfo methodInformation;
				methodInformation.name = functionMethod.name;
				methodInformation.type = methodFunctionType;
				methodInformation.access = functionMethod.access;
				methodInformation.isVirtual = functionMethod.isVirtual;
				methodInformation.isOverride = functionMethod.isOverride;
				methodInformation.isStatic = functionMethod.isStatic;
				methodInformation.isFinal = functionMethod.isFinal;
				methodInformation.isProperty = functionMethod.isProperty;
				methodInformation.virtualTableIndex = -1;
				if( functionMethod.isOverride && classType->baseClass && classType->baseClass->kind == Type::Kind::Class ) {
					ClassTypeSharedPointer baseClassType = std::static_pointer_cast<ClassType>( classType->baseClass );
					MethodInfo* baseMethodInformation = baseClassType->findMethod( functionMethod.name );
					if( baseMethodInformation && baseMethodInformation->isFinal ) {
						std::string finalMethodErrorMessage = fmt::format( "cannot override final method \"{}\" from class \"{}\"", functionMethod.name, baseClassType->name );
						this->diagnostic.error( functionMethod.source, finalMethodErrorMessage );
					}
				}
				FunctionTypeSharedPointer newFunctionType = std::dynamic_pointer_cast<FunctionType>( methodInformation.type );
				MethodInfo* existingMethodInformation = nullptr;
				for( MethodInfo& existingMethod : classType->methods ) {
					if( existingMethod.name != functionMethod.name ) {
						continue;
					}
					FunctionTypeSharedPointer existingFunctionType = std::dynamic_pointer_cast<FunctionType>( existingMethod.type );
					if( newFunctionType == nullptr || existingFunctionType == nullptr ) {
						existingMethodInformation = &existingMethod;
						break;
					}
					if( newFunctionType->parameterTypes.size() != existingFunctionType->parameterTypes.size() ) {
						continue;
					}
					bool paramTypesMatch = true;
					for( size_t paramIndex = 0; paramIndex < newFunctionType->parameterTypes.size(); paramIndex++ ) {
						if( newFunctionType->parameterTypes[paramIndex] && existingFunctionType->parameterTypes[paramIndex] &&
							newFunctionType->parameterTypes[paramIndex]->toString() != existingFunctionType->parameterTypes[paramIndex]->toString() ) {
							paramTypesMatch = false;
							break;
						}
					}
					if( paramTypesMatch ) {
						existingMethodInformation = &existingMethod;
						break;
					}
				}
				if( existingMethodInformation ) {
					*existingMethodInformation = methodInformation;
				}
				else {
					classType->methods.push_back( methodInformation );
				}
			}
		}
		if( classType->isReadonly ) {
			for( FieldInfo& finalField : classType->fields ) {
				finalField.isReadonly = true;
			}
		}
		this->buildVTable( classType );
		this->computeMemoryLayout( classType );
		for( ast::nodes::DeclarationSharedPointer& nestedDeclaration : declaration.nestedDeclarations ) {
			this->analyzeDeclaration( nestedDeclaration );
		}
		for( TypeSharedPointer& interfaceType : classType->interfaces ) {
			this->validateInterfaceImplementation( classType, interfaceType, declaration.source );
			if( interfaceType->qualified == qname::DROPER || interfaceType->name == "Droper" ) {
				classType->implementsDroper = true;
			}
		}
		this->popScope();
	}
	
	void Analyzer::analyzeDeclaration( ast::nodes::DeclarationSharedPointer& declaration ) {
		if( declaration == nullptr ) {
			return;
		}
		switch( declaration->kind ) {
			case ast::Node::Kind::ClassDeclaration:
				this->analyzeClassDeclaration( static_cast<ast::nodes::ClassDeclaration&>( *declaration ) );
				break;
			case ast::Node::Kind::EnumDeclaration:
				this->analyzeEnumDeclaration( static_cast<ast::nodes::EnumDeclaration&>( *declaration ) );
				break;
			case ast::Node::Kind::ExternDeclaration:
				break;
			case ast::Node::Kind::FunctionDeclaration:
				this->analyzeFunctionDeclaration( static_cast<ast::nodes::FunctionDeclaration&>( *declaration ) );
				break;
			case ast::Node::Kind::ImplementationDeclaration:
				this->analyzeImplementDeclaration( static_cast<ast::nodes::ImplementDeclaration&>( *declaration ) );
				break;
			case ast::Node::Kind::InterfaceDeclaration:
				this->analyzeInterfaceDeclaration( static_cast<ast::nodes::InterfaceDeclaration&>( *declaration ) );
				break;
			case ast::Node::Kind::StructDeclaration:
				this->analyzeStructDeclaration( static_cast<ast::nodes::StructDeclaration&>( *declaration ) );
				break;
			case ast::Node::Kind::TraitDeclaration:
				this->analyzeTraitDeclaration( static_cast<ast::nodes::TraitDeclaration&>( *declaration ) );
				break;
			case ast::Node::Kind::TypeAliasDeclaration:
				this->analyzeTypeAliasDeclaration( static_cast<ast::nodes::TypeAliasDeclaration&>( *declaration ) );
				break;
			case ast::Node::Kind::ConstantDeclaration: {
				ast::nodes::ConstantDeclaration& constDecl = static_cast<ast::nodes::ConstantDeclaration&>( *declaration );
				TypeSharedPointer constType = constDecl.type ? this->resolveType( constDecl.type ) : nullptr;
				if( constDecl.initializer ) {
					this->analyzeExpression( constDecl.initializer );
				}
				if( constType ) {
					SymbolSharedPointer constSymbol = std::make_shared<Symbol>( constDecl.name, Symbol::Kind::Variable, constDecl.source, constType );
					constSymbol->isInitialized = true;
					if( constDecl.isGlobalVariable ) {
						constSymbol->isMutable = true;
					}
					this->currentScope->define( constDecl.name, constSymbol );
				}
				break;
			}
			default:
				break;
		}
	}
	
	void Analyzer::analyzeEnumDeclaration( ast::nodes::EnumDeclaration& declaration ) {
		EnumTypeSharedPointer enumType = std::dynamic_pointer_cast<EnumType>( this->typeRegistry.lookupType( declaration.name ) );
		if( enumType == nullptr ) {
			return;
		}
		if( declaration.backedType ) {
			enumType->backedType = this->resolveType( declaration.backedType );
		}
		if( declaration.baseClassType ) {
			TypeSharedPointer baseType = this->resolveType( declaration.baseClassType );
			if( baseType ) {
				enumType->baseClass = baseType;
			}
		}
		for( ast::nodes::TypeNodeSharedPointer& interfaceNode : declaration.interfaces ) {
			TypeSharedPointer interfaceType = this->resolveType( interfaceNode );
			if( interfaceType ) {
				enumType->interfaces.push_back( interfaceType );
			}
		}
		enumType->variants.clear();
		int variantDiscriminant = 0;
		for( ast::nodes::EnumVariantSharedPointer& variant : declaration.variants ) {
			EnumVariantInfo variantInformation;
			variantInformation.name = variant->name;
			variantInformation.discriminant = variantDiscriminant++;
			for( ast::nodes::TypeNodeSharedPointer& associatedTypeNode : variant->associatedTypes ) {
				variantInformation.associatedTypes.push_back( this->resolveType( associatedTypeNode ) );
			}
			for( ast::nodes::DeclarationSharedPointer& methodNode : variant->methods ) {
				if( methodNode->kind != ast::Node::Kind::FunctionDeclaration ) {
					continue;
				}
				ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *methodNode );
				if( functionDeclaration.isAbstract ) {
					std::string abstractMethodErrorMessage = fmt::format( "enum variant method \"{}\" cannot be abstract", functionDeclaration.name );
					this->diagnostic.error( functionDeclaration.source, abstractMethodErrorMessage );
					continue;
				}
				MethodInfo methodInformation;
				methodInformation.name = functionDeclaration.name;
				methodInformation.access = functionDeclaration.access;
				methodInformation.isVirtual = false;
				methodInformation.isOverride = false;
				methodInformation.isStatic = false;
				methodInformation.isFinal = false;
				methodInformation.isProperty = functionDeclaration.isProperty;
				methodInformation.virtualTableIndex = -1;
				variantInformation.methods.push_back( methodInformation );
			}
			enumType->variants.push_back( variantInformation );
			SymbolSharedPointer variantSymbol = std::make_shared<Symbol>( variant->name, Symbol::Kind::EnumVariant, variant->source, enumType );
			std::string variantScopedName = fmt::format( "{}::{}", declaration.name, variant->name );
			this->currentScope->define( variantScopedName, variantSymbol );
		}
		for( ast::nodes::DeclarationSharedPointer& enumMethodNode : declaration.methods ) {
			if( enumMethodNode->kind != ast::Node::Kind::FunctionDeclaration ) {
				continue;
			}
			ast::nodes::FunctionDeclaration& enumFunctionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *enumMethodNode );
			if( enumFunctionDeclaration.isAbstract ) {
				std::string abstractEnumMethodErrorMessage = fmt::format( "enum method \"{}\" cannot be abstract", enumFunctionDeclaration.name );
				this->diagnostic.error( enumFunctionDeclaration.source, abstractEnumMethodErrorMessage );
				continue;
			}
			MethodInfo enumMethodInformation;
			enumMethodInformation.name = enumFunctionDeclaration.name;
			enumMethodInformation.access = enumFunctionDeclaration.access;
			enumMethodInformation.isVirtual = false;
			enumMethodInformation.isOverride = false;
			enumMethodInformation.isStatic = false;
			enumMethodInformation.isFinal = false;
			enumMethodInformation.isProperty = enumFunctionDeclaration.isProperty;
			enumMethodInformation.virtualTableIndex = -1;
			enumType->methods.push_back( enumMethodInformation );
		}
	}
	
	TypeSharedPointer Analyzer::analyzeExpression( ast::nodes::ExpressionSharedPointer& expression ) {
		if( expression == nullptr ) {
			return this->typeRegistry.getError();
		}
		if( this->completionPosition != nullptr &&
			this->completionPosition->location->line == expression->source->location->line &&
			this->completionPosition->location->column >= expression->source->location->column ) {
			if( expression->kind == ast::Node::Kind::IdentifierExpression ) {
				ast::nodes::IdentifierExpression& identExpr = static_cast<ast::nodes::IdentifierExpression&>( *expression );
				this->collectScopeCompletions( identExpr.name, identExpr.source );
			}
		}
		TypeSharedPointer expressionType;
		switch( expression->kind ) {
			case ast::Node::Kind::ArrayExpression: {
				ast::nodes::ArrayExpression& arrayExpression = static_cast<ast::nodes::ArrayExpression&>( *expression );
				TypeSharedPointer elementBaseType;
				for( ast::nodes::ExpressionSharedPointer& element : arrayExpression.elements ) {
					TypeSharedPointer elementType = this->analyzeExpression( element );
					if( elementBaseType == nullptr ) {
						elementBaseType = elementType;
					}
				}
				if( elementBaseType == nullptr ) {
					elementBaseType = this->typeRegistry.getError();
				}
				expressionType = this->typeRegistry.makeArray( elementBaseType, static_cast<int>( arrayExpression.elements.size() ) );
				break;
			}
			case ast::Node::Kind::AwaitExpression: {
				ast::nodes::AwaitExpression& awaitExpression = static_cast<ast::nodes::AwaitExpression&>( *expression );
				if( this->isInsideAsyncFunction == false ) {
					this->diagnostic.error( expression->source, "'await' can only be used inside an async function" );
				}
				TypeSharedPointer operandType = this->analyzeExpression( awaitExpression.operand );
				if( operandType && operandType->kind == Type::Kind::Future ) {
					expressionType = std::static_pointer_cast<FutureType>( operandType )->innerType;
				}
				else if( operandType ) {
					std::string awaitErrorMessage = fmt::format( "'await' requires a Future<T> operand, got \"{}\"", operandType->name );
					this->diagnostic.error( expression->source, awaitErrorMessage );
					expressionType = operandType;
				}
				else {
					expressionType = this->typeRegistry.getError();
				}
				break;
			}
			case ast::Node::Kind::BinaryExpression: {
				expressionType = this->analyzeBinaryExpression( static_cast<ast::nodes::BinaryExpression&>( *expression ) );
				break;
			}
			case ast::Node::Kind::BooleanLiteral: {
				TypeSharedPointer booleanClassType = this->typeRegistry.lookupType( "Boolean" );
				expressionType = booleanClassType ? booleanClassType : this->typeRegistry.getBool();
				break;
			}
			case ast::Node::Kind::CallExpression: {
				expressionType = this->analyzeCallExpression( static_cast<ast::nodes::CallExpression&>( *expression ) );
				break;
			}
			case ast::Node::Kind::CastExpression: {
				ast::nodes::CastExpression& castExpression = static_cast<ast::nodes::CastExpression&>( *expression );
				this->analyzeExpression( castExpression.expression );
				expressionType = this->resolveType( castExpression.targetType );
				break;
			}
			case ast::Node::Kind::CharLiteral: {
				TypeSharedPointer characterClassType = this->typeRegistry.lookupType( "Char" );
				expressionType = characterClassType ? characterClassType : this->typeRegistry.getChar();
				break;
			}
			case ast::Node::Kind::ComprehensionExpression: {
				ast::nodes::ComprehensionExpression& comprehensionExpression = static_cast<ast::nodes::ComprehensionExpression&>( *expression );
				this->pushScope( Scope::Kind::Block );
				TypeSharedPointer iterableType = this->analyzeExpression( comprehensionExpression.iterable );
				TypeSharedPointer variableType;
				if( comprehensionExpression.variableType ) {
					variableType = this->resolveType( comprehensionExpression.variableType );
				}
				else if( iterableType ) {
					if( iterableType->kind == Type::Kind::Array ) {
						variableType = std::static_pointer_cast<ArrayType>( iterableType )->elementType;
					}
					else if( iterableType->kind == Type::Kind::String ) {
						variableType = this->typeRegistry.getChar();
					}
					else if( iterableType->kind == Type::Kind::Generator ) {
						variableType = std::static_pointer_cast<GeneratorType>( iterableType )->yieldType;
					}
					else {
						variableType = this->typeRegistry.getInteger64();
					}
				}
				else {
					variableType = this->typeRegistry.getError();
				}
				SymbolSharedPointer loopSymbol = std::make_shared<Symbol>( comprehensionExpression.variable, Symbol::Kind::Variable, comprehensionExpression.source, variableType );
				loopSymbol->isInitialized = true;
				this->currentScope->define( comprehensionExpression.variable, loopSymbol );
				TypeSharedPointer bodyType = this->analyzeExpression( comprehensionExpression.bodyExpression );
				if( comprehensionExpression.condition ) {
					TypeSharedPointer conditionType = this->analyzeExpression( comprehensionExpression.condition );
					if( conditionType && conditionType->isBool() == false && conditionType->isError() == false ) {
						this->diagnostic.error( comprehensionExpression.condition->source, "comprehension condition must be boolean" );
					}
				}
				this->popScope();
				expressionType = bodyType ? this->typeRegistry.makeArray( bodyType, -1 ) : this->typeRegistry.getError();
				break;
			}
			case ast::Node::Kind::ConstructExpression: {
				ast::nodes::ConstructExpression& constructExpression = static_cast<ast::nodes::ConstructExpression&>( *expression );
				expressionType = this->resolveType( constructExpression.type );
				for( std::pair<std::string,ast::nodes::ExpressionSharedPointer>& field : constructExpression.fields ) {
					if( field.second ) {
						this->analyzeExpression( field.second );
					}
				}
				if( expressionType == nullptr ) {
					expressionType = this->typeRegistry.getError();
				}
				break;
			}
			case ast::Node::Kind::FloatLiteral: {
				TypeSharedPointer floatClassType = this->typeRegistry.lookupType( "F64" );
				expressionType = floatClassType ? floatClassType : this->typeRegistry.getFloat64();
				break;
			}
			case ast::Node::Kind::IdentifierExpression: {
				expressionType = this->analyzeIdentifierExpression( static_cast<ast::nodes::IdentifierExpression&>( *expression ) );
				break;
			}
			case ast::Node::Kind::IndexExpression: {
				expressionType = this->analyzeIndexExpression( static_cast<ast::nodes::IndexExpression&>( *expression ) );
				break;
			}
			case ast::Node::Kind::InstanceofExpression: {
				ast::nodes::InstanceofExpression& instanceOfExpression = static_cast<ast::nodes::InstanceofExpression&>( *expression );
				this->analyzeExpression( instanceOfExpression.object );
				this->resolveType( instanceOfExpression.targetType );
				expressionType = this->typeRegistry.getBool();
				break;
			}
			case ast::Node::Kind::IntegerLiteral: {
				TypeSharedPointer integerClassType = this->typeRegistry.lookupType( "I64" );
				expressionType = integerClassType ? integerClassType : this->typeRegistry.getInteger64();
				break;
			}
			case ast::Node::Kind::LambdaExpression: {
				ast::nodes::LambdaExpression& lambdaExpression = static_cast<ast::nodes::LambdaExpression&>( *expression );
				this->pushScope( Scope::Kind::Function );
				std::vector<TypeSharedPointer> parameterTypes;
				for( ast::nodes::FunctionParameterSharedPointer& parameter : lambdaExpression.parameters ) {
					TypeSharedPointer parameterType = parameter->type ? this->resolveType( parameter->type ) : this->typeRegistry.getError();
					parameterTypes.push_back( parameterType );
					SymbolSharedPointer parameterSymbol = std::make_shared<Symbol>( parameter->name, Symbol::Kind::Variable, parameter->source, parameterType );
					parameterSymbol->isInitialized = true;
					this->currentScope->define( parameter->name, parameterSymbol );
				}
				TypeSharedPointer returnType = lambdaExpression.returnType ? this->resolveType( lambdaExpression.returnType ) : TypeSharedPointer( nullptr );
				if( returnType == nullptr && lambdaExpression.body.empty() == false ) {
					if( lambdaExpression.body[0] && lambdaExpression.body[0]->kind == ast::Node::Kind::ReturnStatement ) {
						ast::nodes::ReturnStatement& returnStatement = static_cast<ast::nodes::ReturnStatement&>( *lambdaExpression.body[0] );
						if( returnStatement.value ) {
							returnType = this->analyzeExpression( returnStatement.value );
						}
					}
				}
				if( returnType == nullptr ) {
					returnType = this->typeRegistry.getVoid();
				}
				this->popScope();
				expressionType = this->typeRegistry.makeFunction( parameterTypes, returnType );
				break;
			}
			case ast::Node::Kind::MatchExpression: {
				ast::nodes::MatchExpression& matchExpression = static_cast<ast::nodes::MatchExpression&>( *expression );
				this->analyzeExpression( matchExpression.subject );
				TypeSharedPointer commonResultType;
				for( size_t patternIndex = 0; patternIndex < matchExpression.patterns.size(); patternIndex++ ) {
					this->analyzeExpression( matchExpression.patterns[patternIndex] );
					TypeSharedPointer patternValueType = this->analyzeExpression( matchExpression.values[patternIndex] );
					if( commonResultType == nullptr && patternValueType ) {
						commonResultType = patternValueType;
					}
				}
				if( matchExpression.defaultValue ) {
					TypeSharedPointer defaultValueType = this->analyzeExpression( matchExpression.defaultValue );
					if( commonResultType == nullptr && defaultValueType ) {
						commonResultType = defaultValueType;
					}
				}
				expressionType = commonResultType ? commonResultType : this->typeRegistry.getError();
				break;
			}
			case ast::Node::Kind::MemberAccessExpression: {
				expressionType = this->analyzeMemberAccessExpression( static_cast<ast::nodes::MemberAccessExpression&>( *expression ) );
				break;
			}
			case ast::Node::Kind::MethodCallExpression: {
				expressionType = this->analyzeMethodCallExpression( static_cast<ast::nodes::MethodCallExpression&>( *expression ) );
				break;
			}
			case ast::Node::Kind::NoneLiteral: {
				expressionType = this->typeRegistry.getVoid();
				break;
			}
			case ast::Node::Kind::RangeExpression: {
				ast::nodes::RangeExpression& rangeExpression = static_cast<ast::nodes::RangeExpression&>( *expression );
				this->analyzeExpression( rangeExpression.start );
				this->analyzeExpression( rangeExpression.end );
				expressionType = this->typeRegistry.makeArray( this->typeRegistry.getInteger64() );
				break;
			}
			case ast::Node::Kind::SelfExpression: {
				ScopeSharedPointer currentSearchingScope = this->currentScope;
				while( currentSearchingScope ) {
					if( currentSearchingScope->classType ) {
						expressionType = currentSearchingScope->classType;
						break;
					}
					currentSearchingScope = currentSearchingScope->parent();
				}
				if( expressionType == nullptr ) {
					this->diagnostic.error( expression->source, "'self' used outside of class context" );
					expressionType = this->typeRegistry.getError();
				}
				break;
			}
			case ast::Node::Kind::StringLiteral: {
				TypeSharedPointer stringClassType = this->typeRegistry.lookupType( "String" );
				expressionType = stringClassType ? stringClassType : this->typeRegistry.getString();
				break;
			}
			case ast::Node::Kind::SubclassofExpression: {
				ast::nodes::SubclassofExpression& subclassOfExpression = static_cast<ast::nodes::SubclassofExpression&>( *expression );
				this->resolveType( subclassOfExpression.sourceType );
				this->resolveType( subclassOfExpression.targetType );
				expressionType = this->typeRegistry.getBool();
				break;
			}
			case ast::Node::Kind::TypeReferenceExpression: {
				ast::nodes::TypeReferenceExpression& typeReferenceExpression = static_cast<ast::nodes::TypeReferenceExpression&>( *expression );
				TypeSharedPointer resolvedReferenceType = this->typeRegistry.lookupType( typeReferenceExpression.typeName );
				if( resolvedReferenceType ) {
					expressionType = this->typeRegistry.makeMeta( resolvedReferenceType );
				}
				else {
					std::string unknownTypeErrorMessage = fmt::format( "unknown type \"{}\"", typeReferenceExpression.typeName );
					this->diagnostic.error( expression->source, unknownTypeErrorMessage );
					expressionType = this->typeRegistry.getError();
				}
				break;
			}
			case ast::Node::Kind::UnaryExpression: {
				expressionType = this->analyzeUnaryExpression( static_cast<ast::nodes::UnaryExpression&>( *expression ) );
				break;
			}
			case ast::Node::Kind::YieldExpression: {
				ast::nodes::YieldExpression& yieldExpression = static_cast<ast::nodes::YieldExpression&>( *expression );
				expressionType = yieldExpression.value ? this->analyzeExpression( yieldExpression.value ) : this->typeRegistry.getVoid();
				break;
			}
			default: {
				expressionType = this->typeRegistry.getError();
				break;
			}
		}
		expression->semanticType = expressionType;
		return expressionType;
	}
	
	void Analyzer::analyzeForStatement( ast::nodes::ForStatement& statement ) {
		this->pushScope( Scope::Kind::Loop );
		if( statement.isCStyle ) {
			TypeSharedPointer variableType = this->typeRegistry.getInteger64();
			if( statement.variableType ) {
				variableType = this->resolveType( statement.variableType );
			}
			SymbolSharedPointer variableSymbol = std::make_shared<Symbol>( statement.variable, Symbol::Kind::Variable, statement.source, variableType );
			variableSymbol->isInitialized = true;
			variableSymbol->isMutable = true;
			this->currentScope->define( statement.variable, variableSymbol );
			if( statement.initializer ) {
				this->analyzeExpression( statement.initializer );
			}
			if( statement.condition ) {
				TypeSharedPointer conditionType = this->analyzeExpression( statement.condition );
				if( conditionType && conditionType->isBool() == false && conditionType->isError() == false ) {
					this->diagnostic.error( statement.condition->source, "for condition must be of type 'bool'" );
				}
			}
			if( statement.update ) {
				this->analyzeStatement( statement.update );
			}
		}
		else {
			TypeSharedPointer iterableType = this->analyzeExpression( statement.iterable );
			bool isRangeExpression = statement.iterable && statement.iterable->kind == ast::Node::Kind::RangeExpression;
			std::function<bool(const TypeSharedPointer&, std::unordered_set<std::string>&)> isIteratorOrIterable;
			isIteratorOrIterable = [&isIteratorOrIterable]( const TypeSharedPointer& iface, std::unordered_set<std::string>& visited ) -> bool {
				if( iface == nullptr ) {
					return false;
				}
				if( qname::startsWith( iface->qualified, qname::ITERATOR ) ||
					qname::startsWith( iface->qualified, qname::ITERABLE ) ||
					iface->name == "Iterator" || iface->name.substr( 0, 9 ) == "Iterator<" ||
					iface->name == "Iterable" || iface->name.substr( 0, 9 ) == "Iterable<" ) {
					return true;
				}
				if( iface->kind == Type::Kind::Interface ) {
					InterfaceTypeSharedPointer ifaceType = std::static_pointer_cast<InterfaceType>( iface );
					std::string key = ifaceType->qualified.empty() ? ifaceType->name : ifaceType->qualified;
					if( visited.count( key ) > 0 ) {
						return false;
					}
					visited.insert( key );
					for( TypeSharedPointer& superIface : ifaceType->superInterfaces ) {
						if( isIteratorOrIterable( superIface, visited ) ) {
							return true;
						}
					}
				}
				return false;
			};
			std::function<TypeSharedPointer(const TypeSharedPointer&)> hasIteratorInterface = [this, &isIteratorOrIterable]( const TypeSharedPointer& targetType ) -> TypeSharedPointer {
				if( targetType->kind == Type::Kind::Class ) {
					ClassTypeSharedPointer classType = std::static_pointer_cast<ClassType>( targetType );
					bool implementsIterator = false;
					std::unordered_set<std::string> visited;
					for( TypeSharedPointer& interfaceType : classType->interfaces ) {
						if( isIteratorOrIterable( interfaceType, visited ) ) {
							implementsIterator = true;
							break;
						}
					}
					if( implementsIterator == false ) {
						return TypeSharedPointer( nullptr );
					}
					MethodInfo* nextMethod = classType->findMethod( "next" );
					if( nextMethod && nextMethod->type && nextMethod->type->kind == Type::Kind::Function ) {
						return std::static_pointer_cast<FunctionType>( nextMethod->type )->returnType;
					}
				}
				return TypeSharedPointer( nullptr );
			};
			TypeSharedPointer variableType = this->typeRegistry.getError();
			if( statement.variableType ) {
				variableType = this->resolveType( statement.variableType );
				if( iterableType && isRangeExpression == false &&
					iterableType->kind != Type::Kind::Array &&
					( iterableType->kind != Type::Kind::String && iterableType->qualified != qname::STRING ) &&
					iterableType->kind != Type::Kind::Generator &&
					hasIteratorInterface( iterableType ) == nullptr &&
					iterableType->isError() == false ) {
					if( iterableType->kind == Type::Kind::Class ) {
						std::string classNotIterableErrorMessage = fmt::format( "type \"{}\" is not iterable; class must implement Iterator or Iterable interface", iterableType->toString() );
						this->diagnostic.error( statement.iterable->source, classNotIterableErrorMessage );
					}
					else {
						std::string genericNotIterableErrorMessage = fmt::format( "type \"{}\" is not iterable", iterableType->toString() );
						this->diagnostic.error( statement.iterable->source, genericNotIterableErrorMessage );
					}
				}
			}
			else if( iterableType ) {
				if( isRangeExpression ) {
					variableType = this->typeRegistry.getInteger64();
				}
				else if( iterableType->kind == Type::Kind::Array ) {
					variableType = std::static_pointer_cast<ArrayType>( iterableType )->elementType;
				}
				else if( iterableType->kind == Type::Kind::String || iterableType->qualified == qname::STRING ) {
					variableType = this->typeRegistry.getChar();
				}
				else if( iterableType->kind == Type::Kind::Generator ) {
					variableType = std::static_pointer_cast<GeneratorType>( iterableType )->yieldType;
				}
				else {
					TypeSharedPointer nextReturnType = hasIteratorInterface( iterableType );
					if( nextReturnType ) {
						if( nextReturnType->kind == Type::Kind::Optional ) {
							variableType = std::static_pointer_cast<OptionalType>( nextReturnType )->inner;
						}
						else {
							variableType = nextReturnType;
						}
					}
					else {
						if( iterableType->kind == Type::Kind::Class ) {
							std::string classInterfaceMissingErrorMessage = fmt::format( "type \"{}\" is not iterable; class must implement Iterator or Iterable interface", iterableType->toString() );
							this->diagnostic.error( statement.iterable->source, classInterfaceMissingErrorMessage );
						}
						else {
							std::string typeNotIterableErrorMessage = fmt::format( "type \"{}\" is not iterable", iterableType->toString() );
							this->diagnostic.error( statement.iterable->source, typeNotIterableErrorMessage );
						}
						variableType = this->typeRegistry.getError();
					}
				}
			}
			TypeSharedPointer pairValueType = nullptr;
			if( statement.variable2.empty() == false && variableType != nullptr &&
				variableType->kind == Type::Kind::Struct ) {
				StructTypeSharedPointer pairStructType = std::static_pointer_cast<StructType>( variableType );
				std::string structBaseName = pairStructType->name;
				size_t angleBracketPosition = structBaseName.find( '<' );
				if( angleBracketPosition != std::string::npos ) {
					structBaseName = structBaseName.substr( 0, angleBracketPosition );
				}
				if( structBaseName == "Pair" ) {
					FieldInfo* keyField = pairStructType->findField( "key" );
					FieldInfo* valueField = pairStructType->findField( "value" );
					if( keyField != nullptr && valueField != nullptr &&
						keyField->type != nullptr && valueField->type != nullptr ) {
						TypeSharedPointer resolvedKeyType = keyField->type;
						TypeSharedPointer resolvedValueType = valueField->type;
						if( iterableType != nullptr && iterableType->kind == Type::Kind::Class ) {
							ClassTypeSharedPointer iterableClassType = std::static_pointer_cast<ClassType>( iterableType );
							if( resolvedKeyType->kind == Type::Kind::GenericParameter ) {
								std::unordered_map<std::string,TypeSharedPointer>::iterator substitution =
									iterableClassType->typeSubstitutions.find( resolvedKeyType->name );
								if( substitution != iterableClassType->typeSubstitutions.end() ) {
									resolvedKeyType = substitution->second;
								}
							}
							if( resolvedValueType->kind == Type::Kind::GenericParameter ) {
								std::unordered_map<std::string,TypeSharedPointer>::iterator substitution =
									iterableClassType->typeSubstitutions.find( resolvedValueType->name );
								if( substitution != iterableClassType->typeSubstitutions.end() ) {
									resolvedValueType = substitution->second;
								}
							}
						}
						variableType = resolvedKeyType;
						pairValueType = resolvedValueType;
					}
					else if( pairStructType->genericParameters.size() >= 2 ) {
						variableType = pairStructType->genericParameters[0];
						pairValueType = pairStructType->genericParameters[1];
					}
				}
			}
			SymbolSharedPointer variableSymbol = std::make_shared<Symbol>( statement.variable, Symbol::Kind::Variable, statement.source, variableType );
			variableSymbol->isInitialized = true;
			this->currentScope->define( statement.variable, variableSymbol );
			if( statement.variable2.empty() == false ) {
				TypeSharedPointer secondaryVariableType = this->typeRegistry.getInteger64();
				if( pairValueType != nullptr ) {
					secondaryVariableType = pairValueType;
				}
				else if( statement.variableType2 ) {
					secondaryVariableType = this->resolveType( statement.variableType2 );
				}
				else if( iterableType && iterableType->kind == Type::Kind::Array ) {
					secondaryVariableType = variableType;
				}
				SymbolSharedPointer secondaryVariableSymbol = std::make_shared<Symbol>( statement.variable2, Symbol::Kind::Variable, statement.source, secondaryVariableType );
				secondaryVariableSymbol->isInitialized = true;
				this->currentScope->define( statement.variable2, secondaryVariableSymbol );
			}
		}
		for( ast::nodes::StatementSharedPointer& bodyStatement : statement.body ) {
			this->analyzeStatement( bodyStatement );
		}
		this->popScope();
	}
	
	void Analyzer::analyzeFunctionDeclaration( ast::nodes::FunctionDeclaration& declaration ) {
		bool hasYieldExpression = false;
		for( ast::nodes::StatementSharedPointer& bodyStatement : declaration.body ) {
			if( statementContainsYield( bodyStatement ) ) {
				hasYieldExpression = true;
				break;
			}
		}
		TypeSharedPointer rawReturnType = declaration.returnType ? this->resolveType( declaration.returnType ) : this->typeRegistry.getVoid();
		bool returnsGeneratorType = rawReturnType && rawReturnType->kind == Type::Kind::Generator;
		if( hasYieldExpression && returnsGeneratorType == false ) {
			std::string generatorErrorMessage = fmt::format( "function \"{}\" contains 'yield' but does not declare return type 'Generator<T>'; use '-> Generator<T>' instead of '-> T'", declaration.name );
			this->diagnostic.error( declaration.source, generatorErrorMessage );
		}
		if( returnsGeneratorType ) {
			declaration.isGenerator = true;
		}
		std::vector<TypeSharedPointer> parameterTypes;
		int variadicParamIndex = -1;
		int keywordParamIndex = -1;
		bool bodySeenVariadic = false;
		TypeSharedPointer variadicElemType;
		TypeSharedPointer kwargValType;
		std::vector<std::string> bodyKwOnlyNames;
		std::vector<TypeSharedPointer> bodyKwOnlyTypes;
		int paramTypeIndex = 0;
		for( ast::nodes::FunctionParameterSharedPointer& parameter : declaration.parameters ) {
			if( parameter->isSelf ) {
				bool isInsideTypeContext = this->currentScope->isInsideClass();
				if( isInsideTypeContext == false ) {
					ScopeSharedPointer searchScope = this->currentScope->parent();
					while( searchScope ) {
						if( searchScope->kind() == Scope::Kind::Class ) {
							isInsideTypeContext = true;
							break;
						}
						searchScope = searchScope->parent();
					}
				}
				if( isInsideTypeContext && this->currentScope->classType ) {
					parameterTypes.push_back( this->currentScope->classType );
					paramTypeIndex++;
				}
				else if( isInsideTypeContext == false ) {
					std::string selfErrorMessage = fmt::format( "parameter 'self' is not allowed in free function \"{}\"; 'self' can only be used in class or struct methods", declaration.name );
					this->diagnostic.error( parameter->source, selfErrorMessage );
				}
				continue;
			}
			TypeSharedPointer parameterType = this->resolveType( parameter->type );
			if( parameter->isVariadic ) {
				bodySeenVariadic = true;
				TypeSharedPointer elementType = parameterType;
				if( parameterType && parameterType->kind == Type::Kind::Array ) {
					elementType = std::static_pointer_cast<ArrayType>( parameterType )->elementType;
				}
				variadicElemType = elementType ? elementType : this->typeRegistry.getError();
				variadicParamIndex = paramTypeIndex;
				parameterTypes.push_back( this->monomorphizeGenericType( "Args", { variadicElemType }, parameter->source ) );
			}
			else if( parameter->isKeyword ) {
				kwargValType = parameterType ? parameterType : this->typeRegistry.getError();
				keywordParamIndex = paramTypeIndex;
				parameterTypes.push_back( this->monomorphizeGenericType( "Kwargs", { kwargValType }, parameter->source ) );
			}
			else {
				parameterTypes.push_back( parameterType ? parameterType : this->typeRegistry.getError() );
				if( bodySeenVariadic && parameter->defaultValue ) {
					bodyKwOnlyNames.push_back( parameter->name );
					bodyKwOnlyTypes.push_back( parameterType ? parameterType : this->typeRegistry.getError() );
				}
			}
			paramTypeIndex++;
		}
		TypeSharedPointer returnType = declaration.returnType ? this->resolveType( declaration.returnType ) : this->typeRegistry.getVoid();
		if( declaration.isAsync && returnType->kind != Type::Kind::Future ) {
			std::string asyncErrorMessage = fmt::format( "async function \"{}\" must declare return type as 'Future<T>' (e.g. 'Future<{}>')", declaration.name, returnType->name );
			this->diagnostic.error( declaration.source, asyncErrorMessage );
		}
		TypeSharedPointer exposedReturnType = returnType;
		if( declaration.isGenerator && returnType->kind != Type::Kind::Generator ) {
			exposedReturnType = this->typeRegistry.makeGenerator( returnType );
		}
		TypeSharedPointer functionType = this->typeRegistry.makeFunction( parameterTypes, exposedReturnType );
		FunctionTypeSharedPointer concreteFunctionType = std::static_pointer_cast<FunctionType>( functionType );
		concreteFunctionType->variadicParameterIndex = variadicParamIndex;
		concreteFunctionType->keywordParameterIndex = keywordParamIndex;
		concreteFunctionType->variadicElementType = variadicElemType;
		concreteFunctionType->keywordValueType = kwargValType;
		concreteFunctionType->keywordOnlyParamNames = std::move( bodyKwOnlyNames );
		concreteFunctionType->keywordOnlyParamTypes = std::move( bodyKwOnlyTypes );
		if( declaration.raisesTypes.empty() == false ) {
			for( ast::nodes::TypeNodeSharedPointer& raiseTypeNode : declaration.raisesTypes ) {
				TypeSharedPointer resolvedRaiseType = this->resolveType( raiseTypeNode );
				if( resolvedRaiseType ) {
					concreteFunctionType->exceptionTypes.push_back( resolvedRaiseType );
				}
			}
		}
		SymbolSharedPointer functionSymbol = std::make_shared<Symbol>( declaration.name, Symbol::Kind::Function, declaration.source, functionType );
		functionSymbol->access = declaration.access;
		functionSymbol->isInitialized = true;
		this->currentScope->define( declaration.name, functionSymbol );
		this->pushScope( Scope::Kind::Function );
		bool savedUserCodeStatus = this->isInUserCode_;
		if( this->userSourceFile_.empty() == false && declaration.source != nullptr ) {
			this->isInUserCode_ = ( declaration.source->pathname == this->userSourceFile_ || declaration.source->filename == this->userSourceFile_ );
		}
		TypeSharedPointer bodyReturnType = returnType;
		if( declaration.isAsync && returnType->kind == Type::Kind::Future ) {
			bodyReturnType = std::static_pointer_cast<FutureType>( returnType )->innerType;
		}
		bool savedAsyncStatus = this->isInsideAsyncFunction;
		this->currentScope->returnType = bodyReturnType;
		this->currentReturnType = bodyReturnType;
		this->isInsideAsyncFunction = declaration.isAsync;
		std::vector<TypeSharedPointer> savedRaisesTypes = this->currentExceptionTypes;
		this->currentExceptionTypes.clear();
		for( ast::nodes::TypeNodeSharedPointer& raiseTypeNode : declaration.raisesTypes ) {
			TypeSharedPointer resolvedRaiseType = this->resolveType( raiseTypeNode );
			if( resolvedRaiseType ) {
				this->currentExceptionTypes.push_back( resolvedRaiseType );
			}
		}
		for( ast::nodes::FunctionParameterSharedPointer& parameter : declaration.parameters ) {
			if( parameter->isSelf ) {
				continue;
			}
			TypeSharedPointer parameterType = this->resolveType( parameter->type );
			if( parameter->isVariadic ) {
				TypeSharedPointer elementType = parameterType;
				if( parameterType && parameterType->kind == Type::Kind::Array ) {
					elementType = std::static_pointer_cast<ArrayType>( parameterType )->elementType;
				}
				parameterType = this->monomorphizeGenericType( "Args", { elementType ? elementType : this->typeRegistry.getError() }, parameter->source );
			}
			else if( parameter->isKeyword ) {
				parameterType = this->monomorphizeGenericType( "Kwargs", { parameterType ? parameterType : this->typeRegistry.getError() }, parameter->source );
			}
			SymbolSharedPointer parameterSymbol = std::make_shared<Symbol>( parameter->name, Symbol::Kind::Parameter, parameter->source, parameterType ? parameterType : this->typeRegistry.getError() );
			parameterSymbol->isMutable = parameter->isMutable;
			parameterSymbol->isInitialized = true;
			this->currentScope->define( parameter->name, parameterSymbol );
		}
		for( ast::nodes::DeclarationSharedPointer& nestedFunction : declaration.nestedFunctions ) {
			this->analyzeDeclaration( nestedFunction );
		}
		for( ast::nodes::StatementSharedPointer& bodyStatement : declaration.body ) {
			this->analyzeStatement( bodyStatement );
		}
		this->popScope();
		this->currentReturnType = nullptr;
		this->isInsideAsyncFunction = savedAsyncStatus;
		this->isInUserCode_ = savedUserCodeStatus;
		this->currentExceptionTypes = savedRaisesTypes;
	}
	
	TypeSharedPointer Analyzer::analyzeIdentifierExpression( ast::nodes::IdentifierExpression& expression ) {
		TypeSharedPointer typeFromRegistry = this->typeRegistry.lookupType( expression.name );
		if( typeFromRegistry ) {
			if( this->isInUserCode_ && this->userImportedIdentifiers_.empty() == false && this->userImportedIdentifiers_.count( expression.name ) == 0 ) {
				std::string importErrorMessage = fmt::format( "\"{}\" is not imported; add 'import' statement to use this type", expression.name );
				this->diagnostic.error( expression.source, importErrorMessage );
				return this->typeRegistry.getError();
			}
			return typeFromRegistry;
		}
		SymbolSharedPointer variableSymbol = this->currentScope->lookup( expression.name );
		if( variableSymbol == nullptr ) {
			std::string undefinedErrorMessage = fmt::format( "undefined identifier \"{}\"", expression.name );
			this->diagnostic.error( expression.source, undefinedErrorMessage );
			return this->typeRegistry.getError();
		}
		if( this->isInUserCode_ && this->userImportedIdentifiers_.empty() == false && this->userImportedIdentifiers_.count( expression.name ) == 0 ) {
			if( variableSymbol->source != nullptr && variableSymbol->source->pathname != this->userSourceFile_ && variableSymbol->source->filename != this->userSourceFile_ ) {
				std::string importErrorMessage = fmt::format( "\"{}\" is not imported; add 'import' statement to use this identifier", expression.name );
				this->diagnostic.error( expression.source, importErrorMessage );
				return this->typeRegistry.getError();
			}
		}
		expression.resolvedSymbol = variableSymbol;
		if( variableSymbol->ownership->isMoved ) {
			std::string movedErrorMessage = fmt::format( "use of moved value \"{}\"", expression.name );
			this->diagnostic.error( expression.source, movedErrorMessage );
		}
		if( variableSymbol->isInitialized == false ) {
			std::string uninitializedErrorMessage = fmt::format( "use of uninitialized variable \"{}\"", expression.name );
			this->diagnostic.error( expression.source, uninitializedErrorMessage );
		}
		this->validateAccessControl( variableSymbol, expression.source );
		return variableSymbol->typeref;
	}
	
	void Analyzer::analyzeIfStatement( ast::nodes::IfStatement& statement ) {
		TypeSharedPointer conditionType = this->analyzeExpression( statement.condition );
		if( conditionType && conditionType->isBool() == false && conditionType->isError() == false ) {
			std::string conditionErrorMessage = fmt::format( "condition must be of type 'bool', found \"{}\"", conditionType->toString() );
			this->diagnostic.error( statement.condition->source, conditionErrorMessage );
		}
		this->pushScope( Scope::Kind::Block );
		for( ast::nodes::StatementSharedPointer& thenStatement : statement.thenBody ) {
			this->analyzeStatement( thenStatement );
		}
		this->popScope();
		for( std::pair<ast::nodes::ExpressionSharedPointer,std::vector<ast::nodes::StatementSharedPointer>>& elifBranch : statement.elifBranches ) {
			ast::nodes::ExpressionSharedPointer& elifCondition = elifBranch.first;
			std::vector<ast::nodes::StatementSharedPointer>& elifBody = elifBranch.second;
			TypeSharedPointer elifConditionType = this->analyzeExpression( elifCondition );
			if( elifConditionType && elifConditionType->isBool() == false && elifConditionType->isError() == false ) {
				this->diagnostic.error( elifCondition->source, "condition must be of type 'bool'" );
			}
			this->pushScope( Scope::Kind::Block );
			for( ast::nodes::StatementSharedPointer& elifStatement : elifBody ) {
				this->analyzeStatement( elifStatement );
			}
			this->popScope();
		}
		if( statement.elseBody.empty() == false ) {
			this->pushScope( Scope::Kind::Block );
			for( ast::nodes::StatementSharedPointer& elseStatement : statement.elseBody ) {
				this->analyzeStatement( elseStatement );
			}
			this->popScope();
		}
	}
	
	void Analyzer::analyzeImplementDeclaration( ast::nodes::ImplementDeclaration& declaration ) {
		TypeSharedPointer targetType = this->resolveType( declaration.targetType );
		if( targetType == nullptr ) {
			this->diagnostic.error( declaration.source, "cannot resolve impl target type" );
			return;
		}
		this->pushScope( Scope::Kind::Class );
		this->currentScope->classType = targetType;
		for( ast::nodes::DeclarationSharedPointer& methodDeclaration : declaration.methods ) {
			this->analyzeDeclaration( methodDeclaration );
		}
		this->popScope();
	}
	
	TypeSharedPointer Analyzer::analyzeIndexExpression( ast::nodes::IndexExpression& expression ) {
		TypeSharedPointer objectType = this->analyzeExpression( expression.object );
		TypeSharedPointer indexType = this->analyzeExpression( expression.index );
		if( objectType == nullptr || objectType->isError() ) {
			return this->typeRegistry.getError();
		}
		if( objectType->kind == Type::Kind::Array ) {
			if( indexType && indexType->isIntegral() == false ) {
				this->diagnostic.error( expression.index->source, "array index must be an integer" );
			}
			return std::static_pointer_cast<ArrayType>( objectType )->elementType;
		}
		if( objectType->kind == Type::Kind::Class || objectType->kind == Type::Kind::Struct ) {
			ClassTypeSharedPointer classType = std::dynamic_pointer_cast<ClassType>( objectType );
			if( classType ) {
				bool implementsIndexable = false;
				for( TypeSharedPointer& iface : classType->interfaces ) {
					if( iface && iface->qualified == qname::INDEXABLE ) {
						implementsIndexable = true;
						break;
					}
				}
				if( implementsIndexable ) {
					MethodInfo* getMethod = classType->findMethod( "get" );
					if( getMethod && getMethod->type && getMethod->type->kind == Type::Kind::Function ) {
						FunctionTypeSharedPointer functionType = std::static_pointer_cast<FunctionType>( getMethod->type );
						return functionType->returnType;
					}
				}
			}
		}
		std::string notIndexableErrorMessage = fmt::format( "type \"{}\" is not indexable", objectType->toString() );
		this->diagnostic.error( expression.source, notIndexableErrorMessage );
		return this->typeRegistry.getError();
	}
	
	void Analyzer::analyzeInterfaceDeclaration( ast::nodes::InterfaceDeclaration& declaration ) {
		InterfaceTypeSharedPointer interfaceType = std::dynamic_pointer_cast<InterfaceType>( this->typeRegistry.lookupType( declaration.name ) );
		if( interfaceType == nullptr ) {
			return;
		}
		bool isAlreadyPreRegistered = interfaceType->methods.empty() == false;
		if( isAlreadyPreRegistered == false ) {
			for( ast::nodes::GenericParameterSharedPointer& genericParameter : declaration.genericParameters ) {
				GenericParameterTypeSharedPointer genericParameterType = std::make_shared<GenericParameterType>( genericParameter->name );
				interfaceType->genericParameters.push_back( genericParameterType );
				this->typeRegistry.registerType( genericParameter->name, genericParameterType );
			}
			for( ast::nodes::TypeNodeSharedPointer& superInterfaceNode : declaration.superInterfaces ) {
				TypeSharedPointer superInterfaceType = this->resolveType( superInterfaceNode );
				if( superInterfaceType ) {
					interfaceType->superInterfaces.push_back( superInterfaceType );
					if( superInterfaceType->kind == Type::Kind::Interface ) {
						InterfaceTypeSharedPointer superInterface = std::static_pointer_cast<InterfaceType>( superInterfaceType );
						for( MethodInfo& methodInfo : superInterface->methods ) {
							interfaceType->methods.push_back( methodInfo );
						}
					}
				}
			}
		}
		else {
			for( TypeSharedPointer& genericParameter : interfaceType->genericParameters ) {
				this->typeRegistry.registerType( genericParameter->name, genericParameter );
			}
		}
		this->pushScope( Scope::Kind::Class );
		for( ast::nodes::DeclarationSharedPointer& methodDeclaration : declaration.methods ) {
			this->analyzeDeclaration( methodDeclaration );
			if( isAlreadyPreRegistered == false && methodDeclaration->kind == ast::Node::Kind::FunctionDeclaration ) {
				ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *methodDeclaration );
				std::vector<TypeSharedPointer> parameterTypes;
				for( ast::nodes::FunctionParameterSharedPointer& parameter : functionDeclaration.parameters ) {
					if( parameter->isSelf ) {
						continue;
					}
					TypeSharedPointer parameterType = this->resolveType( parameter->type );
					parameterTypes.push_back( parameterType ? parameterType : this->typeRegistry.getError() );
				}
				TypeSharedPointer returnType = functionDeclaration.returnType ? this->resolveType( functionDeclaration.returnType ) : this->typeRegistry.getVoid();
				TypeSharedPointer functionType = this->typeRegistry.makeFunction( parameterTypes, returnType );
				MethodInfo methodInformation;
				methodInformation.name = functionDeclaration.name;
				methodInformation.type = functionType;
				methodInformation.access = functionDeclaration.access;
				methodInformation.isVirtual = true;
				methodInformation.isOverride = false;
				methodInformation.isStatic = functionDeclaration.isStatic;
				methodInformation.isFinal = false;
				methodInformation.isProperty = functionDeclaration.isProperty;
				methodInformation.virtualTableIndex = -1;
				interfaceType->methods.push_back( methodInformation );
			}
		}
		for( ast::nodes::DeclarationSharedPointer& nestedDeclaration : declaration.nestedDeclarations ) {
			this->analyzeDeclaration( nestedDeclaration );
		}
		this->popScope();
	}
	
	void Analyzer::analyzeMatchStatement( ast::nodes::MatchStatement& statement ) {
		this->analyzeExpression( statement.subject );
		for( ast::nodes::MatchArmNodeSharedPointer& matchArm : statement.arms ) {
			this->pushScope( Scope::Kind::Block );
			this->analyzeExpression( matchArm->pattern );
			if( matchArm->guard ) {
				this->analyzeExpression( matchArm->guard );
			}
			for( ast::nodes::StatementSharedPointer& armStatement : matchArm->body ) {
				this->analyzeStatement( armStatement );
			}
			this->popScope();
		}
	}
	
	TypeSharedPointer Analyzer::analyzeMemberAccessExpression( ast::nodes::MemberAccessExpression& expression ) {
		TypeSharedPointer objectType = this->analyzeExpression( expression.object );
		if( objectType == nullptr || objectType->isError() ) {
			return this->typeRegistry.getError();
		}
		if( this->completionPosition != nullptr &&
			this->completionPosition->location->line == expression.source->location->line &&
			this->completionPosition->location->column >= expression.source->location->column ) {
			this->collectTypeCompletions( objectType, expression.source );
		}
		if( objectType->kind == Type::Kind::Class ) {
			ClassTypeSharedPointer classType = std::static_pointer_cast<ClassType>( objectType );
			FieldInfo* fieldInformation = classType->findField( expression.member );
			if( fieldInformation == nullptr && classType->astDeclaration ) {
				ClassTypeSharedPointer baseClassType = std::dynamic_pointer_cast<ClassType>( this->typeRegistry.lookupType( classType->astDeclaration->name ) );
				if( baseClassType ) {
					fieldInformation = baseClassType->findField( expression.member );
				}
			}
			if( fieldInformation ) {
				TypeSharedPointer fieldType = fieldInformation->type;
				if( fieldType && fieldType->kind == Type::Kind::GenericParameter ) {
					GenericParameterTypeSharedPointer genericParameter = std::static_pointer_cast<GenericParameterType>( fieldType );
					std::unordered_map<std::string,TypeSharedPointer>::iterator substitutionIterator = classType->typeSubstitutions.find( genericParameter->name );
					if( substitutionIterator != classType->typeSubstitutions.end() ) {
						return substitutionIterator->second;
					}
				}
				return fieldType;
			}
			MethodInfo* methodInformation = classType->findMethod( expression.member );
			if( methodInformation == nullptr && classType->astDeclaration ) {
				ClassTypeSharedPointer baseClassType = std::dynamic_pointer_cast<ClassType>( this->typeRegistry.lookupType( classType->astDeclaration->name ) );
				if( baseClassType ) {
					methodInformation = baseClassType->findMethod( expression.member );
				}
			}
			if( methodInformation && methodInformation->isProperty ) {
				FunctionTypeSharedPointer functionType = std::dynamic_pointer_cast<FunctionType>( methodInformation->type );
				if( functionType ) {
					TypeSharedPointer returnType = functionType->returnType;
					if( returnType && returnType->kind == Type::Kind::GenericParameter ) {
						GenericParameterTypeSharedPointer genericParameter = std::static_pointer_cast<GenericParameterType>( returnType );
						std::unordered_map<std::string,TypeSharedPointer>::iterator substitutionIterator = classType->typeSubstitutions.find( genericParameter->name );
						if( substitutionIterator != classType->typeSubstitutions.end() ) {
							return substitutionIterator->second;
						}
					}
					return returnType;
				}
				return this->typeRegistry.getError();
			}
		}
		else if( objectType->kind == Type::Kind::Struct ) {
			StructTypeSharedPointer structType = std::static_pointer_cast<StructType>( objectType );
			FieldInfo* fieldInformation = structType->findField( expression.member );
			if( fieldInformation == nullptr && structType->astDeclaration ) {
				StructTypeSharedPointer baseStructType = std::dynamic_pointer_cast<StructType>( this->typeRegistry.lookupType( structType->astDeclaration->name ) );
				if( baseStructType ) {
					fieldInformation = baseStructType->findField( expression.member );
				}
			}
			if( fieldInformation ) {
				TypeSharedPointer fieldType = fieldInformation->type;
				if( fieldType && fieldType->kind == Type::Kind::GenericParameter ) {
					GenericParameterTypeSharedPointer genericParameter = std::static_pointer_cast<GenericParameterType>( fieldType );
					std::unordered_map<std::string,TypeSharedPointer>::iterator substitutionIterator = structType->typeSubstitutions.find( genericParameter->name );
					if( substitutionIterator != structType->typeSubstitutions.end() ) {
						return substitutionIterator->second;
					}
				}
				return fieldType;
			}
			MethodInfo* methodInformation = structType->findMethod( expression.member );
			if( methodInformation == nullptr && structType->astDeclaration ) {
				StructTypeSharedPointer baseStructType = std::dynamic_pointer_cast<StructType>( this->typeRegistry.lookupType( structType->astDeclaration->name ) );
				if( baseStructType ) {
					methodInformation = baseStructType->findMethod( expression.member );
				}
			}
			if( methodInformation && methodInformation->isProperty ) {
				FunctionTypeSharedPointer functionType = std::dynamic_pointer_cast<FunctionType>( methodInformation->type );
				if( functionType ) {
					TypeSharedPointer returnType = functionType->returnType;
					if( returnType && returnType->kind == Type::Kind::GenericParameter ) {
						GenericParameterTypeSharedPointer genericParameter = std::static_pointer_cast<GenericParameterType>( returnType );
						std::unordered_map<std::string,TypeSharedPointer>::iterator substitutionIterator = structType->typeSubstitutions.find( genericParameter->name );
						if( substitutionIterator != structType->typeSubstitutions.end() ) {
							return substitutionIterator->second;
						}
					}
					return returnType;
				}
				return this->typeRegistry.getError();
			}
		}
		else if( objectType->kind == Type::Kind::Tuple ) {
			TupleTypeSharedPointer tupleType = std::static_pointer_cast<TupleType>( objectType );
			try {
				int tupleIndex = std::stoi( expression.member );
				if( tupleIndex >= 0 && static_cast<size_t>(tupleIndex) < tupleType->elements.size() ) {
					return tupleType->elements[tupleIndex];
				}
			}
			catch( ... ) {
			}
		}
		else if( objectType->kind == Type::Kind::Enum ) {
			EnumTypeSharedPointer enumType = std::static_pointer_cast<EnumType>( objectType );
			if( expression.member == "name" ) {
				return this->typeRegistry.lookupPrimitive( "String" );
			}
			if( expression.member == "value" ) {
				return enumType->backedType ? enumType->backedType : this->typeRegistry.getInteger32();
			}
			if( enumType->variants.empty() && enumType->astDeclaration != nullptr ) {
				this->analyzeEnumDeclaration( *enumType->astDeclaration );
			}
			if( enumType->findVariant( expression.member ) ) {
				return objectType;
			}
			std::vector<MethodInfo>::iterator methodIterator = std::find_if(
				enumType->methods.begin(),
				enumType->methods.end(),
				[&]( const MethodInfo& methodInformation ) {
					return methodInformation.name == expression.member;
				}
			);
			if( methodIterator != enumType->methods.end() && methodIterator->isProperty ) {
				return this->typeRegistry.getError();
			}
			std::string noEnumMemberErrorMessage = fmt::format( "no variant or property \"{}\" in enum \"{}\"", expression.member, objectType->toString() );
			this->diagnostic.error( expression.source, noEnumMemberErrorMessage );
			return this->typeRegistry.getError();
		}
		if( objectType->kind == Type::Kind::Interface ) {
			InterfaceTypeSharedPointer interfaceType = std::static_pointer_cast<InterfaceType>( objectType );
			std::vector<InterfaceType*> typesToCheck;
			typesToCheck.push_back( interfaceType.get() );
			while( typesToCheck.empty() == false ) {
				InterfaceType* currentInterface = typesToCheck.back();
				typesToCheck.pop_back();
				for( MethodInfo& methodInformation : currentInterface->methods ) {
					if( methodInformation.name == expression.member && methodInformation.isProperty ) {
						FunctionTypeSharedPointer functionType = std::dynamic_pointer_cast<FunctionType>( methodInformation.type );
						if( functionType ) {
							return functionType->returnType;
						}
					}
				}
				for( TypeSharedPointer& superInterfaceType : currentInterface->superInterfaces ) {
					if( superInterfaceType && superInterfaceType->kind == Type::Kind::Interface ) {
						typesToCheck.push_back( std::static_pointer_cast<InterfaceType>( superInterfaceType ).get() );
					}
				}
			}
		}
		if( objectType->kind == Type::Kind::Optional ) {
			OptionalTypeSharedPointer optionalType = std::static_pointer_cast<OptionalType>( objectType );
			if( optionalType->inner ) {
				ast::nodes::MemberAccessExpression unwrappedExpression = expression;
				TypeSharedPointer savedObjectType = objectType;
				TypeSharedPointer innerResult = nullptr;
				if( optionalType->inner->kind == Type::Kind::Class ) {
					ClassTypeSharedPointer classType = std::static_pointer_cast<ClassType>( optionalType->inner );
					FieldInfo* fieldInformation = classType->findField( expression.member );
					if( fieldInformation ) {
						return fieldInformation->type;
					}
					MethodInfo* methodInformation = classType->findMethod( expression.member );
					if( methodInformation && methodInformation->isProperty ) {
						FunctionTypeSharedPointer functionType = std::dynamic_pointer_cast<FunctionType>( methodInformation->type );
						if( functionType ) {
							return functionType->returnType;
						}
					}
				}
				else if( optionalType->inner->kind == Type::Kind::Struct ) {
					StructTypeSharedPointer structType = std::static_pointer_cast<StructType>( optionalType->inner );
					FieldInfo* fieldInformation = structType->findField( expression.member );
					if( fieldInformation ) {
						return fieldInformation->type;
					}
				}
			}
		}
		std::string noMemberErrorMessage = fmt::format( "no member \"{}\" in type \"{}\"", expression.member, objectType->toString() );
		this->diagnostic.error( expression.source, noMemberErrorMessage );
		return this->typeRegistry.getError();
	}
	
	TypeSharedPointer Analyzer::analyzeMethodCallExpression( ast::nodes::MethodCallExpression& expression ) {
		TypeSharedPointer objectType = this->analyzeExpression( expression.object );
		if( objectType == nullptr || objectType->isError() ) {
			return this->typeRegistry.getError();
		}
		for( ast::nodes::ExpressionSharedPointer& argumentExpression : expression.arguments ) {
			this->analyzeExpression( argumentExpression );
		}
		for( ast::nodes::KeywordArgument& kwarg : expression.keywordArguments ) {
			this->analyzeExpression( kwarg.value );
		}
		TypeSharedPointer resolvedObjectType = objectType;
		if( resolvedObjectType->kind == Type::Kind::Optional ) {
			OptionalTypeSharedPointer optionalType = std::static_pointer_cast<OptionalType>( resolvedObjectType );
			resolvedObjectType = optionalType->inner;
		}
		if( resolvedObjectType->kind == Type::Kind::Class ) {
			ClassTypeSharedPointer classType = std::static_pointer_cast<ClassType>( resolvedObjectType );
			MethodInfo* methodInformation = classType->findMethod( expression.method );
			if( methodInformation == nullptr && classType->astDeclaration ) {
				ClassTypeSharedPointer baseClassType = std::dynamic_pointer_cast<ClassType>( this->typeRegistry.lookupType( classType->astDeclaration->name ) );
				if( baseClassType ) {
					methodInformation = baseClassType->findMethod( expression.method );
				}
			}
			if( methodInformation && methodInformation->type && methodInformation->type->kind == Type::Kind::Function ) {
				TypeSharedPointer returnType = std::static_pointer_cast<FunctionType>( methodInformation->type )->returnType;
				if( returnType && returnType->kind == Type::Kind::GenericParameter ) {
					GenericParameterTypeSharedPointer genericParameter = std::static_pointer_cast<GenericParameterType>( returnType );
					std::unordered_map<std::string,TypeSharedPointer>::iterator substitutionIterator = classType->typeSubstitutions.find( genericParameter->name );
					if( substitutionIterator != classType->typeSubstitutions.end() ) {
						return substitutionIterator->second;
					}
				}
				return returnType;
			}
		}
		else if( resolvedObjectType->kind == Type::Kind::Struct ) {
			StructTypeSharedPointer structType = std::static_pointer_cast<StructType>( resolvedObjectType );
			for( MethodInfo& methodInformation : structType->methods ) {
				if( methodInformation.name == expression.method && methodInformation.type && methodInformation.type->kind == Type::Kind::Function ) {
					return std::static_pointer_cast<FunctionType>( methodInformation.type )->returnType;
				}
			}
		}
		else if( resolvedObjectType->kind == Type::Kind::Interface ) {
			InterfaceTypeSharedPointer interfaceType = std::static_pointer_cast<InterfaceType>( resolvedObjectType );
			std::vector<InterfaceType*> typesToCheck;
			typesToCheck.push_back( interfaceType.get() );
			while( typesToCheck.empty() == false ) {
				InterfaceType* currentInterface = typesToCheck.back();
				typesToCheck.pop_back();
				for( MethodInfo& methodInformation : currentInterface->methods ) {
					if( methodInformation.name == expression.method && methodInformation.type && methodInformation.type->kind == Type::Kind::Function ) {
						return std::static_pointer_cast<FunctionType>( methodInformation.type )->returnType;
					}
				}
				for( TypeSharedPointer& superInterfaceType : currentInterface->superInterfaces ) {
					if( superInterfaceType && superInterfaceType->kind == Type::Kind::Interface ) {
						typesToCheck.push_back( std::static_pointer_cast<InterfaceType>( superInterfaceType ).get() );
					}
				}
			}
		}
		return this->typeRegistry.getError();
	}
	
	void Analyzer::analyzeReturnStatement( ast::nodes::ReturnStatement& statement ) {
		if( this->currentScope->isInsideFunction() == false ) {
			this->diagnostic.error( statement.source, "'return' outside of function" );
			return;
		}
		if( statement.value ) {
			TypeSharedPointer valueType = this->analyzeExpression( statement.value );
			if( this->currentReturnType && this->currentReturnType->isVoid() ) {
				this->diagnostic.error( statement.source, "cannot return a value from a function with return type 'Void'" );
			}
			else if( this->currentReturnType && valueType && this->typeRegistry.isAssignable( this->currentReturnType, valueType ) == false ) {
				std::string returnTypeMismatchErrorMessage = fmt::format( "return type mismatch: expected \"{}\" but found \"{}\"", this->currentReturnType->toString(), valueType->toString() );
				this->diagnostic.error( statement.source, returnTypeMismatchErrorMessage );
			}
		}
		else if( this->currentReturnType && this->currentReturnType->isVoid() == false ) {
			std::string expectedReturnValueErrorMessage = fmt::format( "function expects return value of type \"{}\"", this->currentReturnType->toString() );
			this->diagnostic.error( statement.source, expectedReturnValueErrorMessage );
		}
	}
	
	void Analyzer::analyzeStatement( ast::nodes::StatementSharedPointer& statement ) {
		if( statement == nullptr ) {
			return;
		}
		switch( statement->kind ) {
			case ast::Node::Kind::VariableStatement:
				this->analyzeVariableStatement( static_cast<ast::nodes::VariableStatement&>( *statement ) );
				break;
			case ast::Node::Kind::AssignmentStatement:
				this->analyzeAssignStatement( static_cast<ast::nodes::AssignStatement&>( *statement ) );
				break;
			case ast::Node::Kind::ReturnStatement:
				this->analyzeReturnStatement( static_cast<ast::nodes::ReturnStatement&>( *statement ) );
				break;
			case ast::Node::Kind::IfStatement:
				this->analyzeIfStatement( static_cast<ast::nodes::IfStatement&>( *statement ) );
				break;
			case ast::Node::Kind::MatchStatement:
				this->analyzeMatchStatement( static_cast<ast::nodes::MatchStatement&>( *statement ) );
				break;
			case ast::Node::Kind::SwitchStatement: {
				ast::nodes::SwitchStatement& switchStatement = static_cast<ast::nodes::SwitchStatement&>( *statement );
				this->analyzeExpression( switchStatement.subject );
				for( ast::nodes::SwitchCaseNodeSharedPointer& switchCase : switchStatement.cases ) {
					if( switchCase->pattern ) {
						this->analyzeExpression( switchCase->pattern );
					}
					this->pushScope( Scope::Kind::Switch );
					for( ast::nodes::StatementSharedPointer& caseStatement : switchCase->body ) {
						this->analyzeStatement( caseStatement );
					}
					this->popScope();
				}
				break;
			}
			case ast::Node::Kind::ForStatement:
				this->analyzeForStatement( static_cast<ast::nodes::ForStatement&>( *statement ) );
				break;
			case ast::Node::Kind::WhileStatement:
				this->analyzeWhileStatement( static_cast<ast::nodes::WhileStatement&>( *statement ) );
				break;
			case ast::Node::Kind::UnsafeBlock:
				this->analyzeUnsafeBlockStatement( static_cast<ast::nodes::UnsafeBlockStatement&>( *statement ) );
				break;
			case ast::Node::Kind::ExpressionStatement: {
				ast::nodes::ExpressionStatement& expressionStatement = static_cast<ast::nodes::ExpressionStatement&>( *statement );
				this->analyzeExpression( expressionStatement.expression );
				break;
			}
			case ast::Node::Kind::BreakStatement:
				if( this->currentScope->isInsideLoop() == false ) {
					this->diagnostic.error( statement->source, "'break' outside of loop or switch" );
				}
				break;
			case ast::Node::Kind::ContinueStatement:
				if( this->currentScope->isInsideLoop() == false ) {
					this->diagnostic.error( statement->source, "'continue' outside of loop" );
				}
				break;
			case ast::Node::Kind::PassStatement:
				break;
			case ast::Node::Kind::InlineAssemblyStatement:
				break;
			case ast::Node::Kind::DeferStatement: {
				ast::nodes::DeferStatement& deferStatement = static_cast<ast::nodes::DeferStatement&>( *statement );
				this->analyzeStatement( deferStatement.body );
				break;
			}
			case ast::Node::Kind::DeleteStatement: {
				ast::nodes::DeleteStatement& deleteStatement = static_cast<ast::nodes::DeleteStatement&>( *statement );
				this->analyzeExpression( deleteStatement.expression );
				break;
			}
			case ast::Node::Kind::TryCatchStatement: {
				ast::nodes::TryCatchStatement& tryCatchStatement = static_cast<ast::nodes::TryCatchStatement&>( *statement );
				bool savedTryState = this->isInsideTryBlock;
				this->isInsideTryBlock = true;
				for( ast::nodes::StatementSharedPointer& tryStatement : tryCatchStatement.tryBody ) {
					this->analyzeStatement( tryStatement );
				}
				this->isInsideTryBlock = savedTryState;
				for( ast::nodes::ExceptionClause& exceptClause : tryCatchStatement.exceptionClauses ) {
					this->pushScope( Scope::Kind::Block );
					for( ast::nodes::TypeNodeSharedPointer& exceptionTypeNode : exceptClause.exceptionTypes ) {
						this->resolveType( exceptionTypeNode );
					}
					if( exceptClause.variableName.empty() == false ) {
						TypeSharedPointer exceptionVariableType = this->typeRegistry.getString();
						if( exceptClause.exceptionTypes.empty() == false ) {
							TypeSharedPointer resolvedExceptionType = this->resolveType( exceptClause.exceptionTypes[0] );
							if( resolvedExceptionType ) {
								exceptionVariableType = resolvedExceptionType;
							}
						}
						SymbolSharedPointer exceptionSymbol = std::make_shared<Symbol>( exceptClause.variableName, Symbol::Kind::Variable, exceptClause.source, exceptionVariableType );
						exceptionSymbol->isInitialized = true;
						exceptionSymbol->isMutable = false;
						this->currentScope->define( exceptClause.variableName, exceptionSymbol );
					}
					for( ast::nodes::StatementSharedPointer& exceptStatement : exceptClause.body ) {
						this->analyzeStatement( exceptStatement );
					}
					this->popScope();
				}
				for( ast::nodes::StatementSharedPointer& finallyStatement : tryCatchStatement.finallyBody ) {
					this->analyzeStatement( finallyStatement );
				}
				break;
			}
			case ast::Node::Kind::ThrowStatement: {
				ast::nodes::ThrowStatement& throwStatement = static_cast<ast::nodes::ThrowStatement&>( *statement );
				TypeSharedPointer expressionType = this->analyzeExpression( throwStatement.expression );
				if( expressionType && expressionType->isError() == false ) {
					bool isThrowable = false;
					if( expressionType->kind == Type::Kind::String ) {
						isThrowable = true;
					}
					else if( expressionType->kind == Type::Kind::Class ) {
						ClassTypeSharedPointer classType = std::static_pointer_cast<ClassType>( expressionType );
						for( TypeSharedPointer& interfaceType : classType->interfaces ) {
							if( interfaceType && interfaceType->qualified == qname::THROWABLE ) {
								isThrowable = true;
								break;
							}
						}
						TypeSharedPointer baseType = classType->baseClass;
						while( baseType && isThrowable == false && baseType->kind == Type::Kind::Class ) {
							ClassTypeSharedPointer baseClass = std::static_pointer_cast<ClassType>( baseType );
							for( TypeSharedPointer& interfaceType : baseClass->interfaces ) {
								if( interfaceType && interfaceType->qualified == qname::THROWABLE ) {
									isThrowable = true;
									break;
								}
							}
							baseType = baseClass->baseClass;
						}
						if( isThrowable == false ) {
							std::string notThrowableErrorMessage = fmt::format( "only types implementing Throwable can be raised; \"{}\" does not implement Throwable", expressionType->name );
							this->diagnostic.error( throwStatement.source, notThrowableErrorMessage );
						}
					}
				}
				break;
			}
			default:
				break;
		}
	}
	
	void Analyzer::analyzeStructDeclaration( ast::nodes::StructDeclaration& declaration ) {
		StructTypeSharedPointer structType = std::dynamic_pointer_cast<StructType>( this->typeRegistry.lookupType( declaration.name ) );
		if( structType == nullptr ) {
			return;
		}
		for( std::string& traitName : declaration.usedTraits ) {
			TraitTypeSharedPointer traitType = std::dynamic_pointer_cast<TraitType>( this->typeRegistry.lookupType( traitName ) );
			if( traitType == nullptr ) {
				std::string unknownTraitErrorMessage = fmt::format( "unknown trait \"{}\"", traitName );
				this->diagnostic.error( declaration.source, unknownTraitErrorMessage );
				continue;
			}
			if( traitType->astDeclaration ) {
				for( ast::nodes::FieldDeclarationSharedPointer& traitField : traitType->astDeclaration->fields ) {
					bool fieldExists = false;
					for( ast::nodes::FieldDeclarationSharedPointer& structField : declaration.fields ) {
						if( structField->name == traitField->name ) {
							fieldExists = true;
							break;
						}
					}
					if( fieldExists == false ) {
						declaration.fields.push_back( traitField );
					}
				}
				for( ast::nodes::DeclarationSharedPointer& traitMethod : traitType->astDeclaration->methods ) {
					ast::nodes::FunctionDeclaration& traitFunction = static_cast<ast::nodes::FunctionDeclaration&>( *traitMethod );
					bool methodExists = false;
					for( ast::nodes::DeclarationSharedPointer& structMethod : declaration.methods ) {
						if( structMethod->kind == ast::Node::Kind::FunctionDeclaration &&
							static_cast<ast::nodes::FunctionDeclaration&>( *structMethod ).name == traitFunction.name ) {
							methodExists = true;
							break;
						}
					}
					if( methodExists == false ) {
						declaration.methods.push_back( traitMethod );
					}
				}
			}
		}
		this->pushScope( Scope::Kind::Class );
		this->currentScope->classType = structType;
		if( structType->genericParameters.empty() ) {
			for( ast::nodes::GenericParameterSharedPointer& genericParameter : declaration.genericParameters ) {
				structType->genericParameters.push_back( std::make_shared<GenericParameterType>( genericParameter->name ) );
			}
		}
		std::unordered_map<std::string, TypeSharedPointer> savedGenericParams;
		for( TypeSharedPointer& parameter : structType->genericParameters ) {
			TypeSharedPointer existing = this->typeRegistry.lookupType( parameter->name );
			if( existing != nullptr ) {
				savedGenericParams[parameter->name] = existing;
			}
			this->typeRegistry.registerType( parameter->name, parameter );
		}
		structType->fields.clear();
		int currentFieldIndex = 0;
		for( ast::nodes::FieldDeclarationSharedPointer& fieldDeclaration : declaration.fields ) {
			TypeSharedPointer fieldType = this->resolveType( fieldDeclaration->type );
			FieldInfo fieldInformation;
			fieldInformation.name = fieldDeclaration->name;
			fieldInformation.type = fieldType ? fieldType : this->typeRegistry.getError();
			fieldInformation.access = fieldDeclaration->access;
			fieldInformation.isStatic = fieldDeclaration->isStatic;
			fieldInformation.index = currentFieldIndex++;
			structType->fields.push_back( fieldInformation );
		}
		for( ast::nodes::DeclarationSharedPointer& methodDeclaration : declaration.methods ) {
			if( methodDeclaration->kind == ast::Node::Kind::FunctionDeclaration ) {
				ast::nodes::FunctionDeclaration& functionMethod = static_cast<ast::nodes::FunctionDeclaration&>( *methodDeclaration );
				if( functionMethod.isStatic == false ) {
					bool hasSelf = false;
					for( ast::nodes::FunctionParameterSharedPointer& parameter : functionMethod.parameters ) {
						if( parameter->isSelf ) { hasSelf = true; break; }
					}
					if( hasSelf == false ) {
						std::string selfErrorMessage = fmt::format( "non-static method \"{}\" in struct \"{}\" must have 'self' as first parameter, or be declared 'static'", functionMethod.name, declaration.name );
						this->diagnostic.error( functionMethod.source, selfErrorMessage );
					}
				}
				else {
					for( ast::nodes::FunctionParameterSharedPointer& parameter : functionMethod.parameters ) {
						if( parameter->isSelf ) {
							std::string selfErrorMessage = fmt::format( "static method \"{}\" in struct \"{}\" must not have 'self' parameter", functionMethod.name, declaration.name );
							this->diagnostic.error( parameter->source, selfErrorMessage );
							break;
						}
					}
				}
			}
			this->analyzeDeclaration( methodDeclaration );
			if( methodDeclaration->kind == ast::Node::Kind::FunctionDeclaration ) {
				ast::nodes::FunctionDeclaration& functionMethod = static_cast<ast::nodes::FunctionDeclaration&>( *methodDeclaration );
				std::vector<TypeSharedPointer> methodParameterTypes;
				std::vector<std::string> methodParameterNames;
				for( ast::nodes::FunctionParameterSharedPointer& parameter : functionMethod.parameters ) {
					if( parameter->isSelf ) {
						continue;
					}
					TypeSharedPointer parameterType = this->resolveType( parameter->type );
					methodParameterTypes.push_back( parameterType ? parameterType : this->typeRegistry.getError() );
					methodParameterNames.push_back( parameter->name );
				}
				TypeSharedPointer methodReturnType = functionMethod.returnType ? this->resolveType( functionMethod.returnType ) : this->typeRegistry.getVoid();
				TypeSharedPointer methodFunctionType = this->typeRegistry.makeFunction( methodParameterTypes, methodReturnType );
				std::static_pointer_cast<FunctionType>( methodFunctionType )->parameterNames = std::move( methodParameterNames );
				MethodInfo methodInformation;
				methodInformation.name = functionMethod.name;
				methodInformation.type = methodFunctionType;
				methodInformation.access = functionMethod.access;
				methodInformation.isVirtual = functionMethod.isVirtual;
				methodInformation.isOverride = functionMethod.isOverride;
				methodInformation.isStatic = functionMethod.isStatic;
				methodInformation.isFinal = functionMethod.isFinal;
				methodInformation.isProperty = functionMethod.isProperty;
				methodInformation.virtualTableIndex = -1;
				structType->methods.push_back( methodInformation );
			}
		}
		for( ast::nodes::DeclarationSharedPointer& nestedDeclaration : declaration.nestedDeclarations ) {
			this->analyzeDeclaration( nestedDeclaration );
		}
		for( std::unordered_map<std::string, TypeSharedPointer>::iterator savedIterator = savedGenericParams.begin(); savedIterator != savedGenericParams.end(); ++savedIterator ) {
			this->typeRegistry.registerType( savedIterator->first, savedIterator->second );
		}
		this->popScope();
	}
	
	void Analyzer::analyzeTraitDeclaration( ast::nodes::TraitDeclaration& declaration ) {
		TraitTypeSharedPointer traitType = std::dynamic_pointer_cast<TraitType>( this->typeRegistry.lookupType( declaration.name ) );
		if( traitType == nullptr ) {
			return;
		}
		for( ast::nodes::GenericParameterSharedPointer& genericParameter : declaration.genericParameters ) {
			GenericParameterTypeSharedPointer genericParameterType = std::make_shared<GenericParameterType>( genericParameter->name );
			traitType->genericParameters.push_back( genericParameterType );
		}
		for( ast::nodes::FieldDeclarationSharedPointer& fieldDeclaration : declaration.fields ) {
			TypeSharedPointer fieldType = this->resolveType( fieldDeclaration->type );
			FieldInfo fieldInformation;
			fieldInformation.name = fieldDeclaration->name;
			fieldInformation.type = fieldType ? fieldType : this->typeRegistry.getError();
			fieldInformation.access = fieldDeclaration->access;
			fieldInformation.index = static_cast<int>( traitType->fields.size() );
			traitType->fields.push_back( fieldInformation );
		}
		for( ast::nodes::DeclarationSharedPointer& methodDeclaration : declaration.methods ) {
			if( methodDeclaration->kind == ast::Node::Kind::FunctionDeclaration ) {
				ast::nodes::FunctionDeclaration& functionDeclaration = static_cast<ast::nodes::FunctionDeclaration&>( *methodDeclaration );
				std::vector<TypeSharedPointer> parameterTypes;
				for( ast::nodes::FunctionParameterSharedPointer& parameter : functionDeclaration.parameters ) {
					if( parameter->isSelf ) {
						continue;
					}
					TypeSharedPointer parameterType = this->resolveType( parameter->type );
					parameterTypes.push_back( parameterType ? parameterType : this->typeRegistry.getError() );
				}
				TypeSharedPointer returnType = functionDeclaration.returnType ? this->resolveType( functionDeclaration.returnType ) : this->typeRegistry.getVoid();
				TypeSharedPointer functionType = this->typeRegistry.makeFunction( parameterTypes, returnType );
				MethodInfo methodInformation;
				methodInformation.name = functionDeclaration.name;
				methodInformation.type = functionType;
				methodInformation.access = functionDeclaration.access;
				methodInformation.isVirtual = false;
				methodInformation.isOverride = false;
				methodInformation.isStatic = functionDeclaration.isStatic;
				methodInformation.isFinal = false;
				methodInformation.virtualTableIndex = -1;
				traitType->methods.push_back( methodInformation );
			}
		}
	}
	
	void Analyzer::analyzeTypeAliasDeclaration( ast::nodes::TypeAliasDeclaration& declaration ) {
		TypeSharedPointer resolvedType = this->resolveType( declaration.aliasedTypeNode );
		if( resolvedType ) {
			this->typeRegistry.registerType( declaration.name, resolvedType );
		}
	}
	
	TypeSharedPointer Analyzer::analyzeUnaryExpression( ast::nodes::UnaryExpression& expression ) {
		TypeSharedPointer operandType = this->analyzeExpression( expression.operand );
		if( operandType == nullptr ) {
			return this->typeRegistry.getError();
		}
		switch( expression.operation ) {
			case token::Type::Minus: {
				static const std::set<std::string> numericOopNames = {
					"Int", "I8", "I16", "I32", "I64",
					"UInt", "U8", "U16", "U32", "U64",
					"Float", "F32", "F64", "Double",
					"Integer", "Long", "Byte"
				};
				if( operandType->isNumeric() || numericOopNames.count( operandType->name ) > 0 ) {
					return operandType;
				}
				if( operandType->kind == Type::Kind::Class || operandType->kind == Type::Kind::Struct ) {
					ClassTypeSharedPointer classType = std::dynamic_pointer_cast<ClassType>( operandType );
					if( classType && classType->findMethod( "negate" ) ) {
						return operandType;
					}
				}
				std::string cannotNegateErrorMessage = fmt::format( "cannot negate type \"{}\"", operandType->toString() );
				this->diagnostic.error( expression.source, cannotNegateErrorMessage );
				return this->typeRegistry.getError();
			}
			case token::Type::KeywordNot:
			case token::Type::Bang: {
				semantic::TypeSharedPointer boolCheckType = this->typeRegistry.getBool();
				if( operandType->isBool() == false && this->typeRegistry.isAssignable( boolCheckType, operandType ) == false ) {
					this->diagnostic.error( expression.source, "logical not requires boolean operand" );
					return this->typeRegistry.getError();
				}
				return this->typeRegistry.getBool();
			}
			case token::Type::Tilde: {
				if( operandType->isIntegral() == false ) {
					this->diagnostic.error( expression.source, "bitwise not requires integer operand" );
					return this->typeRegistry.getError();
				}
				return operandType;
			}
			case token::Type::KeywordAddressof: {
				return this->typeRegistry.lookupType( "I64" );
			}
			case token::Type::Ampersand: {
				return this->typeRegistry.makeReference( operandType );
			}
			case token::Type::Star: {
				if( operandType->kind == Type::Kind::Pointer ) {
					if( this->currentScope->isInsideUnsafe() == false ) {
						this->diagnostic.error( expression.source, "dereferencing raw pointer requires 'unsafe' block" );
					}
					return std::static_pointer_cast<PointerType>( operandType )->inner;
				}
				if( operandType->kind == Type::Kind::Reference ) {
					return std::static_pointer_cast<ReferenceType>( operandType )->inner;
				}
				std::string cannotDereferenceErrorMessage = fmt::format( "cannot dereference type \"{}\"", operandType->toString() );
				this->diagnostic.error( expression.source, cannotDereferenceErrorMessage );
				return this->typeRegistry.getError();
			}
			default:
				return operandType;
		}
	}
	
	void Analyzer::analyzeUnsafeBlockStatement( ast::nodes::UnsafeBlockStatement& statement ) {
		this->pushScope( Scope::Kind::Unsafe );
		for( ast::nodes::StatementSharedPointer& blockStatement : statement.body ) {
			this->analyzeStatement( blockStatement );
		}
		this->popScope();
	}
	
	void Analyzer::analyzeVariableStatement( ast::nodes::VariableStatement& statement ) {
		TypeSharedPointer variableType;
		if( statement.type ) {
			variableType = this->resolveType( statement.type );
		}
		if( statement.initializer ) {
			TypeSharedPointer initializerType = this->analyzeExpression( statement.initializer );
			if( variableType && variableType->kind == Type::Kind::Meta && initializerType && initializerType->kind != Type::Kind::Meta ) {
				initializerType = this->typeRegistry.makeMeta( initializerType );
			}
			if( variableType == nullptr ) {
				variableType = initializerType;
			}
			else if( initializerType && this->typeRegistry.isAssignable( variableType, initializerType ) == false ) {
				std::string typeMismatchErrorMessage = fmt::format( "cannot assign value of type \"{}\" to variable of type \"{}\"", initializerType->toString(), variableType->toString() );
				this->diagnostic.error( statement.source, typeMismatchErrorMessage );
			}
		}
		if( variableType == nullptr ) {
			variableType = this->typeRegistry.getError();
		}
		SymbolSharedPointer variableSymbol = std::make_shared<Symbol>( statement.name, Symbol::Kind::Variable, statement.source, variableType );
		variableSymbol->isMutable = statement.isMutable;
		variableSymbol->isInitialized = statement.initializer != nullptr;
		if( this->currentScope->define( statement.name, variableSymbol ) == false ) {
			std::string redefinitionErrorMessage = fmt::format( "redefinition of variable \"{}\"", statement.name );
			this->diagnostic.error( statement.source, redefinitionErrorMessage );
		}
	}
	
	void Analyzer::analyzeWhileStatement( ast::nodes::WhileStatement& statement ) {
		TypeSharedPointer conditionType = this->analyzeExpression( statement.condition );
		if( conditionType && conditionType->isBool() == false && conditionType->isError() == false ) {
			this->diagnostic.error( statement.condition->source, "condition must be of type 'bool'" );
		}
		this->pushScope( Scope::Kind::Loop );
		for( ast::nodes::StatementSharedPointer& bodyStatement : statement.body ) {
			this->analyzeStatement( bodyStatement );
		}
		this->popScope();
	}
	
	void Analyzer::buildVTable( ClassTypeSharedPointer classType ) {
		if( classType->baseClass && classType->baseClass->kind == Type::Kind::Class ) {
			ClassTypeSharedPointer baseClass = std::static_pointer_cast<ClassType>( classType->baseClass );
			classType->virtualTable = baseClass->virtualTable;
		}
		for( MethodInfo& methodInformation : classType->methods ) {
			if( methodInformation.isVirtual == false && methodInformation.isOverride == false ) {
				continue;
			}
			bool methodFoundInVTable = false;
			for( size_t vtableIndex = 0; vtableIndex < classType->virtualTable.size(); vtableIndex++ ) {
				if( classType->virtualTable[vtableIndex]->name == methodInformation.name ) {
					classType->virtualTable[vtableIndex] = &methodInformation;
					methodInformation.virtualTableIndex = static_cast<int>( vtableIndex );
					methodFoundInVTable = true;
					break;
				}
			}
			if( methodFoundInVTable == false ) {
				methodInformation.virtualTableIndex = static_cast<int>( classType->virtualTable.size() );
				classType->virtualTable.push_back( &methodInformation );
			}
		}
	}
	
	std::string Analyzer::buildSnippetForFunction( const FunctionTypeSharedPointer& funcType ) {
		std::string snippet = "(";
		int tabStop = 1;
		for( size_t i = 0; i < funcType->parameterTypes.size(); i++ ) {
			if( i > 0 ) {
				snippet+= ", ";
			}
			std::string paramName;
			if( i < funcType->parameterNames.size() && funcType->parameterNames[i].empty() == false ) {
				paramName = funcType->parameterNames[i];
			}
			else {
				paramName = funcType->parameterTypes[i]->toString();
			}
			snippet+= fmt::format( "${{{}:{}}}", tabStop, paramName );
			tabStop++;
		}
		snippet+= ")$0";
		return snippet;
	}
	
	void Analyzer::collectScopeCompletions( const std::string& prefix, const lookup::SourceSharedPointer& triggerSource ) {
		std::unordered_set<std::string> seenLabels;
		int depth = 0;
		ScopeSharedPointer scope = this->currentScope;
		while( scope ) {
			int priority;
			if( depth == 0 ) {
				priority = 0;
			}
			else if( scope->kind() == Scope::Kind::Global ) {
				priority = 2;
			}
			else {
				priority = 1;
			}
			for( const std::pair<const std::string, std::vector<SymbolSharedPointer>>& symbolEntry : scope->symbols() ) {
				const std::string& symbolName = symbolEntry.first;
				if( seenLabels.count( symbolName ) > 0 ) continue;
				if( symbolEntry.second.empty() ) continue;
				seenLabels.insert( symbolName );
				const SymbolSharedPointer& symbol = symbolEntry.second.front();
				std::string typeString = symbol->typeref ? symbol->typeref->toString() : "";
				CompletionItem item( symbolName, symbol->kind, typeString );
				item.sortPriority = priority;
				if( triggerSource != nullptr && triggerSource->location != nullptr ) {
					item.editRangeLine = triggerSource->location->line;
					item.editRangeStartColumn = triggerSource->location->column;
					item.editRangeEndColumn = triggerSource->location->column + static_cast<uint32_t>( prefix.size() );
				}
				if( symbol->kind == Symbol::Kind::Function && symbol->typeref != nullptr
					&& symbol->typeref->kind == Type::Kind::Function ) {
					FunctionTypeSharedPointer funcType = std::static_pointer_cast<FunctionType>( symbol->typeref );
					item.insertText = symbolName + this->buildSnippetForFunction( funcType );
					item.insertTextFormat = 2;
				}
				this->completionItems.push_back( std::move( item ) );
			}
			scope = scope->parent();
			depth++;
		}
		for( const std::string& typeName : this->typeRegistry.typeNames() ) {
			if( seenLabels.count( typeName ) > 0 ) continue;
			seenLabels.insert( typeName );
			CompletionItem item( typeName, Symbol::Kind::Type );
			item.sortPriority = 3;
			if( triggerSource != nullptr && triggerSource->location != nullptr ) {
				item.editRangeLine = triggerSource->location->line;
				item.editRangeStartColumn = triggerSource->location->column;
				item.editRangeEndColumn = triggerSource->location->column + static_cast<uint32_t>( prefix.size() );
			}
			this->completionItems.push_back( std::move( item ) );
		}
	}
	
	void Analyzer::collectTypeCompletions( TypeSharedPointer type, const lookup::SourceSharedPointer& triggerSource ) {
		if( type == nullptr ) return;
		bool isInsideTargetClass = this->currentScope->isInsideClass()
			&& this->currentScope->classType != nullptr;
		std::unordered_set<std::string> seenLabels;
		std::function<void( const MethodInfo& )> addMethodItem = [&]( const MethodInfo& method ) {
			if( seenLabels.count( method.name ) > 0 ) return;
			if( method.access == ast::AccessModifier::Private && isInsideTargetClass == false ) return;
			if( method.access == ast::AccessModifier::Protect && isInsideTargetClass == false ) return;
			seenLabels.insert( method.name );
			CompletionItem item( method.name, Symbol::Kind::Function, method.type ? method.type->toString() : "" );
			item.sortPriority = 0;
			if( method.isProperty == false && method.type != nullptr
				&& method.type->kind == Type::Kind::Function ) {
				FunctionTypeSharedPointer funcType = std::static_pointer_cast<FunctionType>( method.type );
				item.insertText = method.name + this->buildSnippetForFunction( funcType );
				item.insertTextFormat = 2;
			}
			if( triggerSource != nullptr && triggerSource->location != nullptr ) {
				item.editRangeLine = triggerSource->location->line;
				item.editRangeStartColumn = triggerSource->location->column;
				item.editRangeEndColumn = triggerSource->location->column;
			}
			this->completionItems.push_back( std::move( item ) );
		};
		std::function<void( const FieldInfo& )> addFieldItem = [&]( const FieldInfo& field ) {
			if( seenLabels.count( field.name ) > 0 ) return;
			if( field.access == ast::AccessModifier::Private && isInsideTargetClass == false ) return;
			if( field.access == ast::AccessModifier::Protect && isInsideTargetClass == false ) return;
			seenLabels.insert( field.name );
			CompletionItem item( field.name, Symbol::Kind::Field, field.type ? field.type->toString() : "" );
			item.sortPriority = 0;
			if( triggerSource != nullptr && triggerSource->location != nullptr ) {
				item.editRangeLine = triggerSource->location->line;
				item.editRangeStartColumn = triggerSource->location->column;
				item.editRangeEndColumn = triggerSource->location->column;
			}
			this->completionItems.push_back( std::move( item ) );
		};
		if( type->kind == Type::Kind::Class ) {
			ClassTypeSharedPointer classType = std::static_pointer_cast<ClassType>( type );
			for( const FieldInfo& field : classType->fields ) {
				addFieldItem( field );
			}
			for( const MethodInfo& method : classType->methods ) {
				addMethodItem( method );
			}
			if( classType->baseClass ) {
				this->collectTypeCompletions( classType->baseClass, triggerSource );
			}
		}
		else if( type->kind == Type::Kind::Struct ) {
			StructTypeSharedPointer structType = std::static_pointer_cast<StructType>( type );
			for( const FieldInfo& field : structType->fields ) {
				addFieldItem( field );
			}
			for( const MethodInfo& method : structType->methods ) {
				addMethodItem( method );
			}
		}
		else if( type->kind == Type::Kind::Interface ) {
			InterfaceTypeSharedPointer interfaceType = std::static_pointer_cast<InterfaceType>( type );
			for( const MethodInfo& method : interfaceType->methods ) {
				addMethodItem( method );
			}
			for( const TypeSharedPointer& superInterface : interfaceType->superInterfaces ) {
				this->collectTypeCompletions( superInterface, triggerSource );
			}
		}
		else if( type->kind == Type::Kind::Enum ) {
			EnumTypeSharedPointer enumType = std::static_pointer_cast<EnumType>( type );
			for( const EnumVariantInfo& variant : enumType->variants ) {
				if( seenLabels.count( variant.name ) > 0 ) continue;
				seenLabels.insert( variant.name );
				CompletionItem item( variant.name, Symbol::Kind::EnumVariant );
				item.sortPriority = 0;
				if( triggerSource != nullptr && triggerSource->location != nullptr ) {
					item.editRangeLine = triggerSource->location->line;
					item.editRangeStartColumn = triggerSource->location->column;
					item.editRangeEndColumn = triggerSource->location->column;
				}
				this->completionItems.push_back( std::move( item ) );
			}
			for( const MethodInfo& method : enumType->methods ) {
				addMethodItem( method );
			}
		}
	}
	
	void Analyzer::computeMemoryLayout( ClassTypeSharedPointer classType ) {
		int currentMemoryOffset = 0;
		if( classType->virtualTable.empty() == false ) {
			currentMemoryOffset = 8; 
		}
		for( FieldInfo& field : classType->fields ) {
			int fieldSize = 8; 
			int fieldAlignmentRequirement = 8;
			if( field.type ) {
				if( field.type->kind == Type::Kind::Integer ) {
					IntegerTypeSharedPointer integerType = std::static_pointer_cast<IntegerType>( field.type );
					fieldSize = integerType->bitWidth / 8;
					fieldAlignmentRequirement = fieldSize;
				}
				else if( field.type->kind == Type::Kind::Float ) {
					FloatTypeSharedPointer floatType = std::static_pointer_cast<FloatType>( field.type );
					fieldSize = floatType->bitWidth / 8;
					fieldAlignmentRequirement = fieldSize;
				}
				else if( field.type->kind == Type::Kind::Bool ) {
					fieldSize = 1;
					fieldAlignmentRequirement = 1;
				}
				else if( field.type->kind == Type::Kind::Char ) {
					fieldSize = 4;
					fieldAlignmentRequirement = 4;
				}
			}
			currentMemoryOffset = ( currentMemoryOffset + fieldAlignmentRequirement - 1 ) & ~( fieldAlignmentRequirement - 1 );
			currentMemoryOffset+= fieldSize;
		}
		classType->objectSize = ( currentMemoryOffset + classType->alignment - 1 ) & ~( classType->alignment - 1 );
	}
	
	void Analyzer::popScope() {
		if( this->currentScope->parent() ) {
			this->currentScope = this->currentScope->parent();
		}
	}
	
	void Analyzer::populateClassMembers( ClassTypeSharedPointer classType ) {
		if( classType == nullptr || classType->astDeclaration == nullptr ) {
			return;
		}
		if( classType->fields.empty() == false ) {
			return;
		}
		ast::nodes::ClassDeclaration* classAst = classType->astDeclaration;
		if( classType->genericParameters.empty() ) {
			for( ast::nodes::GenericParameterSharedPointer& genericParameter : classAst->genericParameters ) {
				classType->genericParameters.push_back( std::make_shared<GenericParameterType>( genericParameter->name ) );
			}
		}
		std::unordered_map<std::string, TypeSharedPointer> savedGenericParams;
		for( TypeSharedPointer& parameter : classType->genericParameters ) {
			TypeSharedPointer existing = this->typeRegistry.lookupType( parameter->name );
			if( existing != nullptr ) {
				savedGenericParams[parameter->name] = existing;
			}
			this->typeRegistry.registerType( parameter->name, parameter );
		}
		if( classAst->baseClassType ) {
			TypeSharedPointer resolvedBaseType = this->resolveType( classAst->baseClassType );
			if( resolvedBaseType && resolvedBaseType->kind == Type::Kind::Class ) {
				classType->baseClass = resolvedBaseType;
			}
		}
		for( ast::nodes::TypeNodeSharedPointer& interfaceNode : classAst->interfaces ) {
			TypeSharedPointer interfaceType = this->resolveType( interfaceNode );
			if( interfaceType && interfaceType->kind == Type::Kind::Interface ) {
				classType->interfaces.push_back( interfaceType );
			}
		}
		int currentFieldIndex = 0;
		if( classType->baseClass && classType->baseClass->kind == Type::Kind::Class ) {
			std::vector<ClassTypeSharedPointer> ancestors;
			ClassTypeSharedPointer baseTracker = std::static_pointer_cast<ClassType>( classType->baseClass );
			while( baseTracker ) {
				if( baseTracker->fields.empty() && baseTracker->astDeclaration ) {
					this->populateClassMembers( baseTracker );
				}
				ancestors.push_back( baseTracker );
				if( baseTracker->baseClass && baseTracker->baseClass->kind == Type::Kind::Class ) {
					baseTracker = std::static_pointer_cast<ClassType>( baseTracker->baseClass );
				}
				else {
					break;
				}
			}
			std::unordered_set<std::string> addedFields;
			for( std::vector<ClassTypeSharedPointer>::reverse_iterator ancestorIterator = ancestors.rbegin(); ancestorIterator != ancestors.rend(); ++ancestorIterator ) {
				for( FieldInfo& inheritedField : ( *ancestorIterator )->fields ) {
					if( addedFields.insert( inheritedField.name ).second ) {
						FieldInfo copiedField = inheritedField;
						copiedField.index = currentFieldIndex++;
						classType->fields.push_back( copiedField );
					}
				}
			}
		}
		for( ast::nodes::FieldDeclarationSharedPointer& fieldDeclaration : classAst->fields ) {
			TypeSharedPointer fieldType = this->resolveType( fieldDeclaration->type );
			FieldInfo fieldInformation;
			fieldInformation.name = fieldDeclaration->name;
			fieldInformation.type = fieldType ? fieldType : this->typeRegistry.getError();
			fieldInformation.access = fieldDeclaration->access;
			fieldInformation.index = currentFieldIndex++;
			classType->fields.push_back( fieldInformation );
		}
		for( ast::nodes::DeclarationSharedPointer& methodNode : classAst->methods ) {
			if( methodNode == nullptr || methodNode->kind != ast::Node::Kind::FunctionDeclaration ) {
				continue;
			}
			ast::nodes::FunctionDeclaration& functionDecl = static_cast<ast::nodes::FunctionDeclaration&>( *methodNode );
			std::vector<TypeSharedPointer> methodParameterTypes;
			std::vector<std::string> methodParamNames;
			for( ast::nodes::FunctionParameterSharedPointer& parameter : functionDecl.parameters ) {
				if( parameter->isSelf ) {
					continue;
				}
				TypeSharedPointer parameterType = this->resolveType( parameter->type );
				methodParameterTypes.push_back( parameterType ? parameterType : this->typeRegistry.getError() );
				methodParamNames.push_back( parameter->name );
			}
			TypeSharedPointer returnType = functionDecl.returnType ? this->resolveType( functionDecl.returnType ) : this->typeRegistry.getVoid();
			TypeSharedPointer methodFuncType = this->typeRegistry.makeFunction( methodParameterTypes, returnType );
			std::static_pointer_cast<FunctionType>( methodFuncType )->parameterNames = std::move( methodParamNames );
			MethodInfo methodInformation;
			methodInformation.name = functionDecl.name;
			methodInformation.type = methodFuncType;
			methodInformation.access = functionDecl.access;
			methodInformation.isVirtual = functionDecl.isVirtual;
			methodInformation.isOverride = functionDecl.isOverride;
			methodInformation.isStatic = functionDecl.isStatic;
			methodInformation.isFinal = functionDecl.isFinal;
			methodInformation.isProperty = functionDecl.isProperty;
			methodInformation.virtualTableIndex = -1;
			classType->methods.push_back( methodInformation );
		}
		for( std::unordered_map<std::string, TypeSharedPointer>::iterator savedIterator = savedGenericParams.begin(); savedIterator != savedGenericParams.end(); ++savedIterator ) {
			this->typeRegistry.registerType( savedIterator->first, savedIterator->second );
		}
	}
	
	void Analyzer::populateInterfaceMethods( InterfaceTypeSharedPointer interfaceType ) {
		if( interfaceType == nullptr || interfaceType->astDeclaration == nullptr ) {
			return;
		}
		if( interfaceType->inheritanceResolved || interfaceType->methodsPopulating ) {
			return;
		}
		interfaceType->methodsPopulating = true;
		ast::nodes::InterfaceDeclaration* interfaceAst = interfaceType->astDeclaration;
		if( interfaceType->genericParameters.empty() ) {
			for( ast::nodes::GenericParameterSharedPointer& genericParameter : interfaceAst->genericParameters ) {
				interfaceType->genericParameters.push_back( std::make_shared<GenericParameterType>( genericParameter->name ) );
			}
		}
		std::unordered_map<std::string, TypeSharedPointer> savedGenericParams;
		for( TypeSharedPointer& parameter : interfaceType->genericParameters ) {
			TypeSharedPointer existing = this->typeRegistry.lookupType( parameter->name );
			if( existing != nullptr ) {
				savedGenericParams[parameter->name] = existing;
			}
			this->typeRegistry.registerType( parameter->name, parameter );
		}
		if( interfaceType->superInterfaces.empty() ) {
			for( ast::nodes::TypeNodeSharedPointer& superNode : interfaceAst->superInterfaces ) {
				TypeSharedPointer superType = this->resolveType( superNode );
				if( superType && superType->kind == Type::Kind::Interface ) {
					interfaceType->superInterfaces.push_back( superType );
				}
			}
		}
		if( interfaceType->methods.empty() ) {
			for( ast::nodes::DeclarationSharedPointer& methodNode : interfaceAst->methods ) {
				if( methodNode->kind != ast::Node::Kind::FunctionDeclaration ) {
					continue;
				}
				ast::nodes::FunctionDeclaration& functionDecl = static_cast<ast::nodes::FunctionDeclaration&>( *methodNode );
				for( TypeSharedPointer& parameter : interfaceType->genericParameters ) {
					this->typeRegistry.registerType( parameter->name, parameter );
				}
				std::vector<TypeSharedPointer> methodParameterTypes;
				std::vector<std::string> methodParamNames;
				for( ast::nodes::FunctionParameterSharedPointer& parameter : functionDecl.parameters ) {
					if( parameter->isSelf ) {
						continue;
					}
					TypeSharedPointer parameterType = this->resolveType( parameter->type );
					methodParameterTypes.push_back( parameterType ? parameterType : this->typeRegistry.getError() );
					methodParamNames.push_back( parameter->name );
				}
				TypeSharedPointer returnType = functionDecl.returnType ? this->resolveType( functionDecl.returnType ) : this->typeRegistry.getVoid();
				TypeSharedPointer methodFuncType = this->typeRegistry.makeFunction( methodParameterTypes, returnType );
				std::static_pointer_cast<FunctionType>( methodFuncType )->parameterNames = std::move( methodParamNames );
				MethodInfo methodInformation;
				methodInformation.name = functionDecl.name;
				methodInformation.type = methodFuncType;
				methodInformation.access = functionDecl.access;
				methodInformation.isVirtual = true;
				methodInformation.isProperty = functionDecl.isProperty;
				methodInformation.isStatic = functionDecl.isStatic;
				methodInformation.virtualTableIndex = -1;
				interfaceType->methods.push_back( methodInformation );
			}
		}
		for( TypeSharedPointer& superInterface : interfaceType->superInterfaces ) {
			if( superInterface && superInterface->kind == Type::Kind::Interface ) {
				InterfaceTypeSharedPointer superIfacePtr = std::static_pointer_cast<InterfaceType>( superInterface );
				this->populateInterfaceMethods( superIfacePtr );
				for( MethodInfo& superMethod : superIfacePtr->methods ) {
					bool isDuplicate = false;
					for( MethodInfo& existingMethod : interfaceType->methods ) {
						if( existingMethod.name == superMethod.name ) {
							isDuplicate = true;
							break;
						}
					}
					if( isDuplicate == false ) {
						interfaceType->methods.push_back( superMethod );
					}
				}
			}
		}
		for( std::unordered_map<std::string, TypeSharedPointer>::iterator savedIterator = savedGenericParams.begin(); savedIterator != savedGenericParams.end(); ++savedIterator ) {
			this->typeRegistry.registerType( savedIterator->first, savedIterator->second );
		}
		interfaceType->methodOrder.clear();
		for( int methodIndex = 0; methodIndex < static_cast<int>( interfaceType->methods.size() ); methodIndex++ ) {
			interfaceType->methods[methodIndex].interfaceTableIndex = methodIndex;
			interfaceType->methodOrder.push_back( interfaceType->methods[methodIndex].name );
		}
		interfaceType->inheritanceResolved = true;
		interfaceType->methodsPopulating = false;
	}
	
	void Analyzer::populateStructFields( StructTypeSharedPointer structType ) {
		if( structType == nullptr || structType->astDeclaration == nullptr ) {
			return;
		}
		if( structType->fields.empty() == false ) {
			return;
		}
		ast::nodes::StructDeclaration* structAst = structType->astDeclaration;
		if( structType->genericParameters.empty() ) {
			for( ast::nodes::GenericParameterSharedPointer& genericParameter : structAst->genericParameters ) {
				structType->genericParameters.push_back( std::make_shared<GenericParameterType>( genericParameter->name ) );
			}
		}
		std::unordered_map<std::string, TypeSharedPointer> savedGenericParams;
		for( TypeSharedPointer& parameter : structType->genericParameters ) {
			TypeSharedPointer existing = this->typeRegistry.lookupType( parameter->name );
			if( existing != nullptr ) {
				savedGenericParams[parameter->name] = existing;
			}
			this->typeRegistry.registerType( parameter->name, parameter );
		}
		int currentFieldIndex = 0;
		for( ast::nodes::FieldDeclarationSharedPointer& fieldDeclaration : structAst->fields ) {
			TypeSharedPointer fieldType = this->resolveType( fieldDeclaration->type );
			FieldInfo fieldInformation;
			fieldInformation.name = fieldDeclaration->name;
			fieldInformation.type = fieldType ? fieldType : this->typeRegistry.getError();
			fieldInformation.access = fieldDeclaration->access;
			fieldInformation.index = currentFieldIndex++;
			structType->fields.push_back( fieldInformation );
		}
		for( ast::nodes::DeclarationSharedPointer& methodNode : structAst->methods ) {
			if( methodNode == nullptr || methodNode->kind != ast::Node::Kind::FunctionDeclaration ) {
				continue;
			}
			ast::nodes::FunctionDeclaration& functionDecl = static_cast<ast::nodes::FunctionDeclaration&>( *methodNode );
			std::vector<TypeSharedPointer> methodParameterTypes;
			std::vector<std::string> methodParamNames;
			for( ast::nodes::FunctionParameterSharedPointer& parameter : functionDecl.parameters ) {
				if( parameter->isSelf ) {
					continue;
				}
				TypeSharedPointer parameterType = this->resolveType( parameter->type );
				methodParameterTypes.push_back( parameterType ? parameterType : this->typeRegistry.getError() );
				methodParamNames.push_back( parameter->name );
			}
			TypeSharedPointer returnType = functionDecl.returnType ? this->resolveType( functionDecl.returnType ) : this->typeRegistry.getVoid();
			TypeSharedPointer methodFuncType = this->typeRegistry.makeFunction( methodParameterTypes, returnType );
			std::static_pointer_cast<FunctionType>( methodFuncType )->parameterNames = std::move( methodParamNames );
			MethodInfo methodInformation;
			methodInformation.name = functionDecl.name;
			methodInformation.type = methodFuncType;
			methodInformation.access = functionDecl.access;
			methodInformation.isStatic = functionDecl.isStatic;
			methodInformation.isProperty = functionDecl.isProperty;
			methodInformation.virtualTableIndex = -1;
			structType->methods.push_back( methodInformation );
		}
		for( std::unordered_map<std::string, TypeSharedPointer>::iterator savedIterator = savedGenericParams.begin(); savedIterator != savedGenericParams.end(); ++savedIterator ) {
			this->typeRegistry.registerType( savedIterator->first, savedIterator->second );
		}
	}
	
	void Analyzer::pushScope( Scope::Kind scopeKind ) {
		this->currentScope = std::make_shared<Scope>( scopeKind, this->currentScope );
	}
	
	void Analyzer::registerTypeDeclaration( ast::nodes::DeclarationSharedPointer& declaration, const std::string& parentQualified ) {
		if( declaration == nullptr ) {
			return;
		}
		std::function<std::string( const std::string& )> buildQualified = [&]( const std::string& typeName ) -> std::string {
			if( parentQualified.empty() == false ) {
				return fmt::format( "{}.{}", parentQualified, typeName );
			}
			if( this->currentPackageName.empty() == false ) {
				return fmt::format( "{}.{}", this->currentPackageName, typeName );
			}
			return typeName;
		};
		std::function<void( TypeSharedPointer&, const std::string& )> setQualifiedFields = [&]( TypeSharedPointer& type, const std::string& typeName ) {
			type->package = this->currentPackageName;
			type->qualified = buildQualified( typeName );
		};
		switch( declaration->kind ) {
			case ast::Node::Kind::ClassDeclaration: {
				ast::nodes::ClassDeclaration& classDeclaration = static_cast<ast::nodes::ClassDeclaration&>( *declaration );
				std::string thisQualified = buildQualified( classDeclaration.name );
				TypeSharedPointer existing = this->typeRegistry.lookupType( classDeclaration.name );
				if( existing != nullptr && existing->kind == Type::Kind::Class ) {
					ClassTypeSharedPointer existingClass = std::static_pointer_cast<ClassType>( existing );
					if( existingClass->astDeclaration != nullptr ) {
						existingClass->astDeclaration = &classDeclaration;
						if( existingClass->package.empty() ) {
							existingClass->package = this->currentPackageName;
							existingClass->qualified = thisQualified;
						}
						for( ast::nodes::DeclarationSharedPointer& nestedDeclaration : classDeclaration.nestedDeclarations ) {
							this->registerTypeDeclaration( nestedDeclaration, thisQualified );
						}
						break;
					}
				}
				ClassTypeSharedPointer classType = std::make_shared<ClassType>( classDeclaration.name );
				classType->astDeclaration = &classDeclaration;
				classType->package = this->currentPackageName;
				classType->qualified = thisQualified;
				for( ast::nodes::GenericParameterSharedPointer& genericParameter : classDeclaration.genericParameters ) {
					classType->genericParameters.push_back( std::make_shared<GenericParameterType>( genericParameter->name ) );
				}
				this->typeRegistry.registerType( classDeclaration.name, classType );
				for( ast::nodes::DeclarationSharedPointer& nestedDeclaration : classDeclaration.nestedDeclarations ) {
					this->registerTypeDeclaration( nestedDeclaration, thisQualified );
				}
				break;
			}
			case ast::Node::Kind::StructDeclaration: {
				ast::nodes::StructDeclaration& structDeclaration = static_cast<ast::nodes::StructDeclaration&>( *declaration );
				std::string thisQualified = buildQualified( structDeclaration.name );
				TypeSharedPointer existingStruct = this->typeRegistry.lookupType( structDeclaration.name );
				if( existingStruct != nullptr && existingStruct->kind == Type::Kind::Struct ) {
					StructTypeSharedPointer existingStructType = std::static_pointer_cast<StructType>( existingStruct );
					if( existingStructType->astDeclaration != nullptr ) {
						existingStructType->astDeclaration = &structDeclaration;
						if( existingStructType->package.empty() ) {
							existingStructType->package = this->currentPackageName;
							existingStructType->qualified = thisQualified;
						}
						for( ast::nodes::DeclarationSharedPointer& nestedDeclaration : structDeclaration.nestedDeclarations ) {
							this->registerTypeDeclaration( nestedDeclaration, thisQualified );
						}
						break;
					}
				}
				StructTypeSharedPointer structType = std::make_shared<StructType>( structDeclaration.name );
				structType->astDeclaration = &structDeclaration;
				structType->package = this->currentPackageName;
				structType->qualified = thisQualified;
				for( ast::nodes::GenericParameterSharedPointer& genericParameter : structDeclaration.genericParameters ) {
					structType->genericParameters.push_back( std::make_shared<GenericParameterType>( genericParameter->name ) );
				}
				this->typeRegistry.registerType( structDeclaration.name, structType );
				for( ast::nodes::DeclarationSharedPointer& nestedDeclaration : structDeclaration.nestedDeclarations ) {
					this->registerTypeDeclaration( nestedDeclaration, thisQualified );
				}
				break;
			}
			case ast::Node::Kind::EnumDeclaration: {
				ast::nodes::EnumDeclaration& enumDeclaration = static_cast<ast::nodes::EnumDeclaration&>( *declaration );
				TypeSharedPointer existingEnum = this->typeRegistry.lookupType( enumDeclaration.name );
				if( existingEnum != nullptr && existingEnum->kind == Type::Kind::Enum ) {
					EnumTypeSharedPointer existingEnumType = std::static_pointer_cast<EnumType>( existingEnum );
					if( existingEnumType->astDeclaration != nullptr ) {
						existingEnumType->astDeclaration = &enumDeclaration;
						if( existingEnumType->package.empty() ) {
							TypeSharedPointer enumTypeRef = std::static_pointer_cast<Type>( existingEnumType );
							setQualifiedFields( enumTypeRef, enumDeclaration.name );
						}
						break;
					}
				}
				EnumTypeSharedPointer enumType = std::make_shared<EnumType>( enumDeclaration.name );
				enumType->astDeclaration = &enumDeclaration;
				enumType->package = this->currentPackageName;
				enumType->qualified = buildQualified( enumDeclaration.name );
				this->typeRegistry.registerType( enumDeclaration.name, enumType );
				break;
			}
			case ast::Node::Kind::InterfaceDeclaration: {
				ast::nodes::InterfaceDeclaration& interfaceDeclaration = static_cast<ast::nodes::InterfaceDeclaration&>( *declaration );
				std::string thisQualified = buildQualified( interfaceDeclaration.name );
				TypeSharedPointer existingInterface = this->typeRegistry.lookupType( interfaceDeclaration.name );
				if( existingInterface != nullptr && existingInterface->kind == Type::Kind::Interface ) {
					InterfaceTypeSharedPointer existingInterfaceType = std::static_pointer_cast<InterfaceType>( existingInterface );
					if( existingInterfaceType->astDeclaration != nullptr ) {
						existingInterfaceType->astDeclaration = &interfaceDeclaration;
						if( existingInterfaceType->package.empty() ) {
							existingInterfaceType->package = this->currentPackageName;
							existingInterfaceType->qualified = thisQualified;
						}
						for( ast::nodes::DeclarationSharedPointer& nestedDeclaration : interfaceDeclaration.nestedDeclarations ) {
							this->registerTypeDeclaration( nestedDeclaration, thisQualified );
						}
						break;
					}
				}
				InterfaceTypeSharedPointer interfaceType = std::make_shared<InterfaceType>( interfaceDeclaration.name );
				interfaceType->astDeclaration = &interfaceDeclaration;
				interfaceType->package = this->currentPackageName;
				interfaceType->qualified = thisQualified;
				this->typeRegistry.registerType( interfaceDeclaration.name, interfaceType );
				for( ast::nodes::DeclarationSharedPointer& nestedDeclaration : interfaceDeclaration.nestedDeclarations ) {
					this->registerTypeDeclaration( nestedDeclaration, thisQualified );
				}
				break;
			}
			case ast::Node::Kind::TraitDeclaration: {
				ast::nodes::TraitDeclaration& traitDeclaration = static_cast<ast::nodes::TraitDeclaration&>( *declaration );
				TypeSharedPointer existingTrait = this->typeRegistry.lookupType( traitDeclaration.name );
				if( existingTrait != nullptr && existingTrait->kind == Type::Kind::Trait ) {
					TraitTypeSharedPointer existingTraitType = std::static_pointer_cast<TraitType>( existingTrait );
					if( existingTraitType->astDeclaration != nullptr ) {
						existingTraitType->astDeclaration = &traitDeclaration;
						if( existingTraitType->package.empty() ) {
							TypeSharedPointer traitTypeRef = std::static_pointer_cast<Type>( existingTraitType );
							setQualifiedFields( traitTypeRef, traitDeclaration.name );
						}
						break;
					}
				}
				TraitTypeSharedPointer traitType = std::make_shared<TraitType>( traitDeclaration.name );
				traitType->astDeclaration = &traitDeclaration;
				traitType->package = this->currentPackageName;
				traitType->qualified = buildQualified( traitDeclaration.name );
				this->typeRegistry.registerType( traitDeclaration.name, traitType );
				break;
			}
			default: {
				break;
			}
		}
	}
	
	TypeSharedPointer Analyzer::resolveType( const ast::nodes::TypeNodeSharedPointer& typeNode ) {
		if( typeNode == nullptr ) {
			return nullptr;
		}
		switch( typeNode->kind ) {
			case ast::Node::Kind::SimpleType: {
				ast::nodes::SimpleTypeNode& simpleTypeNode = static_cast<ast::nodes::SimpleTypeNode&>( *typeNode );
				TypeSharedPointer resolvedType = this->typeRegistry.lookupType( simpleTypeNode.name );
				if( resolvedType == nullptr && simpleTypeNode.name == "Self" && this->currentScope->isInsideClass() ) {
					return this->currentScope->classType;
				}
				if( resolvedType == nullptr ) {
					std::string unknownTypeErrorMessage = fmt::format( "unknown type \"{}\"", simpleTypeNode.name );
					this->diagnostic.error( typeNode->source, unknownTypeErrorMessage );
					return this->typeRegistry.getError();
				}
				return resolvedType;
			}
			case ast::Node::Kind::GenericType: {
				ast::nodes::GenericTypeNode& genericTypeNode = static_cast<ast::nodes::GenericTypeNode&>( *typeNode );
				if( genericTypeNode.name == "Generator" && genericTypeNode.typeArguments.size() == 1 ) {
					TypeSharedPointer innerType = this->resolveType( genericTypeNode.typeArguments[0] );
					return innerType ? this->typeRegistry.makeGenerator( innerType ) : this->typeRegistry.getError();
				}
				if( genericTypeNode.name == "Future" && genericTypeNode.typeArguments.size() == 1 ) {
					TypeSharedPointer innerType = this->resolveType( genericTypeNode.typeArguments[0] );
					return innerType ? this->typeRegistry.makeFuture( innerType ) : this->typeRegistry.getError();
				}
				TypeSharedPointer baseType = this->typeRegistry.lookupType( genericTypeNode.name );
				if( baseType == nullptr ) {
					return this->typeRegistry.getError();
				}
				std::vector<TypeSharedPointer> typeArguments;
				for( ast::nodes::TypeNodeSharedPointer& argumentNode : genericTypeNode.typeArguments ) {
					typeArguments.push_back( this->resolveType( argumentNode ) );
				}
				std::string monomorphizedName = fmt::format( "{}<", genericTypeNode.name );
				for( size_t index=0; index<typeArguments.size(); index++ ) {
					if( index > 0 ) {
						monomorphizedName+= ",";
					}
					monomorphizedName+= typeArguments[index] ? typeArguments[index]->toString() : "?";
				}
				monomorphizedName+= ">";
				std::string qualifiedMonomorphizedName;
				if( baseType->qualified.empty() == false ) {
					qualifiedMonomorphizedName = fmt::format( "{}<", baseType->qualified );
					for( size_t qIdx = 0; qIdx < typeArguments.size(); qIdx++ ) {
						if( qIdx > 0 ) {
							qualifiedMonomorphizedName+= ",";
						}
						if( typeArguments[qIdx] && typeArguments[qIdx]->qualified.empty() == false ) {
							qualifiedMonomorphizedName+= typeArguments[qIdx]->qualified;
						}
						else {
							qualifiedMonomorphizedName+= typeArguments[qIdx] ? typeArguments[qIdx]->toString() : "?";
						}
					}
					qualifiedMonomorphizedName+= ">";
				}
				TypeSharedPointer existingType = this->typeRegistry.lookupType( monomorphizedName );
				if( existingType ) {
					if( existingType->kind == Type::Kind::Interface ) {
						this->populateInterfaceMethods( std::static_pointer_cast<InterfaceType>( existingType ) );
					}
					return existingType;
				}
				std::function<TypeSharedPointer(TypeSharedPointer,const std::unordered_map<std::string,TypeSharedPointer>&)> substituteType = [&]( TypeSharedPointer targetType, const std::unordered_map<std::string,TypeSharedPointer>& substitutionMap ) -> TypeSharedPointer {
					if( targetType == nullptr ) {
						return targetType;
					}
					if( targetType->kind == Type::Kind::GenericParameter ) {
						std::unordered_map<std::string,TypeSharedPointer>::const_iterator it = substitutionMap.find( targetType->name );
						if( it != substitutionMap.end() ) {
							return it->second;
						}
						return targetType;
					}
					if( targetType->kind == Type::Kind::Optional ) {
						return this->typeRegistry.makeOptional( substituteType( std::static_pointer_cast<OptionalType>( targetType )->inner, substitutionMap ) );
					}
					if( targetType->kind == Type::Kind::Array ) {
						ArrayTypeSharedPointer arrayType = std::static_pointer_cast<ArrayType>( targetType );
						return this->typeRegistry.makeArray( substituteType( arrayType->elementType, substitutionMap ), arrayType->size );
					}
					if( targetType->kind == Type::Kind::Future ) {
						return this->typeRegistry.makeFuture( substituteType( std::static_pointer_cast<FutureType>( targetType )->innerType, substitutionMap ) );
					}
					if( targetType->kind == Type::Kind::Generator ) {
						return this->typeRegistry.makeGenerator( substituteType( std::static_pointer_cast<GeneratorType>( targetType )->yieldType, substitutionMap ) );
					}
					if( targetType->kind == Type::Kind::Reference ) {
						ReferenceTypeSharedPointer referenceType = std::static_pointer_cast<ReferenceType>( targetType );
						return this->typeRegistry.makeReference( substituteType( referenceType->inner, substitutionMap ), referenceType->isMutable );
					}
					if( targetType->kind == Type::Kind::Pointer ) {
						PointerTypeSharedPointer pointerType = std::static_pointer_cast<PointerType>( targetType );
						return this->typeRegistry.makePointer( substituteType( pointerType->inner, substitutionMap ), pointerType->isMutable );
					}
					if( targetType->kind == Type::Kind::Function ) {
						FunctionTypeSharedPointer functionType = std::static_pointer_cast<FunctionType>( targetType );
						std::vector<TypeSharedPointer> substitutedParameters;
						for( TypeSharedPointer& parameter : functionType->parameterTypes ) {
							substitutedParameters.push_back( substituteType( parameter, substitutionMap ) );
						}
						return this->typeRegistry.makeFunction( substitutedParameters, substituteType( functionType->returnType, substitutionMap ), functionType->isVariadic );
					}
					if( targetType->kind == Type::Kind::Callable ) {
						CallableTypeSharedPointer callableType = std::static_pointer_cast<CallableType>( targetType );
						std::vector<TypeSharedPointer> substitutedParameters;
						for( TypeSharedPointer& parameter : callableType->parameterTypes ) {
							substitutedParameters.push_back( substituteType( parameter, substitutionMap ) );
						}
						return this->typeRegistry.makeCallable( substituteType( callableType->returnType, substitutionMap ), substitutedParameters );
					}
					if( targetType->kind == Type::Kind::Class || targetType->kind == Type::Kind::Interface || targetType->kind == Type::Kind::Struct ) {
						std::string baseTypeName;
						std::vector<TypeSharedPointer> genericParameters;
						std::unordered_map<std::string,TypeSharedPointer> currentSubstitutions;
						if( targetType->kind == Type::Kind::Class ) {
							ClassTypeSharedPointer classType = std::static_pointer_cast<ClassType>( targetType );
							if( classType->astDeclaration == nullptr ) {
								return targetType;
							}
							baseTypeName = classType->astDeclaration->name;
							genericParameters = classType->genericParameters;
							currentSubstitutions = classType->typeSubstitutions;
						}
						else if( targetType->kind == Type::Kind::Interface ) {
							InterfaceTypeSharedPointer interfaceType = std::static_pointer_cast<InterfaceType>( targetType );
							if( interfaceType->astDeclaration == nullptr ) {
								return targetType;
							}
							baseTypeName = interfaceType->astDeclaration->name;
							genericParameters = interfaceType->genericParameters;
							currentSubstitutions = interfaceType->typeSubstitutions;
						}
						else {
							StructTypeSharedPointer structType = std::static_pointer_cast<StructType>( targetType );
							if( structType->astDeclaration == nullptr ) {
								return targetType;
							}
							baseTypeName = structType->astDeclaration->name;
							genericParameters = structType->genericParameters;
							currentSubstitutions = structType->typeSubstitutions;
						}
						if( genericParameters.empty() && currentSubstitutions.empty() ) {
							return targetType;
						}
						std::vector<TypeSharedPointer> effectiveArguments;
						bool hasChanges = false;
						TypeSharedPointer baseTypeLookup = this->typeRegistry.lookupType( baseTypeName );
						if( baseTypeLookup == nullptr ) {
							return targetType;
						}
						std::vector<TypeSharedPointer> baseGenericParameters;
						if( baseTypeLookup->kind == Type::Kind::Class ) {
							baseGenericParameters = std::static_pointer_cast<ClassType>( baseTypeLookup )->genericParameters;
						}
						else if( baseTypeLookup->kind == Type::Kind::Interface ) {
							baseGenericParameters = std::static_pointer_cast<InterfaceType>( baseTypeLookup )->genericParameters;
						}
						else if( baseTypeLookup->kind == Type::Kind::Struct ) {
							baseGenericParameters = std::static_pointer_cast<StructType>( baseTypeLookup )->genericParameters;
						}
						for( TypeSharedPointer& gp : baseGenericParameters ) {
							std::unordered_map<std::string,TypeSharedPointer>::iterator sit = currentSubstitutions.find( gp->name );
							TypeSharedPointer oldArgument = ( sit != currentSubstitutions.end() ) ? sit->second : gp;
							TypeSharedPointer newArgument = substituteType( oldArgument, substitutionMap );
							if( newArgument != oldArgument ) {
								hasChanges = true;
							}
							effectiveArguments.push_back( newArgument );
						}
						if( hasChanges == false ) {
							return targetType;
						}
						std::string newMonomorphizedName = fmt::format( "{}<", baseTypeName );
						for( size_t i=0; i<effectiveArguments.size(); i++ ) {
							if( i > 0 ) newMonomorphizedName+= ",";
							newMonomorphizedName+= effectiveArguments[i] ? effectiveArguments[i]->toString() : "?";
						}
						newMonomorphizedName+= ">";
						std::string newQualifiedMonoName;
						if( baseTypeLookup->qualified.empty() == false ) {
							newQualifiedMonoName = fmt::format( "{}<", baseTypeLookup->qualified );
							for( size_t i = 0; i < effectiveArguments.size(); i++ ) {
								if( i > 0 ) newQualifiedMonoName+= ",";
								if( effectiveArguments[i] && effectiveArguments[i]->qualified.empty() == false ) {
									newQualifiedMonoName+= effectiveArguments[i]->qualified;
								}
								else {
									newQualifiedMonoName+= effectiveArguments[i] ? effectiveArguments[i]->toString() : "?";
								}
							}
							newQualifiedMonoName+= ">";
						}
						TypeSharedPointer existingMono = this->typeRegistry.lookupType( newMonomorphizedName );
						if( existingMono ) {
							return existingMono;
						}
						if( targetType->kind == Type::Kind::Interface ) {
							InterfaceTypeSharedPointer baseInterface = std::static_pointer_cast<InterfaceType>( baseTypeLookup );
							InterfaceTypeSharedPointer monoInterface = std::make_shared<InterfaceType>( newMonomorphizedName );
							monoInterface->package = baseInterface->package;
							monoInterface->qualified = newQualifiedMonoName.empty() == false ? newQualifiedMonoName : newMonomorphizedName;
							monoInterface->astDeclaration = baseInterface->astDeclaration;
							std::unordered_map<std::string,TypeSharedPointer> newSubstMap;
							for( size_t i = 0; i < baseGenericParameters.size(); ++i ) newSubstMap[baseGenericParameters[i]->name] = effectiveArguments[i];
							monoInterface->typeSubstitutions = newSubstMap;
							this->typeRegistry.registerType( newMonomorphizedName, monoInterface );
							for( TypeSharedPointer& superInterface : baseInterface->superInterfaces ) {
								monoInterface->superInterfaces.push_back( substituteType( superInterface, newSubstMap ) );
							}
							this->populateInterfaceMethods( baseInterface );
							for( MethodInfo& methodInfo : baseInterface->methods ) {
								MethodInfo substitutedMethod = methodInfo;
								substitutedMethod.type = substituteType( methodInfo.type, newSubstMap );
								monoInterface->methods.push_back( substitutedMethod );
							}
							monoInterface->methodOrder = baseInterface->methodOrder;
							for( int methodIndex = 0; methodIndex < static_cast<int>( monoInterface->methods.size() ); methodIndex++ ) {
								monoInterface->methods[methodIndex].interfaceTableIndex = methodIndex;
							}
							return monoInterface;
						}
					}
					return targetType;
				};
				if( baseType->kind == Type::Kind::Class ) {
					ClassTypeSharedPointer baseClass = std::static_pointer_cast<ClassType>( baseType );
					std::unordered_map<std::string,TypeSharedPointer> substitutionMap;
					for( size_t i = 0; i < baseClass->genericParameters.size() && i < typeArguments.size(); i++ ) {
						substitutionMap[baseClass->genericParameters[i]->name] = typeArguments[i];
					}
					if( baseClass->astDeclaration ) {
						for( size_t i = 0; i < baseClass->astDeclaration->genericParameters.size() && i < typeArguments.size(); i++ ) {
							for( ast::nodes::TypeNodeSharedPointer& constraintNode : baseClass->astDeclaration->genericParameters[i]->constraints ) {
								TypeSharedPointer constraintType = this->resolveType( constraintNode );
								if( constraintType == nullptr || constraintType->kind != Type::Kind::Interface ) {
									continue;
								}
								bool satisfiesConstraint = false;
								if( typeArguments[i]->kind == Type::Kind::Class ) {
									ClassTypeSharedPointer argClass = std::static_pointer_cast<ClassType>( typeArguments[i] );
									for( TypeSharedPointer& iface : argClass->interfaces ) {
										if( iface->equals( constraintType ) ) {
											satisfiesConstraint = true;
											break;
										}
									}
								}
								if( satisfiesConstraint == false ) {
									std::string constraintErrorMessage = fmt::format( "type \"{}\" does not satisfy constraint \"{}\" on generic parameter \"{}\"", typeArguments[i]->name, constraintType->name, baseClass->astDeclaration->genericParameters[i]->name );
									this->diagnostic.error( genericTypeNode.source, constraintErrorMessage );
								}
							}
						}
					}
					ClassTypeSharedPointer monoClass = std::make_shared<ClassType>( monomorphizedName );
					monoClass->package = baseClass->package;
					monoClass->qualified = qualifiedMonomorphizedName.empty() == false ? qualifiedMonomorphizedName : monomorphizedName;
					monoClass->astDeclaration = baseClass->astDeclaration;
					monoClass->baseClass = substituteType( baseClass->baseClass, substitutionMap );
					for( TypeSharedPointer& iface : baseClass->interfaces ) monoClass->interfaces.push_back( substituteType( iface, substitutionMap ) );
					monoClass->typeSubstitutions = substitutionMap;
					for( FieldInfo& field : baseClass->fields ) {
						FieldInfo substitutedField = field;
						substitutedField.type = substituteType( substitutedField.type, substitutionMap );
						monoClass->fields.push_back( substitutedField );
					}
					for( MethodInfo& method : baseClass->methods ) {
						MethodInfo substitutedMethod = method;
						substitutedMethod.type = substituteType( substitutedMethod.type, substitutionMap );
						monoClass->methods.push_back( substitutedMethod );
					}
					this->typeRegistry.registerType( monomorphizedName, monoClass );
					return monoClass;
				}
				if( baseType->kind == Type::Kind::Interface ) {
					InterfaceTypeSharedPointer baseInterface = std::static_pointer_cast<InterfaceType>( baseType );
					this->populateInterfaceMethods( baseInterface );
					std::unordered_map<std::string, TypeSharedPointer> substitutionMap;
					for( size_t i = 0; i < baseInterface->genericParameters.size() && i < typeArguments.size(); i++ ) {
						substitutionMap[baseInterface->genericParameters[i]->name] = typeArguments[i];
					}
					if( baseInterface->astDeclaration ) {
						for( size_t i = 0; i < baseInterface->astDeclaration->genericParameters.size() && i < typeArguments.size(); i++ ) {
							for( ast::nodes::TypeNodeSharedPointer& constraintNode : baseInterface->astDeclaration->genericParameters[i]->constraints ) {
								TypeSharedPointer constraintType = this->resolveType( constraintNode );
								if( constraintType == nullptr || constraintType->kind != Type::Kind::Interface ) {
									continue;
								}
								bool satisfiesConstraint = false;
								if( typeArguments[i]->kind == Type::Kind::Class ) {
									ClassTypeSharedPointer argClass = std::static_pointer_cast<ClassType>( typeArguments[i] );
									for( TypeSharedPointer& iface : argClass->interfaces ) {
										if( iface->equals( constraintType ) ) {
											satisfiesConstraint = true;
											break;
										}
									}
								}
								if( satisfiesConstraint == false ) {
									std::string constraintErrorMessage = fmt::format( "type \"{}\" does not satisfy constraint \"{}\" on generic parameter \"{}\"", typeArguments[i]->name, constraintType->name, baseInterface->astDeclaration->genericParameters[i]->name );
									this->diagnostic.error( genericTypeNode.source, constraintErrorMessage );
								}
							}
						}
					}
					InterfaceTypeSharedPointer monoInterface = std::make_shared<InterfaceType>( monomorphizedName );
					monoInterface->package = baseInterface->package;
					monoInterface->qualified = qualifiedMonomorphizedName.empty() == false ? qualifiedMonomorphizedName : monomorphizedName;
					monoInterface->astDeclaration = baseInterface->astDeclaration;
					monoInterface->typeSubstitutions = substitutionMap;
					this->typeRegistry.registerType( monomorphizedName, monoInterface );
					for( TypeSharedPointer& superInterface : baseInterface->superInterfaces ) {
						monoInterface->superInterfaces.push_back( substituteType( superInterface, substitutionMap ) );
					}
					for( MethodInfo& method : baseInterface->methods ) {
						MethodInfo substitutedMethod = method;
						substitutedMethod.type = substituteType( method.type, substitutionMap );
						monoInterface->methods.push_back( substitutedMethod );
					}
					monoInterface->methodOrder = baseInterface->methodOrder;
					for( int methodIndex = 0; methodIndex < static_cast<int>( monoInterface->methods.size() ); methodIndex++ ) {
						monoInterface->methods[methodIndex].interfaceTableIndex = methodIndex;
					}
					return monoInterface;
				}
				if( baseType->kind == Type::Kind::Struct ) {
					StructTypeSharedPointer baseStruct = std::static_pointer_cast<StructType>( baseType );
					std::unordered_map<std::string, TypeSharedPointer> substitutionMap;
					for( size_t i = 0; i < baseStruct->genericParameters.size() && i < typeArguments.size(); i++ ) {
						substitutionMap[baseStruct->genericParameters[i]->name] = typeArguments[i];
					}
					if( baseStruct->astDeclaration ) {
						for( size_t i = 0; i < baseStruct->astDeclaration->genericParameters.size() && i < typeArguments.size(); i++ ) {
							for( ast::nodes::TypeNodeSharedPointer& constraintNode : baseStruct->astDeclaration->genericParameters[i]->constraints ) {
								TypeSharedPointer constraintType = this->resolveType( constraintNode );
								if( constraintType == nullptr || constraintType->kind != Type::Kind::Interface ) {
									continue;
								}
								bool satisfiesConstraint = false;
								if( typeArguments[i]->kind == Type::Kind::Class ) {
									ClassTypeSharedPointer argClass = std::static_pointer_cast<ClassType>( typeArguments[i] );
									for( TypeSharedPointer& iface : argClass->interfaces ) {
										if( iface->equals( constraintType ) ) {
											satisfiesConstraint = true;
											break;
										}
									}
								}
								if( satisfiesConstraint == false ) {
									std::string constraintErrorMessage = fmt::format( "type \"{}\" does not satisfy constraint \"{}\" on generic parameter \"{}\"", typeArguments[i]->name, constraintType->name, baseStruct->astDeclaration->genericParameters[i]->name );
									this->diagnostic.error( genericTypeNode.source, constraintErrorMessage );
								}
							}
						}
					}
					StructTypeSharedPointer monoStruct = std::make_shared<StructType>( monomorphizedName );
					monoStruct->package = baseStruct->package;
					monoStruct->qualified = qualifiedMonomorphizedName.empty() == false ? qualifiedMonomorphizedName : monomorphizedName;
					monoStruct->astDeclaration = baseStruct->astDeclaration;
					monoStruct->typeSubstitutions = substitutionMap;
					this->typeRegistry.registerType( monomorphizedName, monoStruct );
					for( FieldInfo& field : baseStruct->fields ) {
						FieldInfo substitutedField = field;
						substitutedField.type = substituteType( field.type, substitutionMap );
						monoStruct->fields.push_back( substitutedField );
					}
					for( MethodInfo& method : baseStruct->methods ) {
						MethodInfo substitutedMethod = method;
						substitutedMethod.type = substituteType( method.type, substitutionMap );
						monoStruct->methods.push_back( substitutedMethod );
					}
					return monoStruct;
				}
				return baseType;
			}
			case ast::Node::Kind::ReferenceType: {
				ast::nodes::ReferenceTypeNode& referenceNode = static_cast<ast::nodes::ReferenceTypeNode&>( *typeNode );
				TypeSharedPointer innerType = this->resolveType( referenceNode.innerType );
				return innerType ? this->typeRegistry.makeReference( innerType, referenceNode.isMutable ) : this->typeRegistry.getError();
			}
			case ast::Node::Kind::PointerType: {
				ast::nodes::PointerTypeNode& pointerNode = static_cast<ast::nodes::PointerTypeNode&>( *typeNode );
				TypeSharedPointer innerType = this->resolveType( pointerNode.innerType );
				return innerType ? this->typeRegistry.makePointer( innerType, pointerNode.isMutable ) : this->typeRegistry.getError();
			}
			case ast::Node::Kind::ArrayType: {
				ast::nodes::ArrayTypeNode& arrayNode = static_cast<ast::nodes::ArrayTypeNode&>( *typeNode );
				TypeSharedPointer elementType = this->resolveType( arrayNode.elementType );
				return elementType ? this->typeRegistry.makeArray( elementType ) : this->typeRegistry.getError();
			}
			case ast::Node::Kind::TupleType: {
				ast::nodes::TupleTypeNode& tupleNode = static_cast<ast::nodes::TupleTypeNode&>( *typeNode );
				std::vector<TypeSharedPointer> elementTypes;
				for( ast::nodes::TypeNodeSharedPointer& elementNode : tupleNode.elements ) {
					TypeSharedPointer resolvedElement = this->resolveType( elementNode );
					elementTypes.push_back( resolvedElement ? resolvedElement : this->typeRegistry.getError() );
				}
				return this->typeRegistry.makeTuple( std::move( elementTypes ) );
			}
			case ast::Node::Kind::FunctionType: {
				ast::nodes::FunctionTypeNode& functionTypeNode = static_cast<ast::nodes::FunctionTypeNode&>( *typeNode );
				std::vector<TypeSharedPointer> parameterTypes;
				for( ast::nodes::TypeNodeSharedPointer& parameterNode : functionTypeNode.parameterTypes ) {
					TypeSharedPointer resolvedParameter = this->resolveType( parameterNode );
					parameterTypes.push_back( resolvedParameter ? resolvedParameter : this->typeRegistry.getError() );
				}
				TypeSharedPointer returnType = this->resolveType( functionTypeNode.returnType );
				return this->typeRegistry.makeFunction( std::move( parameterTypes ), returnType ? returnType : this->typeRegistry.getVoid() );
			}
			case ast::Node::Kind::OptionalType: {
				ast::nodes::OptionalTypeNode& optionalNode = static_cast<ast::nodes::OptionalTypeNode&>( *typeNode );
				TypeSharedPointer innerType = this->resolveType( optionalNode.innerType );
				return innerType ? this->typeRegistry.makeOptional( innerType ) : this->typeRegistry.getError();
			}
			case ast::Node::Kind::UnionType: {
				ast::nodes::UnionTypeNode& unionNode = static_cast<ast::nodes::UnionTypeNode&>( *typeNode );
				std::vector<TypeSharedPointer> resolvedUnionTypes;
				for( ast::nodes::TypeNodeSharedPointer& memberNode : unionNode.types ) {
					TypeSharedPointer resolvedMember = this->resolveType( memberNode );
					resolvedUnionTypes.push_back( resolvedMember ? resolvedMember : this->typeRegistry.getError() );
				}
				return this->typeRegistry.makeUnion( std::move( resolvedUnionTypes ) );
			}
			default:
				return this->typeRegistry.getError();
		}
	}
	
	std::string Analyzer::semanticTypeToRegistryName( const TypeSharedPointer& type ) {
		if( type == nullptr ) {
			return "?";
		}
		if( type->kind == Type::Kind::Integer ) {
			IntegerTypeSharedPointer intType = std::static_pointer_cast<IntegerType>( type );
			if( intType->isSigned ) {
				switch( intType->bitWidth ) {
					case 8: return "I8";
					case 16: return "I16";
					case 32: return "I32";
					case 64: return "I64";
					default: return "I64";
				}
			}
			switch( intType->bitWidth ) {
				case 8: return "U8";
				case 16: return "U16";
				case 32: return "U32";
				case 64: return "U64";
				default: return "U64";
			}
		}
		if( type->kind == Type::Kind::Float ) {
			FloatTypeSharedPointer floatType = std::static_pointer_cast<FloatType>( type );
			return floatType->bitWidth == 32 ? "F32" : "Float";
		}
		if( type->kind == Type::Kind::String ) return "String";
		if( type->kind == Type::Kind::Bool ) return "Boolean";
		if( type->kind == Type::Kind::Char ) return "Char";
		if( type->kind == Type::Kind::Void ) return "Void";
		return type->name;
	}
	
	TypeSharedPointer Analyzer::monomorphizeGenericType( const std::string& baseName, const std::vector<TypeSharedPointer>& typeArgs, const lookup::SourceSharedPointer& source ) {
		std::vector<ast::nodes::TypeNodeSharedPointer> argNodes;
		for( TypeSharedPointer typeArg : typeArgs ) {
			std::string argName = this->semanticTypeToRegistryName( typeArg );
			argNodes.push_back( std::make_shared<ast::nodes::SimpleTypeNode>( argName, source ) );
		}
		ast::nodes::TypeNodeSharedPointer genericNode = std::make_shared<ast::nodes::GenericTypeNode>( baseName, std::move( argNodes ), source );
		return this->resolveType( genericNode );
	}
	
	void Analyzer::validateAccessControl( const SymbolSharedPointer& symbol, const lookup::SourceSharedPointer& source ) {
		if( symbol == nullptr ) {
			return;
		}
		if( symbol->kind != Symbol::Kind::Field ) {
			return;
		}
		switch( symbol->access ) {
			case ast::AccessModifier::Private: {
				if( this->currentScope->isInsideClass() == false ) {
					std::string privateAccessErrorMessage = fmt::format( "cannot access private member \"{}\"", symbol->name );
					this->diagnostic.error( source, privateAccessErrorMessage );
				}
				break;
			}
			case ast::AccessModifier::Protect: {
				if( this->currentScope->isInsideClass() == false ) {
					std::string protectedAccessErrorMessage = fmt::format( "cannot access protect member \"{}\"", symbol->name );
					this->diagnostic.error( source, protectedAccessErrorMessage );
				}
				break;
			}
			default: {
				break;
			}
		}
	}
	
	void Analyzer::validateClassHierarchy( ast::nodes::ClassDeclaration& declaration, ClassTypeSharedPointer classType ) {
		TypeSharedPointer currentType = classType->baseClass;
		while( currentType && currentType->kind == Type::Kind::Class ) {
			if( currentType->equals( classType ) ) {
				std::string circularInheritanceErrorMessage = fmt::format( "circular inheritance detected for class \"{}\"", classType->name );
				this->diagnostic.error( declaration.source, circularInheritanceErrorMessage );
				break;
			}
			currentType = std::static_pointer_cast<ClassType>(currentType)->baseClass;
		}
	}
	
	void Analyzer::validateInterfaceImplementation( TypeSharedPointer implementationType, TypeSharedPointer interfaceType, const lookup::SourceSharedPointer& source ) {
		if( implementationType == nullptr || interfaceType == nullptr ) {
			return;
		}
		if( interfaceType->kind != Type::Kind::Interface ) {
			return;
		}
		InterfaceTypeSharedPointer interfaceDefinition = std::static_pointer_cast<InterfaceType>( interfaceType );
		ClassTypeSharedPointer classImplementation = std::dynamic_pointer_cast<ClassType>( implementationType );
		if( classImplementation == nullptr ) {
			return;
		}
		for( MethodInfo& method : interfaceDefinition->methods ) {
			FunctionTypeSharedPointer interfaceFunction = std::dynamic_pointer_cast<FunctionType>( method.type );
			bool found = false;
			bool nameExists = false;
			for( MethodInfo& implMethod : classImplementation->methods ) {
				if( implMethod.name != method.name ) {
					continue;
				}
				nameExists = true;
				if( interfaceFunction == nullptr || implMethod.type == nullptr ) {
					found = true;
					break;
				}
				FunctionTypeSharedPointer implFunction = std::dynamic_pointer_cast<FunctionType>( implMethod.type );
				if( implFunction == nullptr ) {
					found = true;
					break;
				}
				if( interfaceFunction->parameterTypes.size() != implFunction->parameterTypes.size() ) {
					continue;
				}
				bool returnTypeMatches = true;
				if( interfaceFunction->returnType && implFunction->returnType ) {
					returnTypeMatches = this->typeRegistry.isAssignable( interfaceFunction->returnType, implFunction->returnType );
					if( returnTypeMatches == false ) {
						std::string ifaceRetName = interfaceFunction->returnType->name;
						std::string implRetName = implFunction->returnType->name;
						size_t ifaceGenPos = ifaceRetName.find( '<' );
						size_t implGenPos = implRetName.find( '<' );
						std::string ifaceBaseName = ifaceGenPos != std::string::npos ? ifaceRetName.substr( 0, ifaceGenPos ) : ifaceRetName;
						std::string implBaseName = implGenPos != std::string::npos ? implRetName.substr( 0, implGenPos ) : implRetName;
						returnTypeMatches = ifaceBaseName == implBaseName;
					}
				}
				if( returnTypeMatches ) {
					found = true;
					break;
				}
			}
			if( found == false ) {
				if( nameExists == false ) {
					std::string missingMethodErrorMessage = fmt::format( "class \"{}\" does not implement method \"{}\" required by interface \"{}\"", classImplementation->name, method.name, interfaceDefinition->name );
					this->diagnostic.error( source, missingMethodErrorMessage );
				}
				else {
					std::string wrongSignatureErrorMessage = fmt::format( "method \"{}\" in class \"{}\" has wrong signature for interface \"{}\"", method.name, classImplementation->name, interfaceDefinition->name );
					this->diagnostic.error( source, wrongSignatureErrorMessage );
				}
			}
		}
	}
	
	TypeSharedPointer Analyzer::resolveTypeFromNode( const ast::nodes::TypeNodeSharedPointer& typeNode ) {
		return this->resolveType( typeNode );
	}
	
}
