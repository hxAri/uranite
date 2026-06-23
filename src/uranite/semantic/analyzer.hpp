
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

#ifndef _URANITE_SEMANTIC_ANALYZER_HPP_
#define _URANITE_SEMANTIC_ANALYZER_HPP_

#include <set>
#include <string>
#include <unordered_map>

#include "uranite/ast/node.hpp"
#include "uranite/diagnostic/diagnostic.hpp"
#include "uranite/lookup/source.hpp"
#include "uranite/semantic/scope.hpp"
#include "uranite/semantic/symbol.hpp"
#include "uranite/semantic/typeref.hpp"

namespace uranite::semantic {
	
	/**
	 * @brief Represents a single completion suggestion in the IDE or editor integration.
	 * 
	 * This structure holds the necessary metadata for displaying and documenting
	 * code completion items during development.
	 */
	struct CompletionItem {
		
		std::string detail;
		std::string documentation;
		Symbol::Kind kind;
		std::string label;
		
		std::string insertText;
		int insertTextFormat = 1;
		
		uint32_t editRangeLine = 0;
		uint32_t editRangeStartColumn = 0;
		uint32_t editRangeEndColumn = 0;
		
		int sortPriority = 3;
		
		std::string autoImportText;
		int autoImportInsertLine = -1;
		std::string sourceFile;
		
		int resolveId = -1;
		bool isDocumentationResolved = true;
		
		CompletionItem(
			const std::string& label,
			Symbol::Kind kind,
			const std::string& detail = "",
			const std::string& documentation = ""
		) : detail( detail ),
			documentation( documentation ),
			kind( kind ),
			label( label ) {
		}
	
	};
	
	/**
	 * @brief Performs semantic analysis on the Abstract Syntax Tree (AST).
	 * 
	 * The Analyzer is responsible for type checking, scope management,
	 * object-oriented validation, and preparing the AST for code generation.
	 */
	class Analyzer {
		
		public:
			
			/**
			 * @brief Constructs an Analyzer with a reference to the diagnostic engine.
			 * @param diagnostic The diagnostic engine for reporting semantic errors.
			 */
			Analyzer( diagnostic::Engine& diagnostic );
			
			/**
			 * @brief Entry point for analyzing an entire program.
			 * @param program The root node of the AST representing the program.
			 * @return True if analysis succeeded without fatal errors, false otherwise.
			 */
			bool analyze( ast::nodes::Program& program );
			
			/**
			 * @brief Gets the collection of gathered completion items for autocompletion.
			 * @return A constant reference to the vector of CompletionItems.
			 */
			const std::vector<CompletionItem>& completions() const {
				return this->completionItems;
			}
			
			/**
			 * @brief Sets the source position for which autocompletion should be generated.
			 * @param source The location in the source code.
			 */
			void setCompletionPosition( lookup::SourceSharedPointer source ) {
				this->completionPosition = source;
			}
			
			void importModuleTypes( const std::unordered_map<std::string, TypeSharedPointer>& types );
			void importModuleSymbols( const std::unordered_map<std::string, std::vector<SymbolSharedPointer>>& symbols );
			std::unordered_map<std::string, TypeSharedPointer> getRegisteredTypes() const;
			std::unordered_map<std::string, std::vector<SymbolSharedPointer>> getRegisteredSymbols() const;
			bool analyzeModuleRegistration( ast::nodes::Program& program );
			
			/**
			 * @brief Accesses the type registry used by the analyzer.
			 * @return A reference to the Registry.
			 */
			Registry& types() {
				return this->typeRegistry;
			}
			
			/**
			 * @brief Accesses the type registry used by the analyzer (constant version).
			 * @return A constant reference to the Registry.
			 */
			const Registry& types() const {
				return this->typeRegistry;
			}
			
			ScopeSharedPointer globalScope() const {
				return this->globalScope_;
			}
			
			/** @brief Resolves an AST TypeNode to its semantic Type representation. */
			TypeSharedPointer resolveTypeFromNode( const ast::nodes::TypeNodeSharedPointer& typeNode );
		
		private:
			
			/**
			 * @brief Analyzes an assignment statement.
			 * @param statement The assignment statement node.
			 */
			void analyzeAssignStatement( ast::nodes::AssignStatement& statement );
			
			/**
			 * @brief Resolves types and validates a binary expression.
			 * @param expression The binary expression node.
			 * @return The resolved TypeSharedPointer of the operation.
			 */
			TypeSharedPointer analyzeBinaryExpression( ast::nodes::BinaryExpression& expression );
			
			/**
			 * @brief Resolves types for a function or constructor call.
			 * @param expression The call expression node.
			 * @return The return type of the call.
			 */
			TypeSharedPointer analyzeCallExpression( ast::nodes::CallExpression& expression );
			
			/**
			 * @brief Analyzes a class declaration and its members.
			 * @param declaration The class declaration node.
			 */
			void analyzeClassDeclaration( ast::nodes::ClassDeclaration& declaration );
			
			/**
			 * @brief Dispatches a generic declaration to its specific analyzer method.
			 * @param declaration The declaration shared pointer.
			 */
			void analyzeDeclaration( ast::nodes::DeclarationSharedPointer& declaration );
			
			/**
			 * @brief Analyzes an enumeration declaration.
			 * @param declaration The enum declaration node.
			 */
			void analyzeEnumDeclaration( ast::nodes::EnumDeclaration& declaration );
			
			/**
			 * @brief Dispatches a generic expression to its specific analyzer method.
			 * @param expression The expression shared pointer.
			 * @return The resolved type of the expression.
			 */
			TypeSharedPointer analyzeExpression( ast::nodes::ExpressionSharedPointer& expression );
			
			/**
			 * @brief Analyzes a for-loop statement.
			 * @param statement The for-loop statement node.
			 */
			void analyzeForStatement( ast::nodes::ForStatement& statement );
			
			/**
			 * @brief Analyzes a function declaration, including parameters and body.
			 * @param declaration The function declaration node.
			 */
			void analyzeFunctionDeclaration( ast::nodes::FunctionDeclaration& declaration );
			
			/**
			 * @brief Resolves an identifier to a symbol in the current scope.
			 * @param expression The identifier expression node.
			 * @return The type associated with the identifier.
			 */
			TypeSharedPointer analyzeIdentifierExpression( ast::nodes::IdentifierExpression& expression );
			
			/**
			 * @brief Analyzes an if-conditional statement.
			 * @param statement The if-statement node.
			 */
			void analyzeIfStatement( ast::nodes::IfStatement& statement );
			
			/**
			 * @brief Analyzes an interface implementation block.
			 * @param declaration The implement declaration node.
			 */
			void analyzeImplementDeclaration( ast::nodes::ImplementDeclaration& declaration );
			
			/**
			 * @brief Analyzes an array or collection indexing expression.
			 * @param expression The index expression node.
			 * @return The type of the element being accessed.
			 */
			TypeSharedPointer analyzeIndexExpression( ast::nodes::IndexExpression& expression );
			
			/**
			 * @brief Analyzes an interface declaration.
			 * @param declaration The interface declaration node.
			 */
			void analyzeInterfaceDeclaration( ast::nodes::InterfaceDeclaration& declaration );
			
			/**
			 * @brief Analyzes a match (switch-like) statement.
			 * @param statement The match statement node.
			 */
			void analyzeMatchStatement( ast::nodes::MatchStatement& statement );
			
			/**
			 * @brief Resolves a member access expression (e.g., object.property).
			 * @param expression The member access expression node.
			 * @return The type of the member.
			 */
			TypeSharedPointer analyzeMemberAccessExpression( ast::nodes::MemberAccessExpression& expression );
			
			/**
			 * @brief Resolves a method call expression.
			 * @param expression The method call expression node.
			 * @return The return type of the method.
			 */
			TypeSharedPointer analyzeMethodCallExpression( ast::nodes::MethodCallExpression& expression );
			
			/**
			 * @brief Analyzes a return statement and validates it against the current function's return type.
			 * @param statement The return statement node.
			 */
			void analyzeReturnStatement( ast::nodes::ReturnStatement& statement );
			
			/**
			 * @brief Dispatches a generic statement to its specific analyzer method.
			 * @param statement The statement shared pointer.
			 */
			void analyzeStatement( ast::nodes::StatementSharedPointer& statement );
			
			/**
			 * @brief Analyzes a struct declaration.
			 * @param declaration The struct declaration node.
			 */
			void analyzeStructDeclaration( ast::nodes::StructDeclaration& declaration );
			
			/**
			 * @brief Analyzes a trait declaration.
			 * @param declaration The trait declaration node.
			 */
			void analyzeTraitDeclaration( ast::nodes::TraitDeclaration& declaration );
			
			/**
			 * @brief Analyzes a type alias declaration.
			 * @param declaration The type alias declaration node.
			 */
			void analyzeTypeAliasDeclaration( ast::nodes::TypeAliasDeclaration& declaration );
			
			/**
			 * @brief Validates and resolves a unary expression.
			 * @param expression The unary expression node.
			 * @return The resulting type of the expression.
			 */
			TypeSharedPointer analyzeUnaryExpression( ast::nodes::UnaryExpression& expression );
			
			/**
			 * @brief Analyzes an unsafe code block.
			 * @param statement The unsafe block statement node.
			 */
			void analyzeUnsafeBlockStatement( ast::nodes::UnsafeBlockStatement& statement );
			
			/**
			 * @brief Analyzes a variable declaration statement.
			 * @param statement The variable statement node.
			 */
			void analyzeVariableStatement( ast::nodes::VariableStatement& statement );
			
			/**
			 * @brief Analyzes a while-loop statement.
			 * @param statement The while-statement node.
			 */
			void analyzeWhileStatement( ast::nodes::WhileStatement& statement );
			
			/**
			 * @brief Generates a Virtual Method Table (VTable) for the given class.
			 * @param classType The class type to process.
			 */
			void buildVTable( ClassTypeSharedPointer classType );
			
			/**
			 * @brief Scans the current scope chain to provide completion suggestions.
			 * @param prefix The partial identifier typed so far, used for edit range calculation.
			 * @param triggerSource The source location where completion was triggered.
			 */
			void collectScopeCompletions( const std::string& prefix, const lookup::SourceSharedPointer& triggerSource );
			
			/**
			 * @brief Collects member completion items for a given type (fields, methods, variants).
			 * @param type The type whose members should be suggested.
			 * @param triggerSource The source location where completion was triggered.
			 */
			void collectTypeCompletions( TypeSharedPointer type, const lookup::SourceSharedPointer& triggerSource );
			
			/**
			 * @brief Builds a snippet string with tab stops for a function's parameters.
			 * @param funcType The function type containing parameter names and types.
			 * @return A snippet-formatted string, e.g. "(${1:x}, ${2:y})$0".
			 */
			std::string buildSnippetForFunction( const FunctionTypeSharedPointer& funcType );
			
			/**
			 * @brief Calculates the memory offsets and size for a class.
			 * @param classType The class type to process.
			 */
			void computeMemoryLayout( ClassTypeSharedPointer classType );
			
			/**
			 * @brief Removes the current scope and returns to the parent scope.
			 */
			void popScope();
			
			/**
			 * @brief Populates a class type's fields, base class, interfaces, and method signatures from its AST.
			 * @param classType The class type to populate.
			 */
			void populateClassMembers( ClassTypeSharedPointer classType );
			
			/**
			 * @brief Populates an interface type's generic params, super interfaces, and method signatures.
			 * @param interfaceType The interface type to populate.
			 */
			void populateInterfaceMethods( InterfaceTypeSharedPointer interfaceType );
			
			/**
			 * @brief Populates a struct type's fields and method signatures from its AST.
			 * @param structType The struct type to populate.
			 */
			void populateStructFields( StructTypeSharedPointer structType );
			
			/**
			 * @brief Creates a new scope level.
			 * @param kind The category of scope to be created.
			 */
			void pushScope( Scope::Kind kind );
			
			/**
			 * @brief Performs the first pass registration of type declarations.
			 * @param declaration The declaration shared pointer.
			 */
			void registerTypeDeclaration( ast::nodes::DeclarationSharedPointer& declaration, const std::string& parentQualified = "" );
			
			/**
			 * @brief Resolves a high-level AST type node into a semantic Type.
			 * @param typeNode The AST node representing the type.
			 * @return The resolved TypeSharedPointer.
			 */
			TypeSharedPointer resolveType( const ast::nodes::TypeNodeSharedPointer& typeNode );
			
			/**
			 * @brief Looks up a type in the registry by its string identifier.
			 * @param name The name of the type.
			 * @return The resolved TypeSharedPointer.
			 */
			TypeSharedPointer resolveTypeByName( const std::string& name );
			
			TypeSharedPointer monomorphizeGenericType( const std::string& baseName, const std::vector<TypeSharedPointer>& typeArgs, const lookup::SourceSharedPointer& source );
			std::string semanticTypeToRegistryName( const TypeSharedPointer& type );
			
			/**
			 * @brief Validates access modifiers (public, private, etc.) for a symbol access.
			 * @param symbol The symbol being accessed.
			 * @param source The source location of the access.
			 */
			void validateAccessControl( const SymbolSharedPointer& symbol, const lookup::SourceSharedPointer& source );
			
			/**
			 * @brief Validates the integrity of a class's inheritance hierarchy.
			 * @param declaration The class declaration AST node.
			 * @param classType The resolved semantic class type.
			 */
			void validateClassHierarchy( ast::nodes::ClassDeclaration& declaration, ClassTypeSharedPointer classType );
			
			/**
			 * @brief Ensures that a type correctly implements all methods of an interface.
			 * @param implementationType The type attempting the implementation.
			 * @param interfaceType The interface being implemented.
			 * @param source The source location of the implementation.
			 */
			void validateInterfaceImplementation( TypeSharedPointer implementationType, TypeSharedPointer interfaceType, const lookup::SourceSharedPointer& source );
			
			/** @brief The position in the source code where code completion was triggered. */
			lookup::SourceSharedPointer completionPosition;
			
			/** @brief A list of available completion items discovered during analysis. */
			std::vector<CompletionItem> completionItems;
			
			/** @brief The types that are currently allowed to be thrown/raised in the current context. */
			std::vector<TypeSharedPointer> currentExceptionTypes;
			
			/** @brief The expected return type of the function currently being analyzed. */
			TypeSharedPointer currentReturnType;
			
			ScopeSharedPointer currentScope;
			ScopeSharedPointer globalScope_;
			
			/** @brief Reference to the diagnostic engine for reporting errors and warnings. */
			diagnostic::Engine& diagnostic;
			
			/** @brief Flag indicating if the analysis is currently inside an asynchronous function. */
			bool isInsideAsyncFunction = false;
			
			/** @brief Flag indicating if the analysis is currently within a try-catch block. */
			bool isInsideTryBlock = false;
			
			/** @brief The current package name during analysis. */
			std::string currentPackageName;
			
			/** @brief The registry containing all known primitive and user-defined types. */
			Registry typeRegistry;
		
	};
	
} // namespace uranite::semantic

#endif // end _URANITE_SEMANTIC_ANALYZER_HPP_
