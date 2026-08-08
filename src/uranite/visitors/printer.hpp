
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

#ifndef _URANITE_VISITORS_PRINTER_HPP_
#define _URANITE_VISITORS_PRINTER_HPP_

#include <string>
#include <sstream>

#include "uranite/visitors/visitor.hpp"

namespace uranite::visitors {
	
	/**
	 * @brief A visitor implementation used to generate a string representation of the Abstract Syntax Tree (AST).
	 * 
	 * This class traverses the AST nodes and formats them into a human-readable string with proper indentation.
	 */
	class ASTPrinter : public ASTVisitor {
		
		public:
			
			/**
			 * @brief Default constructor for ASTPrinter.
			 */
			ASTPrinter() = default;
			
			/**
			 * @brief Entry point for printing the entire AST starting from the program root.
			 * @param program The root node of the AST program.
			 * @return A formatted string representing the source code structure.
			 */
			std::string print( ast::nodes::Program& program );
			
			void visit( ast::nodes::ArrayExpression& node ) override;
			void visit( ast::nodes::AssignStatement& node ) override;
			void visit( ast::nodes::BinaryExpression& node ) override;
			void visit( ast::nodes::BlockStatement& node ) override;
			void visit( ast::nodes::BoolLiteralExpression& node ) override;
			void visit( ast::nodes::BreakStatement& node ) override;
			void visit( ast::nodes::CallExpression& node ) override;
			void visit( ast::nodes::CastExpression& node ) override;
			void visit( ast::nodes::CharLiteralExpression& node ) override;
			void visit( ast::nodes::ClassDeclaration& node ) override;
			void visit( ast::nodes::ConstructExpression& node ) override;
			void visit( ast::nodes::ContinueStatement& node ) override;
			void visit( ast::nodes::DeferStatement& node ) override;
			void visit( ast::nodes::DeleteStatement& node ) override;
			void visit( ast::nodes::EnumDeclaration& node ) override;
			void visit( ast::nodes::ExpressionStatement& node ) override;
			void visit( ast::nodes::ExternDeclaration& node ) override;
			void visit( ast::nodes::FloatLiteralExpression& node ) override;
			void visit( ast::nodes::ForStatement& node ) override;
			void visit( ast::nodes::FunctionDeclaration& node ) override;
			void visit( ast::nodes::IdentifierExpression& node ) override;
			void visit( ast::nodes::IfStatement& node ) override;
			void visit( ast::nodes::ImplementDeclaration& node ) override;
			void visit( ast::nodes::ImportDeclaration& node ) override;
			void visit( ast::nodes::IndexExpression& node ) override;
			void visit( ast::nodes::IntegerLiteralExpression& node ) override;
			void visit( ast::nodes::InterfaceDeclaration& node ) override;
			void visit( ast::nodes::LambdaExpression& node ) override;
			void visit( ast::nodes::MatchStatement& node ) override;
			void visit( ast::nodes::MemberAccessExpression& node ) override;
			void visit( ast::nodes::MethodCallExpression& node ) override;
			void visit( ast::nodes::ModuleDeclaration& node ) override;
			void visit( ast::nodes::NoneLiteralExpression& node ) override;
			void visit( ast::nodes::PassStatement& node ) override;
			void visit( ast::nodes::Program& node ) override;
			void visit( ast::nodes::RangeExpression& node ) override;
			void visit( ast::nodes::ReturnStatement& node ) override;
			void visit( ast::nodes::SelfExpression& node ) override;
			void visit( ast::nodes::StringLiteralExpression& node ) override;
			void visit( ast::nodes::StructDeclaration& node ) override;
			void visit( ast::nodes::ParentExpression& node ) override;
			void visit( ast::nodes::ThrowStatement& node ) override;
			void visit( ast::nodes::TryCatchStatement& node ) override;
			void visit( ast::nodes::TupleExpression& node ) override;
			void visit( ast::nodes::TypeAliasDeclaration& node ) override;
			void visit( ast::nodes::UnaryExpression& node ) override;
			void visit( ast::nodes::UnsafeBlockStatement& node ) override;
			void visit( ast::nodes::VariableStatement& node ) override;
			void visit( ast::nodes::WhileStatement& node ) override;
			
		private:
			
			/**
			 * @brief Decrements the current indentation level.
			 */
			void dedent();
			
			/**
			 * @brief Increments the current indentation level.
			 */
			void indent();
			
			/**
			 * @brief Formats and writes a type node to the output stream.
			 * @param type Shared pointer to the type node.
			 */
			void printType( const ast::nodes::TypeNodeSharedPointer& type );
			
			/**
			 * @brief Dispatches the visitor to a specific declaration node.
			 * @param declaration Shared pointer to the declaration node to visit.
			 */
			void visitDeclaration( const ast::nodes::DeclarationSharedPointer& declaration );
			
			/**
			 * @brief Dispatches the visitor to a specific expression node.
			 * @param expression Shared pointer to the expression node to visit.
			 */
			void visitExpression( const ast::nodes::ExpressionSharedPointer& expression );
			
			/**
			 * @brief Dispatches the visitor to a specific statement node.
			 * @param statement Shared pointer to the statement node to visit.
			 */
			void visitStatement( const ast::nodes::StatementSharedPointer& statement );
			
			/**
			 * @brief Appends text to the current output stream without a newline.
			 * @param text The string to be written.
			 */
			void write( const std::string& text );
			
			/**
			 * @brief Writes a line of text to the output stream with current indentation.
			 * @param text The string to be written as a full line.
			 */
			void writeLine( const std::string& text );
			
			/** @brief The indentation level currently applied to the output. */
			int indentLevel = 0;
			
			/** @brief The underlying string stream where the formatted output is accumulated. */
			std::ostringstream output;
		
	};
	
} // namespace uranite::visitors

#endif // end _URANITE_VISITORS_PRINTER_HPP_
