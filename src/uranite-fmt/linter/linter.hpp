
//
// @author hxAri (hxari)
// @create 2026-06-15 10:30
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

#ifndef _URANITE_FMT_LINTER_LINTER_HPP_
#define _URANITE_FMT_LINTER_LINTER_HPP_

#include <string>
#include <unordered_set>
#include <vector>

#include "uranite/ast/node.hpp"
#include "uranite/visitors/visitor.hpp"
#include "uranite-fmt/comments/comment-extractor.hpp"
#include "uranite-fmt/linter/lint-rules.hpp"

namespace uranite::formatter::linter {
	
	/**
	 * @class Linter
	 * @brief Static analysis visitor engine enforcing coding standards and doccomment rules on Uranite ASTs.
	 *
	 * This engine traverses the Abstract Syntax Tree selective to structural scopes that impact 
	 * code metrics, documentation compliance, naming safety, and maximum structural block nesting depths.
	 * It cross-references token coordinates against the extracted comments map to track policy breaches.
	 */
	class Linter : public visitors::ASTVisitor {
			
		public:
			
			/**
			 * @brief Constructs a Linter bound to a target source track file context.
			 * @param sourceFilePath Path to the physical file under review, used to fill diagnostic trackers.
			 * @param extractedComments Vector containing pre-lexed standalone comment blocks.
			 * @param ruleConfig Configurations mapping severity guidelines and scalar ceilings.
			 */
			Linter( const std::string& sourceFilePath, const std::vector<comments::CommentEntry>& extractedComments, const LintRuleConfig& ruleConfig );
			
			/**
			 * @brief Master driver driving the static evaluation pipeline on a top-level program node block.
			 * * Clears prior tracking arrays, dispatches explicit iteration passes through the root nodes, 
			 * and gathers all emitted diagnostic markers.
			 * * @param program Reference to the root program AST node to analyze.
			 * @return std::vector<LintDiagnostic> Collection of all discovered rule validations or code odors.
			 */
			std::vector<LintDiagnostic> lint( ast::nodes::Program& program );
			
			void visit( ast::nodes::ArrayExpression& node ) override {}
			void visit( ast::nodes::AssignStatement& node ) override {}
			void visit( ast::nodes::BinaryExpression& node ) override {}
			void visit( ast::nodes::BlockStatement& node ) override {}
			void visit( ast::nodes::BoolLiteralExpression& node ) override {}
			void visit( ast::nodes::BreakStatement& node ) override {}
			void visit( ast::nodes::CallExpression& node ) override {}
			void visit( ast::nodes::CastExpression& node ) override {}
			void visit( ast::nodes::CharLiteralExpression& node ) override {}
			void visit( ast::nodes::ConstructExpression& node ) override {}
			void visit( ast::nodes::ContinueStatement& node ) override {}
			void visit( ast::nodes::DeferStatement& node ) override {}
			void visit( ast::nodes::DeleteStatement& node ) override {}
			void visit( ast::nodes::ExpressionStatement& node ) override {}
			void visit( ast::nodes::ExternDeclaration& node ) override {}
			void visit( ast::nodes::FloatLiteralExpression& node ) override {}
			void visit( ast::nodes::IdentifierExpression& node ) override {}
			void visit( ast::nodes::ImplementDeclaration& node ) override {}
			void visit( ast::nodes::IndexExpression& node ) override {}
			void visit( ast::nodes::IntegerLiteralExpression& node ) override {}
			void visit( ast::nodes::LambdaExpression& node ) override {}
			void visit( ast::nodes::MemberAccessExpression& node ) override {}
			void visit( ast::nodes::MethodCallExpression& node ) override {}
			void visit( ast::nodes::ModuleDeclaration& node ) override {}
			void visit( ast::nodes::NoneLiteralExpression& node ) override {}
			void visit( ast::nodes::PassStatement& node ) override {}
			void visit( ast::nodes::Program& node ) override {}
			void visit( ast::nodes::RangeExpression& node ) override {}
			void visit( ast::nodes::ReturnStatement& node ) override {}
			void visit( ast::nodes::SelfExpression& node ) override {}
			void visit( ast::nodes::StringLiteralExpression& node ) override {}
			void visit( ast::nodes::SuperExpression& node ) override {}
			void visit( ast::nodes::ThrowStatement& node ) override {}
			void visit( ast::nodes::TupleExpression& node ) override {}
			void visit( ast::nodes::TypeAliasDeclaration& node ) override {}
			void visit( ast::nodes::UnaryExpression& node ) override {}
			void visit( ast::nodes::UnsafeBlockStatement& node ) override {}
			
			/** @brief Validates class encapsulation doccomments and checks for deeply nested declaration layouts. */
			void visit( ast::nodes::ClassDeclaration& node ) override;
			
			/** @brief Validates enum type constraints, naming safety, and attached doccomments. */
			void visit( ast::nodes::EnumDeclaration& node ) override;
			
			/** @brief Inspects iterator variable naming identifiers and increments loop nesting tracking layers. */
			void visit( ast::nodes::ForStatement& node ) override;
			
			/** @brief Inspects functional bodies against maximum line count constraints, type annotations, parameter identifiers, and checks for API doccomments. */
			void visit( ast::nodes::FunctionDeclaration& node ) override;
			
			/** @brief Evaluates conditional expression statement blocks to ensure code nesting depth thresholds are not breached. */
			void visit( ast::nodes::IfStatement& node ) override;
			
			/** @brief Enforces package import styling laws, checking that imports are split down to a one-per-line pattern. */
			void visit( ast::nodes::ImportDeclaration& node ) override;
			
			/** @brief Validates interface contracts, traits, and associated documentation requirements. */
			void visit( ast::nodes::InterfaceDeclaration& node ) override;
			
			/** @brief Gauges execution path patterns inside match-case structures to assert depth constraints. */
			void visit( ast::nodes::MatchStatement& node ) override;
			
			/** @brief Validates structured system data definitions, member layout parameters, and mandatory structural comments. */
			void visit( ast::nodes::StructDeclaration& node ) override;
			
			/** @brief Evaluates exception safety boundaries to track nesting indentation layers. */
			void visit( ast::nodes::TryCatchStatement& node ) override;
			
			/** @brief Audits newly allocated variable names against character width boundaries. */
			void visit( ast::nodes::VariableStatement& node ) override;
			
			/** @brief Evaluates conditional loop scopes to compute nesting metrics. */
			void visit( ast::nodes::WhileStatement& node ) override;
			
		private:
				
			/**
			 * @brief Validates documentation blocks bound to public API elements.
			 * * Verifies existence and unpacks metadata markers to check structural `@param` tags 
			 * and `@complexity` performance attributes.
			 * * @param entityKind Display keyword tag naming the asset tier (e.g., "class", "function").
			 * @param entityName Explicit raw identifier name of the entity being audited.
			 * @param declarationLine Text line where the signature block initiates.
			 * @param declarationColumn Text column character offset location.
			 * @param hasBody Indicates whether the target declaration contains an execution block body.
			 */
			void checkDoccomment( const std::string& entityKind, const std::string& entityName, uint32_t declarationLine, uint32_t declarationColumn, bool hasBody );
			
			/**
			 * @brief Checks if a function's total line range overrides length parameters.
			 * @param functionName Name identifier of the target function block.
			 * @param startLine Initial line location coordinate.
			 * @param endLine Ending boundary line coordinate.
			 */
			void checkFunctionLength( const std::string& functionName, uint32_t startLine, uint32_t endLine );
			
			/**
			 * @brief Audits functional parameter naming strings to catch cryptic identifiers.
			 * @param parameterName Target argument parameter name.
			 * @param lineNumber Line location coordinates.
			 * @param columnNumber Column position coordinates.
			 */
			void checkParameterName( const std::string& parameterName, uint32_t lineNumber, uint32_t columnNumber );
			
			/**
			 * @brief Audits local variable identifiers against cryptic length boundaries.
			 * * Skips validation checks if the variable name matches a token registered within the `allowedShortNames_` exception set.
			 * * @param variableName Target variable identifier name string.
			 * @param lineNumber Line location coordinates.
			 * @param columnNumber Column position coordinates.
			 */
			void checkVariableName( const std::string& variableName, uint32_t lineNumber, uint32_t columnNumber );
			
			/**
			 * @brief Utility factory method compiling a structural LintDiagnostic entry and appending it to the collection.
			 * @param severity Breaking status tier of the rule violation.
			 * @param ruleIdentifier Unique text label indexing the broken constraint rule.
			 * @param message Explanatory notes mapping the issue.
			 * @param lineNumber Coordinates charting the line violation.
			 * @param columnNumber Coordinates charting the column violation.
			 */
			void emitDiagnostic( LintSeverity severity, const std::string& ruleIdentifier, const std::string& message, uint32_t lineNumber, uint32_t columnNumber );
				
			/**
			 * @brief Fetches raw documentation text blocks attached to code line boundaries.
			 * @param declarationLine Starting line tracking the target declaration signature.
			 * @return std::string Extracted raw doccomment text payload block, or empty string if not found.
			 */
			std::string getDoccommentForLine( uint32_t declarationLine ) const;
			
			/**
			 * @brief Evaluates whether a target declaration line has a valid leading doccomment attached to it.
			 * @param declarationLine Starting line index of the target declaration signature.
			 * @return true If a comment sequence matches layout parameters immediately preceding this line.
			 * @return false Otherwise.
			 */
			bool hasDoccommentForLine( uint32_t declarationLine ) const;
			
			/**
			 * @brief Recursive walk helper that measures control block scoping depths.
			 * * Inspects conditional blocks inside the body vector, incrementing depth trackers 
			 * and triggering violations if the nesting counts override `maximumNestingDepth`.
			 * * @param body Vector array storing the sequential statements inside a block layout scope.
			 * @param currentDepth Active relative scoping nesting measurement counter.
			 */
			void walkBodyForNesting( const std::vector<ast::nodes::StatementSharedPointer>& body, uint32_t currentDepth );
				
			/** @brief Whitelist cache mapping exceptions for short identifiers (e.g., standard loop indexes like `i`, `j`, `k`, or data streams `id`). */
			std::unordered_set<std::string> allowedShortNames_;
			
			/** @brief Global collection storing all recorded rule diagnostics during an operational run. */
			std::vector<LintDiagnostic> diagnostics_;
			
			/** @brief Internal copy tracking all parsed comments captured out of the input file text stream. */
			std::vector<comments::CommentEntry> extractedComments_;
			
			/** @brief System configurator storing limits, ceilings, thresholds, and severity guidelines. */
			LintRuleConfig ruleConfig_;
			
			/** @brief Target source file address workspace token. */
			std::string sourceFilePath_;
		
	};

} // namespace uranite::formatter::linter

#endif // _URANITE_FMT_LINTER_LINTER_HPP_
