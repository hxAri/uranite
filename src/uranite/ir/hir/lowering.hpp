
//
// @author hxAri (hxari)
// @create 13-06-2026
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

#ifndef _URANITE_IR_HIR_LOWERING_HPP_
#define _URANITE_IR_HIR_LOWERING_HPP_

#include <memory>

#include "uranite/ast/node.hpp"
#include "uranite/diagnostic/diagnostic.hpp"
#include "uranite/ir/hir.hpp"
#include "uranite/semantic/analyzer.hpp"

namespace uranite::ir::hir {
	
	/** @brief Lowers a type-checked AST program into HIR, desugaring syntax constructs. */
	class HIRLowering {
	public:
		
		HIRLowering( semantic::Analyzer& semanticAnalyzer, diagnostic::Engine& diagnosticEngine );
		
		/** @brief Lowers the entire program AST into an HIR module. */
		std::shared_ptr<HIRModule> lower( ast::nodes::Program& programRoot );
	
	private:
		
		// Declaration lowering
		std::shared_ptr<HIRFunctionDefinition> lowerFunctionDeclaration( ast::nodes::FunctionDeclaration& declaration );
		std::shared_ptr<HIRClassDefinition> lowerClassDeclaration( ast::nodes::ClassDeclaration& declaration );
		std::shared_ptr<HIRStructDefinition> lowerStructDefinition( ast::nodes::StructDeclaration& declaration );
		std::shared_ptr<HIREnumDefinition> lowerEnumDeclaration( ast::nodes::EnumDeclaration& declaration );
		std::shared_ptr<HIRInterfaceDefinition> lowerInterfaceDeclaration( ast::nodes::InterfaceDeclaration& declaration );
		std::shared_ptr<HIRExternFunctionDeclaration> lowerExternDeclaration( ast::nodes::ExternDeclaration& declaration );
		std::shared_ptr<HIRGlobalVariableDefinition> lowerConstantDeclaration( ast::nodes::ConstantDeclaration& declaration );
		
		// Statement lowering
		HIRNodeSharedPointer lowerStatement( const ast::nodes::StatementSharedPointer& statement );
		std::shared_ptr<HIRBlock> lowerStatementBlock( const std::vector<ast::nodes::StatementSharedPointer>& statements, const lookup::SourceSharedPointer& sourceLocation );
		
		// Expression lowering
		HIRNodeSharedPointer lowerExpression( const ast::nodes::ExpressionSharedPointer& expression );
		
		// Desugaring helpers
		std::shared_ptr<HIRIf> desugarIfStatement( ast::nodes::IfStatement& ifStatement );
		std::shared_ptr<HIRLoop> desugarForStatement( ast::nodes::ForStatement& forStatement );
		std::shared_ptr<HIRLoop> desugarWhileStatement( ast::nodes::WhileStatement& whileStatement );
		
		// Parameter/field extraction
		HIRParameterDescriptor lowerParameter( ast::nodes::FunctionParameterNode& parameter );
		HIRFieldDescriptor lowerFieldDeclaration( ast::nodes::FieldDeclarationNode& field, int fieldIndex );
		HIRGenericParameterDescriptor lowerGenericParameter( ast::nodes::GenericParameterNode& genericParameter );
		
		/** @brief Resolves a TypeNode to its semantic Type using the Analyzer. */
		semantic::TypeSharedPointer resolveTypeNode( const ast::nodes::TypeNodeSharedPointer& typeNode );
		
		semantic::TypeSharedPointer inferExpressionType( const ast::nodes::ExpressionSharedPointer& expression );
		
		semantic::Analyzer& semanticAnalyzer;
		diagnostic::Engine& diagnosticEngine;
		std::string currentOwnerClassName;
		
	};

} // namespace uranite::ir::hir

#endif // end _URANITE_IR_HIR_LOWERING_HPP_
