
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

#include <fmt/format.h>

#include "uranite-fmt/formatter/formatter.hpp"

namespace uranite::formatter {
	
	Formatter::Formatter()
		: isInlineExpression_( false ) {
	}
	
	Formatter::Formatter( const FormattingRules& formattingRules )
		: isInlineExpression_( false ),
		  writer_( formattingRules ),
		  formattingRules_( formattingRules ) {
	}
	
	std::string Formatter::format( ast::nodes::Program& program ) {
		SourceWriter freshWriter( this->formattingRules_ );
		std::swap( this->writer_, freshWriter );
		this->isInlineExpression_ = false;
		this->visit( program );
		return this->writer_.finalize();
	}
	
	std::string Formatter::accessModifierToString( ast::AccessModifier accessModifier ) {
		switch( accessModifier ) {
			case ast::AccessModifier::Public: return "public";
			case ast::AccessModifier::Protect: return "protect";
			case ast::AccessModifier::Private: return "private";
			default: return "";
		}
	}
	
	std::string Formatter::operatorToString( token::Type operatorType ) {
		return std::string( token::toString( operatorType ) );
	}
	
	std::string Formatter::typeToString( const ast::nodes::TypeNodeSharedPointer& typeNode ) {
		if( typeNode == nullptr ) {
			return "";
		}
		switch( typeNode->kind ) {
			case ast::Node::Kind::SimpleType: {
				ast::nodes::SimpleTypeNode& simpleType = static_cast<ast::nodes::SimpleTypeNode&>( *typeNode );
				return simpleType.name;
			}
			case ast::Node::Kind::GenericType: {
				ast::nodes::GenericTypeNode& genericType = static_cast<ast::nodes::GenericTypeNode&>( *typeNode );
				std::string result = genericType.name + "<";
				for( size_t argumentIndex = 0; argumentIndex < genericType.typeArguments.size(); argumentIndex++ ) {
					if( argumentIndex > 0 ) {
						result+= ", ";
					}
					result+= this->typeToString( genericType.typeArguments[argumentIndex] );
				}
				result+= ">";
				return result;
			}
			case ast::Node::Kind::ReferenceType: {
				ast::nodes::ReferenceTypeNode& referenceType = static_cast<ast::nodes::ReferenceTypeNode&>( *typeNode );
				std::string prefix = referenceType.isMutable ? "&mut " : "&";
				return prefix + this->typeToString( referenceType.innerType );
			}
			case ast::Node::Kind::PointerType: {
				ast::nodes::PointerTypeNode& pointerType = static_cast<ast::nodes::PointerTypeNode&>( *typeNode );
				std::string prefix = pointerType.isMutable ? "*mut " : "*";
				return prefix + this->typeToString( pointerType.innerType );
			}
			case ast::Node::Kind::ArrayType: {
				ast::nodes::ArrayTypeNode& arrayType = static_cast<ast::nodes::ArrayTypeNode&>( *typeNode );
				std::string result = "[" + this->typeToString( arrayType.elementType );
				if( arrayType.size ) {
					result+= "; ";
					SourceWriter temporaryWriter( this->formattingRules_ );
					std::swap( this->writer_, temporaryWriter );
					this->isInlineExpression_ = true;
					this->emitExpression( arrayType.size );
					this->isInlineExpression_ = false;
					result+= this->writer_.finalize();
					std::swap( this->writer_, temporaryWriter );
				}
				result+= "]";
				return result;
			}
			case ast::Node::Kind::FunctionType: {
				ast::nodes::FunctionTypeNode& functionType = static_cast<ast::nodes::FunctionTypeNode&>( *typeNode );
				std::string result = "fn(";
				for( size_t parameterIndex = 0; parameterIndex < functionType.parameterTypes.size(); parameterIndex++ ) {
					if( parameterIndex > 0 ) {
						result+= ", ";
					}
					result+= this->typeToString( functionType.parameterTypes[parameterIndex] );
				}
				result+= ") -> ";
				result+= this->typeToString( functionType.returnType );
				return result;
			}
			case ast::Node::Kind::OptionalType: {
				ast::nodes::OptionalTypeNode& optionalType = static_cast<ast::nodes::OptionalTypeNode&>( *typeNode );
				return "?" + this->typeToString( optionalType.innerType );
			}
			case ast::Node::Kind::TupleType: {
				ast::nodes::TupleTypeNode& tupleType = static_cast<ast::nodes::TupleTypeNode&>( *typeNode );
				std::string result = "(";
				for( size_t elementIndex = 0; elementIndex < tupleType.elements.size(); elementIndex++ ) {
					if( elementIndex > 0 ) {
						result+= ", ";
					}
					result+= this->typeToString( tupleType.elements[elementIndex] );
				}
				result+= ")";
				return result;
			}
			case ast::Node::Kind::UnionType: {
				ast::nodes::UnionTypeNode& unionType = static_cast<ast::nodes::UnionTypeNode&>( *typeNode );
				std::string result;
				for( size_t typeIndex = 0; typeIndex < unionType.types.size(); typeIndex++ ) {
					if( typeIndex > 0 ) {
						result+= " | ";
					}
					result+= this->typeToString( unionType.types[typeIndex] );
				}
				return result;
			}
			default:
				return "<unknown-type>";
		}
	}
	
	void Formatter::emitType( const ast::nodes::TypeNodeSharedPointer& typeNode ) {
		this->writer_.writeRaw( this->typeToString( typeNode ) );
	}
	
	void Formatter::emitGenericParameters( const std::vector<ast::nodes::GenericParameterSharedPointer>& genericParameters ) {
		if( genericParameters.empty() ) {
			return;
		}
		this->writer_.writeRaw( "<" );
		for( size_t parameterIndex = 0; parameterIndex < genericParameters.size(); parameterIndex++ ) {
			if( parameterIndex > 0 ) {
				this->writer_.writeRaw( ", " );
			}
			this->writer_.writeRaw( genericParameters[parameterIndex]->name );
			if( genericParameters[parameterIndex]->constraints.empty() == false ) {
				this->writer_.writeRaw( ": " );
				for( size_t constraintIndex = 0; constraintIndex < genericParameters[parameterIndex]->constraints.size(); constraintIndex++ ) {
					if( constraintIndex > 0 ) {
						this->writer_.writeRaw( " + " );
					}
					this->emitType( genericParameters[parameterIndex]->constraints[constraintIndex] );
				}
			}
			if( genericParameters[parameterIndex]->defaultType != nullptr ) {
				this->writer_.writeRaw( " = " );
				this->emitType( genericParameters[parameterIndex]->defaultType );
			}
		}
		this->writer_.writeRaw( ">" );
	}
	
	void Formatter::emitFunctionParameters( const std::vector<ast::nodes::FunctionParameterSharedPointer>& parameters ) {
		bool isParameterListEmpty = parameters.empty();
		if( isParameterListEmpty ) {
			if( this->formattingRules_.useSpacesInsideEmptyParens ) {
				this->writer_.writeRaw( "( )" );
			}
			else {
				this->writer_.writeRaw( "()" );
			}
			return;
		}
		if( this->formattingRules_.useSpacesInsideParentheses ) {
			this->writer_.writeRaw( "( " );
		}
		else {
			this->writer_.writeRaw( "(" );
		}
		for( size_t parameterIndex = 0; parameterIndex < parameters.size(); parameterIndex++ ) {
			if( parameterIndex > 0 ) {
				this->writer_.writeRaw( ", " );
			}
			const ast::nodes::FunctionParameterNode& parameter = *parameters[parameterIndex];
			if( parameter.isSelf ) {
				this->writer_.writeRaw( "self" );
				continue;
			}
			if( parameter.isProperty ) {
				std::string accessPrefix = this->accessModifierToString( parameter.propertyAccess );
				if( accessPrefix.empty() == false ) {
					this->writer_.writeRaw( accessPrefix + " " );
				}
			}
			if( parameter.isMutable ) {
				this->writer_.writeRaw( "mut " );
			}
			if( parameter.type != nullptr ) {
				if( parameter.isVariadic && parameter.type->kind == ast::Node::Kind::ArrayType ) {
					ast::nodes::ArrayTypeNode& arrayType = static_cast<ast::nodes::ArrayTypeNode&>( *parameter.type );
					this->emitType( arrayType.elementType );
					this->writer_.writeRaw( " " + parameter.name + "[]" );
				}
				else if( parameter.isKeyword ) {
					this->emitType( parameter.type );
					this->writer_.writeRaw( " " + parameter.name + "{}" );
				}
				else {
					this->emitType( parameter.type );
					this->writer_.writeRaw( " " + parameter.name );
				}
			}
			else {
				this->writer_.writeRaw( parameter.name );
			}
			if( parameter.defaultValue != nullptr ) {
				this->writer_.writeRaw( "=" );
				this->isInlineExpression_ = true;
				this->emitExpression( parameter.defaultValue );
				this->isInlineExpression_ = false;
			}
		}
		if( this->formattingRules_.useSpacesInsideParentheses ) {
			this->writer_.writeRaw( " )" );
		}
		else {
			this->writer_.writeRaw( ")" );
		}
	}
	
	void Formatter::emitBody( const std::vector<ast::nodes::StatementSharedPointer>& bodyStatements ) {
		if( bodyStatements.empty() ) {
			return;
		}
		this->writer_.increaseIndent();
		for( const ast::nodes::StatementSharedPointer& statement : bodyStatements ) {
			this->visitStatement( statement );
		}
		this->writer_.decreaseIndent();
	}
	
	void Formatter::emitExpression( const ast::nodes::ExpressionSharedPointer& expression ) {
		if( expression != nullptr ) {
			bool savedInline = this->isInlineExpression_;
			this->isInlineExpression_ = true;
			this->visitExpression( expression );
			this->isInlineExpression_ = savedInline;
		}
	}
	
	void Formatter::emitInlineAssembly( ast::nodes::InlineAssemblyStatement& asmStatement ) {
		this->writer_.writeIndentation();
		this->writer_.writeRaw( "asm" );
		if( asmStatement.isVolatile ) {
			this->writer_.writeRaw( " volatile" );
		}
		this->writer_.writeRaw( " \"" + asmStatement.asmTemplate + "\"" );
		if( asmStatement.outputs.empty() == false ) {
			this->writer_.writeRaw( " : " );
			for( size_t outputIndex = 0; outputIndex < asmStatement.outputs.size(); outputIndex++ ) {
				if( outputIndex > 0 ) {
					this->writer_.writeRaw( ", " );
				}
				this->writer_.writeRaw( "output(" );
				if( asmStatement.outputs[outputIndex].constraint.empty() == false ) {
					this->writer_.writeRaw( "\"" + asmStatement.outputs[outputIndex].constraint + "\" " );
				}
				if( asmStatement.outputs[outputIndex].expression != nullptr ) {
					this->emitExpression( asmStatement.outputs[outputIndex].expression );
				}
				this->writer_.writeRaw( ")" );
			}
		}
		else if( asmStatement.inputs.empty() == false || asmStatement.clobbers.empty() == false ) {
			this->writer_.writeRaw( " : output()" );
		}
		if( asmStatement.inputs.empty() == false ) {
			this->writer_.writeRaw( " : " );
			for( size_t inputIndex = 0; inputIndex < asmStatement.inputs.size(); inputIndex++ ) {
				if( inputIndex > 0 ) {
					this->writer_.writeRaw( ", " );
				}
				this->writer_.writeRaw( "input( " );
				if( asmStatement.inputs[inputIndex].constraint.empty() == false ) {
					this->writer_.writeRaw( "\"" + asmStatement.inputs[inputIndex].constraint + "\" " );
				}
				if( asmStatement.inputs[inputIndex].expression != nullptr ) {
					this->emitExpression( asmStatement.inputs[inputIndex].expression );
				}
				this->writer_.writeRaw( " )" );
			}
		}
		else if( asmStatement.clobbers.empty() == false ) {
			this->writer_.writeRaw( " : input()" );
		}
		if( asmStatement.clobbers.empty() == false ) {
			this->writer_.writeRaw( " : clobber( " );
			for( size_t clobberIndex = 0; clobberIndex < asmStatement.clobbers.size(); clobberIndex++ ) {
				if( clobberIndex > 0 ) {
					this->writer_.writeRaw( ", " );
				}
				this->writer_.writeRaw( "\"" + asmStatement.clobbers[clobberIndex] + "\"" );
			}
			this->writer_.writeRaw( " )" );
		}
		this->writer_.writeNewline();
	}
	
	void Formatter::emitConstantDeclaration( ast::nodes::ConstantDeclaration& node ) {
		this->writer_.writeIndentation();
		std::string accessPrefix = this->accessModifierToString( node.access );
		if( accessPrefix.empty() == false ) {
			this->writer_.writeRaw( accessPrefix + " " );
		}
		if( node.isGlobalVariable == false ) {
			this->writer_.writeRaw( "const " );
		}
		if( node.type != nullptr ) {
			this->emitType( node.type );
			this->writer_.writeRaw( " " );
		}
		this->writer_.writeRaw( node.name );
		if( node.initializer != nullptr ) {
			this->writer_.writeRaw( " = " );
			this->isInlineExpression_ = true;
			this->emitExpression( node.initializer );
			this->isInlineExpression_ = false;
		}
		this->writer_.writeNewline();
	}
	
	void Formatter::emitExportDeclaration( ast::nodes::ExportDeclaration& node ) {
		if( node.declaration != nullptr ) {
			this->visitDeclaration( node.declaration );
			return;
		}
		this->writer_.writeIndentedLine( "export {" );
		this->writer_.increaseIndent();
		for( size_t itemIndex = 0; itemIndex < node.items.size(); itemIndex++ ) {
			const ast::nodes::ExportItem& exportItem = node.items[itemIndex];
			std::string itemLine = exportItem.name;
			if( exportItem.alias.empty() == false && exportItem.alias != exportItem.name ) {
				itemLine+= " as " + exportItem.alias;
			}
			if( itemIndex + 1 < node.items.size() ) {
				itemLine+= ",";
			}
			this->writer_.writeIndentedLine( itemLine );
		}
		this->writer_.decreaseIndent();
		this->writer_.writeIndentedLine( "}" );
	}
	
	void Formatter::visitDeclaration( const ast::nodes::DeclarationSharedPointer& declaration ) {
		if( declaration == nullptr ) {
			return;
		}
		if( declaration->kind == ast::Node::Kind::ConstantDeclaration ) {
			this->emitConstantDeclaration( static_cast<ast::nodes::ConstantDeclaration&>( *declaration ) );
			return;
		}
		if( declaration->kind == ast::Node::Kind::ExportDeclaration ) {
			this->emitExportDeclaration( static_cast<ast::nodes::ExportDeclaration&>( *declaration ) );
			return;
		}
		visitors::dispatchVisit( *this, *declaration );
	}
	
	void Formatter::visitExpression( const ast::nodes::ExpressionSharedPointer& expression ) {
		if( expression == nullptr ) {
			return;
		}
		if( expression->kind == ast::Node::Kind::AwaitExpression ) {
			ast::nodes::AwaitExpression& awaitExpr = static_cast<ast::nodes::AwaitExpression&>( *expression );
			this->writer_.writeRaw( "await " );
			this->emitExpression( awaitExpr.operand );
			return;
		}
		if( expression->kind == ast::Node::Kind::YieldExpression ) {
			ast::nodes::YieldExpression& yieldExpr = static_cast<ast::nodes::YieldExpression&>( *expression );
			this->writer_.writeRaw( "yield " );
			this->emitExpression( yieldExpr.value );
			return;
		}
		if( expression->kind == ast::Node::Kind::InstanceofExpression ) {
			ast::nodes::InstanceofExpression& instanceofExpr = static_cast<ast::nodes::InstanceofExpression&>( *expression );
			this->emitExpression( instanceofExpr.object );
			this->writer_.writeRaw( " instanceof " );
			this->writer_.writeRaw( this->typeToString( instanceofExpr.targetType ) );
			return;
		}
		if( expression->kind == ast::Node::Kind::SubclassofExpression ) {
			ast::nodes::SubclassofExpression& subclassofExpr = static_cast<ast::nodes::SubclassofExpression&>( *expression );
			this->writer_.writeRaw( this->typeToString( subclassofExpr.sourceType ) );
			this->writer_.writeRaw( " subclassof " );
			this->writer_.writeRaw( this->typeToString( subclassofExpr.targetType ) );
			return;
		}
		visitors::dispatchVisit( *this, *expression );
	}
	
	void Formatter::visitStatement( const ast::nodes::StatementSharedPointer& statement ) {
		if( statement == nullptr ) {
			return;
		}
		if( statement->kind == ast::Node::Kind::InlineAssemblyStatement ) {
			this->emitInlineAssembly( static_cast<ast::nodes::InlineAssemblyStatement&>( *statement ) );
			return;
		}
		visitors::dispatchVisit( *this, *statement );
	}
	
	void Formatter::visit( ast::nodes::Program& node ) {
		if( node.module ) {
			this->visit( *node.module );
		}
		if( node.imports.empty() == false ) {
			this->writer_.writeBlankLine();
			for( const ast::nodes::ImportDeclarationSharedPointer& importDecl : node.imports ) {
				this->visit( *importDecl );
			}
		}
		bool isFirstDeclaration = true;
		for( const ast::nodes::DeclarationSharedPointer& declaration : node.declarations ) {
			if( this->formattingRules_.blankLineBetweenTopLevelDeclarations ) {
				this->writer_.writeBlankLine();
			}
			else if( isFirstDeclaration ) {
				this->writer_.writeBlankLine();
			}
			isFirstDeclaration = false;
			this->visitDeclaration( declaration );
		}
		for( const ast::nodes::ExportDeclarationSharedPointer& exportDecl : node.exports ) {
			this->writer_.writeBlankLine();
			this->emitExportDeclaration( *exportDecl );
		}
	}
	
	void Formatter::visit( ast::nodes::ModuleDeclaration& node ) {
		this->writer_.writeIndentedLine( "package " + node.name );
	}
	
	void Formatter::visit( ast::nodes::ImportDeclaration& node ) {
		std::string modulePath;
		for( size_t segmentIndex = 0; segmentIndex < node.path.size(); segmentIndex++ ) {
			if( segmentIndex > 0 ) {
				modulePath+= ".";
			}
			modulePath+= node.path[segmentIndex];
		}
		if( node.isFromImport ) {
			this->writer_.writeIndentation();
			this->writer_.writeRaw( "from " + modulePath + " import " );
			if( node.importAll ) {
				this->writer_.writeRaw( "*" );
			}
			else if( node.importItems.size() == 1 ) {
				std::string itemText = node.importItems[0].name;
				if( node.importItems[0].alias.empty() == false && node.importItems[0].alias != node.importItems[0].name ) {
					itemText+= " as " + node.importItems[0].alias;
				}
				this->writer_.writeRaw( itemText );
			}
			else if( node.importItems.size() > 1 ) {
				bool shouldUseBraces = node.importItems.size() > 3;
				if( shouldUseBraces ) {
					this->writer_.writeRaw( "{\n" );
					this->writer_.increaseIndent();
					for( size_t itemIndex = 0; itemIndex < node.importItems.size(); itemIndex++ ) {
						this->writer_.writeIndentation();
						std::string itemText = node.importItems[itemIndex].name;
						if( node.importItems[itemIndex].alias.empty() == false && node.importItems[itemIndex].alias != node.importItems[itemIndex].name ) {
							itemText+= " as " + node.importItems[itemIndex].alias;
						}
						if( itemIndex + 1 < node.importItems.size() ) {
							itemText+= ",";
						}
						this->writer_.writeRaw( itemText + "\n" );
					}
					this->writer_.decreaseIndent();
					this->writer_.writeIndentation();
					this->writer_.writeRaw( "}" );
				}
				else {
					for( size_t itemIndex = 0; itemIndex < node.importItems.size(); itemIndex++ ) {
						if( itemIndex > 0 ) {
							this->writer_.writeRaw( ", " );
						}
						std::string itemText = node.importItems[itemIndex].name;
						if( node.importItems[itemIndex].alias.empty() == false && node.importItems[itemIndex].alias != node.importItems[itemIndex].name ) {
							itemText+= " as " + node.importItems[itemIndex].alias;
						}
						this->writer_.writeRaw( itemText );
					}
				}
			}
			this->writer_.writeNewline();
		}
		else {
			std::string importLine = "import " + modulePath;
			if( node.alias.empty() == false ) {
				importLine+= " as " + node.alias;
			}
			this->writer_.writeIndentedLine( importLine );
		}
	}
	
	void Formatter::visit( ast::nodes::ClassDeclaration& node ) {
		this->writer_.writeIndentation();
		std::string accessPrefix = this->accessModifierToString( node.access );
		if( accessPrefix.empty() == false ) {
			this->writer_.writeRaw( accessPrefix + " " );
		}
		if( node.isReadonly ) {
			this->writer_.writeRaw( "Readonly " );
		}
		if( node.isAbstract ) {
			this->writer_.writeRaw( "abstract " );
		}
		if( node.isFinal ) {
			this->writer_.writeRaw( "final " );
		}
		if( node.isNative ) {
			this->writer_.writeRaw( "native " );
		}
		this->writer_.writeRaw( "class " + node.name );
		this->emitGenericParameters( node.genericParameters );
		if( node.baseClassType != nullptr ) {
			this->writer_.writeRaw( " extends " );
			this->emitType( node.baseClassType );
		}
		if( node.interfaces.empty() == false ) {
			this->writer_.writeRaw( " implements " );
			for( size_t interfaceIndex = 0; interfaceIndex < node.interfaces.size(); interfaceIndex++ ) {
				if( interfaceIndex > 0 ) {
					this->writer_.writeRaw( ", " );
				}
				this->emitType( node.interfaces[interfaceIndex] );
			}
		}
		this->writer_.writeRaw( ":\n" );
		this->writer_.increaseIndent();
		for( const ast::nodes::FieldDeclarationSharedPointer& field : node.fields ) {
			this->writer_.writeIndentation();
			std::string fieldAccess = this->accessModifierToString( field->access );
			if( fieldAccess.empty() == false ) {
				this->writer_.writeRaw( fieldAccess + " " );
			}
			if( field->isStatic ) {
				this->writer_.writeRaw( "static " );
			}
			if( field->isReadonly ) {
				this->writer_.writeRaw( "readonly " );
			}
			if( field->type != nullptr ) {
				this->emitType( field->type );
				this->writer_.writeRaw( " " );
			}
			this->writer_.writeRaw( field->name );
			if( field->defaultValue != nullptr ) {
				this->writer_.writeRaw( " = " );
				this->isInlineExpression_ = true;
				this->emitExpression( field->defaultValue );
				this->isInlineExpression_ = false;
			}
			this->writer_.writeNewline();
		}
		for( const ast::nodes::DeclarationSharedPointer& method : node.methods ) {
			this->writer_.writeBlankLine();
			this->visitDeclaration( method );
		}
		for( const ast::nodes::DeclarationSharedPointer& nested : node.nestedDeclarations ) {
			this->writer_.writeBlankLine();
			this->visitDeclaration( nested );
		}
		this->writer_.decreaseIndent();
	}
	
	void Formatter::visit( ast::nodes::InterfaceDeclaration& node ) {
		this->writer_.writeIndentation();
		std::string accessPrefix = this->accessModifierToString( node.access );
		if( accessPrefix.empty() == false ) {
			this->writer_.writeRaw( accessPrefix + " " );
		}
		this->writer_.writeRaw( "interface " + node.name );
		this->emitGenericParameters( node.genericParameters );
		if( node.superInterfaces.empty() == false ) {
			this->writer_.writeRaw( " extends " );
			for( size_t interfaceIndex = 0; interfaceIndex < node.superInterfaces.size(); interfaceIndex++ ) {
				if( interfaceIndex > 0 ) {
					this->writer_.writeRaw( ", " );
				}
				this->emitType( node.superInterfaces[interfaceIndex] );
			}
		}
		this->writer_.writeRaw( ":\n" );
		this->writer_.increaseIndent();
		bool isFirstMethod = true;
		for( const ast::nodes::DeclarationSharedPointer& method : node.methods ) {
			if( isFirstMethod == false ) {
				this->writer_.writeBlankLine();
			}
			isFirstMethod = false;
			this->visitDeclaration( method );
		}
		for( const ast::nodes::DeclarationSharedPointer& nested : node.nestedDeclarations ) {
			this->writer_.writeBlankLine();
			this->visitDeclaration( nested );
		}
		this->writer_.decreaseIndent();
	}
	
	void Formatter::visit( ast::nodes::StructDeclaration& node ) {
		this->writer_.writeIndentation();
		std::string accessPrefix = this->accessModifierToString( node.access );
		if( accessPrefix.empty() == false ) {
			this->writer_.writeRaw( accessPrefix + " " );
		}
		this->writer_.writeRaw( "struct " + node.name );
		this->emitGenericParameters( node.genericParameters );
		this->writer_.writeRaw( ":\n" );
		this->writer_.increaseIndent();
		for( const ast::nodes::FieldDeclarationSharedPointer& field : node.fields ) {
			this->writer_.writeIndentation();
			std::string fieldAccess = this->accessModifierToString( field->access );
			if( fieldAccess.empty() == false ) {
				this->writer_.writeRaw( fieldAccess + " " );
			}
			if( field->type != nullptr ) {
				this->emitType( field->type );
				this->writer_.writeRaw( " " );
			}
			this->writer_.writeRaw( field->name );
			this->writer_.writeNewline();
		}
		for( const ast::nodes::DeclarationSharedPointer& method : node.methods ) {
			this->writer_.writeBlankLine();
			this->visitDeclaration( method );
		}
		for( const ast::nodes::DeclarationSharedPointer& nested : node.nestedDeclarations ) {
			this->writer_.writeBlankLine();
			this->visitDeclaration( nested );
		}
		this->writer_.decreaseIndent();
	}
	
	void Formatter::visit( ast::nodes::EnumDeclaration& node ) {
		this->writer_.writeIndentation();
		std::string accessPrefix = this->accessModifierToString( node.access );
		if( accessPrefix.empty() == false ) {
			this->writer_.writeRaw( accessPrefix + " " );
		}
		this->writer_.writeRaw( "enum " + node.name );
		this->emitGenericParameters( node.genericParameters );
		if( node.backedType != nullptr ) {
			this->writer_.writeRaw( " backed " );
			this->emitType( node.backedType );
		}
		this->writer_.writeRaw( ":\n" );
		this->writer_.increaseIndent();
		for( const ast::nodes::EnumVariantSharedPointer& variant : node.variants ) {
			this->writer_.writeIndentation();
			this->writer_.writeRaw( "unit " + variant->name );
			if( variant->backedValue != nullptr ) {
				this->writer_.writeRaw( " " );
				this->isInlineExpression_ = true;
				this->emitExpression( variant->backedValue );
				this->isInlineExpression_ = false;
			}
			this->writer_.writeNewline();
		}
		for( const ast::nodes::DeclarationSharedPointer& method : node.methods ) {
			this->writer_.writeBlankLine();
			this->visitDeclaration( method );
		}
		this->writer_.decreaseIndent();
	}
	
	void Formatter::visit( ast::nodes::FunctionDeclaration& node ) {
		this->writer_.writeIndentation();
		std::string accessPrefix = this->accessModifierToString( node.access );
		if( accessPrefix.empty() == false ) {
			this->writer_.writeRaw( accessPrefix + " " );
		}
		if( node.isAbstract ) {
			this->writer_.writeRaw( "abstract " );
		}
		if( node.isVirtual ) {
			this->writer_.writeRaw( "virtual " );
		}
		if( node.isStatic ) {
			this->writer_.writeRaw( "static " );
		}
		if( node.isOverride ) {
			this->writer_.writeRaw( "override " );
		}
		if( node.isFinal ) {
			this->writer_.writeRaw( "final " );
		}
		if( node.isAsync ) {
			this->writer_.writeRaw( "async " );
		}
		if( node.isProperty ) {
			this->writer_.writeRaw( "property " + node.name );
		}
		else {
			this->writer_.writeRaw( "function " + node.name );
		}
		this->emitGenericParameters( node.genericParameters );
		this->emitFunctionParameters( node.parameters );
		if( node.returnType != nullptr ) {
			this->writer_.writeRaw( " -> " );
			this->emitType( node.returnType );
		}
		if( node.raisesTypes.empty() == false ) {
			this->writer_.writeRaw( " raises " );
			for( size_t raiseIndex = 0; raiseIndex < node.raisesTypes.size(); raiseIndex++ ) {
				if( raiseIndex > 0 ) {
					this->writer_.writeRaw( ", " );
				}
				this->emitType( node.raisesTypes[raiseIndex] );
			}
		}
		if( node.body.empty() ) {
			this->writer_.writeRaw( ":" );
			this->writer_.writeNewline();
		}
		else {
			this->writer_.writeRaw( ":\n" );
			this->emitBody( node.body );
		}
	}
	
	void Formatter::visit( ast::nodes::ExternDeclaration& node ) {
		this->writer_.writeIndentation();
		std::string accessPrefix = this->accessModifierToString( node.access );
		if( accessPrefix.empty() == false ) {
			this->writer_.writeRaw( accessPrefix + " " );
		}
		this->writer_.writeRaw( "extern function " );
		if( node.linkName != node.name && node.linkName.empty() == false ) {
			this->writer_.writeRaw( node.linkName + " as " );
		}
		bool isExternParameterListPopulated = ( node.parameters.empty() == false || node.isVariadic );
		if( isExternParameterListPopulated ) {
			if( this->formattingRules_.useSpacesInsideParentheses ) {
				this->writer_.writeRaw( node.name + "( " );
			}
			else {
				this->writer_.writeRaw( node.name + "(" );
			}
		}
		else {
			if( this->formattingRules_.useSpacesInsideEmptyParens ) {
				this->writer_.writeRaw( node.name + "( )" );
			}
			else {
				this->writer_.writeRaw( node.name + "()" );
			}
		}
		for( size_t parameterIndex = 0; parameterIndex < node.parameters.size(); parameterIndex++ ) {
			if( parameterIndex > 0 ) {
				this->writer_.writeRaw( ", " );
			}
			const ast::nodes::ExternParameter& externParam = node.parameters[parameterIndex];
			if( externParam.type != nullptr ) {
				this->emitType( externParam.type );
				if( externParam.name.empty() == false ) {
					this->writer_.writeRaw( " " + externParam.name );
				}
			}
			else {
				this->writer_.writeRaw( externParam.name );
			}
		}
		if( node.isVariadic ) {
			if( node.parameters.empty() == false ) {
				this->writer_.writeRaw( ", " );
			}
			this->writer_.writeRaw( "..." );
		}
		if( isExternParameterListPopulated ) {
			if( this->formattingRules_.useSpacesInsideParentheses ) {
				this->writer_.writeRaw( " )" );
			}
			else {
				this->writer_.writeRaw( ")" );
			}
		}
		if( node.returnType != nullptr ) {
			this->writer_.writeRaw( " -> " );
			this->emitType( node.returnType );
		}
		this->writer_.writeRaw( ";" );
		this->writer_.writeNewline();
	}
	
	void Formatter::visit( ast::nodes::ImplementDeclaration& node ) {
		this->writer_.writeIndentation();
		this->writer_.writeRaw( "implement " );
		if( node.interfaceType != nullptr ) {
			this->emitType( node.interfaceType );
			this->writer_.writeRaw( " for " );
		}
		if( node.targetType != nullptr ) {
			this->emitType( node.targetType );
		}
		this->writer_.writeRaw( ":\n" );
		this->writer_.increaseIndent();
		for( const ast::nodes::DeclarationSharedPointer& method : node.methods ) {
			this->visitDeclaration( method );
			this->writer_.writeBlankLine();
		}
		this->writer_.decreaseIndent();
	}
	
	void Formatter::visit( ast::nodes::TypeAliasDeclaration& node ) {
		this->writer_.writeIndentation();
		std::string accessPrefix = this->accessModifierToString( node.access );
		if( accessPrefix.empty() == false ) {
			this->writer_.writeRaw( accessPrefix + " " );
		}
		this->writer_.writeRaw( "type " + node.name );
		this->emitGenericParameters( node.genericParameters );
		if( node.aliasedTypeNode != nullptr ) {
			this->writer_.writeRaw( " = " );
			this->emitType( node.aliasedTypeNode );
		}
		this->writer_.writeNewline();
	}
	
	void Formatter::visit( ast::nodes::VariableStatement& node ) {
		this->writer_.writeIndentation();
		if( node.type != nullptr ) {
			this->emitType( node.type );
			this->writer_.writeRaw( " " );
		}
		this->writer_.writeRaw( node.name );
		if( node.initializer != nullptr ) {
			this->writer_.writeRaw( " = " );
			this->isInlineExpression_ = true;
			this->emitExpression( node.initializer );
			this->isInlineExpression_ = false;
		}
		this->writer_.writeNewline();
	}
	
	void Formatter::visit( ast::nodes::AssignStatement& node ) {
		this->writer_.writeIndentation();
		this->isInlineExpression_ = true;
		this->emitExpression( node.target );
		this->writer_.writeRaw( " " + this->operatorToString( node.operation ) + " " );
		this->emitExpression( node.value );
		this->isInlineExpression_ = false;
		this->writer_.writeNewline();
	}
	
	void Formatter::visit( ast::nodes::ExpressionStatement& node ) {
		this->writer_.writeIndentation();
		this->isInlineExpression_ = true;
		this->emitExpression( node.expression );
		this->isInlineExpression_ = false;
		this->writer_.writeNewline();
	}
	
	void Formatter::visit( ast::nodes::ReturnStatement& node ) {
		this->writer_.writeIndentation();
		if( node.value != nullptr ) {
			this->writer_.writeRaw( "return " );
			this->isInlineExpression_ = true;
			this->emitExpression( node.value );
			this->isInlineExpression_ = false;
		}
		else {
			this->writer_.writeRaw( "return" );
		}
		this->writer_.writeNewline();
	}
	
	void Formatter::visit( ast::nodes::BreakStatement& node ) {
		this->writer_.writeIndentedLine( "break" );
	}
	
	void Formatter::visit( ast::nodes::ContinueStatement& node ) {
		this->writer_.writeIndentedLine( "continue" );
	}
	
	void Formatter::visit( ast::nodes::PassStatement& node ) {
		this->writer_.writeIndentedLine( "pass" );
	}
	
	void Formatter::visit( ast::nodes::ThrowStatement& node ) {
		this->writer_.writeIndentation();
		this->writer_.writeRaw( "raise " );
		this->isInlineExpression_ = true;
		this->emitExpression( node.expression );
		this->isInlineExpression_ = false;
		this->writer_.writeNewline();
	}
	
	void Formatter::visit( ast::nodes::DeleteStatement& node ) {
		this->writer_.writeIndentation();
		this->writer_.writeRaw( "delete " );
		this->isInlineExpression_ = true;
		this->emitExpression( node.expression );
		this->isInlineExpression_ = false;
		this->writer_.writeNewline();
	}
	
	void Formatter::visit( ast::nodes::DeferStatement& node ) {
		this->writer_.writeIndentation();
		this->writer_.writeRaw( "defer:\n" );
		this->writer_.increaseIndent();
		this->visitStatement( node.body );
		this->writer_.decreaseIndent();
	}
	
	void Formatter::visit( ast::nodes::BlockStatement& node ) {
		for( const ast::nodes::StatementSharedPointer& statement : node.statements ) {
			this->visitStatement( statement );
		}
	}
	
	void Formatter::visit( ast::nodes::IfStatement& node ) {
		this->writer_.writeIndentation();
		this->writer_.writeRaw( "if " );
		this->isInlineExpression_ = true;
		this->emitExpression( node.condition );
		this->isInlineExpression_ = false;
		this->writer_.writeRaw( ":\n" );
		this->emitBody( node.thenBody );
		for( const std::pair<ast::nodes::ExpressionSharedPointer, std::vector<ast::nodes::StatementSharedPointer>>& elifBranch : node.elifBranches ) {
			this->writer_.writeIndentation();
			this->writer_.writeRaw( "elif " );
			this->isInlineExpression_ = true;
			this->emitExpression( elifBranch.first );
			this->isInlineExpression_ = false;
			this->writer_.writeRaw( ":\n" );
			this->emitBody( elifBranch.second );
		}
		if( node.elseBody.empty() == false ) {
			this->writer_.writeIndentedLine( "else:" );
			this->emitBody( node.elseBody );
		}
	}
	
	void Formatter::visit( ast::nodes::WhileStatement& node ) {
		this->writer_.writeIndentation();
		this->writer_.writeRaw( "while " );
		this->isInlineExpression_ = true;
		this->emitExpression( node.condition );
		this->isInlineExpression_ = false;
		this->writer_.writeRaw( ":\n" );
		this->emitBody( node.body );
	}
	
	void Formatter::visit( ast::nodes::ForStatement& node ) {
		this->writer_.writeIndentation();
		if( node.isCStyle ) {
			this->writer_.writeRaw( "for " );
			if( node.variableType != nullptr ) {
				this->emitType( node.variableType );
				this->writer_.writeRaw( " " );
			}
			this->writer_.writeRaw( node.variable );
			if( node.initializer != nullptr ) {
				this->writer_.writeRaw( " = " );
				this->isInlineExpression_ = true;
				this->emitExpression( node.initializer );
				this->isInlineExpression_ = false;
			}
			this->writer_.writeRaw( "; " );
			if( node.condition != nullptr ) {
				this->isInlineExpression_ = true;
				this->emitExpression( node.condition );
				this->isInlineExpression_ = false;
			}
			this->writer_.writeRaw( "; " );
			if( node.update != nullptr ) {
				this->isInlineExpression_ = true;
				if( node.update->kind == ast::Node::Kind::ExpressionStatement ) {
					ast::nodes::ExpressionStatement& updateExpression = static_cast<ast::nodes::ExpressionStatement&>( *node.update );
					this->emitExpression( updateExpression.expression );
				}
				else if( node.update->kind == ast::Node::Kind::AssignmentStatement ) {
					ast::nodes::AssignStatement& updateAssignment = static_cast<ast::nodes::AssignStatement&>( *node.update );
					this->emitExpression( updateAssignment.target );
					this->writer_.writeRaw( " " + this->operatorToString( updateAssignment.operation ) + " " );
					this->emitExpression( updateAssignment.value );
				}
				this->isInlineExpression_ = false;
			}
			this->writer_.writeRaw( ":\n" );
		}
		else {
			this->writer_.writeRaw( "for " + node.variable + " in " );
			if( node.iterable != nullptr ) {
				this->isInlineExpression_ = true;
				this->emitExpression( node.iterable );
				this->isInlineExpression_ = false;
			}
			this->writer_.writeRaw( ":\n" );
		}
		this->emitBody( node.body );
	}
	
	void Formatter::visit( ast::nodes::MatchStatement& node ) {
		this->writer_.writeIndentation();
		this->writer_.writeRaw( "match " );
		this->isInlineExpression_ = true;
		this->emitExpression( node.subject );
		this->isInlineExpression_ = false;
		this->writer_.writeRaw( ":\n" );
		this->writer_.increaseIndent();
		for( const ast::nodes::MatchArmNodeSharedPointer& arm : node.arms ) {
			this->writer_.writeIndentation();
			this->isInlineExpression_ = true;
			this->emitExpression( arm->pattern );
			this->isInlineExpression_ = false;
			if( arm->guard != nullptr ) {
				this->writer_.writeRaw( " if " );
				this->isInlineExpression_ = true;
				this->emitExpression( arm->guard );
				this->isInlineExpression_ = false;
			}
			this->writer_.writeRaw( ":\n" );
			this->writer_.increaseIndent();
			for( const ast::nodes::StatementSharedPointer& statement : arm->body ) {
				this->visitStatement( statement );
			}
			this->writer_.decreaseIndent();
		}
		this->writer_.decreaseIndent();
	}
	
	void Formatter::visit( ast::nodes::TryCatchStatement& node ) {
		this->writer_.writeIndentedLine( "try:" );
		this->emitBody( node.tryBody );
		for( const ast::nodes::ExceptionClause& clause : node.exceptionClauses ) {
			this->writer_.writeIndentation();
			this->writer_.writeRaw( "except " );
			for( size_t typeIndex = 0; typeIndex < clause.exceptionTypes.size(); typeIndex++ ) {
				if( typeIndex > 0 ) {
					this->writer_.writeRaw( ", " );
				}
				this->emitType( clause.exceptionTypes[typeIndex] );
			}
			if( clause.variableName.empty() == false ) {
				this->writer_.writeRaw( " as " + clause.variableName );
			}
			this->writer_.writeRaw( ":\n" );
			this->emitBody( clause.body );
		}
		if( node.finallyBody.empty() == false ) {
			this->writer_.writeIndentedLine( "finally:" );
			this->emitBody( node.finallyBody );
		}
	}
	
	void Formatter::visit( ast::nodes::UnsafeBlockStatement& node ) {
		this->writer_.writeIndentedLine( "unsafe:" );
		this->emitBody( node.body );
	}
	
	void Formatter::visit( ast::nodes::BinaryExpression& node ) {
		if( this->isInlineExpression_ ) {
			this->emitExpression( node.left );
			this->writer_.writeRaw( " " + this->operatorToString( node.operation ) + " " );
			this->emitExpression( node.right );
		}
		else {
			this->writer_.writeIndentation();
			this->isInlineExpression_ = true;
			this->emitExpression( node.left );
			this->writer_.writeRaw( " " + this->operatorToString( node.operation ) + " " );
			this->emitExpression( node.right );
			this->isInlineExpression_ = false;
			this->writer_.writeNewline();
		}
	}
	
	void Formatter::visit( ast::nodes::UnaryExpression& node ) {
		std::string operatorText = this->operatorToString( node.operation );
		if( node.operation == token::Type::KeywordNot ) {
			this->writer_.writeRaw( "not " );
		}
		else if( node.operation == token::Type::KeywordMove ) {
			this->writer_.writeRaw( "move " );
		}
		else if( node.operation == token::Type::KeywordAddressof ) {
			this->writer_.writeRaw( "addressof " );
		}
		else if( node.operation == token::Type::Minus ) {
			this->writer_.writeRaw( "-" );
		}
		else if( node.operation == token::Type::Tilde ) {
			this->writer_.writeRaw( "~" );
		}
		else if( node.operation == token::Type::Increment ) {
			this->writer_.writeRaw( "++" );
		}
		else if( node.operation == token::Type::Decrement ) {
			this->writer_.writeRaw( "--" );
		}
		else {
			this->writer_.writeRaw( operatorText );
		}
		this->emitExpression( node.operand );
	}
	
	void Formatter::visit( ast::nodes::CallExpression& node ) {
		this->emitExpression( node.callee );
		bool isArgumentListEmpty = ( node.arguments.empty() && node.keywordArguments.empty() );
		if( isArgumentListEmpty ) {
			if( this->formattingRules_.useSpacesInsideEmptyParens ) {
				this->writer_.writeRaw( "( )" );
			}
			else {
				this->writer_.writeRaw( "()" );
			}
			return;
		}
		if( this->formattingRules_.useSpacesInsideParentheses ) {
			this->writer_.writeRaw( "( " );
		}
		else {
			this->writer_.writeRaw( "(" );
		}
		bool hasArguments = false;
		for( size_t argumentIndex = 0; argumentIndex < node.arguments.size(); argumentIndex++ ) {
			if( argumentIndex > 0 ) {
				this->writer_.writeRaw( ", " );
			}
			hasArguments = true;
			this->emitExpression( node.arguments[argumentIndex] );
		}
		for( size_t keywordIndex = 0; keywordIndex < node.keywordArguments.size(); keywordIndex++ ) {
			if( hasArguments || keywordIndex > 0 ) {
				this->writer_.writeRaw( ", " );
			}
			hasArguments = true;
			this->writer_.writeRaw( node.keywordArguments[keywordIndex].name + "=" );
			this->emitExpression( node.keywordArguments[keywordIndex].value );
		}
		if( this->formattingRules_.useSpacesInsideParentheses ) {
			this->writer_.writeRaw( " )" );
		}
		else {
			this->writer_.writeRaw( ")" );
		}
	}
	
	void Formatter::visit( ast::nodes::MethodCallExpression& node ) {
		this->emitExpression( node.object );
		bool isArgumentListEmpty = ( node.arguments.empty() && node.keywordArguments.empty() );
		if( isArgumentListEmpty ) {
			if( this->formattingRules_.useSpacesInsideEmptyParens ) {
				this->writer_.writeRaw( "." + node.method + "( )" );
			}
			else {
				this->writer_.writeRaw( "." + node.method + "()" );
			}
			return;
		}
		if( this->formattingRules_.useSpacesInsideParentheses ) {
			this->writer_.writeRaw( "." + node.method + "( " );
		}
		else {
			this->writer_.writeRaw( "." + node.method + "(" );
		}
		bool hasArguments = false;
		for( size_t argumentIndex = 0; argumentIndex < node.arguments.size(); argumentIndex++ ) {
			if( argumentIndex > 0 ) {
				this->writer_.writeRaw( ", " );
			}
			hasArguments = true;
			this->emitExpression( node.arguments[argumentIndex] );
		}
		for( size_t keywordIndex = 0; keywordIndex < node.keywordArguments.size(); keywordIndex++ ) {
			if( hasArguments || keywordIndex > 0 ) {
				this->writer_.writeRaw( ", " );
			}
			hasArguments = true;
			this->writer_.writeRaw( node.keywordArguments[keywordIndex].name + "=" );
			this->emitExpression( node.keywordArguments[keywordIndex].value );
		}
		if( this->formattingRules_.useSpacesInsideParentheses ) {
			this->writer_.writeRaw( " )" );
		}
		else {
			this->writer_.writeRaw( ")" );
		}
	}
	
	void Formatter::visit( ast::nodes::MemberAccessExpression& node ) {
		this->emitExpression( node.object );
		this->writer_.writeRaw( "." + node.member );
	}
	
	void Formatter::visit( ast::nodes::IndexExpression& node ) {
		this->emitExpression( node.object );
		this->writer_.writeRaw( "[" );
		this->emitExpression( node.index );
		this->writer_.writeRaw( "]" );
	}
	
	void Formatter::visit( ast::nodes::ConstructExpression& node ) {
		this->writer_.writeRaw( "new " );
		if( node.type != nullptr ) {
			this->emitType( node.type );
		}
		bool isFieldListEmpty = node.fields.empty();
		if( isFieldListEmpty ) {
			if( this->formattingRules_.useSpacesInsideEmptyParens ) {
				this->writer_.writeRaw( "( )" );
			}
			else {
				this->writer_.writeRaw( "()" );
			}
			return;
		}
		if( this->formattingRules_.useSpacesInsideParentheses ) {
			this->writer_.writeRaw( "( " );
		}
		else {
			this->writer_.writeRaw( "(" );
		}
		for( size_t fieldIndex = 0; fieldIndex < node.fields.size(); fieldIndex++ ) {
			if( fieldIndex > 0 ) {
				this->writer_.writeRaw( ", " );
			}
			this->emitExpression( node.fields[fieldIndex].second );
		}
		if( this->formattingRules_.useSpacesInsideParentheses ) {
			this->writer_.writeRaw( " )" );
		}
		else {
			this->writer_.writeRaw( ")" );
		}
	}
	
	void Formatter::visit( ast::nodes::CastExpression& node ) {
		this->emitExpression( node.expression );
		this->writer_.writeRaw( " as " );
		if( node.targetType != nullptr ) {
			this->emitType( node.targetType );
		}
	}
	
	void Formatter::visit( ast::nodes::LambdaExpression& node ) {
		this->writer_.writeRaw( "lambda" );
		if( node.parameters.empty() == false ) {
			this->writer_.writeRaw( " " );
			for( size_t parameterIndex = 0; parameterIndex < node.parameters.size(); parameterIndex++ ) {
				if( parameterIndex > 0 ) {
					this->writer_.writeRaw( ", " );
				}
				const ast::nodes::FunctionParameterNode& parameter = *node.parameters[parameterIndex];
				if( parameter.type != nullptr ) {
					this->emitType( parameter.type );
					this->writer_.writeRaw( " " );
				}
				this->writer_.writeRaw( parameter.name );
			}
		}
		if( node.returnType != nullptr ) {
			this->writer_.writeRaw( " -> " );
			this->emitType( node.returnType );
		}
		this->writer_.writeRaw( ":\n" );
		this->emitBody( node.body );
	}
	
	void Formatter::visit( ast::nodes::ArrayExpression& node ) {
		this->writer_.writeRaw( "[" );
		for( size_t elementIndex = 0; elementIndex < node.elements.size(); elementIndex++ ) {
			if( elementIndex > 0 ) {
				this->writer_.writeRaw( ", " );
			}
			this->emitExpression( node.elements[elementIndex] );
		}
		this->writer_.writeRaw( "]" );
	}
	
	void Formatter::visit( ast::nodes::TupleExpression& node ) {
		this->writer_.writeRaw( "(" );
		for( size_t elementIndex = 0; elementIndex < node.elements.size(); elementIndex++ ) {
			if( elementIndex > 0 ) {
				this->writer_.writeRaw( ", " );
			}
			this->emitExpression( node.elements[elementIndex] );
		}
		this->writer_.writeRaw( ")" );
	}
	
	void Formatter::visit( ast::nodes::RangeExpression& node ) {
		this->emitExpression( node.start );
		if( node.inclusive ) {
			this->writer_.writeRaw( "..=" );
		}
		else {
			this->writer_.writeRaw( ".." );
		}
		this->emitExpression( node.end );
	}
	
	void Formatter::visit( ast::nodes::IdentifierExpression& node ) {
		this->writer_.writeRaw( node.name );
	}
	
	void Formatter::visit( ast::nodes::IntegerLiteralExpression& node ) {
		this->writer_.writeRaw( std::to_string( node.value ) );
	}
	
	void Formatter::visit( ast::nodes::FloatLiteralExpression& node ) {
		this->writer_.writeRaw( fmt::format( "{}", node.value ) );
	}
	
	void Formatter::visit( ast::nodes::StringLiteralExpression& node ) {
		std::string escapedValue;
		for( char character : node.value ) {
			switch( character ) {
				case '\n': escapedValue+= "\\n"; break;
				case '\r': escapedValue+= "\\r"; break;
				case '\t': escapedValue+= "\\t"; break;
				case '\0': escapedValue+= "\\0"; break;
				case '\\': escapedValue+= "\\\\"; break;
				case '"': escapedValue+= "\\\""; break;
				default: escapedValue+= character; break;
			}
		}
		this->writer_.writeRaw( "\"" + escapedValue + "\"" );
	}
	
	void Formatter::visit( ast::nodes::CharLiteralExpression& node ) {
		this->writer_.writeRaw( "'" + std::string( 1, node.value ) + "'" );
	}
	
	void Formatter::visit( ast::nodes::BoolLiteralExpression& node ) {
		this->writer_.writeRaw( node.value ? "True" : "False" );
	}
	
	void Formatter::visit( ast::nodes::NoneLiteralExpression& node ) {
		this->writer_.writeRaw( "None" );
	}
	
	void Formatter::visit( ast::nodes::SelfExpression& node ) {
		this->writer_.writeRaw( "self" );
	}
	
	void Formatter::visit( ast::nodes::SuperExpression& node ) {
		this->writer_.writeRaw( "parent" );
	}

}
