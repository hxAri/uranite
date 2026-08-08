
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

#ifndef _URANITE_VISITORS_VISITOR_HPP_
#define _URANITE_VISITORS_VISITOR_HPP_

#include "uranite/ast/node.hpp"

namespace uranite::visitors {
	
	/**
	 * @brief Abstract base class for implementing the Visitor pattern on the Abstract Syntax Tree (AST).
	 * 
	 * This class provides a unified interface for traversing and processing different types of nodes
	 * within the AST. Every visit method must be implemented by concrete visitors.
	 */
	class ASTVisitor {
		
		public:
			
			/**
			 * @brief Virtual destructor for proper cleanup of derived visitor objects.
			 */
			virtual ~ASTVisitor() = default;
			
			/** @brief Visits an array literal or initialization expression. */
			virtual void visit( ast::nodes::ArrayExpression& node ) = 0;
			
			/** @brief Visits an assignment statement. */
			virtual void visit( ast::nodes::AssignStatement& node ) = 0;
			
			/** @brief Visits a binary operation expression (e.g., addition, comparison). */
			virtual void visit( ast::nodes::BinaryExpression& node ) = 0;
			
			/** @brief Visits a block of statements enclosed in braces. */
			virtual void visit( ast::nodes::BlockStatement& node ) = 0;
			
			/** @brief Visits a boolean literal expression (true/false). */
			virtual void visit( ast::nodes::BoolLiteralExpression& node ) = 0;
			
			/** @brief Visits a break statement for control flow. */
			virtual void visit( ast::nodes::BreakStatement& node ) = 0;
			
			/** @brief Visits a function or method call expression. */
			virtual void visit( ast::nodes::CallExpression& node ) = 0;
			
			/** @brief Visits a type casting expression. */
			virtual void visit( ast::nodes::CastExpression& node ) = 0;
			
			/** @brief Visits a character literal expression. */
			virtual void visit( ast::nodes::CharLiteralExpression& node ) = 0;
			
			/** @brief Visits a class declaration. */
			virtual void visit( ast::nodes::ClassDeclaration& node ) = 0;
			
			/** @brief Visits a constructor or object creation expression. */
			virtual void visit( ast::nodes::ConstructExpression& node ) = 0;
			
			/** @brief Visits a continue statement for control flow. */
			virtual void visit( ast::nodes::ContinueStatement& node ) = 0;
			
			/** @brief Visits a defer statement for delayed execution. */
			virtual void visit( ast::nodes::DeferStatement& node ) = 0;
			
			/** @brief Visits a delete statement for manual memory deallocation. */
			virtual void visit( ast::nodes::DeleteStatement& node ) = 0;
			
			/** @brief Visits an enumeration declaration. */
			virtual void visit( ast::nodes::EnumDeclaration& node ) = 0;
			
			/** @brief Visits an expression wrapped as a standalone statement. */
			virtual void visit( ast::nodes::ExpressionStatement& node ) = 0;
			
			/** @brief Visits an external linkage declaration (FFI). */
			virtual void visit( ast::nodes::ExternDeclaration& node ) = 0;
			
			/** @brief Visits a floating-point literal expression. */
			virtual void visit( ast::nodes::FloatLiteralExpression& node ) = 0;
			
			/** @brief Visits a for-loop control flow statement. */
			virtual void visit( ast::nodes::ForStatement& node ) = 0;
			
			/** @brief Visits a function definition or declaration. */
			virtual void visit( ast::nodes::FunctionDeclaration& node ) = 0;
			
			/** @brief Visits an identifier expression (variable or symbol reference). */
			virtual void visit( ast::nodes::IdentifierExpression& node ) = 0;
			
			/** @brief Visits an if-else conditional control flow statement. */
			virtual void visit( ast::nodes::IfStatement& node ) = 0;
			
			/** @brief Visits a trait or interface implementation declaration. */
			virtual void visit( ast::nodes::ImplementDeclaration& node ) = 0;
			
			/** @brief Visits an import declaration for external modules. */
			virtual void visit( ast::nodes::ImportDeclaration& node ) = 0;
			
			/** @brief Visits an array or collection index access expression. */
			virtual void visit( ast::nodes::IndexExpression& node ) = 0;
			
			/** @brief Visits an integer literal expression. */
			virtual void visit( ast::nodes::IntegerLiteralExpression& node ) = 0;
			
			/** @brief Visits an interface definition. */
			virtual void visit( ast::nodes::InterfaceDeclaration& node ) = 0;
			
			/** @brief Visits a lambda or anonymous function expression. */
			virtual void visit( ast::nodes::LambdaExpression& node ) = 0;
			
			/** @brief Visits a pattern matching (switch-like) statement. */
			virtual void visit( ast::nodes::MatchStatement& node ) = 0;
			
			/** @brief Visits a member access expression (dot or arrow notation). */
			virtual void visit( ast::nodes::MemberAccessExpression& node ) = 0;
			
			/** @brief Visits a specific method call expression on an object. */
			virtual void visit( ast::nodes::MethodCallExpression& node ) = 0;
			
			/** @brief Visits a module or namespace declaration. */
			virtual void visit( ast::nodes::ModuleDeclaration& node ) = 0;
			
			/** @brief Visits a null or none literal expression. */
			virtual void visit( ast::nodes::NoneLiteralExpression& node ) = 0;
			
			/** @brief Visits a no-op or pass statement. */
			virtual void visit( ast::nodes::PassStatement& node ) = 0;
			
			/** @brief Visits the root program node. */
			virtual void visit( ast::nodes::Program& node ) = 0;
			
			/** @brief Visits a range creation expression (e.g., start..end). */
			virtual void visit( ast::nodes::RangeExpression& node ) = 0;
			
			/** @brief Visits a return statement. */
			virtual void visit( ast::nodes::ReturnStatement& node ) = 0;
			
			/** @brief Visits a self-reference expression (this or self). */
			virtual void visit( ast::nodes::SelfExpression& node ) = 0;
			
			/** @brief Visits a string literal expression. */
			virtual void visit( ast::nodes::StringLiteralExpression& node ) = 0;
			
			/** @brief Visits a struct definition. */
			virtual void visit( ast::nodes::StructDeclaration& node ) = 0;
			
			/** @brief Visits a parent-class reference expression. */
			virtual void visit( ast::nodes::ParentExpression& node ) = 0;
			
			/** @brief Visits an exception throw statement. */
			virtual void visit( ast::nodes::ThrowStatement& node ) = 0;
			
			/** @brief Visits an exception handling block (try-catch). */
			virtual void visit( ast::nodes::TryCatchStatement& node ) = 0;
			
			/** @brief Visits a tuple literal expression. */
			virtual void visit( ast::nodes::TupleExpression& node ) = 0;
			
			/** @brief Visits a type alias or typedef declaration. */
			virtual void visit( ast::nodes::TypeAliasDeclaration& node ) = 0;
			
			/** @brief Visits a unary operation expression (e.g., negation). */
			virtual void visit( ast::nodes::UnaryExpression& node ) = 0;
			
			/** @brief Visits a block that allows unsafe or low-level operations. */
			virtual void visit( ast::nodes::UnsafeBlockStatement& node ) = 0;
			
			/** @brief Visits a variable declaration statement. */
			virtual void visit( ast::nodes::VariableStatement& node ) = 0;
			
			/** @brief Visits a while-loop control flow statement. */
			virtual void visit( ast::nodes::WhileStatement& node ) = 0;
			
	};
	
	/**
	 * @brief Dispatcher helper that routes a generic AST node to its specific visitor method.
	 * 
	 * This function implements the dispatcher component of the Visitor Pattern, performing
	 * a downcast from ast::Node to its derived concrete types based on the NodeKind.
	 * 
	 * @param visitor The ASTVisitor implementation that will process the node.
	 * @param node The generic AST node to be visited.
	 */
	inline void dispatchVisit( ASTVisitor& visitor, ast::Node& node ) {
		switch( node.kind ) {
			case ast::Node::Kind::ArrayExpression:
				visitor.visit( static_cast<ast::nodes::ArrayExpression&>( node ) );
				break;
			case ast::Node::Kind::AssignmentStatement:
				visitor.visit( static_cast<ast::nodes::AssignStatement&>( node ) );
				break;
			case ast::Node::Kind::BinaryExpression:
				visitor.visit( static_cast<ast::nodes::BinaryExpression&>( node ) );
				break;
			case ast::Node::Kind::BlockStatement:
				visitor.visit( static_cast<ast::nodes::BlockStatement&>( node ) );
				break;
			case ast::Node::Kind::BooleanLiteral:
				visitor.visit( static_cast<ast::nodes::BoolLiteralExpression&>( node ) );
				break;
			case ast::Node::Kind::BreakStatement:
				visitor.visit( static_cast<ast::nodes::BreakStatement&>( node ) );
				break;
			case ast::Node::Kind::CallExpression:
				visitor.visit( static_cast<ast::nodes::CallExpression&>( node ) );
				break;
			case ast::Node::Kind::CastExpression:
				visitor.visit( static_cast<ast::nodes::CastExpression&>( node ) );
				break;
			case ast::Node::Kind::CharLiteral:
				visitor.visit( static_cast<ast::nodes::CharLiteralExpression&>( node ) );
				break;
			case ast::Node::Kind::ClassDeclaration:
				visitor.visit( static_cast<ast::nodes::ClassDeclaration&>( node ) );
				break;
			case ast::Node::Kind::ContinueStatement:
				visitor.visit( static_cast<ast::nodes::ContinueStatement&>( node ) );
				break;
			case ast::Node::Kind::ConstructExpression:
				visitor.visit( static_cast<ast::nodes::ConstructExpression&>( node ) );
				break;
			case ast::Node::Kind::DeferStatement:
				visitor.visit( static_cast<ast::nodes::DeferStatement&>( node ) );
				break;
			case ast::Node::Kind::DeleteStatement:
				visitor.visit( static_cast<ast::nodes::DeleteStatement&>( node ) );
				break;
			case ast::Node::Kind::EnumDeclaration:
				visitor.visit( static_cast<ast::nodes::EnumDeclaration&>( node ) );
				break;
			case ast::Node::Kind::ExpressionStatement:
				visitor.visit( static_cast<ast::nodes::ExpressionStatement&>( node ) );
				break;
			case ast::Node::Kind::ExternDeclaration:
				visitor.visit( static_cast<ast::nodes::ExternDeclaration&>( node ) );
				break;
			case ast::Node::Kind::FloatLiteral:
				visitor.visit( static_cast<ast::nodes::FloatLiteralExpression&>( node ) );
				break;
			case ast::Node::Kind::ForStatement:
				visitor.visit( static_cast<ast::nodes::ForStatement&>( node ) );
				break;
			case ast::Node::Kind::FunctionDeclaration:
				visitor.visit( static_cast<ast::nodes::FunctionDeclaration&>( node ) );
				break;
			case ast::Node::Kind::IdentifierExpression:
				visitor.visit( static_cast<ast::nodes::IdentifierExpression&>( node ) );
				break;
			case ast::Node::Kind::IfStatement:
				visitor.visit( static_cast<ast::nodes::IfStatement&>( node ) );
				break;
			case ast::Node::Kind::ImplementationDeclaration:
				visitor.visit( static_cast<ast::nodes::ImplementDeclaration&>( node ) );
				break;
			case ast::Node::Kind::ImportDeclaration:
				visitor.visit( static_cast<ast::nodes::ImportDeclaration&>( node ) );
				break;
			case ast::Node::Kind::IndexExpression:
				visitor.visit( static_cast<ast::nodes::IndexExpression&>( node ) );
				break;
			case ast::Node::Kind::IntegerLiteral:
				visitor.visit( static_cast<ast::nodes::IntegerLiteralExpression&>( node ) );
				break;
			case ast::Node::Kind::InterfaceDeclaration:
				visitor.visit( static_cast<ast::nodes::InterfaceDeclaration&>( node ) );
				break;
			case ast::Node::Kind::LambdaExpression:
				visitor.visit( static_cast<ast::nodes::LambdaExpression&>( node ) );
				break;
			case ast::Node::Kind::MatchStatement:
				visitor.visit( static_cast<ast::nodes::MatchStatement&>( node ) );
				break;
			case ast::Node::Kind::MemberAccessExpression:
				visitor.visit( static_cast<ast::nodes::MemberAccessExpression&>( node ) );
				break;
			case ast::Node::Kind::MethodCallExpression:
				visitor.visit( static_cast<ast::nodes::MethodCallExpression&>( node ) );
				break;
			case ast::Node::Kind::ModuleDeclaration:
				visitor.visit( static_cast<ast::nodes::ModuleDeclaration&>( node ) );
				break;
			case ast::Node::Kind::NoneLiteral:
				visitor.visit( static_cast<ast::nodes::NoneLiteralExpression&>( node ) );
				break;
			case ast::Node::Kind::PassStatement:
				visitor.visit( static_cast<ast::nodes::PassStatement&>( node ) );
				break;
			case ast::Node::Kind::Program:
				visitor.visit( static_cast<ast::nodes::Program&>( node ) );
				break;
			case ast::Node::Kind::RangeExpression:
				visitor.visit( static_cast<ast::nodes::RangeExpression&>( node ) );
				break;
			case ast::Node::Kind::ReturnStatement:
				visitor.visit( static_cast<ast::nodes::ReturnStatement&>( node ) );
				break;
			case ast::Node::Kind::SelfExpression:
				visitor.visit( static_cast<ast::nodes::SelfExpression&>( node ) );
				break;
			case ast::Node::Kind::StringLiteral:
				visitor.visit( static_cast<ast::nodes::StringLiteralExpression&>( node ) );
				break;
			case ast::Node::Kind::StructDeclaration:
				visitor.visit( static_cast<ast::nodes::StructDeclaration&>( node ) );
				break;
			case ast::Node::Kind::ParentExpression:
				visitor.visit( static_cast<ast::nodes::ParentExpression&>( node ) );
				break;
			case ast::Node::Kind::ThrowStatement:
				visitor.visit( static_cast<ast::nodes::ThrowStatement&>( node ) );
				break;
			case ast::Node::Kind::TryCatchStatement:
				visitor.visit( static_cast<ast::nodes::TryCatchStatement&>( node ) );
				break;
			case ast::Node::Kind::TupleExpression:
				visitor.visit( static_cast<ast::nodes::TupleExpression&>( node ) );
				break;
			case ast::Node::Kind::TypeAliasDeclaration:
				visitor.visit( static_cast<ast::nodes::TypeAliasDeclaration&>( node ) );
				break;
			case ast::Node::Kind::UnaryExpression:
				visitor.visit( static_cast<ast::nodes::UnaryExpression&>( node ) );
				break;
			case ast::Node::Kind::UnsafeBlock:
				visitor.visit( static_cast<ast::nodes::UnsafeBlockStatement&>( node ) );
				break;
			case ast::Node::Kind::VariableStatement:
				visitor.visit( static_cast<ast::nodes::VariableStatement&>( node ) );
				break;
			case ast::Node::Kind::WhileStatement:
				visitor.visit( static_cast<ast::nodes::WhileStatement&>( node ) );
				break;
			default:
				break;
		}
	}
	
} // namespace uranite::visitors

#endif // end _URANITE_VISITORS_VISITOR_HPP_
