
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

#include <functional>

#include "uranite/parser/parser.hpp"

namespace uranite::parser {
	
	Parser::Parser(
		std::vector<token::Token> tokens,
		diagnostic::Engine& diagnostic
	) : diagnostic( diagnostic ),
		tokens( std::move( tokens ) ) {
	}
	
	const token::Token& Parser::advance() {
		const token::Token& token = this->current();
		if( this->position < this->tokens.size() ) {
			this->position++;
		}
		return token;
	}
	
	bool Parser::check( token::Type type ) const {
		return this->current().type == type;
	}
	
	bool Parser::check( token::Type type, bool optional ) const {
		return this->check( type ) == optional;
	}
	
	bool Parser::check( const std::vector<token::Type>& types ) const {
		for( const token::Type& type : types ) {
			if( this->check( type ) ) {
				return true;
			}
		}
		return false;
	}
	
	bool Parser::check( const std::vector<token::Type>& types, bool optional ) const {
		return this->check( types ) == optional;
	}
	
	bool Parser::checkGreaterThan() const {
		return this->pendingGreaterThans > 0 || this->check({ token::Type::GreaterThan, token::Type::ShiftRight });
	}
	
	token::Token Parser::consumeGreaterThan( const std::string& message ) {
		if( this->pendingGreaterThans > 0 ) {
			this->pendingGreaterThans--;
			return token::Token( token::Type::GreaterThan, this->current().source, ">" );
		}
		if( this->check( token::Type::GreaterThan ) ) {
			return advance();
		}
		if( this->check( token::Type::ShiftRight ) ) {
			token::Token tok = this->current();
			this->advance();
			this->pendingGreaterThans = 1;
			return token::Token( token::Type::GreaterThan, tok.source, ">" );
		}
		this->diagnostic.error( this->current().source, fmt::format( "expected > but found '{}' ({})", this->current().value, token::toString( this->current().type ) ), message );
		return token::Token( token::Type::Error, this->current().source, "" );
	}
	
	const token::Token& Parser::current() const {
		if( this->position >= this->tokens.size() ) {
			static token::Token eof( token::Type::Eof, std::make_shared<lookup::Source>(), "" );
			return eof;
		}
		return this->tokens[this->position];
	}
	
	token::Token Parser::expect( token::Type type, const std::string& message ) {
		if( type == token::Type::GreaterThan ) {
			return this->consumeGreaterThan( message );
		}
		if( this->check( type ) ) {
			return this->advance();
		}
		this->diagnostic.error( this->current().source, fmt::format( "expected \"{}\" but found \"{}\" ({})", std::string( token::toString( type ) ), this->current().value, token::toString( this->current().type ) ), message );
		return token::Token( token::Type::Error, this->current().source, "" );
	}
	
	void Parser::expectNewline( const std::string& context ) {
		if( this->check( { token::Type::Dedent, token::Type::Eof, token::Type::Newline }, false ) ) {
			this->diagnostic.error( this->current().source, fmt::format( "expected newline after {}", context ) );
		}
		this->skipNewline();
	}
	
	int Parser::getOperatorPrecedenceValue( token::Type type ) const {
		switch( type ) {
			case token::Type::Ampersand:
				return 5;
			case token::Type::Caret:
				return 4;
			case token::Type::Equal:
			case token::Type::NotEqual:
				return 6;
			case token::Type::GreaterThan:
			case token::Type::GreaterThanEqual:
			case token::Type::KeywordIn:
			case token::Type::KeywordInstanceOf:
			case token::Type::KeywordIs:
			case token::Type::KeywordSubclassOf:
			case token::Type::LessThan:
			case token::Type::LessThanEqual:
				return 7;
			case token::Type::KeywordAnd:
				return 2;
			case token::Type::KeywordAs:
				return 12;
			case token::Type::KeywordOr:
				return 1;
			case token::Type::Minus:
			case token::Type::Plus:
				return 9;
			case token::Type::Percent:
			case token::Type::Slash:
			case token::Type::Star:
				return 10;
			case token::Type::Pipe:
				return 3;
			case token::Type::Power:
				return 11;
			case token::Type::ShiftLeft:
			case token::Type::ShiftRight:
				return 8;
			default:
				return -1;
		}
	}
	
	bool Parser::isAtEnd() const {
		return this->check( token::Type::Eof );
	}
	
	bool Parser::isAtEnd( bool optional ) const {
		return this->isAtEnd() == optional;
	}
	
	bool Parser::isBinaryOperator( token::Type type ) const {
		return this->getOperatorPrecedenceValue( type ) > 0;
	}
	
	bool Parser::isExpressionStart() const {
		switch( this->current().type ) {
			case token::Type::Ampersand:
			case token::Type::Bang:
			case token::Type::Identifier:
			case token::Type::KeywordFalse:
			case token::Type::KeywordFunction:
			case token::Type::KeywordMove:
			case token::Type::KeywordNew:
			case token::Type::KeywordNone:
			case token::Type::KeywordNot:
			case token::Type::KeywordParent:
			case token::Type::KeywordSelf:
			case token::Type::KeywordTrue:
			case token::Type::LeftBracket:
			case token::Type::LeftParenthesis:
			case token::Type::LiteralChar:
			case token::Type::LiteralFloat:
			case token::Type::LiteralInteger:
			case token::Type::LiteralRegex:
			case token::Type::LiteralString:
			case token::Type::Minus:
			case token::Type::Star:
			case token::Type::Tilde:
				return true;
			default:
				return false;
		}
	}
	
	bool Parser::isStatementStart() const {
		return (
			this->check( token::Type::KeywordAssembly ) ||
			this->check( token::Type::KeywordBreak ) ||
			this->check( token::Type::KeywordConstant ) ||
			this->check( token::Type::KeywordContinue ) ||
			this->check( token::Type::KeywordDefer ) ||
			this->check( token::Type::KeywordDelete ) ||
			this->check( token::Type::KeywordFor ) ||
			this->check( token::Type::KeywordIf ) ||
			this->check( token::Type::KeywordMatch ) ||
			this->check( token::Type::KeywordPrivate ) ||
			this->check( token::Type::KeywordProtect ) ||
			this->check( token::Type::KeywordPublic ) ||
			this->check( token::Type::KeywordRaise ) ||
			this->check( token::Type::KeywordReturn ) ||
			this->check( token::Type::KeywordTry ) ||
			this->check( token::Type::KeywordUnsafe ) ||
			this->check( token::Type::KeywordWhile ) ||
			this->check( token::Type::Question ) ||  // nullable type variable declaration
           	this->isExpressionStart()
		);
	}
	
	bool Parser::match( token::Type type ) {
		if( type == token::Type::GreaterThan ) {
			if( this->checkGreaterThan() ) {
				this->consumeGreaterThan( "" );
				return true;
			}
			return false;
		}
		if( this->check( type ) ) {
			this->advance();
			return true;
		}
		return false;
	}
	
	bool Parser::match( token::Type type, bool optional ) {
		return this->match( type ) == optional;
	}
	
	ast::nodes::ProgramSharedPointer Parser::parse() {
		ast::nodes::ProgramSharedPointer program(
			std::make_shared<ast::nodes::Program>( this->current().source )
		);
		this->skipNewline();
		
		// Optional package declaration
		if( this->check( token::Type::KeywordPackage ) ) {
			program->module = this->parseModuleDeclaration();
			this->skipNewline();
		}
		
		// Import declarations (before any other declarations)
		while( this->check( token::Type::KeywordImport ) || this->check( token::Type::KeywordFrom ) ) {
			ast::nodes::ImportDeclarationSharedPointer importDeclaration = this->parseImportDeclaration();
			program->imports.push_back( importDeclaration );
			this->skipNewline();
		}
		
		// Top-level declarations, exports, and imports (interleaved)
		while( this->isAtEnd( false ) ) {
			this->skipNewline();
			if( this->isAtEnd() ) {
				break;
			}
			if( this->check( token::Type::KeywordImport ) || this->check( token::Type::KeywordFrom ) ) {
				ast::nodes::ImportDeclarationSharedPointer importDeclaration = this->parseImportDeclaration();
				program->imports.push_back( importDeclaration );
			}
			else if( this->check( token::Type::KeywordExport ) ) {
				ast::nodes::ExportDeclarationSharedPointer exportDeclaration = this->parseExportDeclaration();
				if( exportDeclaration ) {
					program->exports.push_back( exportDeclaration );
					if( exportDeclaration->declaration ) {
						program->declarations.push_back( exportDeclaration->declaration );
					}
				}
			}
			else {
				ast::nodes::DeclarationSharedPointer declaration = this->parseDeclaration();
				if( declaration ) {
					program->declarations.push_back( declaration );
				}
			}
			this->skipNewline();
		}
		return program;
	}
	
	ast::AccessModifier Parser::parseAccessModifier() {
		if( this->match( token::Type::KeywordPublic ) ) {
			return ast::AccessModifier::Public;
		}
		if( this->match( token::Type::KeywordPrivate ) ) {
			return ast::AccessModifier::Private;
		}
		if( this->match( token::Type::KeywordProtect ) ) {
			return ast::AccessModifier::Protect;
		}
		return ast::AccessModifier::Default;
	}
	
	ast::nodes::ExpressionSharedPointer Parser::parseArrayExpression() {
		lookup::SourceSharedPointer source = this->current().source;
		this->expect( token::Type::LeftBracket, "" );
		if( this->check( token::Type::RightBracket ) ) {
			this->advance();
			return std::make_shared<ast::nodes::ArrayExpression>( std::vector<ast::nodes::ExpressionSharedPointer>{}, source );
		}
		ast::nodes::ExpressionSharedPointer firstExpression = parseExpression();
		
		// Comprehension: [expression for Type variable in iterable] or [expression for Type variable in iterable if condition]
		if( this->check( token::Type::KeywordFor ) ) {
			this->advance(); // consume 'for'
			ast::nodes::TypeNodeSharedPointer variableType;
			std::string variableName;
			
			// Parse optional type and variable name
			if( this->check( token::Type::Identifier ) && this->peek( 1 ).type == token::Type::Identifier ) {
				variableType = this->parseTypeNode();
				variableName = this->expect( token::Type::Identifier, "expected variable name" ).value;
			}
			else if( this->check( token::Type::Identifier ) && this->peek( 1 ).type == token::Type::KeywordIn ) {
				variableName = this->current().value;
				this->advance();
			}
			else {
				variableName = this->expect( token::Type::Identifier, "expected variable name" ).value;
			}
			this->expect( token::Type::KeywordIn, "expected 'in' in comprehension" );
			ast::nodes::ExpressionSharedPointer iterable = this->parseExpression();
			ast::nodes::ExpressionSharedPointer condition;
			if( this->match( token::Type::KeywordIf ) ) {
				condition = this->parseExpression();
			}
			this->expect( token::Type::RightBracket, "expected ']' after comprehension" );
			return std::make_shared<ast::nodes::ComprehensionExpression>(
				firstExpression, 
				condition, 
				iterable, 
				source,
				variableName, 
				variableType
			);
		}
		
		// Regular array literal
		std::vector<ast::nodes::ExpressionSharedPointer> elements;
		elements.push_back( firstExpression );
		while( this->match(token::Type::Comma ) ) {
			elements.push_back( this->parseExpression() );
		}
		this->expect( token::Type::RightBracket, "expected ']'" );
		return std::make_shared<ast::nodes::ArrayExpression>( std::move( elements ), source );
	}
	
	ast::nodes::TypeNodeSharedPointer Parser::parseBaseTypeNode() {
		lookup::SourceSharedPointer source = this->current().source;
		
		// Reference types: &Type, &mut Type
		if( this->check( token::Type::Ampersand ) ) {
			this->advance();
			bool isMutable = this->match( token::Type::KeywordMutable );
			ast::nodes::TypeNodeSharedPointer inner = this->parseTypeNode();
			return std::make_shared<ast::nodes::ReferenceTypeNode>( inner, isMutable, source );
		}
		
		// Pointer types: *Type, *mut Type
		if( this->check( token::Type::Star ) ) {
			this->advance();
			bool isMutable = this->match( token::Type::KeywordMutable );
			ast::nodes::TypeNodeSharedPointer inner = this->parseTypeNode();
			return std::make_shared<ast::nodes::PointerTypeNode>( inner, isMutable, source );
		}
		
		// Array types: [Type], [Type; N]
		if( this->check( token::Type::LeftBracket ) ) {
			this->advance();
			ast::nodes::TypeNodeSharedPointer elementType = this->parseTypeNode();
			ast::nodes::ExpressionSharedPointer size;
			if( this->match( token::Type::Semicolon ) ) {
				size = this->parseExpression();
			}
			this->expect( token::Type::RightBracket, "expected ']' in array type" );
			return std::make_shared<ast::nodes::ArrayTypeNode>( elementType, size, source );
		}
		
		// Tuple types: (Type1, Type2)
		if( this->check( token::Type::LeftParenthesis ) ) {
			this->advance();
			std::vector<ast::nodes::TypeNodeSharedPointer> elements;
			if( this->check( token::Type::RightParenthesis, false ) ) {
				elements.push_back( this->parseTypeNode() );
				while( this->match( token::Type::Comma ) ) {
					elements.push_back( this->parseTypeNode() );
				}
			}
			this->expect( token::Type::RightParenthesis, "expected ')'" );
			if( elements.size() == 1 ) {
				return elements[0]; // Just grouping
			}
			return std::make_shared<ast::nodes::TupleTypeNode>( std::move( elements ), source );
		}
		
		// Function types: fn(Type, ...) -> ReturnType
		if( this->check( token::Type::KeywordFunction ) ) {
			this->advance();
			this->expect( token::Type::LeftParenthesis, "expected '('" );
			std::vector<ast::nodes::TypeNodeSharedPointer> parameterTypes;
			if( this->check( token::Type::RightParenthesis, false ) ) {
				parameterTypes.push_back( this->parseTypeNode() );
				while( this->match( token::Type::Comma ) ) {
					parameterTypes.push_back( this->parseTypeNode() );
				}
			}
			this->expect( token::Type::RightParenthesis, "expected ')'" );
			ast::nodes::TypeNodeSharedPointer returnType;
			if( this->match( token::Type::Arrow ) ) {
				returnType = this->parseTypeNode();
			}
			else {
				returnType = std::make_shared<ast::nodes::SimpleTypeNode>( "void", source );
			}
			return std::make_shared<ast::nodes::FunctionTypeNode>( std::move( parameterTypes ), returnType, source );
		}
		
		// Named types (including primitives and user-defined)
		std::string name;
		if( this->check( token::Type::Identifier ) ) {
			name = this->current().value;
			this->advance();
		}
		else if( this->check( token::Type::KeywordSelf ) ) {
			name = "Self";
			this->advance();
		}
		else {
			this->diagnostic.error( this->current().source, fmt::format( "expected type name but found \"{}\"", this->current().value ) );
			return std::make_shared<ast::nodes::SimpleTypeNode>( "error", source );
		}
		
		// Meta<T> — type-as-value wrapper
		if( name == "Meta" && this->check( token::Type::LessThan ) ) {
			this->advance(); // consume <
			ast::nodes::TypeNodeSharedPointer inner = this->parseTypeNode();
			this->expect( token::Type::GreaterThan, "expected '>' after Meta type parameter" );
			return std::make_shared<ast::nodes::MetaTypeNode>( inner, source );
		}
		
		// Callable<Return, <Parameter1, Parameter2, ...>>
		if( name == "Callable" && this->check( token::Type::LessThan ) ) {
			this->advance(); // consume <
			ast::nodes::TypeNodeSharedPointer returnType = this->parseTypeNode();
			this->expect( token::Type::Comma, "expected ',' after Callable return type" );
			this->expect( token::Type::LessThan, "expected '<' before Callable parameter types" );
			std::vector<ast::nodes::TypeNodeSharedPointer> parameterTypes;
			if( this->check( { token::Type::GreaterThan, token::Type::ShiftRight }, false ) ) {
				parameterTypes.push_back( this->parseTypeNode() );
				while( this->match( token::Type::Comma ) ) {
					parameterTypes.push_back( this->parseTypeNode() );
				}
			}
			this->expect( token::Type::GreaterThan, "expected '>' after Callable parameter types" );
			this->expect( token::Type::GreaterThan, "expected '>' to close Callable type" );
			return std::make_shared<ast::nodes::CallableTypeNode>( returnType, std::move( parameterTypes ), source );
		}
		
		// Check for generic arguments: Name<T1, T2>
		ast::nodes::TypeNodeSharedPointer result;
		if( this->check( token::Type::LessThan ) ) {
			std::vector<ast::nodes::TypeNodeSharedPointer> typeArguments = this->parseGenericArguments();
			if( typeArguments.empty() == false ) {
				result = std::make_shared<ast::nodes::GenericTypeNode>( name, std::move( typeArguments ), source );
			}
		}
		if( result == nullptr ) {
			result = std::make_shared<ast::nodes::SimpleTypeNode>( name, source );
		}
		// T[] array syntax sugar → [T]
		// if( this->check( token::Type::LeftBracket ) && this->peek(1).type == token::Type::RightBracket ) {
		// 	this->advance(); // consume [
		// 	this->advance(); // consume ]
		// 	result = std::make_shared<ast::nodes::ArrayTypeNode>( result, nullptr, source );
		// }
		return result;
	}
	
	ast::nodes::ClassDeclarationSharedPointer Parser::parseClassDeclaration( ast::AccessModifier access ) {
		this->expect( token::Type::KeywordClass, "" );
		token::Token nameToken = this->expect( token::Type::Identifier, "expected class name" );
		ast::nodes::ClassDeclarationSharedPointer declaration = std::make_shared<ast::nodes::ClassDeclaration>( nameToken.value, nameToken.source );
		declaration->access = access;
		
		// Generic parameters
		declaration->genericParameters = this->parseGenericParameters();
		
		// Inheritance: class Name extends Base implements Interface1, Interface2:
		if( this->check( token::Type::KeywordExtends ) ) {
			this->advance(); // consume 'extends'
			declaration->baseClassType = this->parseTypeNode();
			
			// Additional base classes via comma (multiple inheritance not supported, but parse gracefully)
			while( this->match( token::Type::Comma ) ) {
				declaration->interfaces.push_back( this->parseTypeNode() );
			}
		}
		
		// Interface implementation: class Name implements Interface1, Interface2:
		if( this->check( token::Type::KeywordImplements ) ) {
			this->advance(); // consume 'implements'
			declaration->interfaces.push_back( this->parseTypeNode() );
			while( this->match( token::Type::Comma ) ) {
				declaration->interfaces.push_back( this->parseTypeNode() );
			}
		}
		
		// Forward declaration: class X;
		if( this->match( token::Type::Semicolon ) ) {
			this->expectNewline( "class forward declaration" );
			return declaration;
		}
		this->expect( token::Type::Colon, "expected ':' or ';' after class declaration" );
		this->expectNewline( "class declaration" );
		
		// Class body
		if( this->match( token::Type::Indent ) ) {
			while( this->check( { token::Type::Dedent, token::Type::Eof }, false ) ) {
				this->skipNewline();
				if( this->check( token::Type::Dedent ) ) {
					break;
				}
				ast::AccessModifier memberAccess = this->parseAccessModifier();
				bool memberAbstract = false;
				bool memberAsync = false;
				bool memberFinal = false;
				bool memberNative = false;
				bool memberOverride = false;
				bool memberReadonly = false;
				bool memberStatic = false;
				bool memberVirtual = false;
				while( true ) {
					if( this->match( token::Type::KeywordAbstract ) ) {
						memberAbstract = true; continue;
					}
					if( this->match( token::Type::KeywordAsync ) ) {
						memberAsync = true; continue;
					}
					if( this->match( token::Type::KeywordConstant ) ) {
						continue;
					}
					if( this->match( token::Type::KeywordFinal ) ) {
						memberFinal = true; continue;
					}
					if( this->match( token::Type::KeywordNative ) ) {
						memberNative = true; continue;
					}
					if( this->match( token::Type::KeywordOverride ) ) {
						memberOverride = true; continue;
					}
					if( this->match( token::Type::KeywordReadonly ) ) {
						memberReadonly = true; continue;
					}
					if( this->match( token::Type::KeywordStatic ) ) {
						memberStatic = true; continue;
					}
					if( this->match( token::Type::KeywordVirtual ) ) {
						memberVirtual = true; continue;
					}
					break;
				}
				if( this->check( token::Type::KeywordFunction ) || 
					this->check( token::Type::KeywordProperty ) ) {
					bool isProperty = this->check( token::Type::KeywordProperty );
					ast::nodes::FunctionDeclarationSharedPointer method = std::static_pointer_cast<ast::nodes::FunctionDeclaration>(
						this->parseFunctionDeclaration(
							memberAccess, 
							memberVirtual,
							memberOverride, 
							memberAbstract, 
							memberStatic
						)
					);
					if( isProperty ) {
						method->isProperty = true;
					}
					if( memberAsync ) {
						method->isAsync = true;
					}
					if( memberFinal ) {
						method->isFinal = true;
					}
					if( memberNative ) {
						method->isNative = true;
					}
					declaration->methods.push_back( method );
				}
				else if( this->check( token::Type::KeywordClass ) ) {
					declaration->nestedDeclarations.push_back( this->parseClassDeclaration( memberAccess ) );
				}
				else if( this->check( token::Type::KeywordInterface ) ) {
					declaration->nestedDeclarations.push_back( this->parseInterfaceDeclaration( memberAccess ) );
				}
				else if( this->check( token::Type::KeywordStruct ) ) {
					declaration->nestedDeclarations.push_back( this->parseStructDeclaration( memberAccess ) );
				}
				else if( this->check( token::Type::KeywordEnum ) ) {
					declaration->nestedDeclarations.push_back( this->parseEnumDeclaration( memberAccess ) );
				}
				else if( this->check( token::Type::KeywordUse ) ) {
					this->advance();
					std::string traitName = this->expect( token::Type::Identifier, "expected trait name after 'use'" ).value;
					declaration->usedTraits.push_back( traitName );
					while( this->match( token::Type::Comma ) ) {
						traitName = this->expect( token::Type::Identifier, "expected trait name" ).value;
						declaration->usedTraits.push_back( traitName );
					}
					this->expectNewline( "use statement" );
				}
				else if( this->check( token::Type::Identifier ) || this->check( token::Type::Question ) ) {
					
					// Field: Type name [= value]  or ?Type name [= value]
					ast::nodes::FieldDeclarationSharedPointer field = this->parseFieldDeclaration( memberAccess );
					if( memberFinal ) {
						field->isFinal = true;
					}
					if( memberReadonly ) {
						field->isReadonly = true;
					}
					if( memberStatic ) {
						field->isStatic = true;
					}
					declaration->fields.push_back( field );
				}
				else {
					this->diagnostic.error( this->current().source, fmt::format( "unexpected token \"{}\" in class body", this->current().value ) );
					this->advance();
				}
				this->skipNewline();
			}
			this->match( token::Type::Dedent );
		}
		return declaration;
	}
	
	ast::nodes::DeclarationSharedPointer Parser::parseDeclaration() {
		ast::AccessModifier access = this->parseAccessModifier();
		
		// Check for modifiers before function
		bool isAbstract = false;
		bool isAsync = false;
		bool isFinal = false;
		bool isNative = false;
		bool isOverride = false;
		bool isReadonly = false;
		bool isStatic = false;
		bool isVirtual = false;
		while( true ) {
			if( this->match( token::Type::KeywordAbstract ) ) {
				isAbstract = true;
				continue;
			}
			if( this->match( token::Type::KeywordAsync ) ) {
				isAsync = true;
				continue;
			}
			if( this->match( token::Type::KeywordFinal ) ) {
				isFinal = true;
				continue;
			}
			if( this->match( token::Type::KeywordNative ) ) {
				isNative = true;
				continue;
			}
			if( this->match( token::Type::KeywordOverride ) ) {
				isOverride = true;
				continue;
			}
			if( this->match( token::Type::KeywordReadonly ) ) {
				isReadonly = true;
				continue;
			}
			if( this->match( token::Type::KeywordStatic ) ) {
				isStatic = true;
				continue;
			}
			if( this->match( token::Type::KeywordVirtual ) ) {
				isVirtual = true;
				continue;
			}
			break;
		}
		switch( this->current().type ) {
			case token::Type::KeywordProperty: {
				ast::nodes::FunctionDeclarationSharedPointer declaration = this->parseFunctionDeclaration( access, isVirtual, isOverride, isAbstract, isStatic );
				declaration->isProperty = true;
				if( isFinal ) {
					declaration->isFinal = true;
				}
				if( isNative ) {
					declaration->isNative = true;
				}
				return declaration;
			}
			case token::Type::KeywordFunction: {
				ast::nodes::FunctionDeclarationSharedPointer declaration = this->parseFunctionDeclaration( access, isVirtual, isOverride, isAbstract, isStatic );
				if( isAsync ) {
					declaration->isAsync = true;
				}
				if( isFinal ) {
					declaration->isFinal = true;
				}
				if( isNative ) {
					declaration->isNative = true;
				}
				return declaration;
			}
			case token::Type::KeywordClass: {
				ast::nodes::ClassDeclarationSharedPointer declaration = this->parseClassDeclaration( access );
				if( isFinal ) {
					declaration->isFinal = true;
				}
				if( isNative ) {
					declaration->isNative = true;
				}
				if( isReadonly ) {
					declaration->isReadonly = true;
				}
				return declaration;
			}
			case token::Type::KeywordStruct:
				return this->parseStructDeclaration( access );
			case token::Type::KeywordEnum:
				return this->parseEnumDeclaration( access );
			case token::Type::KeywordInterface: {
				ast::nodes::InterfaceDeclarationSharedPointer declaration = parseInterfaceDeclaration( access );
				if( isFinal ) {
					declaration->isFinal = true;
				}
				return declaration;
			}
			case token::Type::KeywordTrait:
				return this->parseTraitDeclaration( access );
			case token::Type::KeywordImplements:
				return this->parseImplementDeclaration();
			case token::Type::KeywordType:
				return this->parseTypeAliasDeclaration( access );
			case token::Type::KeywordExtern:
				return this->parseExternDeclaration();
			case token::Type::KeywordConstant: {
				lookup::SourceSharedPointer constSource = this->current().source;
				this->advance();
				ast::nodes::TypeNodeSharedPointer constType = this->parseTypeNode();
				std::string constName = this->current().value;
				this->expect( token::Type::Identifier, "expected constant name" );
				ast::nodes::ExpressionSharedPointer constInitializer = nullptr;
				if( this->match( token::Type::Assignment ) ) {
					constInitializer = this->parseExpression();
				}
				return std::make_shared<ast::nodes::ConstantDeclaration>( constName, constType, constInitializer, access, constSource );
			}
			case token::Type::Identifier: {
				if( this->peek().type == token::Type::Identifier ) {
					lookup::SourceSharedPointer varSource = this->current().source;
					ast::nodes::TypeNodeSharedPointer varType = this->parseTypeNode();
					std::string varName = this->expect( token::Type::Identifier, "expected variable name" ).value;
					ast::nodes::ExpressionSharedPointer varInitializer = nullptr;
					if( this->match( token::Type::Assignment ) ) {
						varInitializer = this->parseExpression();
					}
					return std::make_shared<ast::nodes::ConstantDeclaration>( varName, varType, varInitializer, access, varSource, true );
				}
				[[fallthrough]];
			}
			default:
				std::string errorHinting( "expected a declaration (function, class, struct, enum, interface, trait, implements, type, const)" );
				std::string errorMessage( fmt::format( "unexpected token \"{}\" at top level", this->current().value ) );
				this->diagnostic.error( this->current().source, errorMessage, errorHinting );
				this->advance();
				return nullptr;
		}
	}
	
	ast::nodes::StatementSharedPointer Parser::parseInlineAssemblyStatement() {
		lookup::SourceSharedPointer source = this->current().source;
		this->expect( token::Type::KeywordAssembly, "" );
		bool isVolatile = false;
		if( this->check( token::Type::KeywordVolatile ) ) {
			isVolatile = true;
			this->advance();
		}
		std::string asmTemplate = this->current().value;
		this->expect( token::Type::LiteralString, "expected assembly template string" );
		std::vector<ast::nodes::InlineAssemblyOperand> outputs;
		std::vector<ast::nodes::InlineAssemblyOperand> inputs;
		std::vector<std::string> clobbers;
		std::function<std::vector<ast::nodes::InlineAssemblyOperand>()> parseOperandList = [&]() -> std::vector<ast::nodes::InlineAssemblyOperand> {
			std::vector<ast::nodes::InlineAssemblyOperand> operands;
			this->expect( token::Type::LeftParenthesis, "expected '(' after output/input" );
			while( this->check( token::Type::RightParenthesis ) == false && this->isAtEnd() == false ) {
				ast::nodes::InlineAssemblyOperand operand;
				operand.constraint = this->current().value;
				this->expect( token::Type::LiteralString, "expected constraint string" );
				operand.expression = this->parseExpression();
				if( this->check( token::Type::Comma ) ) {
					this->advance();
				}
				operands.push_back( std::move( operand ) );
			}
			this->expect( token::Type::RightParenthesis, "expected ')'" );
			return operands;
		};
		if( this->check( token::Type::Colon ) ) {
			this->advance();
			if( this->check( token::Type::Identifier ) && this->current().value == "output" ) {
				this->advance();
				outputs = parseOperandList();
			}
			if( this->check( token::Type::Colon ) ) {
				this->advance();
				if( this->check( token::Type::Identifier ) && this->current().value == "input" ) {
					this->advance();
					inputs = parseOperandList();
				}
				if( this->check( token::Type::Colon ) ) {
					this->advance();
					if( this->check( token::Type::Identifier ) && this->current().value == "clobber" ) {
						this->advance();
						this->expect( token::Type::LeftParenthesis, "expected '(' after clobber" );
						while( this->check( token::Type::RightParenthesis ) == false && this->isAtEnd() == false ) {
							clobbers.push_back( this->current().value );
							this->expect( token::Type::LiteralString, "expected clobber register name" );
							if( this->check( token::Type::Comma ) ) {
								this->advance();
							}
						}
						this->expect( token::Type::RightParenthesis, "expected ')'" );
					}
				}
			}
		}
		this->expectNewline( "inline assembly" );
		return std::make_shared<ast::nodes::InlineAssemblyStatement>(
			isVolatile,
			asmTemplate,
			std::move( outputs ),
			std::move( inputs ),
			std::move( clobbers ),
			source
		);
	}
	
	ast::nodes::StatementSharedPointer Parser::parseDeferStatement() {
		lookup::SourceSharedPointer source = this->current().source;
		this->expect( token::Type::KeywordDefer, "" );
		if( this->match( token::Type::Colon ) ) {
			this->expectNewline( "defer block" );
			std::vector<ast::nodes::StatementSharedPointer> body = this->parseStatementBlock();
			return std::make_shared<ast::nodes::DeferStatement>( 
				std::make_shared<ast::nodes::BlockStatement>( 
					std::move( body ), 
					source 
				), 
				source 
			);
		}
		return std::make_shared<ast::nodes::DeferStatement>( this->parseStatement(), source );
	}
	
	ast::nodes::EnumDeclarationSharedPointer Parser::parseEnumDeclaration( ast::AccessModifier access ) {
		this->expect( token::Type::KeywordEnum, "" );
		token::Token nameToken = this->expect( token::Type::Identifier, "expected enum name" );
		ast::nodes::EnumDeclarationSharedPointer declaration = std::make_shared<ast::nodes::EnumDeclaration>( nameToken.value, nameToken.source );
		declaration->access = access;
		declaration->genericParameters = this->parseGenericParameters();
		
		// backed type: enum Color backed Integer:
		if( this->match( token::Type::KeywordBacked ) ) {
			declaration->backedType = this->parseTypeNode();
		}
		
		// extends: enum Color backed Integer extends String:
		if( this->match( token::Type::KeywordExtends ) ) {
			declaration->baseClassType = this->parseTypeNode();
		}
		
		// implements: enum Color backed Integer extends String implements Stringable:
		if( this->match( token::Type::KeywordImplements ) ) {
			do {
				declaration->interfaces.push_back( this->parseTypeNode() );
			}
			while( this->match( token::Type::Comma ) );
		}
		
		// Forward declaration: enum X;
		if( this->match( token::Type::Semicolon ) ) {
			this->expectNewline( "enum forward declaration" );
			return declaration;
		}
		
		this->expect( token::Type::Colon, "expected ':' or ';' after enum declaration" );
		this->expectNewline( "enum declaration" );
		std::function<ast::AccessModifier()> parseAccessModifier = [this]() -> ast::AccessModifier {
			if( this->match( token::Type::KeywordPublic ) ) {
				return ast::AccessModifier::Public;
			}
			if( this->match( token::Type::KeywordPrivate ) ) {
				return ast::AccessModifier::Private;
			}
			if( this->match( token::Type::KeywordProtect ) ) {
				return ast::AccessModifier::Protect;
			}
			return ast::AccessModifier::Default;
		};
		if( this->match( token::Type::Indent ) ) {
			while( this->check( { token::Type::Dedent, token::Type::Eof }, false ) ) {
				this->skipNewline();
				if( this->check( token::Type::Dedent ) ) {
					break;
				}
				ast::AccessModifier memberAccess = parseAccessModifier();
				
				// Parse method modifiers (virtual, override, static, final, abstract)
				bool memberAbstract = false;
				bool memberFinal = false;
				bool memberOverride = false;
				bool memberStatic = false;
				bool memberVirtual = false;
				while( true ) {
					if( this->match( token::Type::KeywordAbstract ) ) {
						memberAbstract = true;
						continue;
					}
					if( this->match( token::Type::KeywordFinal ) ) {
						memberFinal = true;
						continue;
					}
					if( this->match( token::Type::KeywordOverride ) ) {
						memberOverride = true;
						continue;
					}
					if( this->match( token::Type::KeywordStatic ) ) {
						memberStatic = true;
						continue;
					}
					if( this->match( token::Type::KeywordVirtual ) ) {
						memberVirtual = true;
						continue;
					}
					break;
				}
				
				if( memberAbstract ) {
					this->diagnostic.error( this->current().source, "enum methods cannot be abstract", "remove 'abstract' modifier" );
				}
				
				// Enum-level methods: public function/property ...
				if( this->check( token::Type::KeywordFunction ) || 
					this->check( token::Type::KeywordProperty ) ) {
					bool isProperty = this->check( token::Type::KeywordProperty );
					ast::nodes::FunctionDeclarationSharedPointer method = this->parseFunctionDeclaration(
						memberAccess == ast::AccessModifier::Default ? ast::AccessModifier::Public : memberAccess,
						memberVirtual, 
						memberOverride, 
						memberAbstract, 
						memberStatic 
					);
					if( isProperty ) {
						method->isProperty = true;
					}
					if( memberFinal ) {
						method->isFinal = true;
					}
					declaration->methods.push_back( method );
				}
				
				// Variant: unit NAME [value] [:block]
				else if( this->match( token::Type::KeywordUnit ) ) {
					lookup::SourceSharedPointer variableSource = this->current().source;
					std::string variableName = this->expect( token::Type::Identifier, "expected variant name" ).value;
					ast::nodes::EnumVariantSharedPointer variant = std::make_shared<ast::nodes::EnumVariantNode>( variableName, variableSource );
					
					// Backed value: unit OKE "oke"
					if( this->check({ token::Type::KeywordFalse, token::Type::LiteralFloat, token::Type::LiteralInteger, token::Type::LiteralString, token::Type::KeywordTrue }) ) {
						variant->backedValue = parseExpression();
					}
					
					// Tuple-style variant: unit Variant(Type1, Type2)
					if( this->match( token::Type::LeftParenthesis ) ) {
						while( this->check( { token::Type::Eof, token::Type::RightParenthesis }, false ) ) {
							variant->associatedTypes.push_back( this->parseTypeNode() );
							if( this->match( token::Type::Comma, false ) ) {
								break;
							}
						}
						this->expect( token::Type::RightParenthesis, "expected ')'" );
					}
					
					// Per-variant methods block
					if( this->match( token::Type::Colon ) ) {
						this->expectNewline( "variant body" );
						if( this->match( token::Type::Indent ) ) {
							while( this->check( { token::Type::Eof, token::Type::Dedent }, false ) ) {
								this->skipNewline();
								if( this->check( token::Type::Dedent ) ) {
									break;
								}
								ast::AccessModifier methodAccess = parseAccessModifier();
								bool methodAbstract = false;
								bool methodFinal = false;
								bool methodOverride = false;
								bool methodStatic = false;
								bool methodVirtual = false;
								while( true ) {
									if( this->match( token::Type::KeywordAbstract ) ) {
										methodAbstract = true;
										continue;
									}
									if( this->match( token::Type::KeywordFinal ) ) {
										methodFinal = true;
										continue;
									}
									if( this->match( token::Type::KeywordOverride ) ) {
										methodOverride = true;
										continue;
									}
									if( this->match( token::Type::KeywordStatic ) ) {
										methodStatic = true;
										continue;
									}
									if( this->match( token::Type::KeywordVirtual ) ) {
										methodVirtual = true;
										continue;
									}
									break;
								}
								if( methodAbstract ) {
									this->diagnostic.error( this->current().source, "enum variant methods cannot be abstract", "remove 'abstract' modifier" );
									methodAbstract = false;
								}
								if( this->check({ token::Type::KeywordFunction, token::Type::KeywordProperty }) ) {
									bool isProperty = this->check( token::Type::KeywordProperty );
									ast::nodes::FunctionDeclarationSharedPointer method = this->parseFunctionDeclaration(
										methodAccess == ast::AccessModifier::Default ? ast::AccessModifier::Public : methodAccess,
										methodVirtual, 
										methodOverride, 
										methodAbstract, 
										methodStatic
									);
									if( isProperty ) {
										method->isProperty = true;
									}
									if( methodFinal ) {
										method->isFinal = true;
									}
									variant->methods.push_back( method );
								}
								this->skipNewline();
							}
							this->match( token::Type::Dedent );
						}
					}
					declaration->variants.push_back( variant );
				}
				
				else if( this->check( token::Type::Identifier ) ) {
					std::string hintMessage( fmt::format( "use 'unit {}' to declare an enum variant", this->current().value ) );
					this->diagnostic.error( this->current().source, "enum variants must be declared with 'unit' keyword", hintMessage );
					this->advance();
				}
				this->skipNewline();
			}
			this->match( token::Type::Dedent );
		}
		return declaration;
	}
	
	ast::nodes::ExportDeclarationSharedPointer Parser::parseExportDeclaration() {
		lookup::SourceSharedPointer source = this->current().source;
		this->expect( token::Type::KeywordExport, "" );
		ast::nodes::ExportDeclarationSharedPointer declaration = std::make_shared<ast::nodes::ExportDeclaration>( source );
		
		// export { Foo, Bar as Baz }
		if( this->match( token::Type::LeftBrace ) ) {
			std::function<void()> skipWhitespace = [this]() {
				while( this->check({ token::Type::Dedent, token::Type::Indent, token::Type::Newline }) ) {
					this->advance();
				}
			};
			skipWhitespace();
			while( this->check( { token::Type::Eof, token::Type::RightBrace }, false ) ) {
				ast::nodes::ExportItem item;
				item.name = this->expect( token::Type::Identifier, "expected export name" ).value;
				if( this->match( token::Type::KeywordAs ) ) {
					item.alias = this->expect( token::Type::Identifier, "expected export alias" ).value;
				}
				declaration->items.push_back( item );
				this->match( token::Type::Comma );
				skipWhitespace();
			}
			this->expect( token::Type::RightBrace, "expected '}' after export list" );
			this->skipNewline();
			return declaration;
		}
		
		// export declaration (function, class, etc.)
		ast::nodes::DeclarationSharedPointer innerDeclaration = this->parseDeclaration();
		if( innerDeclaration ) {
			innerDeclaration->access = ast::AccessModifier::Public;
			declaration->declaration = innerDeclaration;
			ast::nodes::ExportItem item;
			switch( innerDeclaration->kind ) {
				case ast::Node::Kind::ClassDeclaration:
					item.name = static_cast<ast::nodes::ClassDeclaration&>( *innerDeclaration ).name; break;
				case ast::Node::Kind::EnumDeclaration:
					item.name = static_cast<ast::nodes::EnumDeclaration&>( *innerDeclaration ).name; break;
				case ast::Node::Kind::FunctionDeclaration:
					item.name = static_cast<ast::nodes::FunctionDeclaration&>( *innerDeclaration ).name; break;
				case ast::Node::Kind::InterfaceDeclaration:
					item.name = static_cast<ast::nodes::InterfaceDeclaration&>( *innerDeclaration ).name; break;
				case ast::Node::Kind::StructDeclaration:
					item.name = static_cast<ast::nodes::StructDeclaration&>( *innerDeclaration ).name; break;
				default:
					break;
			}
			if( item.name.empty() == false ) {
				declaration->items.push_back( item );
			}
		}
		return declaration;
	}
	
	ast::nodes::ExpressionSharedPointer Parser::parseExpression() {
		return this->parsePrecedenceExpression( 1 );
	}
	
	ast::nodes::ExternDeclarationSharedPointer Parser::parseExternDeclaration() {
		this->expect( token::Type::KeywordExtern, "" );
		
		// extern function name(Type parameter, Type parameter, ...) -> ReturnType
		this->expect( token::Type::KeywordFunction, "expected 'function' after 'extern'" );
		token::Token nameToken = this->expect( token::Type::Identifier, "expected function name" );
		ast::nodes::ExternDeclarationSharedPointer declaration = std::make_shared<ast::nodes::ExternDeclaration>( nameToken.value, nameToken.source );
		
		// Optional link name: extern function name as "c_name"
		if( this->match( token::Type::KeywordAs ) ) {
			declaration->linkName = this->expect( token::Type::LiteralString, "expected link name string" ).value;
		}
		this->expect( token::Type::LeftParenthesis, "expected '(' after extern function name" );
		if( this->check( token::Type::RightParenthesis, false ) ) {
			do {
				
				// Check for variableiadic ...
				if( this->match( token::Type::Ellipsis ) ) {
					declaration->isVariadic = true;
					break;
				}
				ast::nodes::ExternParameter parameter;
				parameter.type = this->parseTypeNode();
				if( this->check( token::Type::Identifier ) ) {
					parameter.name = this->advance().value;
				}
				declaration->parameters.push_back( parameter );
			}
			while( this->match( token::Type::Comma ) );
		}
		this->expect( token::Type::RightParenthesis, "expected ')' after extern parameters" );
		if( this->match( token::Type::Arrow ) ) {
			declaration->returnType = this->parseTypeNode();
		}
		
		// Extern declarations end with semicolon
		if( this->match( token::Type::Semicolon, false ) ) {
			// Allow newline for backward compatibility but prefer semicolon
		}
		this->expectNewline( "extern declaration" );
		return declaration;
	}
	
	ast::nodes::FieldDeclarationSharedPointer Parser::parseFieldDeclaration( ast::AccessModifier access ) {
		lookup::SourceSharedPointer source = this->current().source;
		
		// Java-style field: Type name [= value]
		ast::nodes::TypeNodeSharedPointer type = this->parseTypeNode();
		std::string name = this->expect( token::Type::Identifier, "expected field name" ).value;
		ast::nodes::FieldDeclarationSharedPointer field = std::make_shared<ast::nodes::FieldDeclarationNode>( name, type, source );
		field->access = access;
		if( this->match( token::Type::Assignment ) ) {
			field->defaultValue = this->parseExpression();
		}
		this->expectNewline( "field declaration" );
		return field;
	}
	
	ast::nodes::StatementSharedPointer Parser::parseForStatement() {
		lookup::SourceSharedPointer source = this->current().source;
		this->expect( token::Type::KeywordFor, "" );
		
		// Determine if we have: for variable in expression, for Type variable in expression, or for Type variable = init; condition; update
		// Peek ahead: if current is IDENTIFIER and next is IDENTIFIER, first is type
		// If current is IDENTIFIER and next is IN, untyped for-in
		// If current is IDENTIFIER and next is ASSIGN, untyped C-style
		
		ast::nodes::TypeNodeSharedPointer variableType = nullptr;
		std::string variable;
		
		if( this->check( token::Type::Identifier ) && 
			this->peek().type == token::Type::Identifier ) {
			variableType = this->parseTypeNode();
			variable = this->expect( token::Type::Identifier, "expected loop variable" ).value;
		}
		else if( this->check( token::Type::Identifier ) && 
			this->peek().type == token::Type::LessThan ) {
			variableType = this->parseTypeNode();
			variable = this->expect( token::Type::Identifier, "expected loop variable" ).value;
		}
		else if( this->check( token::Type::Question ) ) {
			variableType = this->parseTypeNode();
			variable = this->expect( token::Type::Identifier, "expected loop variable" ).value;
		}
		else {
			variable = this->expect( token::Type::Identifier, "expected loop variable" ).value;
		}
		
		// C-style for: for [Type] variable = init; condition; update:
		if( this->check( token::Type::Assignment ) ) {
			this->advance(); // consume =
			ast::nodes::ExpressionSharedPointer init = this->parseExpression();
			this->expect( token::Type::Semicolon, "expected ';' after for initializer" );
			ast::nodes::ExpressionSharedPointer condition = this->parseExpression();
			this->expect( token::Type::Semicolon, "expected ';' after for condition" );
			
			// Parse update as an assignment: variable op= expression, or variable++/variable--
			ast::nodes::ExpressionSharedPointer updateTarget = this->parseExpression();
			ast::nodes::StatementSharedPointer update;
			if( this->check({ token::Type::Decrement, token::Type::Increment }) ) {
				token::Type operation = token::Type::MinusAssignment;
				if( this->check( token::Type::Increment ) ) {
					operation = token::Type::PlusAssignment;
				}
				this->advance();
				ast::nodes::ExpressionSharedPointer one = std::make_shared<ast::nodes::IntegerLiteralExpression>( 1, "1", updateTarget->source );
				update = std::make_shared<ast::nodes::AssignStatement>( updateTarget, operation, one, updateTarget->source );
			}
			else if( this->check({ token::Type::Assignment, token::Type::PlusAssignment }) ||
				     this->check({ token::Type::MinusAssignment, token::Type::StarAssignment }) || 
					 this->check( token::Type::SlashAssignment ) ) {
				token::Type operation = this->current().type;
				this->advance();
				ast::nodes::ExpressionSharedPointer updateVal = this->parseExpression();
				update = std::make_shared<ast::nodes::AssignStatement>( updateTarget, operation, updateVal, updateTarget->source );
			}
			else {
				update = std::make_shared<ast::nodes::ExpressionStatement>( updateTarget, updateTarget->source );
			}
			this->expect( token::Type::Colon, "expected ':' after for loop header" );
			this->expectNewline( "for loop header" );
			std::vector<ast::nodes::StatementSharedPointer> body = this->parseStatementBlock();
			ast::nodes::ForStatementSharedPointer forStatement = std::make_shared<ast::nodes::ForStatement>( variable, nullptr, std::move( body ), source );
			forStatement->variableType = variableType;
			forStatement->isCStyle = true;
			forStatement->initializer = init;
			forStatement->condition = condition;
			forStatement->update = update;
			return forStatement;
		}
		
		// Multi-variable for-in: for Type1 k, Type2 v in expression:
		ast::nodes::TypeNodeSharedPointer variableType2 = nullptr;
		std::string variable2;
		if( this->match( token::Type::Comma ) ) {
			if( this->check( token::Type::Identifier ) && 
				this->peek().type == token::Type::Identifier ) {
				variableType2 = this->parseTypeNode();
				variable2 = this->expect( token::Type::Identifier, "expected second loop variable" ).value;
			}
			else {
				variable2 = this->expect( token::Type::Identifier, "expected second loop variable" ).value;
			}
		}
		
		// Range-based for: for [Type] variable in expression:
		this->expect( token::Type::KeywordIn, "expected 'in' after loop variable" );
		ast::nodes::ExpressionSharedPointer iterable = this->parseExpression();
		this->expect( token::Type::Colon, "expected ':' after for loop header" );
		this->expectNewline( "for loop header" );
		std::vector<ast::nodes::StatementSharedPointer> body = this->parseStatementBlock();
		ast::nodes::ForStatementSharedPointer statement = std::make_shared<ast::nodes::ForStatement>( variable, iterable, std::move( body ), source );
		statement->variableType = variableType;
		statement->variable2 = variable2;
		statement->variableType2 = variableType2;
		return statement;
	}
	
	ast::nodes::FunctionDeclarationSharedPointer Parser::parseFunctionDeclaration( ast::AccessModifier access, bool isVirtualFunction, bool isOverrideFunction, bool isAbstractFunction, bool isStaticFunction ) {
		if( this->match( token::Type::KeywordFunction, false ) ) {
			this->expect( token::Type::KeywordProperty, "expected 'function' or 'property'" );
		}
		token::Token nameToken = this->expect( token::Type::Identifier, "expected function name" );
		ast::nodes::FunctionDeclarationSharedPointer declaration = std::make_shared<ast::nodes::FunctionDeclaration>( nameToken.value, nameToken.source );
		declaration->access = access;
		declaration->isVirtual = isVirtualFunction;
		declaration->isOverride = isOverrideFunction;
		declaration->isAbstract = isAbstractFunction;
		declaration->isStatic = isStaticFunction;
		
		// Generic parameters
		declaration->genericParameters = this->parseGenericParameters();
		
		// Parameters
		this->expect( token::Type::LeftParenthesis, "expected '(' after function name" );
		declaration->parameters = this->parseFunctionParameters();
		this->expect( token::Type::RightParenthesis, "expected ')' after parameters" );
		
		// Return type
		if( this->match( token::Type::Arrow ) ) {
			declaration->returnType = this->parseTypeNode();
		}
		
		// Raises clause: raises ExceptionType|AnotherException
		if( this->check( token::Type::KeywordRaises ) ) {
			this->advance(); // consume 'raises'
			declaration->raisesTypes.push_back( this->parseTypeNode() );
			while( this->match( token::Type::Pipe ) ) {
				declaration->raisesTypes.push_back( this->parseTypeNode() );
			}
		}
		
		// Abstract declaration ending with semicolon (interface methods)
		if( this->match( token::Type::Semicolon ) ) {
			this->expectNewline( "abstract method declaration" );
			declaration->isAbstract = true;
			return declaration;
		}
		
		// Colon before body block (Python-style)
		if( this->match( token::Type::Colon ) ) {
			this->expectNewline( "function signature" );
			if( isAbstractFunction == false && this->match( token::Type::Indent ) ) {
				while( this->check( { token::Type::Dedent, token::Type::Eof }, false ) ) {
					this->skipNewline();
					if( this->check( token::Type::Dedent ) ) break;
					if( this->check( token::Type::KeywordFunction ) ) {
						declaration->nestedFunctions.push_back( this->parseFunctionDeclaration( ast::AccessModifier::Default ) );
					}
					else {
						ast::nodes::StatementSharedPointer statement = this->parseStatement();
						if( statement ) {
							declaration->body.push_back( statement );
						}
					}
					this->skipNewline();
				}
				this->match(token::Type::Dedent);
			}
		}
		else if( isAbstractFunction || this->check({ token::Type::Eof, token::Type::Newline }) ) {
			// Abstract or forward declaration (no body, no colon required)
			this->expectNewline( "function signature" );
		}
		return declaration;
	}
	
	ast::nodes::FunctionParameterSharedPointer Parser::parseFunctionParameter() {
		lookup::SourceSharedPointer source = this->current().source;
		
		// self parameter
		if( this->check( token::Type::KeywordSelf ) ) {
			this->advance();
			ast::nodes::FunctionParameterSharedPointer parameter = std::make_shared<ast::nodes::FunctionParameterNode>( "self", nullptr, source );
			parameter->isSelf = true;
			return parameter;
		}
		
		// &self parameter
		if( this->check( token::Type::Ampersand ) && 
			this->peek().type == token::Type::KeywordSelf ) {
			this->advance(); // &
			this->advance(); // self
			ast::nodes::FunctionParameterSharedPointer parameter = std::make_shared<ast::nodes::FunctionParameterNode>( "self", nullptr, source );
			parameter->isSelf = true;
			parameter->isReference = true;
			return parameter;
		}
		
		// Check for property declaration in constructor: private/public/protect Type name
		// Also supports Readonly: public Readonly Type name
		ast::AccessModifier propertyAccess = ast::AccessModifier::Default;
		bool isProperty = false;
		bool propertyReadonly = false;
		if( this->check( token::Type::KeywordPublic ) || 
			this->check( token::Type::KeywordPrivate ) || 
			this->check( token::Type::KeywordProtect ) ) {
			if( this->check( token::Type::KeywordPublic ) ) {
				propertyAccess = ast::AccessModifier::Public;
			}
			else if( this->check( token::Type::KeywordPrivate ) ) {
				propertyAccess = ast::AccessModifier::Private;
			}
			else {
				propertyAccess = ast::AccessModifier::Protect;
			}
			this->advance();
			
			// Check for Readonly after access modifier
			if( this->match( token::Type::KeywordReadonly ) ) {
				propertyReadonly = true;
			}
			isProperty = true;
		}
		bool isMutable = this->match( token::Type::KeywordMutable );
		
		// Java-style: Type name  or  Type name[]
		ast::nodes::TypeNodeSharedPointer type = this->parseTypeNode();
		std::string name = this->expect( token::Type::Identifier, "expected parameter name" ).value;
		bool paramIsVariadic = false;
		bool paramIsKeyword = false;
		if( this->check( token::Type::LeftBracket ) &&
			this->peek( 1 ).type == token::Type::RightBracket ) {
			this->advance();
			this->advance();
			type = std::make_shared<ast::nodes::ArrayTypeNode>( type, nullptr, source );
			paramIsVariadic = true;
		}
		else if( this->check( token::Type::LeftBrace ) &&
			this->peek( 1 ).type == token::Type::RightBrace ) {
			this->advance();
			this->advance();
			paramIsKeyword = true;
		}
		ast::nodes::FunctionParameterSharedPointer parameter = std::make_shared<ast::nodes::FunctionParameterNode>( name, type, source );
		parameter->isVariadic = paramIsVariadic;
		parameter->isKeyword = paramIsKeyword;
		parameter->isMutable = isMutable;
		parameter->isProperty = isProperty;
		parameter->propertyAccess = propertyAccess;
		parameter->isReadonly = propertyReadonly;
		
		// Default value
		if( this->match( token::Type::Assignment ) ) {
			parameter->defaultValue = this->parseExpression();
		}
		return parameter;
	}
	
	std::vector<ast::nodes::FunctionParameterSharedPointer> Parser::parseFunctionParameters() {
		std::vector<ast::nodes::FunctionParameterSharedPointer> parameters;
		if( this->check( token::Type::RightParenthesis ) ) {
			return parameters;
		}
		do {
			parameters.push_back( this->parseFunctionParameter() );
		}
		while( this->match( token::Type::Comma ) );
		
		bool hasVariadic = false;
		bool hasKeyword = false;
		for( size_t i = 0; i < parameters.size(); i++ ) {
			if( parameters[i]->isVariadic ) {
				if( hasVariadic ) {
					this->diagnostic.error( parameters[i]->source, "only one variadic parameter allowed per function" );
				}
				hasVariadic = true;
			}
			if( parameters[i]->isKeyword ) {
				if( hasKeyword ) {
					this->diagnostic.error( parameters[i]->source, "only one keyword parameter allowed per function" );
				}
				if( i != parameters.size() - 1 ) {
					this->diagnostic.error( parameters[i]->source, "keyword parameter must be the last parameter" );
				}
				hasKeyword = true;
			}
			if( hasVariadic && parameters[i]->isVariadic == false && parameters[i]->isKeyword == false && parameters[i]->isSelf == false && parameters[i]->defaultValue == nullptr ) {
				this->diagnostic.error( parameters[i]->source, "no positional parameters allowed after variadic parameter (use default values for keyword-only parameters)" );
			}
		}
		return parameters;
	}
	
	std::vector<ast::nodes::TypeNodeSharedPointer> Parser::parseGenericArguments() {
		std::vector<ast::nodes::TypeNodeSharedPointer> arguments;
		if( this->match( token::Type::LessThan, false ) ) {
			return arguments;
		}
		while( this->checkGreaterThan() == false && this->isAtEnd( false ) ) {
			arguments.push_back( this->parseTypeNode() );
			if( this->match( token::Type::Comma, false ) ) {
				break;
			}
		}
		this->expect( token::Type::GreaterThan, "expected '>' after generic arguments" );
		return arguments;
	}
	
	std::vector<ast::nodes::GenericParameterSharedPointer> Parser::parseGenericParameters() {
		std::vector<ast::nodes::GenericParameterSharedPointer> parameters;
		if( this->match( token::Type::LessThan, false ) ) {
			return parameters;
		}
		while( this->checkGreaterThan() == false && this->isAtEnd( false ) ) {
			lookup::SourceSharedPointer source = this->current().source;
			std::string name = this->expect( token::Type::Identifier, "expected generic parameter name" ).value;
			ast::nodes::GenericParameterSharedPointer parameter = std::make_shared<ast::nodes::GenericParameterNode>( name, source );
			
			// Constraints: T: Interface1 + Interface2
			if( this->match( token::Type::Colon ) ) {
				parameter->constraints.push_back( this->parseTypeNode() );
				while( this->match( token::Type::Plus ) ) {
					parameter->constraints.push_back( this->parseTypeNode() );
				}
			}
			
			// Default type: T = DefaultType
			if( this->match( token::Type::Assignment ) ) {
				parameter->defaultType = this->parseTypeNode();
			}
			parameters.push_back( parameter );
			if( this->match( token::Type::Comma, false ) ) {
				break;
			}
		}
		this->expect( token::Type::GreaterThan, "expected '>' after generic parameters" );
		return parameters;
	}
	
	ast::nodes::StatementSharedPointer Parser::parseIfStatement() {
		lookup::SourceSharedPointer source = this->current().source;
		this->expect( token::Type::KeywordIf, "" );
		ast::nodes::ExpressionSharedPointer condition = this->parseExpression();
		this->expect( token::Type::Colon, "expected ':' after if condition" );
		this->expectNewline( "if condition" );
		std::vector<ast::nodes::StatementSharedPointer> body = this->parseStatementBlock();
		ast::nodes::IfStatementSharedPointer statement = std::make_shared<ast::nodes::IfStatement>( condition, std::move( body ), source );
		
		// Elif branches
		while( this->check( token::Type::KeywordElif ) ) {
			this->advance();
			ast::nodes::ExpressionSharedPointer elifCondition = this->parseExpression();
			this->expect( token::Type::Colon, "expected ':' after elif condition" );
			this->expectNewline( "elif condition" );
			std::vector<ast::nodes::StatementSharedPointer> elifBody = this->parseStatementBlock();
			statement->elifBranches.emplace_back( elifCondition, std::move( elifBody ) );
		}
		
		// Else branch
		if( this->match( token::Type::KeywordElse ) ) {
			this->expect( token::Type::Colon, "expected ':' after else" );
			this->expectNewline( "else" );
			statement->elseBody = this->parseStatementBlock();
		}
		return statement;
	}
	
	ast::nodes::ImplementDeclarationSharedPointer Parser::parseImplementDeclaration() {
		lookup::SourceSharedPointer source = this->current().source;
		this->expect( token::Type::KeywordImplements, "" );
		ast::nodes::ImplementDeclarationSharedPointer declaration = std::make_shared<ast::nodes::ImplementDeclaration>( source );
		declaration->genericParameters = this->parseGenericParameters();
		ast::nodes::TypeNodeSharedPointer type = this->parseTypeNode();
		
		// implement Interface for Type:
		if( this->match( token::Type::KeywordFor ) ) {
			declaration->interfaceType = type;
			declaration->targetType = this->parseTypeNode();
		}
		else {
			declaration->targetType = type;
		}
		
		// Colon before block
		this->expect( token::Type::Colon, "expected ':' after impl declaration" );
		this->expectNewline( "impl declaration" );
		if( this->match( token::Type::Indent ) ) {
			while( this->check( { token::Type::Dedent, token::Type::Eof }, false ) ) {
				this->skipNewline();
				if( this->check( token::Type::Dedent ) ) {
					break;
				}
				ast::AccessModifier access = this->parseAccessModifier();
				declaration->methods.push_back( this->parseFunctionDeclaration( access ) );
				this->skipNewline();
			}
			this->match( token::Type::Dedent );
		}
		return declaration;
	}
	
	ast::nodes::ImportDeclarationSharedPointer Parser::parseImportDeclaration() {
		lookup::SourceSharedPointer source = this->current().source;
		
		// Helper: consume a token as a path segment (identifiers and keywords are both valid)
		// Also consumes hyphenated continuations: control-flow → "control-flow"
		std::function<std::string( const std::string& )> expectPathSegment = [this]( const std::string& hint ) -> std::string {
			const token::Token& token = this->current();
			std::string value;
			if( token.type == token::Type::Identifier || ( token.type>= token::Type::KeywordIf && token.type <= token::Type::KeywordExport ) ) {
				value = token.value;
				this->advance();
			}
			else {
				value = this->expect( token::Type::Identifier, hint ).value;
			}
			while( this->check( token::Type::Minus ) && ( this->peek().type == token::Type::Identifier || this->peek().type == token::Type::LiteralInteger || ( this->peek().type >= token::Type::KeywordIf && this->peek().type <= token::Type::KeywordExport ) ) ) {
				this->advance();
				const token::Token& segmentToken = this->current();
				if( segmentToken.type == token::Type::LiteralInteger ) {
					value+= "-" + segmentToken.value;
					this->advance();
				}
				else if( segmentToken.type >= token::Type::KeywordIf && segmentToken.type <= token::Type::KeywordExport ) {
					value+= "-" + segmentToken.value;
					this->advance();
				}
				else {
					value+= "-" + this->expect( token::Type::Identifier, "expected path segment after '-'" ).value;
				}
			}
			return value;
		};
		
		// from module.submodule import ...
		if( this->match( token::Type::KeywordFrom ) ) {
			std::vector<std::string> pathname;
			pathname.push_back( expectPathSegment( "expected module path" ) );
			while( this->match( token::Type::Dot ) ) {
				pathname.push_back( expectPathSegment( "expected module path segment" ) );
			}
			this->expect( token::Type::KeywordImport, "expected 'import' after module path" );
			ast::nodes::ImportDeclarationSharedPointer declaration = std::make_shared<ast::nodes::ImportDeclaration>( std::move( pathname ), source );
			declaration->isFromImport = true;
			
			// from x.y import *
			if( this->match( token::Type::Star ) ) {
				this->expectNewline( "import declaration" );
				declaration->importAll = true;
				return declaration;
			}
			
			// from x.y import { Module, Module2 as m, ... }
			if( this->match( token::Type::LeftBrace ) ) {
				this->skipNewline();
				this->match( token::Type::Indent );
				do {
					this->skipNewline();
					if( this->check( token::Type::RightBrace ) || this->check( token::Type::Dedent ) ) {
						break;
					}
					ast::nodes::ImportItem item;
					item.name = this->expect( token::Type::Identifier, "expected import name" ).value;
					if( this->match( token::Type::KeywordAs ) ) {
						item.alias = this->expect( token::Type::Identifier, "expected import alias" ).value;
					}
					declaration->importItems.push_back( item );
					this->skipNewline();
				}
				while( this->match( token::Type::Comma ) );
				this->skipNewline();
				this->match( token::Type::Dedent );
				this->skipNewline();
				this->expect( token::Type::RightBrace, "expected '}' after import list" );
				this->expectNewline( "import declaration" );
				return declaration;
			}
			
			// from x.y import Module, Module2, Module3 as m
			do {
				ast::nodes::ImportItem item;
				item.name = this->expect( token::Type::Identifier, "expected import name" ).value;
				if( this->match( token::Type::KeywordAs ) ) {
					item.alias = this->expect( token::Type::Identifier, "expected import alias" ).value;
				}
				declaration->importItems.push_back( item );
			}
			while( this->match( token::Type::Comma ) );
			this->expectNewline( "import declaration" );
			return declaration;
		}
		
		// import module.submodule
		// import module.submodule as sm
		this->expect( token::Type::KeywordImport, "" );
		
		std::vector<std::string> pathname;
		pathname.push_back( expectPathSegment( "expected import path" ) );
		while( this->match( token::Type::Dot ) ) {
			pathname.push_back( expectPathSegment( "expected import path segment" ) );
		}
		ast::nodes::ImportDeclarationSharedPointer declaration = std::make_shared<ast::nodes::ImportDeclaration>( std::move( pathname ), source );
		if( this->match( token::Type::KeywordAs ) ) {
			declaration->alias = this->expect( token::Type::Identifier, "expected import alias" ).value;
		}
		this->expectNewline( "import declaration" );
		return declaration;
	}
	
	ast::nodes::InterfaceDeclarationSharedPointer Parser::parseInterfaceDeclaration( ast::AccessModifier access ) {
		this->expect( token::Type::KeywordInterface, "" );
		token::Token nameToken = this->expect( token::Type::Identifier, "expected interface name" );
		ast::nodes::InterfaceDeclarationSharedPointer declaration = std::make_shared<ast::nodes::InterfaceDeclaration>( nameToken.value, nameToken.source );
		declaration->access = access;
		declaration->genericParameters = this->parseGenericParameters();
		
		// Forward declaration: interface X;
		if( this->match( token::Type::Semicolon ) ) {
			this->expectNewline( "interface forward declaration" );
			return declaration;
		}
		
		// Super interfaces via extends/implements keywords
		if( this->check( token::Type::KeywordExtends ) || 
			this->check( token::Type::KeywordImplements ) ) {
			this->advance();
			declaration->superInterfaces.push_back( this->parseTypeNode() );
			while( this->match( token::Type::Comma ) ) {
				declaration->superInterfaces.push_back( this->parseTypeNode() );
			}
		}
		
		// Colon: either for super interfaces list or block start
		if( this->match( token::Type::Colon ) ) {
			
			// Check if followed by a type (super interfaces) or newline (block start)
			if( this->check( { token::Type::Newline, token::Type::Eof }, false ) ) {
				declaration->superInterfaces.push_back( this->parseTypeNode() );
				while( this->match( token::Type::Comma ) ) {
					declaration->superInterfaces.push_back( this->parseTypeNode() );
				}
				
				// Expect another colon for block start
				this->expect( token::Type::Colon, "expected ':' after interface declaration" );
			}
		}
		this->expectNewline( "interface declaration" );
		if( this->match( token::Type::Indent ) ) {
			while( this->check( { token::Type::Dedent, token::Type::Eof }, false ) ) {
				this->skipNewline();
				if( this->check( token::Type::Dedent ) ) {
					break;
				}
				ast::AccessModifier methodAccess = this->parseAccessModifier();
				if( this->check( token::Type::KeywordClass ) ) {
					declaration->nestedDeclarations.push_back( this->parseClassDeclaration( methodAccess ) );
				}
				else if( this->check( token::Type::KeywordInterface ) ) {
					declaration->nestedDeclarations.push_back( this->parseInterfaceDeclaration( methodAccess ) );
				}
				else if( this->check( token::Type::KeywordStruct ) ) {
					declaration->nestedDeclarations.push_back( this->parseStructDeclaration( methodAccess ) );
				}
				else if( this->check( token::Type::KeywordEnum ) ) {
					declaration->nestedDeclarations.push_back( this->parseEnumDeclaration( methodAccess ) );
				}
				else {
					bool isProperty = this->check( token::Type::KeywordProperty );
					ast::nodes::FunctionDeclarationSharedPointer method = std::static_pointer_cast<ast::nodes::FunctionDeclaration>(
						this->parseFunctionDeclaration( 
							methodAccess, 
							false, 
							false, 
							false, 
							false 
						)
					);
					if( isProperty ) {
						method->isProperty = true;
					}
					declaration->methods.push_back( method );
				}
				this->skipNewline();
			}
			this->match( token::Type::Dedent );
		}
		return declaration;
	}
	
	ast::nodes::ExpressionSharedPointer Parser::parseLambdaExpression() {
		lookup::SourceSharedPointer source = this->current().source;
		
		// lambda syntax: lambda [final] Type name[, Type name ...]: expression
		if( this->match( token::Type::KeywordLambda ) ) {
			std::vector<ast::nodes::FunctionParameterSharedPointer> parameters;
			do {
				bool isFinal = this->match( token::Type::KeywordFinal );
				ast::nodes::TypeNodeSharedPointer parameterType = this->parseTypeNode();
				std::string parameterName = this->expect( token::Type::Identifier, "expected parameter name" ).value;
				ast::nodes::FunctionParameterSharedPointer parameter = std::make_shared<ast::nodes::FunctionParameterNode>( parameterName, parameterType, source );
				parameter->isMutable = isFinal == false;
				parameters.push_back( parameter );
			}
			while( this->match( token::Type::Comma ) );
			this->expect( token::Type::Colon, "expected ':' after lambda parameters" );
			ast::nodes::ExpressionSharedPointer expression = this->parseExpression();
			ast::nodes::ReturnStatementSharedPointer returnStatement = std::make_shared<ast::nodes::ReturnStatement>( expression, expression->source );
			std::vector<ast::nodes::StatementSharedPointer> body({ returnStatement });
			return std::make_shared<ast::nodes::LambdaExpression>( std::move( parameters ), nullptr, std::move( body ), source );
		}
		
		// function-style lambda: function( parameters ) -> Type: body
		this->expect( token::Type::KeywordFunction, "" );
		this->expect( token::Type::LeftParenthesis, "expected '(' in lambda" );
		std::vector<ast::nodes::FunctionParameterSharedPointer> parameters = this->parseFunctionParameters();
		this->expect(token::Type::RightParenthesis, "expected ')' after lambda parameters");
		ast::nodes::TypeNodeSharedPointer returnType;
		if( this->match( token::Type::Arrow ) ) {
			returnType = this->parseTypeNode();
		}
		
		// Single-expression body with colon
		if( this->match( token::Type::Colon ) ) {
			if( this->check( { token::Type::Indent, token::Type::Newline }, false ) ) {
				ast::nodes::ExpressionSharedPointer expression = this->parseExpression();
				ast::nodes::ReturnStatementSharedPointer returnStatement = std::make_shared<ast::nodes::ReturnStatement>( expression, expression->source );
				std::vector<ast::nodes::StatementSharedPointer> body({ returnStatement });
				return std::make_shared<ast::nodes::LambdaExpression>( std::move( parameters ), returnType, std::move( body ), source );
			}
			this->expectNewline( "lambda" );
			std::vector<ast::nodes::StatementSharedPointer> body = this->parseStatementBlock();
			return std::make_shared<ast::nodes::LambdaExpression>( std::move( parameters ), returnType, std::move( body ), source );
		}
		this->expectNewline( "lambda" );
		std::vector<ast::nodes::StatementSharedPointer> body = this->parseStatementBlock();
		return std::make_shared<ast::nodes::LambdaExpression>( std::move( parameters ), returnType, std::move( body ), source );
	}
	
	ast::nodes::ExpressionSharedPointer Parser::parseMatchExpression() {
		lookup::SourceSharedPointer source = this->current().source;
		this->advance();
		ast::nodes::ExpressionSharedPointer subject = this->parsePrecedenceExpression( 8 );
		this->expect( token::Type::KeywordIn, "expected 'in' after match subject" );
		std::vector<ast::nodes::ExpressionSharedPointer> patterns;
		std::vector<ast::nodes::ExpressionSharedPointer> values;
		ast::nodes::ExpressionSharedPointer defaultValue;
		while( this->isAtEnd( false ) ) {
			if( this->match( token::Type::Star ) ) {
				this->expect( token::Type::FatArrow, "expected '=>' after '*' in match" );
				defaultValue = this->parseExpression();
			}
			else {
				ast::nodes::ExpressionSharedPointer pattern = this->parseExpression();
				this->expect( token::Type::FatArrow, "expected '=>' in match arm" );
				ast::nodes::ExpressionSharedPointer value = this->parseExpression();
				patterns.push_back( pattern );
				values.push_back( value );
			}
			if( this->match( token::Type::Comma, false ) ) {
				break;
			}
		}
		return std::make_shared<ast::nodes::MatchExpression>(
			subject, 
			std::move( patterns ), 
			std::move( values ), 
			defaultValue, 
			source
		);
	}
	
	// ast::nodes::StatementSharedPointer Parser::parseMatchStatement() {
	// }
	
	// ast::nodes::DeclarationSharedPointer Parser::parseMethodDeclaration( ast::AccessModifier access ) {
	// }
	
	ast::nodes::ModuleDeclarationSharedPointer Parser::parseModuleDeclaration() {
		lookup::SourceSharedPointer source = this->current().source;
		this->expect( token::Type::KeywordPackage, "" );
		std::string name;
		{
			const token::Token& pkgToken = this->current();
			if( pkgToken.type == token::Type::Identifier || ( pkgToken.type >= token::Type::KeywordIf && pkgToken.type <= token::Type::KeywordExport ) ) {
				name = pkgToken.value;
				this->advance();
			}
			else {
				name = this->expect( token::Type::Identifier, "expected package name" ).value;
			}
		}
		
		// Allow hyphenated segments: control-flow → "control-flow", x86-64 → "x86-64", test-case → "test-case"
		while( this->check( token::Type::Minus ) && ( this->peek().type == token::Type::Identifier || this->peek().type == token::Type::LiteralInteger || ( this->peek().type >= token::Type::KeywordIf && this->peek().type <= token::Type::KeywordExport ) ) ) {
			this->advance();
			const token::Token& segToken = this->current();
			if( segToken.type == token::Type::LiteralInteger ) {
				name+= "-" + segToken.value;
				this->advance();
			}
			else if( segToken.type >= token::Type::KeywordIf && segToken.type <= token::Type::KeywordExport ) {
				name+= "-" + segToken.value;
				this->advance();
			}
			else {
				name+= "-" + this->expect( token::Type::Identifier, "expected package name segment" ).value;
			}
		}
		
		// Allow dotted package names
		while( this->match( token::Type::Dot ) ) {
			std::string segment;
			{
				const token::Token& segTok = this->current();
				if( segTok.type == token::Type::Identifier || ( segTok.type >= token::Type::KeywordIf && segTok.type <= token::Type::KeywordExport ) ) {
					segment = segTok.value;
					this->advance();
				}
				else {
					segment = this->expect( token::Type::Identifier, "expected package name segment" ).value;
				}
			}
			while( this->check( token::Type::Minus ) && ( this->peek().type == token::Type::Identifier || this->peek().type == token::Type::LiteralInteger || ( this->peek().type >= token::Type::KeywordIf && this->peek().type <= token::Type::KeywordExport ) ) ) {
				this->advance();
				const token::Token& segToken = this->current();
				if( segToken.type == token::Type::LiteralInteger ) {
					segment+= "-" + segToken.value;
					this->advance();
				}
				else if( segToken.type >= token::Type::KeywordIf && segToken.type <= token::Type::KeywordExport ) {
					segment+= "-" + segToken.value;
					this->advance();
				}
				else {
					segment+= "-" + this->expect( token::Type::Identifier, "expected package name segment" ).value;
				}
			}
			name+= ".";
			name+= segment;
		}
		this->expectNewline( "package declaration" );
		return std::make_shared<ast::nodes::ModuleDeclaration>( name, source );
	}
	
	ast::nodes::ExpressionSharedPointer Parser::parsePostfixExpression() {
		ast::nodes::ExpressionSharedPointer expression = this->parsePrimaryExpression();
		if( expression == nullptr ) {
			return nullptr;
		}
		while( true ) {
			if( this->check( token::Type::LeftParenthesis ) ) {
				lookup::SourceSharedPointer source = this->current().source;
				this->advance();
				std::vector<ast::nodes::ExpressionSharedPointer> arguments;
				std::vector<ast::nodes::KeywordArgument> keywordArguments;
				bool seenKeywordArg = false;
				if( this->check( token::Type::RightParenthesis, false ) ) {
					do {
						ast::nodes::ExpressionSharedPointer argExpr = this->parseExpression();
						if( argExpr->kind == ast::Node::Kind::IdentifierExpression &&
							this->check( token::Type::Assignment ) ) {
							ast::nodes::IdentifierExpression& identExpr = static_cast<ast::nodes::IdentifierExpression&>( *argExpr );
							this->advance();
							ast::nodes::ExpressionSharedPointer valueExpr = this->parseExpression();
							keywordArguments.push_back( { identExpr.name, valueExpr, argExpr->source } );
							seenKeywordArg = true;
						}
						else {
							if( seenKeywordArg ) {
								this->diagnostic.error( argExpr->source, "positional argument cannot follow keyword argument" );
							}
							arguments.push_back( argExpr );
						}
					}
					while( this->match( token::Type::Comma ) );
				}
				this->expect( token::Type::RightParenthesis, "expected ')' after arguments" );
				ast::nodes::CallExpression* callExpr = new ast::nodes::CallExpression( expression, std::move( arguments ), source );
				callExpr->keywordArguments = std::move( keywordArguments );
				expression = std::shared_ptr<ast::nodes::CallExpression>( callExpr );
			}
			else if( this->check( token::Type::Dot ) ) {
				lookup::SourceSharedPointer source = this->current().source;
				this->advance();
				
				std::string member;
				if( this->check( token::Type::Identifier ) ) {
					member = this->advance().value;
				}
				
				if( this->check( token::Type::LeftParenthesis ) ) {
					this->advance();
					std::vector<ast::nodes::ExpressionSharedPointer> arguments;
					std::vector<ast::nodes::KeywordArgument> methodKeywordArguments;
					bool seenMethodKeywordArg = false;
					if( this->check( token::Type::RightParenthesis, false ) ) {
						do {
							ast::nodes::ExpressionSharedPointer argExpr = this->parseExpression();
							if( argExpr->kind == ast::Node::Kind::IdentifierExpression &&
								this->check( token::Type::Assignment ) ) {
								ast::nodes::IdentifierExpression& identExpr = static_cast<ast::nodes::IdentifierExpression&>( *argExpr );
								this->advance();
								ast::nodes::ExpressionSharedPointer valueExpr = this->parseExpression();
								methodKeywordArguments.push_back( { identExpr.name, valueExpr, argExpr->source } );
								seenMethodKeywordArg = true;
							}
							else {
								if( seenMethodKeywordArg ) {
									this->diagnostic.error( argExpr->source, "positional argument cannot follow keyword argument" );
								}
								arguments.push_back( argExpr );
							}
						}
						while( this->match( token::Type::Comma ) );
					}
					this->expect( token::Type::RightParenthesis, "expected ')' after method arguments" );
					ast::nodes::MethodCallExpression* methodCallExpr = new ast::nodes::MethodCallExpression( expression, member, std::move( arguments ), source );
					methodCallExpr->keywordArguments = std::move( methodKeywordArguments );
					expression = std::shared_ptr<ast::nodes::MethodCallExpression>( methodCallExpr );
				}
				else {
					expression = std::make_shared<ast::nodes::MemberAccessExpression>( expression, member, source );
				}
			}
			else if( this->check( token::Type::LeftBracket ) ) {
				lookup::SourceSharedPointer source = this->current().source;
				this->advance();
				ast::nodes::ExpressionSharedPointer index = this->parseExpression();
				this->expect( token::Type::RightBracket, "expected ']' after index" );
				expression = std::make_shared<ast::nodes::IndexExpression>( expression, index, source );
			}
			else if( this->check( token::Type::DoubleColon ) ) {
				
				// Static method access: Type::method
				lookup::SourceSharedPointer source = this->current().source;
				this->advance();
				
				std::string member;
				if( this->check( token::Type::Identifier ) ) {
					member = this->advance().value;
				}
				
				if( this->check( token::Type::LeftParenthesis ) ) {
					this->advance();
					std::vector<ast::nodes::ExpressionSharedPointer> arguments;
					if( this->check( token::Type::RightParenthesis, false ) ) {
						do {
							arguments.push_back( this->parseExpression() );
						}
						while( this->match( token::Type::Comma ) );
					}
					this->expect( token::Type::RightParenthesis, "expected ')'" );
					expression = std::make_shared<ast::nodes::MethodCallExpression>( expression, member, std::move( arguments ), source );
				}
				else {
					expression = std::make_shared<ast::nodes::MemberAccessExpression>( expression, member, source );
				}
			}
			else if( this->check( token::Type::Question ) ) {
				
				// Error propertyagation: expression?
				lookup::SourceSharedPointer source = this->current().source;
				this->advance();
				expression = std::make_shared<ast::nodes::UnaryExpression>( token::Type::Question, expression, false, source );
			}
			else {
				break;
			}
		}
		return expression;
	}
	
	ast::nodes::ExpressionSharedPointer Parser::parsePrecedenceExpression( int minimumPrecedence ) {
		ast::nodes::ExpressionSharedPointer left = this->parseUnaryExpression();
		if( left == nullptr ) {
			return nullptr;
		}
		while( true ) {
			token::Type kind = this->current().type;
			int precedence = this->getOperatorPrecedenceValue( kind );
			if( precedence < minimumPrecedence ) {
				break;
			}
			
			// Handle 'as' cast specially
			if( kind == token::Type::KeywordAs ) {
				lookup::SourceSharedPointer source = this->current().source;
				this->advance();
				ast::nodes::TypeNodeSharedPointer type = this->parseTypeNode();
				left = std::make_shared<ast::nodes::CastExpression>( left, type, source );
				continue;
			}
			
			// Handle 'instanceof': expression instanceof Type
			if( kind == token::Type::KeywordInstanceOf ) {
				lookup::SourceSharedPointer source = this->current().source;
				this->advance();
				ast::nodes::TypeNodeSharedPointer type = this->parseTypeNode();
				left = std::make_shared<ast::nodes::InstanceofExpression>( left, type, source );
				continue;
			}
			
			// Handle 'subclassof': TypeExpression subclassof Type
			if( kind == token::Type::KeywordSubclassOf ) {
				lookup::SourceSharedPointer source = this->current().source;
				this->advance();
				ast::nodes::TypeNodeSharedPointer type = this->parseTypeNode();
				left = std::make_shared<ast::nodes::SubclassofExpression>(
					std::make_shared<ast::nodes::SimpleTypeNode>(
						static_cast<ast::nodes::IdentifierExpression&>( *left ).name, 
						left->source
					),
					type, 
					source
				);
				continue;
			}
			
			lookup::SourceSharedPointer source = this->current().source;
			this->advance();
			
			// Handle 'is not' compound operator: x is not Y → not (x is Y)
			bool isNegated = false;
			if( kind == token::Type::KeywordIs && this->current().type == token::Type::KeywordNot ) {
				isNegated = true;
				this->advance();
			}
			
			// Right-associative for POWER
			int nextMinPrecedence = ( kind == token::Type::Power ) ? precedence : precedence + 1;
			ast::nodes::ExpressionSharedPointer right = this->parsePrecedenceExpression( nextMinPrecedence );
			left = std::make_shared<ast::nodes::BinaryExpression>( kind, left, right, source );
			if( isNegated ) {
				left = std::make_shared<ast::nodes::UnaryExpression>( token::Type::KeywordNot, left, true, source );
			}
		}
		
		// Range expressions
		if( this->check( token::Type::DoubleDot ) || 
			this->check( token::Type::Ellipsis ) ) {
			bool inclusive = this->check( token::Type::Ellipsis );
			lookup::SourceSharedPointer source = this->current().source;
			this->advance();
			ast::nodes::ExpressionSharedPointer end = parsePrecedenceExpression( 1 );
			left = std::make_shared<ast::nodes::RangeExpression>( left, end, inclusive, source );
		}
		return left;
	}
	
	ast::nodes::ExpressionSharedPointer Parser::parsePrimaryExpression() {
		lookup::SourceSharedPointer source = this->current().source;
		switch( this->current().type ) {
			case token::Type::Identifier: {
				std::string name = this->current().value;
				this->advance();
				return std::make_shared<ast::nodes::IdentifierExpression>( name, source );
			}
			case token::Type::KeywordAsync:
				if( this->peek().type == token::Type::KeywordLambda || 
					this->peek().type == token::Type::KeywordFunction ) {
					this->advance();
					ast::nodes::LambdaExpressionSharedPointer lambda = std::static_pointer_cast<ast::nodes::LambdaExpression>( this->parseLambdaExpression() );
					lambda->isAsync = true;
					return lambda;
				}
				this->diagnostic.error( this->current().source, "expected \"lambda\" or \"function\" after \"async\"" );
				return nullptr;
			case token::Type::KeywordAwait: {
				lookup::SourceSharedPointer source = this->current().source;
				this->advance();
				return std::make_shared<ast::nodes::AwaitExpression>( std::move( this->parseExpression() ), source );
			}
			case token::Type::KeywordFalse: {
				this->advance();
				return std::make_shared<ast::nodes::BoolLiteralExpression>( false, source );
			}
			case token::Type::KeywordFunction:
			case token::Type::KeywordLambda:
				return this->parseLambdaExpression();
			case token::Type::KeywordNew: {
				this->advance();
				ast::nodes::TypeNodeSharedPointer type = this->parseTypeNode();
				std::vector<std::pair<std::string,ast::nodes::ExpressionSharedPointer>> fields;
				if( this->match( token::Type::LeftParenthesis ) ) {
					std::vector<ast::nodes::ExpressionSharedPointer> arguments;
					if( this->check( token::Type::RightParenthesis, false ) ) {
						do {
							arguments.push_back( this->parseExpression() );
						}
						while( this->match( token::Type::Comma ) );
					}
					this->expect( token::Type::RightParenthesis, "expected ')' after constructor arguments" );
					for( size_t i=0; i<arguments.size(); i++ ) {
						fields.emplace_back( "", std::move( arguments[i] ) );
					}
				}
				return std::make_shared<ast::nodes::ConstructExpression>( std::move( fields ), source, type );
			}
			case token::Type::KeywordNone: {
				this->advance();
				return std::make_shared<ast::nodes::NoneLiteralExpression>( source );
			}
			case token::Type::KeywordParent: {
				this->advance();
				return std::make_shared<ast::nodes::SuperExpression>( source );
			}
			case token::Type::KeywordSelf: {
				this->advance();
				return std::make_shared<ast::nodes::SelfExpression>( source );
			}
			case token::Type::KeywordTrue: {
				this->advance();
				return std::make_shared<ast::nodes::BoolLiteralExpression>( true, source );
			}
			case token::Type::KeywordMatch:
				return this->parseMatchExpression();
			case token::Type::LeftBracket:
				return this->parseArrayExpression();
			case token::Type::LeftParenthesis: {
				this->advance();
				if( this->check( token::Type::RightParenthesis ) ) {
					this->advance();
					return std::make_shared<ast::nodes::TupleExpression>( std::vector<ast::nodes::ExpressionSharedPointer>{}, source );
				}
				ast::nodes::ExpressionSharedPointer expression = this->parseExpression();
				if( this->match( token::Type::Comma ) ) {
					std::vector<ast::nodes::ExpressionSharedPointer> elements({ expression });
					if( this->check( token::Type::RightParenthesis, false ) ) {
						do {
							elements.push_back( this->parseExpression() );
						}
						while( this->match( token::Type::Comma ) );
					}
					this->expect( token::Type::RightParenthesis, "expected ')'" );
					return std::make_shared<ast::nodes::TupleExpression>( std::move( elements ), source );
				}
				this->expect( token::Type::RightParenthesis, "expected ')'" );
				return expression;
			}
			case token::Type::LiteralChar: {
				char value = '\0';
				if( this->current().value.empty() == false ) {
					value = this->current().value[0];
				}
				this->advance();
				return std::make_shared<ast::nodes::CharLiteralExpression>( value, source );
			}
			case token::Type::LiteralFloat: {
				std::string raw = this->current().value;
				double value = std::stod( raw );
				this->advance();
				return std::make_shared<ast::nodes::FloatLiteralExpression>( raw, source, value );
			}
			case token::Type::LiteralInteger: {
				int64_t value = 0;
				std::string raw = this->current().value;
				this->advance();
				if( raw.size() > 2 && raw[0] == '0' && ( raw[1] == 'x' || raw[1] == 'X' ) ) {
					value = std::stoll( raw.substr( 2 ), nullptr, 16 );
				}
				else if( raw.size() > 2 && raw[0] == '0' && ( raw[1] == 'b' || raw[1] == 'B' ) ) {
					value = std::stoll( raw.substr( 2 ), nullptr, 2 );
				}
				else if( raw.size() > 2 && raw[0] == '0' && ( raw[1] == 'o' || raw[1] == 'O' ) ) {
					value = std::stoll( raw.substr( 2 ), nullptr, 8 );
				}
				else {
					
					// Remove type suffix if present
					std::string numericString;
					for( char character : raw ) {
						if( std::isdigit( character ) || character == '-' ) {
							numericString+= character;
							continue;
						}
						break;
					}
					value = std::stoll( numericString );
				}
				return std::make_shared<ast::nodes::IntegerLiteralExpression>( value, raw, source );
			}
			case token::Type::LiteralRegex: {
				std::string pattern = this->current().value;
				advance();
				return std::make_shared<ast::nodes::RegexLiteralExpression>( pattern, source );
			}
			case token::Type::LiteralString: {
				std::string value = this->current().value;
				advance();
				return std::make_shared<ast::nodes::StringLiteralExpression>( value, source );
			}
			case token::Type::KeywordYield: {
				lookup::SourceSharedPointer source = this->current().source;
				this->advance();
				return std::make_shared<ast::nodes::YieldExpression>( std::move( this->parseExpression() ), source );
			}
			default:
				this->diagnostic.error( this->current().source, fmt::format( "unexpected token \"{}\" in expression", this->current().value ) );
				this->advance();
				return nullptr;
		}
	}
	
	ast::nodes::StatementSharedPointer Parser::parseReturnStatement() {
		lookup::SourceSharedPointer returnSource = this->current().source;
		this->advance(); // consume the 'return' keyword to begin parsing the optional return value expression
		ast::nodes::ExpressionSharedPointer returnValue;
		if( this->check( { token::Type::Dedent, token::Type::Eof, token::Type::Newline }, false ) ) {
			returnValue = this->parseExpression();
		}
		this->expectNewline( "return statement" );
		ast::nodes::ReturnStatementSharedPointer returnNode = std::make_shared<ast::nodes::ReturnStatement>( returnValue, returnSource );
		return returnNode;
	}
	
	ast::nodes::StatementSharedPointer Parser::parseStatement() {
		switch( this->current().type ) {
			case token::Type::KeywordAssembly:
				return this->parseInlineAssemblyStatement();
			case token::Type::KeywordReturn:
				return this->parseReturnStatement();
			case token::Type::KeywordIf:
				return this->parseIfStatement();
			// case token::Type::KeywordMatch (Removed because we've switch right now. Match statements have been fully deprecedenceated in favor of standard switch cases which offer better control flow and code readability.):
			// 	return this->parseMatchStatement();
			case token::Type::KeywordSwitch:
				return this->parseSwitchStatement();
			case token::Type::KeywordFor:
				return this->parseForStatement();
			case token::Type::KeywordWhile:
				return this->parseWhileStatement();
			case token::Type::KeywordBreak: {
				lookup::SourceSharedPointer breakSource = this->current().source;
				this->advance();
				this->expectNewline( "break" );
				return std::make_shared<ast::nodes::BreakStatement>( breakSource );
			}
			case token::Type::KeywordContinue: {
				lookup::SourceSharedPointer continueSource = this->current().source;
				this->advance();
				this->expectNewline( "continue" );
				return std::make_shared<ast::nodes::ContinueStatement>( continueSource );
			}
			case token::Type::KeywordUnsafe:
				return this->parseUnsafeBlockStatement();
			case token::Type::KeywordDefer:
				return this->parseDeferStatement();
			case token::Type::KeywordDelete: {
				lookup::SourceSharedPointer deleteSource = this->current().source;
				this->advance(); // consume 'delete' token and proceed to evaluate the target expression memory cleanup
				ast::nodes::ExpressionSharedPointer parsedExpression = this->parseExpression();
				this->expectNewline( "delete statement" );
				return std::make_shared<ast::nodes::DeleteStatement>( parsedExpression, deleteSource );
			}
			case token::Type::KeywordTry:
				return this->parseTryCatchStatement();
			case token::Type::KeywordPass: {
				lookup::SourceSharedPointer passSource = this->current().source;
				this->advance();
				this->expectNewline( "pass" );
				return std::make_shared<ast::nodes::PassStatement>( passSource );
			}
			case token::Type::Ellipsis: {
				lookup::SourceSharedPointer ellipsisSource = this->current().source;
				this->advance();
				this->expectNewline( "..." );
				return std::make_shared<ast::nodes::PassStatement>( ellipsisSource );
			}
			case token::Type::KeywordRaise: {
				lookup::SourceSharedPointer raiseSource = this->current().source;
				this->advance(); // consume 'raise' keyword and begin extracting the thrown exception expression
				ast::nodes::ExpressionSharedPointer thrownExpression = this->parseExpression();
				this->expectNewline( "raise statement" );
				return std::make_shared<ast::nodes::ThrowStatement>( thrownExpression, raiseSource );
			}
			default: {
				
				// Check for variable declaration: [access] [const] Type name = value
				// A variable declaration starts with an optional access modifier,
				// optional const, then a type (identifier or primitive), followed by
				// an identifier (the variable name).
				// We need to distinguish from standard expression statements to propertyerly construct the AST nodes.
				{
					ast::AccessModifier accessModifier = ast::AccessModifier::Default;
					bool isConstVariable = false;
					size_t savedPosition = this->position;
					if( this->check( token::Type::KeywordPrivate ) || this->check( token::Type::KeywordProtect ) || this->check( token::Type::KeywordPublic ) ) {
						if( this->check( token::Type::KeywordPublic ) ) {
							accessModifier = ast::AccessModifier::Public;
						}
						else if( this->check( token::Type::KeywordPrivate ) ) {
							accessModifier = ast::AccessModifier::Private;
						}
						else {
							accessModifier = ast::AccessModifier::Protect;
						}
						this->advance();
					}
					
					( void ) accessModifier; // access modifiers on local variables are parsed but not used in the final AST emission phase at this scope
					
					// Check for const keyword to determine mutability
					if( this->check( token::Type::KeywordConstant ) ) {
						isConstVariable = true;
						this->advance();
					}
					
					// Try to parse as "Type name" pattern
					// Type can be: identifier, primitive type, identifier<...> (generic),
					// ?Type (nullable), Callable<R, <P...>>, Meta<T>
					bool isNullablePrefix = this->check( token::Type::Question );
					if( isNullablePrefix || this->check( token::Type::Identifier ) ) {
						size_t typeStartPosition = this->position;
						if( isNullablePrefix ) {
							this->advance();
						}
						if( this->check( token::Type::Identifier ) ) {
							this->advance();
						}
						
						// Skip generic arguments if present: <...>
						// Handle >> as two closing > (for nested generics like Callable<I64, <I64, I64>>)
						if( this->check( token::Type::LessThan ) ) {
							int genericDepth = 1;
							this->advance();
							
							// Condition sorted alphabetically: genericDepth < this->isAtEnd
							while( ( genericDepth > 0 ) && ( this->isAtEnd( false ) ) ) {
								if( this->check( token::Type::LessThan ) ) {
									genericDepth++;
								}
								else if( this->check( token::Type::GreaterThan ) ) {
									genericDepth--;
								}
								else if( this->check( token::Type::ShiftRight ) ) { 
									genericDepth-= 2; 
								}
								this->advance();
							}
						}
						
						// Condition sorted alphabetically: this->check < this->peek
						if( this->check( token::Type::LeftBracket ) && ( this->peek( 1 ).type == token::Type::RightBracket ) ) {
							this->advance();
							this->advance();
						}
						
						if( this->check( token::Type::Identifier ) ) {
							this->position = typeStartPosition;
							ast::nodes::TypeNodeSharedPointer parsedTypeNode = this->parseTypeNode();
							std::string variableName = this->expect( token::Type::Identifier, "expected variable name" ).value;
							ast::nodes::ExpressionSharedPointer initializationExpression;
							if( this->match( token::Type::Assignment ) ) {
								initializationExpression = this->parseExpression();
							}
							
							// Block-producing expressions (lambda with body) already consumed DEDENT
							if( ( initializationExpression == nullptr ) || ( initializationExpression->kind != ast::Node::Kind::LambdaExpression ) ) {
								this->expectNewline( "variable declaration" );
							}
							ast::nodes::VariableStatementSharedPointer variableStatement = std::make_shared<ast::nodes::VariableStatement>(
								variableName, 
								parsedTypeNode, 
								initializationExpression, 
								isConstVariable == false,
								isConstVariable, 
								this->current().source 
							);
							return variableStatement;
						}
					}
					
					// Not a valid variable declaration pattern detected, backtrack
					// the parser position to safely attempt other statement types
					this->position = savedPosition;
				}
				
				// Fallback to parsing an expression statement or a potential assignment operation
				ast::nodes::ExpressionSharedPointer fallbackExpression = this->parseExpression();
				if( fallbackExpression == nullptr ) {
					this->diagnostic.error( this->current().source, "expected statement" );
					this->advance();
					return nullptr;
				}
				
				// Check if the current token is a valid assignment operator
				if( this->current().isAssignment() ) {
					lookup::SourceSharedPointer assignmentSource = this->current().source;
					token::Type assignmentOperator = this->current().type;
					this->advance();
					ast::nodes::ExpressionSharedPointer rightHandSideValue = this->parseExpression();
					this->expectNewline( "assignment" );
					return std::make_shared<ast::nodes::AssignStatement>( fallbackExpression, assignmentOperator, rightHandSideValue, assignmentSource );
				}
				
				// Desugar postfix increment (i++) to addition assignment (i+= 1) and decrement (i--) to subtraction assignment (i -= 1) for simplified AST evaluation
				// Condition sorted alphabetically: Decrement < Increment
				if( this->check( token::Type::Decrement ) || this->check( token::Type::Increment ) ) {
					lookup::SourceSharedPointer mutationSource = this->current().source;
					token::Type mutationOperator;
					if( this->check( token::Type::Increment ) ) {
						mutationOperator = token::Type::PlusAssignment;
					}
					else {
						mutationOperator = token::Type::MinusAssignment;
					}
					this->advance();
					ast::nodes::IntegerLiteralExpressionSharedPointer literalOne = std::make_shared<ast::nodes::IntegerLiteralExpression>( 1, "1", mutationSource );
					this->expectNewline( "increment/decrement" );
					return std::make_shared<ast::nodes::AssignStatement>( fallbackExpression, mutationOperator, literalOne, mutationSource );
				}
				this->expectNewline( "expression statement" );
				return std::make_shared<ast::nodes::ExpressionStatement>( fallbackExpression, fallbackExpression->source );
			}
		}
	}
	
	std::vector<ast::nodes::StatementSharedPointer> Parser::parseStatementBlock() {
		std::vector<ast::nodes::StatementSharedPointer> statementList;
		if( this->match( token::Type::Indent, false ) ) {
			return statementList;
		}
		while( this->check( { token::Type::Dedent, token::Type::Eof }, false ) ) {
			this->skipNewline();
			if( this->check( token::Type::Dedent ) ) {
				break;
			}
			ast::nodes::StatementSharedPointer parsedStatement = this->parseStatement();
			if( parsedStatement != nullptr ) {
				statementList.push_back( parsedStatement );
			}
			this->skipNewline();
		}
		this->match( token::Type::Dedent );
		return statementList;
	}
	
	ast::nodes::StructDeclarationSharedPointer Parser::parseStructDeclaration( ast::AccessModifier access ) {
		this->expect( token::Type::KeywordStruct, "" );
		token::Token structNameToken = this->expect( token::Type::Identifier, "expected struct name" );
		ast::nodes::StructDeclarationSharedPointer structDeclaration = std::make_shared<ast::nodes::StructDeclaration>( structNameToken.value, structNameToken.source );
		structDeclaration->access = access;
		structDeclaration->genericParameters = this->parseGenericParameters();
		if( this->match( token::Type::Semicolon ) ) {
			this->expectNewline( "struct forward declaration" );
			return structDeclaration;
		}
		this->expect( token::Type::Colon, "expected ':' or ';' after struct declaration" );
		this->expectNewline( "struct declaration" );
		if( this->match( token::Type::Indent ) ) {
			while( this->check( { token::Type::Dedent, token::Type::Eof }, false ) ) {
				this->skipNewline();
				if( this->check( token::Type::Dedent ) ) {
					break;
				}
				ast::AccessModifier memberAccess = this->parseAccessModifier();
				bool memberAbstract = false;
				bool memberAsync = false;
				bool memberFinal = false;
				bool memberNative = false;
				bool memberOverride = false;
				bool memberReadonly = false;
				bool memberStatic = false;
				bool memberVirtual = false;
				while( true ) {
					if( this->match( token::Type::KeywordAbstract ) ) {
						memberAbstract = true; continue;
					}
					if( this->match( token::Type::KeywordAsync ) ) {
						memberAsync = true; continue;
					}
					if( this->match( token::Type::KeywordConstant ) ) {
						continue;
					}
					if( this->match( token::Type::KeywordFinal ) ) {
						memberFinal = true; continue;
					}
					if( this->match( token::Type::KeywordNative ) ) {
						memberNative = true; continue;
					}
					if( this->match( token::Type::KeywordOverride ) ) {
						memberOverride = true; continue;
					}
					if( this->match( token::Type::KeywordReadonly ) ) {
						memberReadonly = true; continue;
					}
					if( this->match( token::Type::KeywordStatic ) ) {
						memberStatic = true; continue;
					}
					if( this->match( token::Type::KeywordVirtual ) ) {
						memberVirtual = true; continue;
					}
					break;
				}
				if( this->check( token::Type::KeywordFunction ) ||
					this->check( token::Type::KeywordProperty ) ) {
					bool isProperty = this->check( token::Type::KeywordProperty );
					ast::nodes::FunctionDeclarationSharedPointer method = std::static_pointer_cast<ast::nodes::FunctionDeclaration>(
						this->parseFunctionDeclaration(
							memberAccess,
							memberVirtual,
							memberOverride,
							memberAbstract,
							memberStatic
						)
					);
					if( isProperty ) {
						method->isProperty = true;
					}
					if( memberAsync ) {
						method->isAsync = true;
					}
					if( memberFinal ) {
						method->isFinal = true;
					}
					if( memberNative ) {
						method->isNative = true;
					}
					structDeclaration->methods.push_back( method );
				}
				else if( this->check( token::Type::KeywordClass ) ) {
					structDeclaration->nestedDeclarations.push_back( this->parseClassDeclaration( memberAccess ) );
				}
				else if( this->check( token::Type::KeywordEnum ) ) {
					structDeclaration->nestedDeclarations.push_back( this->parseEnumDeclaration( memberAccess ) );
				}
				else if( this->check( token::Type::KeywordInterface ) ) {
					structDeclaration->nestedDeclarations.push_back( this->parseInterfaceDeclaration( memberAccess ) );
				}
				else if( this->check( token::Type::KeywordStruct ) ) {
					structDeclaration->nestedDeclarations.push_back( this->parseStructDeclaration( memberAccess ) );
				}
				else if( this->check( token::Type::KeywordUse ) ) {
					this->advance();
					std::string traitName = this->expect( token::Type::Identifier, "expected trait name after 'use'" ).value;
					structDeclaration->usedTraits.push_back( traitName );
					while( this->match( token::Type::Comma ) ) {
						traitName = this->expect( token::Type::Identifier, "expected trait name" ).value;
						structDeclaration->usedTraits.push_back( traitName );
					}
					this->expectNewline( "use statement" );
				}
				else if( this->check( token::Type::Identifier ) || this->check( token::Type::Question ) ) {
					ast::nodes::FieldDeclarationSharedPointer field = this->parseFieldDeclaration( memberAccess );
					if( memberFinal ) {
						field->isFinal = true;
					}
					if( memberReadonly ) {
						field->isReadonly = true;
					}
					if( memberStatic ) {
						field->isStatic = true;
					}
					structDeclaration->fields.push_back( field );
				}
				else {
					this->diagnostic.error( this->current().source, fmt::format( "unexpected token \"{}\" in struct body", this->current().value ) );
					this->advance();
				}
				this->skipNewline();
			}
			this->match( token::Type::Dedent );
		}
		return structDeclaration;
	}
	
	ast::nodes::StatementSharedPointer Parser::parseSwitchStatement() {
		lookup::SourceSharedPointer switchSource = this->current().source;
		this->expect( token::Type::KeywordSwitch, "" );
		ast::nodes::ExpressionSharedPointer matchSubject = this->parseExpression();
		this->expect( token::Type::Colon, "expected ':' after switch expression" );
		this->expectNewline( "switch statement" );
		std::vector<ast::nodes::SwitchCaseNodeSharedPointer> switchCases;
		if( this->match( token::Type::Indent ) ) {
			while( this->check( { token::Type::Dedent, token::Type::Eof }, false ) ) {
				this->skipNewline();
				if( this->check( token::Type::Dedent ) ) {
					break;
				}
				this->expect( token::Type::KeywordCase, "expected 'case' in switch body" );
				lookup::SourceSharedPointer caseSource = this->current().source;
				bool isDefaultBranch = false;
				ast::nodes::ExpressionSharedPointer branchPattern;
				if( this->match( token::Type::Star ) ) {
					isDefaultBranch = true;
				}
				else {
					branchPattern = this->parseExpression();
				}
				this->expect( token::Type::Colon, "expected ':' after case pattern" );
				std::vector<ast::nodes::StatementSharedPointer> branchBody;
				if( this->check( token::Type::Newline ) ) {
					this->expectNewline( "case body" );
					branchBody = this->parseStatementBlock();
				}
				else {
					branchBody.push_back( this->parseStatement() );
				}
				switchCases.push_back( std::make_shared<ast::nodes::SwitchCaseNode>( branchPattern, isDefaultBranch, std::move( branchBody ), caseSource ) );
			}
			this->match( token::Type::Dedent );
		}
		return std::make_shared<ast::nodes::SwitchStatement>( matchSubject, std::move( switchCases ), switchSource );
	}
	
	ast::nodes::TraitDeclarationSharedPointer Parser::parseTraitDeclaration( ast::AccessModifier access ) {
		this->expect( token::Type::KeywordTrait, "" );
		token::Token traitNameToken = this->expect( token::Type::Identifier, "expected trait name" );
		ast::nodes::TraitDeclarationSharedPointer traitDeclaration = std::make_shared<ast::nodes::TraitDeclaration>( traitNameToken.value, traitNameToken.source );
		traitDeclaration->access = access;
		traitDeclaration->genericParameters = this->parseGenericParameters();
		this->expect( token::Type::Colon, "expected ':' after trait declaration" );
		this->expectNewline( "trait declaration" );
		if( this->match( token::Type::Indent ) ) {
			while( this->check( { token::Type::Dedent, token::Type::Eof }, false ) ) {
				this->skipNewline();
				if( this->check( token::Type::Dedent ) ) {
					break;
				}
				ast::AccessModifier memberAccess = this->parseAccessModifier();
				bool isMemberStatic = false;
				while( true ) {
					if( this->match( token::Type::KeywordStatic ) ) {
						isMemberStatic = true;
						continue;
					}
					break;
				}
				if( this->check( token::Type::KeywordFunction ) || this->check( token::Type::KeywordProperty ) ) {
					bool isPropertyMember = this->check( token::Type::KeywordProperty );
					ast::nodes::DeclarationSharedPointer traitMethod = this->parseFunctionDeclaration( memberAccess, false, false, false, isMemberStatic );
					if( isPropertyMember ) {
						static_cast<ast::nodes::FunctionDeclaration&>( *traitMethod ).isProperty = true;
					}
					traitDeclaration->methods.push_back( traitMethod );
				}
				else if( this->check( token::Type::Identifier ) || this->check( token::Type::Question ) ) {
					ast::nodes::FieldDeclarationSharedPointer traitField = this->parseFieldDeclaration( memberAccess );
					traitDeclaration->fields.push_back( traitField );
				}
				else {
					std::string unexpectedValue = this->current().value;
					std::string traitErrorMessage = fmt::format( "unexpected token '{}' in trait body", unexpectedValue );
					this->diagnostic.error( this->current().source, traitErrorMessage );
					this->advance();
				}
				this->skipNewline();
			}
			this->match( token::Type::Dedent );
		}
		return traitDeclaration;
	}
	
	ast::nodes::StatementSharedPointer Parser::parseTryCatchStatement() {
		lookup::SourceSharedPointer trySource = this->current().source;
		this->expect( token::Type::KeywordTry, "" );
		this->expect( token::Type::Colon, "expected ':' after 'try'" );
		this->expectNewline( "try block" );
		ast::nodes::TryCatchStatementSharedPointer tryStatement = std::make_shared<ast::nodes::TryCatchStatement>( trySource );
		tryStatement->tryBody = this->parseStatementBlock();
		
		// Parse except clauses: This loop processes one or more error handlers.
		// The supported syntax variableieties include typed exceptions, union types via pipe '|',
		// named exception variables using 'as', and bare catch-all blocks.
		while( this->check( token::Type::KeywordExcept ) ) {
			ast::nodes::ExceptionClause exceptionClause;
			exceptionClause.source = this->current().source;
			this->advance(); // consume 'except'
			
			// Exception Type Resolution: If an identifier follows 'except', it represents 
			// a specific error type or a union of multiple types separated by the pipe operator.
			if( this->check( token::Type::Identifier ) ) {
				
				// First exception type
				exceptionClause.exceptionTypes.push_back( std::make_shared<ast::nodes::SimpleTypeNode>( this->current().value, this->current().source ) );
				this->advance();
				
				// Additional exception types: except ExA|ExB
				while( this->match( token::Type::Pipe ) ) {
					if( this->check( token::Type::Identifier, false ) ) {
						this->diagnostic.error( this->current().source, "expected exception type after '|'" );
						break;
					}
					exceptionClause.exceptionTypes.push_back( std::make_shared<ast::nodes::SimpleTypeNode>( this->current().value, this->current().source ) );
					this->advance();
				}
			}
			
			// Variable Binding: The 'as' keyword allows the caught exception instance 
			// to be bound to a specific local variable name for use within the catch block.
			if( this->match( token::Type::KeywordAs ) ) {
				exceptionClause.variableName = this->expect( token::Type::Identifier, "expected variable name after 'as'" ).value;
			}
			
			// Clause Completion: Each except header must terminate with a colon and newline
			// before the indented statement block containing the recovery logic begins.
			this->expect( token::Type::Colon, "expected ':' after except clause" );
			this->expectNewline( "except block" );
			exceptionClause.body = this->parseStatementBlock();
			tryStatement->exceptionClauses.push_back( std::move( exceptionClause ) );
		}
		
		// Parse optional finally block: This block is guaranteed to execute regardless 
		// of whether an exception was raised or caught, typically used for resource cleanup.
		if( this->check( token::Type::KeywordFinally ) ) {
			this->advance(); // consume 'finally'
			this->expect( token::Type::Colon, "expected ':' after 'finally'" );
			this->expectNewline( "finally block" );
			tryStatement->finallyBody = this->parseStatementBlock();
		}
		return tryStatement;
	}
	
	ast::nodes::TypeAliasDeclarationSharedPointer Parser::parseTypeAliasDeclaration( ast::AccessModifier access ) {
		lookup::SourceSharedPointer aliasSource = this->current().source;
		this->expect( token::Type::KeywordType, "" );
		std::string aliasName = this->expect( token::Type::Identifier, "expected type name" ).value;
		std::vector<ast::nodes::GenericParameterSharedPointer> genericParams = this->parseGenericParameters();
		this->expect( token::Type::Assignment, "expected '=' in type alias" );
		ast::nodes::TypeNodeSharedPointer targetType = this->parseTypeNode();
		ast::nodes::TypeAliasDeclarationSharedPointer aliasDeclaration = std::make_shared<ast::nodes::TypeAliasDeclaration>( aliasName, targetType, aliasSource );
		aliasDeclaration->access = access;
		aliasDeclaration->genericParameters = std::move( genericParams );
		this->expectNewline( "type alias" );
		return aliasDeclaration;
	}
	
	ast::nodes::TypeNodeSharedPointer Parser::parseTypeNode() {
		
		// Optional type prefix: ?Type (nullable): This branch handles the prefix notation 
		// for optional types, allowing types to be declared as nullable using a leading question mark.
		if( this->check( token::Type::Question ) ) {
			lookup::SourceSharedPointer prefixSource = this->current().source;
			this->advance();
			ast::nodes::TypeNodeSharedPointer innerType = this->parseBaseTypeNode();
			return std::make_shared<ast::nodes::OptionalTypeNode>( innerType, prefixSource );
		}
		
		// Union type: Type1 | Type2 | Type3: This logic processes the bitwise-pipe symbol 
		// to construct a union type node, representing a value that could be any of the specified types.
		ast::nodes::TypeNodeSharedPointer baseType = this->parseBaseTypeNode();
		if( this->check( token::Type::Pipe ) ) {
			lookup::SourceSharedPointer unionSource = baseType->source;
			std::vector<ast::nodes::TypeNodeSharedPointer> memberTypes;
			memberTypes.push_back( baseType );
			while( this->match( token::Type::Pipe ) ) {
				memberTypes.push_back( this->parseBaseTypeNode() );
			}
			baseType = std::make_shared<ast::nodes::UnionTypeNode>( std::move( memberTypes ), unionSource );
		}
		
		// Optional type suffix: Type? (also supported): Provides compatibility for 
		// trailing question mark notation to indicate a nullable type, wrapping the existing type node.
		if( this->check( token::Type::Question ) ) {
			lookup::SourceSharedPointer suffixSource = this->current().source;
			this->advance();
			baseType = std::make_shared<ast::nodes::OptionalTypeNode>( baseType, suffixSource );
		}
		return baseType;
	}
	
	ast::nodes::ExpressionSharedPointer Parser::parseUnaryExpression() {
		lookup::SourceSharedPointer unarySource = this->current().source;
		switch( this->current().type ) {
			case token::Type::Ampersand: {
				this->advance();
				bool isMutable = this->match( token::Type::KeywordMutable );
				( void ) isMutable; // Note: stored in type info later
				return std::make_shared<ast::nodes::UnaryExpression>( token::Type::Ampersand, this->parseUnaryExpression(), true, unarySource );
			}
			case token::Type::Bang:
			case token::Type::KeywordNot: {
				token::Type negationOp = this->current().type;
				this->advance();
				return std::make_shared<ast::nodes::UnaryExpression>( negationOp, this->parseUnaryExpression(), true, unarySource );
			}
			case token::Type::KeywordAddressof: {
				this->advance();
				return std::make_shared<ast::nodes::UnaryExpression>( token::Type::KeywordAddressof, this->parseUnaryExpression(), true, unarySource );
			}
			case token::Type::KeywordMove: {
				this->advance();
				return std::make_shared<ast::nodes::UnaryExpression>( token::Type::KeywordMove, this->parseUnaryExpression(), true, unarySource );
			}
			case token::Type::Minus: {
				this->advance();
				return std::make_shared<ast::nodes::UnaryExpression>( token::Type::Minus, this->parseUnaryExpression(), true, unarySource );
			}
			case token::Type::Star: {
				this->advance();
				return std::make_shared<ast::nodes::UnaryExpression>( token::Type::Star, this->parseUnaryExpression(), true, unarySource );
			}
			case token::Type::Tilde: {
				this->advance();
				return std::make_shared<ast::nodes::UnaryExpression>( token::Type::Tilde, this->parseUnaryExpression(), true, unarySource );
			}
			default:
				return this->parsePostfixExpression();
		}
	}
	
	ast::nodes::StatementSharedPointer Parser::parseUnsafeBlockStatement() {
		lookup::SourceSharedPointer blockSource = this->current().source;
		this->expect( token::Type::KeywordUnsafe, "" );
		this->expect( token::Type::Colon, "expected ':' after unsafe" );
		this->expectNewline( "unsafe block" );
		std::vector<ast::nodes::StatementSharedPointer> statementBody = this->parseStatementBlock();
		return std::make_shared<ast::nodes::UnsafeBlockStatement>( std::move( statementBody ), blockSource );
	}
	
	ast::nodes::StatementSharedPointer Parser::parseWhileStatement() {
		lookup::SourceSharedPointer whileSource = this->current().source;
		this->expect( token::Type::KeywordWhile, "" );
		ast::nodes::ExpressionSharedPointer loopCondition = this->parseExpression();
		this->expect( token::Type::Colon, "expected ':' after while condition" );
		this->expectNewline( "while condition" );
		std::vector<ast::nodes::StatementSharedPointer> loopBody = this->parseStatementBlock();
		return std::make_shared<ast::nodes::WhileStatement>( loopCondition, std::move( loopBody ), whileSource );
	}
	
	const token::Token& Parser::peek( int offset ) const {
		size_t targetIndex = this->position + offset;
		if( targetIndex >= this->tokens.size() ) {
			static token::Token eofToken( token::Type::Eof, std::make_shared<lookup::Source>(), "" );
			return eofToken;
		}
		return this->tokens[targetIndex];
	}
	
	void Parser::skipNewline() {
		while( this->check( token::Type::Newline ) ) {
			this->advance();
		}
	}
	
}
