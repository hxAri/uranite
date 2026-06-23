
//
// @author hxAri (hxari)
// @create 2026-06-15 05:00
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

#ifndef _URANITE_FMT_FORMATTER_FORMATTER_HPP_
#define _URANITE_FMT_FORMATTER_FORMATTER_HPP_

#include <string>

#include "uranite/visitors/visitor.hpp"
#include "uranite-fmt/formatter/formatting-rules.hpp"
#include "uranite-fmt/formatter/source-writer.hpp"

namespace uranite::formatter {
	
	/**
	 * @class Formatter
	 * @brief Concrete AST visitor engine responsible for pretty-printing and formatting Uranite syntax.
	 *
	 * Inheriting from the master ASTVisitor hierarchy, this engine traverses the complete node tree 
	 * of a parsed program. It handles code styling rules, dynamic tab stop indentations, and block 
	 * expansions by delegating specific node structural reconstructions to dedicated visit passes.
	 */
	class Formatter : public visitors::ASTVisitor {
			
		public:
				
			/**
			 * @brief Constructs a Formatter using default configuration rules.
			 */
			Formatter();
			
			/**
			 * @brief Constructs a Formatter with fully customized configuration styling options.
			 * @param formattingRules Explicit layout rules (e.g., brace positions, tab sizes, spacing).
			 */
			explicit Formatter( const FormattingRules& formattingRules );
				
			/**
			 * @brief Entry point driver to initiate pretty-printing formatting on a root program node.
			 * @param program Reference to the root program AST node to format.
			 * @return std::string The resulting perfectly formatted plain text source code string.
			 */
			std::string format( ast::nodes::Program& program );
			
			/** @brief Pretty-prints an array expression context (e.g., `[1, 2, 3]`). */
			void visit( ast::nodes::ArrayExpression& node ) override;
			
			/** @brief Pretty-prints a variable assignment mutation (e.g., `x = value;`). */
			void visit( ast::nodes::AssignStatement& node ) override;
			
			/** @brief Pretty-prints mathematical or logical binary operation node tracks (e.g., `a + b`). */
			void visit( ast::nodes::BinaryExpression& node ) override;
			
			/** @brief Pretty-prints a brace-enclosed compound statement scope block. */
			void visit( ast::nodes::BlockStatement& node ) override;
			
			/** @brief Pretty-prints a boolean literal keyword (`true` or `false`). */
			void visit( ast::nodes::BoolLiteralExpression& node ) override;
			
			/** @brief Pretty-prints a loop control break instruction statement. */
			void visit( ast::nodes::BreakStatement& node ) override;
			
			/** @brief Pretty-prints a standalone global or bound call expression frame (e.g., `invoke()`). */
			void visit( ast::nodes::CallExpression& node ) override;
			
			/** @brief Pretty-prints type casting expressions (e.g., `value as float64`). */
			void visit( ast::nodes::CastExpression& node ) override;
			
			/** @brief Pretty-prints a primitive character token literal value. */
			void visit( ast::nodes::CharLiteralExpression& node ) override;
			
			/** @brief Pretty-prints an Object-Oriented class definition block. */
			void visit( ast::nodes::ClassDeclaration& node ) override;
			
			/** @brief Pretty-prints explicit initialization type construct allocations. */
			void visit( ast::nodes::ConstructExpression& node ) override;
			
			/** @brief Pretty-prints a loop control continue instruction statement. */
			void visit( ast::nodes::ContinueStatement& node ) override;
			
			/** @brief Pretty-prints an scope-deferred cleanup routine statement (e.g., `defer cleanup()`). */
			void visit( ast::nodes::DeferStatement& node ) override;
			
			/** @brief Pretty-prints a manual resource memory deallocation statement. */
			void visit( ast::nodes::DeleteStatement& node ) override;
			
			/** @brief Pretty-prints a typed enum declaration structure wrapper. */
			void visit( ast::nodes::EnumDeclaration& node ) override;
			
			/** @brief Pretty-prints an individual expression evaluated as a structural statement boundary. */
			void visit( ast::nodes::ExpressionStatement& node ) override;
			
			/** @brief Pretty-prints foreign linkage declarations linking native C signatures. */
			void visit( ast::nodes::ExternDeclaration& node ) override;
			
			/** @brief Pretty-prints primitive floating-point value tracking nodes. */
			void visit( ast::nodes::FloatLiteralExpression& node ) override;
			
			/** @brief Pretty-prints standard iteration statement components (`for` loops). */
			void visit( ast::nodes::ForStatement& node ) override;
			
			/** @brief Pretty-prints a standalone or structural method routine function block signature and body. */
			void visit( ast::nodes::FunctionDeclaration& node ) override;
			
			/** @brief Pretty-prints variable lookup token markers and generic symbol identifiers. */
			void visit( ast::nodes::IdentifierExpression& node ) override;
			
			/** @brief Pretty-prints conditional selection evaluation statements (`if-else` blocks). */
			void visit( ast::nodes::IfStatement& node ) override;
			
			/** @brief Pretty-prints explicit interface/trait implementation blocks (`impl Trait for Target`). */
			void visit( ast::nodes::ImplementDeclaration& node ) override;
			
			/** @brief Pretty-prints module dependency tracking declarations (e.g., `import std::io;`). */
			void visit( ast::nodes::ImportDeclaration& node ) override;
			
			/** @brief Pretty-prints collection index access expressions (e.g., `matrix[i]`). */
			void visit( ast::nodes::IndexExpression& node ) override;
			
			/** @brief Pretty-prints primitive constant integer literal value representations. */
			void visit( ast::nodes::IntegerLiteralExpression& node ) override;
			
			/** @brief Pretty-prints abstract interface contract structural definitions. */
			void visit( ast::nodes::InterfaceDeclaration& node ) override;
			
			/** @brief Pretty-prints localized anonymous function definitions or lambda block enclosures. */
			void visit( ast::nodes::LambdaExpression& node ) override;
			
			/** @brief Pretty-prints structural pattern-matching branch statements (`match` blocks). */
			void visit( ast::nodes::MatchStatement& node ) override;
			
			/** @brief Pretty-prints member accessor property operations (e.g., `instance.field`). */
			void visit( ast::nodes::MemberAccessExpression& node ) override;
			
			/** @brief Pretty-prints bound instance method invokers (e.g., `instance.action()`). */
			void visit( ast::nodes::MethodCallExpression& node ) override;
			
			/** @brief Pretty-prints top-level namespace module tracking units. */
			void visit( ast::nodes::ModuleDeclaration& node ) override;
			
			/** @brief Pretty-prints a null/empty tracking state literal (`none`). */
			void visit( ast::nodes::NoneLiteralExpression& node ) override;
			
			/** @brief Pretty-prints a semantic fallback placeholder block statement (`pass`). */
			void visit( ast::nodes::PassStatement& node ) override;
			
			/** @brief Pretty-prints the root tracking context of a compiled program payload sheet. */
			void visit( ast::nodes::Program& node ) override;
			
			/** @brief Pretty-prints sequential interval range expressions (e.g., `0..10`). */
			void visit( ast::nodes::RangeExpression& node ) override;
			
			/** @brief Pretty-prints a function exit payload transfer execution statement (`return`). */
			void visit( ast::nodes::ReturnStatement& node ) override;
			
			/** @brief Pretty-prints instance context references self pointers (`self`). */
			void visit( ast::nodes::SelfExpression& node ) override;
			
			/** @brief Pretty-prints inline string constant literals. */
			void visit( ast::nodes::StringLiteralExpression& node ) override;
			
			/** @brief Pretty-prints a systems-level raw data layout structure component (`struct`). */
			void visit( ast::nodes::StructDeclaration& node ) override;
			
			/** @brief Pretty-prints base derivation parent class accessor pointers (`super`). */
			void visit( ast::nodes::SuperExpression& node ) override;
			
			/** @brief Pretty-prints error emission stack control statements (`throw`). */
			void visit( ast::nodes::ThrowStatement& node ) override;
			
			/** @brief Pretty-prints explicit error safety catching scopes (`try-catch` blocks). */
			void visit( ast::nodes::TryCatchStatement& node ) override;
			
			/** @brief Pretty-prints heterogeneous grouped data structures (e.g., `(x, y)`). */
			void visit( ast::nodes::TupleExpression& node ) override;
			
			/** @brief Pretty-prints custom type aliasing or nickname tracking mapping rules. */
			void visit( ast::nodes::TypeAliasDeclaration& node ) override;
			
			/** @brief Pretty-prints single-operand prefix/postfix modifier expressions (e.g., `-value`, `!flag`). */
			void visit( ast::nodes::UnaryExpression& node ) override;
			
			/** @brief Pretty-prints explicit raw pointer/low-level systems execution blocks (`unsafe` blocks). */
			void visit( ast::nodes::UnsafeBlockStatement& node ) override;
			
			/** @brief Pretty-prints local or package-level variable initialization statements. */
			void visit( ast::nodes::VariableStatement& node ) override;
			
			/** @brief Pretty-prints foundational expression loop iterations (`while` loops). */
			void visit( ast::nodes::WhileStatement& node ) override;
			
		private:
			
			/**
			 * @brief Converts an access modifier enum token into its exact lowercase string equivalent.
			 * @param accessModifier The visibility enumeration modifier.
			 * @return std::string Clear keyword text (e.g., "public", "private").
			 */
			std::string accessModifierToString( ast::AccessModifier accessModifier );
				
			/**
			 * @brief Emits a collection of statements belonging to a structural body block, applying indentation rules.
			 * @param bodyStatements Vector of statement pointers to write.
			 */
			void emitBody( const std::vector<ast::nodes::StatementSharedPointer>& bodyStatements );
			
			/**
			 * @brief Formats and writes an isolated constant variable definition component.
			 * @param node The target constant declaration node.
			 */
			void emitConstantDeclaration( ast::nodes::ConstantDeclaration& node );
			
			/**
			 * @brief Formats visibility export decorators preceding modular type expressions.
			 * @param node The target export declaration node context.
			 */
			void emitExportDeclaration( ast::nodes::ExportDeclaration& node );
			
			/**
			 * @brief Dispatches the visitor to format and stream an isolated expression node.
			 * @param expression Shared pointer to the target expression layout.
			 */
			void emitExpression( const ast::nodes::ExpressionSharedPointer& expression );
			
			/**
			 * @brief Formats functional argument variable vectors, handling custom spacing and comma limits.
			 * @param parameters Reference vector containing function argument declaration nodes.
			 */
			void emitFunctionParameters( const std::vector<ast::nodes::FunctionParameterSharedPointer>& parameters );
			
			/**
			 * @brief Formats generic type abstraction parameters within angle bracket matrices (e.g., `<T, U>`).
			 * @param genericParameters Reference vector containing generic parameter definitions.
			 */
			void emitGenericParameters( const std::vector<ast::nodes::GenericParameterSharedPointer>& genericParameters );
			
			/**
			 * @brief Emits raw inline assembly blocks directly into code writer line paths.
			 * @param asmStatement Structural inline assembly statement tracker.
			 */
			void emitInlineAssembly( ast::nodes::InlineAssemblyStatement& asmStatement );
			
			/**
			 * @brief Unpacks type node signatures and writes them cleanly into the active stream.
			 * @param typeNode Shared tracking type node pointer.
			 */
			void emitType( const ast::nodes::TypeNodeSharedPointer& typeNode );
				
			/**
			 * @brief Converts operational token types into their textual representation (e.g., `token::Type::Plus` to `"+"`).
			 * @param operatorType The primitive structural operator symbol enum.
			 * @return std::string The printable string character literal.
			 */
			std::string operatorToString( token::Type operatorType );
				
			/**
			 * @brief Traverses a complex type node configuration to resolve it down to a plain text type name.
			 * @param typeNode Target type signature descriptor node.
			 * @return std::string The compiled data type string representation.
			 */
			std::string typeToString( const ast::nodes::TypeNodeSharedPointer& typeNode );
				
			/**
			 * @brief Dispatch helper that routes a general declaration pointer down to its specific concrete visitor method.
			 * @param declaration Shared pointer wrapping a general declaration node instance.
			 */
			void visitDeclaration( const ast::nodes::DeclarationSharedPointer& declaration );
			
			/**
			 * @brief Dispatch helper that routes a general expression pointer down to its specific concrete visitor method.
			 * @param expression Shared pointer wrapping a general expression node instance.
			 */
			void visitExpression( const ast::nodes::ExpressionSharedPointer& expression );
			
			/**
			 * @brief Dispatch helper that routes a general statement pointer down to its specific concrete visitor method.
			 * @param statement Shared pointer wrapping a general statement node instance.
			 */
			void visitStatement( const ast::nodes::StatementSharedPointer& statement );
				
			/** @brief Configuration dataset tracking active style rules. */
			FormattingRules formattingRules_;
			
			/** @brief Internal state flag to suppress line breaks when formatting compressed inline expressions. */
			bool isInlineExpression_;
			
			/** @brief High-performance text streaming writer wrapper maintaining tab stop indentation states. */
			SourceWriter writer_;
		
	};

} // namespace uranite::formatter

#endif // _URANITE_FMT_FORMATTER_FORMATTER_HPP_
