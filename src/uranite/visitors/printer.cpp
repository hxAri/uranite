
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

#include "uranite/visitors/printer.hpp"

namespace uranite::visitors {
	
	void ASTPrinter::dedent() {
		this->indentLevel--;
	}
	
	void ASTPrinter::indent() {
		this->indentLevel++;
	}
	
	std::string ASTPrinter::print( ast::nodes::Program& program ) {
		this->output.str( "" );
		this->visit( program );
		return this->output.str();
	}
	
	void ASTPrinter::printType( const ast::nodes::TypeNodeSharedPointer& type ) {
		if( type == nullptr ) {
			this->write( "?" );
			return;
		}
		switch( type->kind ) {
			case ast::Node::Kind::SimpleType:
				this->write( static_cast<ast::nodes::SimpleTypeNode&>( *type ).name );
				break;
			case ast::Node::Kind::GenericType: {
				ast::nodes::GenericTypeNode& genericTypeNode = static_cast<ast::nodes::GenericTypeNode&>( *type );
				this->write( fmt::format( "<{}", genericTypeNode.name ) );
				for( size_t i=0; i<genericTypeNode.typeArguments.size(); i++ ) {
					if( i > 0 ) {
						this->write( ", " );
					}
					this->printType( genericTypeNode.typeArguments[i] );
				}
				this->write( ">" );
				break;
			}
			case ast::Node::Kind::ReferenceType: {
				ast::nodes::ReferenceTypeNode& referenceTypeNode = static_cast<ast::nodes::ReferenceTypeNode&>( *type );
				this->write( referenceTypeNode.isMutable ? "&mut " : "&" );
				this->printType( referenceTypeNode.innerType );
				break;
			}
			case ast::Node::Kind::PointerType: {
				ast::nodes::PointerTypeNode& pointerTypeNode = static_cast<ast::nodes::PointerTypeNode&>( *type );
				this->write( pointerTypeNode.isMutable ? "*mut " : "*" );
				this->printType( pointerTypeNode.innerType );
				break;
			}
			case ast::Node::Kind::ArrayType: {
				ast::nodes::ArrayTypeNode& arrayTypeNode = static_cast<ast::nodes::ArrayTypeNode&>( *type );
				this->write( "[" );
				this->printType( arrayTypeNode.elementType );
				if( arrayTypeNode.size ) {
					this->write( "; " );
					this->visitExpression( arrayTypeNode.size );
				}
				this->write( "]" );
				break;
			}
			case ast::Node::Kind::FunctionType: {
				ast::nodes::FunctionTypeNode& functionTypeNode = static_cast<ast::nodes::FunctionTypeNode&>( *type );
				this->write( "fn(" );
				for( size_t i=0; i<functionTypeNode.parameterTypes.size(); i++ ) {
					if( i > 0 ) {
						this->write( ", " );
					}
					this->printType( functionTypeNode.parameterTypes[i] );
				}
				this->write( ") -> " );
				this->printType( functionTypeNode.returnType );
				break;
			}
			case ast::Node::Kind::OptionalType: {
				ast::nodes::OptionalTypeNode& optionalTypeNode = static_cast<ast::nodes::OptionalTypeNode&>( *type );
				this->printType( optionalTypeNode.innerType );
				this->write( "?" );
				break;
			}
			default:
				this->write( "<unknown-type>" );
				break;
		}
	}
	
	void ASTPrinter::visit( ast::nodes::ArrayExpression& node ) {
		this->writeLine( fmt::format( "({}", node.toString() ) );
		this->indent();
		for( ast::nodes::ExpressionSharedPointer& element : node.elements ) {
			this->visitExpression( element );
		}
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::AssignStatement& node ) {
		this->writeLine( fmt::format( "({}", node.toString() ) );
		this->indent();
		this->visitExpression( node.target );
		this->visitExpression( node.value );
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::BinaryExpression& node ) {
		for( int i=0; i<this->indentLevel; i++ ) {
			this->output << "  ";
		}
		this->output << fmt::format( "({} {}\n", node.toString(), token::toString( node.operation ) );
		this->indent();
		this->visitExpression( node.left );
		this->visitExpression( node.right );
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::BlockStatement& node ) {
		this->writeLine( fmt::format( "({}", node.toString() ) );
		this->indent();
		for( ast::nodes::StatementSharedPointer& statement : node.statements ) {
			this->visitStatement( statement );
		}
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::BoolLiteralExpression& node ) {
		std::string nodeType( node.toString() );
		std::string nodeBooleanValue( "False" );
		if( node.value ) {
			nodeBooleanValue = "True";
		}
		this->writeLine( fmt::format( "({} {})", nodeType, nodeBooleanValue ) );
	}
	
	void ASTPrinter::visit( ast::nodes::BreakStatement& node ) {
		this->writeLine( fmt::format( "({})", node.toString() ) );
	}
	
	void ASTPrinter::visit( ast::nodes::CallExpression& node ) {
		this->writeLine( fmt::format( "({}", node.toString() ) );
		this->indent();
		this->visitExpression( node.callee );
		for( ast::nodes::ExpressionSharedPointer& argument : node.arguments ) {
			this->visitExpression( argument );
		}
		for( ast::nodes::KeywordArgument& kwarg : node.keywordArguments ) {
			this->writeLine( fmt::format( "(kwarg {}=", kwarg.name ) );
			this->indent();
			this->visitExpression( kwarg.value );
			this->dedent();
			this->writeLine( ")" );
		}
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::CastExpression& node ) {
		this->writeLine( fmt::format( "({}", node.toString() ) );
		this->indent();
		this->visitExpression( node.expression );
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::CharLiteralExpression& node ) {
		this->writeLine( fmt::format( "({} '{}')", node.toString(), std::string( 1, node.value ) ) );
	}
	
	void ASTPrinter::visit( ast::nodes::ClassDeclaration& node ) {
		this->writeLine( fmt::format( "({} {}", node.toString(), node.name ) );
		this->indent();
		for( ast::nodes::FieldDeclarationSharedPointer& field : node.fields ) {
			this->writeLine( fmt::format( "({} {})", field->toString(), field->name ) );
		}
		for( ast::nodes::DeclarationSharedPointer& method : node.methods ) {
			this->visitDeclaration( method );
		}
		for( ast::nodes::DeclarationSharedPointer& entity : node.nestedDeclarations) {
			this->visitDeclaration( entity );
		}
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::ConstructExpression& node ) {
		this->writeLine( fmt::format( "({}", node.toString() ) );
		this->indent();
		for( std::pair<std::string,ast::nodes::ExpressionSharedPointer> pair : node.fields ) {
			this->writeLine( fmt::format( "({} =", pair.first ) );
			this->indent();
			this->visitExpression( pair.second );
			this->dedent();
			this->writeLine( ")" );
		}
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::ContinueStatement& node ) {
		this->writeLine( fmt::format( "({})", node.toString() ) );
	}
	
	void ASTPrinter::visit( ast::nodes::DeferStatement& node ) {
		this->writeLine( fmt::format( "({}", node.toString() ) );
		this->indent();
		this->visitStatement( node.body );
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::DeleteStatement& node ) {
		this->writeLine( fmt::format( "({}", node.toString() ) );
		this->indent();
		this->visitExpression( node.expression );
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::EnumDeclaration& node ) {
		this->writeLine( fmt::format( "({} {}", node.toString(), node.name ) );
		this->indent();
		for( ast::nodes::EnumVariantSharedPointer& variant : node.variants ) {
			this->writeLine( fmt::format( "({} {})", variant->toString(), variant->name ) );
		}
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::ExpressionStatement& node ) {
		this->visitExpression( node.expression );
	}
	
	void ASTPrinter::visit( ast::nodes::ExternDeclaration& node ) {
		this->writeLine( fmt::format( "({} {})", node.toString(), node.name ) );
	}
	
	void ASTPrinter::visit( ast::nodes::FloatLiteralExpression& node ) {
		this->writeLine( fmt::format( "({} {})", node.toString(), node.value ) );
	}
	
	void ASTPrinter::visit( ast::nodes::ForStatement& node ) {
		if( node.isCStyle ) {
			this->writeLine( fmt::format( "({}CStyle {}", node.toString(), node.variable ) );
			this->indent();
			if( node.initializer ) {
				this->visitExpression( node.initializer );
			}
			if( node.condition ) {
				this->visitExpression( node.condition );
			}
			if( node.update ) {
				this->visitStatement( node.update );
			}
			for( ast::nodes::StatementSharedPointer& statement : node.body ) {
				this->visitStatement( statement );
			}
			this->dedent();
		}
		else {
			this->writeLine( fmt::format( "({} {}", node.toString(), node.variable ) );
			this->indent();
			if( node.iterable ) {
				this->visitExpression( node.iterable );
			}
			for( ast::nodes::StatementSharedPointer& statement : node.body ) {
				this->visitStatement( statement );
			}
			this->dedent();
		}
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::FunctionDeclaration& node ) {
		std::string signature( fmt::format( "({} {}(", node.toString(), node.name ) );
		for( size_t i=0; i < node.parameters.size(); i++ ) {
			if( i > 0 ) {
				signature+= ", ";
			}
			signature+= node.parameters[i]->name;
		}
		signature+= ")";
		if( node.returnType ) {
			signature+= " -> ";
			std::ostringstream tmp;
			std::swap( this->output, tmp );
			this->printType( node.returnType );
			signature+= this->output.str();
			std::swap( this->output, tmp );
		}
		this->writeLine( signature );
		this->indent();
		for( ast::nodes::DeclarationSharedPointer& entity : node.nestedFunctions ) {
			this->visitDeclaration( entity );
		}
		for( ast::nodes::StatementSharedPointer& statement : node.body ) {
			this->visitStatement( statement );
		}
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::IdentifierExpression& node ) {
		this->writeLine( fmt::format( "({} {})", node.toString(), node.name ) );
	}
	
	void ASTPrinter::visit( ast::nodes::IfStatement& node ) {
		this->writeLine( fmt::format( "({}", node.toString() ) );
		this->indent();
		this->visitExpression( node.condition );
		for( ast::nodes::StatementSharedPointer& statement : node.thenBody ) {
			this->visitStatement( statement );
		}
		this->dedent();
		for( std::pair<ast::nodes::ExpressionSharedPointer,std::vector<ast::nodes::StatementSharedPointer>> pair : node.elifBranches ) {
			this->writeLine( fmt::format( "({}Elif", node.toString() ) );
			this->indent();
			this->visitExpression( pair.first );
			for( ast::nodes::StatementSharedPointer& statement : pair.second ) {
				this->visitStatement( statement );
			}
			this->dedent();
			this->writeLine( ")" );
		}
		if( node.elseBody.empty() == false ) {
			this->writeLine( fmt::format( "({}Else", node.toString() ) );
			this->indent();
			for( ast::nodes::StatementSharedPointer& statement : node.elseBody ) {
				this->visitStatement( statement );
			}
			this->dedent();
			this->writeLine( ")" );
		}
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::ImplementDeclaration& node ) {
		this->writeLine( fmt::format( "({}", node.toString() ) );
		this->indent();
		for( ast::nodes::DeclarationSharedPointer& method : node.methods ) {
			this->visitDeclaration( method );
		}
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::ImportDeclaration& node ) {
		std::string path;
		for( size_t i=0; i<node.path.size(); i++ ) {
			if( i > 0 ) {
				path+= ".";
			}
			path+= node.path[i];
		}
		this->writeLine( fmt::format( "({} {})", node.toString(), path ) );
	}
	
	void ASTPrinter::visit( ast::nodes::IndexExpression& node ) {
		this->writeLine( fmt::format( "({}", node.toString() ) );
		this->indent();
		this->visitExpression( node.object );
		this->visitExpression( node.index );
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::IntegerLiteralExpression& node ) {
		this->writeLine( fmt::format( "({} {})", node.toString(), node.value ) );
	}
	
	void ASTPrinter::visit( ast::nodes::InterfaceDeclaration& node ) {
		this->writeLine( fmt::format( "({} {}", node.toString(), node.name ) );
		this->indent();
		for( ast::nodes::DeclarationSharedPointer& method : node.methods ) {
			this->visitDeclaration( method );
		}
		for( ast::nodes::DeclarationSharedPointer& entity : node.nestedDeclarations ) {
			this->visitDeclaration( entity );
		}
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::LambdaExpression& node ) {
		this->writeLine( fmt::format( "({}", node.toString() ) );
		this->indent();
		for( ast::nodes::StatementSharedPointer& statement : node.body ) {
			this->visitStatement( statement );
		}
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::MatchStatement& node ) {
	}
	
	void ASTPrinter::visit( ast::nodes::MemberAccessExpression& node ) {
		this->writeLine( fmt::format( "({} .", node.toString(), node.member ) );
		this->indent();
		this->visitExpression( node.object );
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::MethodCallExpression& node ) {
		this->writeLine( fmt::format( "({} .", node.toString(), node.method ) );
		this->indent();
		this->visitExpression( node.object );
		for( ast::nodes::ExpressionSharedPointer& argument : node.arguments ) {
			this->visitExpression( argument );
		}
		for( ast::nodes::KeywordArgument& kwarg : node.keywordArguments ) {
			this->writeLine( fmt::format( "(kwarg {}=", kwarg.name ) );
			this->indent();
			this->visitExpression( kwarg.value );
			this->dedent();
			this->writeLine( ")" );
		}
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::ModuleDeclaration& node ) {
		this->writeLine( fmt::format( "({} {})", node.toString(), node.name ) );
	}
	
	void ASTPrinter::visit( ast::nodes::NoneLiteralExpression& node ) {
		this->writeLine( fmt::format( "({})", node.toString() ) );
	}
	
	void ASTPrinter::visit( ast::nodes::PassStatement& node ) {
		this->writeLine( fmt::format( "({})", node.toString() ) );
	}
	
	void ASTPrinter::visit( ast::nodes::Program& node ) {
		this->writeLine( fmt::format( "({}", node.toString() ) );
		this->indent();
		if( node.module ) {
			this->visit( *node.module );
		}
		for( ast::nodes::ImportDeclarationSharedPointer& import : node.imports ) {
			this->visit( *import );
		}
		for( ast::nodes::DeclarationSharedPointer& declaration : node.declarations ) {
			this->visitDeclaration( declaration );
		}
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::RangeExpression& node ) {
		if( node.inclusive ) {
			this->writeLine( fmt::format( "({}Inclusive", node.toString() ) );
		}
		else {
			this->writeLine( fmt::format( "({}", node.toString() ) );
		}
		this->indent();
		this->visitExpression( node.start );
		this->visitExpression( node.end );
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::ReturnStatement& node ) {
		if( node.value ) {
			this->writeLine( fmt::format( "({}", node.toString() ) );
			this->indent();
			this->visitExpression( node.value );
			this->dedent();
			this->writeLine( ")" );
		}
		else {
			this->writeLine( fmt::format( "({})", node.toString() ) );
		}
	}
	
	void ASTPrinter::visit( ast::nodes::SelfExpression& node ) {
		this->writeLine( fmt::format( "({})", node.toString() ) );
	}
	
	void ASTPrinter::visit( ast::nodes::StringLiteralExpression& node ) {
		this->writeLine( fmt::format( "({} \"{}\")", node.toString(), node.value ) );
	}
	
	void ASTPrinter::visit( ast::nodes::StructDeclaration& node ) {
		this->writeLine( fmt::format( "({} {}", node.toString(), node.name ) );
		this->indent();
		for( ast::nodes::FieldDeclarationSharedPointer& field : node.fields ) {
			this->writeLine( fmt::format( "({} {})", field->toString(), field->name ) );
		}
		for( ast::nodes::DeclarationSharedPointer& method : node.methods ) {
			this->visitDeclaration( method );
		}
		for( ast::nodes::DeclarationSharedPointer& entity : node.nestedDeclarations ) {
			this->visitDeclaration( entity );
		}
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::SuperExpression& node ) {
		this->writeLine( fmt::format( "({})", node.toString() ) );
	}
	
	void ASTPrinter::visit( ast::nodes::ThrowStatement& node ) {
		this->writeLine( fmt::format( "({}", node.toString() ) );
		this->indent();
		this->visitExpression( node.expression );
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::TryCatchStatement& node ) {
		this->writeLine( fmt::format( "({}", node.toString() ) );
		this->indent();
		for( ast::nodes::StatementSharedPointer& statement : node.tryBody ) {
			this->visitStatement( statement );
		}
		this->dedent();
		for( ast::nodes::ExceptionClause& clause : node.exceptionClauses ) {
			std::string exception( "(Except" );
			for( ast::nodes::TypeNodeSharedPointer& type : clause.exceptionTypes ) {
				if( type->kind == ast::Node::Kind::SimpleType ) {
					exception+= " ";
					exception+= static_cast<ast::nodes::SimpleTypeNode&>( *type ).name;
				}
			}
			if( clause.variableName.empty() == false ) {
				exception+= " as ";
				exception+= clause.variableName;
			}
			this->writeLine( exception );
			this->indent();
			for( ast::nodes::StatementSharedPointer& statement : clause.body ) {
				this->visitStatement( statement );
			}
			this->dedent();
			this->writeLine( ")" );
		}
		if( node.finallyBody.empty() == false ) {
			this->writeLine( "(Finally" );
			this->indent();
			for( ast::nodes::StatementSharedPointer& statement : node.finallyBody ) {
				this->visitStatement( statement );
			}
			this->dedent();
			this->writeLine( ")" );
		}
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::TupleExpression& node ) {
		this->writeLine( fmt::format( "({}", node.toString() ) );
		this->indent();
		for( ast::nodes::ExpressionSharedPointer& element : node.elements ) {
			this->visitExpression( element );
		}
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::TypeAliasDeclaration& node ) {
		writeLine("(TypeAlias " + node.name + ")");
	}
	
	void ASTPrinter::visit( ast::nodes::UnaryExpression& node ) {
		for( int i=0; i<this->indentLevel; i++ ) {
			this->output << "  ";
		}
		this->output << fmt::format( "({} {}\n", node.toString(), token::toString( node.operation ) );
		this->indent();
		this->visitExpression( node.operand );
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::UnsafeBlockStatement& node ) {
		this->writeLine( fmt::format( "({}", node.toString() ) );
		this->indent();
		for( ast::nodes::StatementSharedPointer& statement : node.body ) {
			this->visitStatement( statement );
		}
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::VariableStatement& node ) {
		if( node.isMutable ) {
			this->writeLine( fmt::format( "({}Mutable {}", node.toString(), node.name ) );
		}
		else {
			this->writeLine( fmt::format( "({} {}", node.toString(), node.name ) );
		}
		if( node.initializer ) {
			this->indent();
			this->visitExpression( node.initializer );
			this->dedent();
		}
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visit( ast::nodes::WhileStatement& node ) {
		this->writeLine( fmt::format( "({}", node.toString() ) );
		this->indent();
		this->visitExpression( node.condition );
		for( ast::nodes::StatementSharedPointer& statement : node.body ) {
			this->visitStatement( statement );
		}
		this->dedent();
		this->writeLine( ")" );
	}
	
	void ASTPrinter::visitDeclaration( const ast::nodes::DeclarationSharedPointer& declaration ) {
		if( declaration != nullptr ) {
			dispatchVisit( *this, *declaration );
		}
	}
	
	void ASTPrinter::visitExpression( const ast::nodes::ExpressionSharedPointer& expression ) {
		if( expression != nullptr ) {
			dispatchVisit( *this, *expression );
		}
	}
	
	void ASTPrinter::visitStatement( const ast::nodes::StatementSharedPointer& statement ) {
		if( statement != nullptr ) {
			dispatchVisit( *this, *statement );
		}
	}
	
	void ASTPrinter::write( const std::string& text ) {
		this->output << text;
	}
	
	void ASTPrinter::writeLine( const std::string& text ) {
		for( int i=0; i<this->indentLevel; i++ ) {
			this->output << "  ";
		}
		this->output << text << "\n";
	}
	
}
