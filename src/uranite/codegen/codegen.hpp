
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

#ifndef _URANITE_CODEGEN_CODEGEN_HPP_
#define _URANITE_CODEGEN_CODEGEN_HPP_


#include <functional>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <unordered_set>
#include <vector>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

#include "uranite/ast/node.hpp"
#include "uranite/codegen/context.hpp"
#include "uranite/codegen/runtime-interface.hpp"
#include "uranite/lookup/source.hpp"
#include "uranite/descriptor/descriptor.hpp"
#include "uranite/diagnostic/diagnostic.hpp"
#include "uranite/semantic/analyzer.hpp"

namespace uranite::codegen {
	
	/**
	 * @brief Example of a function utilizing the included LLVM headers.
	 * @param context The LLVM context.
	 * @param moduleName The identifier for the module.
	 * @return A unique pointer to the generated LLVM module.
	 */
	inline std::unique_ptr<llvm::Module> createModule( llvm::LLVMContext& context, const std::string& moduleName ) {
		return std::make_unique<llvm::Module>( moduleName, context );
	}
	
	/**
	 * @brief The LLVM Code Generator responsible for transforming the AST into LLVM Intermediate Representation.
	 */
	class LLVMCodegen final {
		
		public:
		
			/**
			 * @brief Constructs the code generator with semantic and diagnostic support.
			 * @param semanticAnalyzer Reference to the analyzer for type and symbol information.
			 * @param diagnosticEngine Reference to the engine for reporting compilation errors.
			 */
			LLVMCodegen( semantic::Analyzer& semanticAnalyzer, diagnostic::Engine& diagnosticEngine );

			/**
			 * @brief Overrides the target triple for cross-compilation.
			 * @param triple The LLVM target triple string (e.g., "aarch64-linux-gnu").
			 */
			void setTargetTriple( const std::string& triple );

			/**
			 * @brief Dumps the current state of the LLVM module to standard error.
			 */
			void dump();
			
			/**
			 * @brief Entry point for generating LLVM IR from the program AST.
			 * @param program The root node of the Abstract Syntax Tree.
			 * @return True if generation was successful, false otherwise.
			 */
			bool generate( ast::nodes::Program& program );
			
			/**
			 * @brief Retrieves the underlying LLVM module.
			 * @return Pointer to the LLVM Module.
			 */
			llvm::Module* getModule() {
				return this->module.get();
			}
			
			/**
			 * @brief Writes the generated IR to a text file.
			 * @param filename The destination file path.
			 * @return True if writing succeeded.
			 */
			bool writeIR( const std::string& filename );
			
			/**
			 * @brief Compiles and writes the IR to a binary object file.
			 * @param filename The destination file path.
			 * @return True if writing succeeded.
			 */
			bool writeObject( const std::string& filename );
			
		private:
			
			// --- Declaration Generation ---
			
			/**
			 * @brief Generates code for a class declaration node.
			 * @param declaration The class declaration AST node to process.
			 */
			void preRegisterClassMethods( ast::nodes::ClassDeclaration& declaration, const semantic::ClassTypeSharedPointer& classType );
			bool ensureClassMethodsRegistered( const std::string& className );
			void generateClassDeclaration( ast::nodes::ClassDeclaration& declaration );
			
			/**
			 * @brief Generates code for a generic declaration node.
			 * @param declaration A shared pointer to the declaration AST node.
			 */
			void generateDeclaration( const ast::nodes::DeclarationSharedPointer& declaration );
			
			/**
			 * @brief Generates code for an enumeration declaration node.
			 * @param declaration The enum declaration AST node to process.
			 */
			void generateEnumDeclaration( ast::nodes::EnumDeclaration& declaration );
			
			/**
			 * @brief Generates code for an external (extern) declaration node.
			 * @param declaration The extern declaration AST node to process.
			 */
			void generateExternDeclaration( ast::nodes::ExternDeclaration& declaration );
			
			/**
			 * @brief Generates code for a function declaration node.
			 * @param declaration The function declaration AST node to process.
			 */
			void generateFunctionDeclaration( ast::nodes::FunctionDeclaration& declaration );
			
			/**
			 * @brief Generates code for a struct declaration node.
			 * @param declaration The struct declaration AST node to process.
			 */
			void generateStructDeclaration( ast::nodes::StructDeclaration& declaration );
			
			// --- Statement Generation ---
			
			/**
			 * @brief Generates code for an assignment statement.
			 * @param statement The assignment statement node to be processed.
			 */
			void generateAssignmentStatement( ast::nodes::AssignStatement& statement );
			
			/**
			 * @brief Generates code for a block of statements.
			 * @param statements A vector of shared pointers to the statements within the block.
			 */
			void generateBlock( const std::vector<ast::nodes::StatementSharedPointer>& statements );
			
			/**
			 * @brief Generates code for a for-loop statement.
			 * @param statement The for-loop statement node to be processed.
			 */
			void generateForStatement( ast::nodes::ForStatement& statement );
			
			/**
			 * @brief Generates code for an if-conditional statement.
			 * @param statement The if-statement node to be processed.
			 */
			void generateIfStatement( ast::nodes::IfStatement& statement );
			
			/**
			 * @brief Generates code for a match-pattern statement.
			 * @param statement The match statement node to be processed.
			 */
			void generateMatchStatement( ast::nodes::MatchStatement& statement );
			
			/**
			 * @brief Generates code for a return statement.
			 * @param statement The return statement node to be processed.
			 */
			void generateReturnStatement( ast::nodes::ReturnStatement& statement );
			
			/**
			 * @brief Dispatches a generic statement node to its specific generation logic.
			 * @param statement A shared pointer to the statement node.
			 */
			void generateStatement( const ast::nodes::StatementSharedPointer& statement );
			
			/**
			 * @brief Generates code for a switch-case statement.
			 * @param statement The switch statement node to be processed.
			 */
			void generateSwitchStatement( ast::nodes::SwitchStatement& statement );
			
			/**
			 * @brief Generates code for a throw (exception) statement.
			 * @param statement The throw statement node to be processed.
			 */
			void generateThrowStatement( ast::nodes::ThrowStatement& statement );
			
			/**
			 * @brief Generates code for a try-catch block statement.
			 * @param statement The try-catch statement node to be processed.
			 */
			void generateTryCatchStatement( ast::nodes::TryCatchStatement& statement );
			
			/**
			 * @brief Generates code for a (variable declaration) statement.
			 * @param statement The variable statement node to be processed.
			 */
			void generateVariableStatement( ast::nodes::VariableStatement& statement );
			
			/**
			 * @brief Generates code for a while-loop statement.
			 * @param statement The while-statement node to be processed.
			 */
			void generateWhileStatement( ast::nodes::WhileStatement& statement );
			
			void generateInlineAssemblyStatement( ast::nodes::InlineAssemblyStatement& statement );
			
			// --- Expression Generation ---
			
			/**
			 * @brief Generates LLVM IR for a binary operation expression.
			 * @param expression The binary expression node containing the operator and operands.
			 * @return A pointer to the generated LLVM Value.
			 */
			llvm::Value* generateBinaryExpression( ast::nodes::BinaryExpression& expression );
			
			/**
			 * @brief Generates LLVM IR for a standard function call expression.
			 * @param expression The call expression node containing the callee and arguments.
			 * @return A pointer to the generated LLVM Value.
			 */
			llvm::Value* generateCallExpression( ast::nodes::CallExpression& expression );
			llvm::Value* generateBuiltinPuts( ast::nodes::CallExpression& expression );
			llvm::Value* coerceValueToString( llvm::Value* value );

			/**
			 * @brief Generates LLVM IR for a constructor or object construction expression.
			 * @param expression The construction expression node.
			 * @return A pointer to the generated LLVM Value.
			 */
			llvm::Value* generateConstructExpression( ast::nodes::ConstructExpression& expression );
			
			/**
			 * @brief Generates the LLVM IR value for an `uranite.memory.arena.Arena` construction expression.
			 * @param expression The AST node representing the construct expression.
			 * @return A pointer to the generated LLVM Value.
			 */
			llvm::Value* generateConstructArenaExpression( ast::nodes::ConstructExpression& expression );
			
			/**
			 * @brief Generates the LLVM IR value for a `uranite.memory.memory.Memory` construction expression.
			 * @param expression The AST node representing the construct expression.
			 * @return A pointer to the generated LLVM Value.
			 */
			llvm::Value* generateConstructMemoryExpression( ast::nodes::ConstructExpression& expression );
			
			/**
			 * @brief The entry point for generating LLVM IR from any generic expression node.
			 * @param expression A shared pointer to the base expression node.
			 * @return A pointer to the generated LLVM Value.
			 */
			llvm::Value* generateExpression( const ast::nodes::ExpressionSharedPointer& expression );
			
			/**
			 * @brief Generates LLVM IR to retrieve the value of an identifier (variable).
			 * @param expression The identifier expression node.
			 * @return A pointer to the generated LLVM Value.
			 */
			llvm::Value* generateIdentifierExpression( ast::nodes::IdentifierExpression& expression );
			
			/**
			 * @brief Generates LLVM IR for an indexing operation (e.g., array or map access).
			 * @param expression The index expression node.
			 * @return A pointer to the generated LLVM Value.
			 */
			llvm::Value* generateIndexExpression( ast::nodes::IndexExpression& expression );
			
			/**
			 * @brief Generates LLVM IR for a lambda (anonymous function) expression.
			 * @param expression The lambda expression node.
			 * @return A pointer to the generated LLVM Value representing the lambda function or closure.
			 */
			llvm::Value* generateLambdaExpression( ast::nodes::LambdaExpression& expression );
			
			/**
			 * @brief Generates LLVM IR for a pattern matching (match) expression.
			 * @param expression The match expression node containing branches and patterns.
			 * @return A pointer to the generated LLVM Value resulting from the match.
			 */
			llvm::Value* generateMatchExpression( ast::nodes::MatchExpression& expression );
			
			/**
			 * @brief Generates LLVM IR for accessing a member of a struct or class.
			 * @param expression The member access expression node.
			 * @return A pointer to the generated LLVM Value (usually a pointer to the member).
			 */
			llvm::Value* generateMemberAccessExpression( ast::nodes::MemberAccessExpression& expression );
			
			/**
			 * @brief Generates LLVM IR for a method call on a specific object instance.
			 * @param expression The method call expression node.
			 * @return A pointer to the generated LLVM Value.
			 */
			llvm::Value* generateMethodCallExpression( ast::nodes::MethodCallExpression& expression );
			
			/**
			 * @brief Generates LLVM IR for a unary operation expression (e.g., negation).
			 * @param expression The unary expression node.
			 * @return A pointer to the generated LLVM Value.
			 */
			llvm::Value* generateUnaryExpression( ast::nodes::UnaryExpression& expression );
			
			// --- Interface & Virtual Table Helpers ---
			
			/**
			 * @brief Creates a fat pointer for an interface, combining the object instance and its interface table.
			 * @param objectPointer The LLVM value pointing to the actual object instance.
			 * @param className The name of the class implementing the interface.
			 * @param interfaceName The name of the interface being referenced.
			 * @return An LLVM Value representing the constructed fat pointer struct.
			 */
			llvm::Value* createInterfaceFatPointer( llvm::Value* objectPointer, const std::string& className, const std::string& interfaceName );
			
			/**
			 * @brief Generates the LLVM IR for a method call dispatched through an interface fat pointer.
			 * @param expression The AST node representing the method call expression.
			 * @param fatPointer The LLVM fat pointer value containing the instance and interface table.
			 * @param interfaceName The name of the interface where the method is defined.
			 * @return An LLVM Value representing the result of the method call.
			 */
			llvm::Value* generateInterfaceMethodCall( ast::nodes::MethodCallExpression& expression, llvm::Value* fatPointer, const std::string& interfaceName );
			llvm::Value* generateInterfacePropertyAccess( llvm::Value* interfaceFatPointer, const semantic::InterfaceTypeSharedPointer& interfaceType, semantic::MethodInfo* methodInfo );
			
			/**
			 * @brief Generates the interface table (ITable) for a specific class-interface mapping.
			 * @param className The name of the class.
			 * @param classType Shared pointer to the semantic information of the class.
			 * @param interfaceType Shared pointer to the semantic information of the interface.
			 */
			void generateInterfaceTable( const std::string& className, const semantic::ClassTypeSharedPointer& classType, const semantic::InterfaceTypeSharedPointer& interfaceType );
			
			/**
			 * @brief Generates the virtual method table (VTable) for a given class.
			 * @param className The name of the class.
			 * @param classType Shared pointer to the semantic information of the class.
			 */
			void generateVirtualTable( const std::string& className, const semantic::ClassTypeSharedPointer& classType );
			
			/**
			 * @brief Retrieves the LLVM struct type defined for interface fat pointers.
			 * @return An LLVM StructType representing the { instance_pointer, interface_table_pointer } struct.
			 */
			llvm::StructType* getInterfaceFatPointerType();
			
			// --- General Helpers ---
			
			/**
             * @brief Collects variables that are used in an expression but not defined within its scope.
             * @param expression The expression node to analyze.
             * @param bound A set of variable names that are already bound in the current scope.
             * @param freeVariables A vector to be populated with the names of discovered free variables.
             */
            void collectFreeVariables( const ast::nodes::ExpressionSharedPointer& expression, const std::unordered_set<std::string>& bound, std::vector<std::string>& freeVariables );
			
            /**
             * @brief Collects free variables within a statement and updates the bound variables set.
             * @param statement The statement node to analyze.
             * @param bound A set of currently bound variable names, which may be modified as new bindings occur.
             * @param freeVariables A vector to be populated with the names of discovered free variables.
             */
            void collectFreeVariablesInStatement( const ast::nodes::StatementSharedPointer& statement, std::unordered_set<std::string>& bound, std::vector<std::string>& freeVariables );
			
            /**
             * @brief Creates an alloca instruction in the entry block of a function.
             * @param function The LLVM function where the alloca will be placed.
             * @param name The name for the newly created variable.
             * @param type The LLVM type to allocate.
             * @return A pointer to the created AllocaInst.
             */
            llvm::AllocaInst* createEntryBlockAllocation( llvm::Function* function, const std::string& name, llvm::Type* type );
			
            /**
             * @brief Generates an LLVM value representing an implicit type conversion.
             * @param value The original LLVM value to cast.
             * @param targetType The destination LLVM type.
             * @return The LLVM Value resulting from the cast operation.
             */
            llvm::Value* generateImplicitCast( llvm::Value* value, llvm::Type* targetType, bool isUnsigned = false );
			
            /**
             * @brief Retrieves an existing LLVM struct type or creates a new one if it does not exist.
             * @param name The name of the struct.
             * @param type The semantic type information used to define the struct.
             * @return A pointer to the LLVM StructType.
             */
            llvm::StructType* getOrCreateStructType( const std::string& name, const semantic::TypeSharedPointer& type );
			
            /**
             * @brief Mangles a name to ensure uniqueness, optionally including a class context.
             * @param name The base name to mangle.
             * @param className The optional class name to include in the mangled identity.
             * @return The resulting mangled string.
             */
            std::string buildMonomorphizedName( const ast::nodes::TypeNodeSharedPointer& typeNode );
            std::string mangleName( const std::string& name, const std::string& className = "" );
			
            /**
             * @brief Resolves an Abstract Syntax Tree type node into its corresponding LLVM type.
             * @param typeNode The AST type node to resolve.
             * @return A pointer to the resolved LLVM type.
             */
            llvm::Type* resolveAstType( const ast::nodes::TypeNodeSharedPointer& typeNode );
			
            /**
             * @brief Resolves an expression to obtain a pointer to the underlying object.
             * @param expression The expression representing an object.
             * @return An LLVM Value pointer to the object's memory location.
             */
            llvm::Value* resolveObjectPointer( const ast::nodes::ExpressionSharedPointer& expression );
			
            /**
             * @brief Determines the name of a struct type from a given expression.
             * @param expression The expression to analyze for type naming.
             * @return The string name of the resolved struct type.
             */
            std::string resolveStructTypeName( const ast::nodes::ExpressionSharedPointer& expression );
			
            /**
             * @brief Converts a semantic type reference into its equivalent LLVM type representation.
             * @param type The semantic type to convert.
             * @return A pointer to the resulting LLVM type.
             */
            llvm::Type* toLLVMType( const semantic::TypeSharedPointer& type );
			
			llvm::Function* getOrCreatePersonalityFunction();
			llvm::Function* getOrCreateUraniteThrow();
			llvm::Function* getOrCreateUraniteBeginCatch();
			llvm::Function* getOrCreateUraniteEndCatch();
			llvm::Function* getOrCreatePushFrame();
			llvm::Function* getOrCreatePopFrame();
			
			void emitPushFrame( const std::string& file, int64_t line, int64_t column, const std::string& functionName );
			void emitPopFrame();
			
			llvm::Value* createCallOrInvoke( llvm::Function* callee, llvm::ArrayRef<llvm::Value*> args, const llvm::Twine& name = "" );
			llvm::Value* createCallOrInvoke( llvm::FunctionType* type, llvm::Value* callee, llvm::ArrayRef<llvm::Value*> args, const llvm::Twine& name = "" );

			llvm::Function* getOrCreateFree();
			llvm::Function* getOrCreateMalloc();
			llvm::Function* getOrCreateCalloc();
			llvm::Function* getOrCreateMemoryCopy();
			llvm::Function* getOrCreateSchedulerInit();
			llvm::Function* getOrCreateRunScheduler();
			
			/**
			 * @brief Provides access to the formatted string output utility.
			 * @return An LLVM Function pointer for the 'snprintf' operation.
			 */
			llvm::Function* getOrCreateSnprintf();
			
			/**
			 * @brief Provides access to the string concatenation utility.
			 * @return An LLVM Function pointer for the 'strcat' operation.
			 */
			llvm::Function* getOrCreateStringConcatenate();
			
			/**
			 * @brief Provides access to the string comparison utility.
			 * @return An LLVM Function pointer for the 'strcmp' operation.
			 */
			llvm::Function* getOrCreateStringCompare();
			
			/**
			 * @brief Provides access to the string copy utility.
			 * @return An LLVM Function pointer for the 'strcpy' operation.
			 */
			llvm::Function* getOrCreateStringCopy();
			
			/**
			 * @brief Provides access to the string length measurement utility.
			 * @return An LLVM Function pointer for the 'strlen' operation.
			 */
			llvm::Function* getOrCreateStringLength();
			
			llvm::Function* getOrCreateStringHash();
			
			/**
			 * @brief Registers all built-in structural types into the compiler's type system.
			 * 
			 * This method initializes internal representations for structs that are
			 * natively supported by the language runtime.
			 */
			void registerBuiltInStructTypes();
			
			/**
			 * @brief Attempts to resolve and generate a call to a built-in type descriptor or method.
			 * 
			 * This function checks if a specific method exists for a given built-in type
			 * and handles the generation of the corresponding LLVM IR.
			 * 
			 * @param typeName The name of the built-in type being accessed.
			 * @param methodName The name of the method or descriptor to invoke.
			 * @param self The LLVM Value pointer representing the instance ('self' or 'this').
			 * @param arguments A vector of LLVM Value pointers to be passed as arguments to the method.
			 * @return llvm::Value* The resulting LLVM Value from the operation, or nullptr if not found.
			 */
			llvm::Value* tryBuiltInDescriptor(
				const std::string& typeName,
				const std::string& methodName,
				llvm::Value* self,
				std::vector<llvm::Value*>& arguments,
				std::vector<std::pair<std::string, llvm::Value*>>& keywordArguments
			);
			
			llvm::Value* unwrapOOPWrapperValue( llvm::Value* value, const std::string& wrapperTypeHint = "" );

			bool isThrowableClass( const std::string& typeName );
			void injectTracebackInfo( llvm::Value* objectPointer, const std::string& typeName, const lookup::SourceSharedPointer& source, bool overwriteFileAndLine = true );
			void generateArithmeticErrorCheck( llvm::Value* condition, const std::string& errorClassName, const std::string& message, const lookup::SourceSharedPointer& source );
			void generateThrowError( const std::string& errorClassName, const std::string& message, const lookup::SourceSharedPointer& source );
			
			/** @brief Reference to the semantic analyzer. */
			semantic::Analyzer& analyzer;
			
			/** @brief Tracking element types for Arena allocations. */
			std::unordered_map<std::string, llvm::Type*> arenaElementTypes;
			
			/** @brief Argument used for asynchronous task execution. */
			llvm::Value* asyncTaskArgument = nullptr;

			/** @brief Stack of landing pad basic blocks for nested try/catch. */
			std::stack<llvm::BasicBlock*> landingPads;
			
			/** @brief Stack for managing break labels in loops. */
			std::stack<llvm::BasicBlock*> breakTargets;
			
			/** @brief The current LLVM context. */
			llvm::LLVMContext context;
			
			/** @brief The LLVM module containing all generated code. */
			std::unique_ptr<llvm::Module> module;
			
			/** @brief Builder for creating LLVM instructions. */
			llvm::IRBuilder<> builder;
			
			/** @brief The registry for built-in types. */
			descriptor::Builtin builtinRegistry;
			
			/** @brief Stack for managing continue labels in loops. */
			std::stack<llvm::BasicBlock*> continueTargets;
			
			/** @brief Name of the class currently being processed. */
			std::string currentClassName;
			
			/** @brief Pointer to the LLVM function currently being generated. */
			llvm::Function* currentFunction = nullptr;
			
			/** @brief Context for the generator currently being generated. */
			GeneratorContext* currentGenerator = nullptr;
			
			/** @brief Hierarchical defer scope stack. Each entry holds defers for one lexical scope. */
			std::vector<std::vector<ast::nodes::StatementSharedPointer>> deferScopeStack;

			void pushDeferScope();
			void popDeferScope();
			void emitCurrentScopeDefers();
			void emitAllDefers();
			void addDefer( ast::nodes::StatementSharedPointer statement );
			
			/** @brief Reference to the diagnostic engine. */
			diagnostic::Engine& diagnostic;
			
			/** @brief Backed values for enum variants. */
			std::unordered_map<std::string, llvm::Constant*> enumBackedValues;
			
			/** @brief Names of all registered enum types. */
			std::unordered_set<std::string> enumTypeNames;
			
			/** @brief Discriminant tags for enum variants. */
			std::unordered_map<std::string, int> enumVariants;
			
			/** @brief Cached LLVM functions. */
			std::unordered_map<std::string, llvm::Function*> functions;
			
			/** @brief Global interface tables. */
			std::unordered_map<std::string, llvm::GlobalVariable*> interfaceTables;
			
			/** @brief The fat pointer type used for interface dispatch. */
			llvm::StructType* interfaceFatPointerType = nullptr;
			
			/** @brief Last used element type for Arena allocations. */
			llvm::Type* lastArenaElementType = nullptr;
			
			/** @brief Last used element type for Memory allocations. */
			llvm::Type* lastMemoryElementType = nullptr;
			
			/** @brief Global counter for generating unique lambda names. */
			unsigned lambdaCounter = 0;
			
			/** @brief Compile-time constant values (survives namedValues.clear()). */
			std::unordered_map<std::string, llvm::Constant*> constantValues;

			/** @brief Global variable declarations needing runtime initialization. */
			std::vector<ast::nodes::ConstantDeclaration*> globalVariableInits;
			
			/** @brief Mapping of variable names to their LLVM values. */
			std::unordered_map<std::string, llvm::Value*> namedValues;
			
			/** @brief Flag indicating if the program requires async runtime. */
			bool programUsesAsync = false;
			bool currentFunctionEmittedPushFrame = false;
			
			/** @brief Indices of fields within struct types. */
			std::unordered_map<std::string, std::unordered_map<std::string, unsigned>> structFieldIndices;
			
			/** @brief Global variables for static class fields. */
			std::unordered_map<std::string, std::unordered_map<std::string, llvm::GlobalVariable*>> staticClassFields;
			
			/** @brief Cached LLVM struct types. */
			std::unordered_map<std::string, llvm::StructType*> structTypes;
			
			/** @brief Tracking element types for Memory allocations. */
			std::unordered_map<std::string, llvm::Type*> varMemoryElementTypes;
			
			/** @brief Mapping of variable names to their respective struct type names. */
			std::unordered_map<std::string, std::string> variableStructType;

			std::string lastConstructedClassName;
			std::string lastTargetInterfaceName;
			
			/** @brief Set of class names whose methods have been pre-registered. */
			std::unordered_set<std::string> preRegisteredClasses;

			std::unordered_map<std::string, int> overloadedFunctionCounts;

			/** @brief Pluggable runtime interface for function name/signature resolution. */
			std::shared_ptr<RuntimeInterface> runtimeInterface_;

			struct ScopeCleanupEntry {
				std::string variableName;
				std::string typeName;
				llvm::Value* allocaPtr;
				llvm::Value* aliveFlag;
				bool implementsDroper;
			};

			std::vector<std::vector<ScopeCleanupEntry>> scopeCleanupStack;
			std::unordered_map<std::string, llvm::Value*> aliveFlags;

			void pushCleanupScope();
			void popCleanupScope();
			void emitScopeCleanup( std::vector<ScopeCleanupEntry>& entries );
			void emitAllScopeCleanups();
			void registerCleanupEntry( const std::string& varName, const std::string& typeName, llvm::Value* alloca, bool implementsDroper );
			
			struct KeywordOnlyParam {
				std::string name;
				llvm::Type* llvmType = nullptr;
				ast::nodes::ExpressionSharedPointer defaultValue;
			};
			struct FunctionParamInfo {
				int variadicIndex = -1;
				int keywordIndex = -1;
				llvm::Type* variadicElementLLVMType = nullptr;
				llvm::Type* keywordValueLLVMType = nullptr;
				std::vector<KeywordOnlyParam> keywordOnlyParams;
			};
			std::unordered_map<std::string, FunctionParamInfo> functionParamInfos;
			
			/** @brief Global virtual tables for classes. */
			std::unordered_map<std::string, llvm::GlobalVariable*> virtualTables;
			
	};
	
} // namespace uranite::codegen

#endif // end _URANITE_CODEGEN_CODEGEN_HPP_
