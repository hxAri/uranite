
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

#ifndef _URANITE_PARSER_PARSER_HPP_
#define _URANITE_PARSER_PARSER_HPP_

#include <memory>
#include <vector>

#include "uranite/ast/node.hpp"
#include "uranite/diagnostic/diagnostic.hpp"
#include "uranite/token/token.hpp"

namespace uranite::parser {
	
	class Parser {
		
		public:
			
			/**
			 * @brief Initialize parser instance with token stream and diagnostic engine.
			 * 
			 * @param tokens Token stream produced by lexical analysis.
			 * @param diagnostic Diagnostic engine used for reporting parser errors.
			 */
			Parser( std::vector<token::Token> tokens, diagnostic::Engine& diagnostic );
			
			/**
			 * @brief Parse entire source file into abstract syntax tree program node.
			 * 
			 * @return Parsed program node.
			 */
			ast::nodes::ProgramSharedPointer parse();
		
		private:
		
			// --- Token Navigation ---
			
			/**
			 * @brief Retrieve current token from parser cursor.
			 * 
			 * @return Current token reference.
			 */
			const token::Token& current() const;
			
			/**
			 * @brief Peek token at specified offset without advancing parser cursor.
			 * 
			 * @param offset Offset relative to current parser position.
			 * 
			 * @return Peeked token reference.
			 */
			const token::Token& peek( int offset = 1 ) const;
			
			/**
			 * @brief Advance parser cursor to next token.
			 * 
			 * @return Consumed token reference.
			 */
			const token::Token& advance();
			
			/**
			 * @brief Check whether current token matches specified token type.
			 * 
			 * @param type Expected token type.
			 * 
			 * @return True if token type matches.
			 */
			bool check( token::Type type ) const;
			bool check( token::Type type, bool optional ) const;
			
			bool check( const std::vector<token::Type>& types ) const;
			bool check( const std::vector<token::Type>& types, bool optional ) const;
			
			/**
			 * @brief Match and consume token if current token matches specified type.
			 * 
			 * @param type Expected token type.
			 * 
			 * @return True if token successfully matched and consumed.
			 */
			bool match( token::Type type );
			bool match( token::Type type, bool optional );
			bool looksLikeGenericCall();
			
			/**
			 * @brief Consume expected token or emit parser diagnostic.
			 * 
			 * @param type Expected token type.
			 * @param message Diagnostic message for parser failure.
			 * 
			 * @return Consumed token instance.
			 */
			token::Token expect(
				token::Type type,
				const std::string& message
			);
			
			/**
			 * @brief Skip all newline tokens from current parser position.
			 */
			void skipNewline();
			
			/**
			 * @brief Ensure current token sequence ends with newline token.
			 * 
			 * @param context Human readable parser context.
			 */
			void expectNewline( const std::string& context );
			
			/**
			 * @brief Determine whether parser reached end of token stream.
			 * 
			 * @return True if parser cursor reached end-of-file token.
			 */
			bool isAtEnd() const;
			bool isAtEnd( bool optional ) const;
			
			// --- Access Modifiers ---
			
			/**
			 * @brief Parse declaration access modifier.
			 * 
			 * @return Parsed access modifier value.
			 */
			ast::AccessModifier parseAccessModifier();
			
			// --- Top-Level Declarations ---
			
			/**
			 * @brief Parse generic top-level declaration.
			 * 
			 * @return Parsed declaration node.
			 */
			ast::nodes::DeclarationSharedPointer parseDeclaration();
			
			/**
			 * @brief Parse module declaration.
			 * 
			 * @return Parsed module declaration node.
			 */
			ast::nodes::ModuleDeclarationSharedPointer parseModuleDeclaration();
			
			/**
			 * @brief Parse import declaration.
			 * 
			 * @return Parsed import declaration node.
			 */
			ast::nodes::ImportDeclarationSharedPointer parseImportDeclaration();
			
			/**
			 * @brief Parse export declaration.
			 * 
			 * @return Parsed export declaration node.
			 */
			ast::nodes::ExportDeclarationSharedPointer parseExportDeclaration();
			
			/**
			 * @brief Parse function declaration.
			 * 
			 * @param access Access modifier applied to declaration.
			 * @param isVirtualFunction Indicates whether declaration is virtual.
			 * @param isOverrideFunction Indicates whether declaration overrides parent implementation.
			 * @param isAbstractFunction Indicates whether declaration is abstract.
			 * @param isStaticFunction Indicates whether declaration is static.
			 * 
			 * @return Parsed function declaration node.
			 */
			ast::nodes::FunctionDeclarationSharedPointer parseFunctionDeclaration(
				ast::AccessModifier access,
				bool isVirtualFunction=false,
				bool isOverrideFunction=false,
				bool isAbstractFunction=false,
				bool isStaticFunction=false
			);
			
			/**
			 * @brief Parse class declaration.
			 * 
			 * @param access Access modifier applied to declaration.
			 * 
			 * @return Parsed class declaration node.
			 */
			ast::nodes::ClassDeclarationSharedPointer parseClassDeclaration( ast::AccessModifier access );
			
			/**
			 * @brief Parse struct declaration.
			 * 
			 * @param access Access modifier applied to declaration.
			 * 
			 * @return Parsed  struct declaration node.
			 */
			ast::nodes::StructDeclarationSharedPointer parseStructDeclaration( ast::AccessModifier access );
			
			/**
			 * @brief Parse enumeration declaration.
			 * 
			 * @param access Access modifier applied to declaration.
			 * 
			 * @return Parsed enum declaration node.
			 */
			ast::nodes::EnumDeclarationSharedPointer parseEnumDeclaration( ast::AccessModifier access );
			
			/**
			 * @brief Parse interface declaration.
			 * 
			 * @param access Access modifier applied to declaration.
			 * 
			 * @return Parsed interface declaration node.
			 */
			ast::nodes::InterfaceDeclarationSharedPointer parseInterfaceDeclaration( ast::AccessModifier access );
			
			/**
			 * @brief Parse trait declaration.
			 * 
			 * @param access Access modifier applied to declaration.
			 * 
			 * @return Parsed trait declaration node.
			 */
			ast::nodes::TraitDeclarationSharedPointer parseTraitDeclaration( ast::AccessModifier access );
			
			/**
			 * @brief Parse implementation declaration.
			 * 
			 * @return Parsed implementation declaration node.
			 */
			ast::nodes::ImplementDeclarationSharedPointer parseImplementDeclaration();
			
			/**
			 * @brief Parse type alias declaration.
			 * 
			 * @param access Access modifier applied to declaration.
			 * 
			 * @return Parsed type alias declaration node.
			 */
			ast::nodes::TypeAliasDeclarationSharedPointer parseTypeAliasDeclaration( ast::AccessModifier access );
			
			/**
			 * @brief Parse external declaration block.
			 * 
			 * @return Parsed extern declaration node.
			 */
			ast::nodes::ExternDeclarationSharedPointer parseExternDeclaration();
			
			// --- Class And Structure Internals ---
			
			/**
			 * @brief Parse field declaration.
			 * 
			 * @param access Access modifier applied to declaration.
			 * 
			 * @return Parsed field declaration node.
			 */
			ast::nodes::FieldDeclarationSharedPointer parseFieldDeclaration( ast::AccessModifier access );
			
			/**
			 * @brief Parse method declaration.
			 * 
			 * @param access Access modifier applied to declaration.
			 * 
			 * @return Parsed method declaration node.
			 */
			ast::nodes::FunctionDeclarationSharedPointer parseMethodDeclaration( ast::AccessModifier access );
			
			// --- Generic Parameters ---
			
			/**
			 * @brief Parse generic parameter list.
			 * 
			 * @return Parsed generic parameter collection.
			 */
			std::vector<ast::nodes::GenericParameterSharedPointer> parseGenericParameters();
			
			/**
			 * @brief Parse generic argument list.
			 * 
			 * @return Parsed generic argument collection.
			 */
			std::vector<ast::nodes::TypeNodeSharedPointer> parseGenericArguments();
			
			// --- Function Parameters ---
			
			/**
			 * @brief Parse function parameter list.
			 * 
			 * @return Parsed function parameter collection.
			 */
			std::vector<ast::nodes::FunctionParameterSharedPointer> parseFunctionParameters();
			
			/**
			 * @brief Parse single function parameter.
			 * 
			 * @return Parsed function parameter node.
			 */
			ast::nodes::FunctionParameterSharedPointer parseFunctionParameter();
			
			// --- Statements ---
			
			/**
			 * @brief Parse generic statement node.
			 * 
			 * @return Parsed statement node.
			 */
			ast::nodes::StatementSharedPointer parseStatement();
			
			/**
			 * @brief Parse return statement.
			 * 
			 * @return Parsed statement node.
			 */
			ast::nodes::StatementSharedPointer parseReturnStatement();
			
			/**
			 * @brief Parse conditional if statement.
			 * 
			 * @return Parsed statement node.
			 */
			ast::nodes::StatementSharedPointer parseIfStatement();
			
			/**
			 * @brief Parse match statement.
			 * 
			 * @return Parsed statement node.
			 */
			ast::nodes::StatementSharedPointer parseMatchStatement();
			
			/**
			 * @brief Parse switch statement.
			 * 
			 * @return Parsed statement node.
			 */
			ast::nodes::StatementSharedPointer parseSwitchStatement();
			
			/**
			 * @brief Parse for-loop statement.
			 * 
			 * @return Parsed statement node.
			 */
			ast::nodes::StatementSharedPointer parseForStatement();
			
			/**
			 * @brief Parse while-loop statement.
			 * 
			 * @return Parsed statement node.
			 */
			ast::nodes::StatementSharedPointer parseWhileStatement();
			
			/**
			 * @brief Parse unsafe execution block.
			 * 
			 * @return Parsed statement node.
			 */
			ast::nodes::StatementSharedPointer parseUnsafeBlockStatement();
			
			/**
			 * @brief Parse defer statement.
			 * 
			 * @return Parsed statement node.
			 */
			ast::nodes::StatementSharedPointer parseDeferStatement();
			
			/**
			 * @brief Parse try-catch statement.
			 * 
			 * @return Parsed statement node.
			 */
			ast::nodes::StatementSharedPointer parseTryCatchStatement();
			
			ast::nodes::StatementSharedPointer parseInlineAssemblyStatement();
			
			/**
			 * @brief Parse statement block.
			 * 
			 * @return Parsed statement collection.
			 */
			std::vector<ast::nodes::StatementSharedPointer> parseStatementBlock();
			
			// --- Expressions ---
			
			/**
			 * @brief Parse expression node.
			 * 
			 * @return Parsed expression node.
			 */
			ast::nodes::ExpressionSharedPointer parseExpression();
			
			/**
			 * @brief Parse precedence-based expression.
			 * 
			 * @param minimumPrecedence Minimum operator precedence threshold.
			 * 
			 * @return Parsed expression node.
			 */
			ast::nodes::ExpressionSharedPointer parsePrecedenceExpression(
				int minimumPrecedence
			);
			
			/**
			 * @brief Parse unary expression.
			 * 
			 * @return Parsed expression node.
			 */
			ast::nodes::ExpressionSharedPointer parseUnaryExpression();
			
			/**
			 * @brief Parse postfix expression.
			 * 
			 * @return Parsed expression node.
			 */
			ast::nodes::ExpressionSharedPointer parsePostfixExpression();
			
			/**
			 * @brief Parse primary expression.
			 * 
			 * @return Parsed expression node.
			 */
			ast::nodes::ExpressionSharedPointer parsePrimaryExpression();
			
			/**
			 * @brief Parse lambda expression.
			 * 
			 * @return Parsed expression node.
			 */
			ast::nodes::ExpressionSharedPointer parseLambdaExpression();
			
			/**
			 * @brief Parse match expression.
			 * 
			 * @return Parsed expression node.
			 */
			ast::nodes::ExpressionSharedPointer parseMatchExpression();
			
			/**
			 * @brief Parse array expression.
			 * 
			 * @return Parsed expression node.
			 */
			ast::nodes::ExpressionSharedPointer parseArrayExpression();
		ast::nodes::ExpressionSharedPointer parseMapOrSetExpression();
			
			// --- Type Parsing ---
			
			/**
			 * @brief Parse type node.
			 * 
			 * @return Parsed type node.
			 */
			ast::nodes::TypeNodeSharedPointer parseTypeNode();
			
			/**
			 * @brief Parse base type node.
			 * 
			 * @return Parsed type node.
			 */
			ast::nodes::TypeNodeSharedPointer parseBaseTypeNode();
			
			// --- Utilities ---
			
			/**
			 * @brief Check whether current token represents greater-than operator.
			 * 
			 * @return True if current token is greater-than operator.
			 */
			bool checkGreaterThan() const;
			
			/**
			 * @brief Consume greater-than token or emit diagnostic.
			 * 
			 * @param message Diagnostic message for parser failure.
			 * 
			 * @return Consumed token instance.
			 */
			token::Token consumeGreaterThan(
				const std::string& message
			);
			
			/**
			 * @brief Retrieve operator precedence value.
			 * 
			 * @param type Operator token type.
			 * 
			 * @return Operator precedence value.
			 */
			int getOperatorPrecedenceValue(
				token::Type type
			) const;
			
			/**
			 * @brief Determine whether token type is binary operator.
			 * 
			 * @param type Token type to evaluate.
			 * 
			 * @return True if token type represents binary operator.
			 */
			bool isBinaryOperator(
				token::Type type
			) const;
			
			/**
			 * @brief Determine whether current token can begin expression parsing.
			 * 
			 * @return True if token can begin expression parsing.
			 */
			bool isExpressionStart() const;
			
			/**
			 * @brief Determine whether current token can begin statement parsing.
			 * 
			 * @return True if token can begin statement parsing.
			 */
			bool isStatementStart() const;
			
			/// @brief Reference to the engine used for reporting compilation errors.
			diagnostic::Engine& diagnostic;
			
			/// @brief Counter for pending Greater Than tokens during template parsing.
			int pendingGreaterThans = 0;
			
			/// @brief Current index position within the token list.
			size_t position = 0;
			
			/// @brief The complete list of tokens to be processed.
			std::vector<token::Token> tokens;
			
	};
	
} // namespace uranite::parser

#endif // end _URANITE_PARSER_PARSER_HPP_
